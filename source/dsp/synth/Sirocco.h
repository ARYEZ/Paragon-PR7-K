#pragma once

#include "Arpeggiator.h"

#include <juce_core/juce_core.h>

#include <array>

// SIROCCO (0.58.0, the SANDSEA expansion's signature, the owner's name): a harmony that answers every note played, the
// keys' and the arps', snapped to a scale. Our own design. Not an audio effect: each answer is a voice of the synth
// itself (the same oscillators, filter and envelopes as the note it answers), so it sounds as clean as the note.
//
//  - KEY and SCALE: the scale the answers keep to (the arp randomizer's scales: Phrygian dominant, double harmonic,
//    pelog and the rest). A played note outside the scale is answered in parallel, keeping its distance from the
//    scale note under it.
//  - INTERVAL: how far the first answer is, in steps of the scale (a 3rd is two steps: in Phrygian dominant on D, D's
//    3rd is F#, E-flat's is G). VOICES 2 and 3 stack further answers a 3rd beyond the last (3rd, 5th, 7th above, or
//    below for the BELOW intervals), a small choir.
//  - DELAY: the answers come this long after the note (0: with it).
//  - LEVEL: how loud the answers are. With VOICES over 1 they come in one at a time as it rises (the first by 1/VOICES,
//    all at the top); each answer at full is 3 dB under the note it answers.
//  - DETUNE and SPREAD: each answer a few cents off (alternately sharp and flat) and placed left and right.
//
// Answers follow their note: a mono lead's answers glide when it glides; let go of the note and they let go too.
namespace synth::sirocco
{
    inline const juce::StringArray keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };   // never reorder
    inline const juce::StringArray intervalNames { "3RD ABOVE", "4TH ABOVE", "5TH ABOVE", "6TH ABOVE", "OCTAVE ABOVE",
                                                   "3RD BELOW", "4TH BELOW", "5TH BELOW", "6TH BELOW", "OCTAVE BELOW" };   // never reorder
    inline constexpr int maxVoices = 3;
    inline constexpr float answerGain = 0.7071f;   // -3 dB under the note at full

    struct Settings
    {
        bool on = false;
        int key = 0;            // keyNames: C
        int scale = 1;          // arp::scaleNames: Major
        int interval = 0;       // intervalNames: 3RD ABOVE
        int voices = 1;         // 1 to 3
        float delayMs = 0.0f;   // 0 to 500
        float level = 0.7f;     // 0 to 1
        float detuneCents = 6.0f;
        float spread = 0.5f;    // 0 to 1

        bool answers() const noexcept { return on && level > 0.0f; }
    };

    // The scale's notes above the key (semitones 0-11), and how many.
    inline int scaleNotes (int scale, std::array<int, 12>& out) noexcept
    {
        int count = 0;
        for (int s = 0; s < 12; ++s)
            if (arp::inScale (scale, s))
                out[(size_t) count++] = s;
        return count;
    }

    // Steps of the scale for answer `layer` (0 the first) at `interval`.
    inline int stepsFor (int interval, int layer, int notesInScale) noexcept
    {
        const auto i = juce::jlimit (0, intervalNames.size() - 1, interval);
        const auto above = i < 5;
        const auto kind = above ? i : i - 5;   // 3rd, 4th, 5th, 6th, octave
        const auto base = kind == 4 ? notesInScale : kind + 2;
        return (above ? 1 : -1) * (base + 2 * layer);
    }

    // The note answering `note`: `steps` steps of the scale from it. Outside the scale, in parallel from the scale note
    // under it.
    inline int harmonyNote (int note, int steps, int key, int scale) noexcept
    {
        std::array<int, 12> notes {};
        const auto count = scaleNotes (scale, notes);
        if (count == 0)
            return note;
        const auto fromKey = note - ((key % 12) + 12) % 12;
        const auto octave = (fromKey >= 0 ? fromKey / 12 : (fromKey - 11) / 12);
        const auto semitone = fromKey - octave * 12;
        auto index = 0;
        for (int k = 0; k < count; ++k)
            if (notes[(size_t) k] <= semitone)
                index = k;
        const auto outside = semitone - notes[(size_t) index];
        const auto target = octave * count + index + steps;
        const auto targetOctave = target >= 0 ? target / count : (target - count + 1) / count;
        const auto targetIndex = target - targetOctave * count;
        return juce::jlimit (0, 127, note - fromKey + targetOctave * 12 + notes[(size_t) targetIndex] + outside);
    }

    // How loud answer `layer` is at LEVEL `level` with `voices` voices (0 to 1, times answerGain).
    inline float layerGain (float level, int layer, int voices) noexcept
    {
        const auto v = juce::jlimit (1, maxVoices, voices);
        return answerGain * juce::jlimit (0.0f, 1.0f, juce::jlimit (0.0f, 1.0f, level) * (float) v - (float) layer);
    }
}
