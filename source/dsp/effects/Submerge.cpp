#include "Submerge.h"

namespace effects
{
    namespace
    {
        // The sea's four lines (ms), apart enough not to share echoes; its fall about 4 s. Our own choices.
        constexpr float lineMs[Submerge::numLines] = { 43.7f, 57.1f, 71.3f, 89.9f };
        constexpr float seaSeconds = 4.0f, seaToneHz = 1600.0f, seaIn = 0.35f, seaOut = 0.8f;
        constexpr int chunk = 32;                     // the water's cutoff worked out every 32 samples
        constexpr float depthSeconds = 0.03f;         // DEPTH's smoothing
        constexpr float dryDip = 0.2f;                // the sound a little lower at the bottom, under the sea
        constexpr float levelSeconds = 0.3f, gainLow = 0.5f, gainHigh = 1.6f;   // the level kept (as AFTERMATH's)
        // The pressure: the delay is DEPTH x CURRENT x (10 + 9 wander) ms, the wander -1..1 from two slow sines at
        // unrelated rates and a random drift; the delay never moves faster than 0.012 s a second (about 21 cents), so
        // turning DEPTH up quickly sags the pitch a little rather than diving.
        constexpr float baseMs = 10.0f, swingMs = 9.0f, slewLimit = 0.012f;
        constexpr double wanderHz1 = 0.13, wanderHz2 = 0.31;
        constexpr float wander1 = 0.55f, wander2 = 0.3f, driftShare = 0.15f, driftSeconds = 1.5f;
        // The bubbles: 300-1400 Hz, rising by half again over their life (20-70 ms, the higher the shorter), up to
        // 14 a second; as loud as the music lets them (a follower 10 ms up, 0.5 s down; none under about -54 dB).
        constexpr float bubbleLowHz = 300.0f, bubbleHighHz = 1400.0f, bubbleClimb = 1.5f, bubbleLevel = 0.3f;
        constexpr float followUpSeconds = 0.01f, followDownSeconds = 0.5f, presenceFull = 0.05f, presenceGone = 0.002f;
    }

    void Submerge::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        auto size = 1;
        while (size < (int) (0.03 * sampleRate))   // the longest pressure delay (19 ms) with room to spare
            size *= 2;
        delaySize = size;
        for (auto& d : delay)
            d.assign ((size_t) size, 0.0f);
        size = 1;
        while (size < (int) (0.1 * sampleRate))    // the longest sea line
            size *= 2;
        lineSize = size;
        for (auto& l : lines)
            l.assign ((size_t) size, 0.0f);
        for (int k = 0; k < numLines; ++k)
            lengths[(size_t) k] = juce::jlimit (8, lineSize - 2, juce::roundToInt (lineMs[k] * 0.001f * (float) sampleRate));
        depthCoef = 1.0f - std::exp (-1.0f / (depthSeconds * (float) sampleRate));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * (float) sampleRate));
        followUp = 1.0f - std::exp (-1.0f / (followUpSeconds * (float) sampleRate));
        followDown = 1.0f - std::exp (-1.0f / (followDownSeconds * (float) sampleRate));
        reset();
    }

    void Submerge::reset() noexcept
    {
        for (auto& d : delay)
            std::fill (d.begin(), d.end(), 0.0f);
        delayWrite = 0;
        wanderPhase1 = 0.0;
        wanderPhase2 = 0.37;
        drift = driftTarget = driftCountdown = 0.0f;
        lastDelay = 0.0f;
        darkA = darkB = {};
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        writes = {};
        loopTone = {};
        seaPeak = 0.0f;
        for (auto& b : bubbles)
            b = {};
        follower = 0.0f;
        untilNext = 0.0f;
        powerIn = powerOut = 0.0f;
        gainNow = 1.0f;
        depthNow = 0.0f;
        quietSamples = 0;
        resting = true;
        depthShown.store (0.0f);
        seaShown.store (0.0f);
        gainShown.store (0.0f);
        bendShown.store (0.0f);
        for (int k = 0; k < maxBubbles; ++k)
        {
            bubblePan[(size_t) k].store (0.0f);
            bubbleRise[(size_t) k].store (-1.0f);
        }
    }

    void Submerge::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (delaySize == 0 || n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.depth < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.depth);
        if (resting && target <= 0.0f)
            return;   // untouched, bit for bit
        resting = false;

        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto rate = (float) sampleRate;
        const auto dt = 1.0f / rate;

        const auto current = juce::jlimit (0.0f, 1.0f, s.current);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        const auto same = 0.5f * (1.0f + width), cross = 0.5f * (1.0f - width);
        const auto sea = juce::jlimit (0.0f, 1.0f, s.sea);
        const auto bubbleAmount = juce::jlimit (0.0f, 1.0f, s.bubbles);
        const auto murky = murkHz (s.murk);
        float mean = 0.0f;
        for (int k = 0; k < numLines; ++k)
            mean += (float) lengths[(size_t) k];
        const auto seaFeedback = std::pow (10.0f, -3.0f * mean / (float) numLines / rate / seaSeconds);
        const auto seaCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * seaToneHz / rate);
        const auto maxDelay = (float) (delaySize - 4);
        const auto slew = slewLimit * dt * rate;   // samples of delay per sample
        const auto driftCoef = 1.0f - std::exp (-1.0f / (driftSeconds * rate));

        float seaHigh = 0.0f, bendNow = 0.0f;
        for (int start = 0; start < n; start += chunk)
        {
            const auto m = juce::jmin (chunk, n - start);
            const auto hz = juce::jmin (0.45f * rate, 20000.0f * std::pow (murky / 20000.0f, depthNow));
            const auto darkCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / rate);
            for (int i = 0; i < m; ++i)
            {
                const auto at = start + i;
                depthNow += (target - depthNow) * depthCoef;
                if (target <= 0.0f && depthNow < 1.0e-5f)
                    depthNow = 0.0f;
                const auto inL = left[at], inR = right != nullptr ? right[at] : inL;

                // The pressure: the delay's length wandering, never faster than the slew limit.
                wanderPhase1 += wanderHz1 / sampleRate;
                wanderPhase2 += wanderHz2 / sampleRate;
                if (wanderPhase1 >= 1.0) wanderPhase1 -= 1.0;
                if (wanderPhase2 >= 1.0) wanderPhase2 -= 1.0;
                driftCountdown -= dt;
                if (driftCountdown <= 0.0f)
                {
                    driftTarget = 2.0f * uniform() - 1.0f;
                    driftCountdown = 0.8f + 1.7f * uniform();
                }
                drift += (driftTarget - drift) * driftCoef;
                const auto wander = wander1 * (float) std::sin (juce::MathConstants<double>::twoPi * wanderPhase1)
                                  + wander2 * (float) std::sin (juce::MathConstants<double>::twoPi * wanderPhase2 + 1.0)
                                  + driftShare * drift;
                const auto wanted = depthNow * current * (baseMs + swingMs * wander) * 0.001f * rate;
                const auto step = juce::jlimit (-slew, slew, wanted - lastDelay);
                lastDelay = juce::jlimit (0.0f, maxDelay, lastDelay + step);
                if (target <= 0.0f && depthNow <= 0.0f && lastDelay < 1.0e-4f)
                    lastDelay = 0.0f;
                bendNow = -1731.2f * step;   // a delay growing by d samples a sample bends the pitch about -1731 d cents

                delay[0][(size_t) delayWrite] = inL;
                delay[1][(size_t) delayWrite] = inR;
                std::array<float, 2> pressed { inL, inR };
                if (lastDelay > 0.0f)
                {
                    const auto whole = (int) lastDelay;
                    const auto frac = lastDelay - (float) whole;
                    const auto a = (delayWrite - whole) & (delaySize - 1), b = (a - 1) & (delaySize - 1);
                    for (int c = 0; c < 2; ++c)
                    {
                        const auto& d = delay[(size_t) c];
                        pressed[(size_t) c] = d[(size_t) a] + (d[(size_t) b] - d[(size_t) a]) * frac;
                    }
                }
                delayWrite = (delayWrite + 1) & (delaySize - 1);

                // The water closing over it.
                darkA[0] += (pressed[0] - darkA[0]) * darkCoef;
                darkB[0] += (darkA[0] - darkB[0]) * darkCoef;
                darkA[1] += (pressed[1] - darkA[1]) * darkCoef;
                darkB[1] += (darkA[1] - darkB[1]) * darkCoef;
                const auto wetL = pressed[0] + (darkB[0] - pressed[0]) * depthNow;
                const auto wetR = pressed[1] + (darkB[1] - pressed[1]) * depthNow;

                // The sea: four lines, Householder mixed, darkened in the loop, fed the drowned sound.
                std::array<float, numLines> y {};
                float sum = 0.0f;
                for (int k = 0; k < numLines; ++k)
                {
                    y[(size_t) k] = lines[(size_t) k][(size_t) ((writes[(size_t) k] - lengths[(size_t) k]) & (lineSize - 1))];
                    sum += y[(size_t) k];
                }
                const auto feed = depthNow * sea * seaIn;
                for (int k = 0; k < numLines; ++k)
                {
                    const auto mixed = y[(size_t) k] - 0.5f * sum;
                    loopTone[(size_t) k] += (mixed - loopTone[(size_t) k]) * seaCoef;
                    const auto in = (k % 2 == 0 ? wetL : wetR) * feed;
                    lines[(size_t) k][(size_t) writes[(size_t) k]] = in + seaFeedback * loopTone[(size_t) k];
                    writes[(size_t) k] = (writes[(size_t) k] + 1) & (lineSize - 1);
                }
                const auto seaL = 0.5f * (y[0] + y[2]), seaR = 0.5f * (y[1] + y[3]);
                const auto outSeaL = (same * seaL + cross * seaR) * seaOut, outSeaR = (same * seaR + cross * seaL) * seaOut;
                seaHigh = juce::jmax (seaHigh, std::abs (outSeaL), std::abs (outSeaR));

                const auto under = 1.0f - dryDip * depthNow;
                const auto outL = wetL * under + outSeaL, outR = wetR * under + outSeaR;

                // The level kept.
                powerIn += (inL * inL + inR * inR - powerIn) * levelCoef;
                powerOut += (outL * outL + outR * outR - powerOut) * levelCoef;
                const auto gainWanted = depthNow <= 0.0f ? 1.0f
                                      : powerOut > 1.0e-12f ? juce::jlimit (gainLow, gainHigh, std::sqrt (powerIn / powerOut)) : gainNow;
                gainNow += (gainWanted - gainNow) * depthCoef;

                // The bubbles, only while there's music.
                const auto level = juce::jmax (std::abs (inL), std::abs (inR));
                follower += (level - follower) * (level > follower ? followUp : followDown);
                const auto presence = follower < presenceGone ? 0.0f : juce::jmin (1.0f, follower / presenceFull);
                const auto bubblesPerSecond = bubbleRate (bubbleAmount, depthNow) * (presence > 0.0f ? 1.0f : 0.0f);
                if (bubblesPerSecond > 0.0f)
                {
                    // The gap counted in bubbles (an exponential of mean 1, used up at the rate now), so a gap drawn
                    // while DEPTH is still rising doesn't stretch to the rate it had then.
                    untilNext -= bubblesPerSecond * dt;
                    if (untilNext <= 0.0f)
                    {
                        untilNext = -std::log (juce::jmax (1.0e-6f, uniform()));
                        for (auto& b : bubbles)
                            if (! b.active)
                            {
                                const auto size = uniform();
                                b.active = true;
                                b.phase = 0.0;
                                b.hz = bubbleLowHz * std::pow (bubbleHighHz / bubbleLowHz, size);
                                b.life = 0.07f - 0.05f * size;
                                b.age = 0.0f;
                                b.rise = std::log (bubbleClimb) / b.life;
                                b.pan = (2.0f * uniform() - 1.0f) * width;
                                const auto angle = (b.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                                b.gainL = std::cos (angle) * juce::MathConstants<float>::sqrt2;
                                b.gainR = std::sin (angle) * juce::MathConstants<float>::sqrt2;
                                const auto strength = 0.4f + 0.6f * uniform() * uniform();
                                b.level = bubbleLevel * bubbleAmount * follower * strength;
                                started.fetch_add (1, std::memory_order_relaxed);
                                break;
                            }
                    }
                }
                auto addL = 0.0f, addR = 0.0f;
                for (auto& b : bubbles)
                {
                    if (! b.active)
                        continue;
                    const auto hzNow = b.hz * std::exp (b.rise * b.age);
                    b.phase += hzNow / sampleRate;
                    if (b.phase >= 1.0)
                        b.phase -= 1.0;
                    const auto envelope = juce::jmin (1.0f, b.age / 0.002f) * std::exp (-4.0f * b.age / b.life);
                    const auto v = b.level * envelope * (float) std::sin (juce::MathConstants<double>::twoPi * b.phase);
                    addL += v * b.gainL;
                    addR += v * b.gainR;
                    b.age += dt;
                    if (b.age >= 1.6f * b.life)
                        b.active = false;
                }

                left[at] = outL * gainNow + addL;
                if (right != nullptr)
                    right[at] = outR * gainNow + addR;
            }
        }

        bool anyBubble = false;
        for (int k = 0; k < maxBubbles; ++k)
        {
            const auto& b = bubbles[(size_t) k];
            anyBubble = anyBubble || b.active;
            bubblePan[(size_t) k].store (b.pan, std::memory_order_relaxed);
            bubbleRise[(size_t) k].store (b.active ? juce::jmin (1.0f, b.age / (1.6f * b.life)) : -1.0f, std::memory_order_relaxed);
        }
        depthShown.store (depthNow);
        seaShown.store (juce::jlimit (0.0f, 1.0f, seaHigh * 3.0f));
        gainShown.store (juce::Decibels::gainToDecibels (gainNow));
        bendShown.store (bendNow);

        // Back at rest once DEPTH is 0, the delay gone, no bubble left, the sea died away and the gain settled: cleared, so it wakes clean.
        if (target <= 0.0f && depthNow <= 0.0f && lastDelay <= 0.0f && ! anyBubble && seaHigh < 1.0e-5f)
        {
            quietSamples = std::abs (gainNow - 1.0f) < 1.0e-3f ? quietSamples + n : 0;
            if (quietSamples > (int) (0.3 * sampleRate))
                reset();
        }
        else
            quietSamples = 0;
    }
}
