#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// REMNANT's page (0.64.0): nested rings, your sound the bright one outside and each layer a ring further in, turning
// slower the deeper it goes (STEP's speed each), lit by how loud that layer is now, its renewing marked as a sweep
// round it; the layers in use, STEP and LENGTH across the top. Under it ON, LAYERS, STEP, LENGTH, TAPER, TONE, WIDTH
// and MIX.
class RemnantDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit RemnantDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    double clock = 0.0, lastTick = 0.0;
};

class RemnantPage final : public juce::Component
{
public:
    explicit RemnantPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    RemnantDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    RemnantDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
