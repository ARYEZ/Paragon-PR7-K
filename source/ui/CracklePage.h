#pragma once

#include "PluginProcessor.h"
#include "dsp/effects/Crackle.h"
#include "ui/SynthControls.h"

// CRACKLE's page (0.56.0): a counter. The last 2.5 seconds of clicks as bars (one a 20 ms slice, newest at the right),
// the clicks counted in the last second in big digits, and under them RATE, CLUSTER, TONE, WIDTH, FOLLOW and LEVEL.
class CrackleDisplay final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    explicit CrackleDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    std::array<int, effects::Crackle::numBins> shown {};
    float rate = 0.0f;
};

class CracklePage final : public juce::Component
{
public:
    explicit CracklePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    CrackleDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    CrackleDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
