#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>

// POLYMETER (1.4.0, SANDSEA's signature, the owner's choice of three offered after the research into how the desert
// film's score was made: its "anti-groove" of inhuman, synthesized drum patterns, and vocal-percussion grooves "tracked
// up in a bunch of meters"; our own design, nothing copied). The held notes struck again in several meters at once.
// Not an audio effect: each strike is a real new hit in the engine, as RAGTIME's and RUNNER's (the note's envelopes
// start again from where they are, an SFZ oscillator starts a fresh recording), a quick damper LIFT just before it.
//  - The METERS: 4, 3, 5 and 7 strikes a CYCLE (`meters`), all lining up at the cycle's start. AMOUNT brings them in
//    one at a time (`metersAt`): 4 alone above 0, then 4 against 3 from a quarter, + 5 from half, + 7 from three
//    quarters.
//  - Which note plays which: the held notes from the lowest up take the meters in that order (the lowest on 4, the
//    pulse), round again if there are more notes than meters; with fewer notes than meters the highest note plays all
//    the meters left over together, so a lone note plays every meter at once (4 against 3 is its strikes at 0, 1/4,
//    1/3, 1/2, 2/3, 3/4 of the cycle: a rhythm no foot can follow).
//  - CYCLE (`cycleNames`): the span the meters share, 1 beat to 2 bars (4/4).
//  - DENSITY (`densityNames`): every meter struck 1, 2 or 4 times as often (each meter's own beats then subdivided).
//  - ACCENT: the subdivisions softer than each meter's own beats (velocity x 1 - 0.6 ACCENT); the cycle's start full.
//  - LIFT: how far the damper lifts the sound just before each strike (0.92 x LIFT), from about 4 ms before it.
// Only keys' notes (and the pedal's) are struck, not the arps', SIROCCO's answers or release tails; a note under
// 10 ms old isn't struck (its own attack is the strike), nor one struck under `minimumGap` ago (strikes of two meters
// that nearly meet make one). In time with `ppq` while the song plays; its own count while stopped (from the first
// key). Off, or AMOUNT under 0.001, nothing is touched (bit for bit).
namespace synth::polymeter
{
    inline constexpr int numMeters = 4;
    inline constexpr std::array<int, numMeters> meters { 4, 3, 5, 7 };                            // in the order they come in
    inline const juce::StringArray cycleNames { "1 BEAT", "2 BEATS", "1 BAR", "2 BARS" };          // never reorder
    inline constexpr std::array<double, 4> cycleBeats { 1.0, 2.0, 4.0, 8.0 };
    inline const juce::StringArray densityNames { "x1", "x2", "x4" };                              // never reorder
    inline constexpr std::array<int, 3> densities { 1, 2, 4 };
    inline constexpr double minimumGap = 0.025;   // seconds: a note isn't struck again sooner than this

    // How many meters AMOUNT `a` brings in: 0 off, then 1 to 4, one more every quarter of the knob.
    inline int metersAt (float a) noexcept
    {
        if (a < 0.001f)
            return 0;
        return juce::jlimit (1, numMeters, 1 + (int) std::floor (juce::jlimit (0.0f, 0.99999f, a) * 4.0f));
    }

    struct Settings
    {
        bool on = false;
        float amount = 0.0f;   // 0 to 1 (the macro's)
        int cycle = 2;         // cycleNames: 1 BAR
        int density = 0;       // densityNames: x1
        float accent = 0.5f;   // 0 to 1
        float lift = 0.5f;     // 0 to 1

        bool playing() const noexcept { return on && amount >= 0.001f; }
        int activeMeters() const noexcept { return metersAt (amount); }
        double beatsPerCycle() const noexcept { return cycleBeats[(size_t) juce::jlimit (0, (int) cycleBeats.size() - 1, cycle)]; }
        int timesPerBeat() const noexcept { return densities[(size_t) juce::jlimit (0, (int) densities.size() - 1, density)]; }
        // Meter j's strikes a cycle.
        int strikesPerCycle (int j) const noexcept { return meters[(size_t) juce::jlimit (0, numMeters - 1, j)] * timesPerBeat(); }
        float liftDepth() const noexcept { return 0.92f * juce::jlimit (0.0f, 1.0f, lift); }
        float strikeVelocity (float velocity, bool main) const noexcept
        {
            return main ? velocity : velocity * (1.0f - 0.6f * juce::jlimit (0.0f, 1.0f, accent));
        }
    };

    // Which meters (bits) the note at place `rank` (0 the lowest) of `held` held notes plays, with `active` meters in.
    inline std::uint8_t metersFor (int rank, int held, int active) noexcept
    {
        if (active <= 0 || held <= 0 || rank < 0 || rank >= held)
            return 0;
        if (held >= active)
            return (std::uint8_t) (1 << (rank % active));
        if (rank < held - 1)
            return (std::uint8_t) (1 << rank);
        std::uint8_t mask = 0;
        for (int j = rank; j < active; ++j)
            mask = (std::uint8_t) (mask | (1 << j));
        return mask;
    }

    // Strike k of a meter struck `perCycle` times a cycle: where it falls, in beats, and the first at or after `beats`.
    inline double strikeBeat (std::int64_t k, int perCycle, double cycle) noexcept { return (double) k * cycle / (double) perCycle; }
    inline std::int64_t nextStrike (double beats, int perCycle, double cycle) noexcept
    {
        return (std::int64_t) std::ceil (beats * (double) perCycle / cycle - 1.0e-9);
    }
}
