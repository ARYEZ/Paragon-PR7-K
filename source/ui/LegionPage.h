#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// LEGION's page (0.76.0): a stage seen from above, the sound itself at the front as the leader and up to eight players
// behind it, each where SPREAD places it across the stage and as far back as it plays late (LOOSE), lit by how far it
// has joined (SIZE) and swaying as it drifts in and out of tune (DETUNE); how many are playing across the top. Under it
// ON, SIZE, SPREAD, LOOSE, DETUNE and TONE.
class LegionDisplay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit LegionDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class LegionPage final : public juce::Component
{
public:
    explicit LegionPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    LegionDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    LegionDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
