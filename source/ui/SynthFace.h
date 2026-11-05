#pragma once

#include "ui/SynthControls.h"

#include <juce_audio_processors/juce_audio_processors.h>

class ParagonX1AudioProcessor;

// A performance wheel on the front panel (0.49.0): a ridged black wheel in a slot, lit orange from below as it's
// turned, its name under it. Drag up and down; double-click returns it to its rest (MOD 0, EXPR full).
class FaceWheel final : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    FaceWheel (const juce::String& name, float rest);
    float getValue() const noexcept { return value; }
    void setValue (float, bool notify);
    bool isBeingDragged() const noexcept { return dragging; }
    std::function<void (float)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseDoubleClick (const juce::MouseEvent&) override { setValue (rest, true); }

private:
    juce::String name;
    float rest, value, dragStartValue = 0.0f;
    bool dragging = false;
    juce::Rectangle<float> slotArea() const;
};

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
        // 0.73.1: a library being downloaded ("DOWNLOADING THE ORCHESTRA 34%"), shown in the notes' place with a bar
        // (progress 0-1; below 0, no bar: a failure's message).
        juce::String status;
        float progress = -1.0f;
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
    FaceWheel& getModWheel() noexcept { return modWheel; }
    FaceWheel& getExpressionWheel() noexcept { return exprWheel; }

private:
    void timerCallback() override;   // scrolls the notes
    void paintScreen (juce::Graphics&) const;

    ParagonX1AudioProcessor& processor;
    FaceWheel modWheel { "MOD", 0.0f }, exprWheel { "EXPR", 1.0f };
    std::array<std::unique_ptr<ParamControl>, 3> macros;
    ParamControl cutoff, resonance, attack, release;
    juce::TextButton previous { juce::CharPointer_UTF8 ("\xe2\x96\xb2") }, next { juce::CharPointer_UTF8 ("\xe2\x96\xbc") };
    juce::Rectangle<int> screenArea;
    Screen screen;
    float tickerOffset = 0.0f;   // how far the notes have scrolled, in pixels
    juce::Image grain;
};
