#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// The on-screen keyboard in THE GRID's neon (1.1.0, the owner's design, option A of two mocked up): the white keys
// glow in the pink -> red -> orange gradient across the keyboard, dim at the back and bright at the front; the black
// keys are black with a thin neon rim; keys being played glow nearly white-hot, so the chord shows. Playing it is
// unchanged (a JUCE MidiKeyboardComponent).
class NeonKeyboard final : public juce::MidiKeyboardComponent
{
public:
    NeonKeyboard (juce::MidiKeyboardState&, Orientation);

    // The neon at a place across the keyboard (0 the left end, 1 the right).
    static juce::Colour neonAt (float position) noexcept;

private:
    void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
    float positionOf (juce::Rectangle<float> area) const noexcept;
};
