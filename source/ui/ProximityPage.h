#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// PROXIMITY's page (0.65.0): a keyboard seen from above with a microphone hanging over it, lower and nearer the more
// CLOSE is up; each touch lights its key and sends out a ripple (presses bright by velocity, releases small), the
// pedal under it lit while down, the room's hum as a faint line along the bottom. Under it ON, NOISE, TONE, ROOM,
// HUM, CLOSE and WIDTH.
class ProximityDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit ProximityDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    struct Ripple { float note, velocity; int kind; float age; };
    std::vector<Ripple> ripples;
    int lastCount = 0;
    bool pedalDown = false;
    double clock = 0.0, lastTick = 0.0;
};

class ProximityPage final : public juce::Component
{
public:
    explicit ProximityPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    ProximityDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    ProximityDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
