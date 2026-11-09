#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// HYPERJUMP's page (0.86.0): a warp tunnel closing in as the build runs (rings drawn towards the centre with the
// build's progress, the image's narrowing), the build's beats along the bottom lit as they pass with the drop marked at
// the end, and a white flash as the drop lands. Across the top the state (OFF, LANDED, BUILDING, DROP), the climb and
// the beats to the drop; under it ON, AMOUNT, LENGTH, MODE, RISE, CHOP and HIT.
class HyperJumpDisplay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit HyperJumpDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class HyperJumpPage final : public juce::Component
{
public:
    explicit HyperJumpPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    HyperJumpDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    HyperJumpDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
