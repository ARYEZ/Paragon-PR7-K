#include "FreezeTag.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;   // AMOUNT's smoothing
        constexpr float cutSeconds = 0.003f;     // the freeze's edge in
        constexpr float returnSeconds = 0.004f;  // and out
        constexpr float grainScale = 0.8f;       // three grains at random places: about the slice's own level
        constexpr double trembleHz = 5.5, shakeHz = 7.3;
        constexpr double trembleRatio = 0.0145;  // 25 cents
        constexpr float shakeDepth = 0.15f;
        constexpr int crushHold = 6;
        constexpr float crushLevels = 32.0f;     // 6 bits: 64 levels across -1 to 1
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
    }

    uint32_t FreezeTag::nextRandom() noexcept
    {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return random;
    }

    void FreezeTag::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * (float) sampleRate));
        gateIn = 1.0f / (cutSeconds * (float) sampleRate);
        gateOut = 1.0f / (returnSeconds * (float) sampleRate);
        int size = 1;
        while (size < (int) sampleRate)
            size <<= 1;
        historyMask = size - 1;
        for (auto& side : sides)
        {
            side.history.assign ((size_t) size, 0.0f);
            side.slice.assign ((size_t) (0.26 * sampleRate) + 4, 0.0f);
        }
        writeIndex = 0;
        reset();
    }

    void FreezeTag::reset() noexcept
    {
        resting = true;
        amountNow = 0.0f;
        ownBeats = 0.0;
        frozen = caught = false;
        gate = 0.0f;
        tremblePhase = 0.0;
        sink = sinkPerSample = 1.0;
        crushCount = 0;
        settledSamples = 0;
        for (auto& side : sides)
        {
            std::fill (side.history.begin(), side.history.end(), 0.0f);
            std::fill (side.slice.begin(), side.slice.end(), 0.0f);
            side.low = side.held = 0.0f;
        }
        grains = {};
        amountShown.store (0.0f);
        gateShown.store (0.0f);
        positionShown.store (0.0f);
        caughtShown.store (false);
    }

    void FreezeTag::catchMoment (const Settings& s) noexcept
    {
        sliceLength = juce::jlimit (16, (int) sides[0].slice.size() - 4, (int) (sliceSeconds (s.slice) * sampleRate));
        grainLength = juce::jmax (8, sliceLength / 2);
        for (auto& side : sides)
        {
            // The last SLICE before now, oldest first.
            for (int k = 0; k < sliceLength; ++k)
                side.slice[(size_t) k] = side.history[(size_t) ((writeIndex - sliceLength + k) & historyMask)];
            side.low = side.slice[(size_t) (sliceLength - 1)];
            side.held = 0.0f;
        }
        const auto room = (double) sliceLength - 1.03 * (double) grainLength - 2.0;
        for (int g = 0; g < numGrains; ++g)
        {
            auto& grain = grains[(size_t) g];
            grain.start = room * (double) (nextRandom() & 0xffff) / 65535.0;
            grain.age = g * grainLength / numGrains;
            grain.position = grain.start + grain.age;
        }
        caught = (float) (nextRandom() & 0xffff) / 65535.0f < juce::jlimit (0.0f, 1.0f, s.caught) * amountNow;
        sink = 1.0;
        crushCount = 0;
    }

    void FreezeTag::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0 || historyMask == 0)
            return;
        const auto target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        if (resting)
        {
            // Untouched, bit for bit; the history keeps recording.
            for (int i = 0; i < n; ++i)
            {
                sides[0].history[(size_t) writeIndex] = left[i];
                sides[1].history[(size_t) writeIndex] = right != nullptr ? right[i] : left[i];
                writeIndex = (writeIndex + 1) & historyMask;
            }
            ownBeats += beatsPerSample * n;
            if (target <= 0.0f)
                return;
            resting = false;
            amountNow = 0.0f;
            gate = 0.0f;
            settledSamples = 0;
            // Waking inside a freeze waits for the next one.
            frozen = frozenAt (ppq >= 0.0 ? ppq : ownBeats, s.cycle, s.hold);
            return;
        }
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;

        const auto sr = (float) sampleRate;
        const auto level = juce::jlimit (0.0f, 1.0f, s.level);
        const auto tremble = juce::jlimit (0.0f, 1.0f, s.tremble);
        const auto frostCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (0.45f * sr, frostHz (s.frost)) / sr);
        const auto holdSamples = holdBeats (s.cycle, s.hold) / beatsPerSample;

        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountCoef;
            if (target <= 0.0f && amountNow < 1.0e-5f)
                amountNow = 0.0f;

            const auto f = frozenAt (start + beatsPerSample * i, s.cycle, s.hold);
            if (f && ! frozen)
            {
                frozen = true;
                catchMoment (s);
                sinkPerSample = caught ? std::exp2 (-1.0 / juce::jmax (1.0, holdSamples)) : 1.0;
                freezesShown.store (freezesShown.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
                caughtShown.store (caught, std::memory_order_relaxed);
            }
            else if (! f && frozen)
                frozen = false;
            gate = frozen ? juce::jmin (1.0f, gate + gateIn) : juce::jmax (0.0f, gate - gateOut);

            float* io[] = { &left[i], right != nullptr ? &right[i] : nullptr };
            const float dry[] = { left[i], right != nullptr ? right[i] : left[i] };
            // Record what's played (before anything here changes it).
            sides[0].history[(size_t) writeIndex] = dry[0];
            sides[1].history[(size_t) writeIndex] = dry[1];
            writeIndex = (writeIndex + 1) & historyMask;

            if (gate <= 0.0f || amountNow <= 0.0f)
                continue;   // playing: untouched

            // THE GHOST: three grains of the moment caught.
            tremblePhase += 1.0 / sampleRate;
            if (tremblePhase > 1000.0)
                tremblePhase -= 1000.0;
            const auto speed = sink * (1.0 + tremble * trembleRatio * std::sin (twoPi * trembleHz * tremblePhase));
            const auto shake = 1.0f + shakeDepth * tremble * (float) std::sin (twoPi * shakeHz * tremblePhase);
            sink *= sinkPerSample;
            std::array<float, 2> ghost { 0.0f, 0.0f };
            for (auto& grain : grains)
            {
                const auto w = (float) std::sin (juce::MathConstants<double>::pi * (double) grain.age / (double) grainLength);
                const auto window = w * w;
                const auto p = juce::jlimit (0.0, (double) sliceLength - 2.0, grain.position);
                const auto k = (int) p;
                const auto frac = (float) (p - k);
                for (int c = 0; c < 2; ++c)
                {
                    const auto& slice = sides[(size_t) c].slice;
                    ghost[(size_t) c] += window * (slice[(size_t) k] + frac * (slice[(size_t) k + 1] - slice[(size_t) k]));
                }
                grain.position += speed;
                if (++grain.age >= grainLength)
                {
                    grain.age = 0;
                    grain.start = ((double) sliceLength - 1.03 * (double) grainLength - 2.0) * (double) (nextRandom() & 0xffff) / 65535.0;
                    grain.position = grain.start;
                }
            }
            const auto crushNow = caught && (crushCount++ % crushHold == 0);
            const auto mix = amountNow * gate;
            for (int c = 0; c < 2; ++c)
            {
                if (io[c] == nullptr)
                    continue;
                auto& side = sides[(size_t) c];
                auto g = ghost[(size_t) c] * grainScale * shake;
                side.low += (g - side.low) * frostCoef;
                g = side.low;
                if (caught)
                {
                    if (crushNow)
                        side.held = std::round (juce::jlimit (-1.0f, 1.0f, g) * crushLevels) / crushLevels;
                    g = side.held;
                }
                *io[c] = dry[c] * (1.0f - mix) + g * level * mix;
            }
        }

        const auto c = cycleBeats[(size_t) juce::jlimit (0, (int) cycleBeats.size() - 1, s.cycle)];
        const auto end = start + beatsPerSample * n;
        amountShown.store (amountNow, std::memory_order_relaxed);
        gateShown.store (gate, std::memory_order_relaxed);
        positionShown.store ((float) ((end - c * std::floor (end / c)) / c), std::memory_order_relaxed);

        const auto settled = target <= 0.0f && amountNow <= 0.0f && gate <= 0.0f;
        settledSamples = settled ? settledSamples + n : 0;
        if (settled && settledSamples > (int) (0.3 * sampleRate))
            resting = true;   // the history and the clock go on
    }
}
