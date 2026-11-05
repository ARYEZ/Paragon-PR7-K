// PROXIMITY (0.65.0): the sound of playing, up close. Checked by what it does to the sound: off and at NOISE, ROOM
// and CLOSE 0 the sound is untouched; a press makes a low thud on its sample (louder for a harder note, lower for a
// lower one), a release a quieter click, the pedal its own sounds; WIDTH places a note on the keyboard; the room
// follows the music and leaves silence silent; the hum is at its frequency; CLOSE lifts the lows and leaves the highs,
// waking without a step; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/Proximity.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Proximity::Settings;
    using Kind = effects::Proximity::Kind;
    struct TouchAt { long long sample; float note, velocity; Kind kind; };

    template <typename Input>
    std::array<std::vector<float>, 2> run (double seconds, Input input, Settings s, std::vector<TouchAt> touches = {})
    {
        effects::Proximity fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (5);
        std::array<std::vector<float>, 2> out;
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) std::ceil (seconds * sampleRate / blockSize); ++b)   // at least `seconds`: a measurement up to the end stays inside
        {
            for (size_t i = 0; i < (size_t) blockSize; ++i)
            {
                const auto [a, c] = input (t + (long long) i);
                l[i] = a;
                r[i] = c;
            }
            for (const auto& touch : touches)
                if (touch.sample >= t && touch.sample < t + blockSize)
                    fx.addTouch ((int) (touch.sample - t), touch.note, touch.velocity, touch.kind);
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), s);
            out[0].insert (out[0].end(), l.begin(), l.end());
            out[1].insert (out[1].end(), r.begin(), r.end());
            t += blockSize;
        }
        return out;
    }

    Settings only (float noise, float room = 0.0f, float close = 0.0f)
    {
        Settings s;
        s.on = true;
        s.noise = noise;
        s.room = room;
        s.close = close;
        s.width = 0.0f;
        return s;
    }

    std::pair<float, float> silence (long long) { return { 0.0f, 0.0f }; }
    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }
    double peak (const std::vector<float>& x, size_t from, size_t to)
    {
        double p = 0.0;
        for (auto i = from; i < to && i < x.size(); ++i)
            p = std::max (p, (double) std::abs (x[i]));
        return p;
    }
}

TEST_CASE ("PROXIMITY: off, and at NOISE, ROOM and CLOSE 0, leave the sound exactly as it was", "[proximity]")
{
    const auto input = [] (long long i) { const auto v = 0.3f * (float) std::sin (0.021 * (double) i); return std::pair<float, float> { v, -0.4f * v }; };
    std::vector<float> l, r;
    for (long long i = 0; i < (long long) (2.0 * sampleRate / blockSize) * blockSize; ++i)
    {
        const auto [a, b] = input (i);
        l.push_back (a);
        r.push_back (b);
    }
    const std::vector<TouchAt> touches { { 4000, 60.0f, 1.0f, Kind::press }, { 30000, 60.0f, 1.0f, Kind::release } };
    const auto zero = run (2.0, input, only (0.0f), touches);
    CHECK (zero[0] == l);
    CHECK (zero[1] == r);
    auto off = only (0.8f, 0.8f, 0.8f);
    off.on = false;
    CHECK (run (2.0, input, off, touches)[0] == l);
}

TEST_CASE ("PROXIMITY: a press thuds on its sample; harder is louder, lower is lower", "[proximity]")
{
    const auto press = [] (float note, float velocity) { return run (1.0, silence, only (1.0f), { { 10000, note, velocity, Kind::press } }); };
    const auto hard = press (60.0f, 1.0f), soft = press (60.0f, 0.3f);
    CHECK (peak (hard[0], 0, 10000) == 0.0);           // nothing before it
    CHECK (peak (hard[0], 10000, 10010) > 0.0);         // there on its sample
    const auto hardDb = db (peak (hard[0], 10000, 14000)), softDb = db (peak (soft[0], 10000, 14000));
    INFO ("hard " << hardDb << " dB, soft " << softDb << " dB");
    CHECK (hardDb > -12.0);   // NOISE at full: -6 dB
    CHECK (hardDb - softDb > 6.0);
    CHECK (peak (hard[0], at (0.6), at (1.0)) < 1.0e-5);   // and gone
    // Low notes thud lower: the low key's energy sits lower than the high key's.
    const auto low = press (24.0f, 1.0f), high = press (100.0f, 1.0f);
    const auto lowRatio = levelAt (low[0], 60.0, 10000, 2400) / (levelAt (low[0], 160.0, 10000, 2400) + 1e-12);
    const auto highRatio = levelAt (high[0], 60.0, 10000, 2400) / (levelAt (high[0], 160.0, 10000, 2400) + 1e-12);
    INFO ("60 Hz over 160 Hz: low key " << lowRatio << ", high key " << highRatio);
    CHECK (lowRatio > highRatio * 2.0);
}

TEST_CASE ("PROXIMITY: a release clicks, quieter than the press; the pedal is heard going down and up", "[proximity]")
{
    const auto pressed = run (1.0, silence, only (1.0f), { { 10000, 60.0f, 1.0f, Kind::press } });
    const auto released = run (1.0, silence, only (1.0f), { { 10000, 60.0f, 1.0f, Kind::release } });
    const auto pressDb = db (peak (pressed[0], 10000, 20000)), releaseDb = db (peak (released[0], 10000, 20000));
    INFO ("press " << pressDb << " dB, release " << releaseDb << " dB");
    CHECK (releaseDb < pressDb - 6.0);
    CHECK (releaseDb > pressDb - 30.0);
    for (const auto kind : { Kind::pedalDown, Kind::pedalUp })
    {
        const auto pedal = run (1.0, silence, only (1.0f), { { 10000, 0.0f, 1.0f, kind } });
        CHECK (db (peak (pedal[0], 10000, 20000)) > pressDb - 30.0);
    }
}

TEST_CASE ("PROXIMITY: WIDTH places a note where it is on the keyboard", "[proximity]")
{
    auto s = only (1.0f);
    s.width = 1.0f;
    const auto low = run (0.5, silence, s, { { 1000, 30.0f, 1.0f, Kind::press } });
    const auto high = run (0.5, silence, s, { { 1000, 100.0f, 1.0f, Kind::press } });
    CHECK (rms (low[0], 1000, 8000) > rms (low[1], 1000, 8000) * 2.0);
    CHECK (rms (high[1], 1000, 8000) > rms (high[0], 1000, 8000) * 2.0);
    const auto centred = run (0.5, silence, only (1.0f), { { 1000, 30.0f, 1.0f, Kind::press } });
    CHECK (std::abs (rms (centred[0], 1000, 8000) - rms (centred[1], 1000, 8000)) < 1.0e-6);
}

TEST_CASE ("PROXIMITY: the room follows the music and leaves silence silent; the hum is at its frequency", "[proximity]")
{
    // A tone for two seconds, then nothing.
    const auto tone = [] (long long i) { const auto v = i < (long long) (2.0 * sampleRate) ? 0.3f * (float) std::sin (0.05 * (double) i) : 0.0f; return std::pair<float, float> { v, v }; };
    auto s = only (0.0f, 1.0f);   // ROOM at full: -30 dB
    s.hum = 1;                     // 50 Hz
    const auto out = run (10.0, tone, s);
    std::vector<float> room;
    for (size_t i = 0; i < out[0].size(); ++i)
        room.push_back (out[0][i] - tone ((long long) i).first);
    const auto during = db (rms (room, at (1.0), at (2.0)));
    INFO ("the room while playing " << during << " dB");
    CHECK (during > -45.0);
    CHECK (during < -20.0);
    CHECK (db (levelAt (room, 50.0, at (1.0), at (1.0))) > db (levelAt (room, 70.0, at (1.0), at (1.0))) + 10.0);
    CHECK (peak (out[0], at (9.0), at (10.0)) == 0.0);   // silence after: silent
    s.hum = 0;
    const auto noHum = run (3.0, tone, s);
    std::vector<float> quiet;
    for (size_t i = 0; i < noHum[0].size(); ++i)
        quiet.push_back (noHum[0][i] - tone ((long long) i).first);
    CHECK (db (levelAt (quiet, 50.0, at (1.0), at (1.0))) < db (levelAt (room, 50.0, at (1.0), at (1.0))) - 10.0);
}

TEST_CASE ("PROXIMITY: CLOSE lifts the lows and leaves the highs, and wakes without a step", "[proximity]")
{
    const auto sine = [] (double hz) { return [hz] (long long i) { const auto v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate); return std::pair<float, float> { v, v }; }; };
    const auto lowLift = db (rms (run (1.0, sine (60.0), only (0.0f, 0.0f, 1.0f))[0], at (0.5), at (1.0))) - db (0.3 / std::sqrt (2.0));
    const auto highLift = db (rms (run (1.0, sine (2000.0), only (0.0f, 0.0f, 1.0f))[0], at (0.5), at (1.0))) - db (0.3 / std::sqrt (2.0));
    INFO ("60 Hz +" << lowLift << " dB, 2 kHz +" << highLift << " dB");
    CHECK (std::abs (lowLift - 6.0) < 0.6);
    CHECK (std::abs (highLift) < 0.3);
    // Turned up from 0 halfway through a 200 Hz tone: no step bigger than the tone's own.
    effects::Proximity fx;
    fx.prepare (sampleRate, blockSize);
    std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize), all;
    float* ptrs[] = { l.data(), r.data() };
    for (int b = 0; b < 200; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
            l[(size_t) i] = r[(size_t) i] = sine (200.0) (b * blockSize + i).first;
        fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), only (0.0f, 0.0f, b < 100 ? 0.0f : 1.0f));
        all.insert (all.end(), l.begin(), l.end());
    }
    auto step = 0.0f;
    for (size_t i = 1; i < all.size(); ++i)
        step = std::max (step, std::abs (all[i] - all[i - 1]));
    CHECK (step < 0.3f * (float) (juce::MathConstants<double>::twoPi * 200.0 / sampleRate) * 2.2f);
}

TEST_CASE ("CPU: PROXIMITY, a fast run with the room", "[proximity][cpu]")
{
    std::vector<TouchAt> touches;
    for (long long t = 0; t < (long long) (20.0 * sampleRate); t += 3000)
    {
        touches.push_back ({ t, 40.0f + (float) (t / 3000 % 40), 0.8f, Kind::press });
        touches.push_back ({ t + 2500, 40.0f + (float) (t / 3000 % 40), 0.8f, Kind::release });
    }
    const auto started = std::chrono::steady_clock::now();
    run (20.0, [] (long long i) { const auto v = 0.2f * (float) std::sin (0.03 * (double) i); return std::pair<float, float> { v, v }; }, only (0.8f, 0.6f, 0.5f), touches);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("PROXIMITY, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input included)");
    CHECK (seconds / 20.0 * 100.0 < 5.0);
}
