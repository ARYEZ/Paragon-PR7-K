#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// TALKBACK's page (1.9.0): CALL, the last phrase you played as notes over its bar(s), and under it ANSWER, the drum's
// strokes on a line bending between them as the drum's pitch does; on the right a talking drum (an hourglass with its
// cords), glowing with each stroke and squeezed as its pitch rises; across the top what it's doing, AMOUNT's share of
// phrases answered and the drummer's own touches; under it ON and the knobs.
class TalkBackDisplay final : public juce::Component,
                              public juce::SettableTooltipClient,
                              private juce::Timer
{
public:
    explicit TalkBackDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;
    float getGlow() const noexcept { return glow; }

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    int shownStrokes = 0;
    float glow = 0.0f;   // 1 on a stroke, fading
};

class TalkBackPage final : public juce::Component
{
public:
    explicit TalkBackPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    TalkBackDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    TalkBackDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
