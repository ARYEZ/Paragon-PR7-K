// TALKBACK (1.9.0): a talking drum answers each phrase you play on the next bar line (or beat), following its rise and
// fall. Off, AMOUNT 0 or nothing played: nothing added, bit for bit; the answer starts exactly on the line, in the
// phrase's rhythm, at two tempos and while stopped; each stroke's pitch follows the melody by RANGE (measured in the
// drum alone); a long note bends up; AMOUNT's share of phrases answered, the same every time; the drummer's flams
// and tags only past halfway; NEXT BEAT; the song moving drops what was heard; in the plugin, a played phrase answered,
// its matrix destinations, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/TalkBack.h"
#include "ui/RackSpecs.h"
#include "ui/TalkBackPage.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
    using effects::TalkBack;

    struct Note { double beat, length; float note, velocity = 0.9f; };

    struct Run
    {
        std::vector<float> left, right;
        std::vector<double> strokeTimes;   // seconds a stroke began
        std::vector<float> pitchAfter;     // the drum's pitch 60 ms after each stroke
        int answers = 0, strokes = 0;
    };

    // The effect alone, fed `notes`, for `seconds` at `bpm`; the song from `startPpq` (-1: stopped, its own count).
    Run play (const std::vector<Note>& notes, TalkBack::Settings s, double bpm, double seconds, double startPpq = 0.0, int block = 64)
    {
        TalkBack fx;
        fx.prepare (sampleRate, block);
        fx.seedRandom (1);
        Run run;
        const auto total = (int) (seconds * sampleRate);
        run.left.assign ((size_t) total, 0.0f);
        run.right.assign ((size_t) total, 0.0f);
        const auto beatsPerSample = bpm / 60.0 / sampleRate;
        int lastStrokes = 0;
        std::vector<int> pending;
        for (int at = 0; at < total; at += block)
        {
            const auto n = std::min (block, total - at);
            for (const auto& note : notes)
                for (const auto [beat, press] : { std::pair { note.beat, true }, std::pair { note.beat + note.length, false } })
                {
                    const auto sample = (int) std::llround (beat / beatsPerSample);
                    if (sample >= at && sample < at + n)
                        fx.addNote (sample - at, note.note, note.velocity, press);
                }
            fx.render (n, s, bpm, startPpq < 0.0 ? -1.0 : startPpq + at * beatsPerSample);
            for (int c = 0; c < 2; ++c)
                std::copy (fx.getOutput (c), fx.getOutput (c) + n, (c == 0 ? run.left : run.right).begin() + at);
            if (fx.getStrokeCount() != lastStrokes)
            {
                // Where in this block: the first sample the drum sounds after a silence, else the block's start.
                auto first = at;
                for (int i = 0; i < n; ++i)
                    if (run.left[(size_t) (at + i)] != 0.0f && (at + i == 0 || run.left[(size_t) (at + i - 1)] == 0.0f))
                    {
                        first = at + i;
                        break;
                    }
                for (int k = lastStrokes; k < fx.getStrokeCount(); ++k)
                {
                    run.strokeTimes.push_back (first / sampleRate);
                    pending.push_back (at + (int) (0.06 * sampleRate));
                }
                lastStrokes = fx.getStrokeCount();
            }
            for (auto& p : pending)
                if (p >= 0 && p < at + n)
                {
                    run.pitchAfter.push_back (fx.getPitchNow());
                    p = -1;
                }
        }
        run.answers = fx.getAnswers();
        run.strokes = fx.getStrokeCount();
        return run;
    }

    TalkBack::Settings talk (float amount)
    {
        TalkBack::Settings s;
        s.on = true;
        s.amount = amount;
        s.range = 0.6f;
        s.bend = 0.0f;
        s.levelDb = 0.0f;
        return s;
    }

    // A rising call in the first bar, resting before the second: C4 E4 G4, short.
    const std::vector<Note> call { { 0.0, 0.25, 60.0f }, { 1.0, 0.25, 64.0f }, { 2.0, 0.25, 67.0f } };

    size_t firstSound (const std::vector<float>& x)
    {
        for (size_t i = 0; i < x.size(); ++i)
            if (x[i] != 0.0f)
                return i;
        return x.size();
    }
}

TEST_CASE ("TALKBACK: AMOUNT's share of phrases answered, the drummer's own touches past halfway", "[talkback]")
{
    CHECK (TalkBack::answerChance (0.0f) == 0.0f);
    CHECK (TalkBack::answerChance (0.25f) == 0.5f);
    CHECK (TalkBack::answerChance (0.5f) == 1.0f);
    CHECK (TalkBack::ornamentChance (0.5f) == 0.0f);
    CHECK (TalkBack::ornamentChance (1.0f) == 1.0f);
    // The hash spreads phrases' places evenly and is the same every time.
    int below = 0;
    for (int bar = 0; bar < 2000; ++bar)
    {
        CHECK (TalkBack::phraseHash (bar * 4.0, 1) == TalkBack::phraseHash (bar * 4.0, 1));
        below += TalkBack::phraseHash (bar * 4.0, 1) < 0.5f ? 1 : 0;
    }
    CHECK (below > 900);
    CHECK (below < 1100);
}

TEST_CASE ("TALKBACK off, at AMOUNT 0 or with nothing played adds nothing at all", "[talkback]")
{
    for (const auto amount : { 0.0f, 1.0f })
    {
        auto s = talk (amount);
        const auto played = play (call, s, 120.0, 4.0);
        if (amount == 0.0f)
        {
            CHECK (firstSound (played.left) == played.left.size());
            CHECK (played.answers == 0);
        }
        s.on = false;
        const auto off = play (call, s, 120.0, 4.0);
        CHECK (firstSound (off.left) == off.left.size());
    }
    const auto nothing = play ({}, talk (1.0f), 120.0, 4.0);
    CHECK (firstSound (nothing.left) == nothing.left.size());

    // In a block: the sound passes untouched while the drum is silent.
    TalkBack fx;
    fx.prepare (sampleRate, 256);
    fx.render (256, talk (1.0f), 120.0, 0.0);
    juce::AudioBuffer<float> buffer (2, 256);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 256; ++i)
            buffer.setSample (c, i, 0.3f * std::sin (0.05f * (float) i + (float) c));
    juce::AudioBuffer<float> copy (buffer);
    fx.addTo (juce::dsp::AudioBlock<float> (buffer));
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 256; ++i)
            REQUIRE (buffer.getSample (c, i) == copy.getSample (c, i));
}

TEST_CASE ("TALKBACK answers on the next bar line in the phrase's rhythm, at two tempos and while stopped", "[talkback]")
{
    for (const auto [bpm, ppq] : { std::pair { 90.0, 0.0 }, std::pair { 140.0, 0.0 }, std::pair { 120.0, -1.0 } })
    {
        CAPTURE (bpm, ppq);
        const auto run = play (call, talk (0.5f), bpm, 4.0 * 60.0 / bpm * 2.5, ppq);
        const auto beat = 60.0 / bpm;
        CHECK (run.answers == 1);
        REQUIRE (run.strokeTimes.size() == 3);
        // Silent until the bar line, then each stroke a bar after its note.
        const auto first = (double) firstSound (run.left) / sampleRate;
        CHECK (std::abs (first - 4.0 * beat) < 1.5 / sampleRate);
        for (size_t k = 0; k < 3; ++k)
            CHECK (std::abs (run.strokeTimes[k] - (4.0 + call[k].beat) * beat) < 2.0 / sampleRate + 64.0 / sampleRate);
    }
    // While the phrase's last key is held the bar line passes; it's answered on the next one.
    auto held = call;
    held.back().length = 2.5;   // G4 held from beat 2 to 4.5
    const auto late = play (held, talk (0.5f), 120.0, 6.0);
    CHECK (std::abs ((double) firstSound (late.left) / sampleRate - 8.0 * 0.5) < 1.5 / sampleRate);
}

TEST_CASE ("TALKBACK: each stroke's pitch follows the melody by RANGE, a long note bending up", "[talkback]")
{
    // The middle note (E4) is the drum's PITCH; C4 and G4 at RANGE's share of their 4 and 3 semitones.
    for (const auto range : { 0.5f, 1.0f })
    {
        auto s = talk (0.5f);
        s.range = range;
        const auto run = play (call, s, 120.0, 5.0);
        REQUIRE (run.pitchAfter.size() == 3);
        CHECK_THAT (run.pitchAfter[0], Catch::Matchers::WithinAbs (-4.0f * range, 0.01f));
        CHECK_THAT (run.pitchAfter[1], Catch::Matchers::WithinAbs (0.0f, 0.01f));
        CHECK_THAT (run.pitchAfter[2], Catch::Matchers::WithinAbs (3.0f * range, 0.01f));
        // And in the sound: the E4 stroke rings at PITCH (150 Hz), not a semitone either side.
        const auto from = (size_t) ((2.5 + 0.06) * sampleRate);
        const auto at = levelAt (run.left, 150.0, from, (size_t) (0.12 * sampleRate));
        CHECK (at > 4.0 * levelAt (run.left, 150.0 * std::exp2 (1.0 / 12.0), from, (size_t) (0.12 * sampleRate)));
        CHECK (at > 4.0 * levelAt (run.left, 150.0 * std::exp2 (-1.0 / 12.0), from, (size_t) (0.12 * sampleRate)));
    }
    // A note held a beat bends up BEND x (0.5 + 0.5 AMOUNT) over its length; short ones don't.
    auto s = talk (0.5f);
    s.bend = 2.0f;
    const std::vector<Note> longNote { { 0.0, 1.0, 64.0f } };
    TalkBack fx;
    fx.prepare (sampleRate, 64);
    float atEnd = 0.0f, atStart = 0.0f;
    for (int at = 0; at < (int) (3.2 * sampleRate); at += 64)
    {
        if (at == 0)
            fx.addNote (0, 64.0f, 0.9f, true);
        if (at == (int) (0.5 * sampleRate))
            fx.addNote (0, 64.0f, 0.9f, false);
        fx.render (64, s, 120.0, at * 2.0 / sampleRate);
        if (std::abs (at - (int) (2.03 * sampleRate)) <= 32)
            atStart = fx.getPitchNow();
        if (std::abs (at - (int) (2.49 * sampleRate)) <= 32)
            atEnd = fx.getPitchNow();
    }
    CHECK (atStart < 0.3f);
    CHECK_THAT (atEnd, Catch::Matchers::WithinAbs (2.0f * 0.75f, 0.1f));
}

TEST_CASE ("TALKBACK: past halfway the drummer adds a flam before long notes and a falling tag; NEXT BEAT", "[talkback]")
{
    std::vector<Note> phrase { { 0.0, 0.25, 60.0f }, { 1.0, 1.25, 64.0f } };   // a short note, then a long one
    const auto plain = play (phrase, talk (0.5f), 120.0, 5.0);
    CHECK (plain.strokes == 2);
    const auto full = play (phrase, talk (1.0f), 120.0, 5.0);
    CHECK (full.strokes == 5);   // the two, a flam before the long one, a two-stroke tag
    CHECK (full.answers == 1);
    // The same phrase in the same place: the same answer, sample for sample.
    const auto again = play (phrase, talk (1.0f), 120.0, 5.0);
    CHECK (again.left == full.left);

    auto beatWise = talk (0.5f);
    beatWise.wait = 1;
    const auto quick = play ({ { 0.0, 0.25, 62.0f } }, beatWise, 120.0, 2.0);
    CHECK (std::abs ((double) firstSound (quick.left) / sampleRate - 0.5) < 1.5 / sampleRate);   // on beat 1, not the bar
}

TEST_CASE ("TALKBACK: the song jumping drops what was heard; PAN places the drum; it dies away", "[talkback]")
{
    // Played in bar 1, then the song jumps back before the line: no answer.
    TalkBack fx;
    fx.prepare (sampleRate, 64);
    auto s = talk (1.0f);
    s.pan = -1.0f;
    for (int at = 0; at < (int) (1.6 * sampleRate); at += 64)
    {
        if (at == 0)
            fx.addNote (0, 60.0f, 0.9f, true);
        if (at == 6400)
            fx.addNote (0, 60.0f, 0.9f, false);
        fx.render (64, s, 120.0, at * 2.0 / sampleRate);
    }
    for (int at = 0; at < (int) (3.0 * sampleRate); at += 64)
        fx.render (64, s, 120.0, at * 2.0 / sampleRate);   // from the top again
    CHECK (fx.getAnswers() == 0);

    const auto run = play (call, s, 120.0, 6.0);
    CHECK (rms (run.left, 0, run.left.size()) > 0.01);
    CHECK (rms (run.right, 0, run.right.size()) < 1.0e-6);   // hard left
    CHECK (rms (run.left, (size_t) (5.0 * sampleRate), run.left.size()) < 1.0e-3);   // the last stroke at 3 s rings out (about -70 dB two seconds on)
}

TEST_CASE ("TALKBACK CPU", "[talkback][cpu]")
{
    std::vector<Note> busy;
    for (int k = 0; k < 400; ++k)
        busy.push_back ({ k * 0.5, 0.2, 55.0f + (float) (k % 7) });
    const auto started = juce::Time::getMillisecondCounterHiRes();
    const auto run = play (busy, talk (1.0f), 120.0, 20.0, 0.0, 256);
    const auto share = (juce::Time::getMillisecondCounterHiRes() - started) / 20000.0;
    UNSCOPED_INFO ("TALKBACK: " << 100.0 * share << "% of a core");
    CHECK (run.strokes > 0);
    CHECK (share < 0.05);
}

TEST_CASE ("TALKBACK in the plugin: a played phrase answered, matrix destinations, the SIGNATURE slot and its page", "[talkback][plugin]")
{
    for (const auto* id : { ParamIDs::talkAmount, ParamIDs::talkBend, ParamIDs::talkLevel })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Talkback");
    }
    CHECK (signatureNames()[39] == "TALKBACK");

    const auto render = [] (bool on, float amount, int& answers)
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
        set (ParamIDs::talkOn, on ? 1.0f : 0.0f);
        set (ParamIDs::talkAmount, amount);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int b = 0; b < (int) (3.0 * sampleRate / blockSize); ++b)
        {
            if (b == 0)
                processor.getKeyboardState().noteOn (1, 60, 0.9f);
            if (b == 20)
                processor.getKeyboardState().noteOff (1, 60, 0.0f);
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        answers = processor.getTalkBack().getAnswers();
        if (on && amount > 0.0f)
        {
            CHECK (signatureOn (processor, signatureNames().indexOf ("TALKBACK")));
            TalkBackPage page (processor);
            page.setSize (1200, 700);
            CHECK (page.getShownControls().size() == 10);
            juce::Image image (juce::Image::ARGB, 1200, 700, true);
            juce::Graphics g (image);
            page.paintEntireComponent (g, false);
        }
        return out;
    };
    int answers = 0;
    const auto off = render (false, 0.6f, answers);
    const auto rest = render (true, 0.0f, answers);
    CHECK (rest == off);   // AMOUNT 0: untouched bit for bit
    const auto on = render (true, 0.5f, answers);
    CHECK (answers == 1);
    // Unchanged until the bar line (2 s at 120 BPM), the drum added after it.
    const auto line = (size_t) (2.0 * sampleRate);
    CHECK (std::equal (off.begin(), off.begin() + (long) line - 256, on.begin()));
    double difference = 0.0;
    for (auto i = line; i < on.size(); ++i)
        difference += std::abs (on[i] - off[i]);
    CHECK (difference > 1.0);
}
