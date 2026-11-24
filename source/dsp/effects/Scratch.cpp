#include "Scratch.h"

#include <cmath>

namespace effects
{
    namespace
    {
        // The record's four copies: each low-passed for a faster stroke (the speed it's read at most).
        constexpr std::array<double, 4> levelSpeeds { 1.0, 2.5, 6.0, 15.0 };
        constexpr std::array<double, 4> levelCutoffs { 0.0, 8000.0, 3300.0, 1330.0 };

        double eased (double u, double snap) noexcept   // a hand's stroke: quick through the middle
        {
            u = juce::jlimit (0.0, 1.0, u);
            return (1.0 - snap) * (1.0 - std::cos (juce::MathConstants<double>::pi * u)) * 0.5 + snap * u;
        }

        double strokeAt (double u, double from, double to, double snap = 0.45) noexcept
        {
            return from + (to - from) * eased (u, snap);
        }

        bool shut (double u, double from, double to) noexcept { return u >= from && u < to; }

        // A fixed number for a bar (the same bar always the same routine).
        struct BarRandom
        {
            std::uint32_t state;
            explicit BarRandom (std::int64_t bar) noexcept
                : state ((std::uint32_t) ((std::uint64_t) (bar + 1000003) * 2654435761ull) ^ 0x5bd1e995u)
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
    }

    int Scratch::sixteenthsFor (float amount, std::int64_t bar) noexcept
    {
        const auto odd = (bar & 1) != 0;   // the second bar of each pair
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

    int Scratch::planBar (std::int64_t bar, float amount, std::array<Planned, maxPlanned>& out) noexcept
    {
        const auto span = sixteenthsFor (amount, bar);
        if (span <= 0)
            return 0;
        BarRandom r (bar);
        const auto odd = (bar & 1) != 0;
        int count = 0, at = 16 - span;
        // The moves a stretch is made of (two sixteenths each, TRANSFORM four, JUGGLE two drops of one) and how often.
        struct Choice { Move kind; float weight; };
        constexpr Choice choices[] = { { Move::chirp, 3.0f }, { Move::baby, 2.0f }, { Move::flare, 2.0f }, { Move::flare2, 1.5f },
                                       { Move::crab, 1.5f }, { Move::tear, 1.5f }, { Move::scribble, 1.0f },
                                       { Move::transform, 1.5f }, { Move::juggle, 1.5f } };
        float total = 0.0f;
        for (const auto& c : choices)
            total += c.weight;
        const auto push = [&] (Move kind, int length)
        {
            if (count < maxPlanned && at + length <= 16)
            {
                out[(size_t) count++] = { kind, at, length };
                at += length;
            }
        };
        while (at < 16 && count < maxPlanned)
        {
            const auto left = 16 - at;
            // A rewind into the bar line ends the second bar of a pair: always in a lone fill (from AMOUNT 1/6), often
            // in a longer stretch.
            if (odd && left == 4 && (span == 4 ? amount >= 1.0f / 6.0f : r.next() < 0.65f))
            {
                push (Move::rewind, 4);
                break;
            }
            auto pick = r.next() * total;
            auto kind = Move::chirp;
            for (const auto& c : choices)
            {
                if (pick < c.weight)
                {
                    kind = c.kind;
                    break;
                }
                pick -= c.weight;
            }
            if (kind == Move::transform && left < 4)
                kind = Move::chirp;
            const auto repeat = r.next() < 0.5f;
            for (int k = 0; k < (repeat ? 2 : 1) && at < 16; ++k)
            {
                if (kind == Move::juggle)
                {
                    push (Move::juggle, 1);
                    push (Move::juggle, 1);
                }
                else if (kind == Move::transform)
                {
                    if (16 - at < 4)
                        break;
                    push (kind, 4);
                }
                else
                    push (kind, 2);
                if (odd && 16 - at == 4)   // leave the rewind its place
                    break;
            }
        }
        return count;
    }

    Scratch::Hand Scratch::handAt (Move move, int i, int n, double L, double sr, double sixteenthSamples) noexcept
    {
        Hand h;
        n = juce::jmax (2, n);
        i = juce::jlimit (0, n - 1, i);
        const auto half = n / 2;
        const auto u = (double) i / n;
        const auto t = i / sr;
        const auto outAndBack = [&] (double reach, double snap = 0.45)
        {
            return i < half ? strokeAt ((double) i / half, 0.0, reach, snap) : strokeAt ((double) (i - half) / (n - half), reach, 0.0, snap);
        };
        switch (move)
        {
            case Move::baby:
                h.position = outAndBack (L);
                break;
            case Move::chirp:   // the fader closes as the record goes out, opens as it comes back
                h.position = outAndBack (L);
                h.fader = shut (u, 0.3, 0.68) ? 0.0f : 1.0f;
                break;
            case Move::flare:
            {
                h.position = outAndBack (L);
                const auto c = 0.016 * sr / n * 0.5;
                h.fader = shut (u, 0.25 - c, 0.25 + c) || shut (u, 0.75 - c, 0.75 + c) ? 0.0f : 1.0f;
                break;
            }
            case Move::flare2:
            {
                h.position = outAndBack (L);
                const auto c = 0.014 * sr / n * 0.5;
                bool closed = false;
                for (const auto q : { 0.17, 0.33, 0.67, 0.83 })
                    closed = closed || shut (u, q - c, q + c);
                h.fader = closed ? 0.0f : 1.0f;
                break;
            }
            case Move::crab:
            {
                h.position = outAndBack (L);
                bool closed = false;
                for (int k = 0; k < 4; ++k)
                    closed = closed || shut (u, 0.06 + 0.1 * k, 0.11 + 0.1 * k);
                h.fader = closed ? 0.0f : 1.0f;
                break;
            }
            case Move::tear:
            {
                const auto q = half / 2;
                const auto stop = (int) (0.02 * sr);
                if (i < q)
                    h.position = strokeAt ((double) i / juce::jmax (1, q), 0.0, L * 0.5);
                else if (i < q + stop)
                    h.position = L * 0.5;
                else if (i < half)
                    h.position = strokeAt ((double) (i - q - stop) / juce::jmax (1, half - q - stop), L * 0.5, L);
                else
                    h.position = strokeAt ((double) (i - half) / (n - half), L, 0.0);
                break;
            }
            case Move::transform:   // a slow stroke chopped every 32nd
            {
                h.position = outAndBack (L * 1.6, 0.8);
                const auto step = juce::jmax (2.0, sixteenthSamples * 0.5);
                h.fader = std::fmod ((double) i, step) < step * 0.55 ? 1.0f : 0.0f;
                break;
            }
            case Move::scribble:
                h.position = 0.04 + 0.012 * std::sin (juce::MathConstants<double>::twoPi * 22.0 * t) + strokeAt (u, 0.0, L * 0.4);
                break;
            case Move::juggle:   // the hit dropped straight from the needle
                h.position = t;
                break;
            case Move::rewind:   // spun back hard, slowing into the bar line
            {
                const auto T = n / sr;
                h.position = 0.25 - 14.0 * T / 2.6 * (1.0 - std::pow (1.0 - t / T, 2.6));
                break;
            }
        }
        return h;
    }

    double Scratch::reachOf (Move move, double L, double seconds) noexcept
    {
        switch (move)
        {
            case Move::transform: return 1.6 * L;
            case Move::scribble:  return 0.052 + 0.4 * L;
            case Move::juggle:    return seconds;
            case Move::rewind:    return 0.25;
            case Move::baby: case Move::chirp: case Move::flare: case Move::flare2: case Move::crab: case Move::tear:
            default:              return L;
        }
    }

    Scratch::Biquad Scratch::lowPass (double hz, double q, double sr) noexcept
    {
        const auto w0 = juce::MathConstants<double>::twoPi * juce::jmin (hz, 0.45 * sr) / sr;
        const auto alpha = std::sin (w0) / (2.0 * q), c = std::cos (w0), a0 = 1.0 + alpha;
        Biquad b;
        b.b0 = (float) ((1.0 - c) * 0.5 / a0); b.b1 = (float) ((1.0 - c) / a0); b.b2 = b.b0;
        b.a1 = (float) (-2.0 * c / a0); b.a2 = (float) ((1.0 - alpha) / a0);
        return b;
    }

    Scratch::Biquad Scratch::highPass (double hz, double q, double sr) noexcept
    {
        const auto w0 = juce::MathConstants<double>::twoPi * juce::jmin (hz, 0.45 * sr) / sr;
        const auto alpha = std::sin (w0) / (2.0 * q), c = std::cos (w0), a0 = 1.0 + alpha;
        Biquad b;
        b.b0 = (float) ((1.0 + c) * 0.5 / a0); b.b1 = (float) (-(1.0 + c) / a0); b.b2 = b.b0;
        b.a1 = (float) (-2.0 * c / a0); b.a2 = (float) ((1.0 - alpha) / a0);
        return b;
    }

    Scratch::Biquad Scratch::peak (double hz, double q, double db, double sr) noexcept
    {
        const auto w0 = juce::MathConstants<double>::twoPi * juce::jmin (hz, 0.45 * sr) / sr;
        const auto A = std::pow (10.0, db / 40.0), alpha = std::sin (w0) / (2.0 * q), c = std::cos (w0);
        const auto a0 = 1.0 + alpha / A;
        Biquad b;
        b.b0 = (float) ((1.0 + alpha * A) / a0); b.b1 = (float) (-2.0 * c / a0); b.b2 = (float) ((1.0 - alpha * A) / a0);
        b.a1 = b.b1; b.a2 = (float) ((1.0 - alpha / A) / a0);
        return b;
    }

    void Scratch::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        size = (int) std::ceil (historySeconds * sampleRate) + 8;
        for (auto& level : history)
            for (auto& side : level)
                side.assign ((size_t) size, 0.0f);
        for (int l = 1; l < levels; ++l)
            for (auto& side : recordFilters[(size_t) l])
            {
                side[0] = lowPass (levelCutoffs[(size_t) l], 0.5412, sampleRate);   // a fourth-order Butterworth
                side[1] = lowPass (levelCutoffs[(size_t) l], 1.3066, sampleRate);
            }
        reset();
    }

    void Scratch::reset() noexcept
    {
        // The recording isn't cleared (it's large, and this can run on the audio thread): with the count back at 0
        // nothing older than what's recorded from now on is ever read.
        for (auto& level : recordFilters)
            for (auto& side : level)
                for (auto& stage : side)
                    stage.clear();
        write = 0;
        clock = 0;
        pressCount = pressWrite = 0;
        ownBeat = 0.0;
        lastBeat = -1.0e9;
        planBarIndex = -1;
        numPlanned = nextPlanned = 0;
        spanStarted = false;
        active = false;
        delay = 0;
        fader = 1.0f;
        gate = 0.0f;
        inputPower = 0.0f;
        wetPower = 0.1f;
        wetGain = 1.0f;
        resting = true;
        moveNow.store (-1, std::memory_order_relaxed);
        shownPlanned.store (0, std::memory_order_relaxed);
    }

    float Scratch::random01() noexcept
    {
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        return (float) (noise >> 8) / 16777216.0f;
    }

    float Scratch::gaussian() noexcept
    {
        return (random01() + random01() + random01() - 1.5f) * 2.0f;   // near enough: sd 1
    }

    void Scratch::addNote (int offset) noexcept
    {
        presses[(size_t) pressWrite] = clock + juce::jmax (0, offset);
        pressWrite = (pressWrite + 1) % (int) presses.size();
        pressCount = juce::jmin (pressCount + 1, (int) presses.size());
    }

    float Scratch::readLevel (int level, int channel, double position) const noexcept
    {
        // The needle at `position` (samples recorded), four-point Hermite.
        position = juce::jlimit ((double) juce::jmax<std::int64_t> (1, clock - size + 4), (double) juce::jmax<std::int64_t> (1, clock - 3), position);
        const auto whole = (std::int64_t) std::floor (position);
        const auto f = (float) (position - (double) whole);
        const auto& buffer = history[(size_t) level][(size_t) channel];
        const auto at = [&] (std::int64_t k)
        {
            auto index = (int) (k % size);
            if (index < 0)
                index += size;
            return buffer[(size_t) index];
        };
        const auto y0 = at (whole - 1), y1 = at (whole), y2 = at (whole + 1), y3 = at (whole + 2);
        const auto c1 = 0.5f * (y2 - y0);
        const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    void Scratch::startChain() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            lowCut[(size_t) c] = highPass (150.0, 0.7071, sampleRate);
            top[(size_t) c] = lowPass (9000.0, 0.7071, sampleRate);
            hissLow[(size_t) c] = highPass (1200.0, 0.7071, sampleRate);
            hissHigh[(size_t) c] = lowPass (5000.0, 0.7071, sampleRate);
            dcIn[(size_t) c] = dcOut[(size_t) c] = 0.0f;
            readLevelNow[(size_t) c] = 0.0f;
        }
        fader = 1.0f;
    }

    void Scratch::startSpan (const Settings& s, double sixteenthSamples) noexcept
    {
        // Park the needle just before the newest hit far enough back for every move of the stretch to read.
        double need = 0.0;
        for (int k = nextPlanned; k < numPlanned; ++k)
            need = juce::jmax (need, reachOf (planned[(size_t) k].kind, s.stroke * 1.15,
                                              planned[(size_t) k].length * sixteenthSamples / sampleRate));
        need += 0.02;
        const auto needSamples = (std::int64_t) (need * sampleRate);
        std::int64_t best = -1;
        for (int k = 0; k < pressCount; ++k)
        {
            const auto p = presses[(size_t) k];
            const auto age = clock - p;
            if (age >= needSamples && age <= (std::int64_t) (maxAnchorAge * sampleRate) && p > best)
                best = p;
        }
        anchor = best >= 0 ? (double) best - 0.004 * sampleRate
                           : (double) (clock - needSamples) - 0.25 * sampleRate;   // no hit: the last beat or so
        if (! active)
            startChain();
        spanStarted = true;
    }

    void Scratch::startMove (const Planned& p, double sixteenthSamples) noexcept
    {
        kind = p.kind;
        moveStroke = 0.0;   // set below with its variation
        moveLength = juce::jmax (2, (int) std::round (p.length * sixteenthSamples));
        moveSample = 0;
        hasLast = false;
        active = true;
        moveCount.fetch_add (1, std::memory_order_relaxed);
    }

    void Scratch::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = juce::jmin (2, (int) block.getNumChannels());
        if (n <= 0 || channels <= 0)
            return;
        if (! s.on && resting)
        {
            if (clock != 0)
                reset();   // off: nothing kept (switched on again, it records afresh)
            return;
        }
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto amount = s.on ? s.amount : 0.0f;
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        const auto sixteenthSamples = 0.25 / beatsPerSample;
        const auto playing = ppq >= 0.0;
        auto beat = playing ? ppq : ownBeat;
        if (lastBeat > -1.0e8 && (beat < lastBeat - 0.5 || beat > lastBeat + 2.0))
        {
            planBarIndex = -1;   // the song jumped: what was planned is dropped
            active = false;
            delay = 0;
        }
        const auto drive = 4.5f * std::pow (4.0f, juce::jlimit (0.0f, 1.0f, s.dirt));
        const auto bias = 0.25f + 0.25f * s.dirt;
        const auto fuzz = 1.0f + 3.0f * juce::jmax (0.0f, 2.0f * s.dirt - 1.0f);
        const auto fuzzNorm = 1.0f / std::tanh (fuzz);
        const auto biasOffset = std::tanh (bias);
        for (int c = 0; c < 2; ++c)
            bite[(size_t) c] = peak (1800.0, 1.0, 5.0 + 4.0 * s.dirt, sampleRate);
        const auto faderStep = 1.0f / (float) (0.0005 * sampleRate);
        const auto gateStep = 1.0f / (float) (0.002 * sampleRate);
        const auto attack = (float) (1.0 - std::exp (-1.0 / (0.05 * sampleRate)));
        const auto release = (float) (1.0 - std::exp (-1.0 / (2.0 * sampleRate)));
        const auto wetFollow = (float) (1.0 - std::exp (-1.0 / (0.3 * sampleRate)));
        const auto gainGlide = (float) (1.0 - std::exp (-1.0 / (0.05 * sampleRate)));
        const auto dcPole = (float) std::exp (-juce::MathConstants<double>::twoPi * 10.0 / sampleRate);
        const auto mix = juce::jlimit (0.0f, 1.0f, s.mix);
        const auto heardFollow = (float) (1.0 - std::exp (-1.0 / (0.005 * sampleRate)));

        for (int i = 0; i < n; ++i)
        {
            // Record.
            const float x[2] = { left[i], right != nullptr ? right[i] : left[i] };
            for (int c = 0; c < channels; ++c)
            {
                history[0][(size_t) c][(size_t) write] = x[c];
                for (int l = 1; l < levels; ++l)
                {
                    auto& f = recordFilters[(size_t) l][(size_t) c];
                    history[(size_t) l][(size_t) c][(size_t) write] = f[1].run (f[0].run (x[c]));
                }
            }
            write = write + 1 < size ? write + 1 : 0;
            ++clock;
            const auto power = 0.5f * (x[0] * x[0] + x[channels > 1 ? 1 : 0] * x[channels > 1 ? 1 : 0]);
            inputPower += (power - inputPower) * (power > inputPower ? attack : release);

            // The song: a new bar's routine, and the next move when its sixteenth comes.
            const auto bar = (std::int64_t) std::floor (beat / 4.0);
            const auto sixteenthNow = (beat - (double) bar * 4.0) * 4.0;
            if (bar != planBarIndex)
            {
                planBarIndex = bar;
                numPlanned = planBar (bar, amount, planned);
                nextPlanned = 0;
                spanStarted = false;
                while (nextPlanned < numPlanned && sixteenthNow > planned[(size_t) nextPlanned].start + 0.25)
                    ++nextPlanned;   // woken late in the bar: what's gone is skipped
                shownPlanned.store (numPlanned, std::memory_order_relaxed);
                for (int k = 0; k < numPlanned; ++k)
                {
                    shownKind[(size_t) k].store ((int) planned[(size_t) k].kind, std::memory_order_relaxed);
                    shownStart[(size_t) k].store (planned[(size_t) k].start, std::memory_order_relaxed);
                    shownLength[(size_t) k].store (planned[(size_t) k].length, std::memory_order_relaxed);
                }
            }
            if (nextPlanned < numPlanned && sixteenthNow >= planned[(size_t) nextPlanned].start && delay <= 0)
            {
                // A hand is a few ms late now and then: the move waits that long (the last one held where it was).
                delay = 1 + (int) (random01() * 0.005f * (float) sampleRate);
            }
            if (delay > 0 && --delay == 0 && nextPlanned < numPlanned)
            {
                if (! spanStarted)
                    startSpan (s, sixteenthSamples);
                startMove (planned[(size_t) nextPlanned], sixteenthSamples);
                moveStroke = s.stroke * (0.85 + 0.3 * random01());
                ++nextPlanned;
            }
            beat += beatsPerSample;

            if (! active && gate <= 0.0f)
            {
                resting = true;
                continue;   // nothing scratched: the sound untouched
            }
            resting = false;

            // The hand.
            const auto hand = handAt (kind, moveSample, moveLength, moveStroke, sampleRate, sixteenthSamples);
            const auto position = anchor + hand.position * sampleRate;
            auto speed = hasLast ? std::abs (position - lastPosition) : 0.0;
            if (! hasLast)
            {
                const auto next = handAt (kind, moveSample + 1, moveLength, moveStroke, sampleRate, sixteenthSamples);
                speed = std::abs (next.position - hand.position) * sampleRate;
            }
            lastPosition = position;
            hasLast = true;
            if (moveSample + 1 < moveLength)
                ++moveSample;
            else if (delay <= 0 && nextPlanned >= numPlanned)
                active = false;   // the stretch is over: the hand let go, the sound back
            else if (delay <= 0 && planned[(size_t) nextPlanned].start > sixteenthNow + 0.5)
                active = false;

            fader += juce::jlimit (-faderStep, faderStep, (active ? hand.fader : 0.0f) - fader);
            gate += juce::jlimit (-gateStep, gateStep, (active ? 1.0f : 0.0f) - gate);

            // Which copy of the record (by speed), and the cartridge's fade as the hand slows.
            const auto ls = std::log (juce::jmax (1.0, speed));
            int lo = 0;
            while (lo + 2 < levels && ls >= std::log (levelSpeeds[(size_t) lo + 1]))
                ++lo;
            const auto hi = juce::jmin (levels - 1, lo + 1);
            const auto blend = (float) juce::jlimit (0.0, 1.0, (ls - std::log (levelSpeeds[(size_t) lo]))
                                                              / (std::log (levelSpeeds[(size_t) hi]) - std::log (levelSpeeds[(size_t) lo])));
            const auto cartridge = (float) std::pow (juce::jmin (1.0, speed / 0.6), 0.6);
            const auto ref = juce::jmax (1.0e-3f, std::sqrt (inputPower));
            const auto hiss = 0.0069f * (float) juce::jmin (4.0, speed) * fader;
            float out[2] {};
            float wetSum = 0.0f;
            for (int c = 0; c < channels; ++c)
            {
                auto w = readLevel (lo, c, position) * (1.0f - blend) + readLevel (hi, c, position) * blend;
                w *= cartridge;
                // The hand's hiss on the vinyl, only while there's sound under the needle (silence stays silent).
                auto& heard = readLevelNow[(size_t) c];
                heard += (std::abs (w) - heard) * heardFollow;
                // The mixer pushed into the red.
                w = lowCut[(size_t) c].run (w);
                w = std::tanh (w / (3.0f * ref) * drive + bias) - biasOffset;
                if (fuzz > 1.0f)
                    w = std::tanh (w * fuzz) * fuzzNorm;
                dcOut[(size_t) c] = w - dcIn[(size_t) c] + dcPole * dcOut[(size_t) c];
                dcIn[(size_t) c] = w;
                w = bite[(size_t) c].run (dcOut[(size_t) c]);
                w = top[(size_t) c].run (w) * fader;   // the crossfader after the mixer's drive: cut is silence
                w += hissHigh[(size_t) c].run (hissLow[(size_t) c].run ((random01() * 2.0f - 1.0f))) * hiss
                   * juce::jmin (1.0f, heard / (0.05f * ref));
                out[c] = w;
                wetSum += w * w;
            }
            wetPower += (wetSum / (float) channels - wetPower) * wetFollow;
            const auto target = juce::jlimit (0.1f, 10.0f, 1.25f * ref / std::sqrt (juce::jmax (1.0e-9f, wetPower)));
            wetGain += (target - wetGain) * gainGlide;
            const auto dry = 1.0f - mix * gate;
            left[i] = x[0] * dry + out[0] * wetGain * gate;
            if (right != nullptr)
                right[i] = x[1] * dry + out[1] * wetGain * gate;
            if (! active && gate <= 0.0f)
                resting = true;
        }
        ownBeat = beat;   // stopped: its own count carries on from here
        lastBeat = beat;
        moveNow.store (active ? (int) kind : -1, std::memory_order_relaxed);
        recordNow.store ((float) ((lastPosition - anchor) / sampleRate), std::memory_order_relaxed);
        faderNow.store (fader, std::memory_order_relaxed);
        barNow.store ((float) ((beat - std::floor (beat / 4.0) * 4.0) * 4.0), std::memory_order_relaxed);
    }
}
