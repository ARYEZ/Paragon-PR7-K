#include "BlackHole.h"

#include <cmath>

namespace effects
{
    void BlackHole::prepare (double rate, int)
    {
        sampleRate = rate;
        const auto size = (size_t) std::ceil (4.0 * rate) + 8;   // SIZE's 3 s and a grain behind it
        for (auto& l : lines)
            l.assign (size, 0.0f);
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));   // 30 ms
        reset();
    }

    void BlackHole::reset() noexcept
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        for (auto& side : grains)
            for (auto& g : side)
                g = {};
        writeIndex = 0;
        toneState = {};
        haze = {};
        spawnCountdown = 0;
        mixNow = 0.0f;
        hazePower = 0.0f;
    }

    float BlackHole::readLine (int side, double pos) const noexcept
    {
        const auto& line = lines[(size_t) side];
        const auto size = (int) line.size();
        const auto a = (int) pos;
        const auto f = (float) (pos - a);
        const auto i0 = ((a % size) + size) % size, i1 = (i0 + 1) % size;
        return line[(size_t) i0] + f * (line[(size_t) i1] - line[(size_t) i0]);
    }

    void BlackHole::startGrain (int side, Grain& g, float sizeSamples) noexcept
    {
        g.length = (int) (sampleRate * (0.16 + 0.08 * random01()));
        g.place = random01();
        const auto lag = (double) g.length + (double) g.place * (double) sizeSamples;   // never ahead of what's written
        const auto size = (double) lines[(size_t) side].size();
        g.readPos = std::fmod ((double) writeIndex - lag + size, size);
        g.age = 0;
        g.active = true;
    }

    void BlackHole::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto mixTarget = s.on ? juce::jlimit (0.0f, 1.0f, s.mix) : 0.0f;
        const auto n = (int) block.getNumSamples();
        if (mixTarget <= 0.0f && isResting())
        {
            hazeShown.store (-100.0f, std::memory_order_relaxed);
            for (int g = 0; g < grainsPerSide; ++g)
                progressShown[(size_t) g].store (-1.0f, std::memory_order_relaxed);
            return;
        }

        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        const auto size = (int) lines[0].size();
        const auto sizeSamples = (float) (sizeSeconds (s.size) * sampleRate);
        const auto rate = std::exp2 (pullSemitones (s.pull) / 12.0f);
        // Each pass round the loop is about a grain plus half of SIZE: HOLD's fall over that many passes.
        const auto loopSeconds = 0.2f + 0.5f * sizeSeconds (s.size);
        const auto feedback = std::pow (10.0f, -3.0f * loopSeconds / holdSeconds (s.hold));
        const auto tone = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * toneHz (s.tone) / sampleRate));
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        const auto hop = (int) (0.2 * sampleRate / 4.0);   // about four grains overlapping on each side
        constexpr float grainGain = 0.8165f;             // 1 / sqrt (4 x 0.375): four raised-cosine slices adding by power
        auto power = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            mixNow += (mixTarget - mixNow) * smoothing;
            if (mixTarget <= 0.0f && mixNow < 1.0e-5f)
                mixNow = 0.0f;

            // A new slice on each side now and then, each side its own (WIDTH blends the sides afterwards).
            if (--spawnCountdown <= 0)
            {
                spawnCountdown = hop;
                for (int side = 0; side < 2; ++side)
                    for (auto& g : grains[(size_t) side])
                        if (! g.active)
                        {
                            startGrain (side, g, sizeSamples);
                            break;
                        }
            }

            // The haze: every slice sounding, each faded in and out.
            std::array<float, 2> raw {};
            for (int side = 0; side < 2; ++side)
                for (auto& g : grains[(size_t) side])
                    if (g.active)
                    {
                        const auto t = (float) g.age / (float) g.length;
                        const auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);
                        raw[(size_t) side] += w * readLine (side, g.readPos);
                        g.readPos += rate;
                        if (g.readPos >= (double) size)
                            g.readPos -= (double) size;
                        if (++g.age >= g.length)
                            g.active = false;
                    }
            for (int side = 0; side < 2; ++side)
            {
                toneState[(size_t) side] += tone * (grainGain * raw[(size_t) side] - toneState[(size_t) side]);
                haze[(size_t) side] = toneState[(size_t) side];
            }
            // WIDTH: each side its own slices at 1, both the same (their middle) at 0.
            const auto mid = 0.5f * (haze[0] + haze[1]);
            const float out[2] = { mid + width * (haze[0] - mid), mid + width * (haze[1] - mid) };

            // What it hears: the sound and the haze fed back, softly limited so it can't run away.
            const float in[2] = { left[i], right[i] };
            for (int side = 0; side < 2; ++side)
                lines[(size_t) side][(size_t) writeIndex] = in[side] + std::tanh (feedback * haze[(size_t) side]);
            writeIndex = writeIndex + 1 == size ? 0 : writeIndex + 1;

            left[i] = in[0] + mixNow * out[0];
            if (stereo)
                right[i] = in[1] + mixNow * out[1];
            power += out[0] * out[0] + out[1] * out[1];
        }

        hazePower = power / (float) juce::jmax (1, 2 * n);
        if (mixTarget <= 0.0f && mixNow <= 0.0f)
        {
            // Turned down: nothing kept, so a haze starts from what's played after it's turned up again.
            for (auto& l : lines)
                std::fill (l.begin(), l.end(), 0.0f);
            for (auto& side : grains)
                for (auto& g : side)
                    g.active = false;
            toneState = {};
            haze = {};
            hazePower = 0.0f;
        }
        hazeShown.store (hazePower > 1.0e-12f ? 10.0f * std::log10 (hazePower) : -100.0f, std::memory_order_relaxed);
        for (int g = 0; g < grainsPerSide; ++g)
        {
            const auto& grain = grains[0][(size_t) g];
            placeShown[(size_t) g].store (grain.place, std::memory_order_relaxed);
            progressShown[(size_t) g].store (grain.active ? (float) grain.age / (float) juce::jmax (1, grain.length) : -1.0f, std::memory_order_relaxed);
        }
    }
}
