#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>

// RUNNER (1.0.0, WICKED GAME's signature; the owner's choice of two offered (RUNNER over CLOSING) and the owner's
// design: the speed steps up with every fifth of the knob; our own design otherwise). The chase: every held note
// struck again in time with the song, faster as AMOUNT turns. Not an audio effect: each strike is a real new hit in
// the engine, as RAGTIME's (the note's envelopes start again from where they are, an SFZ oscillator starts a fresh
// recording), a quick damper LIFT just before it, so a held sound is struck too.
//  - AMOUNT: off at 0; then the speed by fifths of the knob (`speedNames`): 1/4 above 0 to 20%, 1/8 to 40%, 1/16 to
//    60%, 1/32 to 80%, 1/64 above. Every speed is a power of two of a beat, so the strikes of each sit on the next
//    one's grid and a change of speed never stumbles;
//  - ACCENT: the strikes off the beat softer (velocity x 1 - 0.6 ACCENT), so the beat stays felt at any speed;
//  - LIFT: how far the damper lifts the sound just before each strike (0.92 x LIFT), from about 4 ms before it (never
//    more than a third of a strike's length).
// Only keys' notes (and the pedal's) are struck: not the arps' notes, SIROCCO's answers or release tails; a note under
// 10 ms old isn't struck (its own attack is the strike). In time with `ppq` while the song plays; its own count while
// stopped (from the first key). Off, or AMOUNT under 0.001, nothing is touched (bit for bit).
namespace synth::runner
{
    inline const juce::StringArray speedNames { "OFF", "1/4", "1/8", "1/16", "1/32", "1/64" };   // what AMOUNT gives

    // The speed for AMOUNT `a`: 0 off, then 1 to 5 (1/4 to 1/64), a step every fifth of the knob.
    inline int speedAt (float a) noexcept
    {
        if (a < 0.001f)
            return 0;
        return juce::jlimit (1, 5, 1 + (int) std::floor (juce::jlimit (0.0f, 0.99999f, a) * 5.0f));
    }

    // A strike's length in beats at a speed (1: a beat, 5: a sixteenth of one).
    inline double beatsPerStrike (int speed) noexcept { return std::exp2 (-(double) (juce::jlimit (1, 5, speed) - 1)); }

    struct Settings
    {
        bool on = false;
        float amount = 0.0f;   // 0 to 1 (the macro's)
        float accent = 0.4f;   // 0 to 1
        float lift = 0.5f;     // 0 to 1

        bool playing() const noexcept { return on && amount >= 0.001f; }
        int speed() const noexcept { return speedAt (amount); }
        float liftDepth() const noexcept { return 0.92f * juce::jlimit (0.0f, 1.0f, lift); }
        float strikeVelocity (float velocity, bool onBeat) const noexcept
        {
            return onBeat ? velocity : velocity * (1.0f - 0.6f * juce::jlimit (0.0f, 1.0f, accent));
        }
    };

    // The strike grid: strike k at k x beatsPerStrike beats; the first at or after `beats`.
    inline std::int64_t nextStrike (double beats, double perStrike) noexcept
    {
        return (std::int64_t) std::ceil (beats / perStrike - 1.0e-9);
    }
}
