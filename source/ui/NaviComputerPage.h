#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// NAVICOMPUTER's page (0.86.0): the pattern's 16 steps as bars above and below a centre line (how far each moves the
// notes, in steps of the scale), lit where AMOUNT lets them move, the step playing outlined; across the top the
// pattern, the key and scale and the division; under it ON, AMOUNT, PATTERN, DIVISION, KEY, SCALE, CHAOS and GLIDE.
class NaviComputerDisplay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit NaviComputerDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class NaviComputerPage final : public juce::Component
{
public:
    explicit NaviComputerPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    NaviComputerDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    NaviComputerDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
