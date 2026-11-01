#include "Crackle.h"

namespace effects
{
    namespace
    {
        constexpr double burstSecondsLong = 0.0012;   // a click's noise: about a millisecond, falling to -60 dB
        constexpr float clickMakeUp = 2.2f;           // a click through the band-pass peaks near LEVEL (measured)
        constexpr float followRef = 0.18f;            // the level (about -15 dBFS) at which FOLLOW lets the full RATE through
    }

    void Crackle::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        burstSamples = juce::jmax (4, (int) std::round (burstSecondsLong * sampleRate));
        burstDecay = (float) std::pow (0.001, 1.0 / burstSamples);
        followAttack = 1.0f - (float) std::exp (-1.0 / (0.002 * sampleRate));
        followRelease = 1.0f - (float) std::exp (-1.0 / (0.15 * sampleRate));
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
        for (auto& t : tone)
        {
            t.prepare (spec);
            t.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            t.setResonance (1.1f);
        }
        reset();
    }

    void Crackle::reset() noexcept
    {
        for (auto& c : clicks)
            c = {};
        for (auto& t : tone)
            t.reset();
        follower = 0.0f;
        wander = wanderTarget = wanderStep = 0.0f;
        wanderLeft = 0;
        lastToneHz = -1.0f;
        binSamplesLeft = 0;
    }

    void Crackle::startClick (const Settings& s) noexcept
    {
        for (auto& c : clicks)
            if (c.left <= 0)
            {
                // Each click its own strength (mostly middling, now and then a hard one) and its own place.
                const auto r = random01();
                c.level = 0.25f + 0.75f * r * r;
                const auto pan = (random01() * 2.0f - 1.0f) * juce::jlimit (0.0f, 1.0f, s.width);
                const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                c.gainL = std::cos (angle) * juce::MathConstants<float>::sqrt2;
                c.gainR = std::sin (angle) * juce::MathConstants<float>::sqrt2;
                c.left = burstSamples;
                return;
            }
        // All busy (a storm): the click is lost in the others.
    }

    void Crackle::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto numSamples = (int) block.getNumSamples();
        const auto numChannels = juce::jmin (2, (int) block.getNumChannels());
        if (numChannels == 0 || numSamples == 0)
            return;
        if (bypass.isSilent() && ! s.on)
        {
            currentRate = 0.0f;
            return;
        }

        bypass.process (block, s.on, [this, &s, numChannels, numSamples] (juce::dsp::AudioBlock<float> wet)
        {
            const auto toneHz = juce::jlimit (300.0f, (float) (0.45 * sampleRate), s.toneHz);
            if (toneHz != lastToneHz)
            {
                for (auto& t : tone)
                    t.setCutoffFrequency (toneHz);
                lastToneHz = toneHz;
            }
            const auto gain = juce::Decibels::decibelsToGain (s.levelDb) * clickMakeUp;
            const auto follow = juce::jlimit (0.0f, 1.0f, s.follow);
            const auto cluster = juce::jlimit (0.0f, 1.0f, s.cluster);
            const auto baseRate = juce::jlimit (0.0f, 2000.0f, s.rate);
            const auto binLength = (int) std::round (binSeconds * sampleRate);
            auto* l = wet.getChannelPointer (0);
            auto* r = numChannels > 1 ? wet.getChannelPointer (1) : nullptr;
            float lastRate = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                // How loud the sound coming in is (both sides), for FOLLOW.
                const auto in = juce::jmax (std::abs (l[i]), r != nullptr ? std::abs (r[i]) : 0.0f);
                follower += (in - follower) * (in > follower ? followAttack : followRelease);

                // CLUSTER: every so often a new target for the wander (in log rate), reached in a straight line.
                if (--wanderLeft <= 0)
                {
                    wanderLeft = (int) ((0.12 + 0.5 * random01()) * sampleRate);
                    wanderTarget = (random01() * 2.0f - 1.0f) * 2.5f;   // e^2.5: up to about 12 times either way
                    wanderStep = (wanderTarget - wander) / (float) wanderLeft;
                }
                wander += wanderStep;

                const auto followShare = (1.0f - follow) + follow * juce::jmin (1.0f, follower / followRef);
                const auto rate = baseRate * std::exp (cluster * wander) * followShare;
                lastRate = rate;
                if (rate > 0.0f && random01() < rate / (float) sampleRate)
                {
                    startClick (s);
                    totalClicks.fetch_add (1, std::memory_order_relaxed);
                    const auto b = binIndex.load (std::memory_order_relaxed);
                    bins[(size_t) b].fetch_add (1, std::memory_order_relaxed);
                }

                // The clicks sounding: noise bursts falling fast.
                float sumL = 0.0f, sumR = 0.0f;
                for (auto& c : clicks)
                    if (c.left > 0)
                    {
                        const auto noise = (random01() * 2.0f - 1.0f) * c.level;
                        sumL += noise * c.gainL;
                        sumR += noise * c.gainR;
                        c.level *= burstDecay;
                        --c.left;
                    }
                const auto outL = tone[0].processSample (0, sumL) * gain;
                const auto outR = tone[1].processSample (0, sumR) * gain;
                if (r != nullptr)
                {
                    l[i] += outL;
                    r[i] += outR;
                }
                else
                    l[i] += 0.5f * (outL + outR);

                // The display's slices.
                if (--binSamplesLeft <= 0)
                {
                    binSamplesLeft = binLength;
                    const auto next = (binIndex.load (std::memory_order_relaxed) + 1) % numBins;
                    bins[(size_t) next].store (0, std::memory_order_relaxed);
                    binIndex.store (next, std::memory_order_relaxed);
                }
            }
            currentRate.store (lastRate, std::memory_order_relaxed);
        });
    }

    std::array<int, Crackle::numBins> Crackle::getHistory() const noexcept
    {
        std::array<int, numBins> out {};
        const auto newest = binIndex.load (std::memory_order_relaxed);
        for (int k = 0; k < numBins; ++k)
            out[(size_t) k] = bins[(size_t) ((newest + 1 + k) % numBins)].load (std::memory_order_relaxed);
        return out;
    }

    float Crackle::getRecentRate() const noexcept
    {
        // The last second's full slices (the newest is still filling).
        const auto history = getHistory();
        const auto perSecond = (int) std::round (1.0 / binSeconds);
        int count = 0;
        for (int k = numBins - 1 - perSecond; k < numBins - 1; ++k)
            count += history[(size_t) k];
        return (float) count;
    }
}
