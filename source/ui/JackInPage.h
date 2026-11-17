#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// JACK-IN's page (0.85.0): the phrase as a grid of slices, two bars of them, each cell dark while untouched and lit by
// what it became (REPLAY, REVERSE, STUTTER, UP, DOWN, STOP) once taken, the newest outlined; scan lines and a trace
// across it for the break-in. Across the top the stage (CLEAN, PROBE, BREACH, TAKEOVER), the slice and the reach;
// under it ON, AMOUNT, SLICE, REACH, CHAOS, CRUSH and DROPS.
class JackInDisplay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit JackInDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class JackInPage final : public juce::Component
{
public:
    explicit JackInPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    JackInDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    JackInDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
