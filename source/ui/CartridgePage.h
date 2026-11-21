#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// CARTRIDGE's page (1.10.0): an old game console's screen, in chunky pixels. Across the top what the chip is doing and
// the glitches so far; the four stages as a level bar (TUNED, TAKING TURNS, CRUNCH, 60 HZ FRAMES), each lit as AMOUNT
// passes it, the next one's place marked; under it the chip's channels as blocks, one per note in the chip, the one whose
// turn it is lit (at the top stage only CHANNELS of them); the screen shudders on a glitch. Under it ON, AMOUNT, RATE,
// CHANNELS, CRUNCH and GLITCH.
class CartridgeDisplay final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer
{
public:
    explicit CartridgeDisplay (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    int shownGlitches = 0;
    float shudder = 0.0f;   // 1 on a new glitch, fading
};

class CartridgePage final : public juce::Component
{
public:
    explicit CartridgePage (ParagonX1AudioProcessor&);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    CartridgeDisplay& getDisplay() noexcept { return display; }
    std::vector<ParamControl*> getShownControls();

private:
    CartridgeDisplay display;
    std::vector<std::unique_ptr<ParamControl>> top, controls;
    juce::TextButton back { "BACK TO RACK" };
};
