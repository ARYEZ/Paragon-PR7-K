#pragma once

#include "Sirocco.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>

// PRESS (1.3.0, the feudal-Japan bank's signature, agreed with the owner; our own design, no instrument or product
// modelled). A koto player's left hand: after a string is plucked, the hand presses it behind the bridge and the note
// rises to the next note of the scale. Not an audio effect: every note sounding (the keys' and the arps'; not SIROCCO's
// answers or release tails) is moved in the engine, so the oscillators, the SFZ recordings and the filter all follow.
//  - AMOUNT (the macro's): how many notes are pressed. Each note's chance is judged once, when it's struck, from a fixed
//    hash of the note and the grid line its press falls on (`pressedAt`), so the same passage at the same place in the
//    song always presses the same notes. 0 (under 0.001): nothing at all, bit for bit.
//  - WHEN (`divisionNames`): the press starts on the next line of this grid after the strike (at least `minimumGap`
//    after it), in time with the song (`ppq` while it plays; its own count while stopped, from the first key).
//  - STYLE (`styleNames`, after the koto's own techniques, our reading of them):
//      PUSH     a quick press up and back (tsuki-iro),
//      PRESS    pressed up and held (ato-oshi),
//      RELEASE  struck already pressed, let back down to the note on the grid line (oshi-hanashi),
//      WOBBLE   pressed and let go over and over, a slow swing above the note, never below it (yuri-iro),
//      MIXED    each note one of those four, from the same hash.
//  - STEPS (`stepNames`): how far a press goes: the next note of the scale up, or the one after.
//  - KEY and SCALE: the notes it lands on (the arp randomizer's scales: hirajoshi, in and yo among them). A note
//    outside the scale lands on the scale note STEPS above the scale note under it, so a press always ends in key.
//  - SPEED: how long the press takes (`riseSeconds`: 40 to 400 ms).
// A koto string is only ever pressed sharp, so nothing here bends a note below where it was played. A note keeps the
// press it was given; turning AMOUNT to 0 (or PRESS off) lets every press back down over about 50 ms.
namespace synth::press
{
    inline const juce::StringArray styleNames { "PUSH", "PRESS", "RELEASE", "WOBBLE", "MIXED" };   // never reorder
    enum Style { push = 0, hold = 1, release = 2, wobble = 3, mixed = 4 };
    inline const juce::StringArray divisionNames { "1/16", "1/8", "1/4", "1/2" };                 // never reorder
    inline constexpr std::array<double, 4> divisionBeats { 0.25, 0.5, 1.0, 2.0 };
    inline const juce::StringArray stepNames { "1 STEP", "2 STEPS" };                             // never reorder
    inline constexpr double minimumGap = 0.03;          // seconds between the strike and its press, at least
    inline constexpr float wobbleHz = 4.5f;             // WOBBLE's swing
    inline constexpr float wobbleShare = 0.5f;          // ...up to this share of the press
    inline constexpr double letGoSeconds = 0.05;        // AMOUNT to 0: every press back down over about this

    struct Settings
    {
        bool on = false;
        float amount = 0.0f;   // 0 to 1 (the macro's)
        int style = mixed;     // styleNames
        int division = 1;      // divisionNames: 1/8
        int steps = 0;         // stepNames: 1 STEP
        int key = 2;           // sirocco::keyNames: D
        int scale = 18;        // arp::scaleNames: In
        float speed = 0.4f;    // 0 to 1

        bool pressing() const noexcept { return on && amount >= 0.001f; }
    };

    inline double beatsPerLine (int division) noexcept { return divisionBeats[(size_t) juce::jlimit (0, (int) divisionBeats.size() - 1, division)]; }
    inline double riseSeconds (float speed) noexcept { return 0.04 + 0.36 * (double) juce::square (juce::jlimit (0.0f, 1.0f, speed)); }

    // A fixed hash of the grid line and the note, 0 to 1.
    inline float hash01 (std::int64_t line, int note, std::uint32_t salt) noexcept
    {
        auto x = (std::uint64_t) line * 0x9e3779b97f4a7c15ull ^ ((std::uint64_t) (std::uint32_t) note << 32) ^ salt;
        x ^= x >> 31; x *= 0xbf58476d1ce4e5b9ull; x ^= x >> 27; x *= 0x94d049bb133111ebull; x ^= x >> 31;
        return (float) ((x >> 40) & 0xffffff) / 16777216.0f;
    }
    // Is the note pressed, and how (a style, never MIXED); -1 not pressed.
    inline int pressedAt (const Settings& s, std::int64_t line, int note) noexcept
    {
        if (! s.pressing() || hash01 (line, note, 0x5f3759dfu) >= s.amount)
            return -1;
        if (s.style != mixed)
            return juce::jlimit (0, (int) wobble, s.style);
        // MIXED: PRESS and PUSH the most, as a player would.
        const auto r = hash01 (line, note, 0x2545f491u);
        return r < 0.35f ? (int) hold : r < 0.65f ? (int) push : r < 0.85f ? (int) release : (int) wobble;
    }

    // `note` pressed `steps` (1 or 2) up the scale, always landing on a note of it.
    inline int pressedNote (int note, int steps, int key, int scale) noexcept
    {
        std::array<int, 12> notes {};
        const auto count = sirocco::scaleNotes (scale, notes);
        if (count == 0 || steps <= 0)
            return note;
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
        return juce::jlimit (0, 127, note - fromKey + targetOctave * 12 + notes[(size_t) targetIndex]);
    }
    inline float semitonesFor (int note, const Settings& s) noexcept
    {
        return (float) (pressedNote (note, juce::jlimit (0, 1, s.steps) + 1, s.key, s.scale) - note);
    }

    // A smooth step 0 to 1 (raised cosine) over [0, 1].
    inline float ease (double x) noexcept
    {
        if (x <= 0.0)
            return 0.0f;
        if (x >= 1.0)
            return 1.0f;
        return (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * x));
    }

    // How far pressed (0 to 1 of the press) `t` seconds after the grid line (negative: before it), for a style.
    inline float shapeAt (int style, double t, double rise) noexcept
    {
        switch (style)
        {
            case push:    return t < 2.5 * rise ? ease (t / rise) : 1.0f - ease ((t - 2.5 * rise) / rise);
            case hold:    return ease (t / rise);
            case release: return 1.0f - ease (t / rise);
            case wobble:
            {
                if (t <= 0.0)
                    return 0.0f;
                const auto swing = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * wobbleHz * t);
                return (float) (wobbleShare * swing) * ease (t / (2.0 * rise));
            }
            default:      return 0.0f;
        }
    }
}
