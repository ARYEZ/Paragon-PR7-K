#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// COLOSSUS's page (0.82.0): dunes under a dusk sky, and a vast dark shape rising over the horizon as SIZE grows (its
// glow the octave under), rings spreading behind it with the hall, the ground and the dust jumping with each boom.
// Across the top the stage (AS IT IS, GIANT, TITAN, COLOSSUS), the hall's time and the booms; under it ON, SIZE, DEPTH,
// WEIGHT, HALL, BOOM, TONE and WIDTH.
class ColossusDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit ColossusDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class ColossusPage final : public juce::Component
{
public:
    explicit ColossusPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    ColossusDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    ColossusDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
