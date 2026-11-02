#include "Infernal.h"

namespace effects
{
    namespace
    {
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        constexpr float fuzzBias = 0.35f;     // the fuzz's lopsidedness (even harmonics, a buzz)
        const float fuzzRest = (float) std::tanh ((double) fuzzBias);   // its output at rest, taken off (as the clip works it out)
        constexpr float glueThreshold = 0.25f;   // about -12 dBFS
        constexpr float glueRatio = 4.0f;
        // Each chain's make-up so their blends at 1 come out at about the same level (measured on a saw, see
        // InfernalTests), and the destroyed part at about the dry sound's level at DRIVE 0.5.
        constexpr float fuzzMakeUp = 0.55f, crushMakeUp = 0.5f, ampMakeUp = 0.75f, noiseMakeUp = 0.9f;
        constexpr float wetMakeUp = 1.0f;
    }

    float Infernal::tightHz (float tight) noexcept
    {
        return 30.0f * std::pow (400.0f / 30.0f, juce::jlimit (0.0f, 1.0f, tight));
    }

    float Infernal::cabHz (float cab) noexcept
    {
        return 2500.0f * std::pow (3.0f, juce::jlimit (0.0f, 1.0f, cab));
    }

    float Infernal::driveGain (float drive) noexcept
    {
        return juce::Decibels::decibelsToGain (42.0f * juce::jlimit (0.0f, 1.0f, drive));
    }

    void Infernal::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        const auto coefficient = [this] (double seconds) { return 1.0f - (float) std::exp (-1.0 / (seconds * sampleRate)); };
        followAttack = coefficient (0.002);
        followRelease = coefficient (0.12);
        bloomUp = coefficient (1.2);      // a held note's feedback swells in over a second or so
        bloomDown = coefficient (0.08);
        glueAttack = coefficient (0.004);
        glueRelease = coefficient (0.09);
        crushStep = juce::jmax (1, (int) std::round (sampleRate / 11025.0));
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize), 1 };
        for (auto& ch : channels)
        {
            ch.mid.prepare (spec);
            ch.mid.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            ch.mid.setCutoffFrequency (900.0f);
            ch.mid.setResonance (0.7f);
            ch.cab.prepare (spec);
            ch.cab.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            ch.cab.setResonance (1.2f);
            ch.bump.prepare (spec);
            ch.bump.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            ch.bump.setCutoffFrequency (juce::jmin (1800.0f, (float) (0.4 * sampleRate)));
            ch.bump.setResonance (1.5f);
        }
        lastCabHz = -1.0f;
        reset();
    }

    void Infernal::reset() noexcept
    {
        quietReset();
        follower = 0.0f;
        lastMix = 0.0f;
        notePhase.fill (0.0);
        bloomPhase.fill (0.0);
    }

    void Infernal::quietReset() noexcept
    {
        for (auto& ch : channels)
        {
            ch.split = ch.split2 = 0.0f;
            ch.fuzzClip = {};
            ch.fuzzClip.last = (double) fuzzBias;   // at rest the fuzz sits on its bias (or waking would click)
            ch.ampClip1 = {};
            ch.ampClip2 = {};
            ch.noiseClip = {};
            ch.crushClip = {};
            ch.fuzzTone = ch.noiseTone = ch.crushHeld = ch.cabLow = 0.0f;
            ch.crushCount = 0;
            ch.mid.reset();
            ch.cab.reset();
            ch.bump.reset();
        }
        bloom = 0.0f;
        glueEnvelope = 0.0f;
        quietSamples = 0;
    }

    void Infernal::setNotes (const float* newNotes, int count) noexcept
    {
        if (count <= 0)
            return;
        // The lowest four: the riff's root and its power chord.
        std::array<float, 64> sorted {};
        const auto n = juce::jmin (count, (int) sorted.size());
        std::copy_n (newNotes, n, sorted.begin());
        std::sort (sorted.begin(), sorted.begin() + n);
        numNotes = juce::jmin (n, maxNotes);
        std::copy_n (sorted.begin(), numNotes, notes.begin());
    }

    void Infernal::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto numSamples = (int) block.getNumSamples();
        const auto numChannels = juce::jmin (2, (int) block.getNumChannels());
        if (numChannels == 0 || numSamples == 0)
            return;
        const auto mixTarget = s.on ? juce::jlimit (0.0f, 1.0f, s.mix) : 0.0f;
        if (mixTarget <= 0.0f && lastMix <= 0.0f)
        {
            // Nothing to blend in: the dry sound exactly (and the chains rest, ready).
            if (quietSamples >= 0)
                quietReset();
            quietSamples = -1;
            for (auto& d : chainDb)
                d.store (-100.0f, std::memory_order_relaxed);
            bloomShown.store (0.0f, std::memory_order_relaxed);
            reductionShown.store (0.0f, std::memory_order_relaxed);
            return;
        }
        if (quietSamples < 0)
            quietSamples = 0;

        const auto split = 1.0f - (float) std::exp (-twoPi * tightHz (s.tight) / sampleRate);
        const auto gain = driveGain (s.drive);
        const auto cab = juce::jmin (cabHz (s.cab), (float) (0.42 * sampleRate));
        if (! juce::approximatelyEqual (cab, lastCabHz))
        {
            for (auto& ch : channels)
                ch.cab.setCutoffFrequency (cab);
            lastCabHz = cab;
        }
        const auto cabLowCut = 1.0f - (float) std::exp (-twoPi * 90.0 / sampleRate);
        const auto toneCut = 1.0f - (float) std::exp (-twoPi * 6500.0 / sampleRate);
        const float blends[numChains] = { juce::jlimit (0.0f, 1.0f, s.fuzz) * fuzzMakeUp, juce::jlimit (0.0f, 1.0f, s.crush) * crushMakeUp,
                                          juce::jlimit (0.0f, 1.0f, s.amp) * ampMakeUp, juce::jlimit (0.0f, 1.0f, s.noise) * noiseMakeUp };
        const auto feedbackAmount = juce::jlimit (0.0f, 1.0f, s.feedback);
        const auto level = juce::Decibels::decibelsToGain (s.levelDb) * wetMakeUp;

        // The notes' rates (cycles a sample), for the noise's pulse and the bloom (an octave up).
        std::array<double, maxNotes> rate {};
        for (int k = 0; k < numNotes; ++k)
            rate[(size_t) k] = 440.0 * std::exp2 ((notes[(size_t) k] - 69.0) / 12.0) / sampleRate;
        const auto noteScale = numNotes > 0 ? 1.0f / (float) numNotes : 0.0f;

        float peaks[numChains] {};
        float loudest = 0.0f, reduction = 1.0f;
        auto* left = block.getChannelPointer (0);
        auto* right = numChannels > 1 ? block.getChannelPointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto mix = lastMix + (mixTarget - lastMix) * (float) (i + 1) / (float) numSamples;
            const float in[2] = { left[i], right != nullptr ? right[i] : left[i] };
            const auto x = juce::jmax (std::abs (in[0]), std::abs (in[1]));
            follower += (x - follower) * (x > follower ? followAttack : followRelease);
            const auto heard = follower < 1.0e-4f ? 0.0f : juce::jmin (1.0f, follower / 0.15f);

            // The notes: their pulse (for the noise, once a cycle) and the bloom's sine (an octave up).
            float pulse = 0.0f, bloomWave = 0.0f;
            for (int k = 0; k < numNotes; ++k)
            {
                notePhase[(size_t) k] += rate[(size_t) k];
                notePhase[(size_t) k] -= std::floor (notePhase[(size_t) k]);
                bloomPhase[(size_t) k] += 2.0 * rate[(size_t) k];
                bloomPhase[(size_t) k] -= std::floor (bloomPhase[(size_t) k]);
                const auto c = 0.5f + 0.5f * (float) std::sin (twoPi * notePhase[(size_t) k]);
                pulse += c * c;
                bloomWave += (float) std::sin (twoPi * bloomPhase[(size_t) k]);
            }
            pulse *= noteScale;
            bloomWave *= noteScale;
            const auto bloomTarget = feedbackAmount * heard;
            bloom += (bloomTarget - bloom) * (bloomTarget > bloom ? bloomUp : bloomDown);

            float wet[2] {};
            float lows[2] {};
            for (int c = 0; c < numChannels; ++c)
            {
                auto& ch = channels[(size_t) c];
                // TIGHT: the lows (two poles) stay clean; the rest is destroyed.
                ch.split += (in[c] - ch.split) * split;
                ch.split2 += (ch.split - ch.split2) * split;
                lows[c] = ch.split2;
                const auto u = (in[c] - ch.split2) * gain;

                // FUZZ.
                auto fuzz = ch.fuzzClip.process ((double) (u * 1.5f + fuzzBias)) - fuzzRest;
                ch.fuzzTone += (fuzz - ch.fuzzTone) * toneCut;
                fuzz = ch.fuzzTone;

                // CRUSH: held every few samples (about 11 kHz), 5 bits.
                if (--ch.crushCount <= 0)
                {
                    ch.crushCount = crushStep;
                    ch.crushHeld = std::round (ch.crushClip.process ((double) u) * 16.0f) / 16.0f;
                }
                const auto crush = ch.crushHeld;

                // AMP: a mid push and the bloom into two gain stages, then the cabinet.
                const auto pushed = u * 0.6f + ch.mid.processSample (0, u) * 1.4f + bloomWave * bloom * 0.5f;   // (not through DRIVE: it dies with the note)
                const auto stage1 = ch.ampClip1.process ((double) pushed * 0.8);
                const auto stage2 = ch.ampClip2.process ((double) stage1 * 3.0);
                ch.cabLow += (stage2 - ch.cabLow) * cabLowCut;
                const auto cabIn = stage2 - ch.cabLow;
                const auto amp = ch.cab.processSample (0, cabIn) + 0.5f * ch.bump.processSample (0, cabIn);

                // NOISE: pulsed at the notes' pitch, following the sound, driven.
                const auto n = random (c) * pulse * heard;
                ch.noiseTone += (n - ch.noiseTone) * toneCut;
                const auto noise = ch.noiseClip.process ((double) ch.noiseTone * (1.0 + 0.1 * gain));

                const float chains[numChains] = { fuzz * blends[0], crush * blends[1], amp * blends[2], noise * blends[3] };
                for (int k = 0; k < numChains; ++k)
                {
                    wet[c] += chains[k];
                    peaks[k] = juce::jmax (peaks[k], std::abs (chains[k]));
                }
            }
            if (numChannels == 1)
            {
                wet[1] = wet[0];
                lows[1] = lows[0];
            }

            // The glue: one compressor on both sides.
            const auto level2 = juce::jmax (std::abs (wet[0]), std::abs (wet[1]));
            glueEnvelope += (level2 - glueEnvelope) * (level2 > glueEnvelope ? glueAttack : glueRelease);
            const auto glue = glueEnvelope > glueThreshold ? std::pow (glueEnvelope / glueThreshold, 1.0f / glueRatio - 1.0f) : 1.0f;
            reduction = juce::jmin (reduction, glue);

            for (int c = 0; c < numChannels; ++c)
            {
                const auto destroyed = lows[c] + wet[c] * glue * level;
                const auto out = in[c] * (1.0f - mix) + destroyed * mix;
                (c == 0 ? left : right)[i] = out;
                loudest = juce::jmax (loudest, std::abs (destroyed));
            }
        }
        lastMix = mixTarget;

        // Nothing coming in for a while: rest the chains at exactly zero (the filters' tails would never quite end).
        quietSamples = (follower < 1.0e-6f && loudest < 1.0e-6f) ? quietSamples + numSamples : 0;
        if (quietSamples > (int) (0.1 * sampleRate))
        {
            quietReset();
            quietSamples = 1;
        }

        for (int k = 0; k < numChains; ++k)
            chainDb[(size_t) k].store (peaks[k] > 1.0e-5f ? juce::Decibels::gainToDecibels (peaks[k]) : -100.0f, std::memory_order_relaxed);
        bloomShown.store (bloom, std::memory_order_relaxed);
        reductionShown.store (juce::Decibels::gainToDecibels (reduction), std::memory_order_relaxed);
    }
}
