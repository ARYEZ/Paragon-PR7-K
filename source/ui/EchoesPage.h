#pragma once

#include "PluginProcessor.h"
#include "dsp/effects/Echoes.h"
#include "ui/SynthControls.h"

// ECHOES' page (0.53.0): the past, drawn. A strip of the last two stretches of the recording (newest on the left, at
// NOW), the stretch boundaries marked, and lit over it the part coming back now (an arrow for which way it plays:
// backwards for REVERSE, slow for TAPE, caught for LOOP). Under it MODE, DISTANCE, MIX, FEEDBACK (REPEATS in LOOP)
// and AGE.
class EchoesDisplay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit EchoesDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;
    const effects::Echoes::Picture& getShown() const noexcept { return shown; }   // for tests

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    effects::Echoes::Picture shown;
};

class EchoesPage final : public juce::Component,
                         private juce::Timer
{
public:
    explicit EchoesPage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    EchoesDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    EchoesDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
    int shownMode = -1;
};
