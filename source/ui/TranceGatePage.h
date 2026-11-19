#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// TRANCE GATE's page (1.7.0). Across the top ON and START FROM (the starting patterns); in the middle the pattern: a
// column a step, its bar as tall as the step's level, the steps past LENGTH dimmed, the step playing lit, and with
// STEREO a second row for the right side. Click a step to open or close it; drag across steps to paint them the same;
// hold SHIFT (or drag with the right button) to set a step's level by height. Under it MIX, DIVISION, LENGTH, GATE,
// ATTACK, RELEASE, SWING and STEREO.
class TranceGateEditor final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit TranceGateEditor (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // A step's rectangle (row 0 the left / both, 1 the right) and the cell under a point (-1 none), for the tests too.
    juce::Rectangle<float> cellBounds (int row, int step) const;
    std::pair<int, int> cellAt (juce::Point<float>) const;
    int rows() const;

private:
    void timerCallback() override;
    void setStep (int row, int step, float level);
    float stepLevel (int row, int step) const;
    void applyAt (juce::Point<float>, bool byHeight);

    ParagonX1AudioProcessor& processor;
    float paintValue = 1.0f;
    bool byHeight = false;
    int shownStep = -2;
    std::vector<juce::RangedAudioParameter*> gesturing;
};

class TranceGatePage final : public juce::Component
{
public:
    explicit TranceGatePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    TranceGateEditor& getEditor() noexcept { return editor; }
    std::vector<ParamControl*> getShownControls();
    // Loads one of effects::TranceGate::patterns() (a menu choice; the tests call it).
    void loadPattern (int index);

private:
    ParagonX1AudioProcessor& processor;
    TranceGateEditor editor;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton startFrom { "START FROM" }, back { "BACK TO RACK" };
};
