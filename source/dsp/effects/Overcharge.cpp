#include "Overcharge.h"

namespace effects
{
    namespace
    {
        // Our own choices throughout (none measured from a product).
        constexpr float echoSeconds = 0.16f;        // each climbing repeat this much after the last
        constexpr float feedbackTop = 0.45f;        // the repeats' feedback at full charge
        constexpr float shimmerShare = 0.5f;        // the repeats' level in the glow
        constexpr float crystalShare = 1.2f;        // the crystals' level against how loud you play
        constexpr float wobbleDepth = 0.0046f;      // the crystals' shimmer: about +-8 cents
        constexpr float fadeSeconds = 0.003f;       // a burst's edges
        constexpr float levelSeconds = 0.3f, gainLow = 0.5f, gainHigh = 1.41f;
        constexpr float restSeconds = 0.5f;         // quiet this long after CHARGE reaches 0: at rest
        constexpr float quietLevel = 1.0e-6f;

        int powerOfTwoAtLeast (double samples)
        {
            auto length = 1;
            while (length < (int) samples)
                length *= 2;
            return length;
        }
    }

    void Overcharge::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto rate = (float) sampleRate;
        const auto echoLength = powerOfTwoAtLeast ((echoSeconds + 0.05) * sampleRate);
        const auto historyLength = powerOfTwoAtLeast (1.0 * sampleRate);
        for (int s = 0; s < 2; ++s)
        {
            echoLine[s].assign ((size_t) echoLength, 0.0f);
            history[s].assign ((size_t) historyLength, 0.0f);
        }
        for (auto& shifter : shifters)
            shifter.prepare (sampleRate);
        echoMask = echoLength - 1;
        historyMask = historyLength - 1;
        echoDelay = juce::roundToInt (echoSeconds * rate);
        smoothCoef = 1.0f - std::exp (-1.0f / (0.03f * rate));
        followUp = 1.0f - std::exp (-1.0f / (0.005f * rate));
        followDown = 1.0f - std::exp (-1.0f / (0.15f * rate));
        crystalCoef = 1.0f - std::exp (-1.0f / (0.02f * rate));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * rate));
        flashCoef = std::exp (-1.0f / (0.25f * rate));
        reset();
    }

    void Overcharge::reset() noexcept
    {
        for (int s = 0; s < 2; ++s)
        {
            std::fill (echoLine[s].begin(), echoLine[s].end(), 0.0f);
            std::fill (history[s].begin(), history[s].end(), 0.0f);
        }
        for (auto& shifter : shifters)
            shifter.reset();
        echoWrite = historyWrite = 0;
        shimmerTone = {};
        glowTone = {};
        for (auto& c : crystals)
            c = {};
        follower = 0.0f;
        chargeNow = 0.0f;
        burst = Burst::none;
        burstLeft = burstLength = 0;
        held = {};
        sparkLeft = sparkGain = 0.0f;
        sparkState = {};
        powerIn = powerOut = 0.0f;
        gainNow = 1.0f;
        quietSamples = 0;
        flash = 0.0f;
        resting = true;
        chargeShown.store (0.0f);
        overloadShown.store (0.0f);
        glowShown.store (0.0f);
        flashShown.store (0.0f);
        burstShown.store (0);
    }

    void Overcharge::setNotes (const float* pitches, int count) noexcept
    {
        numNotes = juce::jlimit (0, maxNotes, count);
        for (int k = 0; k < numNotes; ++k)
            notes[(size_t) k] = pitches[k];

        // Each note's crystals: one already at the pitch keeps ringing; a new one takes a silent crystal.
        std::array<bool, maxCrystals> kept {};
        for (int k = 0; k < numNotes; ++k)
            for (int p = 0; p < partials; ++p)
            {
                const auto hz = 440.0f * std::pow (2.0f, (notes[(size_t) k] + partialSemis[p] - 69.0f) / 12.0f);
                if (hz > 0.45f * (float) sampleRate)
                    continue;
                auto found = -1;
                for (int c = 0; c < maxCrystals && found < 0; ++c)
                    if (! kept[(size_t) c] && crystals[(size_t) c].target > 0.0f && std::abs (crystals[(size_t) c].hz - hz) < hz * 0.003f)
                        found = c;
                for (int c = 0; c < maxCrystals && found < 0; ++c)
                    if (! kept[(size_t) c] && crystals[(size_t) c].target <= 0.0f && crystals[(size_t) c].gain < 1.0e-4f)
                    {
                        found = c;
                        auto& fresh = crystals[(size_t) c];
                        fresh.hz = hz;
                        fresh.phase = 0.0;
                        fresh.step = 0.0f;
                        fresh.wobble = next01();
                        fresh.wobbleHz = 5.0f + 2.5f * next01();
                        fresh.pan = ((k + p) % 2 == 0 ? -1.0f : 1.0f) * (0.4f + 0.6f * (float) p / (float) (partials - 1));
                    }
                if (found >= 0)
                {
                    kept[(size_t) found] = true;
                    crystals[(size_t) found].target = partialLevel[p];
                }
            }
        for (int c = 0; c < maxCrystals; ++c)
            if (! kept[(size_t) c])
                crystals[(size_t) c].target = 0.0f;   // fades out
    }

    float Overcharge::readHistory (int side, double delaySamples) const noexcept
    {
        const auto at = (double) historyWrite - 1.0 - delaySamples;
        const auto base = (int) std::floor (at);
        const auto t = (float) (at - (double) base);
        const auto& h = history[side];
        const auto y0 = h[(size_t) ((base - 1) & historyMask)], y1 = h[(size_t) (base & historyMask)];
        const auto y2 = h[(size_t) ((base + 1) & historyMask)], y3 = h[(size_t) ((base + 2) & historyMask)];
        const auto c1 = 0.5f * (y2 - y0);
        const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void Overcharge::startBurst (float chaos) noexcept
    {
        const auto rate = (float) sampleRate;
        const auto pick = next01();
        burst = pick < 0.45f ? Burst::stutter : pick < 0.8f ? Burst::crush : Burst::jolt;
        burstLength = juce::jmax (8, juce::roundToInt ((0.04f + 0.18f * next01()) * rate));
        burstLeft = burstLength;
        burstDepth = 0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, chaos);
        if (burst == Burst::stutter)
        {
            sliceLength = juce::jmax (16, juce::roundToInt ((0.015f + 0.055f * next01()) * rate));
            sliceStart = historyWrite - sliceLength;   // the last moments, repeated
            slicePos = 0;
        }
        else if (burst == Burst::crush)
        {
            holdEvery = 4 + (int) (20.0f * next01() * (0.4f + 0.6f * chaos));
            holdCount = 0;
            crushLevels = std::pow (2.0f, 7.0f - 3.0f * next01() * chaos);
        }
        else
        {
            const auto semis = (next01() < 0.5f ? -1.0f : 1.0f) * (3.0f + 9.0f * next01() * (0.5f + 0.5f * chaos));
            joltRatio = std::pow (2.0f, semis / 12.0f);
            // The read starts far enough back that, gaining on the present, it never passes it.
            joltDelay = joltRatio > 1.0f ? 4.0 + (double) (joltRatio - 1.0f) * burstLength : 4.0;
        }
        burstCount.store (burstCount.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        burstShown.store ((int) burst, std::memory_order_relaxed);
        flash = 1.0f;
    }

    void Overcharge::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (history[0].empty() || n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.charge < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.charge);
        if (! s.on)
        {
            if (! resting)
                reset();   // off: let go at once (the rack's own fade covers the switch)
            return;
        }
        if (resting && target <= 0.0f)
            return;        // at rest: untouched
        if (resting)
        {
            const auto sounding = notes;
            const auto count = numNotes;
            reset();       // waking: clean, the notes kept
            setNotes (sounding.data(), count);
            resting = false;
        }

        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto rate = (float) sampleRate;
        const auto glow = juce::jlimit (0.0f, 1.0f, s.glow), chaos = juce::jlimit (0.0f, 1.0f, s.chaos);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        const auto toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz (s.tone) / rate);
        const auto shimmerCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz (s.tone * 0.8f) / rate);
        const auto crystalScale = crystalShare / std::sqrt ((float) juce::jmax (1, numNotes));
        const auto fadeLength = juce::jmax (1.0f, fadeSeconds * rate);
        std::array<float, maxCrystals> panL {}, panR {};
        for (int c = 0; c < maxCrystals; ++c)
        {
            const auto angle = (crystals[(size_t) c].pan * width + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            panL[(size_t) c] = std::cos (angle);
            panR[(size_t) c] = std::sin (angle);
        }

        for (int i = 0; i < n; ++i)
        {
            chargeNow += (target - chargeNow) * smoothCoef;
            if (target <= 0.0f && chargeNow < 1.0e-5f)
                chargeNow = 0.0f;
            const auto c = chargeOf (chargeNow), o = overloadOf (chargeNow);
            const float in[2] = { left[i], right != nullptr ? right[i] : left[i] };
            const auto mid = 0.5f * (in[0] + in[1]);
            const auto level = std::abs (mid);
            follower += (level > follower ? followUp : followDown) * (level - follower);

            // The crystals above the notes (each crystal's shimmer worked out every 32 samples: it's 5-7.5 Hz).
            const auto newWobble = wobbleCount == 0;
            wobbleCount = (wobbleCount + 1) & 31;
            float crystalL = 0.0f, crystalR = 0.0f, crystalSum = 0.0f;
            for (int k = 0; k < maxCrystals; ++k)
            {
                auto& x = crystals[(size_t) k];
                if (x.target <= 0.0f && x.gain < 1.0e-6f)
                {
                    x.gain = 0.0f;
                    continue;
                }
                x.gain += (x.target - x.gain) * crystalCoef;
                if (newWobble || x.step <= 0.0f)
                {
                    const auto wob = (float) std::sin (juce::MathConstants<double>::twoPi * x.wobble);
                    x.wobble += 32.0 * (double) x.wobbleHz / sampleRate;
                    x.wobble -= std::floor (x.wobble);
                    x.step = (float) ((double) x.hz * (1.0 + (double) (wobbleDepth * wob)) / sampleRate);
                }
                x.phase += (double) x.step;
                if (x.phase >= 1.0)
                    x.phase -= 1.0;
                const auto v = x.gain * std::sin (juce::MathConstants<float>::twoPi * (float) x.phase);
                crystalL += v * panL[(size_t) k];
                crystalR += v * panR[(size_t) k];
                crystalSum += x.gain;
            }
            const auto crystalLevel = c * glow * follower * crystalScale;

            // The shimmer: the sound and the fed-back repeats, an octave up, heard a moment later. (Gated by the charge,
            // so at CHARGE 0 nothing new goes in and the repeats die away.)
            const auto gate = juce::jmin (1.0f, c * 50.0f);
            std::array<float, 2> shifted {};
            for (int side = 0; side < 2; ++side)
                shifted[(size_t) side] = shifters[(size_t) side].processSample (gate * (in[side] + feedbackTop * c * shimmerTone[(size_t) side]), 2.0f);

            float out[2];
            float wetSum = 0.0f;
            for (int side = 0; side < 2; ++side)
            {
                echoLine[side][(size_t) echoWrite] = std::tanh (shifted[(size_t) side]);
                const auto echoed = echoLine[side][(size_t) ((echoWrite - echoDelay) & echoMask)];
                shimmerTone[(size_t) side] += (echoed - shimmerTone[(size_t) side]) * shimmerCoef;
                const auto crystal = (side == 0 ? crystalL : crystalR) * crystalLevel;
                const auto wet = crystal + shimmerShare * c * shimmerTone[(size_t) side];
                glowTone[(size_t) side] += (wet - glowTone[(size_t) side]) * toneCoef;
                out[side] = in[side] + glowTone[(size_t) side];
                wetSum += std::abs (glowTone[(size_t) side]) + std::abs (shimmerTone[(size_t) side]);
            }
            echoWrite = (echoWrite + 1) & echoMask;

            // The overload: a burn, then the bursts.
            for (int side = 0; side < 2; ++side)
            {
                history[side][(size_t) historyWrite] = out[side];
                if (o > 0.0f)
                {
                    const auto drive = 1.0f + 3.0f * o * (0.4f + 0.6f * chaos);
                    out[side] += 0.6f * o * (std::tanh (drive * out[side]) - out[side]);
                }
            }
            historyWrite = (historyWrite + 1) & historyMask;

            if (burst == Burst::none && o > 0.0f && next01() < burstRate (o, chaos) / rate)
                startBurst (chaos);
            if (burst != Burst::none)
            {
                const auto done = (float) (burstLength - burstLeft);
                const auto edge = juce::jmin (1.0f, done / fadeLength, (float) burstLeft / fadeLength);
                const auto d = burstDepth * edge;
                for (int side = 0; side < 2; ++side)
                {
                    float wild = out[side];
                    if (burst == Burst::stutter)
                    {
                        const auto w = juce::jmin (1.0f, (float) slicePos / 32.0f, (float) (sliceLength - slicePos) / 32.0f);
                        wild = w * history[side][(size_t) ((sliceStart + slicePos) & historyMask)];
                    }
                    else if (burst == Burst::crush)
                    {
                        if (holdCount == 0)
                            held[(size_t) side] = std::round (out[side] * crushLevels) / crushLevels;
                        wild = held[(size_t) side];
                    }
                    else
                        wild = readHistory (side, juce::jmax (2.0, joltDelay));
                    out[side] += d * (wild - out[side]);
                }
                if (burst == Burst::stutter)
                    slicePos = (slicePos + 1) % sliceLength;
                else if (burst == Burst::crush)
                    holdCount = (holdCount + 1) % holdEvery;
                else
                    joltDelay += 1.0 - (double) joltRatio;
                if (--burstLeft <= 0)
                    burst = Burst::none;
            }

            // Sparks: short clicks of noise, more with the overload and CHAOS.
            if (o > 0.0f && sparkLeft <= 0.0f && next01() < o * chaos * 40.0f / rate)
            {
                sparkLeft = 0.0012f * rate;
                const auto r = next01();
                sparkGain = (0.25f + 0.75f * r * r) * follower * 1.5f;
            }
            if (sparkLeft > 0.0f)
            {
                sparkLeft -= 1.0f;
                for (int side = 0; side < 2; ++side)
                {
                    const auto noise = next01() * 2.0f - 1.0f;
                    const auto high = noise - sparkState[(size_t) side];   // the noise's top: a crackle, not a thump
                    sparkState[(size_t) side] = noise;
                    out[side] += sparkGain * high * 0.5f;
                }
                sparkGain *= 0.995f;
            }

            // The level kept.
            powerIn += (in[0] * in[0] + in[1] * in[1] - powerIn) * levelCoef;
            powerOut += (out[0] * out[0] + out[1] * out[1] - powerOut) * levelCoef;
            const auto wanted = chargeNow > 0.0f && powerOut > 1.0e-12f ? juce::jlimit (gainLow, gainHigh, std::sqrt (powerIn / powerOut)) : 1.0f;
            gainNow += (wanted - gainNow) * smoothCoef;
            left[i] = out[0] * gainNow;
            if (right != nullptr)
                right[i] = out[1] * gainNow;
            flash *= flashCoef;

            // Back at rest once CHARGE is 0 and everything it added has died away.
            if (chargeNow <= 0.0f && burst == Burst::none && wetSum < quietLevel && crystalLevel * crystalSum < quietLevel)
            {
                if (++quietSamples > (int) (restSeconds * rate) && std::abs (gainNow - 1.0f) < 1.0e-4f)
                {
                    reset();   // the rest of the block passes untouched
                    return;
                }
            }
            else
                quietSamples = 0;
        }

        chargeShown.store (chargeOf (chargeNow), std::memory_order_relaxed);
        overloadShown.store (overloadOf (chargeNow), std::memory_order_relaxed);
        glowShown.store (juce::jlimit (0.0f, 1.0f, chargeOf (chargeNow) * glow * follower * 4.0f), std::memory_order_relaxed);
        flashShown.store (flash, std::memory_order_relaxed);
    }
}
