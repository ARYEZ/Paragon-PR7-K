#include "Dilation.h"

#include <cmath>

namespace effects
{
    void Dilation::prepare (double rate, int)
    {
        sampleRate = rate;
        const auto size = (size_t) std::ceil ((maxReachSeconds + 1.0) * rate) + 8;   // REACH's 8 s and a little
        for (auto& l : lines)
            l.assign (size, 0.0f);
        mixSmoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));   // 30 ms
        fadeStep = (float) (1.0 / (0.06 * rate));                           // 60 ms
        reset();
    }

    void Dilation::reset() noexcept
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        writeIndex = 0;
        lag = fadeLag = 0.0;
        speedLog = 0.0f;
        fadeGain = 0.0f;
        mixNow = 1.0f;
        toneState = {};
        clock = 0.0;
        speedShown.store (1.0f, std::memory_order_relaxed);
        behindShown.store (0.0f, std::memory_order_relaxed);
        catchingShown.store (false, std::memory_order_relaxed);
    }

    // `back` samples before the one just written, by four-point Hermite; points not written yet (less than two
    // samples back) repeat the newest.
    float Dilation::read (int side, double back) const noexcept
    {
        const auto& line = lines[(size_t) side];
        const auto size = (int) line.size();
        const auto whole = (int) std::ceil (back);
        const auto f = (float) ((double) whole - back);   // 0 to 1, towards the newer sample
        const auto at = [&] (int k) { return line[(size_t) ((writeIndex - juce::jmax (0, k) + size) % size)]; };
        const auto xm1 = at (whole + 1), x0 = at (whole), x1 = at (whole - 1), x2 = at (whole - 2);
        if (f == 0.0f)
            return x0;
        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    void Dilation::startFade() noexcept
    {
        fadeLag = lag;
        fadeGain = 1.0f;
    }

    void Dilation::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        const auto size = (int) lines[0].size();
        // SLOW under a thousandth counts as 0: the mod matrix eases a knob back down and never quite lands on 0, so
        // without this it would never catch up.
        const auto slow = s.slow < 1.0e-3f ? 0.0f : juce::jmin (1.0f, s.slow);
        const auto targetLog = s.on ? -2.0f * slow : 0.0f;
        const auto returning = targetLog == 0.0f;
        const auto mixTarget = s.on ? juce::jlimit (0.0f, 1.0f, s.mix) : mixNow;

        if (returning && isResting())
        {
            // Untouched; the sound still goes into the lines, so a slide starts from what was just played.
            for (int i = 0; i < n; ++i)
            {
                lines[0][(size_t) writeIndex] = left[i];
                lines[1][(size_t) writeIndex] = right[i];
                writeIndex = writeIndex + 1 == size ? 0 : writeIndex + 1;
            }
            mixNow = mixTarget;
            if (n > 0)
                toneState = { left[n - 1], right[n - 1] };   // so TONE starts from the sound, without a step
            clock += n / sampleRate;
            speedShown.store (1.0f, std::memory_order_relaxed);
            behindShown.store (0.0f, std::memory_order_relaxed);
            clockShown.store (clock, std::memory_order_relaxed);
            catchingShown.store (false, std::memory_order_relaxed);
            return;
        }

        const auto rampSamples = (double) rampSeconds (s.ramp) * sampleRate;
        const auto speedSmoothing = (float) (1.0 - std::exp (-3.0 / rampSamples));   // most of the way in RAMP
        const auto reachSamples = (double) reachSeconds (s.reach) * sampleRate;
        const auto snap = s.returnMode == 1;
        const auto tone = s.tone;
        auto catching = false;

        for (int i = 0; i < n; ++i)
        {
            const float in[2] = { left[i], right[i] };
            lines[0][(size_t) writeIndex] = in[0];
            lines[1][(size_t) writeIndex] = in[1];

            // The speed: towards SLOW's, or catching up when SLOW is back at 0.
            auto want = targetLog;
            if (returning && lag > 0.0)
            {
                if (snap)
                {
                    startFade();
                    lag = 0.0;
                    speedLog = 0.0f;
                }
                else
                {
                    // Faster than real time by the share of RAMP it's behind (up to twice as fast), never slower
                    // than 2% over, so it does arrive.
                    want = std::log2 (1.0f + (float) juce::jlimit (0.02, 1.0, lag / rampSamples));
                    catching = true;
                }
            }
            speedLog += (want - speedLog) * speedSmoothing;
            if (returning && lag <= 0.0 && std::abs (speedLog) < 1.0e-5f)
                speedLog = 0.0f;
            const auto speed = std::exp2 (speedLog);

            lag = juce::jmax (0.0, lag + 1.0 - (double) speed);
            if (fadeGain > 0.0f)
                fadeLag = juce::jmax (0.0, fadeLag + 1.0 - (double) speed);
            if (lag > reachSamples)
            {
                startFade();   // as far behind as it can fall: back to the present
                lag = 0.0;
            }

            // TONE: the top falls with the speed.
            const auto cutoff = toneHz (tone, speed);
            const auto filtered = cutoff < 19000.0f;
            const auto coefficient = filtered ? 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sampleRate) : 1.0f;
            mixNow += (mixTarget - mixNow) * mixSmoothing;

            for (int side = 0; side < 2; ++side)
            {
                auto wet = read (side, lag);
                if (fadeGain > 0.0f)
                {
                    // A straight fade: never louder than either (an equal-power one rose 3 dB on a held chord,
                    // where the two moments are alike); on unlike moments it dips for its 60 ms.
                    wet += fadeGain * (read (side, fadeLag) - wet);
                }
                auto& state = toneState[(size_t) side];
                state = filtered ? state + coefficient * (wet - state) : wet;
                const auto out = in[side] + mixNow * (state - in[side]);
                if (side == 0)
                    left[i] = out;
                else if (stereo)
                    right[i] = out;
            }
            if (fadeGain > 0.0f)
                fadeGain = juce::jmax (0.0f, fadeGain - fadeStep);

            writeIndex = writeIndex + 1 == size ? 0 : writeIndex + 1;
            clock += speed / sampleRate;
        }

        speedShown.store (std::exp2 (speedLog), std::memory_order_relaxed);
        behindShown.store ((float) (lag / sampleRate), std::memory_order_relaxed);
        clockShown.store (clock, std::memory_order_relaxed);
        catchingShown.store (catching, std::memory_order_relaxed);
    }
}
