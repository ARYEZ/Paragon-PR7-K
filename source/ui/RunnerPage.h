#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// RUNNER's page (0.100.0): a stone corridor running away from you, its walls flashing with every strike; along the
// bottom the five speeds as gates (1/4 to 1/64, the one AMOUNT is at lit, the fifths of the knob marked), over them
// the beat cut into its strikes, the last one lit; across the top the speed, ACCENT and LIFT; under it ON, AMOUNT,
// ACCENT and LIFT.
class RunnerDisplay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit RunnerDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;
    float getFlash() const noexcept { return flash; }

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    int shownStrikes = 0;
    float flash = 0.0f;   // 1 on a strike, fading
};

class RunnerPage final : public juce::Component
{
public:
    explicit RunnerPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    RunnerDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    RunnerDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
