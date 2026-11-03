#include "Spectre.h"

#include <cmath>

namespace effects
{
    void Spectre::prepare (double rate, int)
    {
        sampleRate = rate;
        for (int b = 0; b < numBands; ++b)
        {
            const auto hz = juce::jmin (bandHz (b), 0.45 * rate);
            analysis[(size_t) b].set (hz, analysisQ, rate);
            // Breath under 250 Hz is rumble: thinned towards the bottom band.
            weight[(size_t) b] = (float) std::pow (juce::jmin (1.0, hz / 250.0), 1.5);
            // Each band's own slow swing between the sides (0.05 to 0.2 Hz, spread so neighbours don't move together).
            swingRate[(size_t) b] = (float) (juce::MathConstants<double>::twoPi * (0.05 + 0.15 * std::fmod (b * 0.618034, 1.0)) / rate);
            swingPhase[(size_t) b] = (float) (juce::MathConstants<double>::twoPi * std::fmod (b * 0.381966, 1.0));
        }
        breathAir = -1.0f;
        attack = (float) (1.0 - std::exp (-1.0 / (0.004 * rate)));
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));
        lineSize = (int) std::ceil ((maxTrail + 0.05) * rate) + 4;
        for (auto& l : lines)
            l.assign ((size_t) lineSize, 0.0f);
        reset();
    }

    void Spectre::reset() noexcept
    {
        for (auto& side : analysisState)
            side.fill ({});
        for (auto& side : analysisState2)
            side.fill ({});
        for (auto& side : breathState)
            side.fill ({});
        for (auto& side : power)
            side.fill (0.0f);
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        writeIndex = 0;
        mixNow = 0.0f;
        trailNow = -1.0f;
        for (int b = 0; b < numBands; ++b)
        {
            bandShown[(size_t) b].store (-100.0f, std::memory_order_relaxed);
            ghostShown[(size_t) b].store (-100.0f, std::memory_order_relaxed);
        }
        ghostDb.store (-100.0f, std::memory_order_relaxed);
    }

    void Spectre::setBreath (float air) noexcept
    {
        breathAir = air;
        const auto q = (double) breathQ (air);
        for (int b = 0; b < numBands; ++b)
        {
            const auto hz = juce::jmin (bandHz (b), 0.45 * sampleRate);
            breath[(size_t) b].set (hz, q, sampleRate);
            // White noise (power 1/3 for -1..1) through a unity-peak band-pass keeps about pi f / (Q rate) of it:
            // the gain that brings each band back to unit power, so it comes out at the level it went in.
            breathGain[(size_t) b] = (float) std::sqrt (3.0 * q * sampleRate / (juce::MathConstants<double>::pi * hz));
        }
    }

    float Spectre::readLine (int side, double back) const noexcept
    {
        const auto& line = lines[(size_t) side];
        const auto whole = (int) back;
        const auto f = (float) (back - whole);
        const auto at = [&] (int k) { return line[(size_t) ((writeIndex - k + 2 * lineSize) % lineSize)]; };
        const auto a = at (whole), b = at (whole + 1);
        return a + f * (b - a);
    }

    float Spectre::getDriftOf (int band) const noexcept
    {
        return std::sin (swingPhase[(size_t) juce::jlimit (0, numBands - 1, band)]);
    }

    void Spectre::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        // MIX under a thousandth counts as 0 (the mod matrix eases a knob down and never lands exactly on 0).
        const auto mixTarget = s.on && s.mix >= 1.0e-3f ? juce::jmin (1.0f, s.mix) : 0.0f;
        if (mixTarget <= 0.0f && isResting())
            return;   // untouched; nothing kept, so it wakes clean
        if (s.air != breathAir)
            setBreath (s.air);

        const auto release = (float) (1.0 - std::exp (-std::log (1.0e6) / (lingerSeconds (s.linger) * sampleRate)));   // power down 60 dB
        const auto drift = juce::jlimit (0.0f, 1.0f, s.drift);
        const auto common = std::sqrt (1.0f - drift), own = std::sqrt (drift);
        const auto shiftBands = shiftSemitones (s.shift) / 4.0f;   // a band a third of an octave
        const auto trailTarget = (float) (maxTrail * juce::jlimit (0.0f, 1.0f, s.trail) * sampleRate);
        if (trailNow < 0.0f)
            trailNow = trailTarget;
        constexpr float makeUp = 0.62f;   // the bands overlap: a sound lands in two or three, measured on pink noise
        constexpr float floorPower = 1.0e-12f;   // -120 dB

        // Each band's place between the sides now (the swing moves slowly: once a block is plenty), and where each
        // ghost band reads its outline (SHIFT).
        std::array<float, numBands> toOther {}, readFrom {};
        for (int b = 0; b < numBands; ++b)
        {
            swingPhase[(size_t) b] = std::fmod (swingPhase[(size_t) b] + swingRate[(size_t) b] * (float) n, juce::MathConstants<float>::twoPi);
            toOther[(size_t) b] = drift * 0.5f * (0.5f + 0.5f * std::sin (swingPhase[(size_t) b]));   // up to half to the other side
            readFrom[(size_t) b] = (float) b - shiftBands;
        }

        double ghostPower = 0.0;
        std::array<double, numBands> shownIn {}, shownGhost {};
        for (int i = 0; i < n; ++i)
        {
            mixNow += (mixTarget - mixNow) * smoothing;
            if (mixTarget <= 0.0f && mixNow < 1.0e-5f)
                mixNow = 0.0f;
            trailNow += (trailTarget - trailNow) * smoothing * 0.15f;   // about 0.2 s to move

            const float in[2] = { left[i], right[i] };
            for (int side = 0; side < 2; ++side)
            {
                auto& st = analysisState[(size_t) side];
                auto& st2 = analysisState2[(size_t) side];
                auto& p = power[(size_t) side];
                for (int b = 0; b < numBands; ++b)
                {
                    const auto y = st2[(size_t) b].run (analysis[(size_t) b], st[(size_t) b].run (analysis[(size_t) b], in[side]));
                    const auto e = y * y;
                    auto& pb = p[(size_t) b];
                    pb += (e - pb) * (e > pb ? attack : release);
                    if (pb < floorPower)
                        pb = 0.0f;
                }
            }

            const auto shared = random();
            float ghost[2] = { 0.0f, 0.0f };
            for (int side = 0; side < 2; ++side)
            {
                const auto noise = common * shared + own * random();
                const auto& mine = power[(size_t) side];
                const auto& other = power[(size_t) (1 - side)];
                auto& st = breathState[(size_t) side];
                for (int b = 0; b < numBands; ++b)
                {
                    // The outline at this band's place (SHIFT reads between bands; outside the bank, nothing).
                    const auto from = readFrom[(size_t) b];
                    if (from < 0.0f || from > (float) (numBands - 1))
                    {
                        st[(size_t) b].run (breath[(size_t) b], noise);
                        continue;
                    }
                    const auto k = juce::jmin ((int) from, numBands - 2);
                    const auto f = from - (float) k;
                    const auto ownPower = mine[(size_t) k] + f * (mine[(size_t) k + 1] - mine[(size_t) k]);
                    const auto otherPower = other[(size_t) k] + f * (other[(size_t) k + 1] - other[(size_t) k]);
                    const auto t = toOther[(size_t) b];
                    const auto bandPower = (1.0f - t) * ownPower + t * otherPower;
                    const auto y = st[(size_t) b].run (breath[(size_t) b], noise);
                    if (bandPower <= 0.0f)
                        continue;
                    const auto level = std::sqrt (bandPower) * weight[(size_t) b];
                    ghost[side] += y * breathGain[(size_t) b] * level;
                    if (side == 0)
                        shownGhost[(size_t) b] += (double) (level * level);
                }
            }
            if ((i & 31) == 0)
                for (int b = 0; b < numBands; ++b)
                    shownIn[(size_t) b] += 0.5 * ((double) power[0][(size_t) b] + (double) power[1][(size_t) b]);

            // Into the trail, and out from TRAIL behind.
            lines[0][(size_t) writeIndex] = ghost[0] * makeUp;
            lines[1][(size_t) writeIndex] = ghost[1] * makeUp;
            const auto back = (double) juce::jlimit (0.0f, (float) (lineSize - 3), trailNow);
            const auto out0 = readLine (0, back), out1 = readLine (1, back);
            writeIndex = writeIndex + 1 == lineSize ? 0 : writeIndex + 1;

            left[i] = in[0] + mixNow * out0;
            if (stereo)
                right[i] = in[1] + mixNow * out1;
            ghostPower += 0.5 * ((double) out0 * out0 + (double) out1 * out1) * (double) (mixNow * mixNow);
        }

        const auto counted = (double) ((n + 31) / 32);
        for (int b = 0; b < numBands; ++b)
        {
            bandShown[(size_t) b].store ((float) (10.0 * std::log10 (shownIn[(size_t) b] / counted + 1.0e-12)), std::memory_order_relaxed);
            ghostShown[(size_t) b].store ((float) (10.0 * std::log10 (shownGhost[(size_t) b] / juce::jmax (1, n) + 1.0e-12)), std::memory_order_relaxed);
        }
        ghostDb.store ((float) (10.0 * std::log10 (ghostPower / juce::jmax (1, n) + 1.0e-12)), std::memory_order_relaxed);

        if (mixNow <= 0.0f)
        {
            // Asleep again: let go of everything so it wakes clean.
            for (auto& side : power)
                side.fill (0.0f);
            for (auto& side : analysisState)
                side.fill ({});
            for (auto& side : analysisState2)
                side.fill ({});
            for (auto& side : breathState)
                side.fill ({});
            for (auto& l : lines)
                std::fill (l.begin(), l.end(), 0.0f);
            trailNow = -1.0f;
        }
    }
}
