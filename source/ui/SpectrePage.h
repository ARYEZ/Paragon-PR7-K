#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// SPECTRE's page (0.67.0): the sound's outline across the bands as a thin line, and its ghost as a pale veil drawn
// above and below a centre line, each band of the veil drifting to the side it has wandered to, the trail and
// lingering across the top. Under it ON, MIX, TRAIL, LINGER, AIR, DRIFT and SHIFT.
class SpectreDisplay final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    explicit SpectreDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    std::array<float, 24> veil {}, outline {};   // smoothed for drawing (dB)
};

class SpectrePage final : public juce::Component
{
public:
    explicit SpectrePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    SpectreDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    SpectreDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
