#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// NUCLEARSPRING's page (0.69.0): the tank opened up, three springs a side stretched between their mounts, each
// shaking as the sound runs through it, the frame flashing when a hit kicks the tank; the decay, drip and size
// across the top. Under it ON, MIX, DECAY, DRIP, TONE, SIZE, BOING and WIDTH.
class NuclearSpringDisplay final : public juce::Component,
                                   public juce::SettableTooltipClient,
                                   private juce::Timer
{
public:
    explicit NuclearSpringDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    double clock = 0.0, lastTick = 0.0;
    std::array<std::array<float, 3>, 2> shake {};   // smoothed for drawing
    float flash = 0.0f;
};

class NuclearSpringPage final : public juce::Component
{
public:
    explicit NuclearSpringPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    NuclearSpringDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    NuclearSpringDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
