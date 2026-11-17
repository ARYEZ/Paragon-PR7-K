#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// CANYON's page (0.84.0): a canyon at sundown seen down its length: red rock walls either side, the far wall at the
// end under the sky, and the sound at the bottom; each wall lights up as its echo comes back, and the echoes travel
// out to the walls as arcs. Across the top the stage (OPEN PLAIN, MESA, CANYON, GORGE), the near walls' time and the
// far wall's; under it ON, AMOUNT, SIZE, FAR, REPEATS, TONE and WIDTH.
class CanyonDisplay final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    explicit CanyonDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ParagonX1AudioProcessor& processor;
};

class CanyonPage final : public juce::Component
{
public:
    explicit CanyonPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    CanyonDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    CanyonDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
