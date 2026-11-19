#include "TranceGate.h"

namespace effects
{
    const std::array<TranceGate::Pattern, 8>& TranceGate::patterns()
    {
        static const std::array<Pattern, 8> list {{
            { "CLASSIC",   {{ 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 }}, {{ 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 }}, false },
            { "SIXTEENTHS",{{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }}, {{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }}, false },
            { "EIGHTHS",   {{ 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 }}, {{ 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 }}, false },
            { "OFFBEAT",   {{ 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1 }}, {{ 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1 }}, false },
            { "STUTTER",   {{ 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1 }}, {{ 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1 }}, false },
            { "ACCENTS",   {{ 1, 0.35f, 0.7f, 0.35f, 1, 0.35f, 0.7f, 0.35f, 1, 0.35f, 0.7f, 0.35f, 1, 0.5f, 0.8f, 1 }},
                           {{ 1, 0.35f, 0.7f, 0.35f, 1, 0.35f, 0.7f, 0.35f, 1, 0.35f, 0.7f, 0.35f, 1, 0.5f, 0.8f, 1 }}, false },
            { "BROKEN",    {{ 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0 }}, {{ 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0 }}, false },
            { "PING-PONG", {{ 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 }}, {{ 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 }}, true },
        }};
        return list;
    }

    float TranceGate::defaultStep (int side, int step)
    {
        const auto& p = patterns()[0];
        return (side == 0 ? p.left : p.right)[(size_t) (((step % 16) + 16) % 16)];
    }

    TranceGate::Place TranceGate::placeAt (double steps, int length, float swing) noexcept
    {
        // In pairs of steps: the second starts up to half a step late (SWING), so the first is that much longer.
        const auto pair = std::floor (steps * 0.5);
        const auto r = steps - 2.0 * pair;                   // 0 to 2
        const auto split = 1.0 + 0.5 * (double) juce::jlimit (0.0f, 1.0f, swing);
        const auto second = r >= split;
        Place place;
        const auto index = (std::int64_t) pair * 2 + (second ? 1 : 0);
        const auto n = (std::int64_t) juce::jlimit (1, maxSteps, length);
        place.step = (int) (((index % n) + n) % n);
        place.within = second ? (r - split) / (2.0 - split) : r / split;
        return place;
    }

    float TranceGate::targetAt (const Settings& s, int side, Place place) noexcept
    {
        const auto& row = (side == 1 && s.stereo == 1) ? s.right : s.left;
        const auto level = juce::jlimit (0.0f, 1.0f, row[(size_t) juce::jlimit (0, maxSteps - 1, place.step)]);
        return place.within < (double) juce::jlimit (0.05f, 1.0f, s.gate) ? level : 0.0f;
    }

    void TranceGate::prepare (double rate, int)
    {
        sampleRate = rate;
        mixCoef = 1.0f - std::exp (-1.0f / (0.03f * (float) rate));
        reset();
    }

    void TranceGate::reset() noexcept
    {
        resting = true;
        mixNow = 0.0f;
        ownBeats = 0.0;
        envelope = { 1.0f, 1.0f };
        stepShown.store (-1, std::memory_order_relaxed);
        for (auto& l : levelShown)
            l.store (1.0f, std::memory_order_relaxed);
        mixShown.store (0.0f, std::memory_order_relaxed);
    }

    void TranceGate::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto numSamples = (int) block.getNumSamples();
        const auto numChannels = juce::jmin (2, (int) block.getNumChannels());
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        const auto startBeats = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats = startBeats + beatsPerSample * numSamples;   // carries on from the song when it stops
        const auto target = s.on ? juce::jlimit (0.0f, 1.0f, s.mix) : 0.0f;
        const auto mixTarget = target < 0.001f ? 0.0f : target;
        if (resting && mixTarget == 0.0f)
        {
            mixShown.store (0.0f, std::memory_order_relaxed);
            return;   // untouched
        }
        resting = false;
        const auto stepBeats = beatsPerStep (s.division);
        const auto attack = 1.0f - std::exp (-1.0f / (attackMs (s.attack) * 0.001f * (float) sampleRate));
        const auto release = 1.0f - std::exp (-1.0f / (releaseMs (s.release) * 0.001f * (float) sampleRate));
        Place place;
        for (int i = 0; i < numSamples; ++i)
        {
            mixNow += (mixTarget - mixNow) * mixCoef;
            if (std::abs (mixTarget - mixNow) < 1.0e-4f)
                mixNow = mixTarget;
            place = placeAt ((startBeats + beatsPerSample * i) / stepBeats, s.length, s.swing);
            for (int side = 0; side < 2; ++side)
            {
                const auto t = targetAt (s, side, place);
                auto& e = envelope[(size_t) side];
                e += (t - e) * (t > e ? attack : release);
            }
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const auto e = envelope[(size_t) (numChannels == 1 ? 0 : channel)];
                block.getChannelPointer ((size_t) channel)[i] *= 1.0f - mixNow + mixNow * e;
            }
        }
        if (mixNow == 0.0f && mixTarget == 0.0f)
        {
            resting = true;
            envelope = { 1.0f, 1.0f };
            stepShown.store (-1, std::memory_order_relaxed);
        }
        else
            stepShown.store (place.step, std::memory_order_relaxed);
        for (int side = 0; side < 2; ++side)
            levelShown[(size_t) side].store (envelope[(size_t) side], std::memory_order_relaxed);
        mixShown.store (mixNow, std::memory_order_relaxed);
    }
}
