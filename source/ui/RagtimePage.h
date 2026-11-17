#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// RAGTIME's page (0.92.0): the player piano's paper roll, a bar of 16 steps in two lanes (the bass and the chord),
// each strike a punched hole (the main beats largest, the syncopations smaller, the pickups smallest), the holes
// AMOUNT hasn't reached yet only marked, the step playing lit; across the top the roll, STRIDE and the division;
// under it ON, AMOUNT, PATTERN, DIVISION, SWING, ACCENT, LIFT and STRIDE.
class RagtimeDisplay final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    explicit RagtimeDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class RagtimePage final : public juce::Component
{
public:
    explicit RagtimePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    RagtimeDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    RagtimeDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
