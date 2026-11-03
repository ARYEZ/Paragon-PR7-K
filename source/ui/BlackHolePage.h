#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// BLACKHOLE's page (0.62.0): a dark disc with a glowing ring; each slice of sound (grain) circles it, set where it was
// taken from (further out, further back in SIZE) and drawn inward as it plays, brighter as the haze is louder; HOLD,
// PULL and the haze's level across the top. Under it ON, HOLD, SIZE, PULL, TONE, WIDTH and MIX.
class BlackHoleDisplay final : public juce::Component,
                                public juce::SettableTooltipClient,
                                private juce::Timer
{
public:
    explicit BlackHoleDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    float level = -100.0f;
    float spin = 0.0f;
};

class BlackHolePage final : public juce::Component
{
public:
    explicit BlackHolePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    BlackHoleDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    BlackHoleDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
