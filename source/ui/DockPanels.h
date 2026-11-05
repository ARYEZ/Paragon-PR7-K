#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Icons.h"
#include "ui/SynthControls.h"

// The panels' shared style, and the SETTINGS page.

// Dark rounded panel with a small grey title, shared by the panels.
namespace DockStyle
{
    void drawPanel (juce::Graphics&, juce::Rectangle<float> area);
    void drawTitle (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title);
}

//==============================================================================
// SETTINGS: the synth's tuning, where presets are kept, and what the plugin is built from.
class SettingsPage final : public juce::Component,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit SettingsPage (ParagonX1AudioProcessor&);
    ~SettingsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    // MIDI LINKS (stage 4f): one row per link, each with a button to remove it. For tests: the rows shown.
    int getNumLinkRows() const noexcept { return (int) linkRows.size(); }
    juce::TextButton* getRemoveButton (int row) { return row < (int) linkRows.size() ? linkRows[(size_t) row]->remove.get() : nullptr; }

private:
    void timerCallback() override;
    void rebuildLinks();

    struct LinkRow final : public juce::Component
    {
        juce::Label text;
        std::unique_ptr<juce::TextButton> remove;
        void resized() override;
    };
    std::vector<std::unique_ptr<LinkRow>> linkRows;
    juce::Viewport linkView;
    juce::Component linkList;
    juce::TextButton saveDefault { "SAVE AS DEFAULT" }, exportLinks { "EXPORT..." }, importLinks { "IMPORT..." }, clearLinks { "CLEAR ALL" };
    std::unique_ptr<juce::FileChooser> chooser;
    MidiLinks shownLinks;
    juce::Rectangle<int> midiArea;

    ParagonX1AudioProcessor& processor;
    juce::Slider tune { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::AudioProcessorValueTreeState::SliderAttachment tuneAttachment;
    juce::TextButton openFolder { "OPEN PRESETS FOLDER" };
    ParamControl exprVolume;   // 0.52.0: EXPR WHEEL CONTROLS VOLUME
    // 0.74.1, FINISH (as PR7-G's): the unit's colour, the chosen one lit.
    juce::TextButton pearlWhite { "PEARL WHITE" }, pearlWhiteNeon { "WHITE + NEON" }, pearlBlack { "PEARL BLACK" };
    void showFinish();
    juce::Rectangle<int> tuneArea, rigsArea, aboutArea, exprArea, finishArea;

public:
    ParamControl& getExprVolumeSwitch() noexcept { return exprVolume; }   // for tests
    juce::TextButton& getFinishButton (int type) noexcept { return type == 0 ? pearlWhite : type == 1 ? pearlBlack : pearlWhiteNeon; }   // by Finish::Type, for tests
private:

    void changeListenerCallback (juce::ChangeBroadcaster*) override { showFinish(); }
};
