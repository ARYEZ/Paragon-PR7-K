#include "NuclearSpring.h"

#include <cmath>

namespace effects
{
    namespace
    {
        // The springs' lengths (ms at SIZE x 1), each side's a little different so the sides' tails differ.
        constexpr float lengths[2][NuclearSpring::springsPerSide] = { { 37.1f, 43.7f, 51.3f }, { 39.2f, 46.1f, 54.9f } };
        constexpr float makeUp = 1.1f;   // the tank's sum against the sound (measured on pink noise in the tests)
    }

    void NuclearSpring::prepare (double rate, int)
    {
        sampleRate = rate;
        for (int side = 0; side < 2; ++side)
            for (int k = 0; k < springsPerSide; ++k)
            {
                auto& sp = springs[(size_t) side][(size_t) k];
                sp.lengthMs = lengths[side][k];
                // Room for the longest spring at SIZE x 1.4, and a little over.
                sp.line.assign ((size_t) std::ceil (sp.lengthMs * 1.45 * 0.001 * rate) + 4, 0.0f);
            }
        fastRate = (float) (1.0 - std::exp (-1.0 / (0.003 * rate)));
        slowRate = (float) (1.0 - std::exp (-1.0 / (0.06 * rate)));
        kickRelease = (float) (1.0 - std::exp (-1.0 / (0.08 * rate)));
        lowCutRate = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 100.0 / rate));
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));
        reset();
    }

    void NuclearSpring::reset() noexcept
    {
        for (auto& side : springs)
            for (auto& sp : side)
            {
                std::fill (sp.line.begin(), sp.line.end(), 0.0f);
                sp.write = 0;
                sp.xPrev.fill (0.0f);
                sp.yPrev.fill (0.0f);
                sp.tone = sp.feedback = sp.swing = 0.0f;
            }
        lowCut = {};
        fastLevel = slowLevel = 0.0f;
        kick = 1.0f;
        mixNow = 0.0f;
        levelShown.store (-100.0f, std::memory_order_relaxed);
        kickShown.store (1.0f, std::memory_order_relaxed);
    }

    void NuclearSpring::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        // MIX under a thousandth counts as 0 (the mod matrix eases a knob down and never lands exactly on 0).
        const auto mixTarget = s.on && s.mix >= 1.0e-3f ? juce::jmin (1.0f, s.mix) : 0.0f;
        if (mixTarget <= 0.0f && isResting())
            return;   // untouched; the tank empty, so it wakes clean

        const auto a = dripCoefficient (s.drip);
        const auto toneCut = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz (s.tone) / (float) sampleRate);
        const auto scale = sizeScale (s.size);
        const auto decay = decaySeconds (s.decay);
        const auto boing = juce::jlimit (0.0f, 1.0f, s.boing);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        // Each spring's travel time (samples, the line's read point) and its loop gain for DECAY (the all-passes'
        // own delay, about stages x (1 - a) / (1 + a) samples at low pitches, counted in the loop's time).
        std::array<std::array<float, springsPerSide>, 2> delays {}, gains {};
        for (int side = 0; side < 2; ++side)
            for (int k = 0; k < springsPerSide; ++k)
            {
                auto& sp = springs[(size_t) side][(size_t) k];
                const auto samples = juce::jlimit (2.0f, (float) sp.line.size() - 3.0f, sp.lengthMs * scale * 0.001f * (float) sampleRate);
                delays[(size_t) side][(size_t) k] = samples;
                const auto loopSeconds = (samples + (float) stages * (1.0f - a) / (1.0f + a)) / (float) sampleRate;
                gains[(size_t) side][(size_t) k] = std::pow (10.0f, -3.0f * loopSeconds / decay);
            }

        double outPower = 0.0;
        for (int i = 0; i < n; ++i)
        {
            mixNow += (mixTarget - mixNow) * smoothing;
            if (mixTarget <= 0.0f && mixNow < 1.0e-5f)
                mixNow = 0.0f;

            const float in[2] = { left[i], right[i] };

            // BOING: a hit well over the last moments' level is pushed harder into the tank.
            const auto level = juce::jmax (std::abs (in[0]), std::abs (in[1]));
            fastLevel += (level - fastLevel) * fastRate;
            slowLevel += (level - slowLevel) * slowRate;
            const auto ratio = fastLevel / juce::jmax (1.0e-6f, slowLevel);
            const auto hit = 1.0f + 3.0f * boing * juce::jlimit (0.0f, 1.0f, (ratio - 1.3f) / 2.0f);
            kick = hit > kick ? hit : kick + (1.0f - kick) * kickRelease;

            float wet[2] = { 0.0f, 0.0f };
            for (int side = 0; side < 2; ++side)
            {
                // Into the tank: nothing under 100 Hz, harder on a hit, softly clipped (a kicked tank clangs).
                lowCut[(size_t) side] += (in[side] - lowCut[(size_t) side]) * lowCutRate;
                const auto pushed = (in[side] - lowCut[(size_t) side]) * kick;
                const auto drive = pushed / (1.0f + 0.5f * std::abs (pushed));
                for (int k = 0; k < springsPerSide; ++k)
                {
                    auto& sp = springs[(size_t) side][(size_t) k];
                    // The spring: the sound plus what came back round, spread by the all-passes, then travelling.
                    auto x = drive + sp.feedback;
                    for (int st = 0; st < stages; ++st)
                    {
                        const auto y = a * x + sp.xPrev[(size_t) st] - a * sp.yPrev[(size_t) st];
                        sp.xPrev[(size_t) st] = x;
                        sp.yPrev[(size_t) st] = y;
                        x = y;
                    }
                    const auto size = (int) sp.line.size();
                    sp.line[(size_t) sp.write] = x;
                    const auto d = delays[(size_t) side][(size_t) k];
                    const auto whole = (int) d;
                    const auto frac = d - (float) whole;
                    const auto r0 = sp.line[(size_t) ((sp.write - whole + size) % size)];
                    const auto r1 = sp.line[(size_t) ((sp.write - whole - 1 + size) % size)];
                    const auto travelled = r0 + frac * (r1 - r0);
                    sp.write = sp.write + 1 == size ? 0 : sp.write + 1;
                    sp.tone += (travelled - sp.tone) * toneCut;
                    sp.feedback = sp.tone * gains[(size_t) side][(size_t) k];
                    sp.swing += (std::abs (sp.tone) - sp.swing) * 0.001f;
                    wet[side] += sp.tone;
                }
                wet[side] *= makeUp / (float) springsPerSide;
            }
            // WIDTH: the sides' tails kept apart, or mixed to the middle.
            const auto mid = 0.5f * (wet[0] + wet[1]), sideDiff = 0.5f * (wet[0] - wet[1]) * width;
            const auto outL = mid + sideDiff, outR = mid - sideDiff;
            left[i] = in[0] + mixNow * outL;
            if (stereo)
                right[i] = in[1] + mixNow * outR;
            outPower += 0.5 * ((double) outL * outL + (double) outR * outR) * (double) (mixNow * mixNow);
        }

        levelShown.store ((float) (10.0 * std::log10 (outPower / juce::jmax (1, n) + 1.0e-12)), std::memory_order_relaxed);
        kickShown.store (kick, std::memory_order_relaxed);
        for (int side = 0; side < 2; ++side)
            for (int k = 0; k < springsPerSide; ++k)
                swingShown[(size_t) side][(size_t) k].store (springs[(size_t) side][(size_t) k].swing, std::memory_order_relaxed);

        if (mixNow <= 0.0f)
            reset();   // asleep again: the tank emptied so it wakes clean
    }
}
