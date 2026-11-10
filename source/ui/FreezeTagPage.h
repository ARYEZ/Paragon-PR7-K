#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// FREEZE TAG's page (0.89.0): the cycle as a row of beats (green while the sound plays, red where it freezes, the beat
// now lit), and a playground figure that runs on green and stands frozen on red, shivering with TREMBLE, knocked flat
// when a freeze is caught. Across the top the cycle, the freezes counted and GO / FREEZE; under it ON, AMOUNT, CYCLE,
// HOLD, SLICE, TREMBLE, FROST, CAUGHT and LEVEL.
class FreezeTagDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit FreezeTagDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class FreezeTagPage final : public juce::Component
{
public:
    explicit FreezeTagPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    FreezeTagDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    FreezeTagDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
