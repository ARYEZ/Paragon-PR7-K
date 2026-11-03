#include "Haze.h"

#include <cmath>

namespace effects
{
    float Haze::makeUpFor (float depth) noexcept
    {
        if (depth <= 0.0f)
            return 1.0f;
        // The power of a pass, its passes spread evenly in time (x = -L cos pi p).
        static const auto table = []
        {
            std::array<float, 33> t {};
            for (size_t k = 0; k < t.size(); ++k)
            {
                const auto e = (float) k / (float) (t.size() - 1);
                double power = 0.0;
                constexpr int steps = 2048;
                for (int i = 0; i < steps; ++i)
                {
                    const auto g = (double) levelAt (-roadLength * std::cos (juce::MathConstants<double>::pi * (i + 0.5) / steps), e);
                    power += g * g;
                }
                t[k] = (float) (1.0 / std::sqrt (power / steps));
            }
            return t;
        }();
        const auto position = juce::jlimit (0.0f, 1.0f, depth) * (float) (table.size() - 1);
        const auto k = juce::jmin ((int) position, (int) table.size() - 2);
        return table[(size_t) k] + (position - (float) k) * (table[(size_t) k + 1] - table[(size_t) k]);
    }

    void Haze::prepare (double rate, int)
    {
        sampleRate = rate;
        lineSize = (int) std::ceil (0.5 * rate) + 8;   // the furthest delay: about 70 ms at 40 cents and 4 bars at 40 BPM
        for (auto& l : lines)
            l.assign ((size_t) lineSize, 0.0f);
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));   // 30 ms
        followUp = (float) (1.0 - std::exp (-1.0 / (0.01 * rate)));
        followDown = (float) (1.0 - std::exp (-1.0 / (0.6 * rate)));
        reset();
    }

    void Haze::reset() noexcept
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        writeIndex = 0;
        ownPasses = 0.0;
        delayScale = 0.0;
        depthNow = 0.0f;
        lastDelay = 0.0f;
        toneState = ic1 = ic2 = {};
        follow = whooshLevel = 0.0f;
        positionShown.store ((float) -roadLength, std::memory_order_relaxed);
        shiftShown.store (0.0f, std::memory_order_relaxed);
        levelShown.store (0.0f, std::memory_order_relaxed);
    }

    // `back` samples before the one just written, four-point Hermite; points not written yet repeat the newest.
    float Haze::read (int side, double back) const noexcept
    {
        const auto& line = lines[(size_t) side];
        const auto whole = (int) std::ceil (back);
        const auto f = (float) ((double) whole - back);
        const auto at = [&] (int k) { return line[(size_t) ((writeIndex - juce::jmax (0, k) + lineSize) % lineSize)]; };
        const auto xm1 = at (whole + 1), x0 = at (whole), x1 = at (whole - 1), x2 = at (whole - 2);
        if (f == 0.0f)
            return x0;
        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    void Haze::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        // DEPTH under a thousandth counts as 0 (the mod matrix eases a knob down and never lands exactly on 0).
        const auto depthTarget = s.on && s.depth >= 1.0e-3f ? juce::jmin (1.0f, s.depth) : 0.0f;
        const auto beatsPerPass = divisionBeats[(size_t) juce::jlimit (0, 3, s.division)];
        const auto passSeconds = beatsPerPass * 60.0 / juce::jmax (20.0, bpm);
        const auto passesPerSample = 1.0 / (passSeconds * sampleRate);
        // Where the passes are: from the song while it plays, its own count otherwise.
        auto passes = ppq >= 0.0 ? ppq / beatsPerPass : ownPasses;

        if (depthTarget <= 0.0f && isResting())
        {
            // Untouched; the line still records and the passes still count, so it wakes in step with the song.
            for (int i = 0; i < n; ++i)
            {
                lines[0][(size_t) writeIndex] = left[i];
                lines[1][(size_t) writeIndex] = right[i];
                writeIndex = writeIndex + 1 == lineSize ? 0 : writeIndex + 1;
            }
            ownPasses = passes + n * passesPerSample;
            positionShown.store ((float) (-roadLength * std::cos (juce::MathConstants<double>::pi * ownPasses)), std::memory_order_relaxed);
            shiftShown.store (0.0f, std::memory_order_relaxed);
            levelShown.store (0.0f, std::memory_order_relaxed);
            return;
        }

        // The delay per unit of distance, so the steepest pitch change is DOPPLER's cents (it moves at most
        // L pi / T road units a second).
        const auto scaleTarget = (double) dopplerCents (s.doppler) / 1731.0 * passSeconds / (roadLength * juce::MathConstants<double>::pi);
        const auto far = farHz (s.tone);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        const auto whoosh = s.on ? juce::jlimit (0.0f, 1.0f, s.whoosh) : 0.0f;
        auto lastShift = 0.0f, lastGain = 1.0f, lastX = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float in[2] = { left[i], right[i] };
            lines[0][(size_t) writeIndex] = in[0];
            lines[1][(size_t) writeIndex] = in[1];

            depthNow += (depthTarget - depthNow) * smoothing;
            if (depthTarget <= 0.0f && depthNow < 1.0e-5f)
                depthNow = 0.0f;
            delayScale += (scaleTarget - delayScale) * (double) smoothing;
            const auto e = depthNow;

            // Where the source is, and how near.
            const auto x = -roadLength * std::cos (juce::MathConstants<double>::pi * passes);
            const auto d = std::sqrt (x * x + closest * closest);
            const auto near = (float) (closest / d);
            const auto away = 1.0f - near;   // 0 passing, about 0.79 at the edges

            // Its distance as a delay: the Doppler.
            const auto delay = (float) ((double) e * delayScale * (d - closest) * sampleRate);
            auto out0 = e > 0.0f ? read (0, delay) : in[0];
            auto out1 = e > 0.0f ? read (1, delay) : in[1];

            // Darker and quieter further away; the balance leaning to its side.
            const auto cutoff = 20000.0f * std::pow (far / 20000.0f, e * away);
            const auto gain = (1.0f - e * away) * makeUpFor (e);
            const auto lean = (float) (e * width * 0.85 * x / d);
            const auto gainLeft = std::sqrt (1.0f - lean), gainRight = std::sqrt (1.0f + lean);
            if (cutoff < 19000.0f)
            {
                const auto c = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sampleRate);
                toneState[0] += c * (out0 - toneState[0]);
                toneState[1] += c * (out1 - toneState[1]);
                out0 = toneState[0];
                out1 = toneState[1];
            }
            else
                toneState = { out0, out1 };

            // WHOOSH: a breath of wind as it passes, following the music.
            const auto level = juce::jmax (std::abs (in[0]), std::abs (in[1]));
            follow += (level - follow) * (level > follow ? followUp : followDown);
            const auto presence = juce::jmin (1.0f, follow / 0.03f);
            whooshLevel = presence < 0.02f || whoosh <= 0.0f ? 0.0f : whoosh * e * near * near * 0.06f * presence;
            float wind[2] = { 0.0f, 0.0f };
            if (whooshLevel > 0.0f)
            {
                const auto g = std::tan (juce::MathConstants<float>::pi * (600.0f + 2400.0f * near) / (float) sampleRate);
                constexpr float k = 1.2f;
                const auto a1 = 1.0f / (1.0f + g * (g + k));
                for (int side = 0; side < 2; ++side)
                {
                    const auto white = 2.0f * random01() - 1.0f;
                    const auto v1 = a1 * (ic1[(size_t) side] + g * (white - ic2[(size_t) side]));
                    const auto v2 = ic2[(size_t) side] + g * v1;
                    ic1[(size_t) side] = 2.0f * v1 - ic1[(size_t) side];
                    ic2[(size_t) side] = 2.0f * v2 - ic2[(size_t) side];
                    wind[side] = whooshLevel * k * v1 * (side == 0 ? gainLeft : gainRight);
                }
            }
            else
                ic1 = ic2 = {};

            left[i] = out0 * gain * gainLeft + wind[0];
            if (stereo)
                right[i] = out1 * gain * gainRight + wind[1];

            lastShift = (float) (1200.0 * std::log2 (juce::jmax (0.01, 1.0 - (double) (delay - lastDelay))));
            lastDelay = delay;
            lastGain = gain;
            lastX = (float) x;
            writeIndex = writeIndex + 1 == lineSize ? 0 : writeIndex + 1;
            passes += passesPerSample;
        }
        ownPasses = passes;
        positionShown.store (lastX, std::memory_order_relaxed);
        shiftShown.store (lastShift, std::memory_order_relaxed);
        levelShown.store (juce::Decibels::gainToDecibels (lastGain, -100.0f), std::memory_order_relaxed);
    }
}
