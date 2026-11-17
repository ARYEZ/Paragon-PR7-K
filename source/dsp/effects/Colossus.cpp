#include "Colossus.h"

namespace effects
{
    namespace
    {
        constexpr float sizeSeconds = 0.03f;
        constexpr float levelSeconds = 0.3f, gainLow = 0.35f, gainHigh = 1.6f;
        constexpr float weightRef = 0.3f, maxDrive = 5.0f;   // WEIGHT at full: x 6
        constexpr float underGain = 1.2f;                    // the octave a little over the sound at full DEPTH
        constexpr float hallIn = 0.35f, hallOut = 0.6f;
        constexpr std::array<double, Colossus::numLines> lineMs { 47.3, 53.9, 61.1, 67.7, 74.3, 82.9, 91.7, 101.3 };
        constexpr float boomSeconds = 0.35f, boomHigh = 72.0f, boomLow = 36.0f, boomGain = 1.1f;
        constexpr float thumpSeconds = 0.03f;
        constexpr float holdOffSeconds = 0.06f;   // notes this close make one boom (a chord, a strum)
    }

    void Colossus::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto rate = (float) sampleRate;
        sizeCoef = 1.0f - std::exp (-1.0f / (sizeSeconds * rate));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * rate));
        swellUp = 1.0f - std::exp (-1.0f / (0.25f * rate));
        swellDown = 1.0f - std::exp (-1.0f / (1.5f * rate));
        shifter.prepare (sampleRate);
        for (int k = 0; k < numLines; ++k)
        {
            lengths[(size_t) k] = juce::jmax (8, (int) std::round (lineMs[(size_t) k] * 0.001 * 1.4 * sampleRate));
            lines[(size_t) k].assign ((size_t) lengths[(size_t) k], 0.0f);
        }
        reset();
    }

    void Colossus::reset() noexcept
    {
        sizeNow = 0.0f;
        shifter.reset();
        underLow = underLow2 = 0.0f;
        for (int k = 0; k < numLines; ++k)
        {
            std::fill (lines[(size_t) k].begin(), lines[(size_t) k].end(), 0.0f);
            writes[(size_t) k] = 0;
            lineLow[(size_t) k] = 0.0f;
        }
        swell = 0.0f;
        pendingCount = 0;
        holdOff = 0;
        boomAge = 1.0e9f;
        boomLevel = 0.0f;
        thumpLow = 0.0f;
        boomPhase = 0.0;
        powerIn = powerOut = 0.0f;
        gain = 1.0f;
        quietSamples = 0;
        resting = true;
        sizeShown.store (0.0f);
        underShown.store (0.0f);
        hallShown.store (0.0f);
        boomShown.store (0.0f);
    }

    void Colossus::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.size < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.size);
        if (resting && target <= 0.0f)
        {
            pendingCount = 0;
            return;   // untouched, bit for bit
        }
        resting = false;

        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto rate = (float) sampleRate;
        const auto depth = juce::jlimit (0.0f, 1.0f, s.depth);
        const auto weight = juce::jlimit (0.0f, 1.0f, s.weight);
        const auto boomAmount = juce::jlimit (0.0f, 1.0f, s.boom);
        const auto tone = juce::jlimit (0.0f, 1.0f, s.tone);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        // The octave's top: 400 Hz dark to 2.5 kHz bright; the hall's 2.5 to 9 kHz.
        const auto underHz = 400.0f * std::pow (6.25f, tone);
        const auto underCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * underHz / rate);
        const auto hallHz = 2500.0f * std::pow (3.6f, tone);
        const auto hallCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hallHz / rate);
        // Each line's feedback for the hall's time to fall 60 dB.
        std::array<float, numLines> feedback {};
        const auto seconds = hallSeconds (s.hall);
        for (int k = 0; k < numLines; ++k)
            feedback[(size_t) k] = std::pow (10.0f, -3.0f * (float) lengths[(size_t) k] / (rate * seconds));
        const auto thumpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 180.0f / rate);
        const auto holdOffSamples = (int) (holdOffSeconds * rate);

        float underPeak = 0.0f, hallPeak = 0.0f, boomPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            sizeNow += (target - sizeNow) * sizeCoef;
            if (target <= 0.0f && sizeNow < 1.0e-5f)
                sizeNow = 0.0f;
            const auto a = sizeNow;
            const auto inL = left[i], inR = right != nullptr ? right[i] : inL;
            const auto mid = 0.5f * (inL + inR);

            // UNDER: the octave down, darkened.
            const auto shifted = shifter.processSample (mid, 0.5f);
            underLow += (shifted - underLow) * underCoef;
            underLow2 += (underLow - underLow2) * underCoef;
            const auto under = underLow2 * underGain * depth * a;
            underPeak = juce::jmax (underPeak, std::abs (under));

            // WEIGHT: driven and blended in.
            auto bigL = inL + under, bigR = inR + under;
            const auto drive = 1.0f + maxDrive * weight * a;
            if (drive > 1.0f)
            {
                const auto norm = weightRef / std::tanh (weightRef * drive);
                const auto blend = weight * a;
                bigL += (std::tanh (bigL * drive) * norm - bigL) * blend;
                bigR += (std::tanh (bigR * drive) * norm - bigR) * blend;
            }

            // HALL: fed the big sound, swelling in while there's sound.
            const auto presence = std::abs (mid) > 1.0e-4f ? 1.0f : 0.0f;
            swell += (presence - swell) * (presence > swell ? swellUp : swellDown);
            std::array<float, numLines> outs {};
            float sum = 0.0f;
            for (int k = 0; k < numLines; ++k)
            {
                outs[(size_t) k] = lines[(size_t) k][(size_t) writes[(size_t) k]];
                sum += outs[(size_t) k];
            }
            const auto householder = sum * (2.0f / (float) numLines);
            const auto feed = hallIn * a * swell;
            float wetL = 0.0f, wetR = 0.0f;
            for (int k = 0; k < numLines; ++k)
            {
                auto v = (outs[(size_t) k] - householder) * feedback[(size_t) k];
                lineLow[(size_t) k] += (v - lineLow[(size_t) k]) * hallCoef;
                v = lineLow[(size_t) k] + ((k & 1) == 0 ? bigL : bigR) * feed;
                lines[(size_t) k][(size_t) writes[(size_t) k]] = v;
                if (++writes[(size_t) k] >= lengths[(size_t) k])
                    writes[(size_t) k] = 0;
                ((k & 1) == 0 ? wetL : wetR) += outs[(size_t) k];
            }
            const auto hallMid = 0.5f * (wetL + wetR), hallSide = 0.5f * (wetL - wetR) * width;
            const auto hallL = (hallMid + hallSide) * hallOut * a, hallR = (hallMid - hallSide) * hallOut * a;
            hallPeak = juce::jmax (hallPeak, std::abs (hallMid));

            // BOOM: a note played now starts one (the hardest of a chord's notes).
            if (holdOff > 0)
                --holdOff;
            auto pressed = 0.0f;
            for (int k = 0; k < pendingCount; ++k)
                if (pending[(size_t) k].offset == i || (i == n - 1 && pending[(size_t) k].offset >= n))
                    pressed = juce::jmax (pressed, pending[(size_t) k].velocity);
            if (pressed > 0.0f && holdOff == 0 && boomAmount > 0.0f && a > 0.0f)
            {
                boomLevel = boomAmount * a * (0.35f + 0.65f * pressed) * boomGain;
                boomAge = 0.0f;
                boomPhase = 0.0;
                holdOff = holdOffSamples;
                booms.fetch_add (1, std::memory_order_relaxed);
            }
            auto boom = 0.0f;
            if (boomAge < boomSeconds * 3.0f)
            {
                const auto t = boomAge / boomSeconds;
                const auto hz = boomLow + (boomHigh - boomLow) * std::exp (-3.0f * t);
                boomPhase += hz / sampleRate;
                if (boomPhase >= 1.0)
                    boomPhase -= 1.0;
                const auto sine = (float) std::sin (juce::MathConstants<double>::twoPi * boomPhase) * std::exp (-2.3f * t);
                thumpLow += (noise() - thumpLow) * thumpCoef;
                const auto thump = thumpLow * 3.0f * std::exp (-boomAge / (thumpSeconds / 6.9f));
                boom = boomLevel * (sine + thump);
                boomAge += 1.0f / rate;
                boomPeak = juce::jmax (boomPeak, std::abs (boom));
            }

            // The level kept: what goes in against the whole of it, the hall too (the booms aside).
            const auto wholeL = bigL + hallL, wholeR = bigR + hallR;
            powerIn += (inL * inL + inR * inR - powerIn) * levelCoef;
            powerOut += (wholeL * wholeL + wholeR * wholeR - powerOut) * levelCoef;
            // (held while nothing comes in, so the hall's tail rings out at the level it had)
            const auto wanted = a <= 0.0f ? 1.0f
                              : powerIn > 1.0e-8f && powerOut > 1.0e-12f ? juce::jlimit (gainLow, gainHigh, std::sqrt (powerIn / powerOut)) : gain;
            gain += (wanted - gain) * sizeCoef;

            left[i] = wholeL * gain + boom;
            if (right != nullptr)
                right[i] = wholeR * gain + boom;
        }

        pendingCount = 0;
        sizeShown.store (sizeNow);
        underShown.store (juce::jlimit (0.0f, 1.0f, underPeak * 3.0f));
        hallShown.store (juce::jlimit (0.0f, 1.0f, hallPeak * 4.0f));
        boomShown.store (juce::jlimit (0.0f, 1.0f, boomPeak * 2.0f));

        // Back at rest once SIZE is 0 (the hall's output goes with it), the booms died away and the gain back to 1:
        // cleared, so it wakes clean.
        if (target <= 0.0f && sizeNow <= 0.0f && boomAge >= boomSeconds * 3.0f)
        {
            quietSamples = std::abs (gain - 1.0f) < 1.0e-3f ? quietSamples + n : 0;
            if (quietSamples > (int) (0.3 * sampleRate))
                reset();
        }
        else
            quietSamples = 0;
    }
}
