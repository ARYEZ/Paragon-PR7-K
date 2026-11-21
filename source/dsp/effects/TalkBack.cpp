#include "TalkBack.h"

namespace effects
{
    namespace
    {
        constexpr double glideSeconds = 0.045;    // a stroke's pitch reaches its note this fast (raised cosine)
        constexpr double flamSeconds = 0.045;     // a flam's grace stroke, this long before the note
        constexpr float makeUp = 0.42f;           // the membrane's partials peak near 1 at full strength
        constexpr std::array<float, 5> partialLevels { 1.0f, 0.5f, 0.32f, 0.2f, 0.12f };
        constexpr std::array<float, 5> partialDecays { 1.0f, 0.6f, 0.45f, 0.35f, 0.3f };   // of the fundamental's time
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
    }

    float TalkBack::phraseHash (double line, std::uint32_t salt) noexcept
    {
        auto x = (std::uint32_t) (std::int64_t) std::llround (line * 960.0) ^ (salt * 0x9e3779b9u);
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return (float) (x >> 8) / 16777216.0f;
    }

    void TalkBack::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        output.setSize (2, juce::jmax (1, maxBlockSize));
        crackDecay = (float) std::exp (-1.0 / (0.003 * sampleRate));
        reset();
    }

    void TalkBack::reset() noexcept
    {
        output.clear();
        rendered = 0;
        silent = true;
        numEvents = 0;
        phraseNotes = 0;
        held = 0;
        lastActivity = -1.0e9;
        ownBeat = 0.0;
        lastBeat = -1.0e9;
        numStrokes = nextStroke = 0;
        env.fill (0.0f);
        phase.fill (0.0);
        crack = crackLow = crackHigh = 0.0f;
        fromSemis = toSemis = squeeze = semisNow = 0.0f;
        strokeAge = 1.0e9;
        answering.store (false, std::memory_order_relaxed);
    }

    float TalkBack::random01() noexcept
    {
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        return (float) (noise >> 8) / 16777216.0f;
    }

    void TalkBack::addNote (int offset, float note, float velocity, bool press) noexcept
    {
        if (numEvents < maxEvents)
            events[(size_t) numEvents++] = { offset, note, velocity, press };
    }

    void TalkBack::closePhrase (double line, const Settings& s) noexcept
    {
        const auto unit = s.wait == 1 ? 1.0 : 4.0;
        const auto offset = line - phraseLine;
        // The phrase's middle note: the drum's centre stands for it, the others above and below by RANGE.
        std::array<float, maxNotes> sorted {};
        for (int k = 0; k < phraseNotes; ++k)
            sorted[(size_t) k] = phrase[(size_t) k].note;
        std::sort (sorted.begin(), sorted.begin() + phraseNotes);
        const auto middle = phraseNotes % 2 == 1 ? sorted[(size_t) (phraseNotes / 2)]
                                                 : 0.5f * (sorted[(size_t) (phraseNotes / 2 - 1)] + sorted[(size_t) (phraseNotes / 2)]);

        const auto answered = phraseHash (phraseLine, 1) < answerChance (s.amount);
        const auto ornaments = ornamentChance (s.amount);
        const auto range = juce::jlimit (0.2f, 1.0f, s.range);
        const auto flamBeats = flamSeconds * beatsPerSecond;
        std::array<Stroke, maxNotes + 2> made {};
        int count = 0;
        double lastEnd = 0.0;
        float lastSemis = 0.0f;
        for (int k = 0; k < phraseNotes; ++k)
        {
            const auto& h = phrase[(size_t) k];
            const auto end = h.off >= h.on ? h.off : lastActivity;
            const auto length = juce::jmax (0.0, end - h.on);
            const auto semis = juce::jlimit (-12.0f, 12.0f, range * (h.note - middle));
            const auto onBeat = std::abs (h.on - std::round (h.on)) < 0.02;
            const auto velocity = juce::jlimit (0.1f, 1.0f, h.velocity) * (onBeat ? 1.0f : 0.75f);
            // The drummer's own: a flam (a quick grace stroke a semitone and a half under) before a long note.
            if (length >= 1.0 && phraseHash (h.on, 2) < ornaments && count < (int) made.size())
                made[(size_t) count++] = { h.on + offset - flamBeats, semis - 1.5f, 0.45f * velocity, 0.0 };
            if (count < (int) made.size())
                made[(size_t) count++] = { h.on + offset, semis, velocity, length };
            if (h.on + length >= lastEnd)
            {
                lastEnd = h.on + length;
                lastSemis = semis;
            }
        }
        // ...and a falling two-stroke tag after the answer.
        const auto tag = phraseHash (phraseLine, 7) < ornaments;

        // The page sees every phrase heard, answered or not.
        const auto shown = juce::jmin (phraseNotes, shownNotes);
        for (int k = 0; k < shown; ++k)
        {
            shownBeat[(size_t) k].store ((float) (phrase[(size_t) k].on - phraseLine), std::memory_order_relaxed);
            const auto end = phrase[(size_t) k].off >= phrase[(size_t) k].on ? phrase[(size_t) k].off : lastActivity;
            shownLength[(size_t) k].store ((float) juce::jmax (0.0, end - phrase[(size_t) k].on), std::memory_order_relaxed);
            shownNote[(size_t) k].store (phrase[(size_t) k].note - middle, std::memory_order_relaxed);
        }
        shownNoteCount.store (shown, std::memory_order_relaxed);
        shownUnit.store ((float) unit, std::memory_order_relaxed);
        phraseNotes = 0;

        if (! answered)
        {
            shownStrokeCount.store (0, std::memory_order_relaxed);
            return;
        }
        answers.fetch_add (1, std::memory_order_relaxed);
        int strokesShown = 0;
        const auto add = [&] (const Stroke& stroke)
        {
            if (numStrokes >= maxStrokes)
                return;
            // In order of beat (an answer can still be playing when the next is made).
            auto at = numStrokes;
            while (at > nextStroke && strokes[(size_t) at - 1].beat > stroke.beat)
            {
                strokes[(size_t) at] = strokes[(size_t) at - 1];
                --at;
            }
            strokes[(size_t) at] = stroke;
            ++numStrokes;
            if (strokesShown < shownNotes * 2)
            {
                shownStrokeBeat[(size_t) strokesShown].store ((float) (stroke.beat - line), std::memory_order_relaxed);
                shownStrokeSemis[(size_t) strokesShown].store (stroke.semis, std::memory_order_relaxed);
                ++strokesShown;
            }
        };
        for (int k = 0; k < count; ++k)
            add (made[(size_t) k]);
        if (tag)
        {
            add ({ lastEnd + offset + 0.5, lastSemis - 3.0f, 0.55f, 0.25 });
            add ({ lastEnd + offset + 0.75, lastSemis - 6.0f, 0.7f, 0.5 });
        }
        shownStrokeCount.store (strokesShown, std::memory_order_relaxed);
    }

    void TalkBack::strike (const Stroke& stroke, const Settings& s) noexcept
    {
        fromSemis = semisNow;
        toSemis = stroke.semis;
        strokeAge = 0.0;
        const auto seconds = stroke.length / juce::jmax (0.01, beatsPerSecond);
        // A note held half a beat or more bends up as it's held, the player squeezing the drum's strings.
        squeeze = stroke.length >= 0.5 ? juce::jlimit (0.0f, 3.0f, s.bend) * bendShare (s.amount) : 0.0f;
        squeezeSeconds = juce::jmax (0.05, seconds);
        for (auto& e : env)
            e = juce::jmin (1.0f, e + stroke.velocity);
        crack = stroke.velocity;
        strokeCount.fetch_add (1, std::memory_order_relaxed);
    }

    void TalkBack::render (int numSamples, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = juce::jmin (numSamples, output.getNumSamples());
        rendered = juce::jmax (0, n);
        if (n <= 0)
        {
            numEvents = 0;
            return;
        }
        beatsPerSecond = juce::jmax (20.0, bpm) / 60.0;
        // Off and quiet: nothing heard, nothing to answer, nothing ringing.
        if (! s.on && env[0] < 1.0e-5f && crack < 1.0e-6f)
        {
            numEvents = 0;
            phraseNotes = held = 0;
            numStrokes = nextStroke = 0;
            silent = true;
            lastBeat = -1.0e9;
            answering.store (false, std::memory_order_relaxed);
            return;
        }
        output.clear (0, n);
        const auto step = beatsPerSecond / sampleRate;
        const auto playing = ppq >= 0.0;
        const auto start = playing ? ppq : ownBeat;
        ownBeat = start + n * step;
        // The song moved (a jump, a loop round, play from elsewhere): what was heard and what was to answer belong
        // to another place.
        if (lastBeat > -1.0e8 && (start < lastBeat - 0.5 || start > lastBeat + 2.0))
        {
            phraseNotes = held = 0;
            numStrokes = nextStroke = 0;
        }
        lastBeat = ownBeat;

        const auto listening = s.on && s.amount >= 0.001f;
        if (! s.on)
            numStrokes = nextStroke = 0;   // off: nothing more to say (what rings dies away)
        if (! listening)
        {
            phraseNotes = held = 0;
            numEvents = 0;
        }
        std::stable_sort (events.begin(), events.begin() + numEvents, [] (const Event& a, const Event& b) { return a.offset < b.offset; });

        const auto unit = s.wait == 1 ? 1.0 : 4.0;
        const auto rest = s.wait == 1 ? 0.5 : 0.25;   // how long a rest before a line closes a phrase
        const auto tone = juce::jlimit (0.0f, 1.0f, s.tone);
        const auto baseSeconds = 0.18 + 0.14 * (1.0 - tone);
        for (size_t k = 0; k < partials.size(); ++k)
            decay[k] = (float) std::exp (-1.0 / (baseSeconds * partialDecays[k] * sampleRate));
        std::array<float, partials.size()> levels {};
        for (size_t k = 0; k < partials.size(); ++k)
            levels[k] = partialLevels[k] * (k == 0 ? 1.0f : 0.5f + tone);
        const auto crackLevel = 0.9f * (0.4f + 0.6f * tone);
        const auto crackLowA = 1.0f - (float) std::exp (-twoPi * 700.0 / sampleRate);
        const auto crackHighA = 1.0f - (float) std::exp (-twoPi * juce::jmin (0.45 * sampleRate, 2000.0 + 5000.0 * tone) / sampleRate);
        const auto gain = juce::Decibels::decibelsToGain (juce::jlimit (-36.0f, 6.0f, s.levelDb)) * makeUp;
        const auto angle = (juce::jlimit (-1.0f, 1.0f, s.pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const auto gainL = gain * std::cos (angle) * juce::MathConstants<float>::sqrt2;
        const auto gainR = gain * std::sin (angle) * juce::MathConstants<float>::sqrt2;
        const auto pitchHz = juce::jlimit (80.0f, 260.0f, s.pitchHz);
        auto* left = output.getWritePointer (0);
        auto* right = output.getWritePointer (1);

        int nextEvent = 0;
        auto peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const auto beat = start + i * step;
            // The line crossed at this sample (a bar or a beat): a phrase that has come to rest is answered.
            {
                const auto before = std::floor ((beat - step) / unit), now = std::floor (beat / unit);
                if (now > before && phraseNotes > 0 && held == 0 && now * unit - lastActivity >= rest)
                    closePhrase (now * unit, s);
            }
            while (nextEvent < numEvents && events[(size_t) nextEvent].offset <= i)
            {
                const auto& e = events[(size_t) nextEvent++];
                if (e.press)
                {
                    if (phraseNotes == 0)
                        phraseLine = std::floor (beat / unit) * unit;
                    if (phraseNotes < maxNotes)
                        phrase[(size_t) phraseNotes++] = { beat, -1.0, e.note, e.velocity };
                    ++held;
                }
                else
                {
                    held = juce::jmax (0, held - 1);
                    for (int k = phraseNotes; --k >= 0;)
                        if (phrase[(size_t) k].off < 0.0 && std::abs (phrase[(size_t) k].note - e.note) < 0.01f)
                        {
                            phrase[(size_t) k].off = beat;
                            break;
                        }
                }
                lastActivity = beat;
            }
            // A key whose letting go never came (a stolen note): forgotten after four bars.
            if (held > 0 && beat - lastActivity > 16.0)
                held = 0;

            while (nextStroke < numStrokes && strokes[(size_t) nextStroke].beat <= beat)
                strike (strokes[(size_t) nextStroke++], s);
            if (nextStroke >= numStrokes)
                numStrokes = nextStroke = 0;

            // The drum: silent and nothing to strike, nothing worked out.
            if (env[0] < 1.0e-5f && crack < 1.0e-6f)
                continue;
            const auto t = strokeAge;
            const auto glide = t < glideSeconds ? 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * t / glideSeconds) : 1.0;
            auto semis = fromSemis + (toSemis - fromSemis) * (float) glide;
            if (squeeze > 0.0f)
                semis += squeeze * std::pow ((float) juce::jmin (1.0, t / squeezeSeconds), 0.7f);
            semisNow = semis;
            const auto hz = (double) pitchHz * std::exp2 ((double) semis / 12.0);
            float y = 0.0f;
            for (size_t k = 0; k < partials.size(); ++k)
            {
                phase[k] += hz * partials[k] / sampleRate;
                phase[k] -= std::floor (phase[k]);
                y += levels[k] * env[k] * (float) std::sin (twoPi * phase[k]);
                env[k] *= decay[k];
            }
            // The stick's crack: noise through a band-pass, falling fast.
            if (crack > 1.0e-6f)
            {
                const auto x = (random01() * 2.0f - 1.0f) * crack;
                crack *= crackDecay;
                crackLow += crackLowA * (x - crackLow);
                crackHigh += crackHighA * ((x - crackLow) - crackHigh);
                y += crackLevel * crackHigh;
            }
            left[i] = y * gainL;
            right[i] = y * gainR;
            peak = juce::jmax (peak, std::abs (y));
            strokeAge += 1.0 / sampleRate;
        }
        numEvents = 0;
        silent = peak == 0.0f;
        pitchNow.store (semisNow, std::memory_order_relaxed);
        answering.store (numStrokes > 0 || env[0] > 0.01f, std::memory_order_relaxed);
    }

    void TalkBack::addTo (juce::dsp::AudioBlock<float> block) const noexcept
    {
        if (silent)
            return;
        const auto n = juce::jmin ((int) block.getNumSamples(), rendered);
        if (block.getNumChannels() == 1)
        {
            auto* out = block.getChannelPointer (0);
            for (int i = 0; i < n; ++i)
                out[i] += 0.5f * (output.getSample (0, i) + output.getSample (1, i));
            return;
        }
        for (size_t c = 0; c < 2; ++c)
            juce::FloatVectorOperations::add (block.getChannelPointer (c), output.getReadPointer ((int) c), n);
    }

    TalkBack::Picture TalkBack::getPicture() const noexcept
    {
        Picture p;
        p.notes = juce::jlimit (0, shownNotes, shownNoteCount.load (std::memory_order_relaxed));
        p.strokes = juce::jlimit (0, shownNotes * 2, shownStrokeCount.load (std::memory_order_relaxed));
        p.unit = shownUnit.load (std::memory_order_relaxed);
        for (int k = 0; k < p.notes; ++k)
        {
            p.noteBeat[(size_t) k] = shownBeat[(size_t) k].load (std::memory_order_relaxed);
            p.noteLength[(size_t) k] = shownLength[(size_t) k].load (std::memory_order_relaxed);
            p.noteValue[(size_t) k] = shownNote[(size_t) k].load (std::memory_order_relaxed);
        }
        for (int k = 0; k < p.strokes; ++k)
        {
            p.strokeBeat[(size_t) k] = shownStrokeBeat[(size_t) k].load (std::memory_order_relaxed);
            p.strokeSemis[(size_t) k] = shownStrokeSemis[(size_t) k].load (std::memory_order_relaxed);
        }
        return p;
    }
}
