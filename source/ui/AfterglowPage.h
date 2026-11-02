#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// AFTERGLOW's page (0.60.0): on the left eight voice cards, as an old polysynth's, each set at its own tuning and
// brightness by AGE and swaying with DRIFT (a picture of how far apart AGE and DRIFT set the notes, not the notes
// playing); on the right the ensemble's three voices sweeping round and the tape's wow in cents; under it ON, AGE,
// DRIFT, ENSEMBLE and TAPE.
class AfterglowDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit AfterglowDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    double time = 0.0;
};

class AfterglowPage final : public juce::Component
{
public:
    explicit AfterglowPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    AfterglowDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    AfterglowDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
