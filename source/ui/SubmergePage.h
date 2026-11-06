#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// SUBMERGE's page (0.80.0): a ballroom's arched window going under. The water rises up the glass with DEPTH, the
// chandelier's warm light fading to deep green-blue as it does; the bubbles rise where the effect starts them (each at
// its place across, climbing as it ages); the glass sways with the pressure's bend, and the sea's wash glows in the
// depths. Across the top the stage (DRY LAND, SHALLOWS, UNDER, SEA FLOOR), the bend and the bubbles a second; under it
// ON, DEPTH, MURK, CURRENT, BUBBLES, SEA and WIDTH.
class SubmergeDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit SubmergeDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class SubmergePage final : public juce::Component
{
public:
    explicit SubmergePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    SubmergeDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    SubmergeDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
