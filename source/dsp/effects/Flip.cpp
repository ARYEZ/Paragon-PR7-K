#include "Flip.h"

#include <cmath>

namespace effects
{
    namespace
    {
        struct BarRandom   // a fixed number for a bar (the same bar always flips the same way)
        {
            std::uint32_t state;
            explicit BarRandom (std::int64_t bar) noexcept
                : state ((std::uint32_t) ((std::uint64_t) (bar + 7000003) * 2246822519ull) ^ 0x27d4eb2fu)
            {
                if (state == 0)
                    state = 0x9e3779b9u;
                for (int k = 0; k < 3; ++k)
                    next();
            }
            float next() noexcept
            {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                return (float) (state >> 8) / 16777216.0f;
            }
        };

        constexpr int chopOrder[16] = { 0, 0, 2, 1, 0, 3, 3, 2, 0, 0, 1, 1, 2, 2, 3, 3 };   // the sketch's re-chop
    }

    int Flip::sixteenthsFor (float amount, std::int64_t bar) noexcept
    {
        const auto odd = (bar & 1) != 0;
        if (amount < 0.001f)
            return 0;
        if (amount <= 1.0f / 3.0f)
            return odd ? 4 : 0;
        if (amount <= 2.0f / 3.0f)
            return 8;
        if (odd)
            return 16;
        return 8 + 2 * juce::roundToInt ((amount - 2.0f / 3.0f) * 3.0f * 4.0f);
    }

    Flip::Kind Flip::kindOf (std::int64_t bar) noexcept
    {
        BarRandom r (bar);
        struct Choice { Kind kind; float weight; };
        constexpr Choice choices[] = { { Kind::loop, 2.0f }, { Kind::up, 1.5f }, { Kind::down, 1.5f }, { Kind::reverse, 1.0f }, { Kind::chop, 2.0f } };
        auto pick = r.next() * 8.0f;
        for (const auto& c : choices)
        {
            if (pick < c.weight)
                return c.kind;
            pick -= c.weight;
        }
        return Kind::chop;
    }

    Flip::Plan Flip::planBar (std::int64_t bar, float amount, float pitch) noexcept
    {
        Plan p;
        const auto span = sixteenthsFor (amount, bar);
        if (span <= 0)
            return p;
        BarRandom r (bar * 31 + 5);
        const auto odd = (bar & 1) != 0;
        p.kind = kindOf (bar);
        // The second bar of a pair ends on a stutter into the bar line: always in a lone fill (from AMOUNT 1/6), often
        // in a longer stretch.
        p.stutterEnd = odd && (span == 4 ? amount >= 1.0f / 6.0f : r.next() < 0.6f);
        const auto end = p.stutterEnd ? 12.0f : 16.0f;
        const auto add = [&p] (float start, float length, bool fromB, bool reversed, float semitones, float offset)
        {
            if (p.count < maxCells)
                p.cells[(size_t) p.count++] = { start, length, fromB, reversed, semitones, offset };
        };
        auto at = (float) (16 - span);
        int k = 0;
        while (at < end - 0.01f)
        {
            switch (p.kind)
            {
                case Kind::loop:
                    add (at, juce::jmin (4.0f, end - at), false, false, 0.0f, 0.0f);
                    at += 4.0f;
                    break;
                case Kind::up:
                    add (at, juce::jmin (2.0f, end - at), false, false, (k % 2 == 0) ? pitch : pitch + 2.0f, 0.0f);
                    at += 2.0f;
                    break;
                case Kind::down:
                    add (at, juce::jmin (4.0f, end - at), k % 2 == 1, k % 2 == 1, -pitch, 0.0f);
                    at += 4.0f;
                    break;
                case Kind::reverse:
                    add (at, juce::jmin (4.0f, end - at), true, true, 0.0f, 0.0f);
                    at += 4.0f;
                    break;
                case Kind::chop:
                    add (at, 1.0f, false, false, 0.0f, (float) chopOrder[juce::roundToInt (at) % 16]);
                    at += 1.0f;
                    break;
                case Kind::stutter:
                default:
                    add (at, 0.5f, false, false, 0.0f, 0.0f);
                    at += 0.5f;
                    break;
            }
            ++k;
        }
        if (p.stutterEnd)
            for (int s = 0; s < 8; ++s)
                add (12.0f + 0.5f * (float) s, 0.5f, false, false, s >= 6 ? 12.0f : 0.0f, 0.0f);
        return p;
    }

    void Flip::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        size = (int) std::ceil (historySeconds * sampleRate) + 8;
        for (auto& side : history)
            side.assign ((size_t) size, 0.0f);
        reset();
    }

    void Flip::reset() noexcept
    {
        // The recording isn't cleared (it's large); with the count back at 0 nothing older is read.
        write = 0;
        clock = 0;
        ownBeat = 0.0;
        lastBeat = -1.0e9;
        planBarIndex = -1;
        plan = {};
        nextCell = 0;
        grabbed = false;
        active = false;
        current = -1;
        gate = 0.0f;
        holdPhase = 1.0;
        held.fill (0.0f);
        lowIn.fill (0.0f);
        lowOut.fill (0.0f);
        inputPower = 0.0f;
        wetPower = 0.05f;
        wetGain = 1.0f;
        resting = true;
        kindNow.store (-1, std::memory_order_relaxed);
        cellNow.store (-1, std::memory_order_relaxed);
        shownCount.store (0, std::memory_order_relaxed);
    }

    float Flip::readAt (int channel, double position) const noexcept
    {
        position = juce::jlimit ((double) juce::jmax<std::int64_t> (0, clock - size + 2), (double) juce::jmax<std::int64_t> (0, clock - 2), position);
        const auto whole = (std::int64_t) std::floor (position);
        const auto f = (float) (position - (double) whole);
        const auto& buffer = history[(size_t) channel];
        const auto a = buffer[(size_t) (whole % size)], b = buffer[(size_t) ((whole + 1) % size)];
        return a + (b - a) * f;
    }

    void Flip::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = juce::jmin (2, (int) block.getNumChannels());
        if (n <= 0 || channels <= 0)
            return;
        if (! s.on && resting)
        {
            if (clock != 0)
                reset();
            return;
        }
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto amount = s.on ? s.amount : 0.0f;
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        const auto sixteenthSamples = 0.25 / beatsPerSample;
        const auto beatSeconds = 60.0 / juce::jmax (20.0, bpm);
        const auto playing = ppq >= 0.0;
        auto beat = playing ? ppq : ownBeat;
        if (lastBeat > -1.0e8 && (beat < lastBeat - 0.5 || beat > lastBeat + 2.0))
        {
            planBarIndex = -1;
            active = false;
        }
        const auto holdStep = rateFor (s.crunch) / sampleRate;
        const auto levels = std::exp2 (bitsFor (s.crunch) - 1.0f);
        const auto pump = juce::jlimit (0.0f, 1.0f, s.pump);
        const auto mix = juce::jlimit (0.0f, 1.0f, s.mix);
        const auto fade = 0.0015 * sampleRate;
        const auto gateStep = 1.0f / (float) (0.003 * sampleRate);
        const auto inputFollow = (float) (1.0 - std::exp (-1.0 / (0.4 * sampleRate)));   // an average, as SCRATCH's
        const auto wetFollow = (float) (1.0 - std::exp (-1.0 / (0.3 * sampleRate)));
        const auto gainGlide = (float) (1.0 - std::exp (-1.0 / (0.05 * sampleRate)));
        const auto lowPole = (float) std::exp (-juce::MathConstants<double>::twoPi * 60.0 / sampleRate);

        for (int i = 0; i < n; ++i)
        {
            const float x[2] = { left[i], right != nullptr ? right[i] : left[i] };
            for (int c = 0; c < channels; ++c)
                history[(size_t) c][(size_t) write] = x[c];
            write = write + 1 < size ? write + 1 : 0;
            ++clock;
            const auto power = 0.5f * (x[0] * x[0] + x[channels > 1 ? 1 : 0] * x[channels > 1 ? 1 : 0]);
            inputPower += (power - inputPower) * inputFollow;

            const auto bar = (std::int64_t) std::floor (beat / 4.0);
            const auto sixteenthNow = (beat - (double) bar * 4.0) * 4.0;
            if (bar != planBarIndex)
            {
                planBarIndex = bar;
                plan = planBar (bar, amount, s.pitch);
                nextCell = 0;
                // The grabs: the bar just played's first and third beats (the record holds what was played, flipped
                // or not). Not held that far back yet: this bar plays as it is.
                const auto barStart = (double) clock - sixteenthNow * sixteenthSamples;
                grabA = barStart - 16.0 * sixteenthSamples;
                grabB = grabA + 8.0 * sixteenthSamples;
                grabbed = grabA > -2.0 && grabA >= (double) (clock - size + 8);
                if (! grabbed)
                    plan.count = 0;
                while (nextCell < plan.count && sixteenthNow > plan.cells[(size_t) nextCell].start + 0.25)
                    ++nextCell;
                shownCount.store (plan.count, std::memory_order_relaxed);
                for (int k = 0; k < plan.count; ++k)
                {
                    shownStart[(size_t) k].store (plan.cells[(size_t) k].start, std::memory_order_relaxed);
                    shownLength[(size_t) k].store (plan.cells[(size_t) k].length, std::memory_order_relaxed);
                    shownSemis[(size_t) k].store (plan.cells[(size_t) k].semitones, std::memory_order_relaxed);
                    shownReversed[(size_t) k].store (plan.cells[(size_t) k].reversed, std::memory_order_relaxed);
                }
            }
            if (nextCell < plan.count && sixteenthNow >= plan.cells[(size_t) nextCell].start)
            {
                const auto& cell = plan.cells[(size_t) nextCell];
                current = nextCell++;
                cellAge = 0.0;
                cellLength = cell.length * sixteenthSamples;
                speed = std::exp2 (cell.semitones / 12.0);
                from = (cell.fromB ? grabB : grabA) + cell.offset * sixteenthSamples;
                grabLength = (4.0 - cell.offset) * sixteenthSamples;   // the rest of the grabbed beat
                reversed = cell.reversed;
                active = true;
                flips.fetch_add (1, std::memory_order_relaxed);
            }
            const auto beatPhase = beat - std::floor (beat);
            beat += beatsPerSample;

            if (! active && gate <= 0.0f)
            {
                resting = true;
                continue;
            }
            resting = false;

            // The pad playing: the grab from its start (or end, backwards) at the sampler's speed, faded in and out.
            float env = 0.0f;
            double position = from;
            if (active)
            {
                const auto read = cellAge * speed;
                const auto lasts = juce::jmin (cellLength, (grabLength - 3.0) / speed);
                if (cellAge < lasts)
                {
                    env = (float) juce::jmin (1.0, juce::jmin (cellAge / fade, (lasts - cellAge) / fade));
                    position = reversed ? from + grabLength - 3.0 - read : from + read;
                }
                cellAge += 1.0;
                if (cellAge >= cellLength && nextCell >= plan.count)
                    active = false;
                else if (cellAge >= cellLength && plan.cells[(size_t) nextCell].start > sixteenthNow + 0.5)
                    active = false;
            }
            gate += juce::jlimit (-gateStep, gateStep, (active ? 1.0f : 0.0f) - gate);

            // The sampler: held at its rate, stepped to its bits; then its low cut, a soft squash and the pump.
            holdPhase += holdStep;
            const auto take = holdPhase >= 1.0;
            if (take)
                holdPhase -= std::floor (holdPhase);
            const auto duck = 1.0f - pump * (float) std::exp (-beatPhase * beatSeconds / 0.09);
            float out[2] {};
            float wetSum = 0.0f;
            for (int c = 0; c < channels; ++c)
            {
                if (take)
                    held[(size_t) c] = std::round (readAt (c, position) * env * levels) / levels;
                auto w = held[(size_t) c];
                lowOut[(size_t) c] = w - lowIn[(size_t) c] + lowPole * lowOut[(size_t) c];
                lowIn[(size_t) c] = w;
                w = std::tanh (lowOut[(size_t) c] * 2.2f) / 2.2f * duck;
                out[c] = w;
                wetSum += w * w;
            }
            if (active && env > 0.99f)   // only while a pad is sounding: its gaps would read as a quiet flip
                wetPower += (wetSum / (float) channels - wetPower) * wetFollow;
            const auto ref = std::sqrt (inputPower);
            const auto target = juce::jlimit (0.25f, 4.0f, ref / std::sqrt (juce::jmax (1.0e-9f, wetPower)));
            wetGain += (target - wetGain) * gainGlide;
            const auto dry = 1.0f - mix * gate;
            left[i] = x[0] * dry + out[0] * wetGain * gate;
            if (right != nullptr)
                right[i] = x[1] * dry + out[1] * wetGain * gate;
            if (! active && gate <= 0.0f)
                resting = true;
        }
        ownBeat = beat;
        lastBeat = beat;
        kindNow.store (active && current >= 0 ? (plan.stutterEnd && plan.cells[(size_t) current].start >= 12.0f ? (int) Kind::stutter : (int) plan.kind) : -1,
                       std::memory_order_relaxed);
        cellNow.store (active ? current : -1, std::memory_order_relaxed);
        barNow.store ((float) ((beat - std::floor (beat / 4.0) * 4.0) * 4.0), std::memory_order_relaxed);
    }
}
