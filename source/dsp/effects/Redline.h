#pragma once

#include "PitchShifter.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

namespace effects
{
    // REDLINE (0.88.0, CHROMEFURY's signature, the owner's choice of two offered (the sound as a revving engine, over
    // fire bursts on a war-drum rhythm), chosen after hearing both sketched on one lead; our own design, no engine or
    // product modelled). The sound becomes the engine, in time with the song. One knob, THROTTLE, brings it in:
    //  - THROB: the cylinders. The sound's level pulses at half the pitch of the lowest note sounding (`setNotes`, from
    //    the engine: not tracked from the audio), a rounded square, so every note growls with a throb an octave down;
    //    deeper as the revs climb (60% of THROB idling, all of it at the top; at most 95%).
    //  - THE REVS climb across each GEAR (`gearNames` 1/2 BAR, 1 BAR, 2 BARS, 4 BARS: never reorder) and drop back on
    //    its line: the gear change. SHIFT: how far the level dips there (over about 60 ms).
    //  - SCREAM: the sound an octave up (PR7-G's `PitchShifter`, a side each), climbing RISE (0 to +12 semitones) over
    //    the gear, through a hard drive that grows with the revs, a 150 Hz cut and TONE's top (2-12 kHz): the engine
    //    screaming as it revs, added under the sound at SCREAM x the revs x THROTTLE.
    //  - WHINE: a supercharger's sine two octaves over the lowest note, rising a fifth over the gear, as loud as the
    //    music lets it (a follower a side: silence stays silent, and a silent side too).
    // The level is kept (power in and out followed over 0.3 s, gain 0.5-1.6, glided). In time with `ppq` while the song
    // plays (4/4); its own count while stopped, from the bottom of a gear when it wakes. THROTTLE smoothed 30 ms; under
    // 0.001 it counts as 0, and at 0 (the scream and whine, both x THROTTLE, silent), once the gain is back to 1 for 0.3 s, the sound
    // passes untouched bit for bit (`isResting`).
    class Redline
    {
    public:
        static inline const juce::StringArray gearNames { "1/2 BAR", "1 BAR", "2 BARS", "4 BARS" };   // never reorder
        static constexpr std::array<double, 4> gearBeats { 2.0, 4.0, 8.0, 16.0 };
        static constexpr int maxNotes = 16;

        struct Settings
        {
            bool on = false;
            float throttle = 0.0f;   // 0 to 1 (the macro's)
            int gear = 1;            // gearNames: 1 BAR
            float throb = 0.6f;      // 0 to 1
            float scream = 0.6f;     // 0 to 1
            float rise = 0.2f;       // 0 to 1: 0 to +12 semitones over a gear
            float whine = 0.4f;      // 0 to 1
            float tone = 0.5f;       // 0 to 1: the scream's top, 2 to 12 kHz
            float shift = 0.5f;      // 0 to 1: the gear change's dip
        };

        static float riseSemitones (float r) noexcept { return 12.0f * juce::jlimit (0.0f, 1.0f, r); }
        static float toneHz (float t) noexcept { return 2000.0f * std::pow (6.0f, juce::jlimit (0.0f, 1.0f, t)); }
        // The revs (0 at the bottom of a gear to 1 at its line) `beats` into the song.
        static double revsAt (double beats, int gear) noexcept
        {
            const auto length = gearBeats[(size_t) juce::jlimit (0, (int) gearBeats.size() - 1, gear)];
            return beats / length - std::floor (beats / length);
        }
        // Which gear change this is (0, 1, 2 ...), for the page.
        static int gearCount (double beats, int gear) noexcept
        {
            return (int) std::floor (beats / gearBeats[(size_t) juce::jlimit (0, (int) gearBeats.size() - 1, gear)]);
        }
        // The throb's depth at revs `r` (before THROTTLE).
        static float throbDepth (float throb, float r) noexcept { return juce::jmin (0.95f, juce::jlimit (0.0f, 1.0f, throb) * (0.6f + 0.4f * r)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void setNotes (const float* notes, int count) noexcept;
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page, the unit and the tests.
        float getThrottle() const noexcept  { return throttleShown.load (std::memory_order_relaxed); }
        float getRevs() const noexcept      { return revsShown.load (std::memory_order_relaxed); }      // 0-1 in the gear
        int getGear() const noexcept        { return gearShown.load (std::memory_order_relaxed); }      // 1 to 6, cycling
        float getNoteHz() const noexcept    { return noteShown.load (std::memory_order_relaxed); }      // the note the throb follows
        float getScreamDb() const noexcept  { return screamShown.load (std::memory_order_relaxed); }
        float getGainDb() const noexcept    { return gainShown.load (std::memory_order_relaxed); }

    private:
        struct Side
        {
            PitchShifter shifter;
            float high = 0.0f;     // the scream's low cut (one-pole's state)
            float low = 0.0f;      // its top
            float follower = 0.0f; // this side's level (the whine follows it: a silent side stays silent)
        };

        double sampleRate = 48000.0;
        bool resting = true;
        float throttleNow = 0.0f, throttleCoef = 0.0f;
        double ownBeats = 0.0;
        double throbPhase = 0.0, whinePhase = 0.0;
        float lowestNote = -1.0f;          // MIDI; -1: none yet
        float logHz = 0.0f, logHzCoef = 0.0f;
        float revsSmooth = 0.0f, revsCoef = 0.0f;
        double lastRevs = 0.0;
        float dip = 0.0f, dipFall = 0.0f;
        float followUp = 0.0f, followDown = 0.0f;
        float powerIn = 0.0f, powerOut = 0.0f, gain = 1.0f, levelCoef = 0.0f, gainCoef = 0.0f;
        int settledSamples = 0;
        float highCoef = 0.0f;
        std::array<Side, 2> sides {};

        std::atomic<float> throttleShown { 0.0f }, revsShown { 0.0f }, noteShown { 0.0f }, screamShown { -120.0f }, gainShown { 0.0f };
        std::atomic<int> gearShown { 1 };
    };
}
