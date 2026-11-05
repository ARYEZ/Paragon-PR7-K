#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// LEVIATHAN's page (0.68.0): a dark sea, the voice a vast slow shape breathing in it (swelling with its level, its
// edge rougher with GROWL), the note it sings and how far beneath yours across the top, its vowel marked along the
// bottom (OO to EE). Under it ON, MIX, DEPTH, DRIFT, GROWL, DELAY and SIZE.
class LeviathanDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit LeviathanDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    double clock = 0.0, lastTick = 0.0;
    float swell = 0.0f;   // smoothed for drawing, 0 to 1
};

class LeviathanPage final : public juce::Component
{
public:
    explicit LeviathanPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    LeviathanDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    LeviathanDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
