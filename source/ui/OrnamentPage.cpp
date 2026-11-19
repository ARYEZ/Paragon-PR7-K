#include "OrnamentPage.h"

#include "dsp/synth/Ornament.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour crimson (0xffc4122e), gold (0xffe9b44c), parchment (0xffd9c7a3), ink (0xff1a0a0e);

    juce::String noteName (int note)
    {
        return synth::sirocco::keyNames[((note % 12) + 12) % 12] + juce::String (note / 12 - 1);
    }
}

OrnamentDisplay::OrnamentDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The four ornaments written out on a staff, a step at SPEED: TRILL (the note and the next note up, as long as "
                "LENGTH), MORDENT (a flick up and back), TURN (above, the note, below, the note) and MIXED. The chosen one "
                "is lit; the flame on the right is the newest ornamented note's move now.");
    startTimerHz (30);
}

void OrnamentDisplay::timerCallback()
{
    const auto count = processor.getOrnamentCount();
    if (count != shownCount)
    {
        shownCount = count;
        flash = 1.0f;
    }
    else
        flash = juce::jmax (0.0f, flash - 0.05f);
    repaint();
}

void OrnamentDisplay::paint (juce::Graphics& g)
{
    namespace orn = synth::ornament;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getOrnamentSettings();
    const auto live = settings.ornamenting();
    const auto last = processor.getOrnamentLast();
    const auto lastNote = last >= 0 ? last % 1000 : -1;
    const auto lastStyle = last >= 0 ? last / 1000 : -1;
    const auto now = live && processor.getOrnamentCount() > 0 ? processor.getOrnamentNow() : 0;

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff12060a), view.getCentreX(), view.getY(),
                                             juce::Colour (0xff050203), view.getCentreX(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    g.setColour (crimson.withAlpha (0.35f));
    g.drawRoundedRectangle (view.reduced (3.0f), 5.0f, 1.0f);

    // The top bar.
    auto bar = view.reduced (14.0f, 6.0f).removeFromTop (22.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (! settings.on ? juce::String ("OFF") : ! live ? juce::String ("RESTING")
                : juce::String (juce::roundToInt (100.0f * settings.amount)) + "% OF NOTES ORNAMENTED"
                      + (lastNote >= 0 ? "   LAST: " + noteName (lastNote) + " " + orn::styleNames[juce::jlimit (0, 2, lastStyle)] : juce::String()),
                bar, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.drawText ("STEP " + orn::speedNames[settings.speed] + "   TRILL " + orn::lengthNames[settings.length] + "   "
                    + synth::sirocco::keyNames[settings.key] + " " + synth::arp::scaleNames[settings.scale].toUpperCase(),
                bar, juce::Justification::centredRight, false);

    auto body = view.reduced (14.0f, 0.0f).withTrimmedTop (34.0f).withTrimmedBottom (10.0f);
    auto flame = body.removeFromRight (juce::jmin (130.0f, body.getWidth() * 0.16f));
    body.removeFromRight (10.0f);

    // The four ornaments, side by side, each on a staff of its own.
    const auto panelWidth = body.getWidth() / 4.0f;
    const auto trillSteps = (int) juce::jlimit<std::int64_t> (2, 12, orn::trillSteps (settings));
    for (int style = 0; style < 4; ++style)
    {
        auto panel = juce::Rectangle<float> (body.getX() + panelWidth * (float) style, body.getY(), panelWidth, body.getHeight()).reduced (5.0f, 0.0f);
        const auto chosen = settings.style == style || (settings.style == orn::mixed && style != orn::mixed);
        const auto lit = chosen && settings.on;
        g.setColour (parchment.withAlpha (lit ? 0.10f : 0.04f));
        g.fillRoundedRectangle (panel, 5.0f);
        g.setColour ((lit ? gold : parchment).withAlpha (lit ? 0.9f : 0.25f));
        g.drawRoundedRectangle (panel, 5.0f, lit ? 1.4f : 0.8f);
        auto title = panel.removeFromTop (24.0f);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (orn::styleNames[style], title, juce::Justification::centred, false);
        auto staff = panel.reduced (10.0f, 14.0f);
        const auto lineGap = staff.getHeight() / 8.0f;
        const auto midY = staff.getCentreY();
        g.setColour (parchment.withAlpha (lit ? 0.35f : 0.12f));
        for (int k = -2; k <= 2; ++k)
            g.drawHorizontalLine ((int) (midY + (float) k * lineGap), staff.getX(), staff.getRight());
        // The notes: the struck note first, then each step (MIXED: a trill, a mordent and a turn in turn).
        std::vector<int> steps { 0 };
        if (style == orn::mixed)
        {
            for (int s : { 0, 1, 0, 1, 0 })   steps.push_back (s);
            for (int s : { 1, 0 })            steps.push_back (s);
            for (int s : { 1, 0, -1, 0 })     steps.push_back (s);
        }
        else
        {
            const auto count = style == orn::trill ? trillSteps + 1 : style == orn::mordent ? 2 : 4;
            for (int k = 0; k < count; ++k)
                steps.push_back (orn::stepAt (style, k, trillSteps));
        }
        const auto dx = staff.getWidth() / (float) steps.size();
        for (size_t k = 0; k < steps.size(); ++k)
        {
            const auto x = staff.getX() + dx * ((float) k + 0.5f);
            const auto y = midY - (float) steps[k] * lineGap;
            const auto r = juce::jmin (dx * 0.32f, lineGap * 0.55f, 9.0f);
            g.setColour (k == 0 ? crimson.withAlpha (lit ? 0.95f : 0.35f) : (lit ? gold : parchment).withAlpha (lit ? 0.95f : 0.3f));
            g.fillEllipse (x - r * 1.2f, y - r, r * 2.4f, r * 2.0f);
            g.drawLine (x + r * 1.1f, y, x + r * 1.1f, y - juce::jmin (lineGap * 2.6f, r * 7.0f), 1.2f);
        }
    }

    // The flame: the newest ornamented note's move now (in semitones: up a step, the note, down a step).
    g.setColour (ink);
    g.fillRoundedRectangle (flame, 5.0f);
    g.setColour (crimson.withAlpha (0.5f));
    g.drawRoundedRectangle (flame, 5.0f, 1.0f);
    const auto centre = flame.getCentreY();
    const auto unit = flame.getHeight() * 0.16f;   // a semitone
    g.setColour (parchment.withAlpha (0.3f));
    g.drawHorizontalLine ((int) centre, flame.getX() + 8.0f, flame.getRight() - 8.0f);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("NOTE", flame.reduced (8.0f, 0.0f).withY (centre + 3.0f).withHeight (12.0f), juce::Justification::centredLeft, false);
    g.drawText ("UP", flame.reduced (8.0f, 0.0f).withY (centre - 2.0f * unit - 6.0f).withHeight (12.0f), juce::Justification::centredLeft, false);
    g.drawText ("DOWN", flame.reduced (8.0f, 0.0f).withY (centre + 2.0f * unit - 6.0f).withHeight (12.0f), juce::Justification::centredLeft, false);
    const auto move = juce::jlimit (-2.5f, 2.5f, (float) now / 100.0f);
    const auto tip = centre - move * unit;
    const auto wide = flame.getWidth() * 0.22f * (0.7f + 0.3f * flash);
    juce::Path p;
    p.startNewSubPath (flame.getCentreX(), tip - wide * 1.3f);
    p.quadraticTo (flame.getCentreX() + wide, tip, flame.getCentreX(), tip + wide * 0.9f);
    p.quadraticTo (flame.getCentreX() - wide, tip, flame.getCentreX(), tip - wide * 1.3f);
    g.setGradientFill (juce::ColourGradient (gold.withAlpha (live ? 1.0f : 0.25f), flame.getCentreX(), tip - wide,
                                             crimson.withAlpha (live ? 0.9f : 0.2f), flame.getCentreX(), tip + wide, false));
    g.fillPath (p);
    g.setColour (ParagonColours::text);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    const auto semis = juce::roundToInt ((float) now / 100.0f);
    g.drawText (semis != 0 ? (semis > 0 ? "+" : "") + juce::String (semis) + " SEMITONE" + (std::abs (semis) > 1 ? "S" : "") : juce::String ("ON THE NOTE"),
                flame.removeFromBottom (20.0f), juce::Justification::centred, false);
}

//==============================================================================
OrnamentPage::OrnamentPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::ornOn, "ON", "ORNAMENT on or off: trills, mordents and turns in key on the notes you play, in time with the song.");
    make (controls, ParamIDs::ornAmount, "AMOUNT", "How many notes are ornamented: none at 0 (the sound untouched), every one at the top. "
                                                   "The same passage at the same place in the song gets the same ornaments. "
                                                   "The mod matrix can move it: FX Ornament > Amount.");
    make (controls, ParamIDs::ornStyle, "STYLE", "TRILL: the note and the next note up, back and forth. MORDENT: a flick up and back. "
                                                 "TURN: above, the note, below, the note. MIXED: each note one of those.");
    make (controls, ParamIDs::ornSpeed, "SPEED", "How long each step of an ornament is, in time with the song.");
    make (controls, ParamIDs::ornLength, "LENGTH", "How long a trill goes on: half a beat to as long as the note is held.");
    make (controls, ParamIDs::ornKey, "KEY", "The key the ornaments move in.");
    make (controls, ParamIDs::ornScale, "SCALE", "The scale the ornaments move on (a step of it up or down).");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> OrnamentPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void OrnamentPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("ORNAMENT", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: trills, mordents and turns in key on the notes you play, in time with the song",
                juce::Rectangle<int> (24, 38, 480, 16), juce::Justification::centredLeft, false);
}

void OrnamentPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (500);
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
    SynthPanel::layoutRow (controlRow, row, 96);
}
