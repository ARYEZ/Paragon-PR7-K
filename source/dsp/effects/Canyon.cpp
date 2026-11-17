#include "Canyon.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;
        constexpr std::array<float, Canyon::numNear> nearTimes { 1.0f, 1.53f, 2.21f };    // x the first wall's
        constexpr std::array<float, Canyon::numNear> nearGains { 0.6f, 0.42f, 0.3f };
        constexpr std::array<float, Canyon::numNear> nearPans { -1.0f, 0.8f, -0.45f };    // -1 left, 1 right (x WIDTH)
        constexpr float nearDarker = 0.8f;                  // each further wall's cutoff
        constexpr std::array<double, 2> rockMs[2] { { 0.7, 1.1 }, { 0.9, 1.3 } };   // per side: the rock's smear (short: 3.7 / 5.3 ms put the echo 9 ms late)
        constexpr float rockGain = 0.5f;
        constexpr float farIn = 0.5f, farFromNear = 0.35f, farGain = 0.5f;
        constexpr float farDarker = 0.6f;                   // the far wall's cutoff against TONE's
        constexpr float farHighHz = 120.0f;
        constexpr float glideSeconds = 0.2f;                // the far wall following a tempo change
        constexpr float quietLevel = 1.0e-5f;               // -100 dB
        constexpr float restSeconds = 0.5f;

        float coefFor (float hz, float rate) noexcept
        {
            return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (hz, 0.45f * rate) / rate);
        }
    }

    void Canyon::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * (float) sampleRate));
        const auto nearMax = (int) std::ceil (nearSeconds (1.0f) * nearTimes.back() * sampleRate) + 8;
        nearLine.assign ((size_t) nearMax, 0.0f);
        const auto farMax = (int) std::ceil (maxFarSeconds * sampleRate) + 8;
        for (auto& line : farLines)
            line.assign ((size_t) farMax, 0.0f);
        for (int side = 0; side < 2; ++side)
            for (int k = 0; k < 2; ++k)
                rock[(size_t) side][(size_t) k].line.assign ((size_t) juce::jmax (1, (int) std::round (rockMs[side][(size_t) k] * 0.001 * sampleRate)), 0.0f);
        reset();
    }

    void Canyon::reset() noexcept
    {
        amountNow = 0.0f;
        quietSamples = 0;
        std::fill (nearLine.begin(), nearLine.end(), 0.0f);
        nearWrite = 0;
        for (auto& f : nearLow)
            f.z = 0.0f;
        for (auto& side : rock)
            for (auto& a : side)
            {
                std::fill (a.line.begin(), a.line.end(), 0.0f);
                a.pos = 0;
            }
        for (auto& line : farLines)
            std::fill (line.begin(), line.end(), 0.0f);
        farWrite = 0;
        farDelay = 0.0;
        for (auto& f : farLow)
            f.z = 0.0f;
        for (auto& f : farHighLow)
            f.z = 0.0f;
        wallLevel.fill (0.0f);
        resting = true;
        amountShown.store (0.0f);
        for (auto& w : wallShown)
            w.store (0.0f);
    }

    float Canyon::read (const std::vector<float>& line, int write, double delaySamples) noexcept
    {
        // Straight-line between samples: `delaySamples` back from the newest written.
        const auto size = (int) line.size();
        auto position = (double) write - 1.0 - delaySamples;
        while (position < 0.0)
            position += size;
        const auto i = (int) position;
        const auto frac = (float) (position - i);
        const auto a = line[(size_t) i];
        const auto b = line[(size_t) (i + 1 < size ? i + 1 : 0)];
        return a + frac * (b - a);
    }

    void Canyon::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0 || nearLine.empty())
            return;
        const auto target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        if (resting && target <= 0.0f)
            return;   // untouched, bit for bit
        if (resting)
            farDelay = farSeconds (s.far, bpm) * sampleRate;   // start at the tempo's distance, no glide
        resting = false;

        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto rate = (float) sampleRate;
        const auto nearDelay = (double) nearSeconds (s.size) * sampleRate;
        const auto farTarget = farSeconds (s.far, bpm) * sampleRate;
        const auto glide = 1.0 - std::exp (-1.0 / (glideSeconds * sampleRate));
        const auto cutoff = toneHz (s.tone);
        std::array<float, numNear> nearCoef {};
        for (int k = 0; k < numNear; ++k)
            nearCoef[(size_t) k] = coefFor (cutoff * std::pow (nearDarker, (float) k), rate);
        const auto farCoef = coefFor (cutoff * farDarker, rate);
        const auto farHighCoef = coefFor (farHighHz, rate);
        const auto feedback = feedbackFor (s.repeats);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        std::array<float, numNear> gainLeft {}, gainRight {};
        for (int k = 0; k < numNear; ++k)
        {
            // Equal-power pans: each wall on its side by WIDTH.
            const auto pan = nearPans[(size_t) k] * width;
            const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            gainLeft[(size_t) k] = nearGains[(size_t) k] * std::cos (angle) * juce::MathConstants<float>::sqrt2;
            gainRight[(size_t) k] = nearGains[(size_t) k] * std::sin (angle) * juce::MathConstants<float>::sqrt2;
        }
        // The far pair: the left line's reply mostly left, the right's mostly right (how much by WIDTH).
        const auto farOwn = 0.5f + 0.5f * width, farOther = 0.5f - 0.5f * width;
        const auto nearSize = (int) nearLine.size();
        const auto farSize = (int) farLines[0].size();
        std::array<float, numNear + 1> peaks {};
        float wetPeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            amountNow += amountCoef * (target - amountNow);
            farDelay += glide * (farTarget - farDelay);
            const auto l = left[i];
            const auto r = right != nullptr ? right[i] : l;
            const auto send = 0.5f * (l + r) * amountNow;

            nearLine[(size_t) nearWrite] = send;
            nearWrite = nearWrite + 1 < nearSize ? nearWrite + 1 : 0;

            float nearLeft = 0.0f, nearRight = 0.0f, nearSum = 0.0f;
            for (int k = 0; k < numNear; ++k)
            {
                const auto echo = nearLow[(size_t) k].low (read (nearLine, nearWrite, nearDelay * nearTimes[(size_t) k]), nearCoef[(size_t) k]);
                nearLeft += echo * gainLeft[(size_t) k];
                nearRight += echo * gainRight[(size_t) k];
                nearSum += echo * nearGains[(size_t) k];
                peaks[(size_t) k] = juce::jmax (peaks[(size_t) k], std::abs (echo * nearGains[(size_t) k]));
            }
            // The rock smears each side's near echoes a little.
            for (int k = 0; k < 2; ++k)
            {
                nearLeft = rock[0][(size_t) k].process (nearLeft, rockGain);
                nearRight = rock[1][(size_t) k].process (nearRight, rockGain);
            }

            // The far wall: a ping-pong pair at the song's distance, each pass darker and thinned in the lows.
            const auto replyLeft = read (farLines[0], farWrite, farDelay);
            const auto replyRight = read (farLines[1], farWrite, farDelay);
            const auto into = send * farIn + nearSum * farFromNear;
            auto backLeft = farLow[0].low (into + replyRight * feedback, farCoef);
            backLeft -= farHighLow[0].low (backLeft, farHighCoef);
            auto backRight = farLow[1].low (replyLeft * feedback, farCoef);
            backRight -= farHighLow[1].low (backRight, farHighCoef);
            farLines[0][(size_t) farWrite] = backLeft;
            farLines[1][(size_t) farWrite] = backRight;
            farWrite = farWrite + 1 < farSize ? farWrite + 1 : 0;
            const auto farLeft = farGain * (replyLeft * farOwn + replyRight * farOther);
            const auto farRight = farGain * (replyRight * farOwn + replyLeft * farOther);
            peaks[numNear] = juce::jmax (peaks[numNear], std::abs (farGain * 0.5f * (replyLeft + replyRight)));

            const auto wetLeft = nearLeft + farLeft, wetRight = nearRight + farRight;
            wetPeak = juce::jmax (wetPeak, std::abs (wetLeft), std::abs (wetRight));
            left[i] = l + wetLeft;
            if (right != nullptr)
                right[i] = r + wetRight;
        }

        // The page's levels: quick up, slow down.
        for (int k = 0; k <= numNear; ++k)
        {
            wallLevel[(size_t) k] = juce::jmax (peaks[(size_t) k], wallLevel[(size_t) k] * std::exp (-(float) n / (0.15f * rate)));
            wallShown[(size_t) k].store (juce::jmin (1.0f, wallLevel[(size_t) k] * 2.0f), std::memory_order_relaxed);
        }
        amountShown.store (amountNow, std::memory_order_relaxed);
        farShown.store ((float) (farDelay / sampleRate), std::memory_order_relaxed);

        // At rest once nothing goes in and the canyon has fallen silent.
        if (target <= 0.0f && amountNow < 1.0e-4f && wetPeak < quietLevel)
        {
            quietSamples += n;
            if (quietSamples >= (int) (restSeconds * rate))
                reset();
        }
        else
            quietSamples = 0;
    }
}
