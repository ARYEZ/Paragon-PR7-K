#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// SORT's page (0.93.0): a desk lit by office light, two rows of index cards. Above, the window as it was played, each
// card a slice, its mark as high as its note (blank: a silent slice) and its note named; below, the same cards
// re-filed in ORDER, the one playing now lit. Across the top the stage (LIVE, TIDY, FILED, SORTED), the order and the
// window; under it ON, AMOUNT, SLICE, LENGTH, ORDER, GAPS, EDGE and STRIKE.
class SortDisplay final : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::Timer
{
public:
    explicit SortDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class SortPage final : public juce::Component
{
public:
    explicit SortPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    SortDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    SortDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
