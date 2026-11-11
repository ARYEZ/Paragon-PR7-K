#include "Crossroads.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;    // AMOUNT's smoothing
        constexpr float sideSeconds = 0.013f;     // the change of side (about 40 ms to settle)
        constexpr float levelSeconds = 0.3f;      // the level kept: power followed over this
        constexpr float gainSeconds = 0.1f;
        constexpr float shimmerLevel = 0.55f;     // the octave up at LIGHT 1
        constexpr float airLevel = 0.5f;          // the top lifted at LIGHT 1
        constexpr float growlLevel = 0.75f;       // the octave down at DARK 1
        constexpr float darkenAmount = 0.7f;      // how much of the sound is darkened at DARK 1
        constexpr float roughRef = 0.3f;          // a 0.3 level comes out of the drive at 0.3
    }

    void Crossroads::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto sr = (float) sampleRate;
        const auto onePole = [sr] (float hz) { return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (hz, 0.45f * sr) / sr); };
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * sr));
        darknessCoef = 1.0f - std::exp (-1.0f / (sideSeconds * sr));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * sr));
        gainCoef = 1.0f - std::exp (-1.0f / (gainSeconds * sr));
        shimmerCut = onePole (500.0f);
        growlTop = onePole (900.0f);
        airCut = onePole (4000.0f);
        for (auto& side : sides)
        {
            side.up.prepare (sampleRate);
            side.down.prepare (sampleRate);
        }
        reset();
    }

    void Crossroads::reset() noexcept
    {
        resting = true;
        amountNow = 0.0f;
        darkness = 0.0f;
        ownBeats = 0.0;
        powerIn = powerOut = 0.0f;
        gain = 1.0f;
        settledSamples = 0;
        for (auto& side : sides)
        {
            side.up.reset();
            side.down.reset();
            side.upLow = side.downLow = side.dryLow = side.airLow = 0.0f;
        }
        amountShown.store (0.0f);
        darknessShown.store (0.0f);
        gainShown.store (0.0f);
    }

    void Crossroads::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        if (resting)
        {
            ownBeats += beatsPerSample * n;
            if (target <= 0.0f)
                return;   // untouched, bit for bit
            const auto beats = ownBeats;
            reset();
            ownBeats = beats;
            resting = false;
            const auto now = ppq >= 0.0 ? ppq : ownBeats;
            darkness = synth::crossroads::darkAt (now, s.length) ? 1.0f : 0.0f;
        }
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;

        const auto light = juce::jlimit (0.0f, 1.0f, s.light);
        const auto dark = juce::jlimit (0.0f, 1.0f, s.dark);

        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountCoef;
            if (target <= 0.0f && amountNow < 1.0e-5f)
                amountNow = 0.0f;
            const auto wantDark = synth::crossroads::darkAt (start + beatsPerSample * i, s.length) ? 1.0f : 0.0f;
            darkness += (wantDark - darkness) * darknessCoef;
            const auto a = amountNow;
            const auto lightMix = a * light * (1.0f - darkness);
            const auto darkMix = a * dark * darkness;
            const auto drive = 1.0f + 4.0f * darkMix;
            const auto driveNorm = roughRef / std::tanh (drive * roughRef);

            float* io[] = { &left[i], right != nullptr ? &right[i] : nullptr };
            auto inPower = 0.0f, outPower = 0.0f;
            for (int c = 0; c < 2; ++c)
            {
                if (io[c] == nullptr)
                    continue;
                auto& side = sides[(size_t) c];
                const auto dry = *io[c];
                // LIGHT: the octave up, its lows cut; the air (the top, lifted).
                const auto up = side.up.processSample (dry, 2.0f);
                side.upLow += (up - side.upLow) * shimmerCut;
                side.airLow += (dry - side.airLow) * airCut;
                const auto lightOut = shimmerLevel * (up - side.upLow) + airLevel * (dry - side.airLow);
                // DARK: the octave down, darkened; the sound darkened and roughened.
                const auto down = side.down.processSample (dry, 0.5f);
                side.downLow += (down - side.downLow) * growlTop;
                side.dryLow += (dry - side.dryLow) * growlTop;
                const auto darkened = dry + darkenAmount * darkMix * (side.dryLow - dry);
                const auto rough = darkened + darkMix * (std::tanh (drive * darkened) * driveNorm - darkened);   // dry exactly on the light side
                const auto out = rough + lightMix * lightOut + darkMix * growlLevel * std::tanh (2.0f * side.downLow);
                inPower += dry * dry;
                outPower += out * out;
                *io[c] = out * gain;
            }

            // The level kept.
            powerIn += (inPower - powerIn) * levelCoef;
            powerOut += (outPower - powerOut) * levelCoef;
            if (a > 0.0f && powerIn > 1.0e-9f && powerOut > 1.0e-12f)
                gain += (juce::jlimit (0.5f, 1.6f, std::sqrt (powerIn / powerOut)) - gain) * gainCoef;
            else if (a <= 0.0f)
                gain += (1.0f - gain) * gainCoef;
        }

        const auto endBeats = start + beatsPerSample * n;
        const auto pair = 2.0 * synth::crossroads::beatsPerSide (s.length);
        amountShown.store (amountNow, std::memory_order_relaxed);
        darknessShown.store (darkness, std::memory_order_relaxed);
        positionShown.store ((float) (endBeats / pair - std::floor (endBeats / pair)), std::memory_order_relaxed);
        gainShown.store (juce::Decibels::gainToDecibels (gain), std::memory_order_relaxed);

        // At AMOUNT 0 every layer is x 0 and the drive blended out; rest once the gain is back too.
        const auto settled = target <= 0.0f && amountNow <= 0.0f && std::abs (gain - 1.0f) < 1.0e-3f;
        settledSamples = settled ? settledSamples + n : 0;
        if (settled && settledSamples > (int) (0.3 * sampleRate))
        {
            const auto beats = ownBeats;
            reset();
            ownBeats = beats;
        }
    }
}
