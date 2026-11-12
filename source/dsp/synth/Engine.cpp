#include "Engine.h"

#include <limits>

#include "WaveTables.h"
#include "WaveBend.h"
#include "Wavetable.h"

namespace synth
{
    namespace
    {
        constexpr float killMs = 3.0f;          // a stolen note's fade
        constexpr float oscFadeOutMs = 5.0f;    // an oscillator taken by a newer note (its VOICES limit)
        constexpr float spikeMs = 20.0f;        // the AMP's SPIKE: how long the extra click lasts (our own choice)
        constexpr float spikeMaxGain = 3.0f;    // ...and how much it adds at most: up to 4x, +12 dB (our own choice)
        constexpr float filterEnvelopeOctaves = 8.0f;   // the FILTER's ENV at +-100% moves the cutoff this far
        constexpr float filterVelocityOctaves = 4.0f;   // the FILTER's VELOCITY at 100% closes the softest notes this far

        float pitchToHz (float semitones) noexcept { return 440.0f * std::exp2 ((semitones - 69.0f) / 12.0f); }

        // PAN as a balance: the centre leaves both sides at full level, so a
        // centred sound is as loud as it would be in mono.
        void balance (float pan, float& left, float& right) noexcept
        {
            pan = juce::jlimit (-1.0f, 1.0f, pan);
            left = pan > 0.0f ? 1.0f - pan : 1.0f;
            right = pan < 0.0f ? 1.0f + pan : 1.0f;
        }

        float wrap (float phase) noexcept { return phase - std::floor (phase); }

        // What an oscillator reads this interval: one of the basic waves, or
        // two frames of its wavetable and how far between them it is (moving
        // from mixFrom to mixTo across the interval, so INDEX glides smoothly).
        struct WaveSource
        {
            OscWave wave = OscWave::saw;
            float pulseWidth = 0.5f;
            const Wavetable* table = nullptr;
            int below = 0, above = 0;
            float mixFrom = 0.0f, mixTo = 0.0f;
            bool oneFrame = true;

            // FFT: VSTRENGTH's other copy, and how far towards it (start and end of the interval).
            const Wavetable* partner = nullptr;
            float partnerFrom = 0.0f, partnerTo = 0.0f;
            // FFT LP and HP: where their cutoffs sit among the table's levels
            // (fractional, 0 = 512 harmonics, 3 an octave fewer...; below 0: off),
            // and how many levels SLOPE spreads them over.
            float lpPosition = -1.0f, hpPosition = -1.0f, slopeWidth = 1.0f;
            bool needsGeneralPath() const noexcept { return partner != nullptr || lpPosition >= 0.0f || hpPosition >= 0.0f; }
        };

        // A cutoff among the levels: blend level `lo` (more harmonics) into
        // `hi` (fewer) by `t`. Never richer than `minimum` (the level that
        // keeps the note clean).
        struct LevelBlend
        {
            int lo = 0, hi = 0;
            float t = 0.0f;
        };
        // FFT LP / HP's cutoff `position` among the levels, `width` (SLOPE) levels wide: two levels `span` apart,
        // blended so that on average the cutoff sits at `position`. The pair steps along a grid `span` levels apart
        // (0.47.0): as the cutoff moves past a grid line the blend is all one level on both sides, so a moving cutoff
        // never jumps. Before, the pair moved one level at a time, and with SLOPE above 0 the sound jumped (a click)
        // each time the cutoff crossed a level. With SLOPE at 0 (span 1) it's the same as before.
        LevelBlend blendFor (float position, float width, int minimum) noexcept
        {
            const auto span = juce::jmax (1, juce::roundToInt (width));
            auto lo = span * (int) std::floor (position / (float) span);
            auto hi = lo + span;
            const auto t = juce::jlimit (0.0f, 1.0f, (position - (float) lo) / (float) span);
            lo = juce::jlimit (minimum, tables::numLevels - 1, lo);
            hi = juce::jlimit (minimum, tables::numLevels - 1, hi);
            return { lo, hi, t };
        }

        // A wavetable's two frames at `level`, blended, for `n` samples: calls
        // `write (i, value)` for each.
        template <typename Write>
        void readTable (const WaveSource& source, int level, const float* positions, int n, Write write) noexcept
        {
            const auto below = source.table->get (source.below, level);
            if (source.oneFrame)
            {
                for (int i = 0; i < n; ++i)
                    write (i, Wavetable::read (below, positions[i]));
                return;
            }
            const auto above = source.table->get (source.above, level);
            const auto step = (source.mixTo - source.mixFrom) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                const auto a = Wavetable::read (below, positions[i]);
                const auto b = Wavetable::read (above, positions[i]);
                write (i, a + (b - a) * (source.mixFrom + step * (float) (i + 1)));
            }
        }

        // SYNC, FM and AM for one oscillator this interval (OscModulation.h).
        struct Modulation
        {
            bool active = false;
            bool sync = false, syncAdd = false, syncWindowed = false;
            float syncFrom = 1.0f, syncTo = 1.0f;   // how many times faster SYNC's copy runs, at the interval's start and end
            float fmDepth = 0.0f;                   // cycles
            const float* fmSignal = nullptr;        // another oscillator this interval, or null: fmShape
            ModShape fmShape = ModShape::sine;
            float amDepth = 0.0f;
            ModShape amShape = ModShape::sine;
            float ratio = 1.0f;                     // the modulator's rate, times the copy's
            float speed = 1.0f;                     // how much faster than the note they can make the wave run (for the table level)
            bool usesModulator() const noexcept { return (fmDepth > 0.0f && fmSignal == nullptr) || amDepth > 0.0f; }
        };

        float floorAny (float x) noexcept   // std::floor without the library call (see WaveBend)
        {
            const auto t = (float) (int) x;
            return t > x ? t - 1.0f : t;
        }

        // SHAPE (WaveBend.h), SYNC, FM and AM, sample by sample, for `n`
        // samples: calls `out (i, value)` for each. Slower than the plain
        // reads, so only used while one of them is on.
        template <typename Out>
        void renderBent (const WaveSource& source, WaveBend bending, const Modulation& mod, int level, int modLevel, int n, float& phase,
                         float increment, float& crushPrevious, float& modPhase, float& lastX, Out&& out) noexcept
        {
            const auto basic = source.wave == OscWave::saw || source.wave == OscWave::pulse ? tables::Wave::saw
                             : source.wave == OscWave::triangle ? tables::Wave::triangle : tables::Wave::sine;
            const auto lp = source.lpPosition >= 0.0f ? blendFor (source.lpPosition, source.slopeWidth, level) : LevelBlend { level, level, 0.0f };
            const auto hp = source.hpPosition >= 0.0f ? blendFor (source.hpPosition, source.slopeWidth, level) : LevelBlend {};
            bending.syncEdge = mod.sync && ! mod.syncWindowed;
            const auto step = (source.mixTo - source.mixFrom) / (float) n;
            const auto partnerStep = (source.partnerTo - source.partnerFrom) / (float) n;
            const auto syncStep = (mod.syncTo - mod.syncFrom) / (float) n;
            const auto modIncrement = increment * mod.ratio;
            for (int i = 0; i < n; ++i)
            {
                const auto mix = source.mixFrom + step * (float) (i + 1);
                const auto partnerMix = source.partnerFrom + partnerStep * (float) (i + 1);

                // The plain wave at one level.
                const auto readAt = [&] (int at, float x) -> float
                {
                    if (source.wave == OscWave::wavetable)
                    {
                        const auto frames = [&] (const Wavetable& t)
                        {
                            const auto a = Wavetable::read (t.get (source.below, at), x);
                            return source.oneFrame ? a : a + (Wavetable::read (t.get (source.above, at), x) - a) * mix;
                        };
                        auto v = frames (*source.table);
                        if (source.partner != nullptr)
                            v += (frames (*source.partner) - v) * partnerMix;
                        return v;
                    }
                    const auto* table = tables::get (basic, at);
                    if (source.wave == OscWave::pulse)
                    {
                        auto other = x + source.pulseWidth;   // 0 to 2
                        other -= (float) (other >= 1.0f);
                        return tables::read (table, x) - tables::read (table, other);
                    }
                    return tables::read (table, x);
                };
                // ...through FFT's LP (the levels blended at its cutoff) and HP (what's below its cutoff taken away).
                const auto read = [&] (float x) -> float
                {
                    auto v = readAt (lp.lo, x);
                    if (lp.hi != lp.lo && lp.t > 0.0f)
                        v += (readAt (lp.hi, x) - v) * lp.t;
                    if (source.hpPosition >= 0.0f)
                    {
                        auto below = readAt (hp.lo, x);
                        if (hp.hi != hp.lo && hp.t > 0.0f)
                            below += (readAt (hp.hi, x) - below) * hp.t;
                        v -= below;
                    }
                    return v;
                };

                // SYNC: a copy restarted each cycle, running `ratio` times faster.
                const auto ratio = mod.syncFrom + syncStep * (float) (i + 1);
                const auto synced = [&] (float y) -> float
                {
                    if (! mod.sync)
                        return read (y);
                    auto z = y * ratio;
                    z -= (float) (int) z;
                    auto copy = read (z);
                    if (mod.syncWindowed)
                        copy *= modulation::window (y);
                    return mod.syncAdd ? 0.5f * (read (y) + copy) : copy;
                };

                // FM: where in the wave this sample reads, swung by the modulator.
                auto x = phase;
                auto speed = increment;
                if (mod.fmDepth > 0.0f)
                {
                    const auto m = mod.fmSignal != nullptr ? mod.fmSignal[i] : modulation::shapeAt (mod.fmShape, modPhase, modLevel);
                    x += mod.fmDepth * m;
                    x -= floorAny (x);
                    auto moved = x - lastX;   // how far it really moved (for smoothing edges), wrapping round
                    moved += moved < -0.5f ? 1.0f : moved > 0.5f ? -1.0f : 0.0f;
                    speed = juce::jmax (1.0e-6f, std::abs (moved));
                }
                lastX = x;

                auto value = bending.sample (x, speed, synced);
                if (mod.amDepth > 0.0f)
                    value *= 1.0f - mod.amDepth + mod.amDepth * modulation::shapeAt (mod.amShape, modPhase, modLevel);
                out (i, bending.bits (value, crushPrevious));

                if (mod.usesModulator())
                {
                    modPhase += modIncrement;
                    modPhase -= floorAny (modPhase);
                }
                phase += increment;
                phase -= (float) (phase >= 1.0f);
            }
        }

        // One oscillator's wave for `n` samples into `out` (replacing), advancing `phase`.
        void renderWave (const WaveSource& source, float* out, int n, float& phase, float increment, int level, float gain) noexcept
        {
            switch (source.wave)
            {
                case OscWave::saw:
                case OscWave::pulse:
                {
                    const auto* saw = tables::get (tables::Wave::saw, level);
                    if (source.wave == OscWave::saw)
                    {
                        for (int i = 0; i < n; ++i)
                        {
                            out[i] = tables::read (saw, phase) * gain;
                            phase += increment;
                            phase -= (float) (phase >= 1.0f);
                        }
                    }
                    else
                    {
                        // A pulse is one saw minus another a PULSE WIDTH later in the
                        // cycle: band-limited, because both saws are, and centred on zero.
                        for (int i = 0; i < n; ++i)
                        {
                            auto other = phase + source.pulseWidth;
                            other -= (float) (other >= 1.0f);
                            out[i] = (tables::read (saw, phase) - tables::read (saw, other)) * gain;
                            phase += increment;
                            phase -= (float) (phase >= 1.0f);
                        }
                    }
                    break;
                }
                case OscWave::triangle:
                case OscWave::sine:
                {
                    const auto* table = tables::get (source.wave == OscWave::triangle ? tables::Wave::triangle : tables::Wave::sine, level);
                    for (int i = 0; i < n; ++i)
                    {
                        out[i] = tables::read (table, phase) * gain;
                        phase += increment;
                        phase -= (float) (phase >= 1.0f);
                    }
                    break;
                }
                case OscWave::sfz:        // (played by renderSfz, never here)
                    std::fill (out, out + n, 0.0f);
                    break;
                case OscWave::wavetable:
                case OscWave::freeform:   // (played as a one-frame wavetable: source.wave is wavetable by now)
                {
                    float positions[16];
                    for (int done = 0; done < n; done += 16)
                    {
                        const auto count = juce::jmin (16, n - done);
                        for (int i = 0; i < count; ++i)
                        {
                            positions[i] = phase;
                            phase += increment;
                            phase -= (float) (phase >= 1.0f);
                        }
                        auto part = source;
                        const auto span = source.mixTo - source.mixFrom;
                        part.mixFrom = source.mixFrom + span * (float) done / (float) n;
                        part.mixTo = source.mixFrom + span * (float) (done + count) / (float) n;
                        readTable (part, level, positions, count, [out, done, gain] (int i, float v) { out[done + i] = v * gain; });
                    }
                    break;
                }
            }
        }

        // Adds the SUB to `out`.
        void addSub (SubWave wave, float* out, int n, float& phase, float increment, int level, float gain) noexcept
        {
            const auto* table = tables::get (wave == SubWave::sine ? tables::Wave::sine
                                           : wave == SubWave::triangle ? tables::Wave::triangle
                                                                       : tables::Wave::saw, level);
            if (wave == SubWave::square)
            {
                for (int i = 0; i < n; ++i)
                {
                    auto other = phase + 0.5f;
                    other -= (float) (other >= 1.0f);
                    out[i] += (tables::read (table, phase) - tables::read (table, other)) * gain;
                    phase += increment;
                    phase -= (float) (phase >= 1.0f);
                }
                return;
            }
            for (int i = 0; i < n; ++i)
            {
                out[i] += tables::read (table, phase) * gain;
                phase += increment;
                phase -= (float) (phase >= 1.0f);
            }
        }
    }

    Engine::Engine()
    {
        // Each voice's noise gets its own seeds, so no two notes hiss alike.
        std::uint32_t seed = 0x2545f491u;
        for (auto& voice : voices)
            for (auto& oscillator : voice.oscillators)
            {
                seed = seed * 1664525u + 1013904223u;
                oscillator.noiseLeft = Noise (seed);
                seed = seed * 1664525u + 1013904223u;
                oscillator.noiseRight = Noise (seed);
            }
    }

    void Engine::prepare (double newSampleRate)
    {
        WarpTable::Grid::get();   // SKEW / PINCH's curves for modulated notes: worked out once, here, not on the audio thread
        sampleRate = newSampleRate;
        killStep = (float) (1.0 / juce::jmax (1.0, killMs * 0.001 * sampleRate));
        dcPole = (float) (1.0 - juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
        ragDown = (float) (1.0 - std::exp (-1.0 / (0.0012 * sampleRate)));   // RAGTIME: the damper lifts in about 1 ms...
        ragUp = (float) (1.0 - std::exp (-1.0 / (0.002 * sampleRate)));      // ...and the strike comes back in about 2
        (void) tables::get (tables::Wave::saw, 0);   // builds the tables now, not on the first note
        for (auto& voice : voices)
        {
            voice.amp.setSampleRate (sampleRate);
            voice.filter.setSampleRate (sampleRate);
        }
        reset();
    }

    void Engine::reset() noexcept
    {
        for (auto& voice : voices)
            free (voice);
        monoVoice = nullptr;
        numHeld = 0;
        bend = 0.0f;
        pedalDown = false;
        lastPitch = -1.0f;
        modWheel = breath = expression = channelPressure = 0.0f;
        expressionHeard = false;
        modWheelHeard = false;
        smoothedBend = 0.0f;
        seqClock = 0.0;
        naviClock = 0.0;
        crossClock = 0.0;
        ragClock = 0.0;
        ragLastStep = std::numeric_limits<std::int64_t>::min();
        travelClock.fill (0.0);
        arps = {};
        numArpEvents = 0;
        numPendingAnswers = 0;
        sampleClock = blockStartClock = 0;
        numTouches = 0;
        liveCycles.fill (-1);
        keysDown.fill (false);
        numKeysDown = 0;
        for (auto& lfo : globalLfos)
            lfo = {};
        for (auto& envelope : globalEnvelopes)
            envelope = {};
    }

    int Engine::getNumActiveVoices() const noexcept
    {
        return (int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.playing; });
    }

    int Engine::getSoundingNotes (float* pitches, int capacity, bool* isAnswer) const noexcept
    {
        int count = 0;
        for (const auto& v : voices)
            if (v.playing && ! v.killing && count < capacity)
            {
                if (isAnswer != nullptr)
                    isAnswer[count] = v.source == answerSource;
                pitches[count++] = v.pitch;
            }
        return count;
    }

    //==========================================================================
    void Engine::process (float* left, float* right, int numSamples,
                          const juce::MidiBuffer& midi, int midiStart, const Settings& settings) noexcept
    {
        followMode (settings);
        ampReleaseSeconds = settings.amp.envelope.releaseMs * 0.001f;
        sfzToWheel = settings.modulation.toModWheel;

        // The SHAPERs' EQ bands: the same for every note, so worked out once here.
        for (int k = 0; k < numShapers; ++k)
        {
            const auto& sh = settings.shapers[(size_t) k];
            for (int b = 0; b < 2; ++b)
                shaperCoefficients[(size_t) k].bells[(size_t) b] = sh.on ? shaper::Bell::make (sh.eqGainDb[(size_t) b], sh.eqFreqHz[(size_t) b],
                                                                                                sh.eqQ[(size_t) b], sampleRate)
                                                                         : shaper::Bell {};
        }

        // The STEP SEQUENCER: its pattern worked out for this block, if anything needs it.
        const auto& stepSeq = settings.stepSeq;
        seqActive = stepSeq.on || (settings.modulation.numRoutes > 0 && settings.modulation.uses (mod::Source::stepSeq));
        if (seqActive)
        {
            const auto secondsPerStep = stepSeq.beatsPerStep() * 60.0 / juce::jmax (1.0, settings.modulation.bpm);
            seqPrepared.prepare (stepSeq, secondsPerStep);
            seqStepsPerSample = 1.0 / (secondsPerStep * sampleRate);
        }

        std::fill (left, left + numSamples, 0.0f);
        std::fill (right, right + numSamples, 0.0f);
        outputBase = left;
        for (int b = 0; b < numInsertBuses; ++b)
            if (insertBaseLeft[(size_t) b] != nullptr)
            {
                std::fill (insertBaseLeft[(size_t) b], insertBaseLeft[(size_t) b] + numSamples, 0.0f);
                std::fill (insertBaseRight[(size_t) b], insertBaseRight[(size_t) b] + numSamples, 0.0f);
            }
        for (int s = 0; s < numSends; ++s)
            if (sendsWanted (settings, s))
            {
                std::fill (sendBaseLeft[(size_t) s], sendBaseLeft[(size_t) s] + numSamples, 0.0f);
                std::fill (sendBaseRight[(size_t) s], sendBaseRight[(size_t) s] + numSamples, 0.0f);
            }
        samplesIntoBlock = 0.0;
        blockStartClock = sampleClock;
        updateArpMasks (settings);

        // Rendering up to `until`, stopping at each arp step and note on the way (at its sample).
        int done = 0;
        const auto advance = [&] (int until)
        {
            while (done < until)
            {
                runArps (settings);
                runAnswers (settings);
                auto next = until;
                if (const auto due = nextArpTime (settings) - sampleClock; due > 0 && due < (std::int64_t) (until - done))
                    next = done + (int) due;
                render (left + done, right + done, next - done, settings);
                done = next;
            }
        };
        for (const auto metadata : midi)
        {
            const auto position = metadata.samplePosition - midiStart;
            if (position < 0)
                continue;
            if (position >= numSamples)
                break;
            advance (position);
            handle (metadata.getMessage(), settings);
        }
        advance (numSamples);

        // SFZ: the disk streams no note reads any more are freed.
        if (streamer != nullptr)
        {
            streamsInUse.fill (false);
            for (const auto& voice : voices)
                if (voice.playing)
                    for (const auto& osc : voice.oscillators)
                        for (const auto& player : osc.players)
                            if (player.region != nullptr && player.stream >= 0)
                                streamsInUse[(size_t) player.stream] = true;
            for (int k = 0; k < numPendingTails; ++k)   // (samples handed to a tail not yet started)
                for (const auto& osc : pendingTails[(size_t) k].handed)
                    for (const auto& player : osc)
                        if (player.region != nullptr && player.stream >= 0)
                            streamsInUse[(size_t) player.stream] = true;
            streamer->release (streamsInUse);
        }
    }

    void Engine::handle (const juce::MidiMessage& message, const Settings& settings) noexcept
    {
        if (message.isNoteOn())
            noteOn (message.getNoteNumber(), message.getFloatVelocity(), settings);
        else if (message.isNoteOff())
            noteOff (message.getNoteNumber(), settings);
        else if (message.isPitchWheel())
            bend = juce::jlimit (-1.0f, 1.0f, (float) (message.getPitchWheelValue() - 8192) / 8192.0f);
        else if (message.isController() && message.getControllerNumber() == 1)
        {
            modWheel = (float) message.getControllerValue() / 127.0f;
            modWheelHeard = true;
        }
        else if (message.isController() && message.getControllerNumber() == 2)
            breath = (float) message.getControllerValue() / 127.0f;
        else if (message.isController() && message.getControllerNumber() == 11)
        {
            expression = (float) message.getControllerValue() / 127.0f;
            expressionHeard = true;
        }
        else if (message.isChannelPressure())
            channelPressure = (float) message.getChannelPressureValue() / 127.0f;
        else if (message.isAftertouch())
        {
            for (auto& voice : voices)
                if (voice.playing && voice.note == message.getNoteNumber())
                    voice.pressure = (float) message.getAfterTouchValue() / 127.0f;
        }
        else if (message.isSustainPedalOn())
            sustainPedal (true);
        else if (message.isSustainPedalOff())
            sustainPedal (false);
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            if (message.isAllSoundOff())
                reset();
            else
                allNotesOff();
        }
    }

    //==========================================================================
    void Engine::followMode (const Settings& settings) noexcept
    {
        // Switching between POLY and MONO while notes sound: start afresh.
        if ((settings.play.mode == PlayMode::poly) != (lastMode == PlayMode::poly))
            allNotesOff();
        lastMode = settings.play.mode;
    }

    void Engine::noteOn (int note, float velocity, const Settings& settings) noexcept
    {
        followMode (settings);
        if (velocity <= 0.0f)
        {
            noteOff (note, settings);
            return;
        }
        // KEY SWITCHES (0.75.0): a sampled instrument's switch key picks how it's played and plays nothing itself
        // (nor do the other oscillators: it isn't a note).
        auto switched = false;
        for (size_t i = 0; i < settings.oscillators.size(); ++i)
        {
            const auto& s = settings.oscillators[i];
            if (s.on && s.wave == OscWave::sfz && s.instrument != nullptr && s.instrument->isSwitch (note))
            {
                switchFor (i, s.instrument);
                sfzSwitch[i] = note;
                switchShown[i].store (note);
                switched = true;
            }
        }
        if (switched)
            return;
        // LFOs set to FIRST NOTE start with a note when none is held; LAST NOTE with every note.
        const auto anyHeld = std::any_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.playing && v.held && ! v.killing; });
        for (int k = 0; k < mod::numLfos; ++k)
        {
            const auto& lfo = settings.modulation.lfos[(size_t) k];
            if (lfo.trigger == mod::LfoTrigger::lastNote || (lfo.trigger == mod::LfoTrigger::firstNote && ! anyHeld))
                globalLfos[(size_t) k].start (lfo, random);
        }
        for (int k = 0; k < mod::numEnvelopes; ++k)
        {
            const auto trigger = settings.modulation.envelopes[(size_t) k].trigger;
            if (trigger == mod::EnvelopeTrigger::lastNote || (trigger == mod::EnvelopeTrigger::firstNote && ! anyHeld))
                globalEnvelopes[(size_t) k].start();
        }
        // The STEP SEQUENCER's own clock: SONG (with the song stopped) and FIRST NOTE start
        // with a note when none is held, LAST NOTE with every note.
        const auto seqTrigger = settings.stepSeq.trigger;
        if (seqTrigger == seq::Trigger::lastNote || ((seqTrigger == seq::Trigger::firstNote || seqTrigger == seq::Trigger::song) && ! anyHeld))
            seqClock = 0.0;
        // NAVICOMPUTER's own clock (the song stopped) starts with the first key down.
        if (! anyHeld)
            naviClock = 0.0;
        // ...and CROSSROADS' (the first stretch is light).
        if (! anyHeld)
            crossClock = 0.0;
        // ...and RAGTIME's (the roll starts with the first key).
        if (! anyHeld)
        {
            ragClock = 0.0;
            ragLastStep = std::numeric_limits<std::int64_t>::min();
        }
        // ...and each oscillator's TRAVEL set to FIRST NOTE or LAST NOTE.
        for (size_t i = 0; i < travelClock.size(); ++i)
        {
            const auto trigger = settings.oscillators[i].travel.trigger;
            if (trigger == travel::Trigger::lastNote || (trigger == travel::Trigger::firstNote && ! anyHeld))
                travelClock[i] = 0.0;
        }
        // The ARPEGGIATOR: the arps that take this key hold it; the keys themselves
        // play only the oscillators not routed to an arp (if any).
        updateArpMasks (settings);
        if (numKeysDown == 0)
            sharedStartClock = sampleClock;   // ARPS TOGETHER: every arp's clock from here
        if (! keysDown[(size_t) note])
        {
            keysDown[(size_t) note] = true;
            ++numKeysDown;
        }
        arpKeyDown (note, velocity, settings);
        auto keyOscillators = false;
        for (size_t i = 0; i < settings.oscillators.size(); ++i)
            keyOscillators = keyOscillators || (settings.oscillators[i].on && ((keyMask >> i) & 1u) != 0);
        if (! keyOscillators)
            return;
        if (settings.play.mode == PlayMode::poly)
            polyNoteOn (note, velocity, settings);
        else
            monoNoteOn (note, velocity, settings);
    }

    void Engine::noteOff (int note, const Settings& settings) noexcept
    {
        followMode (settings);
        if (keysDown[(size_t) note])
        {
            keysDown[(size_t) note] = false;
            --numKeysDown;
        }
        arpKeyUp (note);
        if (settings.play.mode != PlayMode::poly)
        {
            monoNoteOff (note, settings);
            releaseGlobalEnvelopesIfNoneHeld();
            return;
        }
        for (auto& voice : voices)
            if (voice.playing && voice.held && voice.note == note && ! voice.killing && voice.source == 0)
            {
                voice.held = false;
                if (pedalDown)
                    voice.sustained = true;
                else
                    releaseVoice (voice);
            }
        releaseGlobalEnvelopesIfNoneHeld();
    }

    void Engine::polyNoteOn (int note, float velocity, const Settings& settings) noexcept
    {
        // The same key again while its last note still sounds: that one lets go.
        auto anyHeld = false;
        for (auto& voice : voices)
            if (voice.playing && ! voice.killing)
            {
                if (voice.note == note && voice.source == 0 && (voice.held || voice.sustained))
                {
                    voice.held = voice.sustained = false;
                    releaseVoice (voice);
                }
                anyHeld = anyHeld || voice.held;
            }

        makeRoom (settings);
        auto* voice = findFreeVoice();
        const auto glide = settings.play.glideMs > 0.0f && lastPitch >= 0.0f && glides (note, settings)
                        && (settings.play.glideMode == GlideMode::always || anyHeld);
        startVoice (*voice, note, velocity, glide ? lastPitch : (float) note, settings, 0, keyMask);
        applyOscillatorLimits (*voice, settings);
        answer (*voice, settings);
    }

    void Engine::makeRoom (const Settings& settings) noexcept
    {
        // Over VOICES: the quietest releasing note goes, or else the oldest. SIROCCO's answers (0.58.0) don't count:
        // they go with their note.
        const auto counts = [this] (const Voice& v) { return v.playing && ! v.killing && v.source != answerSource && v.source != tailSource; };
        auto sounding = (int) std::count_if (voices.begin(), voices.end(), counts);
        while (sounding >= juce::jlimit (1, maxVoices, settings.play.voices))
        {
            Voice* victim = nullptr;
            for (auto& voice : voices)
                if (counts (voice) && voice.amp.isReleasing()
                    && (victim == nullptr || voice.amp.getLevel() < victim->amp.getLevel()))
                    victim = &voice;
            if (victim == nullptr)
                for (auto& voice : voices)
                    if (counts (voice) && (victim == nullptr || voice.age < victim->age))
                        victim = &voice;
            if (victim == nullptr)
                break;
            kill (*victim);
            --sounding;
        }
    }

    void Engine::monoNoteOn (int note, float velocity, const Settings& settings) noexcept
    {
        // Remember the key (moving it to the end if it's somehow already held).
        auto* end = heldNotes.data() + numHeld;
        if (auto* found = std::find (heldNotes.data(), end, note); found != end)
        {
            std::move (found + 1, end, found);
            --numHeld;
        }
        if (numHeld < (int) heldNotes.size())
        {
            heldNotes[(size_t) numHeld] = note;
            heldVelocities[(size_t) note] = velocity;
            ++numHeld;
        }

        const auto target = monoTarget (settings);
        if (target < 0)
            return;

        const auto sounding = monoVoice != nullptr && monoVoice->playing && ! monoVoice->killing;
        if (! sounding)
        {
            auto* voice = findFreeVoice();
            const auto glide = settings.play.glideMs > 0.0f && lastPitch >= 0.0f && glides (target, settings) && settings.play.glideMode == GlideMode::always;
            startVoice (*voice, target, heldVelocities[(size_t) target], glide ? lastPitch : (float) target, settings, 0, keyMask);
            monoVoice = voice;   // after starting it: starting a voice clears whatever it was
            answer (*voice, settings);
            return;
        }
        if (target == monoVoice->note && monoVoice->held)
            return;   // MONO LOW / HIGH: a key that doesn't change which note plays

        // Overlapping a held note: LEGATO glides on (without restarting the
        // envelopes); otherwise the envelopes start again from where they are.
        const auto overlapping = monoVoice->held;
        const auto legato = settings.play.legato && overlapping;
        const auto glide = settings.play.glideMs > 0.0f && glides (target, settings) && (settings.play.glideMode == GlideMode::always || overlapping);
        retrigger (*monoVoice, target, heldVelocities[(size_t) target], ! legato, glide, settings);
        reanswer (*monoVoice, ! legato, glide, settings);
    }

    void Engine::monoNoteOff (int note, const Settings& settings) noexcept
    {
        auto* end = heldNotes.data() + numHeld;
        if (auto* found = std::find (heldNotes.data(), end, note); found != end)
        {
            std::move (found + 1, end, found);
            --numHeld;
        }

        if (monoVoice == nullptr || ! monoVoice->playing || monoVoice->killing)
            return;

        const auto target = monoTarget (settings);
        if (target >= 0)
        {
            // Back to a key still held (a trill): always legato-style glide rules.
            if (target != monoVoice->note)
            {
                retrigger (*monoVoice, target, heldVelocities[(size_t) target], ! settings.play.legato,
                           settings.play.glideMs > 0.0f, settings);
                reanswer (*monoVoice, ! settings.play.legato, settings.play.glideMs > 0.0f, settings);
            }
            return;
        }
        if (monoVoice->note != note && monoVoice->held)
            return;
        monoVoice->held = false;
        if (pedalDown)
            monoVoice->sustained = true;
        else
            releaseVoice (*monoVoice);
    }

    int Engine::monoTarget (const Settings& settings) const noexcept
    {
        if (numHeld == 0)
            return -1;
        const auto* first = heldNotes.data();
        const auto* end = heldNotes.data() + numHeld;
        switch (settings.play.mode)
        {
            case PlayMode::monoLow:  return *std::min_element (first, end);
            case PlayMode::monoHigh: return *std::max_element (first, end);
            case PlayMode::mono:
            case PlayMode::poly:     break;
        }
        return heldNotes[(size_t) numHeld - 1];   // the last one pressed
    }

    void Engine::allNotesOff() noexcept
    {
        for (auto& voice : voices)
            if (voice.playing)
            {
                voice.held = voice.sustained = false;
                releaseVoice (voice);
            }
        numHeld = 0;
        for (auto& st : arps)
            st = {};
        numArpEvents = 0;
        numPendingAnswers = 0;
        keysDown.fill (false);
        numKeysDown = 0;
    }

    int Engine::takeTouches (Touch* out, int capacity) noexcept
    {
        const auto n = juce::jmin (capacity, numTouches);
        std::copy (touches.begin(), touches.begin() + n, out);
        numTouches = 0;
        return n;
    }

    void Engine::sustainPedal (bool down) noexcept
    {
        if (down != pedalDown)
        {
            touch (down ? Touch::Kind::pedalDown : Touch::Kind::pedalUp, 0.0f, 1.0f);
            // SFZ: the pedal's own sounds (startTails finds the oscillators whose instrument has them).
            if (numPendingTails < (int) pendingTails.size())
            {
                auto& tail = pendingTails[(size_t) numPendingTails++];
                tail = {};
                tail.pedal = down ? 127 : 0;
            }
        }
        pedalDown = down;
        if (down)
            return;
        // The arps let go of the keys the pedal kept.
        for (int a = 0; a < arp::numArps; ++a)
        {
            auto& st = arps[(size_t) a];
            for (int note = 0; note < 128; ++note)
                if (st.pedalHeld[(size_t) note])
                {
                    st.pedalHeld[(size_t) note] = false;
                    removeArpKey (st, note);
                }
            stopArpIfEmpty (a);
        }
        for (auto& voice : voices)
            if (voice.playing && voice.sustained && ! voice.held)
            {
                voice.sustained = false;
                releaseVoice (voice);
            }
        releaseGlobalEnvelopesIfNoneHeld();
    }

    //==========================================================================
    Engine::Voice* Engine::findFreeVoice() noexcept
    {
        for (auto& voice : voices)
            if (! voice.playing)
                return &voice;

        // Every slot busy (only possible with many stolen notes still fading):
        // the one furthest through its fade goes at once.
        auto* oldest = &voices.front();
        for (auto& voice : voices)
            if (voice.killing && (! oldest->killing || voice.killGain < oldest->killGain))
                oldest = &voice;
        free (*oldest);
        return oldest;
    }

    void Engine::startVoice (Voice& voice, int note, float velocity, float fromPitch, const Settings& settings, int source, std::uint8_t mask) noexcept
    {
        if (source != answerSource && source != tailSource)
            touch (Touch::Kind::press, (float) note, velocity);
        free (voice);
        voice.source = source;
        voice.oscMask = mask;
        voice.playing = true;
        voice.held = true;
        voice.note = note;
        voice.velocity = velocity;
        voice.age = ++noteCounter;
        voice.pitch = fromPitch;
        voice.naviSemis = 0.0f;   // NAVICOMPUTER: a new note takes its move at once
        voice.naviFresh = true;
        voice.crossSemis = 0.0f;  // CROSSROADS: a new note takes its pull at once
        voice.crossFresh = true;
        voice.ragGain = 1.0f;     // RAGTIME: no lift on a new note
        // AFTERGLOW: this note's own character (drawn always, from its own numbers: used only while it's on).
        voice.glowTune = nextGlow();
        voice.glowCutoff = nextGlow();
        voice.glowTime = nextGlow();
        voice.driftTune = voice.driftTuneTo = nextGlow();
        voice.driftCutoff = voice.driftCutoffTo = nextGlow();
        voice.driftLeft = 0.0f;
        for (auto& path : voice.paths)
        {
            path.key = -1;
            path.reset();
        }
        setGlide (voice, (float) note, ! juce::approximatelyEqual (fromPitch, (float) note), settings);
        retrigger (voice, note, velocity, true, ! juce::approximatelyEqual (fromPitch, (float) note), settings);
    }

    void Engine::restrike (Voice& voice, float velocity, const Settings& settings) noexcept
    {
        voice.velocity = velocity;
        voice.amp.trigger (settings.amp.envelope);
        voice.filter.trigger (settings.filter.envelope);
        voice.spikePosition = 0.0f;
        for (auto& envelope : voice.envelopes)
            envelope.start();
        voice.pitchEnvelope.start();
        for (size_t i = 0; i < voice.oscillators.size(); ++i)
        {
            const auto& s = settings.oscillators[i];
            auto& osc = voice.oscillators[i];
            if (s.wave != OscWave::sfz || ! osc.active || osc.fadingOut)
                continue;
            startSfz (voice, osc, i, s, false);   // a fresh recording (the last fades out over 5 ms)
        }
        ragStrikeCount.store (ragStrikeCount.load() + 1);
    }

    void Engine::retrigger (Voice& voice, int note, float velocity, bool restartEnvelopes, bool glide, const Settings& settings) noexcept
    {
        voice.note = note;
        voice.velocity = velocity;
        voice.ragVelocity = velocity;   // RAGTIME strikes at the velocity the note was played
        voice.held = true;
        voice.sustained = false;
        setGlide (voice, (float) note, glide, settings);
        setNotePan (voice, settings);
        if (glides (note, settings))   // GLIDE SPLIT: notes below it don't glide, nor are they glided from
            lastPitch = (float) note;

        if (! restartEnvelopes)
            return;

        voice.amp.trigger (settings.amp.envelope);
        voice.filter.trigger (settings.filter.envelope);
        startLfos (voice, settings);
        voice.spikePosition = 0.0f;

        for (size_t i = 0; i < voice.oscillators.size(); ++i)
        {
            const auto& s = settings.oscillators[i];
            auto& osc = voice.oscillators[i];
            const auto wasActive = osc.active && ! osc.fadingOut;
            const auto plays = s.on && ((voice.oscMask >> i) & 1u) != 0   // (an arp's note: only its oscillators)
                            && (voice.source != tailSource || s.wave == OscWave::sfz);
            osc.active = plays;
            osc.fadingOut = false;
            if (! plays)
                continue;

            // RANDOM: below zero a random pitch each note, above zero alternating up and down.
            if (s.randomPitch < 0.0f)
                osc.randomOffset = (random.nextFloat() * 2.0f - 1.0f) * -s.randomPitch;
            else if (s.randomPitch > 0.0f)
            {
                alternate[i] = ! alternate[i];
                osc.randomOffset = (alternate[i] ? 1.0f : -1.0f) * s.randomPitch;
            }
            else
                osc.randomOffset = 0.0f;

            // A mono retrigger keeps the wave running (no jump); a fresh note starts it where PHASE says.
            if (! wasActive)
            {
                osc.phase = s.randomPhase ? random.nextFloat() : wrap (s.phase);
                osc.subPhase = s.randomPhase ? random.nextFloat() : wrap (s.phase);
                const auto fadeSamples = s.fadeInMs * 0.001f * (float) sampleRate;
                osc.fade = fadeSamples >= 1.0f ? 0.0f : 1.0f;
                osc.fadeStep = fadeSamples >= 1.0f ? 1.0f / fadeSamples : 0.0f;

                // The copies: a chord note's own pitch starts as the key does; every
                // unison copy at a random point, which keeps them from lining up and
                // sounding hollow (what makes a "supersaw" wide rather than phasey).
                for (int copy = 1; copy < maxCopies; ++copy)
                {
                    auto& c = osc.copies[(size_t) copy];
                    c.phase = (copy % maxUnison == 0 && ! s.randomPhase) ? wrap (s.phase) : random.nextFloat();
                    c.vibratoPhase = random.nextFloat();
                    c.vibratoRate = 0.85f + 0.3f * random.nextFloat();   // INDEPENDENT: each a little faster or slower (our choice)
                    c.vibratoHeld = random.nextFloat() * 2.0f - 1.0f;
                }
                osc.vibratoPhase = 0.0f;
                osc.vibratoHeld = random.nextFloat() * 2.0f - 1.0f;
                osc.tableIndex = -1.0f;
                osc.travelTime = 0.0;
                osc.travelIndex = -1.0f;
                osc.travelGain = 1.0f;
                osc.pulsePhase = random.nextFloat();
                osc.crushPrevious = 0.0f;
                for (auto& c : osc.copies)
                    c.crushPrevious = 0.0f;

                // FM and AM's modulator: PHASE LOCK starts it with the wave (each
                // copy's at its own start), else anywhere.
                const auto ratio = modulation::ratio (s.modRate, s.modGrid);
                osc.modPhase = s.modPhaseLock ? wrap (osc.phase * ratio) : random.nextFloat();
                osc.lastX = osc.phase;
                for (auto& c : osc.copies)
                {
                    c.modPhase = s.modPhaseLock ? wrap (c.phase * ratio) : random.nextFloat();
                    c.lastX = c.phase;
                }
            }
            if (! wasActive || restartEnvelopes)
            {
                osc.vibratoAge = 0;   // the vibrato fades in again with each new note
                osc.syncAge = 0;      // ...and SYNC's envelope starts again
            }
            // SFZ: the regions for this note (a tail voice: the release samples).
            if (s.wave == OscWave::sfz)
            {
                startSfz (voice, osc, i, s, voice.source == tailSource);
                if (std::none_of (osc.players.begin(), osc.players.end(), [] (const SfzPlayer& p) { return p.region != nullptr; }))
                    osc.active = false;   // no region for this key and velocity: silent, and costs nothing
            }
            else
                osc.instrument = nullptr;
        }
    }

    void Engine::setGlide (Voice& voice, float toPitch, bool glide, const Settings& settings) noexcept
    {
        voice.targetPitch = toPitch;
        if (! glide || settings.play.glideMs <= 0.0f)
        {
            voice.pitch = toPitch;
            voice.glideRate = 0.0f;
            return;
        }
        // GLIDE is the time to get there, however far.
        const auto samples = juce::jmax (1.0f, settings.play.glideMs * 0.001f * (float) sampleRate);
        voice.glideRate = std::abs (toPitch - voice.pitch) / samples;
        voice.glideFrom = voice.pitch;
        voice.glideProgress = 0.0f;
        voice.glideStep = 1.0f / samples;
    }

    void Engine::setNotePan (Voice& voice, const Settings& settings) noexcept
    {
        const auto& amp = settings.amp;
        auto pan = amp.pan + amp.panKeytrack * juce::jlimit (-1.0f, 1.0f, ((float) voice.note - 60.0f) / 36.0f);
        if (amp.spread > 0.0f)
            pan += (voice.age % 2 == 0 ? amp.spread : -amp.spread);       // alternating left and right
        else if (amp.spread < 0.0f)
            pan += (random.nextFloat() * 2.0f - 1.0f) * -amp.spread;     // anywhere, at random
        voice.notePan = pan;
        balance (pan, voice.panLeft, voice.panRight);
    }

    void Engine::applyOscillatorLimits (const Voice& newest, const Settings& settings) noexcept
    {
        // An oscillator's own VOICES limit: over it, the oldest note's copy of
        // that oscillator fades out (the rest of that note carries on).
        for (size_t i = 0; i < settings.oscillators.size(); ++i)
        {
            const auto limit = settings.oscillators[i].voiceLimit;
            if (! settings.oscillators[i].on || limit >= maxVoices)
                continue;
            for (;;)
            {
                int count = 0;
                Voice* oldest = nullptr;
                for (auto& voice : voices)
                    if (voice.playing && ! voice.killing && voice.oscillators[i].active && ! voice.oscillators[i].fadingOut)
                    {
                        ++count;
                        if (&voice != &newest && (oldest == nullptr || voice.age < oldest->age))
                            oldest = &voice;
                    }
                if (count <= limit || oldest == nullptr)
                    break;
                auto& osc = oldest->oscillators[i];
                osc.fadingOut = true;
                osc.fadeStep = -1.0f / juce::jmax (1.0f, oscFadeOutMs * 0.001f * (float) sampleRate);
            }
        }
    }

    void Engine::releaseVoice (Voice& voice) noexcept
    {
        if (voice.source == tailSource)
            return;
        // LOOP_SUSTAIN (0.73.0): those samples stop going round and play on past their loop.
        for (auto& osc : voice.oscillators)
            for (auto& player : osc.players)
                if (player.region != nullptr && player.region->loop == sfz::Region::Loop::sustain)
                    player.looping = false;
        // SFZ: the release samples of its sampled oscillators, played in a voice of their own (startTails).
        // Samples whose AMPEG_RELEASE outlasts the AMP's (a piano's undamped top strings) go with them, falling over
        // their own release (60 dB in that time) from the level the AMP had reached.
        if (numPendingTails < (int) pendingTails.size())
        {
            auto& tail = pendingTails[(size_t) numPendingTails];
            tail = {};
            for (size_t i = 0; i < voice.oscillators.size(); ++i)
            {
                auto& osc = voice.oscillators[i];
                if (! osc.active || osc.fadingOut || osc.instrument == nullptr)
                    continue;
                auto handedAny = false;
                for (auto& player : osc.players)
                    if (player.region != nullptr && player.fadeStep >= 0.0f && ! player.region->release
                        && player.region->releaseSeconds > ampReleaseSeconds)
                    {
                        auto& to = tail.handed[i][(size_t) (&player - osc.players.data())];
                        to = player;
                        to.gainLeft *= voice.amp.getLevel();
                        to.gainRight *= voice.amp.getLevel();
                        to.decay = (float) std::pow (10.0, -3.0 / (juce::jmax (0.01, (double) player.region->releaseSeconds) * sampleRate));
                        player.region = nullptr;   // (its stream goes with it)
                        handedAny = true;
                    }
                if (osc.instrument->hasRelease || handedAny)
                {
                    tail.mask = (std::uint8_t) (tail.mask | (1u << i));
                    tail.instruments[i] = osc.instrument;
                    tail.heldSeconds = (float) (osc.heldSamples / sampleRate);
                    tail.note = osc.sfzKey;
                }
            }
            if (tail.mask != 0)
            {
                tail.velocity = voice.velocity;
                ++numPendingTails;
            }
        }
        if (voice.source != answerSource)
            touch (Touch::Kind::release, (float) voice.note, voice.velocity);
        voice.amp.release();
        voice.filter.release();
        for (auto& envelope : voice.envelopes)
            envelope.release();
        voice.pitchEnvelope.release();
        if (voice.source != answerSource)
            releaseAnswers (voice);   // SIROCCO: its answers let go with it
    }

    void Engine::markSfzInUse() noexcept
    {
        // (0.82.1) Each instrument a note is reading gets this check's number. A preset change retires the old
        // instrument; the processor frees it only once a check has found nothing reading it, so a held looped note,
        // a long release or a waiting release tail never reads freed memory (Windows reported a crash on Sanctuary
        // Organ, whose choir loops).
        const auto check = sfzChecks.load (std::memory_order_relaxed);
        const auto mark = [check] (const sfz::Instrument* instrument)
        {
            if (instrument != nullptr)
                instrument->lastUsedCheck.store (check, std::memory_order_release);
        };
        for (const auto& voice : voices)
            for (const auto& osc : voice.oscillators)
            {
                mark (osc.instrument);
                for (const auto& player : osc.players)
                    if (player.region != nullptr)
                        mark (player.instrument);
            }
        for (int t = 0; t < numPendingTails; ++t)
        {
            const auto& tail = pendingTails[(size_t) t];
            for (size_t i = 0; i < tail.instruments.size(); ++i)
            {
                mark (tail.instruments[i]);
                for (const auto& player : tail.handed[i])
                    if (player.region != nullptr)
                        mark (player.instrument);
            }
        }
        sfzChecks.store (check + 1, std::memory_order_release);
    }

    void Engine::releaseGlobalEnvelopesIfNoneHeld() noexcept
    {
        // The FIRST NOTE / LAST NOTE envelopes let go when no key (or pedal) holds a note.
        if (std::none_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.playing && (v.held || v.sustained) && ! v.killing; }))
            for (auto& envelope : globalEnvelopes)
                envelope.release();
    }

    void Engine::kill (Voice& voice) noexcept
    {
        if (voice.source != answerSource && voice.playing)
            releaseAnswers (voice);
        voice.killing = true;
        voice.held = voice.sustained = false;
        if (&voice == monoVoice)
            monoVoice = nullptr;
    }

    void Engine::free (Voice& voice) noexcept
    {
        if (voice.playing && voice.source != answerSource)
            releaseAnswers (voice);
        voice.answers = 0;
        voice.layer = 0;
        voice.playing = voice.held = voice.sustained = voice.killing = false;
        voice.killGain = 1.0f;
        voice.note = -1;
        voice.amp.reset();
        voice.filter.reset();
        for (auto& osc : voice.oscillators)
        {
            osc.active = osc.fadingOut = false;
            osc.fade = 1.0f;
            osc.fadeStep = 0.0f;
            for (auto& player : osc.players)
                player.region = nullptr;   // (its stream is freed at the block's end)
            osc.instrument = nullptr;
        }
        if (&voice == monoVoice)
            monoVoice = nullptr;
    }

    //==========================================================================
    void Engine::render (float* left, float* right, int numSamples, const Settings& settings) noexcept
    {
        if (numPendingTails > 0)
            startTails (settings);
        // In pieces of up to maxIntervals intervals, so the global LFOs' values for each fit.
        constexpr int piece = maxIntervals * controlInterval;
        for (int start = 0; start < numSamples; start += piece)
        {
            const auto length = juce::jmin (piece, numSamples - start);
            const auto& modulation = settings.modulation;

            // The pitch wheel, smoothed by BEND LAG, for each interval.
            for (int i = 0, at = 0; at < length; ++i, at += controlInterval)
            {
                const auto n = juce::jmin (controlInterval, length - at);
                if (settings.pitch.bendLagMs <= 0.0f)
                    smoothedBend = bend;
                else
                    smoothedBend += (bend - smoothedBend) * (1.0f - std::exp (-(float) n / (settings.pitch.bendLagMs * 0.001f * (float) sampleRate)));
                bendValues[(size_t) i] = smoothedBend;
            }
            if (modulation.numRoutes > 0 || modulation.effectSources != 0)
            {
                // The LFOs running once for the whole synth: their value for each interval.
                const auto secondsPerSample = 1.0 / sampleRate;
                for (int k = 0; k < mod::numLfos; ++k)
                {
                    const auto& lfo = modulation.lfos[(size_t) k];
                    if (lfo.trigger == mod::LfoTrigger::voice || ! modulation.uses ((mod::Source) ((int) mod::Source::lfo1 + k)))
                        continue;
                    auto& state = globalLfos[(size_t) k];
                    const auto cycles = lfo.cyclesPerSecond (modulation.bpm);
                    for (int i = 0, at = 0; at < length; ++i, at += controlInterval)
                    {
                        const auto n = juce::jmin (controlInterval, length - at);
                        if (lfo.trigger == mod::LfoTrigger::global && lfo.sync != mod::LfoSync::free && modulation.ppq >= 0.0)
                        {
                            // GLOBAL and synced, with the song playing: in step with the song's beats.
                            const auto beats = modulation.ppq + (samplesIntoBlock + start + at) * secondsPerSample * modulation.bpm / 60.0;
                            const auto position = beats * cycles * 60.0 / modulation.bpm + lfo.phase;
                            state.phase = position - std::floor (position);
                        }
                        globalLfoValues[(size_t) k][(size_t) i] = state.next (lfo, cycles, n * secondsPerSample, random, modulation.lfoShapes);
                        lastGlobalLfo[(size_t) k] = globalLfoValues[(size_t) k][(size_t) i];
                    }
                }
                // ...and the envelopes set to FIRST NOTE or LAST NOTE.
                for (int k = 0; k < mod::numEnvelopes; ++k)
                {
                    const auto& envelope = modulation.envelopes[(size_t) k];
                    if (envelope.trigger == mod::EnvelopeTrigger::voice || modulation.envelopeShapes == nullptr
                        || ! modulation.uses ((mod::Source) ((int) mod::Source::env1 + k)))
                        continue;
                    for (int i = 0, at = 0; at < length; ++i, at += controlInterval)
                    {
                        globalEnvelopeValues[(size_t) k][(size_t) i] = globalEnvelopes[(size_t) k].next (modulation.envelopeShapes[k], envelope,
                                                                                                             juce::jmin (controlInterval, length - at) * secondsPerSample,
                                                                                                             modulation.bpm);
                        lastGlobalEnvelope[(size_t) k] = globalEnvelopeValues[(size_t) k][(size_t) i];
                    }
                }
            }
            // The STEP SEQUENCER (unless each note runs its own): its value at each sample.
            if (seqActive && settings.stepSeq.trigger != seq::Trigger::voice)
            {
                const auto followSong = settings.stepSeq.trigger == seq::Trigger::song && modulation.ppq >= 0.0;
                const auto beatsPerSample = modulation.bpm / 60.0 / sampleRate;
                const auto beatsPerStep = settings.stepSeq.beatsPerStep();
                for (int i = 0; i < length; ++i)
                {
                    const auto position = followSong ? (modulation.ppq + (samplesIntoBlock + start + i) * beatsPerSample) / beatsPerStep
                                                     : seqClock + i * seqStepsPerSample;
                    const auto [l, r] = seqPrepared.values (position);
                    seqValues[0][(size_t) i] = l;
                    seqValues[1][(size_t) i] = r;
                }
                if (! followSong)
                    seqClock = std::fmod (seqClock + length * seqStepsPerSample, (double) seqPrepared.getLength() * 1024.0);
            }
            // NAVICOMPUTER (0.86.0): the move for each interval, in time with the song (or its own count).
            naviMoving = settings.navi.moving();
            {
                const auto& nv = settings.navi;
                const auto beatsPerSample = juce::jmax (1.0, modulation.bpm) / 60.0 / sampleRate;
                const auto followSong = modulation.ppq >= 0.0;
                if (naviMoving)
                {
                    const auto beatsPerStep = nv.beatsPerStep();
                    std::int64_t songStep = 0;
                    for (int i = 0, at = 0; at < length; ++i, at += controlInterval)
                    {
                        const auto beats = followSong ? modulation.ppq + (samplesIntoBlock + start + at) * beatsPerSample : naviClock + at * beatsPerSample;
                        songStep = (std::int64_t) std::floor (beats / beatsPerStep + 1.0e-9);
                        naviMoves[(size_t) i] = navi::moveAt (nv, songStep);
                    }
                    naviStepShown.store ((int) (((songStep % navi::numSteps) + navi::numSteps) % navi::numSteps));
                    // GLIDE: a slide over up to 40% of a step (a time constant a tenth of that share).
                    const auto tau = 0.1 * juce::jlimit (0.0f, 1.0f, nv.glide) * beatsPerStep * 60.0 / juce::jmax (1.0, modulation.bpm);
                    naviGlideCoef = tau <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-(double) controlInterval / (tau * sampleRate)));
                }
                else
                {
                    naviStepShown.store (-1);
                    naviGlideCoef = 1.0f;
                }
                naviClock += length * beatsPerSample;
            }
            // CROSSROADS (0.90.0): which side each interval is on, in time with the song (or its own count).
            {
                const auto& cr = settings.cross;
                crossSteps = cr.on ? crossroads::stepsFor (cr.amount, cr.pull) : 0;
                crossPulling = crossSteps > 0;
                const auto beatsPerSample = juce::jmax (1.0, modulation.bpm) / 60.0 / sampleRate;
                const auto followSong = modulation.ppq >= 0.0;
                auto dark = false;
                for (int i = 0, at = 0; at < length; ++i, at += controlInterval)
                {
                    const auto beats = followSong ? modulation.ppq + (samplesIntoBlock + start + at) * beatsPerSample : crossClock + at * beatsPerSample;
                    dark = crossroads::darkAt (beats, cr.length);
                    crossDark[(size_t) i] = dark ? 1 : 0;
                }
                crossSideShown.store (cr.on ? (dark ? 1 : 0) : -1);
                const auto tau = crossroads::bendSeconds (cr.bend);
                crossGlideCoef = (float) (1.0 - std::exp (-(double) controlInterval / (tau * sampleRate)));
                crossClock += length * beatsPerSample;
            }
            // RAGTIME (0.92.0): which interval each strike of the roll falls in, and the ones its damper lifts in
            // (about 4 ms before it), in time with the song (or its own count); the bass is the lowest key held.
            ragPlaying = settings.ragtime.playing();
            {
                const auto& rt = settings.ragtime;
                const auto beatsPerSample = juce::jmax (1.0, modulation.bpm) / 60.0 / sampleRate;
                const auto followSong = modulation.ppq >= 0.0;
                if (ragPlaying)
                {
                    const auto beatsPerStep = rt.beatsPerStep();
                    const auto liftSteps = 0.004 * juce::jmax (1.0, modulation.bpm) / 60.0 / beatsPerStep;
                    ragBass = -1;
                    for (const auto& v : voices)
                        if (v.playing && ! v.killing && v.source == 0 && (v.held || v.sustained) && (ragBass < 0 || v.note < ragBass))
                            ragBass = v.note;
                    ragChords = 0;   // the notes over the bass (a lone note plays both hands' steps)
                    for (const auto& v : voices)
                        if (v.playing && ! v.killing && v.source == 0 && (v.held || v.sustained) && v.note != ragBass)
                            ++ragChords;
                    std::int64_t lastStep = 0;
                    for (int i = 0, at = 0; at < length; ++i, at += controlInterval)
                    {
                        const auto n = juce::jmin (controlInterval, length - at);
                        const auto beatsAt = [&] (int offset) { return followSong ? modulation.ppq + (samplesIntoBlock + start + offset) * beatsPerSample
                                                                                  : ragClock + offset * beatsPerSample; };
                        const auto from = beatsAt (at) / beatsPerStep, to = beatsAt (at + n) / beatsPerStep;
                        std::uint8_t strike = 0, lift = 0;
                        const auto k = ragtime::nextStrike (from, rt.swing);
                        if (ragtime::strikeTime (k, rt.swing) < to && k != ragLastStep)
                        {
                            ragLastStep = k;   // (a step on a piece's edge can fall in both: strike it once)
                            const auto hit = ragtime::strikeAt (rt, k);
                            strike = (std::uint8_t) (hit.who | (hit.accent ? 4 : 0));
                        }
                        // A strike just ahead: the damper lifts for it now.
                        const auto ahead = ragtime::nextStrike (to, rt.swing);
                        if (ragtime::strikeTime (ahead, rt.swing) - to < liftSteps)
                            lift = (std::uint8_t) ragtime::strikeAt (rt, ahead).who;
                        ragStrikes[(size_t) i] = strike;
                        ragLifts[(size_t) i] = lift;
                        lastStep = (std::int64_t) std::floor (from + 1.0e-9);
                    }
                    ragStepShown.store ((int) (((lastStep % ragtime::numSteps) + ragtime::numSteps) % ragtime::numSteps));
                }
                else
                    ragStepShown.store (-1);
                ragClock += length * beatsPerSample;
            }
            for (auto& voice : voices)
            {
                if (! voice.playing)
                    continue;
                for (int done = 0, interval = 0; done < length && voice.playing; done += controlInterval, ++interval)
                {
                    intervalSeconds = done / sampleRate;
                    renderVoice (voice, left + start + done, right + start + done, juce::jmin (controlInterval, length - done), settings, interval);
                }
            }
            for (auto& clock : travelClock)
                clock += length / sampleRate;
        }
        samplesIntoBlock += numSamples;
        sampleClock += numSamples;
    }

    void Engine::startLfos (Voice& voice, const Settings& settings) noexcept
    {
        for (int k = 0; k < mod::numLfos; ++k)
            voice.lfos[(size_t) k].start (settings.modulation.lfos[(size_t) k], random);
        for (auto& envelope : voice.envelopes)
            envelope.start();
        voice.pitchEnvelope.start();
        voice.seqPosition = voice.seqAge = 0.0;
        voice.seqValue = 0.0f;
        voice.randomValue = random.nextFloat();
        voice.pressure = 0.0f;
        voice.lagFresh = true;
    }

    float Engine::wholeSynthValue (mod::Source source, const Settings& settings) const noexcept
    {
        using S = mod::Source;
        const auto& modulation = settings.modulation;
        if (source >= S::lfo1 && source <= S::lfo4)
        {
            const auto k = (size_t) source - (size_t) S::lfo1;
            return modulation.lfos[k].trigger == mod::LfoTrigger::voice ? std::numeric_limits<float>::quiet_NaN() : lastGlobalLfo[k];
        }
        if (source >= S::env1 && source <= S::env8)
        {
            const auto k = (size_t) source - (size_t) S::env1;
            return modulation.envelopes[k].trigger == mod::EnvelopeTrigger::voice ? std::numeric_limits<float>::quiet_NaN() : lastGlobalEnvelope[k];
        }
        switch (source)
        {
            case S::macro1: case S::macro2: case S::macro3: return modulation.macros[(size_t) source - (size_t) S::macro1];
            case S::button1: return modulation.buttons[0] ? 1.0f : 0.0f;
            case S::button2: return modulation.buttons[1] ? 1.0f : 0.0f;
            case S::modWheel:
            {
                auto wheel = modWheel;
                const auto& toWheel = modulation.toModWheel;
                if (toWheel[0]) wheel = juce::jmax (wheel, breath);
                if (toWheel[1]) wheel = juce::jmax (wheel, expression);
                if (toWheel[2]) wheel = juce::jmax (wheel, channelPressure);
                if (toWheel[3] && pedalDown) wheel = 1.0f;
                return wheel;
            }
            case S::pitchBend: return bend;
            case S::aftertouch: return channelPressure;
            case S::breath: return breath;
            case S::expression: return expressionSource();
            case S::sustain: return pedalDown ? 1.0f : 0.0f;
            default: return std::numeric_limits<float>::quiet_NaN();   // each note has its own
        }
    }

    const Settings& Engine::modulate (Voice& voice, int n, const Settings& settings, int interval) noexcept
    {
        const auto& modulation = settings.modulation;
        voiceSettings = settings;

        // Each source's value for this note now.
        std::array<float, mod::numSources> values {};
        const auto seconds = (double) n / sampleRate;
        const auto at = (size_t) juce::jlimit (0, maxIntervals - 1, interval);
        for (int k = 0; k < mod::numLfos; ++k)
        {
            const auto& lfo = modulation.lfos[(size_t) k];
            if (! modulation.uses ((mod::Source) ((int) mod::Source::lfo1 + k)))
                continue;
            values[(size_t) mod::Source::lfo1 + (size_t) k] = lfo.trigger == mod::LfoTrigger::voice
                ? voice.lfos[(size_t) k].next (lfo, lfo.cyclesPerSecond (modulation.bpm), seconds, random, modulation.lfoShapes)
                : globalLfoValues[(size_t) k][at];
        }
        if (modulation.envelopeShapes != nullptr)
            for (int k = 0; k < mod::numEnvelopes; ++k)
            {
                const auto& envelope = modulation.envelopes[(size_t) k];
                if (! modulation.uses ((mod::Source) ((int) mod::Source::env1 + k)))
                    continue;
                values[(size_t) mod::Source::env1 + (size_t) k] = envelope.trigger == mod::EnvelopeTrigger::voice
                    ? voice.envelopes[(size_t) k].next (modulation.envelopeShapes[k], envelope, seconds, modulation.bpm)
                    : globalEnvelopeValues[(size_t) k][at];
            }
        for (int k = 0; k < 3; ++k)
            values[(size_t) mod::Source::macro1 + (size_t) k] = modulation.macros[(size_t) k];
        values[(size_t) mod::Source::button1] = modulation.buttons[0] ? 1.0f : 0.0f;
        values[(size_t) mod::Source::button2] = modulation.buttons[1] ? 1.0f : 0.0f;
        auto wheel = modWheel;
        const auto& toWheel = modulation.toModWheel;
        if (toWheel[0]) wheel = juce::jmax (wheel, breath);
        if (toWheel[1]) wheel = juce::jmax (wheel, expression);
        if (toWheel[2]) wheel = juce::jmax (wheel, juce::jmax (channelPressure, voice.pressure));
        if (toWheel[3] && pedalDown) wheel = 1.0f;
        values[(size_t) mod::Source::modWheel] = wheel;
        values[(size_t) mod::Source::pitchBend] = bend;
        values[(size_t) mod::Source::velocity] = voice.velocity;
        values[(size_t) mod::Source::aftertouch] = juce::jmax (channelPressure, voice.pressure);
        values[(size_t) mod::Source::keytrack] = (float) voice.note / 127.0f;
        values[(size_t) mod::Source::breath] = breath;
        values[(size_t) mod::Source::expression] = expressionSource();
        values[(size_t) mod::Source::sustain] = pedalDown ? 1.0f : 0.0f;
        values[(size_t) mod::Source::random] = voice.randomValue;
        values[(size_t) mod::Source::pitchEnvelope] = voice.pitchEnvelopeValue;
        values[(size_t) mod::Source::stepSeq] = voice.seqValue;

        // Each route, in turn. Routes to the same destination come together
        // (the processor sorts them), each pushing on from where the last left it.
        float position = 0.0f;
        const mod::Route* previous = nullptr;
        for (int r = 0; r < juce::jmin (modulation.numRoutes, mod::maxRoutes); ++r)
        {
            const auto& route = modulation.routes[r];
            if (route.range == nullptr || route.source == mod::Source::none)
                continue;
            auto source = mod::bend (values[(size_t) route.source], route.curve);
            auto& lagged = voice.lagged[(size_t) r];
            if (voice.lagFresh || route.lagMs <= 0.0f)
                lagged = source;
            else
                lagged += (source - lagged) * (1.0f - std::exp (-(float) n / (route.lagMs * 0.001f * (float) sampleRate)));
            source = lagged;

            const auto sameDestination = previous != nullptr && previous->target.field == route.target.field && previous->target.index == route.target.index;
            if (! sameDestination)
                position = route.base;
            position = mod::combine (position, route.amount * source, route.percent);
            mod::apply (voiceSettings, route.target, route.range->convertFrom0to1 (position));
            previous = &route;
        }
        voice.lagFresh = false;
        return voiceSettings;
    }

    void Engine::renderVoice (Voice& voice, float* left, float* right, int n, const Settings& unmodulated, int interval) noexcept
    {
        // RAGTIME (0.92.0): a key's note struck again on the roll's step (unless it has only just started: its own
        // attack is the strike), the damper lifting it just before; nothing at all while it's off.
        auto ragTarget = 1.0f;
        if (ragPlaying && voice.source == 0 && (voice.held || voice.sustained) && ! voice.killing)
        {
            const auto slot = (size_t) juce::jlimit (0, maxIntervals - 1, interval);
            const int mine = voice.note != ragBass ? ragtime::chord : ragChords > 0 ? ragtime::bass : ragtime::both;
            const auto strike = ragStrikes[slot];
            if ((strike & mine) != 0 && voice.seqAge > 0.01)
            {
                const auto& rt = unmodulated.ragtime;
                restrike (voice, rt.strikeVelocity (voice.ragVelocity, (strike & 4) != 0), unmodulated);
            }
            else if ((ragLifts[slot] & mine) != 0)
                ragTarget = 1.0f - unmodulated.ragtime.liftDepth();
        }
        // The PITCH module's envelope (also a source for the matrix).
        const auto& pitchModule = unmodulated.pitch;
        if (pitchModule.shape != nullptr
            && (pitchModule.envelopeOn || (unmodulated.modulation.numRoutes > 0 && unmodulated.modulation.uses (mod::Source::pitchEnvelope))))
            voice.pitchEnvelopeValue = 2.0f * voice.pitchEnvelope.next (*pitchModule.shape, pitchModule.envelope, (double) n / sampleRate,
                                                                        unmodulated.modulation.bpm) - 1.0f;

        // The STEP SEQUENCER for this interval: its value at each sample (the whole
        // synth's, or this note's own), and how far DELAY and FADE IN let it in.
        const auto& stepSeq = unmodulated.stepSeq;
        auto seqStart = 0.0f, seqEnd = 0.0f;
        if (seqActive && stepSeq.plays (voice.note))
        {
            seqStart = seqDepth (voice.seqAge, stepSeq);
            seqEnd = seqDepth (voice.seqAge + (double) n / sampleRate, stepSeq);
            if (stepSeq.trigger == seq::Trigger::voice)
                for (int i = 0; i < n; ++i)
                {
                    const auto [l, r] = seqPrepared.values (voice.seqPosition + i * seqStepsPerSample);
                    seqLeft[(size_t) i] = l;
                    seqRight[(size_t) i] = r;
                }
            else
            {
                const auto offset = (size_t) juce::jlimit (0, maxIntervals - 1, interval) * controlInterval;
                std::copy_n (seqValues[0].begin() + (std::ptrdiff_t) offset, n, seqLeft.begin());
                std::copy_n (seqValues[1].begin() + (std::ptrdiff_t) offset, n, seqRight.begin());
            }
            voice.seqValue = seqStart * 0.5f * (seqLeft[0] + seqRight[0]);
            voice.seqPosition += n * seqStepsPerSample;
        }
        else
            voice.seqValue = 0.0f;
        voice.seqAge += (double) n / sampleRate;
        const auto gating = seqActive && stepSeq.on && seqEnd + seqStart > 0.0f;

        // With modulation, this note plays its own copy of the settings.
        const auto modulated = unmodulated.modulation.numRoutes > 0;
        baseSettings = &unmodulated;
        const auto& settings = modulated ? modulate (voice, n, unmodulated, interval) : unmodulated;
        // The PITCH envelope in semitones (RANGE can be moved by the matrix).
        voice.pitchSemitones = pitchModule.envelopeOn && pitchModule.shape != nullptr ? voice.pitchEnvelopeValue * settings.pitch.rangeSemitones : 0.0f;
        // NAVICOMPUTER: this note's move now (in its own steps of the scale), glided; nothing at all while it's off.
        if (naviMoving || voice.naviSemis != 0.0f)
        {
            const auto target = naviMoving ? (float) navi::semitonesFor (voice.note, naviMoves[(size_t) juce::jlimit (0, maxIntervals - 1, interval)], unmodulated.navi)
                                           : 0.0f;
            if (voice.naviFresh || naviGlideCoef >= 1.0f)
                voice.naviSemis = target;
            else
            {
                voice.naviSemis += (target - voice.naviSemis) * naviGlideCoef;
                if (std::abs (target - voice.naviSemis) < 1.0e-4f)
                    voice.naviSemis = target;
            }
        }
        voice.naviFresh = false;
        // CROSSROADS: on the dark side this note pulled down the scale (in key), slid into; nothing at all while it's off.
        if (crossPulling || voice.crossSemis != 0.0f)
        {
            const auto& cr = unmodulated.cross;
            const auto dark = crossPulling && crossDark[(size_t) juce::jlimit (0, maxIntervals - 1, interval)] != 0;
            const auto target = dark ? (float) crossroads::semitonesFor (voice.note, crossSteps, cr.key, cr.scale) : 0.0f;
            if (voice.crossFresh)
                voice.crossSemis = target;
            else
            {
                voice.crossSemis += (target - voice.crossSemis) * crossGlideCoef;
                if (std::abs (target - voice.crossSemis) < 1.0e-4f)
                    voice.crossSemis = target;
            }
        }
        voice.crossFresh = false;
        if (modulated)
            balance (voice.notePan + settings.amp.pan - unmodulated.amp.pan, voice.panLeft, voice.panRight);

        const auto& filterSettings = settings.filter;
        const auto& ampSettings = settings.amp;
        // AFTERGLOW: this note's own tuning, filter and envelope times (none of it while it's off or at 0).
        const auto& glow = settings.afterglow;
        const auto glowing = unmodulated.afterglow.on && glow.active();
        auto glowSemitones = 0.0f, glowOctaves = 0.0f;
        if (glowing)
        {
            driftOn (voice, n);
            glowSemitones = 0.01f * (glow.age * Settings::Afterglow::ageCents * voice.glowTune
                                     + glow.drift * Settings::Afterglow::driftCents * voice.driftTune);
            glowOctaves = glow.age * Settings::Afterglow::ageOctaves * voice.glowCutoff
                        + glow.drift * Settings::Afterglow::driftOctaves * voice.driftCutoff;
        }
        if (glowing && glow.age > 0.0f)
        {
            // Each note's envelopes a little faster or slower than the others' (all its stages by one factor).
            const auto scale = std::exp2 (glow.age * Settings::Afterglow::ageTimeOctaves * voice.glowTime);
            auto amp = ampSettings.envelope;
            auto filt = filterSettings.envelope;
            for (auto* e : { &amp, &filt })
            {
                e->attackMs *= scale;
                e->decayMs *= scale;
                e->releaseMs *= scale;
            }
            voice.amp.update (amp);
            voice.filter.update (filt);
        }
        else
        {
            voice.amp.update (ampSettings.envelope);
            voice.filter.update (filterSettings.envelope);
        }

        // Glide, once per interval: straight, or along GLIDE CURVE's path.
        if (voice.glideRate > 0.0f && ! juce::exactlyEqual (settings.pitch.glideCurve, 0.0f))
        {
            voice.glideProgress = juce::jmin (1.0f, voice.glideProgress + voice.glideStep * (float) n);
            voice.pitch = voice.glideFrom + (voice.targetPitch - voice.glideFrom) * synth::bend (voice.glideProgress, settings.pitch.glideCurve);
            if (voice.glideProgress >= 1.0f)
            {
                voice.pitch = voice.targetPitch;
                voice.glideRate = 0.0f;
            }
        }
        else if (voice.glideRate > 0.0f)
        {
            const auto step = voice.glideRate * (float) n;
            if (std::abs (voice.targetPitch - voice.pitch) <= step)
            {
                voice.pitch = voice.targetPitch;
                voice.glideRate = 0.0f;
            }
            else
                voice.pitch += voice.targetPitch > voice.pitch ? step : -step;
        }
        const auto wheel = bendValues[(size_t) juce::jlimit (0, maxIntervals - 1, interval)];
        auto bendSemitones = wheel >= 0.0f ? wheel * settings.play.bendUp : wheel * settings.play.bendDown;
        if (glowing)
            bendSemitones += glowSemitones;

        // Each oscillator's route this interval, and the path it plays into.
        assignPaths (voice, settings);
        for (size_t p = 0; p < pathUsed.size(); ++p)
            if (pathUsed[p])
            {
                std::fill (pathLeft[p].begin(), pathLeft[p].begin() + n, 0.0f);
                std::fill (pathRight[p].begin(), pathRight[p].begin() + n, 0.0f);
            }
        renderOscillators (voice, n, settings, bendSemitones);

        // Every oscillator in it taken by newer notes: the note is over.
        if (std::none_of (voice.oscillators.begin(), voice.oscillators.end(), [] (const OscState& o) { return o.active; }))
        {
            free (voice);
            return;
        }

        // The FILTER's cutoff for the end of this interval (each path glides to it from where it was).
        const auto velocityDown = 1.0f - voice.velocity;
        const auto filterEnvelope = voice.filter.step (n);
        FilterChannel::Coefficients filterTarget;
        if (filterSettings.on)
        {
            auto octaves = filterSettings.keytrack * (voice.pitch - 60.0f) / 12.0f
                         - filterSettings.velocity * filterVelocityOctaves * velocityDown
                         + filterSettings.envelopeAmount * filterEnvelopeOctaves * filterEnvelope
                               * (1.0f - filterSettings.envelopeVelocity * velocityDown);
            if (glowing)
                octaves += glowOctaves;
            const auto cutoff = (double) filterSettings.cutoffHz * std::exp2 ((double) octaves);
            const auto resonance = filterSettings.resonance * (1.0f - filterSettings.resonanceVelocity * velocityDown);
            filterTarget = FilterChannel::Coefficients::make (cutoff, resonance, sampleRate);
        }

        // The VOICE filter's resonators for this note and its LEVELLING, worked out again when the note has moved
        // a quarter tone or its VOWEL, SIZE or SHARP have changed (by the knobs or the matrix).
        if (settings.voice.on)
        {
            const auto& v = settings.voice;
            const std::array<float, 3> wanted { v.vowel, v.size, v.sharp };
            if (wanted != voice.vocalFor || std::abs (voice.pitch - voice.vocalPitch) > 0.5f)
            {
                const auto noteHz = 440.0 * std::exp2 ((voice.pitch - 69.0) / 12.0);
                voice.vocalCoefficients = vocal::Coefficients::make (v.vowel, v.size, v.sharp, sampleRate, noteHz);
                voice.vocalLevel = vocal::levelling (voice.vocalCoefficients, noteHz, sampleRate);
                voice.vocalPitch = voice.pitch;
                voice.vocalFor = wanted;
            }
        }
        const auto& vocalTarget = voice.vocalCoefficients;

        // Each path through its shapers and filter, then all of them together (and, 7b, into the sends at each path's amount).
        std::fill (mixLeft.begin(), mixLeft.begin() + n, 0.0f);
        std::fill (mixRight.begin(), mixRight.begin() + n, 0.0f);
        std::array<bool, numSends> sending {};
        for (int s = 0; s < numSends; ++s)
            if ((sending[(size_t) s] = sendsWanted (unmodulated, s)))
            {
                std::fill (sendMixLeft[(size_t) s].begin(), sendMixLeft[(size_t) s].begin() + n, 0.0f);
                std::fill (sendMixRight[(size_t) s].begin(), sendMixRight[(size_t) s].begin() + n, 0.0f);
            }
        // Stage 7d: paths on an insert bus go into that bus's mix instead (and skip their own sends: the bus sends).
        std::array<bool, numInsertBuses> busing {};
        for (size_t p = 0; p < pathUsed.size(); ++p)
            if (pathUsed[p])
                if (const auto bus = busOf (voice.paths[p].key); bus > 0)
                    if (! busing[(size_t) bus - 1])
                    {
                        busing[(size_t) bus - 1] = true;
                        std::fill (busMixLeft[(size_t) bus - 1].begin(), busMixLeft[(size_t) bus - 1].begin() + n, 0.0f);
                        std::fill (busMixRight[(size_t) bus - 1].begin(), busMixRight[(size_t) bus - 1].begin() + n, 0.0f);
                    }
        for (size_t p = 0; p < pathUsed.size(); ++p)
        {
            if (! pathUsed[p])
                continue;
            processPath (voice, voice.paths[p], pathLeft[p].data(), pathRight[p].data(), n, settings, filterTarget, vocalTarget, voice.vocalLevel);
            if (const auto bus = busOf (voice.paths[p].key); bus > 0)
            {
                for (int i = 0; i < n; ++i)
                {
                    busMixLeft[(size_t) bus - 1][(size_t) i] += pathLeft[p][(size_t) i];
                    busMixRight[(size_t) bus - 1][(size_t) i] += pathRight[p][(size_t) i];
                }
                continue;
            }
            for (int i = 0; i < n; ++i)
            {
                mixLeft[(size_t) i] += pathLeft[p][(size_t) i];
                mixRight[(size_t) i] += pathRight[p][(size_t) i];
            }
            for (int s = 0; s < numSends; ++s)
                if (sending[(size_t) s])
                    if (const auto amount = sendGainOf (voice.paths[p].key, s); amount > 0.0f)
                        for (int i = 0; i < n; ++i)
                        {
                            sendMixLeft[(size_t) s][(size_t) i] += pathLeft[p][(size_t) i] * amount;
                            sendMixRight[(size_t) s][(size_t) i] += pathRight[p][(size_t) i] * amount;
                        }
        }
        const auto offset = left - outputBase;   // this interval's place in the block (and so in the sends)

        // The AMP: envelope, velocity, SPIKE, VOLUME, then this note's pan.
        const auto velocityGain = 1.0f - ampSettings.velocity * velocityDown;
        // SIROCCO (0.58.0): an answer at its LEVEL (layer by layer).
        const auto answerGain = voice.source == answerSource ? sirocco::layerGain (settings.sirocco.level, voice.layer, settings.sirocco.voices) : 1.0f;
        const auto volume = ampSettings.volume * velocityGain * answerGain;
        const auto spike = ampSettings.spike * spikeMaxGain * (1.0f - ampSettings.spikeVelocity * velocityDown);
        const auto spikeStep = 1.0f / juce::jmax (1.0f, spikeMs * 0.001f * (float) sampleRate);
        const auto tail = voice.source == tailSource;   // SFZ release samples: past the AMP's envelope, to their end
        for (int i = 0; i < n; ++i)
        {
            auto gain = tail ? volume : voice.amp.next() * volume;
            if (ragTarget != 1.0f || voice.ragGain != 1.0f)
            {
                // RAGTIME's damper: down for the strike, back with it.
                voice.ragGain += (ragTarget - voice.ragGain) * (ragTarget < voice.ragGain ? ragDown : ragUp);
                if (ragTarget == 1.0f && voice.ragGain > 0.9999f)
                    voice.ragGain = 1.0f;
                gain *= voice.ragGain;
            }
            if (voice.spikePosition < 1.0f && ! tail)
            {
                gain *= 1.0f + spike * (1.0f - synth::bend (voice.spikePosition, ampSettings.spikeCurve));
                voice.spikePosition += spikeStep;
            }
            if (voice.killing)
            {
                gain *= voice.killGain;
                voice.killGain -= killStep;
                if (voice.killGain <= 0.0f)
                    voice.killGain = 0.0f;
            }
            auto gainLeft = gain, gainRight = gain;
            if (gating)
            {
                // The trance gate: the volume down by VOL MIX where the step sequencer is low.
                const auto depth = settings.stepSeq.volMix * (seqStart + (seqEnd - seqStart) * (float) i / (float) n);
                gainLeft *= 1.0f - depth * (1.0f - seqLeft[(size_t) i]);
                gainRight *= 1.0f - depth * (1.0f - seqRight[(size_t) i]);
            }
            left[i] += mixLeft[(size_t) i] * gainLeft * voice.panLeft;
            right[i] += mixRight[(size_t) i] * gainRight * voice.panRight;
            for (int b = 0; b < numInsertBuses; ++b)
                if (busing[(size_t) b])
                {
                    insertBaseLeft[(size_t) b][offset + i] += busMixLeft[(size_t) b][(size_t) i] * gainLeft * voice.panLeft;
                    insertBaseRight[(size_t) b][offset + i] += busMixRight[(size_t) b][(size_t) i] * gainRight * voice.panRight;
                }
            for (int s = 0; s < numSends; ++s)
                if (sending[(size_t) s])
                {
                    sendBaseLeft[(size_t) s][offset + i] += sendMixLeft[(size_t) s][(size_t) i] * gainLeft * voice.panLeft;
                    sendBaseRight[(size_t) s][offset + i] += sendMixRight[(size_t) s][(size_t) i] * gainRight * voice.panRight;
                }
        }

        // Over once it can't be heard.
        const auto& amp = voice.amp;
        if (tail)
        {
            if (voice.killing && voice.killGain <= 0.0f)
                free (voice);
        }
        else if (! amp.isActive()
            || (voice.killing && voice.killGain <= 0.0f)
            || (amp.isReleasing() && amp.getLevel() < silence)
            || amp.isSilentSustain())
            free (voice);
    }

    //==========================================================================
    // The ARPEGGIATOR (stage 5a)
    void Engine::updateArpMasks (const Settings& settings) noexcept
    {
        arpMasks.fill (0);
        keyMask = 0;
        for (size_t i = 0; i < settings.oscillators.size(); ++i)
        {
            const auto a = settings.oscillators[i].arp - 1;
            const auto bit = (std::uint8_t) (1u << i);
            if (a >= 0 && a < arp::numArps && settings.arps[(size_t) a].on)
                arpMasks[(size_t) a] |= bit;
            else
                keyMask |= bit;
        }
        // An arp switched off (or left with no oscillators) stops.
        for (int a = 0; a < arp::numArps; ++a)
            if (! arpActive (a) && (arps[(size_t) a].running || arps[(size_t) a].numHeld > 0))
            {
                arps[(size_t) a].numHeld = 0;
                stopArpIfEmpty (a);
            }
    }

    void Engine::arpKeyDown (int note, float velocity, const Settings& settings) noexcept
    {
        for (int a = 0; a < arp::numArps; ++a)
        {
            if (! arpActive (a) || ! settings.arps[(size_t) a].takes (note))
                continue;
            auto& st = arps[(size_t) a];
            st.pedalHeld[(size_t) note] = false;
            const auto wasEmpty = st.numHeld == 0;
            auto* end = st.held.data() + st.numHeld;
            if (auto* found = std::find_if (st.held.data(), end, [note] (const arp::Held& h) { return h.note == note; }); found != end)
                found->velocity = velocity;   // the same key again
            else if (st.numHeld < arp::maxHeld)
                st.held[(size_t) st.numHeld++] = { note, velocity, ++st.pressCounter };
            st.lastVelocity = velocity;
            if (! wasEmpty)
                continue;
            // The first key: it starts. In step with a playing song, it waits for the next step, unless it came
            // on a step or just after it (up to 30 ms, or a quarter of a step if that's shorter: a chord written on
            // the beat in the DAW, or played a hair late), when it plays that step now;
            // otherwise it starts now, from the first step.
            st.firstVelocity = velocity;
            st.running = true;
            st.noteCounter = 0;
            if (arpFollowsSong (a, settings))
            {
                const auto step = arpStepAt (a, sampleClock, settings);
                const auto late = (double) (sampleClock - arpStepTime (a, step, settings));
                const auto samplesPerStep = settings.arps[(size_t) a].beatsPerStep() * 60.0 / juce::jmax (1.0, settings.modulation.bpm) * sampleRate;
                st.lastStep = step >= 0 && late <= juce::jmin (0.03 * sampleRate, 0.25 * samplesPerStep) ? step - 1 : step;
            }
            else if (settings.arpsTogether && numKeysDown > 1)
            {
                // ARPS TOGETHER: on the clock the first key started; in at its next step.
                st.startClock = sharedStartClock;
                st.lastStep = arpStepAt (a, sampleClock, settings);
            }
            else
            {
                st.startClock = settings.arpsTogether ? sharedStartClock : sampleClock;
                st.lastStep = -1;
            }
        }
    }

    void Engine::removeArpKey (ArpState& st, int note) noexcept
    {
        auto* end = st.held.data() + st.numHeld;
        if (auto* found = std::find_if (st.held.data(), end, [note] (const arp::Held& h) { return h.note == note; }); found != end)
        {
            std::move (found + 1, end, found);
            --st.numHeld;
        }
    }

    void Engine::arpKeyUp (int note) noexcept
    {
        for (int a = 0; a < arp::numArps; ++a)
        {
            auto& st = arps[(size_t) a];
            if (pedalDown)
            {
                if (std::any_of (st.held.begin(), st.held.begin() + st.numHeld, [note] (const arp::Held& h) { return h.note == note; }))
                    st.pedalHeld[(size_t) note] = true;
                continue;
            }
            removeArpKey (st, note);
            stopArpIfEmpty (a);
        }
    }

    void Engine::stopArpIfEmpty (int a) noexcept
    {
        auto& st = arps[(size_t) a];
        if (st.numHeld > 0 || ! st.running)
            return;
        st.running = false;
        // Its notes still to start don't; those sounding end as they were going to.
        auto kept = 0;
        for (int i = 0; i < numArpEvents; ++i)
            if (arpEvents[(size_t) i].arp != a || ! arpEvents[(size_t) i].on)
                arpEvents[(size_t) kept++] = arpEvents[(size_t) i];
        numArpEvents = kept;
        // A note gliding on has nothing to glide into: it ends.
        if (st.slideAge != 0)
        {
            for (auto& voice : voices)
                if (voice.playing && voice.age == st.slideAge && ! voice.killing)
                {
                    voice.held = false;
                    releaseVoice (voice);
                }
            st.slideAge = 0;
            st.slidePair = 0;
        }
    }

    bool Engine::arpFollowsSong (int a, const Settings& settings) const noexcept
    {
        return settings.arps[(size_t) a].trigger == arp::Trigger::song && settings.modulation.ppq >= 0.0;
    }

    std::int64_t Engine::arpStepTime (int a, std::int64_t step, const Settings& settings) const noexcept
    {
        // Where a step starts, in steps: every second one late by SHUFFLE (half a step at most).
        const auto& s = settings.arps[(size_t) a];
        const auto shift = 0.5 * juce::jlimit (0.0, 1.0, (double) settings.stepSeq.shuffle);
        const auto position = (double) step + ((step & 1) != 0 ? shift : 0.0);
        const auto bpm = juce::jmax (1.0, settings.modulation.bpm);
        if (arpFollowsSong (a, settings))
        {
            const auto beatsPerSample = bpm / 60.0 / sampleRate;
            return blockStartClock + (std::int64_t) std::ceil ((position * s.beatsPerStep() - settings.modulation.ppq) / beatsPerSample - 1.0e-6);
        }
        const auto samplesPerStep = s.beatsPerStep() * 60.0 / bpm * sampleRate;
        return arps[(size_t) a].startClock + (std::int64_t) std::ceil (position * samplesPerStep - 1.0e-6);
    }

    std::int64_t Engine::arpStepAt (int a, std::int64_t time, const Settings& settings) const noexcept
    {
        const auto& s = settings.arps[(size_t) a];
        const auto bpm = juce::jmax (1.0, settings.modulation.bpm);
        double position = 0.0;
        if (arpFollowsSong (a, settings))
            position = (settings.modulation.ppq + (double) (time - blockStartClock) * bpm / 60.0 / sampleRate) / s.beatsPerStep();
        else
            position = (double) (time - arps[(size_t) a].startClock) / (s.beatsPerStep() * 60.0 / bpm * sampleRate);
        auto step = (std::int64_t) std::floor (position);
        while (arpStepTime (a, step + 1, settings) <= time)
            ++step;
        while (step >= 0 && arpStepTime (a, step, settings) > time)
            --step;
        return step;
    }

    std::int64_t Engine::nextArpTime (const Settings& settings) const noexcept
    {
        auto next = std::numeric_limits<std::int64_t>::max();
        for (int i = 0; i < numArpEvents; ++i)
            next = juce::jmin (next, arpEvents[(size_t) i].time);
        for (int i = 0; i < numPendingAnswers; ++i)   // SIROCCO's answers after their DELAY
            next = juce::jmin (next, pendingAnswers[(size_t) i].time);
        for (int a = 0; a < arp::numArps; ++a)
            if (arpActive (a) && arps[(size_t) a].running)
                next = juce::jmin (next, arpStepTime (a, arps[(size_t) a].lastStep + 1, settings));
        return next;
    }

    void Engine::runArps (const Settings& settings) noexcept
    {
        // Steps due now.
        for (int a = 0; a < arp::numArps; ++a)
        {
            auto& st = arps[(size_t) a];
            if (! arpActive (a) || ! st.running)
                continue;
            const auto step = arpStepAt (a, sampleClock, settings);
            if (step < st.lastStep)
                st.lastStep = step - 1;   // the song jumped back
            if (step > st.lastStep && step >= 0)
            {
                st.lastStep = step;
                playArpStep (a, step, juce::jmax (sampleClock, arpStepTime (a, step, settings)), settings);
            }
        }
        // Notes due now, in time order (at the same time, ends before starts).
        for (;;)
        {
            auto best = -1;
            for (int i = 0; i < numArpEvents; ++i)
            {
                const auto& e = arpEvents[(size_t) i];
                if (e.time > sampleClock)
                    continue;
                if (best < 0 || e.time < arpEvents[(size_t) best].time
                    || (e.time == arpEvents[(size_t) best].time && ! e.on && arpEvents[(size_t) best].on))
                    best = i;
            }
            if (best < 0)
                break;
            const auto e = arpEvents[(size_t) best];
            arpEvents[(size_t) best] = arpEvents[(size_t) --numArpEvents];
            fireArpEvent (e, settings);
        }
    }

    void Engine::scheduleArp (const ArpEvent& e) noexcept
    {
        if (numArpEvents < maxArpEvents)
            arpEvents[(size_t) numArpEvents++] = e;
    }

    void Engine::playArpStep (int a, std::int64_t step, std::int64_t at, const Settings& settings) noexcept
    {
        const auto& s = settings.arps[(size_t) a];
        auto& st = arps[(size_t) a];
        static const auto fallback = arp::Patterns::makeDefault();
        const auto& patterns = s.patterns != nullptr ? *s.patterns : fallback;

        // AUTODICE: a pattern rolled afresh from the randomizer at the start of each cycle (not with CHAIN).
        const arp::Pattern* live = nullptr;
        if (s.randomizer != nullptr && s.randomizer->autoDice && ! s.chain)
        {
            const auto length = juce::jlimit (2, arp::maxSteps, s.length);
            const auto cycle = step / length;
            if (liveCycles[(size_t) a] != cycle)
            {
                const auto& chosen = patterns.patterns[(size_t) juce::jlimit (0, arp::numPatterns - 1, s.pattern)];
                livePatterns[(size_t) a] = arp::roll (*s.randomizer, length, chosen, (juce::int64) s.randomizer->seed * 7919 + cycle + 1);
                liveCycles[(size_t) a] = cycle;
            }
            live = &livePatterns[(size_t) a];
        }

        arp::KeysState keys { st.held.data(), st.numHeld, &st.noteCounter, st.firstVelocity, st.lastVelocity };
        std::array<arp::Played, arp::maxPlayed> played {};
        const auto samplesPerStep = s.beatsPerStep() * 60.0 / juce::jmax (1.0, settings.modulation.bpm) * sampleRate;
        const auto count = arp::playBlock (s, arp::blockFor (s, patterns, step, live), samplesPerStep, keys, random, played.data(), (int) played.size());
        for (int i = 0; i < count; ++i)
        {
            const auto& p = played[(size_t) i];
            const auto pair = nextPair++;
            scheduleArp ({ at + (std::int64_t) std::llround (p.start), a, true, p.note, p.velocity, pair, p.slide });
            scheduleArp ({ at + juce::jmax ((std::int64_t) std::llround (p.start) + 1, (std::int64_t) std::llround (p.end)), a, false, p.note, 0.0f, pair, false });
        }
    }

    void Engine::arpNoteStarted (const Settings& settings) noexcept
    {
        const auto& modulation = settings.modulation;
        for (int k = 0; k < mod::numLfos; ++k)
            if (modulation.lfos[(size_t) k].trigger == mod::LfoTrigger::arpNotes)
                globalLfos[(size_t) k].start (modulation.lfos[(size_t) k], random);
        for (int k = 0; k < mod::numEnvelopes; ++k)
            if (modulation.envelopes[(size_t) k].trigger == mod::EnvelopeTrigger::arpNotes)
            {
                globalEnvelopes[(size_t) k].start();
                arpEnvelopeMask |= 1u << k;
            }
    }

    void Engine::arpNoteEnded() noexcept
    {
        // ARP NOTES envelopes let go when no arp note is held.
        if (std::any_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.playing && v.held && v.source > 0 && v.source <= arp::numArps && ! v.killing; }))
            return;
        for (int k = 0; k < mod::numEnvelopes; ++k)
            if ((arpEnvelopeMask >> k) & 1u)
                globalEnvelopes[(size_t) k].release();
        arpEnvelopeMask = 0;
    }

    void Engine::fireArpEvent (const ArpEvent& e, const Settings& settings) noexcept
    {
        auto& st = arps[(size_t) e.arp];
        auto& age = pairAges[(size_t) (e.pair % maxArpEvents)];
        const auto source = e.arp + 1;
        if (! e.on)
        {
            if (age == 0)
                return;
            for (auto& voice : voices)
                if (voice.playing && voice.age == age && voice.source == source && ! voice.killing)
                {
                    voice.held = false;
                    releaseVoice (voice);
                }
            arpNoteEnded();
            if (st.slideAge == age)
            {
                st.slideAge = 0;
                st.slidePair = 0;
            }
            age = 0;
            return;
        }

        // Gliding on from the last block: the same voice, its pitch moving (no new attack).
        if (st.slideAge != 0)
            for (auto& voice : voices)
                if (voice.playing && voice.age == st.slideAge && voice.source == source && ! voice.killing)
                {
                    retrigger (voice, e.note, e.velocity, false, settings.play.glideMs > 0.0f, settings);
                    reanswer (voice, false, settings.play.glideMs > 0.0f, settings);
                    pairAges[(size_t) (st.slidePair % maxArpEvents)] = 0;   // the last note's end is cancelled
                    age = voice.age;
                    st.slideAge = e.slide ? voice.age : 0;
                    st.slidePair = e.slide ? e.pair : 0;
                    return;
                }

        // A new note, on this arp's oscillators only (and not moving the keys' glide).
        const auto keysLastPitch = lastPitch;
        makeRoom (settings);
        auto* voice = findFreeVoice();
        arpNoteStarted (settings);
        startVoice (*voice, e.note, e.velocity, (float) e.note, settings, source, arpMasks[(size_t) e.arp]);
        applyOscillatorLimits (*voice, settings);
        lastPitch = keysLastPitch;
        answer (*voice, settings);
        age = voice->age;
        st.slideAge = e.slide ? voice->age : 0;
        st.slidePair = e.slide ? e.pair : 0;
    }

    //==========================================================================
    // SIROCCO (0.58.0): each note's answers are voices of their own, on the note's oscillators, started with it or after
    // DELAY, moved when it moves (a mono lead's glide, an arp's slide), let go when it is.
    void Engine::answer (const Voice& parent, const Settings& settings) noexcept
    {
        const auto& s = settings.sirocco;
        if (! s.answers() || parent.source == answerSource)
            return;
        const auto voicesWanted = juce::jlimit (1, sirocco::maxVoices, s.voices);
        const auto delay = (std::int64_t) std::llround (juce::jlimit (0.0f, 500.0f, s.delayMs) * 0.001 * sampleRate);
        const auto age = parent.age;
        for (int layer = 0; layer < voicesWanted; ++layer)
        {
            if (delay <= 0)
                startAnswer (age, layer, settings);
            else if (numPendingAnswers < (int) pendingAnswers.size())
                pendingAnswers[(size_t) numPendingAnswers++] = { age, layer, sampleClock + delay };
        }
    }

    void Engine::startAnswer (std::uint64_t parentAge, int layer, const Settings& settings) noexcept
    {
        const auto findParent = [this, parentAge]() -> Voice*
        {
            for (auto& v : voices)
                if (v.playing && v.age == parentAge && ! v.killing && (v.held || v.sustained) && v.source != answerSource)
                    return &v;
            return nullptr;
        };
        if (findParent() == nullptr)
            return;   // let go (or stolen) before its DELAY was up
        const auto& s = settings.sirocco;
        // Answers don't take a note's place (a mono lead's VOICES is 1): a free voice of the pool, or none.
        if (std::all_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.playing; }))
            return;
        auto* parent = findParent();
        if (parent == nullptr)
            return;
        std::array<int, 12> notes {};
        const auto inScale = sirocco::scaleNotes (s.scale, notes);
        const auto note = sirocco::harmonyNote (parent->note, sirocco::stepsFor (s.interval, layer, juce::jmax (1, inScale)), s.key, s.scale);
        const auto velocity = parent->velocity;
        const auto mask = parent->oscMask;
        const auto keysLastPitch = lastPitch;   // an answer doesn't move the keys' glide
        auto* voice = findFreeVoice();
        startVoice (*voice, note, velocity, (float) note, settings, answerSource, mask);
        lastPitch = keysLastPitch;
        voice->answers = parentAge;
        voice->layer = layer;
        placeAnswer (*voice, settings);
        applyOscillatorLimits (*voice, settings);
    }

    void Engine::reanswer (const Voice& parent, bool restartEnvelopes, bool glide, const Settings& settings) noexcept
    {
        const auto& s = settings.sirocco;
        // A note with no answers yet (a mono lead's legato phrase begun with LEVEL at 0, or SIROCCO just switched on):
        // they start now, as for a new note.
        const auto hasAnswers = std::any_of (voices.begin(), voices.end(), [&parent] (const Voice& v)
                                             { return v.playing && ! v.killing && v.source == answerSource && v.answers == parent.age; })
                             || std::any_of (pendingAnswers.begin(), pendingAnswers.begin() + numPendingAnswers,
                                             [&parent] (const PendingAnswer& p) { return p.parent == parent.age; });
        if (! hasAnswers)
        {
            answer (parent, settings);
            return;
        }
        std::array<int, 12> notes {};
        const auto inScale = juce::jmax (1, sirocco::scaleNotes (s.scale, notes));
        for (auto& v : voices)
            if (v.playing && ! v.killing && v.source == answerSource && v.answers == parent.age)
            {
                const auto note = sirocco::harmonyNote (parent.note, sirocco::stepsFor (s.interval, v.layer, inScale), s.key, s.scale);
                const auto keysLastPitch = lastPitch;
                retrigger (v, note, parent.velocity, restartEnvelopes, glide, settings);
                lastPitch = keysLastPitch;
                placeAnswer (v, settings);
            }
    }

    void Engine::driftOn (Voice& voice, int numSamples) noexcept
    {
        // AFTERGLOW's wander: each note heads for a new random place every 0.4 to 1.6 s, gliding there with a 0.35 s
        // time constant, so its tuning and filter sway slowly and differently from every other note's.
        const auto seconds = (float) numSamples / (float) sampleRate;
        voice.driftLeft -= seconds;
        if (voice.driftLeft <= 0.0f)
        {
            voice.driftTuneTo = nextGlow();
            voice.driftCutoffTo = nextGlow();
            voice.driftLeft = 1.0f + 0.6f * nextGlow();
        }
        const auto k = 1.0f - std::exp (-seconds / 0.35f);
        voice.driftTune += (voice.driftTuneTo - voice.driftTune) * k;
        voice.driftCutoff += (voice.driftCutoffTo - voice.driftCutoff) * k;
    }

    void Engine::placeAnswer (Voice& voice, const Settings& settings) noexcept
    {
        // DETUNE: the first a little sharp, the second flat, the third half as sharp; SPREAD: left, right, middle.
        constexpr float detunes[] = { 1.0f, -1.0f, 0.5f };
        constexpr float pans[] = { -1.0f, 1.0f, 0.0f };
        const auto layer = (size_t) juce::jlimit (0, sirocco::maxVoices - 1, voice.layer);
        const auto& s = settings.sirocco;
        const auto gliding = voice.glideRate > 0.0f;
        voice.targetPitch = (float) voice.note + s.detuneCents * 0.01f * detunes[layer];
        if (! gliding)
            voice.pitch = voice.targetPitch;
        voice.notePan = juce::jlimit (-1.0f, 1.0f, voice.notePan + juce::jlimit (0.0f, 1.0f, s.spread) * pans[layer]);
        balance (voice.notePan, voice.panLeft, voice.panRight);
    }

    void Engine::releaseAnswers (const Voice& parent) noexcept
    {
        if (parent.age == 0)
            return;
        for (auto& v : voices)
            if (v.playing && v.source == answerSource && v.answers == parent.age && v.held && ! v.killing)
            {
                v.held = v.sustained = false;
                releaseVoice (v);
            }
    }

    void Engine::runAnswers (const Settings& settings) noexcept
    {
        for (int i = 0; i < numPendingAnswers;)
        {
            const auto p = pendingAnswers[(size_t) i];
            if (p.time > sampleClock)
            {
                ++i;
                continue;
            }
            pendingAnswers[(size_t) i] = pendingAnswers[(size_t) --numPendingAnswers];
            startAnswer (p.parent, p.layer, settings);
        }
        // Notes held without answers (begun with LEVEL at 0, MACRO 3 turned up while they're held): answered now.
        if (! settings.sirocco.answers())
            return;
        for (auto& v : voices)
            if (v.playing && ! v.killing && (v.held || v.sustained) && v.source != answerSource)
            {
                const auto answered = std::any_of (voices.begin(), voices.end(), [&v] (const Voice& a)
                                                   { return a.playing && a.source == answerSource && a.answers == v.age; })
                                   || std::any_of (pendingAnswers.begin(), pendingAnswers.begin() + numPendingAnswers,
                                                   [&v] (const PendingAnswer& p) { return p.parent == v.age; });
                if (! answered)
                    answer (v, settings);
            }
    }

    float Engine::seqDepth (double age, const StepSeqSettings& s) const noexcept
    {
        const auto delay = s.delayMs * 0.001;
        if (age < delay)
            return 0.0f;
        return s.fadeInMs > 0.0f ? (float) juce::jmin (1.0, (age - delay) / (s.fadeInMs * 0.001)) : 1.0f;
    }

    void mod::apply (Settings& s, Target target, float value) noexcept
    {
        const auto i = (size_t) juce::jlimit (0, numOscillators - 1, target.index);
        auto& o = s.oscillators[i];
        auto& shaper = s.shapers[(size_t) juce::jlimit (0, numShapers - 1, target.index)];
        switch (target.field)
        {
            case Field::oscVolume: o.volume = value; break;
            case Field::oscPan: o.pan = value; break;
            case Field::oscTranspose: o.transpose = value; break;
            case Field::oscFine: o.fineCents = value; break;
            case Field::oscGain: o.gain = value; break;
            case Field::oscWidth: o.pulseWidth = value; break;
            case Field::oscNoise: o.noise = value; break;
            case Field::oscNoiseColour: o.noiseColour = value; break;
            case Field::oscSub: o.sub = value; break;
            case Field::oscTableIndex: o.tableIndex = value; break;
            case Field::oscFormant: o.formant = value; break;
            case Field::oscCrush: o.crush = value; break;
            case Field::oscSyncAmount: o.syncAmount = value; break;
            case Field::oscSyncEnv: o.syncEnvAmount = value; break;
            case Field::oscFmAmount: o.fmAmount = value; break;
            case Field::oscAmAmount: o.amAmount = value; break;
            case Field::oscModRate: o.modRate = value; break;
            case Field::oscUnisonMix: o.unisonMix = value; break;
            case Field::oscUnisonDetune: o.unisonDetune = value; break;
            case Field::oscUnisonSpread: o.unisonSpread = value; break;
            case Field::oscVibratoRate: o.vibratoRateHz = value; break;
            case Field::oscVibratoAmount: o.vibratoCents = value; break;
            case Field::oscFftLp: o.fftLp = value; break;
            case Field::oscFftHp: o.fftHp = value; break;
            case Field::oscFftSlope: o.fftSlope = value; break;
            case Field::oscFftVSpeed: o.fftVSpeedHz = value; break;
            case Field::filterCutoff: s.filter.cutoffHz = value; break;
            case Field::filterResonance: s.filter.resonance = value; break;
            case Field::filterDrive: s.filter.drive = value; break;
            case Field::filterKeytrack: s.filter.keytrack = value; break;
            case Field::filterEnvelope: s.filter.envelopeAmount = value; break;
            case Field::shaperDrive: shaper.drive = value; break;
            case Field::shaperMix: shaper.mix = value; break;
            case Field::shaperOutput: shaper.outputDb = value; break;
            case Field::ampVolume: s.amp.volume = value; break;
            case Field::ampPan: s.amp.pan = value; break;
            case Field::oscSkew: o.skew = value; break;
            case Field::oscPinch: o.pinch = value; break;
            case Field::oscTravelStrength: o.travel.strength = value; break;
            case Field::oscTravelPhase: o.travel.phase = value; break;
            case Field::shaperEq1Gain: shaper.eqGainDb[0] = value; break;
            case Field::shaperEq1Freq: shaper.eqFreqHz[0] = value; break;
            case Field::shaperEq1Q: shaper.eqQ[0] = value; break;
            case Field::shaperEq2Gain: shaper.eqGainDb[1] = value; break;
            case Field::shaperEq2Freq: shaper.eqFreqHz[1] = value; break;
            case Field::shaperEq2Q: shaper.eqQ[1] = value; break;
            case Field::pitchEnvRange: s.pitch.rangeSemitones = value; break;
            case Field::pitchKeytrack: s.pitch.keytrack = value; break;
            case Field::stepSeqVolMix: s.stepSeq.volMix = value; break;
            case Field::voiceVowel: s.voice.vowel = value; break;
            case Field::voiceSize: s.voice.size = value; break;
            case Field::voiceSharp: s.voice.sharp = value; break;
            case Field::voiceMix: s.voice.mix = value; break;
        }
    }

    void Engine::assignPaths (Voice& voice, const Settings& settings) noexcept
    {
        // An oscillator's route: through the FILTER (if it's on and the
        // oscillator goes to it) and through each SHAPER that's on and the
        // oscillator is routed to.
        std::array<int, numOscillators> keys {};
        for (size_t i = 0; i < keys.size(); ++i)
        {
            const auto& s = settings.oscillators[i];
            keys[i] = -1;
            if (! s.on || ! voice.oscillators[i].active)
                continue;
            auto key = s.toFilter && settings.filter.on ? 1 : 0;
            if (s.toVoice && settings.voice.on)
                key |= voiceBit;
            for (int k = 0; k < numShapers; ++k)
                if (settings.shapers[(size_t) k].on && s.toShaper[(size_t) k])
                    key |= 2 << k;
            // Stage 7b: an oscillator sending a different amount needs its own path (sends are taken after the
            // path). 100%, or a send that's off, is code 0: so switching an effect on re-routes nothing.
            // Stage 7d: on an insert bus (one that's wanted), its own path into the bus; the bus does the sending.
            if (const auto bus = s.insertBus; bus > 0 && bus <= numInsertBuses && insertBaseLeft[(size_t) bus - 1] != nullptr)
                key |= bus << busShift;
            else
                for (int send = 0; send < numSends; ++send)
                    if (sendsWanted (settings, send))
                        if (const auto percent = juce::jlimit (0, 100, juce::roundToInt (s.send[(size_t) send] * 100.0f)); percent != 100)
                            key |= (percent + 1) << (5 + 7 * send);
            keys[i] = key;
        }

        // Routes this note already has keep their paths (and their filters'
        // and shapers' state); a new route takes a path no route wants.
        pathUsed.fill (false);
        oscillatorPath.fill (-1);
        const auto find = [&voice] (int key)
        {
            for (size_t p = 0; p < voice.paths.size(); ++p)
                if (voice.paths[p].key == key)
                    return (int) p;
            return -1;
        };
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] >= 0)
                if (const auto p = find (keys[i]); p >= 0)
                {
                    oscillatorPath[i] = p;
                    pathUsed[(size_t) p] = true;
                }
        for (size_t i = 0; i < keys.size(); ++i)
        {
            if (keys[i] < 0 || oscillatorPath[i] >= 0)
                continue;
            auto p = find (keys[i]);
            if (p < 0 || ! pathUsed[(size_t) p])
            {
                // A route new to this note: an unused path, started afresh.
                for (size_t q = 0; q < voice.paths.size(); ++q)
                    if (! pathUsed[q])
                    {
                        p = (int) q;
                        break;
                    }
                voice.paths[(size_t) p].key = keys[i];
                voice.paths[(size_t) p].reset();
            }
            oscillatorPath[i] = p;
            pathUsed[(size_t) p] = true;
        }
    }

    void Engine::processPath (Voice& voice, Path& path, float* left, float* right, int n, const Settings& settings,
                              const FilterChannel::Coefficients& filterTarget, const vocal::Coefficients& vocalTarget,
                              float vocalLevel) noexcept
    {
        const auto velocityDown = 1.0f - voice.velocity;
        // SHAPERs placed before the FILTER, in order...
        for (int k = 0; k < numShapers; ++k)
            if ((path.key & (2 << k)) != 0 && ! settings.shapers[(size_t) k].afterFilter)
                processShaper (path, k, left, right, n, settings, velocityDown);

        // ...the FILTER...
        if ((path.key & 1) != 0)
        {
            const auto& filterSettings = settings.filter;
            const auto from = path.coefficientsFresh ? filterTarget : path.coefficients;
            path.coefficientsFresh = false;

            // DRIVE: up to +24 dB into the saturation, scaled back so a full-scale
            // wave still peaks near full scale (quieter sounds come out louder,
            // as from any overdriven input).
            const auto drive = filterSettings.drive;
            const auto driveGain = 1.0f + drive * 15.0f;
            const auto makeUp = 1.0f / std::tanh (driveGain);
            const auto type = filterSettings.type;
            for (int i = 0; i < n; ++i)
            {
                const auto c = FilterChannel::Coefficients::lerp (from, filterTarget, (float) (i + 1) / (float) n);
                auto l = left[i], r = right[i];
                if (drive > 0.0f)
                {
                    l = path.driveLeft.process (l, driveGain, makeUp);
                    r = path.driveRight.process (r, driveGain, makeUp);
                }
                left[i] = path.filterLeft.process (l, type, c);
                right[i] = path.filterRight.process (r, type, c);
            }
            path.coefficients = filterTarget;
        }

        // ...the VOICE filter (0.39.0): the vowel's formants at fixed frequencies, MIX against what went in...
        if ((path.key & voiceBit) != 0)
        {
            const auto from = path.vocalFresh ? vocalTarget : path.vocalCoefficients;
            path.vocalFresh = false;
            const auto fromLevel = path.vocalLevel > 0.0f ? path.vocalLevel : vocalLevel;
            const auto wet = settings.voice.mix, dry = 1.0f - wet;
            for (int i = 0; i < n; ++i)
            {
                const auto t = (float) (i + 1) / (float) n;
                const auto c = vocal::Coefficients::lerp (from, vocalTarget, t);
                const auto w = wet * (fromLevel + (vocalLevel - fromLevel) * t);
                left[i] = path.vocalLeft.process (left[i], c) * w + left[i] * dry;
                right[i] = path.vocalRight.process (right[i], c) * w + right[i] * dry;
            }
            path.vocalCoefficients = vocalTarget;
            path.vocalLevel = vocalLevel;
        }

        // ...and the ones after it.
        for (int k = 0; k < numShapers; ++k)
            if ((path.key & (2 << k)) != 0 && settings.shapers[(size_t) k].afterFilter)
                processShaper (path, k, left, right, n, settings, velocityDown);
    }

    void Engine::processShaper (Path& path, int k, float* left, float* right, int n, const Settings& settings, float velocityDown) noexcept
    {
        const auto& sh = settings.shapers[(size_t) k];
        // The EQ bands: the shaper's (worked out once a block), or this note's own when the matrix moves them.
        std::array<shaper::Bell, 2> ownBells;
        auto movedEq = false;
        if (baseSettings != nullptr)
        {
            const auto& base = baseSettings->shapers[(size_t) k];
            movedEq = base.eqGainDb != sh.eqGainDb || base.eqFreqHz != sh.eqFreqHz || base.eqQ != sh.eqQ;
        }
        if (movedEq)
            for (size_t b = 0; b < 2; ++b)
                ownBells[b] = shaper::Bell::make (sh.eqGainDb[b], sh.eqFreqHz[b], sh.eqQ[b], sampleRate);
        const auto& bells = movedEq ? ownBells : shaperCoefficients[(size_t) k].bells;
        auto& chLeft = path.shapeLeft[(size_t) k];
        auto& chRight = path.shapeRight[(size_t) k];

        // DRIVE for this note (softer notes driven less, by DRIVE VELOCITY), glided to from the last update.
        const auto gain = shaper::driveGain (sh.drive * (1.0f - sh.driveVelocity * velocityDown));
        const auto makeUp = shaper::makeUp (sh.model, gain);
        auto& lastGain = path.shapeGain[(size_t) k];
        auto& lastMakeUp = path.shapeMakeUp[(size_t) k];
        const auto fromGain = lastGain > 0.0f ? lastGain : gain;
        const auto fromMakeUp = lastMakeUp > 0.0f ? lastMakeUp : makeUp;
        lastGain = gain;
        lastMakeUp = makeUp;

        const auto output = std::pow (10.0f, sh.outputDb / 20.0f);
        const auto wet = sh.mix * output, dry = (1.0f - sh.mix) * output;
        const auto asymmetric = shaper::isAsymmetric (sh.model);
        const auto model = sh.model;
        for (int i = 0; i < n; ++i)
        {
            const auto t = (float) (i + 1) / (float) n;
            const auto g = fromGain + (gain - fromGain) * t;
            const auto m = fromMakeUp + (makeUp - fromMakeUp) * t;
            const auto inLeft = left[i], inRight = right[i];

            // The two EQ bands colour what's distorted (with EQ SPLIT, band 1 the left and band 2 the right).
            auto l = inLeft, r = inRight;
            if (sh.eqSplit)
            {
                l = chLeft.eq (l, bells[0], 0);
                r = chRight.eq (r, bells[1], 1);
            }
            else
            {
                l = chLeft.eq (chLeft.eq (l, bells[0], 0), bells[1], 1);
                r = chRight.eq (chRight.eq (r, bells[0], 0), bells[1], 1);
            }

            l = chLeft.distort (l, model, g) * m;
            r = chRight.distort (r, model, g) * m;
            if (asymmetric)
            {
                l = chLeft.blockDc (l, dcPole);
                r = chRight.blockDc (r, dcPole);
            }
            // MIX: the dry sound delayed as the distorted one is (by the oversampling), so they add up in step.
            left[i] = l * wet + chLeft.delayedDry (inLeft) * dry;
            right[i] = r * wet + chRight.delayedDry (inRight) * dry;
        }
    }

    float Engine::unisonPosition (int copy, int voices) noexcept
    {
        // The note's own pitch in the middle; the others alternately above and
        // below, further out in turn: for 7, +1/3 -1/3 +2/3 -2/3 +1 -1.
        if (copy <= 0 || voices < 2)
            return 0.0f;
        const auto extras = voices - 1;
        const auto half = (extras + 1) / 2;
        const auto step = (copy + 1) / 2;
        return (copy % 2 == 1 ? 1.0f : -1.0f) * (float) step / (float) half;
    }

    int Engine::getNumActiveOscillators (const Settings& settings) const noexcept
    {
        int count = 0;
        for (const auto& voice : voices)
            if (voice.playing)
                for (size_t i = 0; i < voice.oscillators.size(); ++i)
                    if (voice.oscillators[i].active)
                    {
                        const auto& s = settings.oscillators[i];
                        const auto notes = 1 + (int) std::count_if (s.chord.begin(), s.chord.end(), [] (const ChordVoice& c) { return c.on; });
                        count += notes * (s.hasUnison() ? s.unisonVoices : 1);
                    }
        return count;
    }

    float Engine::vibratoValue (VibratoWave wave, float& phase, float& held, float advance) noexcept
    {
        float value = 0.0f;
        switch (wave)
        {
            case VibratoWave::sine:     value = std::sin (juce::MathConstants<float>::twoPi * phase); break;
            case VibratoWave::triangle: value = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case VibratoWave::square:   value = phase < 0.5f ? 1.0f : -1.0f; break;
            case VibratoWave::saw:      value = 2.0f * phase - 1.0f; break;
            case VibratoWave::random:   value = held; break;
        }
        phase += advance;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = random.nextFloat() * 2.0f - 1.0f;   // RANDOM: a new value each cycle
        }
        return value;
    }

    namespace
    {
        // Adds one copy of a wave to `left` and `right`, each sample scaled by
        // `gains` (FADE IN, VOLUME, GAIN) and the copy's own pan.
        //
        // In two passes, so the compiler can work on several samples at once:
        // first every sample's place in the table (worked out from the start,
        // not each from the last, so no sample waits for the one before), then
        // the reading and mixing.
        void addCopy (const WaveSource& source, float* left, float* right, const float* gains, int n, float& phase, float increment,
                      int level, float gainLeft, float gainRight) noexcept
        {
            constexpr int maxN = Engine::controlInterval;
            float positions[maxN], others[maxN];
            const auto isTable = source.wave == OscWave::wavetable;
            const auto size = isTable ? 1.0f : (float) tables::size;   // a wavetable's levels scale the phase themselves
            for (int i = 0; i < n; ++i)
            {
                auto p = phase + increment * (float) i;
                p -= (float) (int) p;   // 0 to 1 (p is never negative)
                positions[i] = p * size;
                auto q = p + source.pulseWidth;
                q -= (float) (int) q;
                others[i] = q * size;
            }
            phase += increment * (float) n;
            phase -= (float) (int) phase;

            if (isTable)
            {
                readTable (source, level, positions, n, [&] (int i, float value)
                {
                    const auto v = value * gains[i];
                    left[i] += v * gainLeft;
                    right[i] += v * gainRight;
                });
                return;
            }

            const auto read = [] (const float* table, float position)
            {
                const auto index = (int) position;
                const auto fraction = position - (float) index;
                const auto* at = table + (index & (tables::size - 1));
                return at[0] + (at[1] - at[0]) * fraction;
            };

            if (source.wave == OscWave::pulse)
            {
                const auto* saw = tables::get (tables::Wave::saw, level);
                for (int i = 0; i < n; ++i)
                {
                    const auto v = (read (saw, positions[i]) - read (saw, others[i])) * gains[i];
                    left[i] += v * gainLeft;
                    right[i] += v * gainRight;
                }
                return;
            }
            const auto* table = tables::get (source.wave == OscWave::saw ? tables::Wave::saw
                                           : source.wave == OscWave::triangle ? tables::Wave::triangle : tables::Wave::sine, level);
            for (int i = 0; i < n; ++i)
            {
                const auto v = read (table, positions[i]) * gains[i];
                left[i] += v * gainLeft;
                right[i] += v * gainRight;
            }
        }
    }

    void Engine::renderOscillators (Voice& voice, int n, const Settings& settings, float bendSemitones) noexcept
    {
        std::array<float, controlInterval> gains {};

        // FM from other oscillators: an oscillator chosen as another's
        // modulator is rendered first, into fmSignals, and isn't heard itself.
        std::array<bool, numOscillators> isModulator {};
        for (size_t i = 0; i < settings.oscillators.size(); ++i)
        {
            const auto& s = settings.oscillators[i];
            if (s.on && s.hasFm() && s.fmSource >= fmFirstOscillator && (size_t) (s.fmSource - fmFirstOscillator) != i)
                isModulator[(size_t) (s.fmSource - fmFirstOscillator)] = true;
        }
        std::array<size_t, numOscillators> order {};
        size_t count = 0;
        for (const auto modulators : { true, false })
            for (size_t i = 0; i < order.size(); ++i)
                if (isModulator[i] == modulators)
                    order[count++] = i;
        for (size_t i = 0; i < order.size(); ++i)
            if (isModulator[i])
                std::fill (fmSignals[i].begin(), fmSignals[i].end(), 0.0f);

        for (const auto index : order)
        {
            auto& osc = voice.oscillators[index];
            const auto& s = settings.oscillators[index];
            if (! osc.active)
                continue;
            if (! s.on)
            {
                // Switched off while the note plays: gone from this note.
                osc.active = false;
                continue;
            }

            // VIBRATO: its depth now (fading in after the note starts), and the
            // global wobble for this interval.
            auto vibratoDepth = 0.0f;
            if (s.vibratoCents > 0.0f)
            {
                const auto fadeSamples = s.vibratoFadeMs * 0.001f * (float) sampleRate;
                const auto progress = fadeSamples < 1.0f ? 1.0f : juce::jmin (1.0f, (float) osc.vibratoAge / fadeSamples);
                vibratoDepth = s.vibratoCents * 0.01f * synth::bend (progress, s.vibratoCurve);
                osc.vibratoAge += n;
            }
            const auto vibratoAdvance = s.vibratoRateHz * (float) n / (float) sampleRate;
            const auto globalVibrato = vibratoDepth > 0.0f && s.vibratoMode == VibratoMode::global
                                     ? vibratoValue (s.vibratoWave, osc.vibratoPhase, osc.vibratoHeld, vibratoAdvance) * vibratoDepth : 0.0f;

            // Its pitch for this interval, and the tables to read for it.
            // KEYTRACK (from middle C; at 100% the note as played) and the PITCH envelope, if it follows it.
            const auto keyPitch = juce::exactlyEqual (settings.pitch.keytrack, 1.0f) ? voice.pitch : 60.0f + (voice.pitch - 60.0f) * settings.pitch.keytrack;
            const auto basePitch = keyPitch + (s.pitchEnvelope ? voice.pitchSemitones : 0.0f) + bendSemitones + s.transpose + s.fineCents * 0.01f + osc.randomOffset
                                 + settings.play.tuneCents * 0.01f + voice.naviSemis + voice.crossSemis;   // 0.86.0: NAVICOMPUTER's move (0 while off)
            const auto independent = vibratoDepth > 0.0f && s.vibratoMode == VibratoMode::independent;
            const auto ownVibrato = independent ? vibratoValue (s.vibratoWave, osc.copies[0].vibratoPhase, osc.copies[0].vibratoHeld,
                                                                vibratoAdvance * osc.copies[0].vibratoRate) * vibratoDepth
                                                : globalVibrato;
            const auto hz = pitchToHz (basePitch + ownVibrato);
            const auto increment = juce::jlimit (0.0f, 0.5f, hz / (float) sampleRate);
            const auto level = tables::levelFor (hz, sampleRate);
            WaveSource source;
            auto travelTarget = 1.0f;   // TRAVEL's SILENCE steps: 0
            source.wave = s.wave;
            source.pulseWidth = juce::jlimit (0.01f, 0.99f, s.pulseWidth);
            auto audible = s.gain > 0.0f;

            // What it reads: FFT's edited copy if there is one (for a
            // wavetable, with the same frames), else its wavetable, its
            // FREEFORM drawing (a one-frame table) or its wave.
            const auto drawn = s.wave == OscWave::wavetable || s.wave == OscWave::freeform;
            const auto* table = s.spectral != nullptr ? s.spectral : drawn ? s.table : nullptr;
            if (drawn || s.spectral != nullptr)
            {
                source.wave = OscWave::wavetable;
                audible = audible && table != nullptr;
            }
            if (table != nullptr)
            {
                source.table = table;
                if (s.wave == OscWave::wavetable)
                {
                    // TRAVEL: the oscillator's own movement through the table, from INDEX.
                    auto wtIndex = s.tableIndex;
                    if (s.travel.on)
                    {
                        const auto& t = s.travel;
                        const auto seconds = t.trigger == travel::Trigger::voice ? osc.travelTime : travelClock[index] + intervalSeconds;
                        const auto point = travel::at (t, seconds * t.periodsPerSecond (settings.modulation.bpm));
                        const auto target = travel::indexFor (point, s.tableIndex, travel::strengthFor (t, voice.velocity));
                        const auto tau = travel::smoothSeconds (t.smooth);
                        if (osc.travelIndex < 0.0f || tau <= 0.0)
                            osc.travelIndex = target;
                        else
                            osc.travelIndex += (target - osc.travelIndex) * (float) (1.0 - std::exp (-(double) n / (tau * sampleRate)));
                        wtIndex = osc.travelIndex;
                        travelTarget = point.silent ? 0.0f : 1.0f;
                        osc.travelTime += (double) n / sampleRate;
                    }

                    // INDEX: from where it was at the end of the last interval to where
                    // it is now, between the same two frames (if it crossed a frame,
                    // from the new pair's start: a step too small to hear).
                    const auto from = osc.tableIndex < 0.0f ? wtIndex : osc.tableIndex;
                    osc.tableIndex = wtIndex;
                    const auto now = table->locate (wtIndex, s.tableFrames, s.tableSteppy);
                    const auto before = table->locate (from, s.tableFrames, s.tableSteppy);
                    source.below = now.below;
                    source.above = now.above;
                    source.mixTo = now.between;
                    source.mixFrom = before.below == now.below && before.above == now.above ? before.between : now.between;
                    source.oneFrame = now.below == now.above;
                }
                // FFT VSPEED: swinging between the two edited copies.
                if (s.spectralPulse != nullptr && s.spectralPulse->getNumFrames() == table->getNumFrames())
                {
                    const auto swing = [] (float p) { return 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * p); };
                    source.partner = s.spectralPulse;
                    source.partnerFrom = swing (osc.pulsePhase);
                    osc.pulsePhase = wrap (osc.pulsePhase + s.fftVSpeedHz * (float) n / (float) sampleRate);
                    source.partnerTo = swing (osc.pulsePhase);
                }
            }
            // FFT LP and HP: their cutoffs among the levels (3 an octave), live.
            if (s.hasFftFilter())
            {
                const auto position = [] (float harmonics) { return 3.0f * std::log2 ((float) tables::maxHarmonics / juce::jlimit (1.0f, 512.0f, harmonics)); };
                source.lpPosition = s.fftLp < 511.5f ? position (s.fftLp) : -1.0f;
                source.hpPosition = s.fftHp > 1.01f ? position (s.fftHp) : -1.0f;
                source.slopeWidth = 1.0f + 5.0f * juce::jlimit (0.0f, 1.0f, s.fftSlope);
            }

            // SHAPE: bent waves read a level with fewer harmonics, as many as the
            // fastest the bend plays any part of the wave leaves room for.
            // SKEW and PINCH: the oscillator's curve, or this note's own when the matrix moves them.
            const auto* base = baseSettings != nullptr ? &baseSettings->oscillators[index] : nullptr;
            const auto ownWarp = base != nullptr && (! juce::exactlyEqual (s.skew, base->skew) || ! juce::exactlyEqual (s.pinch, base->pinch));
            if (ownWarp)
                osc.warp.updateFromGrid (s.skew, s.pinch);
            else
                warpTables[index].update (s.skew, s.pinch);
            const auto bending = WaveBend::make (s, ownWarp ? osc.warp : warpTables[index]);

            // SYNC, FM and AM: the same, with the copy's or modulator's rate.
            Modulation mod;
            if (s.hasModulation())
            {
                mod.active = true;
                mod.ratio = modulation::ratio (s.modRate, s.modGrid);
                if (s.hasSync())
                {
                    // SYNC's envelope: from the note's start, falling over LENGTH.
                    const auto msPerSample = 1000.0f / (float) sampleRate;
                    const auto from = modulation::syncEnvelope ((float) osc.syncAge * msPerSample, s.syncEnvMs, s.syncEnvCurve);
                    const auto to = modulation::syncEnvelope ((float) (osc.syncAge + n) * msPerSample, s.syncEnvMs, s.syncEnvCurve);
                    osc.syncAge += n;
                    mod.sync = true;
                    mod.syncAdd = s.syncMode == SyncMode::add || s.syncMode == SyncMode::addWindowed;
                    mod.syncWindowed = s.syncMode == SyncMode::pureWindowed || s.syncMode == SyncMode::addWindowed;
                    mod.syncFrom = modulation::syncRatio (s.syncAmount + s.syncEnvAmount * from);
                    mod.syncTo = modulation::syncRatio (s.syncAmount + s.syncEnvAmount * to);
                    mod.speed *= juce::jmax (mod.syncFrom, mod.syncTo);
                }
                if (s.hasFm() && s.fmSource != fmFirstOscillator + (int) index)   // an oscillator can't modulate itself
                {
                    mod.fmDepth = modulation::fmDepth (s.fmAmount);
                    if (s.fmSource >= fmFirstOscillator)
                    {
                        // Another oscillator: rendered first (see the loop's order),
                        // its speed estimated from its pitch against this one's.
                        const auto modulator = (size_t) (s.fmSource - fmFirstOscillator);
                        mod.fmSignal = fmSignals[modulator].data();
                        const auto& other = settings.oscillators[modulator];
                        const auto pitchRatio = std::exp2 ((other.transpose - s.transpose + (other.fineCents - s.fineCents) * 0.01f) / 12.0f);
                        mod.speed *= modulation::fmSpeed (mod.fmDepth, pitchRatio, ModShape::saw);
                    }
                    else
                    {
                        mod.fmShape = (ModShape) s.fmSource;
                        mod.speed *= modulation::fmSpeed (mod.fmDepth, mod.ratio, mod.fmShape);
                    }
                }
                if (s.hasAm())
                {
                    mod.amDepth = juce::jlimit (0.0f, 1.0f, s.amAmount);
                    mod.amShape = s.amShape;
                    mod.speed *= 1.0f + mod.ratio;   // each harmonic gains side tones the modulator's rate away
                }
                if ((mod.sync || mod.fmDepth > 0.0f) && ! (bending.hasFormant || bending.warp != nullptr))
                    mod.speed *= WaveBend::headroom;   // room for side tones, as SHAPE's
            }
            const auto modLevel = tables::levelFor (hz * mod.ratio * 2.0f, sampleRate);

            auto* mono = scratch.data();
            // SFZ: the samples, in stereo (into scratch and sfzRight); nothing of the wave's own shaping.
            const auto sampled = s.wave == OscWave::sfz;
            if (sampled)
            {
                audible = false;
                renderSfz (voice, osc, s, basePitch + ownVibrato, mono, sfzRight.data(), n);
            }
            const auto general = bending.active || mod.active || source.needsGeneralPath();
            if (sampled)
                ;
            else if (audible && general)
                renderBent (source, bending.forIncrement (increment), mod, tables::levelFor (hz * bending.speed * mod.speed, sampleRate), modLevel,
                            n, osc.phase, increment, osc.crushPrevious, osc.modPhase, osc.lastX,
                            [mono, gain = s.gain] (int i, float v) { mono[i] = v * gain; });
            else if (audible)
                renderWave (source, mono, n, osc.phase, increment, level, s.gain);
            else
            {
                std::fill (mono, mono + n, 0.0f);
                osc.phase = wrap (osc.phase + increment * (float) n);   // keeps running, silent
            }

            if (s.sub > 0.0f && ! sampled)
            {
                const auto octaves = (float) (s.subOctave - 2);   // -2, -1 or 0 octaves
                const auto subHz = hz * std::exp2 (octaves);
                addSub (s.subWave, mono, n, osc.subPhase, juce::jlimit (0.0f, 0.5f, subHz / (float) sampleRate),
                        tables::levelFor (subHz, sampleRate), s.sub * (s.subInvert ? -1.0f : 1.0f));
            }
            const auto sign = s.invert ? -1.0f : 1.0f;
            if (s.invert)
                for (int i = 0; i < n; ++i)
                {
                    mono[i] = -mono[i];
                    sfzRight[(size_t) i] = -sfzRight[(size_t) i];
                }

            // FADE IN (or fading out after a newer note took this oscillator) and
            // VOLUME, once for every copy.
            // TRAVEL's SILENCE: a fade to it (and back) over about a millisecond.
            const auto travelFrom = osc.travelGain;
            osc.travelGain = juce::jlimit (travelFrom - 0.33f, travelFrom + 0.33f, travelTarget);
            const auto travelStep = (osc.travelGain - travelFrom) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                gains[(size_t) i] = osc.fade * s.volume * (travelFrom + travelStep * (float) (i + 1));
                if (! juce::exactlyEqual (osc.fadeStep, 0.0f))
                {
                    osc.fade = juce::jlimit (0.0f, 1.0f, osc.fade + osc.fadeStep);
                    if (osc.fade >= 1.0f && osc.fadeStep > 0.0f)
                        osc.fadeStep = 0.0f;
                }
            }

            if (isModulator[index])
            {
                // FM's modulator for another oscillator (with its VOLUME and FADE IN): kept for it, not heard.
                for (int i = 0; i < n; ++i)
                    fmSignals[index][(size_t) i] = (sampled ? 0.5f * (mono[i] + sfzRight[(size_t) i]) : mono[i]) * gains[(size_t) i];
            }
            else
            {
                float panLeft, panRight;
                balance (s.pan, panLeft, panRight);
                const auto path = (size_t) juce::jmax (0, oscillatorPath[index]);
                auto* outLeft = pathLeft[path].data();
                auto* outRight = pathRight[path].data();

                const auto noise = s.noise;
                const auto colour = s.noiseColour;
                for (int i = 0; i < n; ++i)
                {
                    auto l = mono[i], r = sampled ? sfzRight[(size_t) i] : mono[i];
                    if (noise > 0.0f)
                    {
                        const auto noiseLeft = osc.noiseLeft.next (colour);
                        auto noiseRight = noiseLeft;
                        if (s.noiseWidth != NoiseWidth::mono)
                        {
                            const auto other = osc.noiseRight.next (colour);
                            noiseRight = s.noiseWidth == NoiseWidth::stereo ? other : 0.5f * (noiseLeft + other);
                        }
                        l += noiseLeft * noise;
                        r += noiseRight * noise;
                    }
                    outLeft[i] += l * gains[(size_t) i] * panLeft;
                    outRight[i] += r * gains[(size_t) i] * panRight;
                }

                // The other copies: the key's unison copies, then each chord note and
                // its unison copies. The wave only (the sub and noise stay with the key),
                // each with its own pitch, pan and level. None at all unless asked for.
                const auto unison = s.hasUnison() ? s.unisonVoices : 1;
                const auto detuneCents = s.unisonDetune * s.unisonDetune * maxDetuneCents;   // squared: fine control at the low end
                if (audible && (unison > 1 || std::any_of (s.chord.begin(), s.chord.end(), [] (const ChordVoice& c) { return c.on; })))
                {
                    for (int note = 0; note <= numChordVoices; ++note)
                    {
                        const auto* chord = note > 0 ? &s.chord[(size_t) note - 1] : nullptr;
                        if (chord != nullptr && ! chord->on)
                            continue;
                        const auto notePitch = chord != nullptr ? chord->coarse + chord->fineCents * 0.01f : 0.0f;
                        const auto noteVolume = chord != nullptr ? chord->volume : 1.0f;
                        const auto notePan = chord != nullptr ? chord->pan : 0.0f;
                        for (int u = note == 0 ? 1 : 0; u < unison; ++u)
                        {
                            auto& c = osc.copies[(size_t) (note * maxUnison + u)];
                            const auto position = unisonPosition (u, unison);
                            const auto octave = u > 0 && s.unisonOctaves > 0 ? (float) ((u - 1) % (s.unisonOctaves + 1)) * 12.0f : 0.0f;
                            const auto copyVibrato = independent ? vibratoValue (s.vibratoWave, c.vibratoPhase, c.vibratoHeld, vibratoAdvance * c.vibratoRate) * vibratoDepth
                                                                 : globalVibrato;
                            const auto copyHz = pitchToHz (basePitch + notePitch + octave + position * detuneCents * 0.01f + copyVibrato);
                            float left, right;
                            balance (s.pan + notePan + position * s.unisonSpread, left, right);
                            const auto copyLevel = (u == 0 ? 1.0f : s.unisonMix) * noteVolume * s.gain * sign;
                            const auto copyIncrement = juce::jlimit (0.0f, 0.5f, copyHz / (float) sampleRate);
                            if (general)
                            {
                                const auto gainLeft = left * copyLevel, gainRight = right * copyLevel;
                                renderBent (source, bending.forIncrement (copyIncrement), mod, tables::levelFor (copyHz * bending.speed * mod.speed, sampleRate),
                                            tables::levelFor (copyHz * mod.ratio * 2.0f, sampleRate), n, c.phase,
                                            copyIncrement, c.crushPrevious, c.modPhase, c.lastX, [&] (int i, float v)
                                            {
                                                v *= gains[(size_t) i];
                                                outLeft[i] += v * gainLeft;
                                                outRight[i] += v * gainRight;
                                            });
                            }
                            else
                                addCopy (source, outLeft, outRight, gains.data(), n, c.phase, copyIncrement,
                                         tables::levelFor (copyHz, sampleRate), left * copyLevel, right * copyLevel);
                        }
                    }
                }
            }

            if (osc.fadingOut && osc.fade <= 0.0f)
                osc.active = false;
            // SFZ: over when every sample it played has ended.
            if (sampled && std::none_of (osc.players.begin(), osc.players.end(), [] (const SfzPlayer& p) { return p.region != nullptr; }))
                osc.active = false;
        }
    }

    //==========================================================================
    // SFZ (0.71.0)
    void Engine::startSfz (Voice& voice, OscState& osc, size_t index, const OscSettings& s, bool release) noexcept
    {
        // A tail voice: the release samples of the instrument its note played (startTails).
        const auto* inst = release ? (startingTail != nullptr ? startingTail->instruments[index] : nullptr) : s.instrument;
        // The last note's samples (a mono retrigger, a new instrument) fade out over 5 ms.
        const auto fadeStep = -1.0f / juce::jmax (1.0f, 0.005f * (float) sampleRate);
        for (auto& player : osc.players)
            if (player.region != nullptr && player.fadeStep >= 0.0f)
                player.fadeStep = fadeStep;
        osc.instrument = inst;
        osc.heldSamples = 0.0;
        if (inst == nullptr)
            return;

        // SFZ KEY (0.81.0): another key's samples, played at their own pitch (`sfzPlayed` cancels the note's).
        const auto key = juce::jlimit (0, 127, s.sfzKey >= 0 ? s.sfzKey : voice.note);
        osc.sfzKey = key;
        // SFZ KEY TRACK (0.83.0): the chosen key's samples pitched by how far the played key is from it.
        osc.sfzPlayed = s.sfzKey >= 0 && ! s.sfzKeyTrack ? juce::jlimit (0, 127, voice.note) : key;
        const auto velocity127 = juce::jlimit (1, 127, juce::roundToInt (voice.velocity * 127.0f));
        auto& turn = sfzTurns[index][(size_t) key];
        std::array<const sfz::Region*, sfz::maxPlayers> chosen {};
        const auto pedalSounds = release && startingTail != nullptr && startingTail->pedal >= 0;
        const auto switchKey = switchFor (index, inst);
        const auto count = pedalSounds ? inst->selectPedal (startingTail->pedal, random.nextFloat(), chosen.data(), (int) chosen.size())
                                       : inst->select (key, velocity127, release, turn, random.nextFloat(), pedalDown ? 127 : 0,
                                                       chosen.data(), (int) chosen.size(), switchKey);
        if (! release)
            turn = (std::uint8_t) (turn + 1);
        // RT_DECAY: a release sample quieter the longer its note was held.
        const auto held = release && startingTail != nullptr ? startingTail->heldSeconds : 0.0f;
        for (int k = 0; k < count; ++k)
        {
            const auto& region = *chosen[(size_t) k];
            // A free player, else the quietest fading one.
            SfzPlayer* player = nullptr;
            for (auto& p : osc.players)
                if (p.region == nullptr)
                {
                    player = &p;
                    break;
                }
            if (player == nullptr)
                for (auto& p : osc.players)
                    if (p.fadeStep < 0.0f && (player == nullptr || p.fade < player->fade))
                        player = &p;
            if (player == nullptr)
                break;
            const auto& sample = *inst->samples[(size_t) region.sample];
            player->region = &region;
            player->instrument = inst;
            player->sample = &sample;
            player->position = (double) region.offset;
            player->fade = 1.0f;
            player->fadeStep = 0.0f;
            player->decay = 1.0f;
            player->wheel = -1.0f;
            std::fill (std::begin (player->filterA), std::end (player->filterA), 0.0f);
            std::fill (std::begin (player->filterB), std::end (player->filterB), 0.0f);
            auto gain = region.gain * region.velocityGain (voice.velocity) * region.crossfade (velocity127);
            // A loop needs the sample whole in memory (the reader keeps a looped one so).
            player->looping = region.loops() && region.loopEnd < (juce::int64) sample.headFrames;   // (reads past LOOP_END wrap round)
            if (release && region.rtDecayDb > 0.0f)
                gain *= juce::Decibels::decibelsToGain (-region.rtDecayDb * held, -120.0f);
            float l, r;
            balance (region.pan, l, r);
            player->gainLeft = gain * l;
            player->gainRight = gain * r;
            player->stream = sample.stream != nullptr && streamer != nullptr ? streamer->claim (*sample.stream) : -1;
        }
        // The released note's ringing samples, carried on here (their streams too).
        if (release && startingTail != nullptr)
            for (const auto& handed : startingTail->handed[index])
                if (handed.region != nullptr)
                    for (auto& p : osc.players)
                        if (p.region == nullptr)
                        {
                            p = handed;
                            break;
                        }
    }

    int Engine::switchFor (size_t index, const sfz::Instrument* inst) noexcept
    {
        if (inst == nullptr)
            return -1;
        if (sfzSwitchFor[index] != inst)
        {
            sfzSwitchFor[index] = inst;
            sfzSwitch[index] = inst->switchDefault;
            switchShown[index].store (inst->switchDefault);
        }
        return sfzSwitch[index];
    }

    float Engine::sfzWheel127 (const sfz::Instrument* inst) const noexcept
    {
        // The wheel as the matrix hears it (the controllers sent to it too)...
        auto wheel = modWheel;
        if (sfzToWheel[0]) wheel = juce::jmax (wheel, breath);
        if (sfzToWheel[1]) wheel = juce::jmax (wheel, expression);
        if (sfzToWheel[2]) wheel = juce::jmax (wheel, channelPressure);
        if (sfzToWheel[3] && pedalDown) wheel = 1.0f;
        // ...or, until it's touched, where the instrument says it stands (SET_CC1).
        if (! modWheelHeard && wheel <= 0.0f && inst != nullptr && inst->setCc1 >= 0)
            return (float) inst->setCc1;
        return wheel * 127.0f;
    }

    void Engine::renderSfz (Voice& voice, OscState& osc, const OscSettings& s, float semitones, float* left, float* right, int n) noexcept
    {
        juce::ignoreUnused (s);
        const auto cc127 = sfzWheel127 (osc.instrument);
        const auto nyquistish = 0.45f * (float) sampleRate;
        std::fill (left, left + n, 0.0f);
        std::fill (right, right + n, 0.0f);
        osc.heldSamples += n;
        for (auto& player : osc.players)
        {
            if (player.region == nullptr)
                continue;
            const auto& region = *player.region;
            const auto& sample = *player.sample;
            // Its speed: the key played against the region's PITCH_KEYCENTER (PITCH_KEYTRACK cents a key), its TUNE,
            // and the oscillator's pitch against the key (TRANSPOSE, FINE, bend, glide, vibrato...).
            const auto offset = (semitones - (float) osc.sfzPlayed) + ((float) osc.sfzKey - region.keyCenter) * region.keytrack + region.tuneSemitones;
            const auto step = sample.sampleRate / sampleRate * std::exp2 ((double) offset / 12.0);
            const auto length = sample.length;
            const auto readable = player.stream >= 0 ? length : (juce::int64) sample.headFrames;
            const auto* headL = sample.head[0].data();
            const auto* headR = sample.head[1].data();
            const auto heads = (juce::int64) sample.headFrames;
            // LOOPS (0.73.0): past LOOP_END a read goes round to LOOP_START (the sample is whole in memory).
            const auto looping = player.looping;
            const auto loopStart = region.loopStart, loopEnd = region.loopEnd, loopLength = loopEnd - loopStart + 1;
            const auto frameAt = [&] (int channel, juce::int64 frame) -> float
            {
                if (looping && frame > loopEnd)
                    frame = loopStart + (frame - loopStart) % loopLength;
                if (frame < 0 || frame >= readable)
                    return 0.0f;
                if (frame < heads)
                    return (channel == 0 ? headL : headR)[frame];
                return streamer->sample (player.stream, channel, frame, waitForDisk);
            };
            // Heads only (no stream free): a 5 ms fade before the head runs out.
            const auto endFade = player.stream < 0 && readable < length ? 0.005 * sample.sampleRate : 0.0;
            auto pos = player.position;
            // THE MOD WHEEL (0.75.0): its level glides across the block from where the last one left it.
            const auto wheelTarget = region.wheelLevel (cc127);
            if (player.wheel < 0.0f)
                player.wheel = wheelTarget;
            auto wheel = player.wheel;
            const auto wheelStep = (wheelTarget - wheel) / (float) juce::jmax (1, n);
            // THE FILTER (0.75.0): its cutoff for this note, velocity and wheel, set once a block (TPT: zero-delay feedback).
            using F = sfz::Region::Filter;
            const auto filter = region.filter;
            float g = 0.0f, k = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
            if (filter != F::none)
            {
                const auto fc = juce::jlimit (10.0f, nyquistish, region.cutoffFor (osc.sfzKey, voice.velocity, cc127));
                g = std::tan (juce::MathConstants<float>::pi * fc / (float) sampleRate);
                k = 1.0f / region.filterQ;
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }
            for (int i = 0; i < n; ++i)
            {
                const auto frame = (juce::int64) pos;
                if (frame >= readable || player.fade <= 0.0f)
                {
                    player.region = nullptr;
                    break;
                }
                const auto t = (float) (pos - (double) frame);
                float out[2];
                for (int c = 0; c < 2; ++c)
                {
                    float y0, y1, y2, y3;
                    if (frame >= 1 && frame + 2 < heads && (! looping || frame + 2 <= loopEnd))
                    {
                        const auto* h = (c == 0 ? headL : headR) + frame;
                        y0 = h[-1]; y1 = h[0]; y2 = h[1]; y3 = h[2];
                    }
                    else
                    {
                        y0 = frameAt (c, frame - 1); y1 = frameAt (c, frame);
                        y2 = frameAt (c, frame + 1); y3 = frameAt (c, frame + 2);
                    }
                    // Four-point Hermite between y1 and y2.
                    const auto c1 = 0.5f * (y2 - y0);
                    const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                    const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                    out[c] = ((c3 * t + c2) * t + c1) * t + y1;
                    if (filter == F::lowpass1 || filter == F::highpass1)
                    {
                        auto& z = player.filterA[c];
                        const auto v = (out[c] - z) * g / (1.0f + g);
                        const auto lp = v + z;
                        z = lp + v;
                        out[c] = filter == F::lowpass1 ? lp : out[c] - lp;
                    }
                    else if (filter != F::none)
                    {
                        auto& ic1 = player.filterA[c];
                        auto& ic2 = player.filterB[c];
                        const auto v3 = out[c] - ic2;
                        const auto v1 = a1 * ic1 + a2 * v3;
                        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
                        ic1 = 2.0f * v1 - ic1;
                        ic2 = 2.0f * v2 - ic2;
                        out[c] = filter == F::lowpass2 ? v2 : out[c] - k * v1 - v2;
                    }
                }
                wheel += wheelStep;
                auto gain = player.fade * wheel;
                if (endFade > 0.0 && (double) readable - pos < endFade)
                    gain *= (float) (((double) readable - pos) / endFade);
                left[i] += out[0] * player.gainLeft * gain;
                right[i] += out[1] * player.gainRight * gain;
                if (player.fadeStep < 0.0f)
                    player.fade += player.fadeStep;
                if (player.decay < 1.0f && (player.fade *= player.decay) < 1.0e-4f)
                    player.fade = 0.0f;   // 80 dB down: over
                pos += step;
                if (looping && pos >= (double) (loopEnd + 1))
                    pos -= (double) loopLength * std::floor ((pos - (double) loopStart) / (double) loopLength);
            }
            player.position = pos;
            player.wheel = wheelTarget;
            if (player.region != nullptr && player.stream >= 0)
                streamer->consumed (player.stream, juce::jmax ((juce::int64) 0, (juce::int64) pos - 2));
        }
    }

    void Engine::startTails (const Settings& settings) noexcept
    {
        for (int k = 0; k < numPendingTails; ++k)
        {
            auto& tail = pendingTails[(size_t) k];
            if (tail.pedal >= 0)
            {
                // The pedal moved: every sampled oscillator whose instrument has pedal sounds.
                for (size_t i = 0; i < settings.oscillators.size(); ++i)
                {
                    const auto& s = settings.oscillators[i];
                    const auto* inst = s.on && s.wave == OscWave::sfz ? s.instrument : nullptr;
                    tail.instruments[i] = inst != nullptr && inst->hasPedalSounds ? inst : nullptr;
                    if (tail.instruments[i] != nullptr)
                        tail.mask = (std::uint8_t) (tail.mask | (1u << i));
                }
                if (tail.mask == 0)
                    continue;
            }
            // A free voice of the pool, or none (release samples never take a note's place).
            Voice* voice = nullptr;
            for (auto& v : voices)
                if (! v.playing)
                {
                    voice = &v;
                    break;
                }
            if (voice == nullptr)
                break;
            const auto keysLastPitch = lastPitch;   // a tail doesn't move the keys' glide
            startingTail = &tail;
            startVoice (*voice, tail.note, tail.velocity, (float) tail.note, settings, tailSource, tail.mask);
            startingTail = nullptr;
            lastPitch = keysLastPitch;
            voice->held = false;               // nothing lets it go: it ends with its samples
            if (std::none_of (voice->oscillators.begin(), voice->oscillators.end(), [] (const OscState& o) { return o.active; }))
                free (*voice);
        }
        numPendingTails = 0;
    }
}
