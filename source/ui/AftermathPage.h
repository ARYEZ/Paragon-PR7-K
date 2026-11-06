#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// AFTERMATH's page (0.79.0): an open night sky that turns into a concrete room as SEAL rises. The walls slide in from
// the sides and the ceiling comes down over the stars; inside, the room's reflections ring out from the sound as it
// plays (by the room's level), and a vent high on the wall glows with the machinery's hum. Across the top the stage
// (OPEN SKY, SHELTER, SEALED) and the setting; under it ON, SEAL, ROOM, RING, DARK, HUM, HUM HZ and WIDTH.
class AftermathDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit AftermathDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class AftermathPage final : public juce::Component
{
public:
    explicit AftermathPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    AftermathDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    AftermathDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
