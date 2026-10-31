#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Wavetable.h"

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
    };

    // Analyses a recording (any number of channels). Message thread or a worker: takes a fraction of a second for a
    // few seconds of sound. Anything past `maxSeconds` is left out.
    static constexpr double maxSeconds = 20.0;
    Analysis analyse (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name);

    // The same for a file (false in `valid`, with a problem, if it can't be read).
    Analysis analyseFile (const juce::File& file);

    // The note name for a MIDI note ("C3": middle C is C4).
    juce::String noteName (int note);
}
