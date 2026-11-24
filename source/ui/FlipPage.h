#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// FLIP's page (HARDWIRED): a 90s sampler's face. Its green LCD says how this bar flips and the pad playing; sixteen pads
// in a four by four grid, each lit as its cell plays (pitched pads tinted, reversed ones marked); along the bottom this
// bar's cells across its sixteenths, a line where the song is. Under it ON, AMOUNT, PITCH, CRUNCH, PUMP and MIX.
class FlipDisplay final : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::Timer
{
public:
    explicit FlipDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
};

class FlipPage final : public juce::Component
{
public:
    explicit FlipPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    FlipDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    FlipDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
