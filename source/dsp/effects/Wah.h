#pragma once

#include "EffectUtils.h"

#include <array>

namespace effects
{
    // THE WAH (0.51.0, the owner's ask: the signature of the SLINGBLADE expansion, worked by the EXPR wheel). Our own
    // design, grown from PR7-G's expression-pedal wah and auto-wah (a band-pass swept 350 Hz to 2.2 kHz), but stereo
    // (a filter a side, swept together) and fuller: a real wah's output isn't only its resonant peak, it keeps some of
    // the low end under it, so at the heel it's dark and throaty rather than thin. Here that's the resonant band-pass
    // (unity at its peak, times BOOST) plus a share of the same filter's low-pass (BODY), from one zero-delay
    // state-variable filter.
    //
    // It sits first on an insert bus, before the DRIVE, where a wah goes on a pedalboard: the drive then works on the
    // swept peak (the "cry" into distortion).
    //
    // Modes: PEDAL (POSITION is the pedal, heel 0 to toe 1: the EXPR wheel moves it through the mod matrix), AUTO (the
    // sweep opens with how loud you play, from POSITION up by DEPTH), LFO (a sine sweep from POSITION up by DEPTH, free
    // or in time with the song).
    class Wah
    {
    public:
        // Saved by number: only append.
        enum class Mode { pedal, autoWah, lfo };
        static inline const juce::StringArray modeNames { "Pedal", "Auto", "LFO" };
        // The sweep's range and voicing. Saved by number: only append.
        enum class Range { classic, wide, low };
        static inline const juce::StringArray rangeNames { "Classic", "Wide", "Low" };

        struct Settings
        {
            bool on = false;
            Mode mode = Mode::pedal;
            Range range = Range::classic;
            float position = 0.5f;      // 0 (heel) to 1 (toe)
            float resonance = 5.0f;     // 0 to 10: Q 2 to 12
            float sensitivity = 5.0f;   // 0 to 10: how much level opens AUTO fully
            float speed = 5.0f;         // 0 to 10: how fast AUTO falls back (0.6 s to 40 ms)
            float depth = 1.0f;         // 0 to 1: how far AUTO and LFO sweep up from POSITION
            float rateHz = 1.0f;        // LFO, when not synced
            int sync = 0;               // Delay::syncNames: 0 free (RATE), else a note length
            float mix = 1.0f;           // 0 to 1
            float levelDb = 0.0f;
        };

        // The sweep's ends and voicing.
        struct Voicing { float heelHz, toeHz, body; };
        static Voicing voicingFor (Range) noexcept;
        // The filter's centre for a sweep position (0 to 1), spaced evenly by pitch.
        static float frequencyFor (Range, float position) noexcept;
        static float qFor (float resonance) noexcept { return 2.0f + juce::jlimit (0.0f, 10.0f, resonance); }
        // The wet sound's level (dB) at `hz` with the sweep at `sweep`: what the filter does, from its analog
        // prototype (the page draws it; the tests check it against the sound).
        static float responseDb (Range, float sweep, float resonance, float hz) noexcept;

        // The resonant peak's level (unity band-pass times this), and the wet sound's make-up (see Wah.cpp).
        static constexpr float peakBoost = 2.2f, makeUp = 2.0f;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        // `bpm` and `ppq` (the song position in beats, negative while the song is stopped) for LFO's sync.
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;

        // Where the sweep is (0 to 1), for the page.
        float getSweep() const noexcept { return shownSweep.load (std::memory_order_relaxed); }

    private:
        double sampleRate = 48000.0;
        BypassFade bypass;
        struct Side { float s1 = 0.0f, s2 = 0.0f; };
        std::array<Side, 2> sides {};
        float sweep = -1.0f;        // the filter's place, smoothed (-1: not yet set)
        float envelope = 0.0f;      // AUTO's level follower
        double lfoPhase = 0.0;      // free LFO, 0 to 1
        std::atomic<float> shownSweep { 0.5f };
    };
}
