#include "LightTrail.h"

#include <cmath>

namespace effects
{
    void LightTrail::prepare (double rate, int)
    {
        sampleRate = rate;
        // Room for a quarter note at 30 BPM.
        const auto size = (size_t) std::ceil (2.0 * rate) + 8;
        for (auto& l : lines)
            l.assign (size, 0.0f);
        for (auto& s : shifters)
            s.prepare (rate);
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));   // 30 ms
        reset();
    }

    void LightTrail::reset() noexcept
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        for (auto& s : shifters)
            s.reset();
        writeIndex = 0;
        toneState = {};
        mixNow = 0.0f;
        delayNow = -1.0f;
        trailPower = 0.0f;
    }

    void LightTrail::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm) noexcept
    {
        const auto mixTarget = s.on ? juce::jlimit (0.0f, 1.0f, s.mix) : 0.0f;
        const auto n = (int) block.getNumSamples();
        if (mixTarget <= 0.0f && isResting())
        {
            // Resting: nothing added, nothing kept (a trail starts from what's played after it's turned up).
            trailShown.store (-100.0f, std::memory_order_relaxed);
            return;
        }

        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        const auto size = (int) lines[0].size();

        const auto beats = divisionBeats[(size_t) juce::jlimit (0, (int) divisionBeats.size() - 1, s.division)];
        const auto wanted = (float) juce::jlimit (4.0, (double) size - 4.0, beats * 60.0 / juce::jmax (20.0, bpm) * sampleRate);
        if (delayNow < 0.0f)
            delayNow = wanted;
        const auto feedback = feedbackFor (s.length);
        const auto ratio = std::exp2 (juce::jlimit (-12.0f, 12.0f, s.riseSemitones) / 12.0f);
        const auto shifting = std::abs (s.riseSemitones) > 0.01f;
        const auto tone = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * toneHz (s.tone) / sampleRate));
        const auto cross = juce::jlimit (0.0f, 1.0f, s.spread);   // 1: each repeat wholly on the other side
        auto power = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            mixNow += (mixTarget - mixNow) * smoothing;
            if (mixTarget <= 0.0f && mixNow < 1.0e-5f)
                mixNow = 0.0f;
            delayNow += (wanted - delayNow) * 0.0005f;   // a tempo change glides rather than jumps

            // The repeat coming back now, each side.
            std::array<float, 2> back {};
            for (int c = 0; c < 2; ++c)
            {
                auto pos = (float) writeIndex - delayNow;
                while (pos < 0.0f)
                    pos += (float) size;
                const auto a = (int) pos;
                const auto f = pos - (float) a;
                const auto& line = lines[(size_t) c];
                back[(size_t) c] = line[(size_t) a] + f * (line[(size_t) ((a + 1) % size)] - line[(size_t) a]);
            }

            // Into the trail: what's playing (each side) and the repeat fed back, together stepped in pitch, toned,
            // limited and crossed over, so every repeat, the first too, is a step further along.
            const float in[2] = { left[i], right[i] };
            std::array<float, 2> pass {};
            for (int c = 0; c < 2; ++c)
            {
                auto x = in[c] + feedback * back[(size_t) c];
                if (shifting)
                    x = shifters[(size_t) c].processSample (x, ratio);
                toneState[(size_t) c] += tone * (x - toneState[(size_t) c]);
                pass[(size_t) c] = std::tanh (toneState[(size_t) c]);
            }
            for (int c = 0; c < 2; ++c)
                lines[(size_t) c][(size_t) writeIndex] = (1.0f - cross) * pass[(size_t) c] + cross * pass[(size_t) (1 - c)];
            writeIndex = writeIndex + 1 == size ? 0 : writeIndex + 1;

            // The trail added: the repeats only (the sound itself passes unchanged).
            left[i] = in[0] + mixNow * back[0];
            if (stereo)
                right[i] = in[1] + mixNow * back[1];
            power += back[0] * back[0] + back[1] * back[1];
        }

        trailPower = power / (float) juce::jmax (1, 2 * n);
        if (mixTarget <= 0.0f && mixNow <= 0.0f)
        {
            // Turned down: the trail kept no longer once it's silent.
            trailPower = 0.0f;
            for (auto& l : lines)
                std::fill (l.begin(), l.end(), 0.0f);
            toneState = {};
        }
        trailShown.store (trailPower > 1.0e-10f ? 10.0f * std::log10 (trailPower) : -100.0f, std::memory_order_relaxed);
    }
}
