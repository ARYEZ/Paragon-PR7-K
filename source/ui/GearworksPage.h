#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// GEARWORKS' page (0.91.0): two gears meshing, turning in time with the song, the tooth coming round lit and the
// metal flashing as it strikes; along the bottom a whole turn of steps, the cut ones marked, the one now lit. Under it
// ON, AMOUNT, RATIO, DIVISION, BITE, METAL, GRIND, STEAM and CLANK.
class GearworksDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit GearworksDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class GearworksPage final : public juce::Component
{
public:
    explicit GearworksPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    GearworksDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    GearworksDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
