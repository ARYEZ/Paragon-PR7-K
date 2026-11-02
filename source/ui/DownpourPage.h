#pragma once

#include "PluginProcessor.h"
#include "dsp/effects/Downpour.h"
#include "ui/SynthControls.h"

// DOWNPOUR's page (0.57.0): a window in the rain. The patter as streaks (as many as are falling), each drop as a
// ripple where it lands (across by where it's placed, up by its note, named while fresh), fading over a second and a
// half; at the left, the drops a second and the last drop's note; under it RAIN, DROPS, OCTAVE, DISTANCE, SPACE,
// WIDTH, FOLLOW and LEVEL.
class DownpourDisplay final : public juce::Component,
                              public juce::SettableTooltipClient,
                              private juce::Timer
{
public:
    explicit DownpourDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

    // The note's name as the keyboard names it (C4 = 60).
    static juce::String noteName (float note);

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    std::array<effects::Downpour::DropMark, effects::Downpour::numMarks> shown {};
    long long now = 0;
    float patter = 0.0f, dropRate = 0.0f;
    int frame = 0;
};

class DownpourPage final : public juce::Component
{
public:
    explicit DownpourPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    DownpourDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    DownpourDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
