#include "HyperJump.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;   // AMOUNT's smoothing
        constexpr float snapSeconds = 0.005f;    // the build's shaping in and out (the drop snaps back in this)
        constexpr float topHighPassHz = 800.0f;  // the lows thinned to here by the build's end
        constexpr float narrowing = 0.9f;        // the image narrowed by this share at the build's end
        constexpr float chopEdgeIn = 0.0015f, chopEdgeOut = 0.003f;   // the gate's edges, seconds
        constexpr float maxMakeUp = 1.41f;       // the chop's loss made up to +3 dB at most
        constexpr float boomSeconds = 0.3f, boomGlideSeconds = 0.12f, thumpSeconds = 0.025f;
        constexpr float spaceFeedSeconds = 0.25f, spaceSeconds = 1.5f, spaceDampHz = 6000.0f;
        constexpr std::array<double, 4> lineMs { 41.3, 53.9, 67.1, 79.7 };
    }

    void HyperJump::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto sr = (float) sampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * sr));
        snapCoef = 1.0f - std::exp (-1.0f / (snapSeconds * sr));
        followUp = 1.0f - std::exp (-1.0f / (0.01f * sr));
        followDown = 1.0f - std::exp (-1.0f / (0.4f * sr));
        thumpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 1500.0f / sr);
        dampCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * spaceDampHz / sr);
        for (size_t k = 0; k < lines.size(); ++k)
        {
            lineLength[k] = juce::jmax (1, (int) std::round (lineMs[k] * 0.001 * sampleRate));
            lines[k].assign ((size_t) lineLength[k], 0.0f);
        }
        for (auto& s : shifters)
            s.prepare (sampleRate);
        reset();
    }

    void HyperJump::reset() noexcept
    {
        resting = true;
        amountNow = target = 0.0f;
        ownBeats = 0.0;
        lastBeats = -1.0e9;
        wasBuilding = false;
        once = Once::idle;
        onceStart = 0.0;
        shapeMix = shiftMix = 0.0f;
        for (auto& s : shifters)
            s.reset();
        for (auto& side : highPass)
            side = {};
        hpCoef = 0.0f;
        coefCountdown = 0;
        follower = 0.0f;
        dropAge = 1.0e9f;
        hitLevel = boomPhase = thump = 0.0f;
        for (size_t k = 0; k < lines.size(); ++k)
        {
            std::fill (lines[k].begin(), lines[k].end(), 0.0f);
            lineIndex[k] = 0;
        }
        damp = {};
        spaceLevel = tailEnergy = 0.0f;
        quietSamples = 0;
        amountShown.store (0.0f);
        progressShown.store (-1.0f);
        climbShown.store (0.0f);
        toDropShown.store (-1.0f);
        hitShown.store (0.0f);
    }

    double HyperJump::buildAt (double beats, const Settings& s, double buildBeats, bool& drop) noexcept
    {
        drop = false;
        const auto continuous = beats >= lastBeats && beats - lastBeats < 1.0;
        if (s.mode == 0)
        {
            // LOOP: the second half of every 2 x LENGTH bars builds; the bar line after it drops.
            const auto cycle = 2.0 * buildBeats;
            const auto pos = beats - std::floor (beats / cycle) * cycle;
            const auto building = pos >= buildBeats;
            drop = wasBuilding && ! building && continuous && target > 0.0f;
            return building ? (pos - buildBeats) / buildBeats : -1.0;
        }
        // ONCE: from the next bar line after AMOUNT comes up, one build and one drop, then landed until AMOUNT goes to 0.
        if (target <= 0.0f)
        {
            once = Once::idle;
            return -1.0;
        }
        if (once == Once::idle || (once != Once::landed && beats < onceStart - 1.0e-9 && once == Once::building))
        {
            onceStart = std::ceil (beats / 4.0 - 1.0e-9) * 4.0;
            once = Once::waiting;
        }
        if (once == Once::waiting && beats >= onceStart)
            once = Once::building;
        if (once == Once::building)
        {
            const auto p = (beats - onceStart) / buildBeats;
            if (p >= 1.0)
            {
                once = Once::landed;
                drop = true;
                return -1.0;
            }
            return p;
        }
        return -1.0;
    }

    void HyperJump::startDrop() noexcept
    {
        dropAge = 0.0f;
        boomPhase = 0.0f;
        dropsShown.fetch_add (1, std::memory_order_relaxed);
    }

    void HyperJump::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0 || lines[0].empty())
            return;
        target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        const auto bars = lengthBars[(size_t) juce::jlimit (0, (int) lengthBars.size() - 1, s.length)];
        barsShown.store (bars, std::memory_order_relaxed);
        const auto buildBeats = 4.0 * bars;
        const auto tempo = juce::jmax (20.0, bpm);
        const auto beatsPerSample = tempo / 60.0 / sampleRate;
        if (resting)
        {
            if (target <= 0.0f)
            {
                ownBeats += beatsPerSample * n;
                return;   // untouched, bit for bit
            }
            // Waking: with the song stopped, its own count starts a build at once.
            ownBeats = s.mode == 0 ? buildBeats : 0.0;
            once = Once::idle;
            lastBeats = -1.0e9;
            wasBuilding = false;
            resting = false;
        }
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;

        const auto sr = (float) sampleRate;
        const auto rise = juce::jlimit (0.0f, 1.0f, s.rise);
        const auto chop = juce::jlimit (0.0f, 1.0f, s.chop);
        const auto hit = juce::jlimit (0.0f, 1.0f, s.hit);
        const auto nBeats = (int) buildBeats;
        const auto feedback = std::pow (10.0f, -3.0f * (float) (0.25 * (lineMs[0] + lineMs[1] + lineMs[2] + lineMs[3]) * 0.001) / spaceSeconds);
        auto lastP = juce::jmax (0.0f, progressShown.load (std::memory_order_relaxed));
        auto semis = climbShown.load (std::memory_order_relaxed);
        auto progress = -1.0;

        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountCoef;
            if (target <= 0.0f && amountNow < 1.0e-5f)
                amountNow = 0.0f;
            const auto beats = start + beatsPerSample * i;
            bool drop = false;
            progress = buildAt (beats, s, buildBeats, drop);
            const auto building = progress >= 0.0;
            wasBuilding = building;
            lastBeats = beats;
            if (building)
            {
                lastP = (float) progress;
                semis = climbFor ((int) std::floor (progress * nBeats), nBeats, rise, amountNow);
            }

            auto l = left[i];
            auto r = right != nullptr ? right[i] : l;
            const auto level = juce::jmax (std::abs (l), std::abs (r));
            follower += (level - follower) * (level > follower ? followUp : followDown);
            if (drop)
            {
                startDrop();
                hitLevel = hit * amountNow * juce::jmin (0.5f, 1.5f * follower);
                spaceLevel = hit * amountNow;
            }

            // The build's shaping, gliding in and out (the drop snaps it back).
            shapeMix += ((building ? amountNow : 0.0f) - shapeMix) * snapCoef;
            shiftMix += ((building && rise > 0.0f && amountNow > 0.0f ? 1.0f : 0.0f) - shiftMix) * snapCoef;
            if (! building && shapeMix < 1.0e-6f)
                shapeMix = 0.0f;
            if (! building && shiftMix < 1.0e-6f)
                shiftMix = 0.0f;

            // CLIMB (the shifters always hear the sound while awake, so a build starts with the past to hand).
            const auto ratio = std::exp2 (semis / 12.0f);
            const auto shiftedL = shifters[0].processSample (l, ratio);
            const auto shiftedR = right != nullptr ? shifters[1].processSample (r, ratio) : shiftedL;
            if (shiftMix > 0.0f)
            {
                l += (shiftedL - l) * shiftMix;
                r += (shiftedR - r) * shiftMix;
            }

            if (shapeMix > 0.0f)
            {
                // The lows thinned (two one-poles, the cutoff worked out every 16 samples).
                if (--coefCountdown <= 0)
                {
                    coefCountdown = 16;
                    const auto hz = 20.0f + (topHighPassHz - 20.0f) * lastP * lastP;
                    hpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / sr);
                }
                float* sides[] = { &l, &r };
                for (int side = 0; side < 2; ++side)
                {
                    auto x = *sides[side];
                    auto& st = highPass[(size_t) side];
                    st[0] += (x - st[0]) * hpCoef;
                    const auto once1 = x - st[0];
                    st[1] += (once1 - st[1]) * hpCoef;
                    const auto thinned = once1 - st[1];
                    *sides[side] = x + (thinned - x) * shapeMix;
                }

                // CHOP: faster in four stages, deeper as it goes.
                const auto stepBeats = chopBeats (lastP);
                const auto stepSamples = (float) (stepBeats * 60.0 / tempo * sampleRate);
                const auto phase = (float) (beats / stepBeats - std::floor (beats / stepBeats));
                const auto t = phase * stepSamples;
                const auto open = 0.5f * stepSamples;
                const auto edgeIn = juce::jmin (chopEdgeIn * sr, 0.2f * stepSamples);
                const auto edgeOut = juce::jmin (chopEdgeOut * sr, 0.3f * stepSamples);
                auto g = 0.0f;
                if (t < edgeIn)
                    g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t / edgeIn);
                else if (t < open)
                    g = 1.0f;
                else if (t < open + edgeOut)
                    g = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (t - open) / edgeOut);
                const auto depth = shapeMix * chop * (0.35f + 0.65f * lastP);
                const auto makeUp = juce::jmin (maxMakeUp, 1.0f / std::sqrt (0.5f + 0.5f * (1.0f - depth) * (1.0f - depth)));
                const auto gain = (1.0f - depth * (1.0f - g)) * makeUp;
                l *= gain;
                r *= gain;

                // The image narrowing towards a point.
                const auto mid = 0.5f * (l + r);
                const auto side = 0.5f * (l - r) * (1.0f - shapeMix * narrowing * lastP);
                l = mid + side;
                r = mid - side;
            }

            // THE DROP: the boom, the thump and the burst of space.
            if (dropAge < 2.0f)
            {
                const auto age = dropAge;
                const auto hz = 38.0f + 32.0f * std::exp (-age / boomGlideSeconds);
                boomPhase += hz / sr;
                boomPhase -= std::floor (boomPhase);
                const auto envelope = juce::jmin (1.0f, age / 0.002f) * std::exp (-age / boomSeconds);
                thump += (noise() - thump) * thumpCoef;
                const auto boom = hitLevel * (0.75f * envelope * std::sin (juce::MathConstants<float>::twoPi * boomPhase)
                                              + 0.6f * std::exp (-age / thumpSeconds) * thump);
                l += boom;
                r += boom;
                hitShown.store (hitLevel > 0.0f ? envelope : 0.0f, std::memory_order_relaxed);
                dropAge += 1.0f / sr;
            }
            const auto feed = dropAge < spaceFeedSeconds ? spaceLevel * (1.0f - dropAge / spaceFeedSeconds) : 0.0f;
            if (feed > 0.0f || tailEnergy > 1.0e-7f)
            {
                std::array<float, 4> outs {};
                auto sum = 0.0f;
                for (size_t k = 0; k < 4; ++k)
                {
                    outs[k] = lines[k][(size_t) lineIndex[k]];
                    sum += outs[k];
                }
                const auto input = feed * 0.5f * (l + r);
                for (size_t k = 0; k < 4; ++k)
                {
                    const auto mixed = outs[k] - 0.5f * sum;
                    damp[k] += (mixed - damp[k]) * dampCoef;
                    lines[k][(size_t) lineIndex[k]] = input + feedback * damp[k];
                    lineIndex[k] = (lineIndex[k] + 1) % lineLength[k];
                }
                const auto wetL = 0.35f * (outs[0] + outs[2]);
                const auto wetR = 0.35f * (outs[1] + outs[3]);
                l += wetL;
                r += wetR;
                tailEnergy = juce::jmax (std::abs (wetL) + std::abs (wetR), tailEnergy * 0.9995f);
            }

            left[i] = l;
            if (right != nullptr)
                right[i] = r;
        }

        amountShown.store (amountNow, std::memory_order_relaxed);
        progressShown.store ((float) progress, std::memory_order_relaxed);
        climbShown.store (progress >= 0.0 ? semis : 0.0f, std::memory_order_relaxed);
        {
            // Beats to the next drop (none coming: -1).
            const auto now = start + beatsPerSample * n;
            auto toDrop = -1.0;
            if (target > 0.0f && s.mode == 0)
            {
                const auto cycle = 2.0 * buildBeats;
                toDrop = cycle - (now - std::floor (now / cycle) * cycle);
            }
            else if (target > 0.0f && (once == Once::waiting || once == Once::building))
                toDrop = onceStart + buildBeats - now;
            toDropShown.store ((float) toDrop, std::memory_order_relaxed);
        }

        const auto settled = target <= 0.0f && amountNow <= 0.0f && shapeMix <= 0.0f && shiftMix <= 0.0f && dropAge > 1.5f;
        quietSamples = settled && tailEnergy < 1.0e-6f ? quietSamples + n : 0;
        if (settled && quietSamples > (int) (0.3 * sampleRate))
        {
            const auto beatsNow = ownBeats;
            const auto drops = dropsShown.load();
            reset();
            ownBeats = beatsNow;
            dropsShown.store (drops);
        }
    }
}
