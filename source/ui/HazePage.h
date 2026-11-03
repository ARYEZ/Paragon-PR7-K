#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// HAZE's page (0.66.0): a night road seen from above, streetlights along it, you at the kerb, the sound a pair of
// headlights sweeping along the road and back, glowing brighter as it nears; the pitch's lean and the level now,
// and how often it passes, across the top. Under it ON, PASS, DEPTH, DOPPLER, WIDTH, TONE and WHOOSH.
class HazeDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit HazeDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    double clock = 0.0, lastTick = 0.0;
};

class HazePage final : public juce::Component
{
public:
    explicit HazePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    HazeDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    HazeDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
