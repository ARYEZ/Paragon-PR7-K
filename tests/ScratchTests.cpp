// SCRATCH (HARDWIRED): a turntablist cutting up what you just played, in time with the song. AMOUNT's share of each
// bar, the routine filling it and the same every time; off, AMOUNT 0 and the bars it leaves alone untouched bit for
// bit; the cut starting on its sixteenth at two tempos and while stopped; the hand's strokes; the pitch following the
// record's speed (a JUGGLE plays the hit as recorded, a BABY's stroke out plays it higher by how fast the hand moves);
// MIX 1 replacing the sound while the fader is cut, MIX 0 leaving it under; DIRT's harmonics; silence and a silent side
// stay silent; in the plugin, its parameters, matrix destinations, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/Scratch.h"
#include "ui/RackSpecs.h"
#include "ui/ScratchPage.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>

namespace
{
    using effects::Scratch;

    struct Run { std::vector<float> inLeft, inRight, left, right; };

    // The effect alone on `input` (a function of the sample: left, right), notes pressed at `presses` (seconds), at
    // `bpm`, the song from `startPpq` (-1: stopped).
    template <typename Input>
    Run play (Input input, Scratch::Settings s, double bpm, double seconds, std::vector<double> presses, double startPpq = 0.0, int block = 128)
    {
        Scratch fx;
        fx.prepare (sampleRate, block);
        fx.seedRandom (1);
        Run run;
        const auto total = (int) (seconds * sampleRate);
        for (auto* v : { &run.inLeft, &run.inRight, &run.left, &run.right })
            v->assign ((size_t) total, 0.0f);
        for (int i = 0; i < total; ++i)
        {
            const auto [l, r] = input (i);
            run.inLeft[(size_t) i] = run.left[(size_t) i] = l;
            run.inRight[(size_t) i] = run.right[(size_t) i] = r;
        }
        const auto beatsPerSample = bpm / 60.0 / sampleRate;
        for (int at = 0; at < total; at += block)
        {
            const auto n = std::min (block, total - at);
            for (const auto p : presses)
            {
                const auto sample = (int) std::llround (p * sampleRate);
                if (sample >= at && sample < at + n)
                    fx.addNote (sample - at);
            }
            float* channels[] = { run.left.data() + at, run.right.data() + at };
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, (size_t) n), s, bpm, startPpq < 0.0 ? -1.0 : startPpq + at * beatsPerSample);
        }
        return run;
    }

    auto sine (double hz, float level = 0.3f)
    {
        return [hz, level] (int i) { const auto v = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sampleRate); return std::pair { v, v }; };
    }

    // The first sample where the output differs from the input (-1 none).
    int firstChange (const Run& r, int from = 0)
    {
        for (int i = from; i < (int) r.left.size(); ++i)
            if (r.left[(size_t) i] != r.inLeft[(size_t) i] || r.right[(size_t) i] != r.inRight[(size_t) i])
                return i;
        return -1;
    }

    // The frequency of the strongest part of a stretch, by counting where it crosses zero going up (with a little
    // hysteresis so the hiss doesn't count).
    double crossingRate (const std::vector<float>& x, int from, int to)
    {
        int count = 0, first = -1, last = -1;
        bool below = x[(size_t) from] < 0.0f;
        float peak = 0.0f;
        for (int i = from; i < to; ++i)
            peak = std::max (peak, std::abs (x[(size_t) i]));
        for (int i = from; i < to; ++i)
        {
            const auto v = x[(size_t) i];
            if (below && v > 0.1f * peak)
            {
                below = false;
                if (first < 0) first = i; else { last = i; ++count; }
            }
            else if (v < -0.1f * peak)
                below = true;
        }
        return count > 0 ? count * sampleRate / (last - first) : 0.0;
    }

    double goertzel (const std::vector<float>& x, int from, int to, double hz)
    {
        const auto w = juce::MathConstants<double>::twoPi * hz / sampleRate, c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = from; i < to; ++i)
        {
            const auto s0 = x[(size_t) i] + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (to - from);
    }

    // A bar (searching from `from`) whose routine at AMOUNT 1 has a move of `kind` in it: its bar and planned move.
    std::pair<std::int64_t, Scratch::Planned> findMove (Scratch::Move kind, std::int64_t from = 0)
    {
        std::array<Scratch::Planned, Scratch::maxPlanned> plan;
        for (auto bar = from; bar < from + 400; ++bar)
        {
            const auto count = Scratch::planBar (bar, 1.0f, plan);
            for (int k = 0; k < count; ++k)
                if (plan[(size_t) k].kind == kind)
                    return { bar, plan[(size_t) k] };
        }
        return { -1, {} };
    }
}

TEST_CASE ("SCRATCH: AMOUNT's share of each bar, the routine filling it to the bar line, the same every time", "[scratch]")
{
    CHECK (Scratch::sixteenthsFor (0.0f, 0) == 0);
    CHECK (Scratch::sixteenthsFor (0.0f, 1) == 0);
    CHECK (Scratch::sixteenthsFor (0.3f, 0) == 0);    // a fill on every other bar
    CHECK (Scratch::sixteenthsFor (0.3f, 1) == 4);
    CHECK (Scratch::sixteenthsFor (0.5f, 0) == 8);    // the last two beats of every bar
    CHECK (Scratch::sixteenthsFor (0.5f, 1) == 8);
    CHECK (Scratch::sixteenthsFor (1.0f, 0) == 16);   // whole bars
    CHECK (Scratch::sixteenthsFor (1.0f, 1) == 16);
    CHECK (Scratch::sixteenthsFor (0.8f, 0) == 12);
    std::array<Scratch::Planned, Scratch::maxPlanned> plan, again;
    int rewinds = 0, differing = 0;
    for (std::int64_t bar = 0; bar < 64; ++bar)
        for (const auto amount : { 0.2f, 0.3f, 0.5f, 0.8f, 1.0f })
        {
            const auto count = Scratch::planBar (bar, amount, plan);
            const auto span = Scratch::sixteenthsFor (amount, bar);
            REQUIRE (count == (span > 0 ? count : 0));
            if (span == 0)
            {
                CHECK (count == 0);
                continue;
            }
            // Back to back from where the stretch starts to the bar line.
            int at = 16 - span;
            for (int k = 0; k < count; ++k)
            {
                CHECK (plan[(size_t) k].start == at);
                at += plan[(size_t) k].length;
            }
            CHECK (at == 16);
            if (plan[(size_t) count - 1].kind == Scratch::Move::rewind)
                ++rewinds;
            // A lone fill from AMOUNT 1/6 always rewinds into the bar line.
            if (span == 4)
                CHECK (plan[(size_t) count - 1].kind == Scratch::Move::rewind);
            // The same bar, the same routine.
            const auto count2 = Scratch::planBar (bar, amount, again);
            REQUIRE (count2 == count);
            for (int k = 0; k < count; ++k)
                CHECK ((plan[(size_t) k].kind == again[(size_t) k].kind && plan[(size_t) k].start == again[(size_t) k].start));
            if (bar > 0 && amount == 1.0f)
            {
                const auto before = Scratch::planBar (bar - 1, amount, again);
                bool same = before == count;
                for (int k = 0; same && k < count; ++k)
                    same = plan[(size_t) k].kind == again[(size_t) k].kind && plan[(size_t) k].length == again[(size_t) k].length;
                differing += same ? 0 : 1;
            }
        }
    CHECK (rewinds > 40);
    CHECK (differing > 55);   // bars vary
    // Every move turns up.
    for (int k = 0; k < Scratch::moveNames.size(); ++k)
        CHECK (findMove ((Scratch::Move) k).first >= 0);
}

TEST_CASE ("SCRATCH: the hand's strokes", "[scratch]")
{
    const auto n = 12000;   // two sixteenths at 120 BPM
    const auto L = 0.15;
    using M = Scratch::Move;
    // BABY: out to the stroke and back, smoothly.
    CHECK (Scratch::handAt (M::baby, 0, n, L, sampleRate, 6000).position == 0.0);
    CHECK_THAT (Scratch::handAt (M::baby, n / 2, n, L, sampleRate, 6000).position, Catch::Matchers::WithinAbs (L, 1e-9));
    CHECK (Scratch::handAt (M::baby, n - 1, n, L, sampleRate, 6000).position < 0.001);
    double last = -1.0;
    for (int i = 0; i < n / 2; i += 100)
    {
        const auto p = Scratch::handAt (M::baby, i, n, L, sampleRate, 6000).position;
        CHECK (p >= last);
        last = p;
    }
    // CHIRP: the fader shut through the turn, open at both ends.
    CHECK (Scratch::handAt (M::chirp, n / 10, n, L, sampleRate, 6000).fader == 1.0f);
    CHECK (Scratch::handAt (M::chirp, n / 2, n, L, sampleRate, 6000).fader == 0.0f);
    CHECK (Scratch::handAt (M::chirp, n * 9 / 10, n, L, sampleRate, 6000).fader == 1.0f);
    // TRANSFORM: open and cut every 32nd (55% open).
    int open = 0;
    for (int i = 0; i < 2 * n; ++i)
        open += Scratch::handAt (M::transform, i, 2 * n, L, sampleRate, 6000).fader > 0.5f ? 1 : 0;
    CHECK_THAT (open / (2.0 * n), Catch::Matchers::WithinAbs (0.55, 0.01));
    // CRAB: four cuts on the way out.
    int cuts = 0;
    float before = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const auto f = Scratch::handAt (M::crab, i, n, L, sampleRate, 6000).fader;
        cuts += before > 0.5f && f < 0.5f ? 1 : 0;
        before = f;
    }
    CHECK (cuts == 4);
    // JUGGLE: the record let go at its own speed.
    CHECK_THAT (Scratch::handAt (M::juggle, 4800, 6000, L, sampleRate, 6000).position, Catch::Matchers::WithinAbs (0.1, 1e-9));
    // REWIND: from a quarter second past the hit, spun back hard and slowing to a stop.
    const auto r0 = Scratch::handAt (M::rewind, 0, 24000, L, sampleRate, 6000).position;
    const auto r1 = Scratch::handAt (M::rewind, 100, 24000, L, sampleRate, 6000).position;
    const auto rEnd1 = Scratch::handAt (M::rewind, 23898, 24000, L, sampleRate, 6000).position;
    const auto rEnd2 = Scratch::handAt (M::rewind, 23998, 24000, L, sampleRate, 6000).position;
    CHECK (r0 == 0.25);
    CHECK ((r0 - r1) * sampleRate / 100.0 > 12.0);     // fourteen times as fast, backwards
    CHECK ((rEnd1 - rEnd2) * sampleRate / 100.0 < 0.1);   // nearly stopped
}

TEST_CASE ("SCRATCH: off, AMOUNT 0 and the bars it leaves alone are untouched bit for bit", "[scratch]")
{
    juce::Random random (3);
    std::vector<float> noise (48000 * 5);
    for (auto& v : noise)
        v = random.nextFloat() * 0.6f - 0.3f;
    const auto input = [&noise] (int i) { return std::pair { noise[(size_t) i], noise[(size_t) (noise.size() - 1 - i)] }; };
    Scratch::Settings s;
    s.on = false;
    s.amount = 1.0f;
    CHECK (firstChange (play (input, s, 120.0, 4.0, { 0.0, 0.5, 1.0 })) < 0);
    s.on = true;
    s.amount = 0.0f;
    CHECK (firstChange (play (input, s, 120.0, 4.0, { 0.0, 0.5, 1.0 })) < 0);
    // AMOUNT 0.3: bar 0 alone, bar 1 until its last beat (sixteenth 12 at 120 BPM = 3.5 s... bar 1 starts at 2 s).
    s.amount = 0.3f;
    const auto r = play (input, s, 120.0, 4.5, { 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0 });
    const auto change = firstChange (r);
    CHECK (change >= (int) (3.5 * sampleRate));
    CHECK (change <= (int) (3.5 * sampleRate + 0.006 * sampleRate));
    // ...and after the rewind lands on the bar line (4 s), within 3 ms the sound is back untouched.
    CHECK (firstChange (r, (int) (4.0 * sampleRate + 0.008 * sampleRate)) < 0);
}

TEST_CASE ("SCRATCH starts each cut on its sixteenth, at two tempos and while stopped", "[scratch]")
{
    Scratch::Settings s;
    s.on = true;
    s.amount = 0.5f;   // the last two beats of every bar
    for (const auto bpm : { 90.0, 140.0 })
        for (const auto stopped : { false, true })
        {
            const auto beat = 60.0 / bpm;
            std::vector<double> presses;
            for (int k = 0; k < 16; ++k)
                presses.push_back (k * beat);
            // Following the song from beat 1 (so the first bar's cut comes at its beat 2 + 1, i.e. 1 beat in).
            const auto r = play (sine (440.0), s, bpm, 6.0 * beat, presses, stopped ? -1.0 : 1.0);
            const auto change = firstChange (r);
            const auto expected = (stopped ? 2.0 : 1.0) * beat * sampleRate;
            INFO ("bpm " << bpm << (stopped ? " stopped" : " playing"));
            CHECK (change >= (int) expected);
            CHECK (change <= (int) (expected + 0.006 * sampleRate));
        }
}

TEST_CASE ("SCRATCH: the pitch follows the record's speed, a JUGGLE as recorded, a BABY's stroke out higher", "[scratch]")
{
    Scratch::Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.dirt = 0.0f;
    const auto bpm = 120.0;
    const auto sixteenth = 60.0 / bpm / 4.0 * sampleRate;
    std::vector<double> presses;
    for (int k = 0; k < 400; ++k)
        presses.push_back (k * 0.5);
    for (const auto kind : { Scratch::Move::juggle, Scratch::Move::baby })
    {
        const auto [bar, move] = findMove (kind, 2);
        REQUIRE (bar >= 0);
        // Start the song at the bar before, so the record has a bar of tone on it.
        const auto startBeat = (double) (bar - 1) * 4.0;
        const auto r = play (sine (500.0), s, bpm, 4.0 * 60.0 / bpm * 2.0 + 0.1, presses, startBeat);
        const auto moveStart = (int) ((4.0 * 60.0 / bpm) * sampleRate + move.start * sixteenth);
        if (kind == Scratch::Move::juggle)
        {
            // Past the 5 ms a hand can be late, the middle of the drop: the tone at its own pitch.
            const auto f = crossingRate (r.left, moveStart + 400, moveStart + (int) (sixteenth * 0.8));
            CHECK_THAT (f, Catch::Matchers::WithinRel (500.0, 0.04));
        }
        else
        {
            // The stroke out's middle third: the hand at its fastest, the tone up by the stroke's speed.
            const auto n = (int) (move.length * sixteenth);
            const auto from = n / 6 + 300, to = n / 3 + 300;
            const auto p0 = Scratch::handAt (kind, from - 300, n, s.stroke, sampleRate, sixteenth).position;
            const auto p1 = Scratch::handAt (kind, to - 300, n, s.stroke, sampleRate, sixteenth).position;
            const auto speed = (p1 - p0) * sampleRate / (to - from);   // about the hand's mean speed there
            const auto f = crossingRate (r.left, moveStart + from, moveStart + to);
            INFO ("expected about " << 500.0 * speed);
            CHECK (speed > 1.2);
            CHECK_THAT (f, Catch::Matchers::WithinRel (500.0 * speed, 0.3));   // the stroke's stroke length varies by 15%
            CHECK (f > 600.0);
        }
    }
}

TEST_CASE ("SCRATCH: MIX 1 replaces the sound while the fader is cut, MIX 0 leaves it under; DIRT's harmonics", "[scratch]")
{
    const auto bpm = 120.0;
    const auto sixteenth = 60.0 / bpm / 4.0 * sampleRate;
    std::vector<double> presses;
    for (int k = 0; k < 400; ++k)
        presses.push_back (k * 0.5);
    const auto [bar, chirp] = findMove (Scratch::Move::chirp, 2);
    REQUIRE (bar >= 0);
    const auto startBeat = (double) (bar - 1) * 4.0;
    const auto chirpStart = (int) ((4.0 * 60.0 / bpm) * sampleRate + chirp.start * sixteenth);
    const auto n = (int) (chirp.length * sixteenth);
    const auto from = chirpStart + (int) (0.4 * n), to = chirpStart + (int) (0.6 * n);   // the fader shut
    Scratch::Settings s;
    s.on = true;
    s.amount = 1.0f;
    const auto rms = [] (const std::vector<float>& x, int a, int b)
    {
        double sum = 0.0;
        for (int i = a; i < b; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (sum / (b - a));
    };
    const auto seconds = 4.0 * 60.0 / bpm * 2.0 + 0.1;
    s.mix = 1.0f;
    const auto replaced = play (sine (300.0), s, bpm, seconds, presses, startBeat);
    s.mix = 0.0f;
    const auto under = play (sine (300.0), s, bpm, seconds, presses, startBeat);
    const auto input = rms (replaced.inLeft, from, to);
    CHECK (rms (replaced.left, from, to) < 0.02 * input);                    // cut: nearly nothing
    CHECK_THAT (rms (under.left, from, to), Catch::Matchers::WithinRel (input, 0.05));   // the sound carries on under

    // DIRT: on a JUGGLE (the tone at its own pitch) more of the third harmonic.
    const auto [jbar, juggle] = findMove (Scratch::Move::juggle, 2);
    const auto jStart = (int) ((4.0 * 60.0 / bpm) * sampleRate + juggle.start * sixteenth) + 400;
    const auto jEnd = jStart + (int) (sixteenth * 0.7);
    double ratio[2] {};
    for (int d = 0; d < 2; ++d)
    {
        s.mix = 1.0f;
        s.dirt = (float) d;
        const auto r = play (sine (500.0), s, bpm, seconds, presses, (double) (jbar - 1) * 4.0);
        ratio[d] = (goertzel (r.left, jStart, jEnd, 1500.0) + goertzel (r.left, jStart, jEnd, 1000.0))
                 / goertzel (r.left, jStart, jEnd, 500.0);
    }
    CHECK (ratio[1] > ratio[0] * 2.0);
}

TEST_CASE ("SCRATCH: silence stays silent, a silent side stays silent", "[scratch]")
{
    Scratch::Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.dirt = 1.0f;
    std::vector<double> presses;
    for (int k = 0; k < 20; ++k)
        presses.push_back (k * 0.5);
    const auto quiet = play ([] (int) { return std::pair { 0.0f, 0.0f }; }, s, 120.0, 8.0, presses);
    float peak = 0.0f;
    for (size_t i = 0; i < quiet.left.size(); ++i)
        peak = std::max ({ peak, std::abs (quiet.left[i]), std::abs (quiet.right[i]) });
    CHECK (peak == 0.0f);
    const auto oneSide = play ([] (int i) { return std::pair { 0.0f, 0.3f * (float) std::sin (0.05 * i) }; }, s, 120.0, 8.0, presses);
    float left = 0.0f, right = 0.0f;
    for (size_t i = 0; i < oneSide.left.size(); ++i)
    {
        left = std::max (left, std::abs (oneSide.left[i]));
        right = std::max (right, std::abs (oneSide.right[i]));
    }
    CHECK (left == 0.0f);
    CHECK (right > 0.1f);
    CHECK (firstChange (oneSide) >= 0);
}

TEST_CASE ("SCRATCH CPU", "[scratch][cpu]")
{
    Scratch::Settings s;
    s.on = true;
    s.amount = 1.0f;
    std::vector<double> presses;
    for (int k = 0; k < 40; ++k)
        presses.push_back (k * 0.5);
    const auto start = std::chrono::steady_clock::now();
    play (sine (300.0), s, 120.0, 20.0, presses);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    WARN ("SCRATCH at full AMOUNT: " << seconds / 20.0 * 100.0 << "% of a core (with the test's own work)");
    CHECK (seconds / 20.0 < 0.1);
}

TEST_CASE ("SCRATCH in the plugin: parameters, matrix destinations, a chord cut up, the SIGNATURE slot and its page", "[scratch][plugin]")
{
    for (const auto* id : { ParamIDs::scratchAmount, ParamIDs::scratchDirt, ParamIDs::scratchMix })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Scratch");
    }
    CHECK (signatureNames()[41] == "SCRATCH");

    const auto render = [] (bool on, float amount, int& moves)
    {
        ParagonX1AudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
        processor.seedRandomForTest (1);
        const auto set = [&processor] (const char* id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set (ParamIDs::scratchOn, on ? 1.0f : 0.0f);
        set (ParamIDs::scratchAmount, amount);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int b = 0; b < (int) (4.5 * sampleRate / blockSize); ++b)
        {
            if (b % (int) (0.5 * sampleRate / blockSize) == 0)
            {
                for (const auto n : { 60, 64, 67 })
                    processor.getKeyboardState().noteOff (1, n, 0.0f);
                for (const auto n : { 60, 64, 67 })
                    processor.getKeyboardState().noteOn (1, n, 0.9f);
            }
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        moves = processor.getScratch().getMoveCount();
        if (on && amount > 0.0f)
        {
            CHECK (signatureOn (processor, signatureNames().indexOf ("SCRATCH")));
            ScratchPage page (processor);
            page.setSize (1200, 700);
            CHECK (page.getShownControls().size() == 5);
            juce::Image image (juce::Image::ARGB, 1200, 700, true);
            juce::Graphics g (image);
            page.paintEntireComponent (g, false);
        }
        return out;
    };
    int moves = 0;
    const auto off = render (false, 1.0f, moves);
    const auto rest = render (true, 0.0f, moves);
    CHECK (rest == off);
    CHECK (moves == 0);
    const auto cut = render (true, 1.0f, moves);
    CHECK (moves > 10);
    double difference = 0.0, power = 0.0;
    for (size_t i = 0; i < off.size(); ++i)
    {
        difference += (double) (cut[i] - off[i]) * (cut[i] - off[i]);
        power += (double) off[i] * off[i];
        REQUIRE (std::isfinite (cut[i]));
    }
    CHECK (difference > 0.1 * power);
}

TEST_CASE ("SCRATCH keeps about the level of what it replaces: no bursts", "[scratch]")
{
    // (The first builds' level followers turned up in quiet moments and the next hit came 10-20 dB over.) A riff of
    // plucked notes, a quarter beat each with gaps, at full AMOUNT: over the bars it takes, the level within 4.5 dB of
    // the riff's and no peak more than 2.5 times the riff's loudest.
    const auto riff = [] (int i)
    {
        const auto inNote = i % 6000;
        const auto hz = 220.0 * std::exp2 (((i / 6000) % 5) * 3.0 / 12.0);
        const auto v = 0.3f * (float) std::exp (-inNote / 2500.0) * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sampleRate);
        return std::pair { v, v };
    };
    Scratch::Settings s;
    s.on = true;
    s.amount = 1.0f;
    std::vector<double> presses;
    for (int k = 0; k < 64; ++k)
        presses.push_back (k * 0.25);
    for (const auto dirt : { 0.0f, 1.0f })
    {
        s.dirt = dirt;
        const auto r = play (riff, s, 120.0, 16.0, presses);
        double inPower = 0.0, outPower = 0.0;
        float inPeak = 0.0f, outPeak = 0.0f;
        for (size_t i = (size_t) (2.0 * sampleRate); i < r.left.size(); ++i)
        {
            inPower += (double) r.inLeft[i] * r.inLeft[i];
            outPower += (double) r.left[i] * r.left[i];
            inPeak = std::max (inPeak, std::abs (r.inLeft[i]));
            outPeak = std::max (outPeak, std::abs (r.left[i]));
        }
        const auto db = 10.0 * std::log10 (outPower / inPower);
        INFO ("level " << db << " dB, peak x" << outPeak / inPeak);
        CHECK (std::abs (db) < 4.5);
        CHECK (outPeak < 2.5f * inPeak);
    }
}
