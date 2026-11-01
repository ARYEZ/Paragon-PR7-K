#pragma once

#include "ui/SynthControls.h"

#include <juce_audio_processors/juce_audio_processors.h>

class ParagonX1AudioProcessor;

// 0.49.0 (the owner's design): the SYNTH page's front panel, a PR7 unit's face in the space the OSCILLATORS and
// FILTER panels take when the rack is shown. In the middle a big LCD (the floor unit's screen: chrome frame, black
// bezel, orange light) with the preset's slot and bank, its name and, scrolling underneath, its notes. Under it the
// three macros as big pedal knobs, named after the preset's own macros (BRIGHT, SPACE, DREAD...); either side two
// hero knobs: CUTOFF and RESONANCE on the left, ATTACK and RELEASE on the right. Up / down beside the screen step
// through the presets; clicking the screen opens the browser. MainView's SHOW RACK / HIDE RACK swaps it with the
// panels.
class SynthFace final : public juce::Component,
                        private juce::Timer
{
public:
    explicit SynthFace (ParagonX1AudioProcessor&);
    ~SynthFace() override;

    // What the screen shows (MainView fills it from the current preset).
    struct Screen
    {
        juce::String slot;     // "SV01", empty for an unsaved sound
        juce::String bank;     // "SEVERED", "A"...
        juce::String name;
        juce::String notes;
        bool modified = false;
        bool operator== (const Screen&) const = default;
    };
    void setScreen (const Screen&);
    void setMacroNames (const std::array<juce::String, 3>&);

    std::function<void()> onPrevious, onNext, onScreenClicked;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    // For tests: the screen's area and the knobs.
    juce::Rectangle<int> getScreenArea() const noexcept { return screenArea; }
    const ParamControl& getMacroKnob (int index) const { return *macros[(size_t) index]; }

private:
    void timerCallback() override;   // scrolls the notes
    void paintScreen (juce::Graphics&) const;

    std::array<std::unique_ptr<ParamControl>, 3> macros;
    ParamControl cutoff, resonance, attack, release;
    juce::TextButton previous { juce::CharPointer_UTF8 ("\xe2\x96\xb2") }, next { juce::CharPointer_UTF8 ("\xe2\x96\xbc") };
    juce::Rectangle<int> screenArea;
    Screen screen;
    float tickerOffset = 0.0f;   // how far the notes have scrolled, in pixels
    juce::Image grain;
};
