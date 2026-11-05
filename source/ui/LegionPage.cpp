#include "LegionPage.h"

#include "dsp/effects/Legion.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

LegionDisplay::LegionDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The stage from above: your sound at the front, the players joining behind it as SIZE comes up, across "
                "the stage by SPREAD, further back the later they play (LOOSE), swaying as they drift in and out of tune (DETUNE).");
    startTimerHz (30);
}

void LegionDisplay::paint (juce::Graphics& g)
{
    using L = effects::Legion;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getLegionSettings();
    const auto& fx = processor.getLegion();
    const auto live = settings.on;
    g.setColour (juce::Colour (0xff07060a));
    g.fillRoundedRectangle (bounds, 6.0f);
    auto view = bounds.reduced (16.0f).withTrimmedTop (30.0f);

    // The stage: a shallow arc of floor, rows of chairs as faint arcs, the front edge lit.
    const auto front = juce::Point<float> (view.getCentreX(), view.getBottom() - 16.0f);
    const auto radiusX = view.getWidth() * 0.46f, depth = view.getHeight() * 0.78f;
    g.setColour (juce::Colour (0xff1a1420));
    for (int row = 1; row <= 4; ++row)
    {
        const auto t = (float) row / 4.0f;
        juce::Path arc;
        arc.addCentredArc (front.x, front.y, radiusX * (0.35f + 0.65f * t), depth * t, 0.0f,
                           -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (arc, juce::PathStrokeType (1.0f));
    }
    g.setColour (juce::Colour (0xff5a3a6a));
    g.drawLine (view.getX() + 20.0f, front.y + 10.0f, view.getRight() - 20.0f, front.y + 10.0f, 1.5f);

    // Where a player stands: across by its place, back by how late it plays (0-30 ms).
    const auto placeAt = [&] (float pan, float lateMs)
    {
        const auto t = juce::jlimit (0.15f, 1.0f, lateMs / 30.0f);
        return juce::Point<float> (front.x + pan * radiusX * (0.4f + 0.6f * t), front.y - depth * t);
    };
    const auto drawPlayer = [&g] (juce::Point<float> at, float lit, float size, juce::Colour colour)
    {
        g.setColour (colour.withAlpha (0.18f * lit));
        g.fillEllipse (juce::Rectangle<float> (size * 3.0f, size * 3.0f).withCentre (at));
        g.setColour (colour.withAlpha (0.25f + 0.75f * lit));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (at));
        g.setColour (colour.withAlpha (0.4f + 0.6f * lit).darker (0.4f));
        g.drawLine (at.x, at.y + size * 0.5f, at.x, at.y + size * 1.4f, size * 0.35f);   // a stand under each
    };

    auto playing = 0.0f;
    for (int k = 0; k < L::numPlayers; ++k)
    {
        const auto w = live ? fx.getPlayerWeight (k) : 0.0f;
        playing += w;
        // Each in its chair as the settings place it (SPREAD across, LOOSE back), playing or not.
        const auto pan = juce::jlimit (-1.0f, 1.0f, L::placeOf (k) * settings.spread);
        const auto late = 2.0f + (L::looseMs (settings.loose) - 2.0f) * L::latenessOf (k);
        auto at = placeAt (pan, late);
        at.x += 6.0f * w * (live ? fx.getPlayerDrift (k) : 0.0f);   // swaying as it drifts
        if (w <= 0.01f)
        {
            g.setColour (juce::Colour (0xff2a2232));
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (at), 1.0f);   // an empty chair
            continue;
        }
        drawPlayer (at, w, 11.0f, juce::Colour (0xffd9a8ff));
    }
    // The leader: your sound, always there, at the front.
    drawPlayer (front.translated (0.0f, -depth * 0.06f), 1.0f, 15.0f, juce::Colour (0xffffc46b));

    // The readings across the top.
    g.setColour (ParagonColours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    auto bar = bounds.reduced (14.0f, 8.0f).removeFromTop (22.0f);
    const auto players = live ? L::playersAt (fx.getSize()) : 1.0f;
    g.drawText ("PLAYERS " + juce::String (juce::roundToInt (players)), bar, juce::Justification::centredLeft, false);
    g.drawText ("SPREAD " + juce::String (juce::roundToInt (100.0f * settings.spread)) + "%   LOOSE "
                    + juce::String (L::looseMs (settings.loose), 0) + " ms",
                bar, juce::Justification::centred, false);
    g.drawText ("DRIFT +-" + juce::String (L::detuneCents (settings.detune), 1) + " ct", bar, juce::Justification::centredRight, false);
    juce::ignoreUnused (playing);
}

//==============================================================================
LegionPage::LegionPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::legionOn, "ON", "LEGION on or off: one player to a full section.");
    make (controls, ParamIDs::legionSize, "SIZE", "How many play: at 0 just your sound (untouched), all the way up a section of nine. "
                                                  "The mod matrix can move it: FX Legion > Size.");
    make (controls, ParamIDs::legionSpread, "SPREAD", "How wide the section stands, from all in the middle to across the whole stage. "
                                                      "The mod matrix can move it.");
    make (controls, ParamIDs::legionLoose, "LOOSE", "How far apart in time they play: the latest player 6 to 30 ms behind. "
                                                    "Tight for short notes, loose for big, slow ones.");
    make (controls, ParamIDs::legionDetune, "DETUNE", "How far the players drift in and out of tune: 0 to 14 cents, slowly, each its own way. "
                                                      "The mod matrix can move it.");
    make (controls, ParamIDs::legionTone, "TONE", "How bright the players are: 3 to 16 kHz (a section blurs the top a little).");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> LegionPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void LegionPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("LEGION", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: one player to a full section", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void LegionPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (460);
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
    SynthPanel::layoutRow (controlRow, row, 104);
}
