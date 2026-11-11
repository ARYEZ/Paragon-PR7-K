#pragma once

#include "Sirocco.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>

// CROSSROADS (0.90.0, the owner's choice and name; the new bank's signature, chosen after hearing it sketched against
// a "contagion" on one lead; our own design). The sound pulled between two sides in time with the song: a stretch of
// LIGHT, then a stretch of DARK, and so on. This half is in the engine: on the dark side every note sounding is
// pulled DOWN the scale, landing on a note of KEY and SCALE (the owner's word: "keep it in key"; a note outside the
// scale lands on the scale note under it, then moves the steps from there), with a slide into it (BEND). How far:
// nothing below AMOUNT's halfway (there only the colours change: the effect half, `effects::Crossroads`), then one
// step, up to PULL's steps (STEP 1, THIRD 2, FIFTH 4) at the top. Back on the light side the notes slide home.
namespace synth::crossroads
{
    inline const juce::StringArray lengthNames { "1/2 BAR", "1 BAR", "2 BARS", "4 BARS" };   // never reorder
    inline constexpr std::array<double, 4> lengthBeats { 2.0, 4.0, 8.0, 16.0 };
    inline const juce::StringArray pullNames { "STEP", "THIRD", "FIFTH" };                   // never reorder
    inline constexpr std::array<int, 3> pullSteps { 1, 2, 4 };

    struct Settings
    {
        bool on = false;
        float amount = 0.0f;   // 0 to 1 (the macro's)
        int length = 1;        // lengthNames: 1 BAR a side
        int key = 9;           // sirocco::keyNames: A
        int scale = 2;         // arp::scaleNames: Minor
        int pull = 1;          // pullNames: THIRD
        float bend = 0.35f;    // 0 to 1: the slide, 15 to 400 ms
        bool pulling() const noexcept;
    };

    // Scale steps down at `amount`: none below halfway, then 1 up to PULL's.
    inline int stepsFor (float amount, int pull) noexcept
    {
        const auto full = pullSteps[(size_t) juce::jlimit (0, (int) pullSteps.size() - 1, pull)];
        if (amount < 0.5f)
            return 0;
        const auto t = (juce::jlimit (0.5f, 1.0f, amount) - 0.5f) / 0.5f;
        return juce::jlimit (1, full, (int) std::ceil (t * (float) full - 1.0e-4f));
    }
    inline bool Settings::pulling() const noexcept { return on && stepsFor (amount, pull) > 0; }

    inline double beatsPerSide (int length) noexcept { return lengthBeats[(size_t) juce::jlimit (0, (int) lengthBeats.size() - 1, length)]; }
    // The dark side `beats` into the song? (The first stretch light, the next dark.)
    inline bool darkAt (double beats, int length) noexcept
    {
        const auto side = (std::int64_t) std::floor (beats / beatsPerSide (length) + 1.0e-9);
        return (side & 1) != 0;
    }
    // The slide's time constant, seconds.
    inline double bendSeconds (float bend) noexcept { return 0.015 + 0.385 * (double) juce::square (juce::jlimit (0.0f, 1.0f, bend)); }

    // `note` pulled `steps` down the scale, landing in key (a note outside the scale counts from the scale note under it).
    inline int pulledNote (int note, int steps, int key, int scale) noexcept
    {
        if (steps <= 0)
            return note;
        std::array<int, 12> notes {};
        const auto count = sirocco::scaleNotes (scale, notes);
        if (count == 0)
            return note;
        const auto fromKey = note - ((key % 12) + 12) % 12;
        const auto octave = fromKey >= 0 ? fromKey / 12 : (fromKey - 11) / 12;
        const auto semitone = fromKey - octave * 12;
        auto index = 0;
        for (int k = 0; k < count; ++k)
            if (notes[(size_t) k] <= semitone)
                index = k;
        const auto target = octave * count + index - steps;
        const auto targetOctave = target >= 0 ? target / count : (target - count + 1) / count;
        const auto targetIndex = target - targetOctave * count;
        return juce::jlimit (0, 127, note - fromKey + targetOctave * 12 + notes[(size_t) targetIndex]);
    }
    inline int semitonesFor (int note, int steps, int key, int scale) noexcept { return pulledNote (note, steps, key, scale) - note; }
}
