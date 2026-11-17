#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// WOBBLE's page (0.97.0): the pattern's two bars as the wobble opening and closing (the cutoff, low at the foot),
// each beat named with its speed, where the song is now lit; on the right TALK's vowels, OO shut to AH open, the
// one sounding lit. Across the top the pattern and shape, the cutoff, the level kept; under it ON, AMOUNT, PATTERN,
// SHAPE, LOW, RANGE, RESONANCE, TALK and DRIVE.
class WobbleDisplay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit WobbleDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class WobblePage final : public juce::Component
{
public:
    explicit WobblePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    WobbleDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    WobbleDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
