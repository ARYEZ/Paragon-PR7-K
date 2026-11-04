#include "dsp/effects/LightSpeed.h"

namespace effects
{
    namespace
    {
        constexpr std::array<double, LightSpeed::streaks> streakDelayMs { 12.0, 27.0, 45.0 };
        constexpr float whooshQ = 1.5f, whooshMakeUp = 2.0f, duckAtFull = 0.33f;
    }

    void LightSpeed::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        line.assign ((size_t) juce::nextPowerOfTwo ((int) (0.5 * sampleRate)), 0.0f);
        window = 0.06 * sampleRate;
        jumpRate = 1.0f - std::exp (-1.0f / (0.03f * (float) sampleRate));
        spoolRate = 1.0f - std::exp (-1.0f / (0.25f * (float) sampleRate));
        followUp = 1.0f - std::exp (-1.0f / (0.01f * (float) sampleRate));
        followDown = 1.0f - std::exp (-1.0f / (0.3f * (float) sampleRate));
        reset();
    }

    void LightSpeed::reset() noexcept
    {
        std::fill (line.begin(), line.end(), 0.0f);
        write = 0;
        for (auto& s : shifters)
            s.phase = 0.0;
        toneState.fill (0.0f);
        bandLow.fill (0.0f);
        bandBand.fill (0.0f);
        jumpNow = spool = follower = 0.0f;
        spoolShown = 0.0f;
        pitchShown = 0.0f;
        whooshShown = -100.0f;
    }

    float LightSpeed::readLine (double delaySamples) const noexcept
    {
        // `delaySamples` behind the sample last written; four-point Hermite.
        const auto mask = (int) line.size() - 1;
        const auto at = (double) (write - 1) - delaySamples;
        const auto base = (int) std::floor (at);
        const auto t = (float) (at - base);
        const auto y0 = line[(size_t) ((base - 1) & mask)], y1 = line[(size_t) (base & mask)];
        const auto y2 = line[(size_t) ((base + 1) & mask)], y3 = line[(size_t) ((base + 2) & mask)];
        const auto c1 = 0.5f * (y2 - y0);
        const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void LightSpeed::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto target = s.on && s.jump >= 0.001f ? juce::jlimit (0.0f, 1.0f, s.jump) : 0.0f;
        if (target <= 0.0f && isResting())
            return;   // untouched

        const auto n = (int) block.getNumSamples();
        const auto channels = (int) juce::jmin<size_t> (2, block.getNumChannels());
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto mask = (int) line.size() - 1;
        const auto riseTop = riseSemitones (s.rise);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        const auto streakLevel = juce::jlimit (0.0f, 1.0f, s.streak);
        const auto whooshLevel = juce::jlimit (0.0f, 1.0f, s.whoosh);
        const auto pi = juce::MathConstants<double>::pi;
        float g = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        double whooshPower = 0.0;

        for (int i = 0; i < n; ++i)
        {
            jumpNow += (target - jumpNow) * jumpRate;
            spool += (target - spool) * spoolRate;

            const auto inL = left[i], inR = right != nullptr ? right[i] : inL;
            const auto mid = 0.5f * (inL + inR);
            line[(size_t) write] = mid;
            write = (write + 1) & mask;

            // The streaks.
            auto sumL = 0.0f, sumR = 0.0f;
            const auto toneCoefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                                          * juce::jmin (0.45f * (float) sampleRate, toneHz (s.tone) * (1.0f + jumpNow))
                                                          / (float) sampleRate);
            for (int k = 0; k < streaks; ++k)
            {
                auto& shifter = shifters[(size_t) k];
                const auto ratio = std::exp2 ((double) riseTop * spool * (k + 1) / (3.0 * 12.0));
                shifter.phase += (ratio - 1.0) / window;
                shifter.phase -= std::floor (shifter.phase);
                const auto phaseB = shifter.phase + 0.5 - (shifter.phase >= 0.5 ? 1.0 : 0.0);
                const auto base = streakDelayMs[(size_t) k] * 0.001 * sampleRate;
                const auto wA = (float) std::pow (std::sin (pi * shifter.phase), 2.0);
                const auto y = wA * readLine (base + (1.0 - shifter.phase) * window)
                             + (1.0f - wA) * readLine (base + (1.0 - phaseB) * window);
                auto& toned = toneState[(size_t) k];
                toned += (y - toned) * toneCoefficient;
                const auto gain = streakLevel * (1.0f - 0.2f * (float) k) * jumpNow * 1.2f;
                const auto pan = (k % 2 == 0 ? -1.0f : 1.0f) * width * (0.3f + 0.7f * jumpNow);
                sumL += gain * toned * std::sqrt (1.0f - pan);
                sumR += gain * toned * std::sqrt (1.0f + pan);
            }

            // The whoosh: a band sweeping up with the jump, as loud as the music lets it.
            const auto level = std::abs (mid);
            follower += (level - follower) * (level > follower ? followUp : followDown);
            if ((i & 31) == 0)
            {
                g = (float) std::tan (pi * juce::jmin (0.45 * sampleRate, (double) whooshHz (jumpNow)) / sampleRate);
                a1 = 1.0f / (1.0f + g * (g + 1.0f / whooshQ));
                a2 = g * a1;
                a3 = g * a2;
            }
            const auto whooshGain = whooshLevel * std::pow (jumpNow, 1.5f) * juce::jmin (1.0f, follower / 0.1f) * whooshMakeUp;
            std::array<float, 2> whoosh {};
            for (int c = 0; c < 2; ++c)
            {
                const auto x = nextNoise();
                const auto v3 = x - bandLow[(size_t) c];
                const auto v1 = a1 * bandBand[(size_t) c] + a2 * v3;
                const auto v2 = bandLow[(size_t) c] + a2 * bandBand[(size_t) c] + a3 * v3;
                bandBand[(size_t) c] = 2.0f * v1 - bandBand[(size_t) c];
                bandLow[(size_t) c] = 2.0f * v2 - bandLow[(size_t) c];
                whoosh[(size_t) c] = whooshGain * v1;
            }
            whooshPower += 0.5 * ((double) whoosh[0] * whoosh[0] + (double) whoosh[1] * whoosh[1]);

            const auto duck = 1.0f - duckAtFull * jumpNow;
            left[i] = inL * duck + sumL + whoosh[0];
            if (right != nullptr)
                right[i] = inR * duck + sumR + whoosh[1];
        }

        spoolShown.store (spool, std::memory_order_relaxed);
        pitchShown.store (riseTop * spool, std::memory_order_relaxed);
        whooshShown.store (n > 0 ? juce::Decibels::gainToDecibels ((float) std::sqrt (whooshPower / n), -100.0f) : -100.0f,
                           std::memory_order_relaxed);

        // Back at rest: settled to nothing, emptied so the next jump starts clean.
        if (target <= 0.0f && jumpNow < 1.0e-5f && spool < 1.0e-5f)
        {
            const auto seed = noise;
            reset();
            noise = seed;
        }
    }
}
