#pragma once

#include "Sirocco.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>

// ORNAMENT (1.6.0, NIGHT REIGN's signature, the owner's choice: "keep ORNAMENT as the macro"; our own design, nothing
// copied). A baroque player's ornaments on what you play: held notes get trills, mordents and turns, always in key and
// in time with the song. Not an audio effect: every note sounding (the keys' and the arps'; not SIROCCO's answers or
// release tails) is moved in the engine, as PRESS moves them, so the oscillators, the SFZ recordings and the filter all
// follow.
//  - AMOUNT (the macro's): how many notes are ornamented. Each note is judged once, when it's struck, from a fixed hash
//    of the note and the grid line its ornament starts on (`ornamentedAt`), so a passage played at the same place in
//    the song always gets the same ornaments. 0 (under 0.001): nothing at all, bit for bit.
//  - STYLE (`styleNames`):
//      TRILL    the note and the next note up the scale, back and forth (starting on the note),
//      MORDENT  a quick flick to the note above and back (an upper mordent), then the note held,
//      TURN     above, the note, below, the note (a turn), then the note held,
//      MIXED    each note one of the three, from the same hash (trills the most).
//  - SPEED (`speedNames`): how long each step of the ornament is: a 16th, a 16th triplet, a 32nd or a 64th.
//  - LENGTH (`lengthNames`): how long a trill goes on: half a beat, a beat, two beats, or as long as the note is held.
//    (A mordent and a turn are always their own few steps.)
//  - KEY and SCALE: the notes it moves to (the arp randomizer's scales), one step of the scale up or down; a note
//    outside the scale moves from the scale note under it, so every ornament lands in key.
// The ornament starts on the first step line at least `minimumGap` after the strike (so the note is heard first), in
// time with `ppq` while the song plays, its own count while stopped (from the first key). Each step glides in over
// about 2 ms (a pitch that jumps in one sample is harsh on a sustained sound). Turning AMOUNT to 0 (or ORNAMENT off)
// lets every note back to its own pitch at once (over the same glide).
namespace synth::ornament
{
    inline const juce::StringArray styleNames { "TRILL", "MORDENT", "TURN", "MIXED" };   // never reorder
    enum Style { trill = 0, mordent = 1, turn = 2, mixed = 3 };
    inline const juce::StringArray speedNames { "1/16", "1/16 T", "1/32", "1/64" };     // never reorder
    inline constexpr std::array<double, 4> speedBeats { 0.25, 1.0 / 6.0, 0.125, 0.0625 };
    inline const juce::StringArray lengthNames { "1/2 BEAT", "1 BEAT", "2 BEATS", "HELD" };   // never reorder
    inline constexpr std::array<double, 4> lengthBeats { 0.5, 1.0, 2.0, 1.0e9 };
    inline constexpr double minimumGap = 0.04;   // seconds between the strike and the ornament, at least
    inline constexpr double glideSeconds = 0.002;

    struct Settings
    {
        bool on = false;
        float amount = 0.0f;   // 0 to 1 (the macro's)
        int style = mixed;     // styleNames
        int speed = 2;         // speedNames: 1/32
        int length = 1;        // lengthNames: 1 BEAT
        int key = 2;           // sirocco::keyNames: D
        int scale = 3;         // arp::scaleNames: Harmonic Minor

        bool ornamenting() const noexcept { return on && amount >= 0.001f; }
        double beatsPerStep() const noexcept { return speedBeats[(size_t) juce::jlimit (0, (int) speedBeats.size() - 1, speed)]; }
        double lengthInBeats() const noexcept { return lengthBeats[(size_t) juce::jlimit (0, (int) lengthBeats.size() - 1, length)]; }
    };

    // A fixed hash of the line and the note, 0 to 1.
    inline float hash01 (std::int64_t line, int note, std::uint32_t salt) noexcept
    {
        auto x = (std::uint64_t) line * 0x9e3779b97f4a7c15ull ^ ((std::uint64_t) (std::uint32_t) note << 32) ^ salt;
        x ^= x >> 31; x *= 0xbf58476d1ce4e5b9ull; x ^= x >> 27; x *= 0x94d049bb133111ebull; x ^= x >> 31;
        return (float) ((x >> 40) & 0xffffff) / 16777216.0f;
    }
    // Is the note ornamented, and how (a style, never MIXED); -1 not.
    inline int ornamentedAt (const Settings& s, std::int64_t line, int note) noexcept
    {
        if (! s.ornamenting() || hash01 (line, note, 0x1b873593u) >= s.amount)
            return -1;
        if (s.style != mixed)
            return juce::jlimit (0, (int) turn, s.style);
        const auto r = hash01 (line, note, 0xcc9e2d51u);
        return r < 0.5f ? (int) trill : r < 0.75f ? (int) mordent : (int) turn;
    }

    // Semitones from `note` to the note `steps` scale steps away (negative: down); 0 steps: the note itself. A note
    // outside the scale counts from the scale note under it.
    inline int semitonesFor (int note, int steps, int key, int scale) noexcept
    {
        if (steps == 0)
            return 0;
        std::array<int, 12> notes {};
        const auto count = sirocco::scaleNotes (scale, notes);
        if (count == 0)
            return 0;
        const auto fromKey = note - ((key % 12) + 12) % 12;
        const auto octave = fromKey >= 0 ? fromKey / 12 : (fromKey - 11) / 12;
        const auto semitone = fromKey - octave * 12;
        auto index = 0;
        for (int k = 0; k < count; ++k)
            if (notes[(size_t) k] <= semitone)
                index = k;
        const auto target = octave * count + index + steps;
        const auto targetOctave = target >= 0 ? target / count : (target - count + 1) / count;
        const auto targetIndex = target - targetOctave * count;
        return juce::jlimit (0, 127, note - fromKey + targetOctave * 12 + notes[(size_t) targetIndex]) - note;
    }

    // The scale steps away from the note at step `k` (0 the first) of an ornament of `style`, a trill lasting `steps`
    // steps; 0 once it's over.
    inline int stepAt (int style, std::int64_t k, std::int64_t steps) noexcept
    {
        if (k < 0)
            return 0;
        switch (style)
        {
            case trill:   return k < steps ? (int) (k & 1) : 0;           // 0, +1, 0, +1 ...
            case mordent: return k == 0 ? 1 : 0;                          // +1, then the note
            case turn:    { constexpr int t[] { 1, 0, -1 }; return k < 3 ? t[k] : 0; }   // +1, 0, -1, then the note
            default:      return 0;
        }
    }
    // How many steps a trill lasts.
    inline std::int64_t trillSteps (const Settings& s) noexcept
    {
        const auto n = s.lengthInBeats() / s.beatsPerStep();
        return n > 1.0e8 ? std::numeric_limits<std::int64_t>::max() / 4 : juce::jmax<std::int64_t> (2, (std::int64_t) std::llround (n));
    }
}
