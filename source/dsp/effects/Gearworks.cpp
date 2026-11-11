#include "Gearworks.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;   // AMOUNT's smoothing
        constexpr float cutInSeconds = 0.0015f;  // a tooth's bite
        constexpr float cutOutSeconds = 0.004f;  // and its release
        constexpr float biteDepth = 0.92f;       // BITE 1 cuts to 8%
        constexpr float maxMakeUp = 2.0f;        // +6 dB at most
        constexpr float metalDepth = 0.65f;
        constexpr float steamSeconds = 0.08f;    // a hiss falls 60 dB over this
        constexpr float steamHz = 4500.0f, steamDamp = 1.4f, steamLevel = 0.9f;
        constexpr float clankLevel = 0.9f;
        constexpr std::array<float, 2> clankHz { 1200.0f, 760.0f };
        constexpr std::array<float, 3> partialRatio { 1.0f, 2.76f, 5.40f };
        constexpr std::array<float, 3> partialLevel { 1.0f, 0.5f, 0.3f };
        constexpr std::array<float, 3> partialSeconds { 0.09f, 0.05f, 0.03f };   // to fall 60 dB
        constexpr std::array<std::array<float, 2>, 2> clankPan { { { 1.0f, 0.7f }, { 0.7f, 1.0f } } };
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        float noteHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
    }

    uint32_t Gearworks::nextRandom() noexcept
    {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return random;
    }

    void Gearworks::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto sr = (float) sampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * sr));
        cutIn = 1.0f / (cutInSeconds * sr);
        cutOut = 1.0f / (cutOutSeconds * sr);
        noteCoef = 1.0f - std::exp (-1.0f / (0.02f * sr));
        steamFall = std::exp (-6.9f / (steamSeconds * sr));
        followUp = 1.0f - std::exp (-1.0f / (0.01f * sr));
        followDown = 1.0f - std::exp (-1.0f / (0.4f * sr));
        reset();
    }

    void Gearworks::reset() noexcept
    {
        resting = true;
        amountNow = 0.0f;
        ownBeats = 0.0;
        lastStep = -1;
        cut = 0.0f;
        teethNow = 0;
        metalNote = lowestNote;
        metalPhase = 0.0;
        grindCount = 0;
        steam = 0.0f;
        follower = 0.0f;
        sides = {};
        partials = {};
        settledSamples = 0;
        amountShown.store (0.0f);
        turnShown.store (0.0f);
        clankShown.store (0.0f);
        stepShown.store (0);
        teethShown.store (0);
    }

    void Gearworks::setNotes (const float* notes, int count) noexcept
    {
        if (count <= 0)
            return;   // none sounding: keep the last
        auto lowest = notes[0];
        for (int k = 1; k < count; ++k)
            lowest = juce::jmin (lowest, notes[k]);
        lowestNote = lowest;
    }

    void Gearworks::strike (int teeth, float strength) noexcept
    {
        const auto sr = (float) sampleRate;
        for (int gear = 0; gear < 2; ++gear)
        {
            if ((teeth & (1 << gear)) == 0)
                continue;
            for (int k = 0; k < 3; ++k)
            {
                auto& p = partials[(size_t) (gear * 3 + k)];
                const auto hz = juce::jmin (0.45f * sr, clankHz[(size_t) gear] * partialRatio[(size_t) k]);
                p.step = twoPi * hz / sampleRate;
                p.phase = 0.0;
                p.level = strength * partialLevel[(size_t) k];
                p.fall = std::exp (-6.9f / (partialSeconds[(size_t) k] * sr));
            }
        }
        clanksShown.store (clanksShown.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }

    void Gearworks::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        if (resting)
        {
            // Untouched, bit for bit; the clock goes on.
            ownBeats += beatsPerSample * n;
            if (target <= 0.0f)
                return;
            resting = false;
            amountNow = 0.0f;
            cut = 0.0f;
            lastStep = -1;
            follower = 0.0f;
            sides[0].follow = sides[1].follow = 0.0f;
            steam = 0.0f;
            partials = {};
            metalNote = lowestNote;
            settledSamples = 0;
        }
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;

        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto sr = (float) sampleRate;
        const auto ratio = juce::jlimit (0, (int) ratios.size() - 1, s.ratio);
        const auto stepBeats = divisionBeats[(size_t) juce::jlimit (0, (int) divisionBeats.size() - 1, s.division)];
        const auto share = cutShare (ratio);
        const auto bite = juce::jlimit (0.0f, 1.0f, s.bite);
        const auto metal = juce::jlimit (0.0f, 1.0f, s.metal);
        const auto grind = juce::jlimit (0.0f, 1.0f, s.grind);
        const auto steamAmount = juce::jlimit (0.0f, 1.0f, s.steam);
        const auto clank = juce::jlimit (0.0f, 1.0f, s.clank);
        const auto steamG = std::tan (juce::MathConstants<float>::pi * juce::jmin (0.45f * sr, steamHz) / sr);
        const auto steamA = 1.0f / (1.0f + steamG * (steamG + steamDamp));

        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountCoef;
            if (target <= 0.0f && amountNow < 1.0e-5f)
                amountNow = 0.0f;

            // THE GEARS: a new step, and maybe a tooth.
            const auto beats = start + beatsPerSample * i;
            const auto step = (long long) std::floor (beats / stepBeats);
            if (step != lastStep)
            {
                lastStep = step;
                teethNow = teethAt (step, ratio);
                if (teethNow != 0 && amountNow > 0.0f)
                {
                    strike (teethNow, clank * amountNow * clankLevel * follower);
                    steam = 1.0f;
                }
            }
            cut = teethNow != 0 ? juce::jmin (1.0f, cut + cutIn) : juce::jmax (0.0f, cut - cutOut);
            steam *= steamFall;

            const float dry[] = { left[i], right != nullptr ? right[i] : left[i] };
            for (int c = 0; c < 2; ++c)
            {
                auto& f = sides[(size_t) c].follow;
                const auto level = std::abs (dry[c]);
                f += (level - f) * (level > f ? followUp : followDown);
            }
            follower = juce::jmax (sides[0].follow, sides[1].follow);

            // What each part does now (all of it x AMOUNT, so it eases in and out).
            const auto depth = biteDepth * bite * amountNow;
            const auto meanPower = (1.0f - share) + share * (1.0f - depth) * (1.0f - depth);
            const auto gateMakeUp = juce::jmin (maxMakeUp, 1.0f / std::sqrt (juce::jmax (1.0e-6f, meanPower)));
            const auto gain = (1.0f - depth * cut) * gateMakeUp;
            const auto m = metalDepth * metal * amountNow;
            const auto metalMakeUp = 1.0f / std::sqrt (juce::jmax (1.0e-6f, (1.0f - m) * (1.0f - m) + 0.5f * m * m));
            metalNote += (lowestNote - metalNote) * noteCoef;
            auto metalHz = metalRatio * noteHz (metalNote);
            while (metalHz > 0.42f * sr)
                metalHz *= 0.5f;
            metalPhase += twoPi * metalHz / sampleRate;
            if (metalPhase > twoPi)
                metalPhase -= twoPi;
            const auto ring = (1.0f - m + m * (float) std::cos (metalPhase)) * metalMakeUp;
            const auto g = grind * amountNow;
            const auto holdFor = 1 + (int) (7.0f * g);
            const auto levels = std::exp2 (12.0f - 8.0f * g);
            const auto grindNow = (grindCount++ % holdFor) == 0;
            const auto steamNow = steam * steamAmount * amountNow * steamLevel;

            // CLANK: the struck partials.
            std::array<float, 2> clang { 0.0f, 0.0f };
            for (int k = 0; k < 6; ++k)
            {
                auto& p = partials[(size_t) k];
                if (p.level < 1.0e-6f)
                    continue;
                const auto v = p.level * (float) std::sin (p.phase);
                p.phase += p.step;
                if (p.phase > twoPi)
                    p.phase -= twoPi;
                p.level *= p.fall;
                const auto& pan = clankPan[(size_t) (k / 3)];
                clang[0] += v * pan[0];
                clang[1] += v * pan[1];
            }

            float* io[] = { &left[i], right != nullptr ? &right[i] : nullptr };
            for (int c = 0; c < 2; ++c)
            {
                if (io[c] == nullptr)
                    continue;
                auto& side = sides[(size_t) c];
                auto y = dry[c] * ring;
                if (grindNow)
                    side.held = std::round (juce::jlimit (-1.0f, 1.0f, y) * levels) / levels;
                y += (side.held - y) * juce::jmin (1.0f, 1.2f * g);
                y *= gain;
                // STEAM: noise through a band-pass (TPT SVF), on the cuts.
                const auto x = noise() * steamNow * side.follow;   // follows this side: a silent side stays silent
                const auto hp = (x - (steamDamp + steamG) * side.steamBand - side.steamLow) * steamA;
                const auto bp = steamG * hp + side.steamBand;
                side.steamBand = steamG * hp + bp;
                const auto lp = steamG * bp + side.steamLow;
                side.steamLow = steamG * bp + lp;
                const auto heard = follower > 1.0e-9f ? side.follow / follower : 0.0f;
                *io[c] = y + bp * steamDamp + clang[(size_t) c] * heard;   // k x band: a unity-peak band-pass
            }
        }

        const auto turn = turnSteps (ratio);
        const auto endBeats = start + beatsPerSample * n;
        const auto endStep = (long long) std::floor (endBeats / stepBeats);
        float clankNow = 0.0f;
        for (const auto& p : partials)
            clankNow = juce::jmax (clankNow, p.level);
        amountShown.store (amountNow, std::memory_order_relaxed);
        stepShown.store ((int) (endStep % turn), std::memory_order_relaxed);
        teethShown.store (teethNow, std::memory_order_relaxed);
        turnShown.store ((float) std::fmod (endBeats / stepBeats, (double) turn) / (float) turn, std::memory_order_relaxed);
        clankShown.store (clankNow, std::memory_order_relaxed);

        const auto settled = target <= 0.0f && amountNow <= 0.0f && clankNow < 1.0e-6f;
        settledSamples = settled ? settledSamples + n : 0;
        if (settled && settledSamples > (int) (0.3 * sampleRate))
            resting = true;   // the clock goes on
    }
}
