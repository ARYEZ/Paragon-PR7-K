#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// LOCKSTEP's page (0.81.0): a parade ground from above. Two bars of steps run across (each block as wide as GATE
// opens it, the off-beat steps shorter by ACCENT), the step now lit and moving with the song; below, ranks of identical
// figures fall into line as AMOUNT rises (loose and scattered at 0, ruler-straight at full), flashing with each stamp.
// Across the top the stage (FREE, RANKS, MARCH, DRILLED), the step and the make-up; under it ON, AMOUNT, DIVISION,
// GATE, ACCENT, SQUEEZE and STAMP.
class LockstepDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit LockstepDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class LockstepPage final : public juce::Component
{
public:
    explicit LockstepPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    LockstepDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    LockstepDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
