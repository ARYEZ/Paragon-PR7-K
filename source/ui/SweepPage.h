#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// SWEEP's page (0.87.0): on the left the sweep's path through two of its cycles (the cutoff on a log scale, 20 Hz at
// the foot to 20 kHz at the top), the cutoff now as a bright line across it; on the right the filter's response now
// (its resonant peak rising with the sweep). Across the top the shape and length, the cutoff, the level kept; under it
// ON, AMOUNT, LENGTH, SHAPE, FILTER, CENTER, RANGE, RESONANCE and PHASER.
class AnalogSweepDisplay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit AnalogSweepDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class SweepPage final : public juce::Component
{
public:
    explicit SweepPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    AnalogSweepDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    AnalogSweepDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
