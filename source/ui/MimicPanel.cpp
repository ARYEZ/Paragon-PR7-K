#include "MimicPanel.h"

#include "ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pitchColour { 0xffbfe6ff };   // ice blue, as the pitch elsewhere

    juce::String msText (float ms)
    {
        return ms >= 1000.0f ? juce::String (ms / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (ms)) + " ms";
    }
}

MimicPanel::MimicPanel (ParagonX1AudioProcessor& p) : processor (p)
{
    for (auto* button : { &load, &playOriginal, &playMade, &make, &close })
        addAndMakeVisible (*button);
    for (auto* toggle : { &tone, &envelope, &vibrato, &swoop, &unison, &noise })
    {
        toggle->setToggleState (true, juce::dontSendNotification);
        toggle->onClick = [this]
        {
            if (made)
                makeSound();   // taken again as chosen (an undo step each)
        };
        addAndMakeVisible (*toggle);
    }

    load.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("A recording of one note to make a sound from", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& c)
                              {
                                  if (c.getResult() != juce::File())
                                      loadFile (c.getResult());
                              });
    };
    playOriginal.onClick = [this]
    {
        if (processor.isPlayingMimicReference())
            processor.stopMimicReference();
        else
            processor.playMimicReference();
        updateButtons();
    };
    playMade.onClick = [this] { playResult(); };
    make.onClick = [this] { makeSound(); };
    close.onClick = [this]
    {
        processor.stopMimicReference();
        if (onClose)
            onClose();
    };
    updateButtons();
}

MimicPanel::~MimicPanel()
{
    if (playingNote >= 0)
        processor.getKeyboardState().noteOff (1, playingNote, 0.0f);
}

void MimicPanel::loadFile (const juce::File& file)
{
    busyWith = file.getFileName();
    updateButtons();
    repaint();
    juce::Component::SafePointer<MimicPanel> safe (this);
    juce::Thread::launch ([safe, file]
    {
        auto result = synth::mimic::analyseFile (file);
        juce::MessageManager::callAsync ([safe, file, result]
        {
            if (safe == nullptr)
                return;
            safe->busyWith = {};
            if (result.valid)
                safe->processor.setMimicReference (file);
            safe->setAnalysis (result);
        });
    });
}

void MimicPanel::setAnalysis (const synth::mimic::Analysis& a)
{
    analysis = a;
    made = false;
    // What wasn't found can't be taken.
    vibrato.setEnabled (a.vibratoCents > 0.0f);
    swoop.setEnabled (! a.swoop.empty());
    unison.setEnabled (a.detuneCents > 0.0f);
    noise.setEnabled (a.noise > 0.0f);
    for (auto* toggle : { &tone, &envelope, &vibrato, &swoop, &unison, &noise })
        toggle->setAlpha (toggle->isEnabled() ? 1.0f : 0.35f);
    updateButtons();
    repaint();
}

mimic::Choices MimicPanel::getChoices() const
{
    mimic::Choices c;
    c.tone = tone.getToggleState();
    c.envelope = envelope.getToggleState();
    c.vibrato = vibrato.getToggleState();
    c.swoop = swoop.getToggleState();
    c.unison = unison.getToggleState();
    c.noise = noise.getToggleState();
    return c;
}

void MimicPanel::makeSound()
{
    if (! analysis.has_value() || ! analysis->valid)
        return;
    processor.applyMimic (mimic::makeRig (*analysis, getChoices()));
    made = true;
    updateButtons();
}

void MimicPanel::playResult()
{
    if (! analysis.has_value() || ! analysis->valid)
        return;
    if (playingNote >= 0)
        processor.getKeyboardState().noteOff (1, playingNote, 0.0f);
    // The note it was recorded at, held as long as it was.
    playingNote = analysis->rootNote;
    const auto held = juce::jlimit (0.3, 8.0, analysis->releaseStart - analysis->start);
    processor.getKeyboardState().noteOn (1, playingNote, 0.8f);
    noteOffAt = juce::Time::getMillisecondCounterHiRes() + held * 1000.0;
    startTimerHz (30);
}

void MimicPanel::timerCallback()
{
    if (playingNote >= 0 && juce::Time::getMillisecondCounterHiRes() >= noteOffAt)
    {
        processor.getKeyboardState().noteOff (1, playingNote, 0.0f);
        playingNote = -1;
    }
    updateButtons();
    if (playingNote < 0 && ! processor.isPlayingMimicReference())
        stopTimer();
}

void MimicPanel::updateButtons()
{
    const auto ready = analysis.has_value() && analysis->valid && busyWith.isEmpty();
    playOriginal.setEnabled (ready);
    playOriginal.setButtonText (processor.isPlayingMimicReference() ? "STOP" : "PLAY ORIGINAL");
    make.setEnabled (ready);
    make.setButtonText (made ? "MAKE IT AGAIN" : "MAKE THIS SOUND");
    playMade.setEnabled (ready && made);
    load.setEnabled (busyWith.isEmpty());
    if (processor.isPlayingMimicReference() && ! isTimerRunning())
        startTimerHz (30);
}

bool MimicPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

void MimicPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragging = false;
    if (! files.isEmpty())
        loadFile (juce::File (files[0]));
}

juce::StringArray MimicPanel::findings() const
{
    juce::StringArray lines;
    const auto& a = *analysis;
    lines.add ("NOTE|" + synth::mimic::noteName (a.rootNote) + (std::abs (a.cents) >= 1.0f ? " " + juce::String (a.cents >= 0 ? "+" : "")
               + juce::String (juce::roundToInt (a.cents)) + " cents" : juce::String()) + "  (play it at " + synth::mimic::noteName (a.rootNote) + ")");
    lines.add ("TONE|" + juce::String (a.table.numFrames()) + (a.table.numFrames() == 1 ? " frame" : " frames over " + juce::String (a.travelSeconds, 2) + " s"));
    lines.add ("LOUDNESS|" + juce::String (a.sustained ? "held: " : "dying away: ") + "attack " + msText (a.attackMs) + ", decay "
               + msText (a.decayMs) + (a.sustained ? ", sustain " + juce::String (juce::roundToInt (a.sustain * 100.0f)) + "%" : juce::String())
               + ", release " + msText (a.releaseMs));
    lines.add ("VIBRATO|" + (a.vibratoCents > 0.0f ? juce::String (a.vibratoHz, 1) + " Hz, " + juce::String (juce::roundToInt (a.vibratoCents))
               + " cents, from " + msText (a.vibratoDelayMs) : juce::String ("none")));
    lines.add ("SWOOP|" + (a.swoop.empty() ? juce::String ("none") : "from " + juce::String (a.swoop.front(), 1) + " semitones, "
               + msText ((float) a.swoop.size() * 10.0f)));
    lines.add ("UNISON|" + (a.detuneCents > 0.0f ? juce::String (juce::roundToInt (a.detuneCents)) + " cents apart" : juce::String ("none (one oscillator)")));
    lines.add ("NOISE|" + (a.noise > 0.0f ? juce::String (juce::roundToInt (a.noise * 100.0f)) + "% of it" : juce::String ("none")));
    lines.add ("WIDTH|" + (a.width > 0.05f ? juce::String (juce::roundToInt (juce::jmin (1.0f, a.width) * 100.0f)) + "%" : juce::String ("mono")));
    return lines;
}

void MimicPanel::paintPicture (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xff111014));
    g.fillRoundedRectangle (area.toFloat(), 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 6.0f, 1.0f);
    const auto& a = *analysis;
    const auto plot = area.reduced (10, 8).toFloat();
    const auto seconds = juce::jmax (0.1, a.seconds);
    const auto xAt = [&] (double t) { return plot.getX() + (float) (t / seconds) * plot.getWidth(); };
    const auto levelTop = plot.getY(), levelHeight = plot.getHeight() * 0.62f;
    const auto yAt = [&] (float db) { return levelTop + juce::jlimit (0.0f, 1.0f, -db / 60.0f) * levelHeight; };

    // The stretch the tone is taken from.
    g.setColour (ParagonColours::accent.withAlpha (0.10f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (a.start), plot.getY(), xAt (a.start + juce::jmax (0.02, a.travelSeconds)), plot.getBottom()));

    // The loudness.
    juce::Path level;
    for (size_t h = 0; h < a.levels.size(); ++h)
    {
        const auto point = juce::Point<float> (xAt ((double) h * synth::mimic::Analysis::hopSeconds), yAt (a.levels[h]));
        h == 0 ? level.startNewSubPath (point) : level.lineTo (point);
    }
    juce::Path filled (level);
    filled.lineTo (xAt (seconds), levelTop + levelHeight);
    filled.lineTo (plot.getX(), levelTop + levelHeight);
    filled.closeSubPath();
    g.setColour (ParagonColours::accent.withAlpha (0.18f));
    g.fillPath (filled);
    g.setColour (ParagonColours::accent);
    g.strokePath (level, juce::PathStrokeType (1.6f));

    // Where it starts, settles and is let go.
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (const auto& [t, name] : { std::pair { a.start, "START" }, std::pair { a.settle, a.sustained ? "SETTLED" : "PEAK" },
                                   std::pair { a.releaseStart, a.sustained ? "LET GO" : "-40 dB" } })
    {
        const auto x = xAt (t);
        g.setColour (ParagonColours::textDim.withAlpha (0.6f));
        juce::Path line;
        line.startNewSubPath (x, plot.getY());
        line.lineTo (x, levelTop + levelHeight);
        juce::Path dashed;
        const float dashes[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dashes, 2);
        g.fillPath (dashed);
        g.setColour (ParagonColours::textDim);
        g.drawText (name, juce::Rectangle<float> (x + 3.0f, plot.getY(), 70.0f, 12.0f), juce::Justification::centredLeft, false);
    }

    // The pitch, cents off the note, under it.
    const auto pitchArea = juce::Rectangle<float> (plot.getX(), levelTop + levelHeight + 6.0f, plot.getWidth(), plot.getBottom() - levelTop - levelHeight - 6.0f);
    float most = 50.0f;
    for (const auto c : a.pitches)
        if (! std::isnan (c))
            most = juce::jmax (most, juce::jmin (1200.0f, std::abs (c)));
    g.setColour (ParagonColours::panelEdge);
    g.drawHorizontalLine ((int) pitchArea.getCentreY(), pitchArea.getX(), pitchArea.getRight());
    juce::Path pitch;
    bool drawing = false;
    for (size_t h = 0; h < a.pitches.size(); ++h)
    {
        const auto c = a.pitches[h];
        if (std::isnan (c))
        {
            drawing = false;
            continue;
        }
        const auto point = juce::Point<float> (xAt ((double) h * synth::mimic::Analysis::hopSeconds),
                                               pitchArea.getCentreY() - juce::jlimit (-1.0f, 1.0f, c / most) * pitchArea.getHeight() * 0.45f);
        drawing ? pitch.lineTo (point) : pitch.startNewSubPath (point);
        drawing = true;
    }
    g.setColour (pitchColour);
    g.strokePath (pitch, juce::PathStrokeType (1.4f));
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("PITCH (+-" + juce::String (juce::roundToInt (most)) + " CENTS)", pitchArea.withHeight (12.0f), juce::Justification::centredLeft, false);
    g.drawText (juce::String (seconds, 1) + " s", plot.withTrimmedTop (plot.getHeight() - 12.0f), juce::Justification::centredRight, false);
}

void MimicPanel::paint (juce::Graphics& g)
{
    g.setColour (ParagonColours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (dragging ? ParagonColours::accent : ParagonColours::panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, dragging ? 2.0f : 1.0f);

    auto title = titleArea;
    g.setColour (ParagonColours::text);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText ("MIMIC", title.removeFromTop (24), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText ("Make a sound from a recording of one: load a note (or drop one here), held and fairly dry is best. "
                      "MIMIC takes its own waves as a wavetable, its loudness and its movement. Close, not a copy: then shape it as you like.",
                      title, juce::Justification::topLeft, 3);

    if (busyWith.isNotEmpty() || ! analysis.has_value())
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (15.0f));
        g.drawFittedText (busyWith.isNotEmpty() ? "Listening to " + busyWith + "..." : "Load a sound to begin.",
                          pictureArea, juce::Justification::centred, 2);
        return;
    }
    if (! analysis->valid)
    {
        g.setColour (juce::Colour (0xffff6b5e));
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText ("\"" + analysis->name + "\": " + analysis->problem, pictureArea.reduced (20), juce::Justification::centred, 4);
        return;
    }

    paintPicture (g, pictureArea);

    auto area = findingsArea;
    g.setColour (ParagonColours::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("\"" + analysis->name + "\"", area.removeFromTop (22), juce::Justification::centredLeft, true);
    area.removeFromTop (4);
    for (const auto& line : findings())
    {
        auto row = area.removeFromTop (21);
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (line.upToFirstOccurrenceOf ("|", false, false), row.removeFromLeft (78), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::text);
        g.setFont (juce::FontOptions (12.5f));
        g.drawFittedText (line.fromFirstOccurrenceOf ("|", false, false), row, juce::Justification::centredLeft, 1);
    }
}

void MimicPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 12);
    titleArea = area.removeFromTop (60);
    area.removeFromTop (6);

    auto buttons = area.removeFromBottom (32);
    close.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (8);
    make.setBounds (buttons.removeFromRight (170));
    buttons.removeFromRight (8);
    playMade.setBounds (buttons.removeFromRight (120));
    buttons.removeFromRight (8);
    playOriginal.setBounds (buttons.removeFromRight (130));
    load.setBounds (buttons.removeFromLeft (150));
    area.removeFromBottom (10);

    auto toggles = area.removeFromBottom (26);
    const auto width = toggles.getWidth() / 6;
    for (auto* toggle : { &tone, &envelope, &vibrato, &swoop, &unison, &noise })
        toggle->setBounds (toggles.removeFromLeft (width));
    area.removeFromBottom (8);

    findingsArea = area.removeFromRight (juce::jmin (380, area.getWidth() / 2));
    area.removeFromRight (12);
    pictureArea = area;
}
