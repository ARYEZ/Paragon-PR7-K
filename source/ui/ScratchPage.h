#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// SCRATCH's page (HARDWIRED): a DJ's deck from above. On the left the turntable, the record turning as the hand moves it
// (forwards and back, its mark showing where the needle is from the hit), the tonearm on it; on the right the mixer's
// crossfader, open or cut; the move playing in big letters over them; along the bottom this bar's routine, a cell per
// move across its sixteenths, the one playing lit, a line where the song is. Under it ON, AMOUNT, STROKE, DIRT and MIX.
class ScratchDisplay final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    explicit ScratchDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
};

class ScratchPage final : public juce::Component
{
public:
    explicit ScratchPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    ScratchDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    ScratchDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
