#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// OVERCHARGE's page (0.77.0): a crystal in the dark, glowing brighter as CHARGE fills its first half and as you play,
// rays of light round it; past halfway, cracks open across it and arcs of energy jump off it, flashing at each burst
// (STUTTER, CRUSH or JOLT named as it happens). Across the top the stage and the setting; under it ON, CHARGE, GLOW,
// CHAOS, TONE and WIDTH.
class OverchargeDisplay final : public juce::Component,
                                public juce::SettableTooltipClient,
                                private juce::Timer
{
public:
    explicit OverchargeDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class OverchargePage final : public juce::Component
{
public:
    explicit OverchargePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    OverchargeDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    OverchargeDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
