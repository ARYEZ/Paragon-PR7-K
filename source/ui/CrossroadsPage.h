#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// CROSSROADS' page (0.90.0): a road forking, the light way on the left under a warm sky, the dark way on the right
// under a red one, a traveller on the way the sound is on now; along the bottom the stretches of light and dark
// with the one now lit and the pull named. Under it ON, AMOUNT, LENGTH, KEY, SCALE, PULL, BEND, LIGHT and DARK.
class CrossroadsDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit CrossroadsDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class CrossroadsPage final : public juce::Component
{
public:
    explicit CrossroadsPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    CrossroadsDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    CrossroadsDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
