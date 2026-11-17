#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>

// RAGTIME (0.92.0, HIGHNOON's signature; the owner's choice of two sketched (PIANOLA, a player-piano roll, over
// REVERIE) and the owner's name; our own design, no instrument or product modelled). The park's machine playing what
// you hold: in time with the song, a roll of 16 steps re-strikes every held note. Not an audio effect: each strike is
// a real new hammer blow in the engine (the note's envelopes start again from where they are, an SFZ oscillator starts
// a fresh recording), just before it a quick damper LIFT, so a sustained sound is struck too.
//  - PATTERN: the roll (`patternNames` STRIDE, RAG, CAKEWALK, MARCH, ROLL; never reorder). Each step says who's struck:
//    the bass (the lowest note held), the chord (the rest), or both, and its tier;
//  - STRIDE: on, the roll splits your chord as a stride pianist's left and right hands (the bass on the "oom", the
//    chord on the "pah"); off, every step strikes everything;
//  - AMOUNT: how much of the roll plays, by tier (the main beats first, then the syncopations, then the pickups);
//    the LIFT's depth rises with it too;
//  - DIVISION: a step's length (`divisionNames` 1/16, 1/8, 1/16 T, 1/32; never reorder);
//  - SWING: every second step late, by up to a third of a step;
//  - ACCENT: the unaccented strikes softer (velocity x 1 - 0.5 ACCENT);
//  - LIFT: how far the damper lifts the sound just before each strike (0.92 x LIFT at full AMOUNT).
// Only keys' notes (and the pedal's) are struck: not the arps' notes, SIROCCO's answers or release tails; a note the
// pedal holds stays held by the pedal. In time with `ppq` while the song plays; its own count while stopped. Off, or
// AMOUNT under 0.001, nothing is touched (bit for bit).
namespace synth::ragtime
{
    inline constexpr int numSteps = 16;
    inline const juce::StringArray patternNames { "STRIDE", "RAG", "CAKEWALK", "MARCH", "ROLL" };   // never reorder
    inline const juce::StringArray divisionNames { "1/16", "1/8", "1/16 T", "1/32" };                // never reorder
    inline constexpr std::array<double, 4> divisionBeats { 0.25, 0.5, 1.0 / 6.0, 0.125 };

    // Each step: '.' nothing; L (bass), U (chord), B (both): capitals the main beats (tier 1, accented), lower case
    // the syncopations (tier 2), x (chord) and y (bass) the pickups (tier 3).
    inline constexpr std::array<const char*, 5> patterns {
        "L...U.u.L...U.x.",   // STRIDE: oom-pah, the bass on 1 and 3, the chord on 2 and 4
        "B..uy.u.B..uy.u.",   // RAG: the chord in 3 + 3 + 2 over a steady bass
        "B.uU..u.B.uU..x.",   // CAKEWALK: short-long-short
        "L.uxL.u.L.uxL.u.",   // MARCH: the bass on every beat, the chord off it
        "BbbbBbbbBbbbBbbb",   // ROLL: every step, the beats accented
    };

    enum Who : int { none = 0, bass = 1, chord = 2, both = 3 };

    struct Hit
    {
        int who = none;
        int tier = 0;          // 1 the main beats, 2 the syncopations, 3 the pickups
        bool accent = false;
    };

    inline Hit hitAt (int pattern, int step) noexcept
    {
        const auto* p = patterns[(size_t) juce::jlimit (0, (int) patterns.size() - 1, pattern)];
        const auto c = p[((step % numSteps) + numSteps) % numSteps];
        switch (c)
        {
            case 'L': return { bass, 1, true };
            case 'U': return { chord, 1, true };
            case 'B': return { both, 1, true };
            case 'l': return { bass, 2, false };
            case 'u': return { chord, 2, false };
            case 'b': return { both, 2, false };
            case 'y': return { bass, 3, false };
            case 'x': return { chord, 3, false };
            default:  return {};
        }
    }

    // The tiers playing at AMOUNT `a`: the main beats from the start, the syncopations from 0.4, the pickups from 0.7.
    inline int tiersAt (float a) noexcept { return a < 0.001f ? 0 : a < 0.4f ? 1 : a < 0.7f ? 2 : 3; }

    struct Settings
    {
        bool on = false;
        float amount = 0.0f;   // 0 to 1 (the macro's)
        int pattern = 0;       // patternNames: STRIDE
        int division = 0;      // divisionNames: 1/16
        float swing = 0.3f;    // 0 to 1
        float accent = 0.5f;   // 0 to 1
        float lift = 0.6f;     // 0 to 1
        bool stride = true;

        bool playing() const noexcept { return on && amount >= 0.001f; }
        double beatsPerStep() const noexcept { return divisionBeats[(size_t) juce::jlimit (0, (int) divisionBeats.size() - 1, division)]; }
        // How far the damper lifts (the gain taken away) just before a strike.
        float liftDepth() const noexcept { return 0.92f * juce::jlimit (0.0f, 1.0f, lift) * juce::jmin (1.0f, 2.0f * amount); }
        // A strike's velocity, from the note's own.
        float strikeVelocity (float velocity, bool accented) const noexcept
        {
            return accented ? velocity : velocity * (1.0f - 0.5f * juce::jlimit (0.0f, 1.0f, accent));
        }
    };

    // Where step `k` falls, in steps (every second one late by SWING's share of a third of a step).
    inline double strikeTime (std::int64_t k, float swing) noexcept
    {
        return (double) k + ((k & 1) != 0 ? juce::jlimit (0.0f, 1.0f, swing) / 3.0 : 0.0);
    }

    // The first strike at or after `position` (in steps): its step number.
    inline std::int64_t nextStrike (double position, float swing) noexcept
    {
        auto k = (std::int64_t) std::floor (position) - 1;
        while (strikeTime (k, swing) < position - 1.0e-9)
            ++k;
        return k;
    }

    // What's struck at song step `k` at these settings (none where the tier isn't playing yet).
    inline Hit strikeAt (const Settings& s, std::int64_t k) noexcept
    {
        auto hit = hitAt (s.pattern, (int) (((k % numSteps) + numSteps) % numSteps));
        if (hit.who == none || hit.tier > tiersAt (s.amount))
            return {};
        if (! s.stride)
            hit.who = both;
        return hit;
    }
}
