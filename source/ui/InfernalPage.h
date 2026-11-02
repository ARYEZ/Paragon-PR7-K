#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// INFERNAL's page (0.59.0): the four chains side by side (FUZZ, CRUSH, AMP + CAB, NOISE), each a bar lit to its level
// now and named with its blend; the clean lows under TIGHT and the destroyed part over it; the bloom and the glue's
// gain reduction; under it DRIVE, TIGHT, FUZZ, CRUSH, AMP, NOISE, FEEDBACK, CAB, MIX and LEVEL.
class InfernalDisplay final : public juce::Component,
                              public juce::SettableTooltipClient,
                              private juce::Timer
{
public:
    explicit InfernalDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    std::array<float, 4> held { -100.0f, -100.0f, -100.0f, -100.0f };
    float bloom = 0.0f, reduction = 0.0f;
};

class InfernalPage final : public juce::Component
{
public:
    explicit InfernalPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    InfernalDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    InfernalDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
