#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "dsp/synth/Mimic.h"
#include "rigs/MimicRig.h"

#include <optional>

// MIMIC (0.44.0): make a sound from a recording, on the PRESETS page (MIMIC A SOUND... in the right column), over the
// preset list. See dsp/synth/Mimic.h for what it measures and rigs/MimicRig.h for the sound it makes.
//
//   1. LOAD SOUND... (or drop an audio file on it): one note, held, fairly dry, is best. It's analysed in the
//      background, a fraction of a second.
//   2. The picture: the recording's loudness, where the note starts, settles and is let go, the stretch its tone is
//      taken from, and its pitch. Beside it, what was found.
//   3. TONE, LOUDNESS, VIBRATO, SWOOP, UNISON, NOISE: what to take (the ones not found are greyed).
//   4. MAKE THIS SOUND puts it in place (one undo takes it back). PLAY ORIGINAL and PLAY RESULT (the note it was
//      recorded at, held as long) to compare.
class MimicPanel final : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    explicit MimicPanel (ParagonX1AudioProcessor&);
    ~MimicPanel() override;

    std::function<void()> onClose;

    // Analyses `file` in the background; the panel shows the result when it's done.
    void loadFile (const juce::File& file);
    // Shows an analysis made already (tests, the snapshot).
    void setAnalysis (const synth::mimic::Analysis&);
    const std::optional<synth::mimic::Analysis>& getAnalysis() const noexcept { return analysis; }
    mimic::Choices getChoices() const;
    void makeSound();
    void playResult();

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }

private:
    void timerCallback() override;
    void updateButtons();
    void paintPicture (juce::Graphics&, juce::Rectangle<int> area) const;
    juce::StringArray findings() const;

    ParagonX1AudioProcessor& processor;
    std::optional<synth::mimic::Analysis> analysis;
    juce::String busyWith;            // the file being analysed (empty: none)
    bool dragging = false, made = false;
    int playingNote = -1;
    double noteOffAt = 0.0;

    juce::TextButton load { "LOAD SOUND..." }, playOriginal { "PLAY ORIGINAL" }, playMade { "PLAY RESULT" },
                     make { "MAKE THIS SOUND" }, close { "CLOSE" };
    juce::ToggleButton tone { "TONE" }, envelope { "LOUDNESS" }, vibrato { "VIBRATO" }, swoop { "SWOOP" },
                       unison { "UNISON" }, noise { "NOISE" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> titleArea, pictureArea, findingsArea;
};
