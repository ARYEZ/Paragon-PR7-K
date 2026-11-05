#include "Legion.h"

namespace effects
{
    namespace
    {
        // Each player's own: its share of LOOSE's lateness, its drift's speed (Hz) and starting place, how far it drifts
        // (a share of DETUNE), and where it stands (a share of SPREAD; they fill in from the sides inwards alternately so
        // the stage widens as the section grows). Our own choices.
        constexpr float lateness[]  = { 0.20f, 0.85f, 0.45f, 0.65f, 0.30f, 1.00f, 0.55f, 0.75f };
        constexpr float driftHz[]   = { 0.11f, 0.17f, 0.13f, 0.23f, 0.19f, 0.09f, 0.15f, 0.21f };
        constexpr float driftFrom[] = { 0.00f, 0.37f, 0.71f, 0.13f, 0.52f, 0.89f, 0.26f, 0.64f };
        constexpr float driftShare[]= { 0.70f, 1.00f, 0.85f, 0.60f, 0.95f, 0.75f, 0.90f, 0.65f };
        constexpr float place[]     = { -0.75f, 0.75f, -0.35f, 0.35f, -1.00f, 1.00f, -0.55f, 0.55f };
        constexpr int chunk = 16;            // the delays are worked out every 16 samples and glided between
        // The players' share in the level: the whole is turned down by 1 / sqrt (1 + share x their weights squared).
        // Measured on a saw chord and on noise (LegionTests): 0.55 keeps a full section within about 1 dB of one player.
        constexpr float levelShare = 0.55f;
    }

    void Legion::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        auto length = 1;
        while (length < (int) (0.12 * sampleRate))   // 120 ms: the latest player plus its sway
            length *= 2;
        line.assign ((size_t) length, 0.0f);
        mask = length - 1;
        sizeCoef = 1.0f - std::exp (-1.0f / (0.04f * (float) sampleRate));
        reset();
    }

    void Legion::reset() noexcept
    {
        std::fill (line.begin(), line.end(), 0.0f);
        write = 0;
        sizeNow = 0.0f;
        wasOn = false;
        toneState = {};
        for (int k = 0; k < numPlayers; ++k)
        {
            driftPhase[(size_t) k] = driftFrom[k];
            delayNow[(size_t) k] = -1.0f;
            weightShown[(size_t) k].store (0.0f);
            panShown[(size_t) k].store (place[k]);
            delayShown[(size_t) k].store (0.0f);
            driftShown[(size_t) k].store (0.0f);
        }
        sizeShown.store (0.0f);
    }

    float Legion::readAt (double delaySamples) const noexcept
    {
        // Four-point Hermite, `delaySamples` behind the last sample written.
        const auto at = (double) write - 1.0 - delaySamples;
        const auto base = (int) std::floor (at);
        const auto t = (float) (at - (double) base);
        const auto y0 = line[(size_t) ((base - 1) & mask)], y1 = line[(size_t) (base & mask)];
        const auto y2 = line[(size_t) ((base + 1) & mask)], y3 = line[(size_t) ((base + 2) & mask)];
        const auto c1 = 0.5f * (y2 - y0);
        const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void Legion::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (line.empty() || n == 0 || channels == 0)
            return;
        if (! s.on)
        {
            // Off: nothing kept; it lets go of the players at once (the rack's own fade covers the switch).
            if (wasOn)
                reset();
            return;
        }
        if (! wasOn)
        {
            std::fill (line.begin(), line.end(), 0.0f);   // switched on: no old sound in the line
            wasOn = true;
        }
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto target = s.size < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.size);

        // Resting: keep recording the middle so a player joining hears the present; the sound is untouched.
        if (sizeNow <= 0.0f && target <= 0.0f)
        {
            for (int i = 0; i < n; ++i)
            {
                line[(size_t) write] = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
                write = (write + 1) & mask;
            }
            sizeShown.store (0.0f);
            for (auto& w : weightShown)
                w.store (0.0f);
            return;
        }

        const auto rate = (float) sampleRate;
        const auto lateMs = looseMs (s.loose), cents = detuneCents (s.detune);
        // The tone: a one-pole on each side's players.
        const auto toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz (s.tone) / rate);
        std::array<float, numPlayers> panL {}, panR {};
        for (int k = 0; k < numPlayers; ++k)
        {
            const auto p = juce::jlimit (-1.0f, 1.0f, place[k] * s.spread);
            const auto angle = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;   // equal power
            panL[(size_t) k] = std::cos (angle);
            panR[(size_t) k] = std::sin (angle);
            panShown[(size_t) k].store (p);
        }

        for (int start = 0; start < n; start += chunk)
        {
            const auto m = juce::jmin (chunk, n - start);
            // Each player's delay at the chunk's end: its lateness plus its sway. The sway's depth is set so its fastest
            // change makes `cents` x its share of pitch (a delay changing at slope d bends the pitch by about -1731 d cents).
            std::array<float, numPlayers> from {}, to {};
            for (int k = 0; k < numPlayers; ++k)
            {
                auto& phase = driftPhase[(size_t) k];
                phase += (double) driftHz[k] * (double) m / sampleRate;
                phase -= std::floor (phase);
                const auto slope = cents * driftShare[k] / 1731.0f;
                const auto depth = slope / (juce::MathConstants<float>::twoPi * driftHz[k]) * rate;   // samples
                const auto late = (2.0f + (lateMs - 2.0f) * lateness[k]) * 0.001f * rate;
                const auto centre = juce::jmax (late, depth + 0.001f * rate);
                const auto sway = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                to[(size_t) k] = centre + depth * sway;
                from[(size_t) k] = delayNow[(size_t) k] < 0.0f ? to[(size_t) k] : delayNow[(size_t) k];
                delayNow[(size_t) k] = to[(size_t) k];
                delayShown[(size_t) k].store (to[(size_t) k] / rate * 1000.0f);
                driftShown[(size_t) k].store (sway * driftShare[k] * (cents > 0.0f ? 1.0f : 0.0f));
            }
            for (int i = 0; i < m; ++i)
            {
                const auto at = start + i;
                const auto inL = left[at], inR = right != nullptr ? right[at] : inL;
                line[(size_t) write] = 0.5f * (inL + inR);
                write = (write + 1) & mask;

                sizeNow += (target - sizeNow) * sizeCoef;
                if (target <= 0.0f && sizeNow < 1.0e-5f)
                    sizeNow = 0.0f;
                const auto players = sizeNow * (float) numPlayers;
                const auto t = (float) (i + 1) / (float) m;
                float wetL = 0.0f, wetR = 0.0f, power = 0.0f;
                for (int k = 0; k < numPlayers; ++k)
                {
                    const auto w = juce::jlimit (0.0f, 1.0f, players - (float) k);
                    if (w <= 0.0f)
                        break;   // they join in order
                    const auto d = from[(size_t) k] + (to[(size_t) k] - from[(size_t) k]) * t;
                    const auto v = w * readAt (d);
                    wetL += v * panL[(size_t) k];
                    wetR += v * panR[(size_t) k];
                    power += w * w;
                }
                toneState[0] += (wetL - toneState[0]) * toneCoef;
                toneState[1] += (wetR - toneState[1]) * toneCoef;
                const auto gain = 1.0f / std::sqrt (1.0f + levelShare * power);
                left[at] = (inL + toneState[0]) * gain;
                if (right != nullptr)
                    right[at] = (inR + toneState[1]) * gain;
            }
        }
        sizeShown.store (sizeNow);
        for (int k = 0; k < numPlayers; ++k)
            weightShown[(size_t) k].store (juce::jlimit (0.0f, 1.0f, sizeNow * (float) numPlayers - (float) k));
        if (sizeNow <= 0.0f)
            toneState = {};
    }
}
