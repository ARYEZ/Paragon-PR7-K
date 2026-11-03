#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// LIGHTTRAIL's page (0.61.0): the trail drawn as it's set, each repeat a light further along in time (DIVISION), up
// or down by its pitch (RISE), dimmer as it fades (LENGTH), its colour cooling as TONE darkens it and swinging side
// to side as SPREAD crosses it over; a glow along the trail with its level now. Under it ON, DIVISION, LENGTH, RISE,
// TONE, SPREAD and MIX.
class LightTrailDisplay final : public juce::Component,
                                public juce::SettableTooltipClient,
                                private juce::Timer
{
public:
    explicit LightTrailDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    float level = -100.0f;
};

class LightTrailPage final : public juce::Component
{
public:
    explicit LightTrailPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    LightTrailDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    LightTrailDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
