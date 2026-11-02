#include "Afterglow.h"

#include <cmath>

namespace effects
{
    float Afterglow::Line::read (double samplesAgo) const noexcept
    {
        const auto size = (int) data.size();
        const auto whole = (int) samplesAgo;
        const auto frac = (float) (samplesAgo - whole);
        auto i = write - 1 - whole;
        while (i < 0)
            i += size;
        auto j = i - 1;
        if (j < 0)
            j += size;
        const auto a = data[(size_t) i];
        return frac <= 0.0f ? a : a + frac * (data[(size_t) j] - a);
    }

    void Afterglow::prepare (double rate, int)
    {
        sampleRate = rate;
        const auto ensembleSize = (size_t) std::ceil ((baseDelayMs + sweepMs + shimmerMs + 2.0) * 0.001 * rate) + 4;
        const auto tapeSize = (size_t) std::ceil ((2.0 * wowMs + 2.0 * flutterMs + 1.0) * 0.001 * rate) + 4;
        for (auto& l : ensembleLines)
            l.data.assign (ensembleSize, 0.0f);
        for (auto& l : tapeLines)
            l.data.assign (tapeSize, 0.0f);
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));   // 30 ms
        reset();
    }

    void Afterglow::reset() noexcept
    {
        for (auto* lines : { &ensembleLines, &tapeLines })
            for (auto& l : *lines)
            {
                std::fill (l.data.begin(), l.data.end(), 0.0f);
                l.write = 0;
            }
        toneState = {};
        sweepPhase = shimmerPhase = wowPhase = flutterPhase = 0.0;
        ensembleNow = tapeNow = 0.0f;
        powerIn = powerOut = 0.0;
        makeUp = 1.0f;
    }

    void Afterglow::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto ensembleTarget = s.on ? juce::jlimit (0.0f, 1.0f, s.ensemble) : 0.0f;
        const auto tapeTarget = s.on ? juce::jlimit (0.0f, 1.0f, s.tape) : 0.0f;
        const auto n = (int) block.getNumSamples();
        if (ensembleTarget <= 0.0f && tapeTarget <= 0.0f && isResting())
        {
            // Resting: nothing touched. The lines still hear the sound, so turning it on starts from what was playing.
            for (int c = 0; c < 2; ++c)
            {
                const auto* x = block.getChannelPointer ((size_t) juce::jmin (c, (int) block.getNumChannels() - 1));
                for (int i = 0; i < n; ++i)
                {
                    tapeLines[(size_t) c].push (x[i]);
                    ensembleLines[(size_t) c].push (x[i]);
                }
            }
            ensembleShown.store (0.0f, std::memory_order_relaxed);
            tapeShown.store (0.0f, std::memory_order_relaxed);
            wowShown.store (0.0f, std::memory_order_relaxed);
            powerIn = powerOut = 0.0;
            makeUp = 1.0f;
            return;
        }

        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        auto inSum = 0.0, outSum = 0.0;
        for (int i = 0; i < n; ++i)
            inSum += (double) left[i] * left[i] + (double) right[i] * right[i];
        const auto msToSamples = 0.001 * sampleRate;
        const auto twoPi = juce::MathConstants<double>::twoPi;
        const auto sweepStep = sweepHz / sampleRate, shimmerStep = shimmerHz / sampleRate;
        const auto wowStep = wowHz / sampleRate, flutterStep = flutterHz / sampleRate;
        for (int i = 0; i < n; ++i)
        {
            ensembleNow += (ensembleTarget - ensembleNow) * smoothing;
            tapeNow += (tapeTarget - tapeNow) * smoothing;
            if (ensembleTarget <= 0.0f && ensembleNow < 1.0e-5f)
                ensembleNow = 0.0f;
            if (tapeTarget <= 0.0f && tapeNow < 1.0e-5f)
                tapeNow = 0.0f;

            float x[2] = { left[i], right[i] };

            // TAPE: wow and flutter (a delay that sways; none at all at 0), saturation, the top softened.
            if (tapeNow > 0.0f)
            {
                const auto delayMs = (double) tapeNow * (wowMs * (1.0 + std::sin (twoPi * wowPhase)) + flutterMs * (1.0 + std::sin (twoPi * flutterPhase)));
                const auto drive = 1.0f + 2.5f * tapeNow;
                // The top: a one-pole low-pass from 20 kHz down to 7 kHz as TAPE turns up.
                const auto hz = 20000.0 * std::pow (7000.0 / 20000.0, (double) tapeNow);
                const auto tone = (float) (1.0 - std::exp (-twoPi * hz / sampleRate));
                for (int c = 0; c < 2; ++c)
                {
                    tapeLines[(size_t) c].push (x[c]);
                    const auto swayed = tapeLines[(size_t) c].read (delayMs * msToSamples);
                    const auto saturated = std::tanh (drive * swayed) / drive;
                    const auto warm = swayed + tapeNow * (saturated - swayed);
                    toneState[(size_t) c] += tone * (warm - toneState[(size_t) c]);
                    x[c] = warm + tapeNow * (toneState[(size_t) c] - warm);
                }
            }
            else
                for (int c = 0; c < 2; ++c)
                {
                    tapeLines[(size_t) c].push (x[c]);
                    toneState[(size_t) c] = x[c];
                }
            wowPhase += wowStep;
            flutterPhase += flutterStep;
            if (wowPhase >= 1.0) wowPhase -= 1.0;
            if (flutterPhase >= 1.0) flutterPhase -= 1.0;

            // ENSEMBLE: three voices a third of a cycle apart, the right side a sixth on from the left.
            for (int c = 0; c < 2; ++c)
                ensembleLines[(size_t) c].push (x[c]);
            if (ensembleNow > 0.0f)
                for (int c = 0; c < 2; ++c)
                {
                    auto wet = 0.0f;
                    for (int v = 0; v < 3; ++v)
                    {
                        const auto offset = v / 3.0 + (c == 1 ? 1.0 / 6.0 : 0.0);
                        const auto d = baseDelayMs + sweepMs * std::sin (twoPi * (sweepPhase + offset))
                                     + shimmerMs * std::sin (twoPi * (shimmerPhase + offset));
                        wet += ensembleLines[(size_t) c].read (d * msToSamples);
                    }
                    wet *= 0.57735027f;   // 1 / sqrt 3: the three voices sway apart, so they add by power, not in step
                    x[c] = x[c] * (1.0f - 0.65f * ensembleNow) + wet * (0.95f * ensembleNow);   // at full mostly the chorus: wide
                }
            sweepPhase += sweepStep;
            shimmerPhase += shimmerStep;
            if (sweepPhase >= 1.0) sweepPhase -= 1.0;
            if (shimmerPhase >= 1.0) shimmerPhase -= 1.0;

            outSum += (double) x[0] * x[0] + (double) x[1] * x[1];
            left[i] = x[0];
            if (stereo)
                right[i] = x[1];
        }

        // The make-up: toward what brings the output's power back to the input's, glided across the block.
        {
            const auto k = 1.0 - std::exp (-(double) n / (1.0 * sampleRate));
            powerIn += (inSum / juce::jmax (1, n) - powerIn) * k;
            powerOut += (outSum / juce::jmax (1, n) - powerOut) * k;
            auto target = powerIn > 1.0e-9 && powerOut > 1.0e-9 ? (float) std::sqrt (powerIn / powerOut) : makeUp;
            target = juce::jlimit (0.7079f, 1.4125f, target);   // at most 3 dB either way
            const auto step = (target - makeUp) / (float) juce::jmax (1, n);
            for (int i = 0; i < n; ++i)
            {
                makeUp += step;
                left[i] *= makeUp;
                if (stereo)
                    right[i] *= makeUp;
            }
            makeUp = target;
        }

        for (int v = 0; v < 3; ++v)
            voiceShown[(size_t) v].store ((float) (baseDelayMs + sweepMs * std::sin (twoPi * (sweepPhase + v / 3.0))
                                                   + shimmerMs * std::sin (twoPi * (shimmerPhase + v / 3.0))), std::memory_order_relaxed);
        // The wow's pitch: the delay's rate of change, as cents.
        const auto rate = (double) tapeNow * (wowMs * twoPi * wowHz * std::cos (twoPi * wowPhase)) * 0.001;
        wowShown.store ((float) (-1200.0 * std::log2 (1.0 + rate)), std::memory_order_relaxed);
        ensembleShown.store (ensembleNow, std::memory_order_relaxed);
        tapeShown.store (tapeNow, std::memory_order_relaxed);
    }
}
