// FLIP (HARDWIRED): a 90s sampler catching a beat of what you just played and flipping it, in time with the song.
// AMOUNT's share of each bar and the cells filling it, the same bar always flipped the same way; off, AMOUNT 0 and the
// bars it leaves alone untouched bit for bit; the flip starting on its sixteenth at two tempos and while stopped; what
// each pad plays (LOOP the bar before's first beat, CHOP its sixteenths in the new order, UP and DOWN pitched as a
// sampler); MIX; CRUNCH's coarse rate; PUMP's duck; silence and a silent side stay silent; in the plugin, its
// parameters, matrix destinations, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/Flip.h"
#include "ui/FlipPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>

namespace
{
    using effects::Flip;

    struct Run { std::vector<float> inLeft, inRight, left, right; };

    template <typename Input>
    Run play (Input input, Flip::Settings s, double bpm, double seconds, double startPpq = 0.0, int block = 128)
    {
        Flip fx;
        fx.prepare (sampleRate, block);
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
            float* channels[] = { run.left.data() + at, run.right.data() + at };
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, (size_t) n), s, bpm, startPpq < 0.0 ? -1.0 : startPpq + at * beatsPerSample);
        }
        return run;
    }

    int firstChange (const Run& r, int from = 0)
    {
        for (int i = from; i < (int) r.left.size(); ++i)
            if (r.left[(size_t) i] != r.inLeft[(size_t) i] || r.right[(size_t) i] != r.inRight[(size_t) i])
                return i;
        return -1;
    }

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

    double rms (const std::vector<float>& x, int from, int to)
    {
        double sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (sum / std::max (1, to - from));
    }

    // A tone that steps every sixteenth at 120 BPM (6000 samples): sixteenth k of each bar at 300 + 60 (k mod 16) Hz,
    // its phase kept running (no jumps).
    struct Staircase
    {
        std::vector<float> wave;
        explicit Staircase (double seconds)
        {
            wave.resize ((size_t) (seconds * sampleRate));
            double phase = 0.0;
            for (size_t i = 0; i < wave.size(); ++i)
            {
                const auto k = (int) (i / 6000) % 16;
                phase += (300.0 + 60.0 * k) / sampleRate;
                wave[i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            }
        }
        std::pair<float, float> operator() (int i) const { return { wave[(size_t) i], wave[(size_t) i] }; }
        static double hz (int sixteenth) { return 300.0 + 60.0 * (sixteenth % 16); }
    };

    std::int64_t barWith (Flip::Kind kind, std::int64_t from = 2)
    {
        for (auto bar = from; bar < from + 400; ++bar)
            if (Flip::kindOf (bar) == kind)
                return bar;
        return -1;
    }
}

TEST_CASE ("FLIP: AMOUNT's share of each bar, the cells filling it to the bar line, the same bar the same way", "[flip]")
{
    CHECK (Flip::sixteenthsFor (0.3f, 0) == 0);
    CHECK (Flip::sixteenthsFor (0.3f, 1) == 4);
    CHECK (Flip::sixteenthsFor (0.5f, 0) == 8);
    CHECK (Flip::sixteenthsFor (1.0f, 0) == 16);
    int stutters = 0;
    for (std::int64_t bar = 0; bar < 64; ++bar)
        for (const auto amount : { 0.2f, 0.3f, 0.5f, 0.8f, 1.0f })
        {
            const auto p = Flip::planBar (bar, amount, 5.0f);
            const auto span = Flip::sixteenthsFor (amount, bar);
            if (span == 0)
            {
                CHECK (p.count == 0);
                continue;
            }
            REQUIRE (p.count > 0);
            auto at = (float) (16 - span);
            for (int k = 0; k < p.count; ++k)
            {
                CHECK_THAT (p.cells[(size_t) k].start, Catch::Matchers::WithinAbs (at, 1e-4));
                at += p.cells[(size_t) k].length;
            }
            CHECK_THAT (at, Catch::Matchers::WithinAbs (16.0, 1e-4));
            if (span == 4)
                CHECK (p.stutterEnd);
            stutters += p.stutterEnd ? 1 : 0;
            CHECK (p.kind == Flip::kindOf (bar));
            const auto again = Flip::planBar (bar, amount, 5.0f);
            CHECK (again.count == p.count);
        }
    CHECK (stutters > 40);
    for (const auto kind : { Flip::Kind::loop, Flip::Kind::up, Flip::Kind::down, Flip::Kind::reverse, Flip::Kind::chop })
        CHECK (barWith (kind) >= 0);
}

TEST_CASE ("FLIP: off, AMOUNT 0 and the bars it leaves alone are untouched bit for bit", "[flip]")
{
    juce::Random random (5);
    std::vector<float> noise (48000 * 5);
    for (auto& v : noise)
        v = random.nextFloat() * 0.6f - 0.3f;
    const auto input = [&noise] (int i) { return std::pair { noise[(size_t) i], noise[(size_t) (noise.size() - 1 - i)] }; };
    Flip::Settings s;
    s.on = false;
    s.amount = 1.0f;
    CHECK (firstChange (play (input, s, 120.0, 4.5)) < 0);
    s.on = true;
    s.amount = 0.0f;
    CHECK (firstChange (play (input, s, 120.0, 4.5)) < 0);
    s.amount = 0.3f;   // bar 1's last beat (3.5 s), then nothing until bar 3
    const auto r = play (input, s, 120.0, 4.8);
    const auto change = firstChange (r);
    CHECK (change >= (int) (3.5 * sampleRate));
    CHECK (change <= (int) (3.5 * sampleRate + 0.001 * sampleRate));
    CHECK (firstChange (r, (int) (4.0 * sampleRate + 0.004 * sampleRate)) < 0);
}

TEST_CASE ("FLIP starts each flip on its sixteenth, at two tempos and while stopped", "[flip]")
{
    Flip::Settings s;
    s.on = true;
    s.amount = 0.5f;   // the last two beats of every bar, from the second bar (the first has no bar before it)
    for (const auto bpm : { 90.0, 140.0 })
        for (const auto stopped : { false, true })
        {
            const auto beat = 60.0 / bpm;
            const auto r = play ([] (int i) { const auto v = 0.3f * (float) std::sin (0.06 * i); return std::pair { v, v }; },
                                 s, bpm, 12.0 * beat, stopped ? -1.0 : 1.0);
            // Playing from beat 1, bar 1's bar before wasn't heard from its start: bar 2 (7 beats in) is the first to
            // flip, 2 beats later. Stopped, its own count from 0: bar 1, 4 beats in, 2 beats later.
            const auto expected = (stopped ? 6.0 : 9.0) * beat * sampleRate;
            INFO ("bpm " << bpm << (stopped ? " stopped" : " playing"));
            const auto change = firstChange (r);
            CHECK (change >= (int) expected);
            CHECK (change <= (int) (expected + 2.0));
        }
}

TEST_CASE ("FLIP: what the pads play: LOOP the bar before's first beat, CHOP its sixteenths re-ordered, UP and DOWN pitched", "[flip]")
{
    Flip::Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.crunch = 0.0f;
    s.pump = 0.0f;
    const auto bar = 4.0 * 0.5 * sampleRate;   // 120 BPM: 96000 samples
    const auto six = 6000;
    for (const auto kind : { Flip::Kind::loop, Flip::Kind::chop, Flip::Kind::up, Flip::Kind::down })
    {
        const auto b = barWith (kind, 3);
        REQUIRE (b >= 0);
        const Staircase stairs (2.2 * 2.0);
        // The song from the bar before, so bar b is the second bar played.
        const auto r = play (stairs, s, 120.0, 2.2 * 2.0, (double) (b - 1) * 4.0);
        const auto plan = Flip::planBar (b, 1.0f, s.pitch);
        INFO ("bar " << b << " flips " << Flip::flipNames[(int) kind]);
        int checked = 0;
        for (int k = 0; k < plan.count && checked < 6; ++k)
        {
            const auto& cell = plan.cells[(size_t) k];
            if (cell.start >= 12.0f && plan.stutterEnd)
                break;
            // A stretch inside the cell, and inside what of the grab plays (the faster pads end sooner).
            const auto speed = std::exp2 (cell.semitones / 12.0);
            const auto start = (int) (bar + cell.start * six);
            const auto lasts = std::min ((double) cell.length * six, (4.0 - cell.offset) * six / speed);
            if (lasts < 1500)
                continue;
            // What it should be: the grabbed sixteenth's tone, at the sampler's speed; backwards from B's end.
            const auto readFirst = cell.reversed ? 8 + 3 : (int) cell.offset;   // the sixteenth of the bar before it starts in
            const auto expected = Staircase::hz (readFirst) * speed;
            // (only as long as it reads the first grabbed sixteenth: a faster pad reaches the next one sooner)
            const auto f = crossingRate (r.left, start + 200, start + std::min ((int) lasts, (int) (six / speed)) - 200);
            CHECK_THAT (f, Catch::Matchers::WithinRel (expected, 0.04));
            ++checked;
        }
        CHECK (checked > 0);
    }
}

TEST_CASE ("FLIP: MIX, CRUNCH and PUMP", "[flip]")
{
    const auto b = barWith (Flip::Kind::loop, 3);
    REQUIRE (b >= 0);
    const auto tone = [] (int i) { const auto v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / sampleRate); return std::pair { v, v }; };
    Flip::Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.pump = 0.0f;
    const auto bar = 96000;
    s.mix = 1.0f;
    const auto replaced = play (tone, s, 120.0, 4.4, (double) (b - 1) * 4.0);
    s.mix = 0.0f;
    const auto under = play (tone, s, 120.0, 4.4, (double) (b - 1) * 4.0);
    // MIX 0 is MIX 1 with the sound added back where it's flipped.
    float worst = 0.0f;
    for (int i = bar + 1000; i < bar + 20000; ++i)
        worst = std::max (worst, std::abs (under.left[(size_t) i] - replaced.left[(size_t) i] - under.inLeft[(size_t) i]));
    CHECK (worst < 1.0e-5f);

    // CRUNCH: at 1 the sampler holds each sample at 13.02 kHz, which mirrors the 440 Hz tone round its rate (an image
    // at 12.58 kHz); at 0 it holds nothing (52 kHz is past the host's rate), so no image.
    double image[2] {};
    for (int k = 0; k < 2; ++k)
    {
        s.mix = 1.0f;
        s.crunch = (float) k;
        const auto r = play (tone, s, 120.0, 4.4, (double) (b - 1) * 4.0);
        image[k] = goertzel (r.left, bar + 2000, bar + 20000, Flip::rateFor (1.0f) - 440.0);
    }
    CHECK (image[1] > 30.0 * image[0]);

    // PUMP: ducked just after each beat, back by the end of its eighth.
    s.crunch = 0.5f;
    for (const auto pump : { 0.0f, 1.0f })
    {
        s.pump = pump;
        const auto r = play (tone, s, 120.0, 4.4, (double) (b - 1) * 4.0);
        const auto beatStart = bar + 24000;   // the bar's second beat
        const auto early = rms (r.left, beatStart + 200, beatStart + 1200);
        const auto late = rms (r.left, beatStart + 9000, beatStart + 11000);
        INFO ("PUMP " << pump);
        if (pump > 0.5f)
            CHECK (early < 0.4 * late);
        else
            CHECK_THAT (early / late, Catch::Matchers::WithinRel (1.0, 0.1));
    }
}

TEST_CASE ("FLIP: silence stays silent, a silent side stays silent", "[flip]")
{
    Flip::Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.crunch = 1.0f;
    const auto quiet = play ([] (int) { return std::pair { 0.0f, 0.0f }; }, s, 120.0, 8.0);
    float peak = 0.0f;
    for (size_t i = 0; i < quiet.left.size(); ++i)
        peak = std::max ({ peak, std::abs (quiet.left[i]), std::abs (quiet.right[i]) });
    CHECK (peak == 0.0f);
    const auto oneSide = play ([] (int i) { return std::pair { 0.0f, 0.3f * (float) std::sin (0.05 * i) }; }, s, 120.0, 8.0);
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

TEST_CASE ("FLIP CPU", "[flip][cpu]")
{
    Flip::Settings s;
    s.on = true;
    s.amount = 1.0f;
    const auto start = std::chrono::steady_clock::now();
    play ([] (int i) { const auto v = 0.3f * (float) std::sin (0.05 * i); return std::pair { v, v }; }, s, 120.0, 20.0);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    WARN ("FLIP at full AMOUNT: " << seconds / 20.0 * 100.0 << "% of a core (with the test's own work)");
    CHECK (seconds / 20.0 < 0.1);
}

TEST_CASE ("FLIP in the plugin: parameters, matrix destinations, a chord flipped, the SIGNATURE slot and its page", "[flip][plugin]")
{
    for (const auto* id : { ParamIDs::flipAmount, ParamIDs::flipCrunch, ParamIDs::flipPump })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Flip");
    }
    CHECK (signatureNames()[42] == "FLIP");

    const auto render = [] (bool on, float amount, int& flips)
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
        set (ParamIDs::flipOn, on ? 1.0f : 0.0f);
        set (ParamIDs::flipAmount, amount);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        const int chords[][3] = { { 60, 64, 67 }, { 57, 60, 64 }, { 62, 65, 69 }, { 55, 59, 62 } };
        int last = -1;
        for (int b = 0; b < (int) (6.5 * sampleRate / blockSize); ++b)
        {
            const auto which = (int) (b * blockSize / (0.5 * sampleRate)) % 4;   // a new chord every beat
            if (which != last)
            {
                if (last >= 0)
                    for (const auto n : chords[last])
                        processor.getKeyboardState().noteOff (1, n, 0.0f);
                for (const auto n : chords[which])
                    processor.getKeyboardState().noteOn (1, n, 0.9f);
                last = which;
            }
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        flips = processor.getFlip().getFlips();
        if (on && amount > 0.0f)
        {
            CHECK (signatureOn (processor, signatureNames().indexOf ("FLIP")));
            FlipPage page (processor);
            page.setSize (1200, 700);
            CHECK (page.getShownControls().size() == 6);
            juce::Image image (juce::Image::ARGB, 1200, 700, true);
            juce::Graphics g (image);
            page.paintEntireComponent (g, false);
        }
        return out;
    };
    int flips = 0;
    const auto off = render (false, 1.0f, flips);
    const auto rest = render (true, 0.0f, flips);
    CHECK (rest == off);
    CHECK (flips == 0);
    const auto flipped = render (true, 1.0f, flips);
    CHECK (flips > 10);
    double difference = 0.0, power = 0.0;
    for (size_t i = 0; i < off.size(); ++i)
    {
        difference += (double) (flipped[i] - off[i]) * (flipped[i] - off[i]);
        power += (double) off[i] * off[i];
        REQUIRE (std::isfinite (flipped[i]));
    }
    CHECK (difference > 0.1 * power);
}
