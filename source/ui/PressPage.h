#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// PRESS's page (1.3.0): a koto from above, thirteen strings over their bridges tuned to KEY and SCALE; the string of
// the last note pressed glows and bends under the hand as far as it's pressed now; along the bottom the five styles
// (the one chosen lit) and the chosen style's shape over time, from the strike to the grid line it waits for; across
// the top the presses so far, the last one named, WHEN, STEPS and the scale; under it ON, AMOUNT, STYLE, WHEN, STEPS,
// KEY, SCALE and SPEED.
class PressDisplay final : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
public:
    explicit PressDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    int shownCount = 0;
    float flash = 0.0f;   // 1 on a press, fading
};

class PressPage final : public juce::Component
{
public:
    explicit PressPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    PressDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    PressDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
