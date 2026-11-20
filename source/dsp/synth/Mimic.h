#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Wavetable.h"

#include <array>
#include <memory>
#include <vector>

// MIMIC (0.44.0, the owner's ask and name): makes a sound from a recording of one, as the "preset cloners" for other
// synths do. Our own design: the recording is analysed and the synth is set from what's measured, nothing learned
// from examples.
//
//   - PITCH: the YIN method (de Cheveigné & Kawahara, 2002) every 10 ms, refined at the full sample rate.
//   - TONE: single cycles cut at the pitch found there, resampled to 2048 samples, lined up so their fundamentals start
//     together, a few cycles averaged per frame (the noise between harmonics averages away), every frame at the same
//     loudness: a wavetable of the sound's own waves, its frames spread from the start of the note to where it settles
//     (or dies away), for TRAVEL to walk through as the note plays.
//   - LOUDNESS: an attack, decay, sustain and release fitted to how the level moves (the loudness isn't in the frames).
//   - MOVEMENT: vibrato (a steady wobble in the pitch: its rate, depth and how late it starts), a swoop into the note
//     (the pitch starting off and settling), unison (copies a little out of tune make each harmonic's level beat; how
//     fast says how far apart), noise (what lies between the harmonics), and stereo width.
//
// Best on one note, held, fairly dry. Chords, drums and whole mixes have no single pitch to cut cycles from; reverb
// and echoes are measured as part of the sound. The result is close, not a copy: our oscillators and envelopes are
// our own, and the figures (how unison's beating maps to DETUNE, how much noise) are estimates.
namespace synth::mimic
{
    struct Analysis
    {
        bool valid = false;
        juce::String problem;            // why not, in plain words

        juce::String name;
        double sampleRate = 48000.0;
        double seconds = 0.0;            // the recording's length

        // Where the note is in the recording (seconds).
        double start = 0.0, settle = 0.0, releaseStart = 0.0, end = 0.0;

        // The pitch: the note to play it at, and how far off that note it was (cents).
        double hz = 0.0;
        int rootNote = 60;
        float cents = 0.0f;
        float clarity = 0.0f;            // 0 to 1: how clearly pitched

        // The loudness envelope, as the AMP's (curves -1: falling at a steady rate in dB, as natural sounds do).
        float attackMs = 1.0f, decayMs = 300.0f, sustain = 1.0f, releaseMs = 200.0f;
        bool sustained = true;           // held at a level (a pad, a lead) or dying away (a pluck, a bell)

        // Movement.
        float vibratoHz = 0.0f, vibratoCents = 0.0f, vibratoDelayMs = 0.0f;   // cents 0: none
        std::vector<float> swoop;        // semitones off the note at each 10 ms from the start (empty: none)
        float detuneCents = 0.0f;        // unison: how far the outer copies are out (0: one oscillator)
        float noise = 0.0f;              // the share of the sound that's noise, 0 to 1
        float noiseColour = 0.0f;        // 0 white to 1 dark (as the oscillator's NOISE COLOUR)
        float width = 0.0f;              // side against middle, 0 (mono) to about 1

        // A SLOW MOVEMENT (1.8.0): the held part's tone (and level) going round and round, as an LFO on a filter does,
        // every `movementSeconds` (0: none found). `movementStrength` how alike each round is to the last (0 to 1),
        // `movementOctaves` how far the brightness swings across a round (peak to peak), `movementDb` the level's.
        // With one, the table holds the attack's frames (`attackFrames` of them, from the start to `settle`) and then
        // one round of the movement (each of its frames the average of every round at that point), its last frame a
        // copy of its first so the loop is seamless.
        double movementSeconds = 0.0;
        float movementStrength = 0.0f, movementOctaves = 0.0f, movementDb = 0.0f;
        int attackFrames = 0;

        // The tone: the sound's own cycles as a wavetable, walked from first to last frame over `travelSeconds`.
        WavetableSource table;
        double travelSeconds = 0.0;

        // For the page's pictures: the level (dB, 0 at the loudest) and pitch (cents off the note, NaN where there's
        // none) every 10 ms through the recording.
        std::vector<float> levels, pitches;
        static constexpr double hopSeconds = 0.01;

        // The recording itself, mono, at `sampleRate` (part 2 compares the made sound with it).
        std::shared_ptr<const std::vector<float>> audio;
    };

    // Analyses a recording (any number of channels). Message thread or a worker: takes a fraction of a second for a
    // few seconds of sound. Anything past `maxSeconds` is left out.
    // (20 s until 1.8.0: a slow movement wants several rounds of it.)
    static constexpr double maxSeconds = 60.0;
    Analysis analyse (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name);

    // The same for a file (false in `valid`, with a problem, if it can't be read).
    Analysis analyseFile (const juce::File& file);

    // The note name for a MIDI note ("C3": middle C is C4).
    juce::String noteName (int note);

    //==================================================================================================================
    // PICK A NOTE (1.5.0, the owner's ask: thousands of synth stems from construction kits, mostly loops and phrases):
    // a whole stem is cut into its notes first, and MIMIC is made from the one chosen. Our own design:
    //   - the level and the pitch (YIN, as above) every 10 ms over the whole stem;
    //   - a note starts where the sound starts after a silence, where it attacks again (the level jumping 6 dB in 30 ms
    //     at the same pitch) or where the pitch moves to another note (over 0.6 of a semitone from the note's middle
    //     pitch for 40 ms: vibrato and slides inside a note don't split it); it ends where the next starts or where it
    //     falls 40 dB under its own loudest (its release kept);
    //   - each note scored for MIMIC: longer (up to 2 s) is better, clearly pitched (a chord or a noise is not), steady
    //     (not sliding or bending all through), loud enough, and ringing out before the next note (so its release can be
    //     measured). The best is chosen at first; any can be.
    // A stem of one note is just that note. Up to `maxPhraseSeconds` of a stem is read.
    struct FoundNote
    {
        double start = 0.0, end = 0.0;   // seconds into the stem (end: where the next note starts or it has died away)
        double voicedEnd = 0.0;          // where its pitch was last heard
        int note = 60;                   // its middle pitch, as a note...
        float cents = 0.0f;              // ...and how far off it
        float levelDb = -120.0f;         // its loudest, dB from the stem's loudest
        float clarity = 0.0f;            // 0 to 1 (under about 0.8: probably a chord, or noisy)
        float wobbleCents = 0.0f;        // how far its pitch strays from its middle (vibrato, slides), the median
        bool ringsOut = false;           // falls away before the next note starts
        float score = 0.0f;              // how well MIMIC should do with it, 0 to 1
    };

    struct Phrase
    {
        bool valid = false;
        juce::String problem;
        juce::String name;
        double sampleRate = 48000.0;
        double seconds = 0.0;
        std::vector<float> levels;       // dB from the loudest, every 10 ms
        std::vector<float> pitches;      // as a note number (fractional), every 10 ms; NaN where there's none
        std::vector<FoundNote> notes;    // in time order
        int best = -1;                   // the best for MIMIC (an index into notes)
        std::shared_ptr<const juce::AudioBuffer<float>> audio;   // the stem as read
    };

    static constexpr double maxPhraseSeconds = 180.0;
    Phrase findNotes (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name);
    Phrase findNotesInFile (const juce::File& file);
    // The note cut out of the stem (a few ms before its start to its end), every channel, for `analyse`.
    juce::AudioBuffer<float> cutNote (const Phrase&, const FoundNote&);
    // The note analysed, named "<stem> · <note> (n)".
    Analysis analyseNote (const Phrase&, int index);

    //==================================================================================================================
    // THE MATCH (part 2, 0.45.0): how close a sound is to the recording, by ear's measure as far as we can make one:
    //   - its TONE: the spectrum in 32 bands a third of an octave or so wide (60 Hz to 16 kHz) every 10 ms, each moment
    //     compared as a shape (its own loudness taken out), where the recording is within 30 dB of its loudest;
    //   - its LOUDNESS: the level every 10 ms, compared over the whole note and its tail.
    // The overall volume is taken out first (the same sound quieter is the same sound) and reported as `gainDb`, what
    // the candidate needs to match. Both errors are average dB; MATCH = 100 x e^(-error / 10), error the mean of the two
    // (1 dB off: 90%, 3 dB: 74%, 6 dB: 55%). Our own measure.
    struct Fingerprint
    {
        static constexpr int numBands = 32;
        std::vector<std::array<float, numBands>> bands;   // dB, every 10 ms
        std::vector<float> levels;                        // dB, every 10 ms
    };
    // From `start` seconds into `audio` (mono), `seconds` long (0: to the end).
    Fingerprint fingerprint (const std::vector<float>& audio, double sampleRate, double start = 0.0, double seconds = 0.0);

    struct Score
    {
        double match = 0.0;            // 0 to 100
        double toneDb = 0.0, loudnessDb = 0.0;
        double gainDb = 0.0;           // add this much to the candidate to be as loud as the recording
        std::vector<float> levels;     // the candidate's level every 10 ms, gain-matched, dB from the recording's loudest
    };
    Score compare (const Fingerprint& recording, const Fingerprint& candidate);
}
