#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// LIGHTSPEED's page (0.73.0): a starfield seen from the cockpit, the stars stretching into streaks from the centre as
// the jump spools up (how far, from the effect itself) and the whoosh's glow; the pitch the streaks have reached and
// the whoosh's level across the top. Under it ON, JUMP, RISE, STREAKS, WHOOSH, WIDTH and TONE.
class BloomDisplay final : public juce::Component,
                                public juce::SettableTooltipClient,
                                private juce::Timer
{
public:
    explicit BloomDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    float phase = 0.0f;   // the strings' shimmer
};

class BloomPage final : public juce::Component
{
public:
    explicit BloomPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    BloomDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    BloomDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
