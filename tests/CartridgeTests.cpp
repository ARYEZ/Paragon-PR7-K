// CARTRIDGE (1.10.0): the synth through an old game console's sound chip. The chip's tuning is a clock divided by a
// whole number (high notes come out off, as the formula says); its loudness moves in 16 steps; off, or AMOUNT 0, every
// note untouched bit for bit; from a quarter a held chord's notes take turns from the lowest up on RATE's grid (at two
// tempos and with the song stopped); at the top stage they take turns at 60 Hz and only the newest CHANNELS sound;
// effects::Cartridge's crunch holds samples and steps them to few levels, its glitches come at about the rate asked, a
// silent side stays silent, and below its stage it rests bit for bit; in the plugin its parameters, matrix
// destinations, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/Cartridge.h"
#include "dsp/synth/Cartridge.h"
#include "ui/CartridgePage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <set>

namespace
{
    using namespace synth;
    using Catch::Matchers::WithinAbs;
    using Catch::Matchers::WithinRel;

    Player sinePlayer (cartridge::Settings s, double bpm = 120.0, double ppq = -1.0)
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.filter.on = false;
        p.settings.amp.envelope.attackMs = 0.0f;
        p.settings.amp.envelope.sustain = 1.0f;
        p.settings.amp.envelope.releaseMs = 10.0f;
        p.settings.modulation.bpm = bpm;
        p.settings.modulation.ppq = ppq;
        p.settings.cartridge = s;
        return p;
    }

    cartridge::Settings chip (float amount)
    {
        cartridge::Settings s;
        s.on = true;
        s.amount = amount;
        s.rate = 0;   // 1/16
        return s;
    }

    // Which of `notes` is loudest over a window (the one sounding).
    int loudestOf (const std::vector<float>& x, const std::vector<int>& notes, double from, double to)
    {
        int best = -1;
        double bestLevel = 0.0;
        for (size_t k = 0; k < notes.size(); ++k)
        {
            const auto level = levelAt (x, noteHz (notes[k]), (size_t) (from * sampleRate), (size_t) ((to - from) * sampleRate));
            if (level > bestLevel)
            {
                bestLevel = level;
                best = (int) k;
            }
        }
        return best;
    }
}

TEST_CASE ("CARTRIDGE: the chip's tuning and its 16 steps of loudness", "[cartridge]")
{
    // A4: 1789773 / (16 x 254) = 440.40 Hz; C7: 1789773 / (16 x 53) = 2110.6 Hz, about 14 cents sharp.
    CHECK_THAT (cartridge::chipHz (69.0), WithinAbs (1789773.0 / (16.0 * 254.0), 1.0e-9));
    CHECK_THAT (cartridge::chipHz (96.0), WithinAbs (1789773.0 / (16.0 * 53.0), 1.0e-9));
    CHECK (std::abs (cartridge::tuningSemis (96.0)) > 0.1f);
    CHECK (std::abs (cartridge::tuningSemis (57.0)) < 0.02f);
    // Every note within half a step of the chip's grid, never off by more.
    for (int note = 24; note < 120; ++note)
        CHECK (std::abs (cartridge::tuningSemis (note)) < 0.5f);
    CHECK (cartridge::stepped (0.5f) == 8.0f / 15.0f);
    CHECK (cartridge::stepped (1.0f) == 1.0f);
    CHECK (cartridge::stepped (0.02f) == 0.0f);

    // In the engine: C7 plays at the chip's pitch, not the true one.
    auto p = sinePlayer (chip (0.1f));
    p.on (96);
    const auto out = p.render (1.0)[0];
    CHECK_THAT (frequencyOf (out, 4800, 48000), WithinRel (cartridge::chipHz (96.0), 0.0005));

    // A sustain of 0.5 comes out at 8/15 (the nearest of the 16 steps).
    const auto held = [] (bool on)
    {
        auto q = sinePlayer (chip (on ? 0.1f : 0.0f));
        q.settings.amp.envelope.sustain = 0.5f;
        q.settings.amp.envelope.decayMs = 5.0f;
        q.on (57);
        const auto x = q.render (1.0)[0];
        return rms (x, 24000, 48000);
    };
    CHECK_THAT (held (true) / held (false), WithinRel (16.0 / 15.0, 0.005));
}

TEST_CASE ("CARTRIDGE: off, or AMOUNT 0, leaves every note untouched bit for bit", "[cartridge]")
{
    const auto play = [] (cartridge::Settings s)
    {
        auto p = sinePlayer (s);
        p.osc().wave = OscWave::saw;
        p.settings.amp.envelope.attackMs = 30.0f;
        p.settings.amp.envelope.sustain = 0.6f;
        p.settings.amp.envelope.releaseMs = 300.0f;
        for (const auto n : { 48, 60, 64, 67 })
            p.on (n);
        auto a = p.render (1.0);
        p.off (64);
        auto b = p.render (1.0);
        a[0].insert (a[0].end(), b[0].begin(), b[0].end());
        a[1].insert (a[1].end(), b[1].begin(), b[1].end());
        return a;
    };
    const auto plain = play ({});
    auto off = chip (1.0f);
    off.on = false;
    CHECK (play (off) == plain);
    CHECK (play (chip (0.0f)) == plain);
    CHECK (play (chip (0.0009f)) == plain);
    CHECK (play (chip (1.0f)) != plain);
}

TEST_CASE ("CARTRIDGE: from a quarter, a held chord's notes take turns from the lowest up, in time with the song", "[cartridge]")
{
    const std::vector<int> chord { 60, 64, 67 };
    for (const auto bpm : { 120.0, 90.0 })
        for (const auto rate : { 0, 1 })
            for (const auto following : { false, true })
            {
                auto s = chip (0.4f);
                s.rate = rate;
                // Following the song, the song sits half a turn in, so the first turn is half as long.
                const auto turnBeats = 0.25 / (double) (1 << rate);
                auto p = sinePlayer (s, bpm, following ? 0.5 * turnBeats : -1.0);
                for (const auto n : chord)
                    p.on (n);
                const auto seconds = 2.0;
                if (following)
                {
                    // The song moves on with each block (the harness doesn't move it).
                    std::vector<float> out;
                    for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
                    {
                        p.settings.modulation.ppq = 0.5 * turnBeats + (double) (b * blockSize) / sampleRate * bpm / 60.0;
                        const auto x = p.render ((double) blockSize / sampleRate)[0];
                        out.insert (out.end(), x.begin(), x.end());
                    }
                    const auto turnSeconds = turnBeats * 60.0 / bpm;
                    for (int k = 1; k < 10; ++k)
                    {
                        const auto start = turnSeconds * ((double) k - 0.5);
                        INFO ("bpm " << bpm << " rate " << rate << " (song) turn " << k);
                        CHECK (loudestOf (out, chord, start + 0.15 * turnSeconds, start + 0.85 * turnSeconds) == k % 3);
                    }
                }
                else
                {
                    const auto out = p.render (seconds)[0];
                    const auto turnSeconds = turnBeats * 60.0 / bpm;
                    for (int k = 0; k < 10; ++k)
                    {
                        const auto start = turnSeconds * (double) k;
                        INFO ("bpm " << bpm << " rate " << rate << " (stopped) turn " << k);
                        CHECK (loudestOf (out, chord, start + 0.15 * turnSeconds, start + 0.85 * turnSeconds) == k % 3);
                    }
                }
            }

    // One note alone just sounds (nothing to take turns with), and a turn's edges have no click: no jump over a sine's
    // own largest step.
    auto solo = sinePlayer (chip (0.4f));
    solo.on (69);
    const auto alone = solo.render (1.0)[0];
    CHECK (rms (alone, 4800, 48000) > 0.5);
    auto chordPlayer = sinePlayer (chip (0.4f));
    for (const auto n : chord)
        chordPlayer.on (n);
    const auto x = chordPlayer.render (1.0)[0];
    double worst = 0.0;
    for (size_t i = 2000; i < x.size(); ++i)
        worst = juce::jmax (worst, (double) std::abs (x[i] - x[i - 1]));
    CHECK (worst < 2.0 * juce::MathConstants<double>::twoPi * noteHz (67) / sampleRate * 1.5);
}

TEST_CASE ("CARTRIDGE: at the top stage the turns run at 60 Hz and only the newest CHANNELS notes sound", "[cartridge]")
{
    auto s = chip (0.8f);
    s.channels = 2;
    auto p = sinePlayer (s);
    p.on (48);
    p.render (0.05);
    p.on (55);
    p.render (0.05);
    p.on (64);
    p.render (0.05);
    p.on (72);
    const auto out = p.render (1.0)[0];
    const auto level = [&out] (int note) { return levelAt (out, cartridge::chipHz (note), 4800, 38400); };
    // The two newest (64, 72) sound, taking turns; the two oldest are gone.
    CHECK (db (level (64)) > -15.0);
    CHECK (db (level (72)) > -15.0);
    // (the chopping at 60 Hz spreads the sounding notes a little: about -39 dB lands near the stolen notes)
    CHECK (db (level (48)) < db (level (64)) - 30.0);
    CHECK (db (level (55)) < db (level (64)) - 30.0);
    // Taking turns every 1/60 s: over a frame one note, over the next the other.
    const auto frame = 1.0 / 60.0;
    int changes = 0, last = -1;
    for (int k = 6; k < 40; ++k)
    {
        const auto at = loudestOf (out, { 64, 72 }, k * frame + 0.2 * frame, k * frame + 0.8 * frame);
        changes += last >= 0 && at != last ? 1 : 0;
        last = at;
    }
    CHECK (changes >= 30);
}

TEST_CASE ("CARTRIDGE's crunch and glitches: held, stepped, at about the rate asked; silence stays silent; rests bit for bit", "[cartridge]")
{
    effects::Cartridge fx;
    fx.prepare (sampleRate, blockSize);
    fx.seedRandom (1);
    const auto run = [&fx] (const synth::cartridge::Settings& s, double seconds, bool silentRight)
    {
        const auto total = (int) (seconds * sampleRate);
        std::array<std::vector<float>, 2> io;
        for (int c = 0; c < 2; ++c)
        {
            io[(size_t) c].resize ((size_t) total);
            for (int i = 0; i < total; ++i)
                io[(size_t) c][(size_t) i] = c == 1 && silentRight ? 0.0f : 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / (float) sampleRate);
        }
        for (int start = 0; start < total; start += blockSize)
        {
            const auto n = juce::jmin (blockSize, total - start);
            float* channels[] = { io[0].data() + start, io[1].data() + start };
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, (size_t) n), s);
        }
        return io;
    };
    // Below half: untouched.
    const auto plain = run ({}, 0.0, false);
    {
        const auto x = run (chip (0.45f), 1.0, false);
        auto reference = x;
        for (int i = 0; i < (int) x[0].size(); ++i)
            reference[0][(size_t) i] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / (float) sampleRate);
        CHECK (x[0] == reference[0]);
        CHECK (fx.isResting());
    }
    // At the top with all the crunch: each value held 4 samples, at most 2 x 32 + 1 levels.
    auto s = chip (1.0f);
    s.crunch = 1.0f;
    s.glitch = 0.0f;
    {
        const auto x = run (s, 1.0, true);
        std::set<float> values (x[0].begin(), x[0].end());
        CHECK (values.size() <= 65);
        CHECK (values.size() >= 20);
        int changes = 0;
        for (size_t i = 1; i < x[0].size(); ++i)
            changes += x[0][i] != x[0][i - 1] ? 1 : 0;
        CHECK (changes <= (int) x[0].size() / 4 + 1);
        // The silent side stays silent.
        CHECK (std::all_of (x[1].begin(), x[1].end(), [] (float v) { return v == 0.0f; }));
        CHECK (! fx.isResting());
    }
    // Glitches: about 2.5 a second at full GLITCH (less the frames a repeat takes).
    s.crunch = 0.0f;
    s.glitch = 1.0f;
    const auto before = fx.getGlitches();
    const auto x = run (s, 20.0, true);
    const auto count = fx.getGlitches() - before;
    UNSCOPED_INFO ("CARTRIDGE glitches in 20 s: " << count);
    CHECK (count >= 25);
    CHECK (count <= 60);
    CHECK (std::all_of (x[1].begin(), x[1].end(), [] (float v) { return v == 0.0f; }));
    // ...and back to rest, untouched again.
    run (chip (0.2f), 0.2, false);
    CHECK (fx.isResting());
    (void) plain;
}

TEST_CASE ("CARTRIDGE in the plugin: parameters, matrix destinations, a chord through the chip, the SIGNATURE slot and its page", "[cartridge][plugin]")
{
    for (const auto* id : { ParamIDs::cartAmount, ParamIDs::cartCrunch, ParamIDs::cartGlitch })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Cartridge");
    }
    CHECK (signatureNames()[40] == "CARTRIDGE");

    const auto render = [] (bool on, float amount, int& notesInChip)
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
        set (ParamIDs::cartOn, on ? 1.0f : 0.0f);
        set (ParamIDs::cartAmount, amount);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        notesInChip = 0;
        for (int b = 0; b < (int) (2.0 * sampleRate / blockSize); ++b)
        {
            if (b == 0)
                for (const auto n : { 60, 64, 67 })
                    processor.getKeyboardState().noteOn (1, n, 0.9f);
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            notesInChip = juce::jmax (notesInChip, processor.getCartridgeNotes());
        }
        if (on && amount > 0.0f)
        {
            CHECK (signatureOn (processor, signatureNames().indexOf ("CARTRIDGE")));
            CartridgePage page (processor);
            page.setSize (1200, 700);
            CHECK (page.getShownControls().size() == 6);
            juce::Image image (juce::Image::ARGB, 1200, 700, true);
            juce::Graphics g (image);
            page.paintEntireComponent (g, false);
        }
        return out;
    };
    int notes = 0;
    const auto off = render (false, 1.0f, notes);
    const auto rest = render (true, 0.0f, notes);
    CHECK (rest == off);
    const auto full = render (true, 1.0f, notes);
    CHECK (notes == 3);
    CHECK (full != off);
    CHECK (rms (full, 9600, full.size()) > 0.25 * rms (off, 9600, off.size()));
}
