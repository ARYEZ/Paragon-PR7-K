#include "SiroccoPage.h"

#include "dsp/synth/Sirocco.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour answerColour (0xff9fd4ff);

    synth::sirocco::Settings settingsOf (ParagonX1AudioProcessor& p)
    {
        auto& state = p.getParameters();
        const auto value = [&state] (const char* id) { return state.getRawParameterValue (id)->load(); };
        synth::sirocco::Settings s;
        s.on = value (ParamIDs::siroccoOn) > 0.5f;
        s.key = juce::roundToInt (value (ParamIDs::siroccoKey));
        s.scale = juce::roundToInt (value (ParamIDs::siroccoScale));
        s.interval = juce::roundToInt (value (ParamIDs::siroccoInterval));
        s.voices = juce::roundToInt (value (ParamIDs::siroccoVoices));
        return s;
    }
}

SiroccoDisplay::SiroccoDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The scale SIROCCO keeps to (its notes marked, the key's root brightest) and what's sounding: the notes "
                "played in orange, the answers in pale blue.");
    startTimerHz (25);
}

void SiroccoDisplay::timerCallback()
{
    shown = processor.getSoundingNoteFlags();
    repaint();
}

void SiroccoDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff070507));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    const auto s = settingsOf (processor);
    auto area = bounds.reduced (24.0f, 18.0f);

    // The scale, named, and what answers what.
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText (synth::sirocco::keyNames[s.key] + " " + synth::arp::scaleNames[s.scale].toUpperCase(), area.removeFromTop (26.0f),
                juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("ANSWERS: " + synth::sirocco::intervalNames[s.interval] + (s.voices > 1 ? ", " + juce::String (s.voices) + " VOICES STACKED IN 3RDS" : juce::String()),
                area.removeFromTop (18.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (12.0f);

    // The keys: C3 to F5 (48 to 77), white keys first, black over them.
    const int low = 48, high = 77;
    auto keys = area.withTrimmedBottom (34.0f);
    int whites = 0;
    for (int n = low; n <= high; ++n)
        if (! juce::MidiMessage::isMidiNoteBlack (n))
            ++whites;
    const auto whiteWidth = keys.getWidth() / (float) whites;
    std::array<int, 12> scaleNotes {};
    const auto count = synth::sirocco::scaleNotes (s.scale, scaleNotes);
    const auto inScale = [&] (int n)
    {
        const auto semitone = ((n - s.key) % 12 + 12) % 12;
        return std::find (scaleNotes.begin(), scaleNotes.begin() + count, semitone) != scaleNotes.begin() + count;
    };
    std::map<int, juce::Rectangle<float>> rects;
    int w = 0;
    for (int n = low; n <= high; ++n)
        if (! juce::MidiMessage::isMidiNoteBlack (n))
            rects[n] = juce::Rectangle<float> (keys.getX() + whiteWidth * (float) w++, keys.getY(), whiteWidth - 2.0f, keys.getHeight());
    for (int n = low; n <= high; ++n)
        if (juce::MidiMessage::isMidiNoteBlack (n))
            rects[n] = juce::Rectangle<float> (rects[n - 1].getRight() - whiteWidth * 0.3f, keys.getY(), whiteWidth * 0.6f, keys.getHeight() * 0.6f);
    const auto paintKey = [&] (int n)
    {
        const auto r = rects[n];
        const auto black = juce::MidiMessage::isMidiNoteBlack (n);
        g.setColour (black ? juce::Colour (0xff1a1618) : juce::Colour (0xffd8d2d4));
        g.fillRoundedRectangle (r, 3.0f);
        if (inScale (n))
        {
            const auto root = ((n - s.key) % 12 + 12) % 12 == 0;
            g.setColour (ParagonColours::accent.withAlpha (root ? 0.95f : 0.45f));
            g.fillEllipse (r.getCentreX() - 5.0f, r.getBottom() - 16.0f, 10.0f, 10.0f);
        }
        if (const auto flag = shown[(size_t) n]; flag != 0)
        {
            g.setColour (((flag & 1) != 0 ? ParagonColours::accent : answerColour).withAlpha (0.85f));
            g.fillRoundedRectangle (r.reduced (2.0f).withTrimmedBottom (r.getHeight() * 0.3f), 3.0f);
        }
    };
    for (int n = low; n <= high; ++n)
        if (! juce::MidiMessage::isMidiNoteBlack (n))
            paintKey (n);
    for (int n = low; n <= high; ++n)
        if (juce::MidiMessage::isMidiNoteBlack (n))
            paintKey (n);

    // The legend.
    auto legend = area.removeFromBottom (24.0f);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (ParagonColours::accent);
    g.fillRoundedRectangle (legend.removeFromLeft (16.0f).withSizeKeepingCentre (14.0f, 10.0f), 2.0f);
    g.setColour (ParagonColours::textDim);
    g.drawText ("PLAYED", legend.removeFromLeft (80.0f).withTrimmedLeft (6.0f), juce::Justification::centredLeft, false);
    g.setColour (answerColour);
    g.fillRoundedRectangle (legend.removeFromLeft (16.0f).withSizeKeepingCentre (14.0f, 10.0f), 2.0f);
    g.setColour (ParagonColours::textDim);
    g.drawText ("ANSWER", legend.removeFromLeft (90.0f).withTrimmedLeft (6.0f), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::accent.withAlpha (0.6f));
    g.fillEllipse (legend.removeFromLeft (16.0f).withSizeKeepingCentre (10.0f, 10.0f));
    g.setColour (ParagonColours::textDim);
    g.drawText ("IN THE SCALE", legend.withTrimmedLeft (6.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
SiroccoPage::SiroccoPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::siroccoOn, "ON", "SIROCCO on or off: a harmony answering every note you play (and the arps'), kept to a scale.");
    make (controls, ParamIDs::siroccoKey, "KEY", "The scale's root.");
    make (controls, ParamIDs::siroccoScale, "SCALE", "The scale the answers keep to. A note you play outside it is answered in parallel.");
    make (controls, ParamIDs::siroccoInterval, "INTERVAL", "How far the answer is from the note, in steps of the scale (a 3rd is two steps).");
    make (controls, ParamIDs::siroccoVoices, "VOICES", "1 to 3 answers, stacked a 3rd apart: a small choir.");
    make (controls, ParamIDs::siroccoDelay, "DELAY", "The answers come this long after the note (0: with it).");
    make (controls, ParamIDs::siroccoLevel, "LEVEL", "How loud the answers are; with more VOICES they come in one at a time as it rises. "
                                                     "The mod matrix can move it (Sirocco > Level).");
    make (controls, ParamIDs::siroccoDetune, "DETUNE", "Each answer a few cents off, alternately sharp and flat: wider, more human.");
    make (controls, ParamIDs::siroccoSpread, "SPREAD", "How far the answers are placed left and right.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> SiroccoPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void SiroccoPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("SIROCCO", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: answers played by the synth itself", juce::Rectangle<int> (24, 38, 320, 16), juce::Justification::centredLeft, false);
}

void SiroccoPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (330);
    back.setBounds (bar.removeFromRight (150).withSizeKeepingCentre (150, 30));
    bar.removeFromRight (20);
    top[0]->setBounds (bar.removeFromLeft (70));
    area.removeFromTop (8);
    auto controlRow = area.removeFromBottom (ParamControl::knobHeight);
    area.removeFromBottom (10);
    display.setBounds (area);
    std::vector<juce::Component*> row;
    for (auto& c : controls)
        row.push_back (c.get());
    SynthPanel::layoutRow (controlRow, row, 112);
}
