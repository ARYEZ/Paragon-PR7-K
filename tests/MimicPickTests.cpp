// MIMIC's PICK A NOTE (1.5.0): a whole stem cut into its notes, each scored for MIMIC, the best chosen. Stems made
// here with known notes: every note is found where it starts, at its pitch; a fresh attack on the same pitch splits,
// vibrato and a slide inside a note don't; legato steps split; the best is the long held note that rings out; the
// note chosen is analysed as that note alone; a chord or a noise says it has nothing to follow; the panel shows the
// notes, starts on the best, and a click on another makes the sound from it.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"
#include "dsp/synth/Mimic.h"
#include "ui/MimicPanel.h"

#include <cmath>

namespace
{
    constexpr double rate = 48000.0;

    struct N
    {
        double start, length;   // seconds
        double note;            // MIDI (fractional ok)
        double releaseSeconds = 0.08;
        double vibratoCents = 0.0;
        double slideSemitones = 0.0;   // over the note's length
        double level = 1.0;
    };

    // Saw-like notes (12 harmonics), a 5 ms attack, held, then falling over `releaseSeconds` after `length`; notes that
    // overlap their release with the next are summed (as a mono synth's legato wouldn't be: these are separate voices).
    juce::AudioBuffer<float> stem (const std::vector<N>& notes, double seconds, int channels = 2)
    {
        juce::AudioBuffer<float> b (channels, (int) (seconds * rate));
        b.clear();
        for (const auto& n : notes)
        {
            double phase = 0.0;
            const auto from = (int) (n.start * rate), to = juce::jmin (b.getNumSamples(), (int) ((n.start + n.length + n.releaseSeconds) * rate));
            for (int i = from; i < to; ++i)
            {
                const auto t = (i - from) / rate;
                const auto pitch = n.note + n.slideSemitones * juce::jmin (1.0, t / n.length)
                                 + n.vibratoCents / 100.0 * std::sin (juce::MathConstants<double>::twoPi * 5.5 * t);
                const auto hz = 440.0 * std::exp2 ((pitch - 69.0) / 12.0);
                phase += hz / rate;
                double sum = 0.0;
                for (int k = 1; k <= 12 && k * hz < 20000.0; ++k)
                    sum += std::sin (juce::MathConstants<double>::twoPi * k * phase) / k;
                const auto env = t < 0.005 ? t / 0.005 : t < n.length ? 1.0 : std::pow (10.0, -60.0 * (t - n.length) / n.releaseSeconds / 20.0);
                for (int c = 0; c < channels; ++c)
                    b.addSample (c, i, (float) (0.25 * n.level * env * sum));
            }
        }
        return b;
    }

    const synth::mimic::FoundNote* nearest (const synth::mimic::Phrase& p, double start)
    {
        const synth::mimic::FoundNote* best = nullptr;
        for (const auto& f : p.notes)
            if (best == nullptr || std::abs (f.start - start) < std::abs (best->start - start))
                best = &f;
        return best;
    }
}

TEST_CASE ("MIMIC PICK A NOTE: a phrase's notes are found where they start, at their pitches", "[mimic][pick]")
{
    // An arpeggio-like phrase: short notes with gaps, one long held note, then two more.
    const std::vector<N> notes { { 0.10, 0.20, 57 }, { 0.40, 0.20, 60 }, { 0.70, 0.20, 64 }, { 1.00, 1.40, 69, 0.4 },
                                 { 3.00, 0.30, 67 }, { 3.50, 0.30, 64.3 } };
    const auto p = synth::mimic::findNotes (stem (notes, 4.2), rate, "Phrase");
    REQUIRE (p.valid);
    REQUIRE (p.notes.size() == notes.size());
    for (size_t n = 0; n < notes.size(); ++n)
    {
        INFO ("note " << n);
        const auto& f = p.notes[n];
        CHECK_THAT (f.start, Catch::Matchers::WithinAbs (notes[n].start, 0.025));
        CHECK (f.note == juce::roundToInt (notes[n].note));
        CHECK (f.clarity > 0.85f);
        CHECK (f.ringsOut);
    }
    CHECK_THAT (p.notes[5].cents, Catch::Matchers::WithinAbs (30.0, 6.0));
    // The best: the long held note.
    CHECK (p.best == 3);
}

TEST_CASE ("MIMIC PICK A NOTE: a fresh attack on the same pitch splits; vibrato and a slide inside a note don't; legato steps do", "[mimic][pick]")
{
    // Three repeated C4s, legato (each starting as the last ends, a fresh 5 ms attack after a 30 ms dip).
    {
        const std::vector<N> notes { { 0.1, 0.4, 60, 0.03 }, { 0.53, 0.4, 60, 0.03 }, { 0.96, 0.4, 60, 0.03 } };
        const auto p = synth::mimic::findNotes (stem (notes, 1.6), rate, "Repeats");
        REQUIRE (p.valid);
        CHECK (p.notes.size() == 3);
    }
    // One long note with wide vibrato (+-40 cents), then one sliding up two semitones over a second: two notes.
    {
        const std::vector<N> notes { { 0.1, 1.5, 62, 0.1, 40.0 }, { 2.0, 1.0, 62, 0.1, 0.0, 2.0 } };
        const auto p = synth::mimic::findNotes (stem (notes, 3.3), rate, "Vibrato and slide");
        REQUIRE (p.valid);
        for (const auto& f : p.notes)
            UNSCOPED_INFO ("found " << f.start << "-" << f.end << " note " << f.note << " wobble " << f.wobbleCents);
        CHECK (p.notes.size() == 2);
        CHECK (p.notes[0].wobbleCents > 15.0f);
    }
    // A legato line: each note starts as the last is cut (no gap, no dip): still one note each.
    {
        juce::AudioBuffer<float> b (1, (int) (2.0 * rate));
        double phase = 0.0;
        const double pitches[] { 60, 62, 64, 65 };
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto t = i / rate;
            const auto step = juce::jmin (3, (int) ((t - 0.1) / 0.4));
            const auto hz = 440.0 * std::exp2 ((pitches[juce::jmax (0, step)] - 69.0) / 12.0);
            phase += hz / rate;
            double sum = 0.0;
            for (int k = 1; k <= 12; ++k)
                sum += std::sin (juce::MathConstants<double>::twoPi * k * phase) / k;
            b.setSample (0, i, t < 0.1 || t > 1.7 ? 0.0f : (float) (0.25 * sum));
        }
        const auto p = synth::mimic::findNotes (b, rate, "Legato");
        REQUIRE (p.valid);
        REQUIRE (p.notes.size() == 4);
        for (int n = 0; n < 4; ++n)
        {
            CHECK (p.notes[(size_t) n].note == (int) pitches[n]);
            CHECK_THAT (p.notes[(size_t) n].start, Catch::Matchers::WithinAbs (0.1 + 0.4 * n, 0.05));
            CHECK (p.notes[(size_t) n].ringsOut == (n == 3));   // cut by the next, but the last
        }
    }
}

TEST_CASE ("MIMIC PICK A NOTE: the chosen note is analysed alone", "[mimic][pick]")
{
    const std::vector<N> notes { { 0.10, 0.25, 50 }, { 0.50, 1.20, 57, 0.5 }, { 2.40, 0.25, 62 } };
    const auto p = synth::mimic::findNotes (stem (notes, 3.0), rate, "Stem");
    REQUIRE (p.valid);
    REQUIRE (p.notes.size() == 3);
    CHECK (p.best == 1);
    for (int n = 0; n < 3; ++n)
    {
        const auto a = synth::mimic::analyseNote (p, n);
        INFO ("note " << n);
        REQUIRE (a.valid);
        CHECK (a.rootNote == (int) notes[(size_t) n].note);
        CHECK (a.name.contains (synth::mimic::noteName (a.rootNote)));
        // Only that note: the cut is as long as the note and its release, nothing of the next.
        CHECK (a.seconds < notes[(size_t) n].length + notes[(size_t) n].releaseSeconds + 0.1);
    }
    // The held one: held, sustaining near full, released over about half a second.
    const auto held = synth::mimic::analyseNote (p, 1);
    CHECK (held.sustained);
    CHECK (held.sustain > 0.8f);
    // One note on its own: one note found, analysed the same as the whole file.
    const auto single = stem ({ { 0.05, 1.0, 64, 0.3 } }, 1.5);
    const auto one = synth::mimic::findNotes (single, rate, "Single");
    REQUIRE (one.valid);
    REQUIRE (one.notes.size() == 1);
    CHECK (synth::mimic::analyseNote (one, 0).rootNote == synth::mimic::analyse (single, rate, "Single").rootNote);
}

TEST_CASE ("MIMIC PICK A NOTE: a dense chord or a noise has nothing to follow", "[mimic][pick]")
{
    juce::Random random (3);
    juce::AudioBuffer<float> noise (1, (int) (1.0 * rate));
    for (int i = 0; i < noise.getNumSamples(); ++i)
        noise.setSample (0, i, 0.3f * (random.nextFloat() * 2.0f - 1.0f));
    const auto p = synth::mimic::findNotes (noise, rate, "Noise");
    CHECK_FALSE (p.valid);
    CHECK (p.problem.isNotEmpty());
    // A cluster of four notes a semitone apart: no clear pitch, or a poorly scored one.
    const auto cluster = stem ({ { 0.1, 1.0, 60 }, { 0.1, 1.0, 61 }, { 0.1, 1.0, 62 }, { 0.1, 1.0, 63 } }, 1.4);
    const auto c = synth::mimic::findNotes (cluster, rate, "Cluster");
    if (c.valid)
        for (const auto& f : c.notes)
            CHECK (f.score < 0.5f);
}

TEST_CASE ("MIMIC's panel picks a note: the best at first, another by a click, the original the chosen note", "[mimic][pick]")
{
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, rate, 512);
    processor.prepareToPlay (rate, 512);
    const std::vector<N> notes { { 0.10, 0.25, 50 }, { 0.50, 1.20, 57, 0.5 }, { 2.40, 0.25, 62 } };
    const auto audio = stem (notes, 3.0);
    MimicPanel panel (processor);
    panel.setSize (1200, 620);
    panel.setPhrase (synth::mimic::findNotes (audio, rate, "Stem"));
    REQUIRE (panel.getPhrase().has_value());
    CHECK (panel.getChosenNote() == 1);
    REQUIRE (panel.getAnalysis().has_value());
    CHECK (panel.getAnalysis()->rootNote == 57);
    // Click the first note's block.
    juce::Image image (juce::Image::ARGB, 1200, 620, true);
    juce::Graphics g (image);
    panel.paintEntireComponent (g, false);
    panel.chooseNote (0);
    CHECK (panel.getChosenNote() == 0);
    CHECK (panel.getAnalysis()->rootNote == 50);
    panel.makeSound();
    CHECK (processor.getCurrentRig().name.contains ("D3"));   // made from the first note, D3;
    panel.paintEntireComponent (g, false);
}

//======================================================================================================================
// MIMIC on a real recording (run by hand): PARAGON_MIMIC_FILE=<wav> [PARAGON_RENDER=<folder>] "[.mimicfile]". Prints
// the notes found, what the best was measured to be and the match before and after REFINE; with PARAGON_RENDER writes
// the made and refined sounds (the recording's note held as long) beside the cut note, to compare by ear.
#include "MimicRefiner.h"
#include "rigs/MimicRig.h"

TEST_CASE ("MIMIC on a recording", "[.mimicfile]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_MIMIC_FILE", {});
    REQUIRE (path.isNotEmpty());
    const auto phrase = synth::mimic::findNotesInFile (juce::File (path));
    REQUIRE (phrase.valid);
    std::printf ("%s: %.1f s, %d notes\n", phrase.name.toRawUTF8(), phrase.seconds, (int) phrase.notes.size());
    for (size_t k = 0; k < phrase.notes.size() && k < 12; ++k)
    {
        const auto& n = phrase.notes[k];
        std::printf ("  %2d %6.2f-%6.2f s %s %+5.1f c  level %5.1f  clarity %.2f  wobble %.1f c  %s score %.2f%s\n", (int) k, n.start,
                     n.end, synth::mimic::noteName (n.note).toRawUTF8(), n.cents, n.levelDb, n.clarity, n.wobbleCents,
                     n.ringsOut ? "rings out" : "cut off", n.score, (int) k == phrase.best ? "  <- best" : "");
    }
    REQUIRE (phrase.best >= 0);
    const auto a = synth::mimic::analyseNote (phrase, phrase.best);
    REQUIRE (a.valid);
    std::printf ("analysed: %s %+.1f c (%.2f Hz) clarity %.2f, %s\n  attack %.1f ms decay %.0f ms sustain %.2f release %.0f ms\n"
                 "  vibrato %.2f Hz %.1f c delay %.0f ms; swoop %d hops; detune %.1f c; noise %.2f colour %.2f; width %.2f\n"
                 "  table %d frames over %.2f s; note %.2f-%.2f s, settles %.2f, release %.2f\n",
                 synth::mimic::noteName (a.rootNote).toRawUTF8(), a.cents, a.hz, a.clarity, a.sustained ? "held" : "dying",
                 a.attackMs, a.decayMs, a.sustain, a.releaseMs, a.vibratoHz, a.vibratoCents, a.vibratoDelayMs, (int) a.swoop.size(),
                 a.detuneCents, a.noise, a.noiseColour, a.width, a.table.numFrames(), a.travelSeconds, a.start, a.end,
                 a.settle, a.releaseStart);

    std::printf ("  movement: %.3f s a round (alike %.2f), brightness %.2f oct, level %.1f dB; %d attack frames\n",
                 a.movementSeconds, a.movementStrength, a.movementOctaves, a.movementDb, a.attackFrames);
    // PARAGON_MIMIC_OFF: what to leave out (any of tone envelope vibrato swoop unison noise movement), to find what helps.
    mimic::Choices choices;
    const auto off = juce::SystemStats::getEnvironmentVariable ("PARAGON_MIMIC_OFF", {});
    choices.tone = ! off.contains ("tone");
    choices.envelope = ! off.contains ("envelope");
    choices.vibrato = ! off.contains ("vibrato");
    choices.swoop = ! off.contains ("swoop");
    choices.unison = ! off.contains ("unison");
    choices.noise = ! off.contains ("noise");
    choices.movement = ! off.contains ("movement");
    MimicRefiner refiner;
    auto made = mimic::makeRig (a, choices);
    const auto first = refiner.score (made, a);
    std::printf ("MATCH %.1f%% (tone %.2f dB, loudness %.2f dB)\n", first.match, first.toneDb, first.loudnessDb);
    refiner.begin (made, a, choices);
    if (! off.contains ("refine"))
        while (refiner.step()) {}
    auto refined = refiner.getBest();
    std::printf ("refined MATCH %.1f%% (tone %.2f dB, loudness %.2f dB) in %d plays\n", refiner.getBestScore().match,
                 refiner.getBestScore().toneDb, refiner.getBestScore().loudnessDb, refiner.getDone());

    const auto folder = juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER", {});
    if (folder.isEmpty())
        return;
    const auto write = [&] (const juce::String& name, const std::vector<float>& mono, double sampleRate)
    {
        juce::AudioBuffer<float> b (1, (int) mono.size());
        std::copy (mono.begin(), mono.end(), b.getWritePointer (0));
        juce::WavAudioFormat wav;
        const auto file = juce::File (folder).getChildFile (name + ".wav");
        file.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (file), sampleRate, 1, 24, {}, 0));
        REQUIRE (w != nullptr);
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    };
    write ("recording", *a.audio, a.sampleRate);
    write ("made", refiner.play (made, a), MimicRefiner::renderRate);
    write ("refined", refiner.play (refined, a), MimicRefiner::renderRate);
}
