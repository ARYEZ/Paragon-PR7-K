#include "JackIn.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;     // AMOUNT's smoothing
        constexpr double fadeSeconds = 0.0025;     // each change's crossfade
        constexpr double edgeSeconds = 0.001;      // a stutter's repeats faded at their edges
        constexpr float stopChance = 0.2f;         // DROPS at full: a fifth of the beats (at full AMOUNT) end in a stop
        constexpr double stopShare = 0.92;         // a stop slows to nothing over this much of what's left of the beat
        constexpr double stopFadeShare = 0.12;     // and fades out over its last part (no frozen sample left hanging)
    }

    void JackIn::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * (float) sampleRate));
        fadeStep = 1.0 / (fadeSeconds * sampleRate);
        const auto size = (size_t) std::ceil (historySeconds * sampleRate) + 8;
        for (auto& h : history)
            h.assign (size, 0.0f);
        reset();
    }

    void JackIn::reset() noexcept
    {
        amountNow = 0.0f;
        ownBeats = 0.0;
        lastSlice = -1;
        holdSlices = 0;
        awakeSlices = 0;
        for (auto& h : history)
            std::fill (h.begin(), h.end(), 0.0f);
        written = 0;
        current = Player {};
        current.active = true;
        current.gain = 1.0f;
        previous = Player {};
        resting = true;
        amountShown.store (0.0f);
        slicesShown.store (0);
        for (auto& m : moves)
            m.store (0);
        for (auto& c : counts)
            c.store (0);
    }

    float JackIn::readAt (int channel, double position) const noexcept
    {
        // Four-point Hermite from the recording, `position` an absolute sample index.
        const auto& h = history[(size_t) channel];
        const auto size = (std::int64_t) h.size();
        const auto whole = (std::int64_t) std::floor (position);
        const auto frac = (float) (position - (double) whole);
        const auto at = [&h, size] (std::int64_t i) { return h[(size_t) (((i % size) + size) % size)]; };
        const auto ym1 = at (whole - 1), y0 = at (whole), y1 = at (whole + 1), y2 = at (whole + 2);
        const auto c1 = 0.5f * (y1 - ym1);
        const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + y0;
    }

    void JackIn::startSlice (std::int64_t now, std::int64_t sliceIndex, double sliceLength, double slicesPerBeat, const Settings& s)
    {
        const auto keep = [this] (Move m)
        {
            const auto n = slicesShown.load (std::memory_order_relaxed);
            moves[(size_t) (n % maxHistoryKept)].store ((int) m, std::memory_order_relaxed);
            slicesShown.store (n + 1, std::memory_order_relaxed);
        };
        if (holdSlices > 0)
        {
            // A tape stop runs on to the end of its beat.
            --holdSlices;
            keep (current.move);
            return;
        }

        const auto perBeat = juce::jmax (1, juce::roundToInt (slicesPerBeat));
        const auto onBeat = sliceIndex % perBeat == 0;
        const auto chaos = juce::jlimit (0.0f, 1.0f, s.chaos);
        auto move = Move::dry;
        if (onBeat && perBeat > 1 && s.drops > 0.0f && uniform() < stopChance * juce::jlimit (0.0f, 1.0f, s.drops) * amountNow)
            move = Move::stop;
        else if (uniform() < takeChance (amountNow))
        {
            // Low CHAOS: replays and stutters; high: reverses and octaves too.
            const std::array<float, 5> weights { 1.0f, 0.7f, 0.1f + 0.9f * chaos, 0.05f + 0.65f * chaos, 0.05f + 0.65f * chaos };
            const std::array<Move, 5> kinds { Move::replay, Move::stutter, Move::reverse, Move::up, Move::down };
            auto total = 0.0f;
            for (auto w : weights)
                total += w;
            auto pick = uniform() * total;
            move = kinds.back();
            for (size_t k = 0; k < weights.size(); ++k)
            {
                if (pick < weights[k])
                {
                    move = kinds[k];
                    break;
                }
                pick -= weights[k];
            }
        }
        keep (move);
        counts[(size_t) move].fetch_add (1, std::memory_order_relaxed);
        if (move == Move::dry && current.move == Move::dry)
            return;   // already untouched

        previous = current;
        Player p;
        p.move = move;
        p.active = true;
        p.gain = 0.0f;
        p.length = sliceLength;
        if (move == Move::stop)
        {
            holdSlices = perBeat - 1 - (int) (((sliceIndex % perBeat) + perBeat) % perBeat);
            p.start = (double) now - 4.0;
            p.stopLength = juce::jmax (1.0, stopShare * sliceLength * (holdSlices + 1));
        }
        else if (move != Move::dry)
        {
            // Where from: low CHAOS whole beats back (the slice keeps its place in the beat), else anywhere in REACH.
            const auto historyLength = (double) history[0].size();
            const auto reachBeat = reachBeats[(size_t) juce::jlimit (0, (int) reachBeats.size() - 1, s.reach)];
            auto maxSlices = juce::jmax (1, (int) std::floor (reachBeat * perBeat + 1.0e-9));
            maxSlices = juce::jmin (maxSlices, juce::jmax (1, (int) std::floor ((historyLength - 3.0 * sliceLength - 16.0) / sliceLength)));
            int back;
            if (uniform() >= chaos)
            {
                const auto beats = juce::jmax (1, maxSlices / perBeat);
                back = juce::jmin (maxSlices, (int) (1 + next() % (std::uint32_t) beats) * perBeat);
            }
            else
                back = 1 + (int) (next() % (std::uint32_t) maxSlices);
            if (move == Move::up)
                back = juce::jmax (back, 2);
            p.start = (double) now - back * sliceLength;
            p.loop = sliceLength / (uniform() < 0.5f ? 2.0 : 4.0);
            if (uniform() < juce::jlimit (0.0f, 1.0f, s.crush))
            {
                const auto c = juce::jlimit (0.0f, 1.0f, s.crush);
                p.hold = 1 + (int) (uniform() * 7.0f * c);
                const auto bits = 12.0f - 9.0f * c * (0.5f + 0.5f * uniform());
                p.levels = std::exp2 (bits - 1.0f);
            }
        }
        current = p;
    }

    void JackIn::playerValue (Player& p, std::int64_t now, float inL, float inR, float& outL, float& outR) noexcept
    {
        if (p.move == Move::dry)
        {
            outL = inL;
            outR = inR;
            return;
        }
        auto position = 0.0;
        auto window = 1.0;
        const auto t = p.t;
        switch (p.move)
        {
            case Move::replay:  position = p.start + t; break;
            case Move::reverse: position = p.start + p.length - 1.0 - t; break;
            case Move::up:      position = p.start + 2.0 * t; break;
            case Move::down:    position = p.start + 0.5 * t; break;
            case Move::stutter:
            {
                const auto phase = std::fmod (t, p.loop);
                position = p.start + phase;
                const auto edge = edgeSeconds * sampleRate;
                window = juce::jlimit (0.0, 1.0, juce::jmin (phase / edge, (p.loop - phase) / edge));
                break;
            }
            case Move::stop:
            {
                const auto tt = juce::jmin (t, p.stopLength);
                position = p.start + tt - tt * tt / (2.0 * p.stopLength);
                window = juce::jlimit (0.0, 1.0, (p.stopLength - t) / (stopFadeShare * p.stopLength));
                break;
            }
            case Move::dry: break;
        }
        const auto size = (double) history[0].size();
        position = juce::jlimit ((double) now - size + 4.0, (double) now - 2.0, position);
        auto l = readAt (0, position), r = readAt (1, position);
        if (p.hold > 1)
        {
            if (p.held == 0)
                p.heldValue = { l, r };
            p.held = (p.held + 1) % p.hold;
            l = p.heldValue[0];
            r = p.heldValue[1];
        }
        if (p.levels > 0.0f)
        {
            l = std::floor (l * p.levels + 0.5f) / p.levels;
            r = std::floor (r * p.levels + 0.5f) / p.levels;
        }
        outL = l * (float) window;
        outR = r * (float) window;
        p.t += 1.0;
    }

    void JackIn::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0 || history[0].empty())
            return;
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;
        const auto first = written;

        // It always records (so it wakes with the past to hand).
        const auto size = (std::int64_t) history[0].size();
        for (int i = 0; i < n; ++i)
        {
            const auto at = (size_t) ((first + i) % size);
            history[0][at] = left[i];
            history[1][at] = right != nullptr ? right[i] : left[i];
        }
        written += n;

        const auto target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        if (resting && target <= 0.0f)
        {
            lastSlice = -1;
            return;   // untouched, bit for bit
        }
        if (resting)
        {
            awakeSlices = 0;
            slicesShown.store (0);
            for (auto& m : moves)
                m.store (0);
        }
        resting = false;

        const auto sliceBeat = sliceBeats[(size_t) juce::jlimit (0, (int) sliceBeats.size() - 1, s.slice)];
        const auto sliceLength = sliceBeat * 60.0 / juce::jmax (20.0, bpm) * sampleRate;
        const auto slicesPerBeat = 1.0 / sliceBeat;
        perBarShown.store (juce::roundToInt (4.0 * slicesPerBeat), std::memory_order_relaxed);

        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountCoef;
            if (target <= 0.0f && amountNow < 1.0e-5f)
                amountNow = 0.0f;
            const auto now = first + i;
            const auto beats = start + beatsPerSample * i;
            const auto slice = (std::int64_t) std::floor (beats / sliceBeat + 1.0e-9);
            if (slice != lastSlice)
            {
                if (lastSlice >= 0)
                    startSlice (now, slice, sliceLength, slicesPerBeat, s);
                lastSlice = slice;
            }

            const auto inL = left[i], inR = right != nullptr ? right[i] : inL;
            float curL = 0.0f, curR = 0.0f;
            playerValue (current, now, inL, inR, curL, curR);
            if (current.gain < 1.0f)
                current.gain = (float) juce::jmin (1.0, current.gain + fadeStep);
            auto outL = current.gain >= 1.0f ? curL : curL * current.gain;
            auto outR = current.gain >= 1.0f ? curR : curR * current.gain;
            if (previous.active)
            {
                float prevL = 0.0f, prevR = 0.0f;
                playerValue (previous, now, inL, inR, prevL, prevR);
                previous.gain = (float) juce::jmax (0.0, previous.gain - fadeStep);
                outL += prevL * previous.gain;
                outR += prevR * previous.gain;
                if (previous.gain <= 0.0f)
                    previous.active = false;
            }
            left[i] = outL;
            if (right != nullptr)
                right[i] = outR;
        }
        amountShown.store (amountNow, std::memory_order_relaxed);

        if (target <= 0.0f && amountNow <= 0.0f && current.move == Move::dry && current.gain >= 1.0f
            && ! previous.active && holdSlices == 0)
            resting = true;
    }
}
