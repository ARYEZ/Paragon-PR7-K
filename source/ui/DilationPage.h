#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// DILATION's page (0.63.0): a clock. A dim hand keeps real time (a turn every 8 s, REACH's longest); the bright hand
// is the sound you hear, trailing it by how far behind real time the sound has fallen, with ghost hands behind it the
// slower it goes; REACH marked as the arc it can't fall past. The speed, semitones, how far behind and RETURN across
// the top. Under it ON, SLOW, RAMP, RETURN, REACH, TONE and MIX.
class DilationDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit DilationDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    double realTime = 0.0, lastTick = 0.0;
};

class DilationPage final : public juce::Component
{
public:
    explicit DilationPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    DilationDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    DilationDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
