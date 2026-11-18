#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// POLYMETER's page (1.4.0): on the left a wheel, one ring for each meter (4 inside, then 3, 5, 7), a dot for each
// strike of the cycle (each meter's own beats bigger), the rings that AMOUNT has brought in lit, each named with the
// lowest note on it, a hand sweeping round the cycle, a ring flashing as it strikes; on the right the same as lanes
// across the cycle, where the strikes fall against each other; across the top the meters in, the cycle and the strikes
// so far; under it ON, AMOUNT, CYCLE, DENSITY, ACCENT and LIFT.
class PolymeterDisplay final : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
public:
    explicit PolymeterDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    std::array<int, 4> shownHits {};
    std::array<float, 4> flash {};   // 1 as a meter strikes, fading
};

class PolymeterPage final : public juce::Component
{
public:
    explicit PolymeterPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    PolymeterDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    PolymeterDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
