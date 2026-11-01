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
    static constexpr double maxSeconds = 20.0;
    Analysis analyse (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name);

    // The same for a file (false in `valid`, with a problem, if it can't be read).
    Analysis analyseFile (const juce::File& file);

    // The note name for a MIDI note ("C3": middle C is C4).
    juce::String noteName (int note);

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
