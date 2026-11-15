#include "Wobble.h"

#include "dsp/synth/VoiceFilter.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;   // AMOUNT's smoothing
        constexpr float cutoffSeconds = 0.004f;  // the cutoff's glide (SNAP opens over ~4 ms, not in one step)
        constexpr float levelSeconds = 0.3f;     // the level kept: power followed over this
        constexpr float gainSeconds = 0.1f;
        constexpr float openHz = 20000.0f;
        constexpr float driveIn = 1.5f;          // the resonant stage's light saturation
        constexpr float talkMakeUp = 3.0f;       // TALK's band-passes take a lot away; the level keeper does the rest
        constexpr float driveRef = 0.3f;         // DRIVE scaled so this level stays the same
        // TALK's formants widened from the singer's data (40-120 Hz wide): on a low bass most harmonics fell between
        // such narrow peaks and the vowel barely showed (3.6 dB of movement measured); a growl wants broad ones.
        constexpr float talkWidth = 2.5f, talkMinBandwidth = 100.0f;
    }

    void Wobble::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto sr = (float) sampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * sr));
        cutoffCoef = 1.0f - std::exp (-1.0f / (cutoffSeconds * sr));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * sr));
        gainCoef = 1.0f - std::exp (-1.0f / (gainSeconds * sr));
        reset();
    }

    void Wobble::reset() noexcept
    {
        resting = true;
        amountNow = 0.0f;
        ownBeats = 0.0;
        logCutoff = std::log (openHz);
        vowelNow = 0.0f;
        sides = {};
        powerIn = powerOut = 0.0f;
        gain = 1.0f;
        settledSamples = 0;
        countdown = 0;
        amountShown.store (0.0f);
        openShown.store (0.0f);
        beatShown.store (0.0f);
        cutoffShown.store (openHz);
        gainShown.store (0.0f);
    }

    void Wobble::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        if (resting)
        {
            ownBeats += beatsPerSample * n;
            if (target <= 0.0f)
                return;   // untouched, bit for bit
            reset();
            resting = false;
            ownBeats = 0.0;   // its own count starts at the pattern's start
        }
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;

        const auto sr = (float) sampleRate;
        const auto resonance = juce::jlimit (0.0f, 1.0f, s.resonance);
        const auto talk = juce::jlimit (0.0f, 1.0f, s.talk);
        const auto driveSetting = juce::jlimit (0.0f, 1.0f, s.drive);
        const auto logOpen = std::log (openHz);
        const auto glide = 1.0f - std::pow (1.0f - cutoffCoef, 16.0f);
        auto open = 0.0;

        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountCoef;
            if (target <= 0.0f && amountNow < 1.0e-5f)
                amountNow = 0.0f;

            if (--countdown <= 0)
            {
                // Every 16 samples: how open the wobble is, the cutoff and vowel it wants, and the filters' coefficients.
                countdown = 16;
                open = openAt (start + beatsPerSample * i, s.pattern, s.shape);
                const auto wantLog = logOpen + amountNow * (std::log (wobbleHz (open, s)) - logOpen);
                logCutoff += (wantLog - logCutoff) * glide;
                const auto hz = juce::jlimit (20.0f, 0.45f * sr, std::exp (logCutoff));
                g = std::tan (juce::MathConstants<float>::pi * hz / sr);
                k = 2.0f - 1.9f * resonance * amountNow;
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                b1 = 1.0f / (1.0f + g * (g + k2));
                b2 = g * b1;
                b3 = g * b2;

                talkMix = talk * amountNow;
                if (talkMix > 0.0f)
                {
                    vowelNow += (vowelAt (open) - vowelNow) * glide;
                    const auto formants = synth::vocal::formantsFor (vowelNow, 0.0f);
                    for (size_t f = 0; f < (size_t) numFormants; ++f)
                    {
                        const auto fHz = juce::jlimit (40.0f, 0.45f * sr, formants[f].hz);
                        const auto fg = std::tan (juce::MathConstants<float>::pi * fHz / sr);
                        fk[f] = juce::jlimit (0.02f, 2.0f, juce::jmax (talkMinBandwidth, talkWidth * formants[f].bandwidthHz) / fHz);
                        fa1[f] = 1.0f / (1.0f + fg * (fg + fk[f]));
                        fa2[f] = fg * fa1[f];
                        fa3[f] = fg * fa2[f];
                        fGain[f] = juce::Decibels::decibelsToGain (formants[f].db) * talkMakeUp;
                    }
                }
                drive = 1.0f + 7.0f * driveSetting * amountNow;
                driveScale = driveRef / std::tanh (driveRef * drive);
                wet = juce::jmin (1.0f, 3.0f * amountNow);
                cutoffShown.store (hz, std::memory_order_relaxed);
            }

            float* io[] = { &left[i], right != nullptr ? &right[i] : nullptr };
            auto inPower = 0.0f, outPower = 0.0f;
            for (int c = 0; c < 2; ++c)
            {
                if (io[c] == nullptr)
                    continue;
                auto& st = sides[(size_t) c];
                const auto dry = *io[c];
                // DRIVE, then TALK's vowel blended in.
                auto x = drive > 1.0f ? std::tanh (drive * dry) * driveScale : dry;
                if (talkMix > 0.0f)
                {
                    auto vowel = 0.0f;
                    for (size_t f = 0; f < (size_t) numFormants; ++f)
                    {
                        const auto v3 = x - st.f2[f];
                        const auto v1 = fa1[f] * st.f1[f] + fa2[f] * v3;
                        const auto v2 = st.f2[f] + fa2[f] * st.f1[f] + fa3[f] * v3;
                        st.f1[f] = 2.0f * v1 - st.f1[f];
                        st.f2[f] = 2.0f * v2 - st.f2[f];
                        vowel += fGain[f] * fk[f] * v1;
                    }
                    x += (vowel - x) * talkMix;
                }
                // The resonant stage, a little saturated going in, into a plain stage: 24 dB.
                const auto v0 = std::tanh (driveIn * x) / driveIn;
                const auto v3 = v0 - st.s2;
                const auto v1 = a1 * st.s1 + a2 * v3;
                const auto v2 = st.s2 + a2 * st.s1 + a3 * v3;
                st.s1 = 2.0f * v1 - st.s1;
                st.s2 = 2.0f * v2 - st.s2;
                const auto w3 = v2 - st.t2;
                const auto w1 = b1 * st.t1 + b2 * w3;
                const auto w2 = st.t2 + b2 * st.t1 + b3 * w3;
                st.t1 = 2.0f * w1 - st.t1;
                st.t2 = 2.0f * w2 - st.t2;
                const auto out = dry + (w2 - dry) * wet;
                inPower += dry * dry;
                outPower += out * out;
                *io[c] = out * gain;
            }

            // The level kept.
            powerIn += (inPower - powerIn) * levelCoef;
            powerOut += (outPower - powerOut) * levelCoef;
            if (powerIn > 1.0e-9f && powerOut > 1.0e-12f)
            {
                const auto want = juce::jlimit (0.35f, 2.8f, std::sqrt (powerIn / powerOut));
                gain += ((amountNow > 0.0f ? want : 1.0f) - gain) * gainCoef;
            }
            else if (amountNow <= 0.0f)
                gain += (1.0f - gain) * gainCoef;
        }

        const auto end = start + beatsPerSample * n;
        amountShown.store (amountNow, std::memory_order_relaxed);
        openShown.store ((float) open, std::memory_order_relaxed);
        beatShown.store ((float) (end - patternBeats * std::floor (end / patternBeats)), std::memory_order_relaxed);
        gainShown.store (juce::Decibels::gainToDecibels (gain), std::memory_order_relaxed);

        const auto settled = target <= 0.0f && amountNow <= 0.0f && std::abs (gain - 1.0f) < 1.0e-3f;
        settledSamples = settled ? settledSamples + n : 0;
        if (settled && settledSamples > (int) (0.3 * sampleRate))
        {
            const auto beats = ownBeats;
            reset();
            ownBeats = beats;
        }
    }
}
