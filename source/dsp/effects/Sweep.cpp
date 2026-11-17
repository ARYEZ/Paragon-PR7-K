#include "Sweep.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;   // AMOUNT's smoothing
        constexpr float cutoffSeconds = 0.004f;  // the cutoff's glide (UP's restart eases down over ~15 ms)
        constexpr float levelSeconds = 0.3f;     // the level kept: power followed over this
        constexpr float gainSeconds = 0.1f;
        constexpr float openHz = 20000.0f;
        constexpr float driveIn = 1.5f;          // the resonant stage's light saturation
        constexpr float phaserFeedback = 0.3f;
    }

    void Sweep::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto sr = (float) sampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * sr));
        cutoffCoef = 1.0f - std::exp (-1.0f / (cutoffSeconds * sr));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * sr));
        gainCoef = 1.0f - std::exp (-1.0f / (gainSeconds * sr));
        reset();
    }

    void Sweep::reset() noexcept
    {
        resting = true;
        amountNow = 0.0f;
        ownBeats = 0.0;
        logCutoff = std::log (openHz);
        sides = {};
        powerIn = powerOut = 0.0f;
        gain = 1.0f;
        settledSamples = 0;
        countdown = 0;
        amountShown.store (0.0f);
        positionShown.store (0.0f);
        cutoffShown.store (openHz);
        gainShown.store (0.0f);
    }

    void Sweep::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
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
            ownBeats = 0.0;   // its own count starts at the bottom
        }
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;

        const auto sr = (float) sampleRate;
        const auto bandPass = s.filter == 1;
        const auto resonance = juce::jlimit (0.0f, 1.0f, s.resonance);
        const auto phaser = juce::jlimit (0.0f, 1.0f, s.phaser);
        const auto logOpen = std::log (openHz);
        auto position = 0.0;

        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountCoef;
            if (target <= 0.0f && amountNow < 1.0e-5f)
                amountNow = 0.0f;

            if (--countdown <= 0)
            {
                // Every 16 samples: where the sweep is, the cutoff it wants, and the filters' coefficients.
                countdown = 16;
                position = positionAt (start + beatsPerSample * i, s.shape, s.length);
                const auto wantLog = logOpen + amountNow * (std::log (sweepHz (position, s)) - logOpen);
                logCutoff += (wantLog - logCutoff) * (1.0f - std::pow (1.0f - cutoffCoef, 16.0f));
                const auto hz = juce::jlimit (20.0f, 0.45f * sr, std::exp (logCutoff));
                g = std::tan (juce::MathConstants<float>::pi * hz / sr);
                res = resonance * (0.3f + 0.7f * (float) position) * amountNow;
                k = 2.0f - 1.9f * res;
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                b1 = 1.0f / (1.0f + g * (g + k2));
                b2 = g * b1;
                b3 = g * b2;
                for (int side = 0; side < 2; ++side)
                {
                    const auto t = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, 0.45f * sr, 0.7f * hz * (side == 0 ? 1.0f : 1.15f)) / sr);
                    apCoef[(size_t) side] = (t - 1.0f) / (t + 1.0f);
                }
                phaserMix = phaser * amountNow;
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
                // The resonant stage, a little saturated going in.
                const auto v0 = std::tanh (driveIn * dry) / driveIn;
                const auto v3 = v0 - st.s2;
                const auto v1 = a1 * st.s1 + a2 * v3;
                const auto v2 = st.s2 + a2 * st.s1 + a3 * v3;
                st.s1 = 2.0f * v1 - st.s1;
                st.s2 = 2.0f * v2 - st.s2;
                float filtered;
                if (bandPass)
                    filtered = k * v1;
                else
                {
                    // Into a plain stage: 24 dB.
                    const auto w3 = v2 - st.t2;
                    const auto w1 = b1 * st.t1 + b2 * w3;
                    const auto w2 = st.t2 + b2 * st.t1 + b3 * w3;
                    st.t1 = 2.0f * w1 - st.t1;
                    st.t2 = 2.0f * w2 - st.t2;
                    filtered = w2;
                }
                // The phaser riding the sweep.
                auto y = filtered + phaserFeedback * st.last;
                const auto ca = apCoef[(size_t) c];
                for (auto& z : st.ap)
                {
                    const auto out = ca * y + z;
                    z = y - ca * out;
                    y = out;
                }
                st.last = y;
                const auto processed = filtered * (1.0f - 0.5f * phaserMix) + y * 0.5f * phaserMix;
                const auto out = dry + (processed - dry) * wet;
                inPower += dry * dry;
                outPower += out * out;
                *io[c] = out * gain;
            }

            // The level kept.
            powerIn += (inPower - powerIn) * levelCoef;
            powerOut += (outPower - powerOut) * levelCoef;
            if (powerIn > 1.0e-9f && powerOut > 1.0e-12f)
            {
                const auto want = juce::jlimit (0.5f, 2.0f, std::sqrt (powerIn / powerOut));
                gain += ((amountNow > 0.0f ? want : 1.0f) - gain) * gainCoef;
            }
            else if (amountNow <= 0.0f)
                gain += (1.0f - gain) * gainCoef;
        }

        amountShown.store (amountNow, std::memory_order_relaxed);
        positionShown.store ((float) positionAt (start + beatsPerSample * n, s.shape, s.length), std::memory_order_relaxed);
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
