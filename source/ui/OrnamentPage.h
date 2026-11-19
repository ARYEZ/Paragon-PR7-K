#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// ORNAMENT's page (1.6.0): a candlelit manuscript. Across the top the share of notes ornamented and the last one named;
// in the middle the four ornaments written out as notes on a staff, step by step at SPEED (TRILL as long as LENGTH
// shows, MORDENT, TURN, and MIXED's mix), the chosen style lit gold, the others faint; the newest ornamented note's move
// now shown as a flame rising on the right (a step up, the note, a step down); under it ON, AMOUNT, STYLE, SPEED,
// LENGTH, KEY and SCALE.
class OrnamentDisplay final : public juce::Component,
                              public juce::SettableTooltipClient,
                              private juce::Timer
{
public:
    explicit OrnamentDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    int shownCount = 0;
    float flash = 0.0f;   // 1 on a new ornament, fading
};

class OrnamentPage final : public juce::Component
{
public:
    explicit OrnamentPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    OrnamentDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    OrnamentDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
