#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// SIROCCO's page (0.58.0): two octaves and a half of keys, the scale's notes marked (the key's root brightest), the
// notes sounding now lit (played in the plugin's orange, SIROCCO's answers in pale blue); the scale's 3rds named under
// its notes; and under it KEY, SCALE, INTERVAL, VOICES, DELAY, LEVEL, DETUNE and SPREAD.
class SiroccoDisplay final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    explicit SiroccoDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    std::array<std::uint8_t, 128> shown {};
};

class SiroccoPage final : public juce::Component
{
public:
    explicit SiroccoPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    SiroccoDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    SiroccoDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
