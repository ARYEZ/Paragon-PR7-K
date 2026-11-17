#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// REDLINE's page (0.88.0): a tachometer (the revs climbing across each gear, its last fifth the red zone, the needle
// dropping back at each gear change), the gear in big digits beside it, and the engine: the throb's pulse at half the
// note's pitch, the scream's level and the whine. Across the top the gear's length, the note the throb follows and the
// level kept; under it ON, THROTTLE, GEAR, THROB, SCREAM, RISE, WHINE, TONE and SHIFT.
class RedlineDisplay final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    explicit RedlineDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class RedlinePage final : public juce::Component
{
public:
    explicit RedlinePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    RedlineDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    RedlineDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
