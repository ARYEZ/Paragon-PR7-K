// ECHOES (0.53.0): each mode returns exactly the right part of the past, at the right time. ECHO a click a beat later
// and its echoes; REVERSE a rising sweep coming back falling; TAPE a tone an octave down over twice the time; LOOP one
// bar repeated as set, or for ever; stretches lined up with the song's beats; AGE darkening; off changes nothing;
// each side its own.
#include "SynthTestUtils.h"

#include "dsp/effects/Echoes.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
    using Echoes = effects::Echoes;
    constexpr double bpm = 120.0;                        // a beat is 24000 samples at 48 kHz
    constexpr int beat = 24000;

    // `in` (both sides) through ECHOES, the wet sound only (the dry taken back off), left side. `ppqStart` < 0: the
    // song stopped.
    std::vector<float> wetOf (const std::vector<float>& in, Echoes::Settings s, double ppqStart = -1.0)
    {
        Echoes echoes;
        echoes.prepare (sampleRate, blockSize);
        s.on = true;
        std::vector<float> out (in.size(), 0.0f);
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (size_t start = 0; start < in.size(); start += (size_t) blockSize)
        {
            const auto n = (int) std::min ((size_t) blockSize, in.size() - start);
            buffer.clear();
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                    buffer.setSample (c, i, in[start + (size_t) i]);
            auto block = juce::dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) n);
            const auto ppq = ppqStart < 0.0 ? -1.0 : ppqStart + (double) start / (double) beat;
            echoes.process (block, s, bpm, ppq);
            for (int i = 0; i < n; ++i)
                out[start + (size_t) i] = buffer.getSample (0, i) - in[start + (size_t) i];
        }
        return out;
    }

    Echoes::Settings settings (Echoes::Mode mode, int distance, float feedback = 0.0f)
    {
        Echoes::Settings s;
        s.mode = mode;
        s.distance = distance;
        s.mix = 1.0f;
        s.feedback = feedback;
        s.age = 0.0f;
        return s;
    }

    std::vector<float> tone (double hz, size_t from, size_t to, size_t total, float level = 0.3f)
    {
        std::vector<float> x (total, 0.0f);
        for (auto i = from; i < to; ++i)
            x[i] = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) (i - from) / sampleRate);
        return x;
    }

    size_t loudestAt (const std::vector<float>& x, size_t from, size_t to)
    {
        size_t best = from;
        for (auto i = from; i < to; ++i)
            if (std::abs (x[i]) > std::abs (x[best]))
                best = i;
        return best;
    }
}

TEST_CASE ("ECHOES ECHO: a click comes back exactly a beat later, and FEEDBACK echoes it again", "[echoes]")
{
    std::vector<float> in ((size_t) (4 * beat), 0.0f);
    in[1000] = 0.2f;
    const auto wet = wetOf (in, settings (Echoes::Mode::echo, 0, 0.5f));   // 1 beat
    const auto first = loudestAt (wet, 1100, (size_t) (beat + 3000));
    CHECK (std::abs ((int) first - (1000 + beat)) <= 1);
    CHECK (std::abs (wet[first]) > 0.15f);
    const auto second = loudestAt (wet, (size_t) (beat + 3000), (size_t) (2 * beat + 3000));
    CHECK (std::abs ((int) second - (1000 + 2 * beat)) <= 2);
    INFO ("first echo " << wet[first] << ", second " << wet[second]);
    CHECK (std::abs (wet[second]) == Catch::Approx (0.5f * std::abs (wet[first])).margin (0.03));
    CHECK (rms (wet, 0, 1000) == 0.0);   // nothing comes back before anything was played
}

TEST_CASE ("ECHOES REVERSE: the last stretch comes back backwards during the next", "[echoes]")
{
    // Bar 1: a sweep rising from 200 Hz to 2 kHz; bar 2: silence. The wet sound in bar 2 must be that sweep backwards.
    const auto bar = (size_t) (4 * beat);
    std::vector<float> in (3 * bar, 0.0f);
    std::vector<float> sweep (bar);
    double phase = 0.0;
    for (size_t i = 0; i < bar; ++i)
    {
        const auto hz = 200.0 * std::pow (10.0, (double) i / (double) bar);
        phase += hz / sampleRate;
        sweep[i] = in[i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
    }
    const auto wet = wetOf (in, settings (Echoes::Mode::reverse, 2));   // 1 bar
    // Compare the middle of bar 2 with the sweep read backwards (the edges are faded).
    double match = 0.0, energyA = 0.0, energyB = 0.0;
    for (size_t k = 2000; k < bar - 2000; ++k)
    {
        const auto a = (double) wet[bar + k], b = (double) sweep[bar - 1 - k];
        match += a * b;
        energyA += a * a;
        energyB += b * b;
    }
    const auto correlation = match / std::sqrt (energyA * energyB);
    INFO ("correlation with the sweep backwards: " << correlation);
    CHECK (correlation > 0.99);
    // And it isn't simply the sweep forwards.
    double forwards = 0.0;
    for (size_t k = 2000; k < bar - 2000; ++k)
        forwards += (double) wet[bar + k] * sweep[k];
    CHECK (std::abs (forwards / std::sqrt (energyA * energyB)) < 0.2);
}

TEST_CASE ("ECHOES TAPE: a stretch comes back at half speed, an octave down, over twice its length", "[echoes]")
{
    // 1 kHz for the first beat; the stretch is two beats (TAPE plays the last beat over two).
    const auto total = (size_t) (6 * beat);
    const auto in = tone (1000.0, 0, (size_t) beat, total);
    const auto wet = wetOf (in, settings (Echoes::Mode::tape, 0));   // DISTANCE 1 beat
    // Beats 3 and 4 (the next stretch) hold the first beat at half speed: 500 Hz.
    const auto from = (size_t) (2 * beat + 4000), length = (size_t) (beat);
    const auto at500 = levelAt (wet, 500.0, from, length), at1000 = levelAt (wet, 1000.0, from, length);
    INFO ("500 Hz " << db (at500) << " dB, 1 kHz " << db (at1000) << " dB");
    CHECK (at500 > 0.1);
    CHECK (db (at1000) < db (at500) - 30.0);
}

TEST_CASE ("ECHOES LOOP: a bar repeated as many times as set, then the next; or for ever", "[echoes]")
{
    // Bar 1 at 440 Hz, bar 2 at 660 Hz, then silence.
    const auto bar = (size_t) (4 * beat);
    std::vector<float> in (7 * bar, 0.0f);
    const auto a = tone (440.0, 0, bar, in.size()), b = tone (660.0, bar, 2 * bar, in.size());
    for (size_t i = 0; i < in.size(); ++i)
        in[i] = a[i] + b[i];
    const auto barOf = [&] (const std::vector<float>& wet, int k, double hz) { return levelAt (wet, hz, (size_t) k * bar + 4000, bar - 8000); };

    // FEEDBACK 0: each bar once, a bar later (bar 2 plays bar 1, bar 3 plays bar 2).
    auto once = wetOf (in, settings (Echoes::Mode::loop, 2, 0.0f));
    CHECK (barOf (once, 1, 440.0) > 0.1);
    CHECK (barOf (once, 2, 660.0) > 0.1);
    CHECK (barOf (once, 2, 440.0) < 0.01);

    // Three times: bar 1 comes back in bars 2, 3 and 4; then bar 4 (silence) is caught.
    CHECK (Echoes::loopRepeats (2.0f / 7.0f) == 3);
    auto thrice = wetOf (in, settings (Echoes::Mode::loop, 2, 2.0f / 7.0f));
    for (int k = 1; k <= 3; ++k)
        CHECK (barOf (thrice, k, 440.0) > 0.1);
    CHECK (barOf (thrice, 2, 660.0) < 0.01);   // bar 2 was never caught
    CHECK (rms (thrice, 4 * bar + 4000, 5 * bar - 4000) < 0.001);

    // For ever: bar 1 on and on, long after everything stopped.
    auto forever = wetOf (in, settings (Echoes::Mode::loop, 2, 1.0f));
    for (int k = 1; k <= 6; ++k)
        CHECK (barOf (forever, k, 440.0) > 0.1);
}

TEST_CASE ("ECHOES: stretches line up with the song's beats; AGE darkens; off and MIX", "[echoes]")
{
    // The song at beat 0.5 when the block starts: REVERSE's 1-beat stretches cut at beats 1, 2... so a click at
    // beat 0.75 comes back in the stretch from beat 1 to 2, mirrored about beat 1: at beat 1.25.
    {
        std::vector<float> in ((size_t) (3 * beat), 0.0f);
        in[(size_t) (beat / 4)] = 0.3f;   // the block starts at beat 0.5: this is beat 0.75
        const auto wet = wetOf (in, settings (Echoes::Mode::reverse, 0), 0.5);
        const auto at = loudestAt (wet, (size_t) (beat / 4) + 100, (size_t) (2 * beat));
        CHECK (std::abs ((int) at - (int) (beat * 3 / 4)) <= 2);   // beat 1.25 = 0.75 beats after the block's start
    }
    // AGE: an 8 kHz tone comes back much quieter when old than when new; a 200 Hz one hardly changes.
    {
        const auto total = (size_t) (3 * beat);
        for (const auto hz : { 8000.0, 200.0 })
        {
            const auto in = tone (hz, 0, (size_t) beat, total, 0.1f);
            auto fresh = settings (Echoes::Mode::echo, 0), old = fresh;
            old.age = 1.0f;
            const auto newLevel = levelAt (wetOf (in, fresh), hz, (size_t) beat + 2000, (size_t) beat / 2);
            const auto oldLevel = levelAt (wetOf (in, old), hz, (size_t) beat + 2000, (size_t) beat / 2);
            INFO (hz << " Hz: new " << db (newLevel) << " dB, old " << db (oldLevel) << " dB");
            if (hz > 1000.0)
                CHECK (db (oldLevel) < db (newLevel) - 10.0);
            else
                CHECK (db (oldLevel) > db (newLevel) - 3.0);
        }
    }
    // MIX scales what comes back.
    {
        std::vector<float> in ((size_t) (2 * beat), 0.0f);
        in[500] = 0.2f;
        auto half = settings (Echoes::Mode::echo, 0);
        half.mix = 0.5f;
        const auto full = wetOf (in, settings (Echoes::Mode::echo, 0));
        const auto halved = wetOf (in, half);
        CHECK (halved[(size_t) (500 + beat)] == Catch::Approx (0.5f * full[(size_t) (500 + beat)]).margin (1e-4));
    }
    // Off: exactly the input; one side silent stays silent.
    {
        Echoes echoes;
        echoes.prepare (sampleRate, blockSize);
        Echoes::Settings off;
        juce::AudioBuffer<float> buffer (2, blockSize);
        auto rightPeak = 0.0f;
        for (int b = 0; b < 200; ++b)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                buffer.setSample (0, i, 0.3f * (float) std::sin (0.05 * (b * blockSize + i)));
                buffer.setSample (1, i, 0.0f);
            }
            juce::AudioBuffer<float> copy (buffer);
            juce::dsp::AudioBlock<float> block (buffer);
            echoes.process (block, off, bpm, -1.0);
            for (int i = 0; i < blockSize; ++i)
                REQUIRE (buffer.getSample (0, i) == copy.getSample (0, i));
            rightPeak = juce::jmax (rightPeak, buffer.getMagnitude (1, 0, blockSize));
        }
        CHECK (rightPeak == 0.0f);
        // On, with the right side silent: the right side's past is silent too.
        Echoes stereo;
        stereo.prepare (sampleRate, blockSize);
        auto on = settings (Echoes::Mode::echo, 0, 0.5f);
        on.on = true;
        auto rightOut = 0.0f;
        for (int b = 0; b < 400; ++b)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                buffer.setSample (0, i, 0.3f * (float) std::sin (0.05 * (b * blockSize + i)));
                buffer.setSample (1, i, 0.0f);
            }
            juce::dsp::AudioBlock<float> block (buffer);
            stereo.process (block, on, bpm, -1.0);
            rightOut = juce::jmax (rightOut, buffer.getMagnitude (1, 0, blockSize));
        }
        CHECK (rightOut == 0.0f);
    }
}

TEST_CASE ("ECHOES: CPU, and catching a long loop doesn't stall a block", "[echoes]")
{
    Echoes echoes;
    echoes.prepare (sampleRate, blockSize);
    auto s = settings (Echoes::Mode::reverse, 5, 0.4f);   // 8 bars
    s.on = true;
    s.age = 0.5f;
    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto blocks = (int) (10.0 * sampleRate / blockSize);
    const auto start = juce::Time::getMillisecondCounterHiRes();
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
            for (int c = 0; c < 2; ++c)
                buffer.setSample (c, i, 0.2f * (float) std::sin (0.03 * (b * blockSize + i)));
        juce::dsp::AudioBlock<float> block (buffer);
        echoes.process (block, s, bpm, -1.0);
    }
    const auto percent = (juce::Time::getMillisecondCounterHiRes() - start) / 10000.0 * 100.0;
    WARN ("ECHOES (REVERSE, 8 bars, stereo): " << percent << "% of one core");
    CHECK (percent < 5.0);

    // LOOP catching 8 bars at 60 BPM (16 s capped) in one block: well under a block's time.
    Echoes looper;
    looper.prepare (sampleRate, blockSize);
    auto l = settings (Echoes::Mode::loop, 5, 0.0f);
    l.on = true;
    double worst = 0.0;
    for (int b = 0; b < (int) (40.0 * sampleRate / blockSize); ++b)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        looper.process (block, l, 60.0, -1.0);
        worst = juce::jmax (worst, juce::Time::getMillisecondCounterHiRes() - t0);
    }
    WARN ("LOOP's worst block: " << worst << " ms (a block is " << 1000.0 * blockSize / sampleRate << " ms)");
    CHECK (worst < 1000.0 * blockSize / sampleRate);
}
