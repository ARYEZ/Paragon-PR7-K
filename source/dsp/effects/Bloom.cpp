#include "dsp/effects/Bloom.h"

namespace effects
{
    namespace
    {
        // The hall's eight lines (ms): unrelated lengths so their echoes don't line up.
        constexpr std::array<double, Bloom::lines> hallMs { 37.1, 43.3, 51.7, 58.9, 66.1, 73.7, 81.2, 89.9 };
        constexpr float stringLow = 0.6f;          // the string loop's low-pass (a little damping of the top)
        constexpr float stringGroupDelay = 0.67f;  // its delay at low frequencies, in samples (taken off the loop)
        constexpr float detuneCents = 1.5f;        // left flat, right sharp: two strings a note
        // Measured (BloomTests): the strings' and the hall's levels against a held chord's.
        constexpr float excitation = 2.0f, resonanceMakeUp = 1.6f, hallIn = 0.35f, hallMakeUp = 0.55f;
    }

    void Bloom::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto stringSize = juce::nextPowerOfTwo ((int) std::ceil (sampleRate / 25.0) + 8);
        for (auto& s : strings)
            for (auto& line : s.line)
                line.assign ((size_t) stringSize, 0.0f);
        for (int i = 0; i < lines; ++i)
        {
            hallLength[(size_t) i] = juce::jmax (8, (int) std::round (hallMs[(size_t) i] * 0.001 * sampleRate));
            hall[(size_t) i].assign ((size_t) juce::nextPowerOfTwo (hallLength[(size_t) i] + 2), 0.0f);
        }
        const auto sr = (float) sampleRate;
        amountRate = 1.0f - std::exp (-1.0f / (0.03f * sr));
        followUp = 1.0f - std::exp (-1.0f / (0.01f * sr));
        followDown = 1.0f - std::exp (-1.0f / (0.3f * sr));
        closeRate = 1.0f - std::exp (-1.0f / (2.0f * sr));
        energyRate = 1.0f - std::exp (-1.0f / (0.05f * sr));
        reset();
    }

    void Bloom::reset() noexcept
    {
        for (auto& s : strings)
        {
            s.pitch = -1;
            for (auto& line : s.line)
                std::fill (line.begin(), line.end(), 0.0f);
            s.write = {};
            s.low = {};
            s.energy = 0.0f;
            s.wanted = false;
        }
        for (int i = 0; i < lines; ++i)
            std::fill (hall[(size_t) i].begin(), hall[(size_t) i].end(), 0.0f);
        hallWrite = {};
        hallLow = {};
        amountNow = swellNow = follower = 0.0f;
        ringNow = -1.0f;
        for (int k = 0; k < numStrings; ++k)
        {
            stringPitchShown[(size_t) k].store (-1.0f, std::memory_order_relaxed);
            stringLevelShown[(size_t) k].store (0.0f, std::memory_order_relaxed);
        }
        hallShown.store (-100.0f, std::memory_order_relaxed);
        swellShown.store (0.0f, std::memory_order_relaxed);
        amountShown.store (0.0f, std::memory_order_relaxed);
    }

    float Bloom::ringGain (int pitch, float ringS) const noexcept
    {
        const auto hz = 440.0 * std::exp2 ((pitch - 69) / 12.0);
        return (float) std::pow (10.0, -3.0 / (hz * (double) ringS));   // 60 dB in RING
    }

    void Bloom::tune (String& s, int pitch) noexcept
    {
        s.pitch = pitch;
        for (int c = 0; c < 2; ++c)
        {
            const auto hz = 440.0 * std::exp2 ((pitch - 69 + (c == 0 ? -1.0 : 1.0) * detuneCents / 100.0) / 12.0);
            s.delay[(size_t) c] = juce::jmax (2.0, sampleRate / hz - stringGroupDelay);
            std::fill (s.line[(size_t) c].begin(), s.line[(size_t) c].end(), 0.0f);
            s.write[(size_t) c] = 0;
            s.low[(size_t) c] = 0.0f;
        }
        s.energy = 0.0f;
        s.gain = ringNow > 0.0f ? ringGain (pitch, ringNow) : 0.0f;
    }

    void Bloom::setNotes (const float* pitches, int count) noexcept
    {
        if (amountNow <= 0.0f)
            return;   // resting: nothing to tune
        for (auto& s : strings)
            s.wanted = false;
        for (int n = 0; n < juce::jmin (count, maxNotes); ++n)
        {
            const auto note = (int) std::lround (pitches[n]);
            for (const auto offset : { -12, 0, 12, 19 })
            {
                const auto pitch = note + offset;
                if (pitch < 21 || pitch > 108)
                    continue;
                String* same = nullptr;
                String* quietest = nullptr;
                for (auto& s : strings)
                {
                    if (s.pitch == pitch)
                        same = &s;
                    else if (! s.wanted && (quietest == nullptr || (s.pitch < 0 ? -1.0f : s.energy) < (quietest->pitch < 0 ? -1.0f : quietest->energy)))
                        quietest = &s;
                }
                if (same == nullptr && quietest != nullptr)
                {
                    tune (*quietest, pitch);
                    same = quietest;
                }
                if (same != nullptr)
                    same->wanted = true;
            }
        }
    }

    void Bloom::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto target = s.on && s.amount >= 0.001f ? juce::jlimit (0.0f, 1.0f, s.amount) : 0.0f;
        if (target <= 0.0f && isResting())
            return;   // untouched
        juce::ScopedNoDenormals noDenormals;

        const auto n = (int) block.getNumSamples();
        const auto channels = (int) juce::jmin<size_t> (2, block.getNumChannels());
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto sr = (float) sampleRate;

        // This block's loop gains: the strings by RING, the hall by SIZE; TONE in the hall's loop; SWELL's opening.
        const auto ringS = ringSeconds (s.ring);
        if (std::abs (ringS - ringNow) > 1.0e-4f)
        {
            ringNow = ringS;
            for (auto& str : strings)
                if (str.pitch >= 0)
                    str.gain = ringGain (str.pitch, ringNow);
        }
        const auto sizeS = sizeSeconds (s.size);
        std::array<float, lines> hallGain {};
        for (int i = 0; i < lines; ++i)
            hallGain[(size_t) i] = (float) std::pow (10.0, -3.0 * hallLength[(size_t) i] / sampleRate / sizeS);
        const auto toneCoefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (0.45f * sr, toneHz (s.tone)) / sr);
        const auto openRate = 1.0f - std::exp (-1.0f / (swellSeconds (s.swell) * sr));
        const auto resonance = juce::jlimit (0.0f, 1.0f, s.resonance) * resonanceMakeUp;
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        double hallPower = 0.0;

        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountRate;
            if (target <= 0.0f && amountNow < 1.0e-6f)
                amountNow = 0.0f;
            const auto inL = left[i], inR = right != nullptr ? right[i] : inL;
            const auto level = 0.5f * std::abs (inL + inR);
            follower += (level - follower) * (level > follower ? followUp : followDown);
            const auto open = follower > 1.0e-4f ? 1.0f : 0.0f;
            swellNow += (open - swellNow) * (open > swellNow ? openRate : closeRate);

            // The strings: each a comb one period long, excited by its side's sound, ringing after it.
            auto sL = 0.0f, sR = 0.0f;
            for (auto& str : strings)
            {
                if (str.pitch < 0)
                    continue;
                const auto exciteGain = (1.0f - str.gain) * excitation;
                for (int c = 0; c < 2; ++c)
                {
                    auto& line = str.line[(size_t) c];
                    const auto mask = (int) line.size() - 1;
                    const auto at = (double) str.write[(size_t) c] - str.delay[(size_t) c];
                    const auto base = (int) std::floor (at);
                    const auto t = (float) (at - base);
                    const auto read = line[(size_t) (base & mask)] * (1.0f - t) + line[(size_t) ((base + 1) & mask)] * t;
                    auto& low = str.low[(size_t) c];
                    low += (read - low) * stringLow;
                    const auto ring = low * str.gain;
                    line[(size_t) str.write[(size_t) c]] = ring + (c == 0 ? inL : inR) * exciteGain;
                    str.write[(size_t) c] = (str.write[(size_t) c] + 1) & mask;
                    (c == 0 ? sL : sR) += ring;
                }
                str.energy += (std::abs (str.low[0]) - str.energy) * energyRate;
            }

            // The hall, fed the sound and the strings through the swell.
            const auto feedL = (inL + sL) * swellNow * hallIn, feedR = (inR + sR) * swellNow * hallIn;
            std::array<float, lines> v {};
            auto sum = 0.0f;
            for (int k = 0; k < lines; ++k)
            {
                auto& line = hall[(size_t) k];
                const auto mask = (int) line.size() - 1;
                const auto out = line[(size_t) ((hallWrite[(size_t) k] - hallLength[(size_t) k]) & mask)];
                hallLow[(size_t) k] += (out - hallLow[(size_t) k]) * toneCoefficient;
                v[(size_t) k] = hallLow[(size_t) k] * hallGain[(size_t) k];
                sum += v[(size_t) k];
            }
            sum *= 2.0f / (float) lines;
            for (int k = 0; k < lines; ++k)
            {
                auto& line = hall[(size_t) k];
                const auto mask = (int) line.size() - 1;
                line[(size_t) hallWrite[(size_t) k]] = v[(size_t) k] - sum + (k % 2 == 0 ? feedL : feedR);
                hallWrite[(size_t) k] = (hallWrite[(size_t) k] + 1) & mask;
            }
            const auto hL = (v[0] - v[2] + v[4] - v[6]) * hallMakeUp, hR = (v[1] - v[3] + v[5] - v[7]) * hallMakeUp;
            const auto mid = 0.5f * (hL + hR), side = 0.5f * (hL - hR) * width;
            const auto wetL = mid + side, wetR = mid - side;
            hallPower += 0.5 * ((double) wetL * wetL + (double) wetR * wetR);

            left[i] = inL + amountNow * (resonance * sL + wetL);
            if (right != nullptr)
                right[i] = inR + amountNow * (resonance * sR + wetR);
        }

        // For the page.
        for (int k = 0; k < numStrings; ++k)
        {
            stringPitchShown[(size_t) k].store ((float) strings[(size_t) k].pitch, std::memory_order_relaxed);
            stringLevelShown[(size_t) k].store (juce::jlimit (0.0f, 1.0f, strings[(size_t) k].energy * 8.0f), std::memory_order_relaxed);
        }
        hallShown.store (n > 0 ? juce::Decibels::gainToDecibels ((float) std::sqrt (hallPower / n) * amountNow, -100.0f) : -100.0f,
                         std::memory_order_relaxed);
        swellShown.store (swellNow, std::memory_order_relaxed);
        amountShown.store (amountNow, std::memory_order_relaxed);

        // Back at rest: emptied, so the next bloom starts clean.
        if (target <= 0.0f && amountNow <= 0.0f)
            reset();
    }
}
