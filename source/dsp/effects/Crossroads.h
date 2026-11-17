#pragma once

#include "PitchShifter.h"
#include "../synth/Crossroads.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

namespace effects
{
    // CROSSROADS (0.90.0, the owner's choice and name; our own design): the effect half. The sound pulled between two
    // sides in time with the song, a stretch of each (LENGTH: 1/2 BAR to 4 BARS a side; the same clock as the engine
    // half, `synth::crossroads`, which pulls the notes down the scale on the dark side, in key). One knob, AMOUNT:
    //  - LIGHT: a shimmer an octave up (PR7-G's `PitchShifter`, a side each, its lows cut) and a little air on top;
    //  - DARK: a growl an octave down (another shifter, darkened and driven), the sound itself darkened and roughened;
    //  - between them a 40 ms fade at each change of side, so nothing clicks.
    // The level is kept (power in and out followed over 0.3 s, gain 0.5-1.6, glided). In time with `ppq` while the song
    // plays (4/4); its own count while stopped. AMOUNT smoothed 30 ms; under 0.001 it counts as 0, and at 0, once the
    // gain is back to 1 for 0.3 s, the sound passes untouched bit for bit (`isResting`).
    class Crossroads
    {
    public:
        struct Settings
        {
            bool on = false;
            float amount = 0.0f;   // 0 to 1 (the macro's)
            int length = 1;        // synth::crossroads::lengthNames: 1 BAR a side
            float light = 0.6f;    // 0 to 1: how bright the light side
            float dark = 0.6f;     // 0 to 1: how dark the dark side
        };

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page, the unit and the tests.
        float getAmount() const noexcept    { return amountShown.load (std::memory_order_relaxed); }
        float getDarkness() const noexcept  { return darknessShown.load (std::memory_order_relaxed); }   // 0 light, 1 dark
        float getPosition() const noexcept  { return positionShown.load (std::memory_order_relaxed); }   // 0-1 through a light + dark pair
        float getGainDb() const noexcept    { return gainShown.load (std::memory_order_relaxed); }

    private:
        struct Side
        {
            PitchShifter up, down;
            float upLow = 0.0f;      // the shimmer's low cut (one-pole state)
            float downLow = 0.0f;    // the growl's top
            float dryLow = 0.0f;     // the sound darkened
            float airLow = 0.0f;     // for the air (what's over 4 kHz)
        };

        double sampleRate = 48000.0;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f;
        float darkness = 0.0f, darknessCoef = 0.0f;
        double ownBeats = 0.0;
        float powerIn = 0.0f, powerOut = 0.0f, gain = 1.0f, levelCoef = 0.0f, gainCoef = 0.0f;
        int settledSamples = 0;
        float shimmerCut = 0.0f, growlTop = 0.0f, airCut = 0.0f;
        std::array<Side, 2> sides {};

        std::atomic<float> amountShown { 0.0f }, darknessShown { 0.0f }, positionShown { 0.0f }, gainShown { 0.0f };
    };
}
