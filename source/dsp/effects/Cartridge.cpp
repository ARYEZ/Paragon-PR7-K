#include "Cartridge.h"

namespace effects
{
    void Cartridge::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        frame = juce::jmax (1, (int) std::round (sampleRate / synth::cartridge::frameHz));
        for (auto& h : history)
            h.assign ((size_t) frame, 0.0f);
        for (auto& r : repeat)
            r.assign ((size_t) frame, 0.0f);
        reset();
    }

    void Cartridge::reset() noexcept
    {
        for (auto& h : history)
            std::fill (h.begin(), h.end(), 0.0f);
        write = 0;
        repeatLeft = repeatPos = 0;
        heldValue.fill (0.0f);
        holdCount = 0;
        resting = true;
        glitching.store (false, std::memory_order_relaxed);
    }

    float Cartridge::random01() noexcept
    {
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        return (float) (noise >> 8) / 16777216.0f;
    }

    void Cartridge::process (juce::dsp::AudioBlock<float> block, const synth::cartridge::Settings& s) noexcept
    {
        const auto crunch = s.crunchNow(), glitch = s.glitchNow();
        const auto n = (int) block.getNumSamples();
        const auto channels = juce::jmin (2, (int) block.getNumChannels());
        if (crunch <= 0.0f && glitch <= 0.0f && repeatLeft <= 0)
        {
            // Resting: nothing touched; the last frame still kept, so a glitch straight after waking has one.
            if (! resting)
                reset();
            for (int c = 0; c < channels; ++c)
            {
                const auto* x = block.getChannelPointer ((size_t) c);
                auto w = write;
                for (int i = 0; i < n; ++i)
                {
                    history[(size_t) c][(size_t) w] = x[i];
                    w = w + 1 < frame ? w + 1 : 0;
                }
            }
            write = (write + n) % frame;
            return;
        }
        resting = false;
        const auto hold = holdFor (crunch);
        const auto levels = levelsFor (crunch);
        const auto chance = glitch * maxGlitchesPerSecond / (float) synth::cartridge::frameHz;   // a chance each frame
        auto* l = block.getChannelPointer (0);
        auto* r = channels > 1 ? block.getChannelPointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { l[i], r != nullptr ? r[i] : l[i] };
            for (int c = 0; c < channels; ++c)
                history[(size_t) c][(size_t) write] = x[c];
            // A new frame: perhaps a glitch (the frame just heard, repeated).
            if (write == 0 && repeatLeft <= 0 && glitch > 0.0f && random01() < chance)
            {
                for (int c = 0; c < channels; ++c)
                    repeat[(size_t) c] = history[(size_t) c];
                repeatLeft = frame * (2 + (int) (random01() * 4.0f));
                repeatPos = 1 % frame;   // [0] is the sample just heard: start from the frame's oldest
                glitches.fetch_add (1, std::memory_order_relaxed);
            }
            if (repeatLeft > 0)
            {
                for (int c = 0; c < channels; ++c)
                    x[c] = repeat[(size_t) c][(size_t) repeatPos];
                repeatPos = repeatPos + 1 < frame ? repeatPos + 1 : 0;
                --repeatLeft;
            }
            if (crunch > 0.0f)
            {
                if (holdCount <= 0)
                {
                    for (int c = 0; c < channels; ++c)
                        heldValue[(size_t) c] = std::round (x[c] * levels) / levels;
                    holdCount = hold;
                }
                --holdCount;
                for (int c = 0; c < channels; ++c)
                    x[c] = heldValue[(size_t) c];
            }
            l[i] = x[0];
            if (r != nullptr)
                r[i] = x[1];
            write = write + 1 < frame ? write + 1 : 0;
        }
        glitching.store (repeatLeft > 0, std::memory_order_relaxed);
    }
}
