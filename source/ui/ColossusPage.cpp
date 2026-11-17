#include "ColossusPage.h"

#include "dsp/effects/Colossus.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

ColossusDisplay::ColossusDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The sound made enormous: the shape rises over the dunes as SIZE grows, glowing with the octave under it; "
                "the rings are the hall, and the ground jumps with each boom.");
    startTimerHz (30);
}

namespace
{
    juce::String stageOf (float size)
    {
        return size < 0.001f ? "AS IT IS" : size < 0.5f ? "GIANT" : size < 0.999f ? "TITAN" : "COLOSSUS";
    }
}

void ColossusDisplay::paint (juce::Graphics& g)
{
    using C = effects::Colossus;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getColossusSettings();
    const auto& fx = processor.getColossus();
    const auto live = settings.on;
    const auto size = live ? fx.getSize() : 0.0f;
    const auto under = live ? fx.getUnderLevel() : 0.0f;
    const auto hall = live ? fx.getHallLevel() : 0.0f;
    const auto boom = live ? fx.getBoomLevel() : 0.0f;

    // The sky at dusk: deep blue over a burnt orange horizon.
    auto view = bounds.reduced (2.0f);
    const auto horizon = view.getY() + view.getHeight() * 0.68f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141826), view.getX(), view.getY(),
                                             juce::Colour (0xff7a3a1c), view.getX(), horizon, false));
    g.fillRoundedRectangle (view, 6.0f);

    // The hall's rings, spreading from behind the shape.
    const auto centre = juce::Point<float> (view.getCentreX(), horizon);
    for (int r = 1; r <= 5; ++r)
    {
        const auto radius = view.getHeight() * (0.12f + 0.16f * (float) r) * (0.6f + 0.6f * size);
        g.setColour (juce::Colour (0xffe8b56a).withAlpha (juce::jlimit (0.0f, 0.4f, hall * 0.45f / (float) r)));
        g.drawEllipse (centre.x - radius * 1.6f, centre.y - radius, radius * 3.2f, radius * 2.0f, 1.5f);
    }

    // The shape: a vast dark mass rising over the horizon, its glow the octave under it.
    const auto riseH = view.getHeight() * (0.08f + 0.55f * size);
    const auto riseW = view.getWidth() * (0.10f + 0.30f * size);
    juce::Path mass;
    mass.startNewSubPath (centre.x - riseW * 0.5f, horizon + 2.0f);
    mass.cubicTo (centre.x - riseW * 0.45f, horizon - riseH * 0.7f, centre.x - riseW * 0.2f, horizon - riseH,
                  centre.x, horizon - riseH);
    mass.cubicTo (centre.x + riseW * 0.2f, horizon - riseH, centre.x + riseW * 0.45f, horizon - riseH * 0.7f,
                  centre.x + riseW * 0.5f, horizon + 2.0f);
    mass.closeSubPath();
    if (under > 0.01f)
    {
        g.setColour (juce::Colour (0xffd4642a).withAlpha (juce::jlimit (0.0f, 0.5f, under * 0.6f)));
        g.strokePath (mass, juce::PathStrokeType (6.0f + 10.0f * under));
    }
    g.setColour (juce::Colour (0xff0d0c0b));
    g.fillPath (mass);

    // The dunes, jumping with each boom.
    const auto shake = boom * 4.0f;
    for (int d = 0; d < 3; ++d)
    {
        juce::Path dune;
        const auto top = horizon + (float) d * view.getHeight() * 0.07f - shake * (d == 0 ? 1.0f : 0.5f);
        dune.startNewSubPath (view.getX(), view.getBottom());
        dune.lineTo (view.getX(), top + 10.0f);
        for (int k = 0; k <= 24; ++k)
        {
            const auto x = view.getX() + view.getWidth() * (float) k / 24.0f;
            dune.lineTo (x, top + 8.0f * std::sin ((float) k * 0.7f + (float) d * 1.9f));
        }
        dune.lineTo (view.getRight(), view.getBottom());
        dune.closeSubPath();
        g.setColour (juce::Colour (0xffb7834a).darker (0.4f + 0.35f * (float) d));
        g.fillPath (dune);
    }
    if (boom > 0.02f)   // the dust thrown up
    {
        g.setColour (juce::Colour (0xffd9b07a).withAlpha (juce::jlimit (0.0f, 0.5f, boom * 0.6f)));
        g.fillEllipse (centre.x - riseW, horizon - 10.0f - 20.0f * boom, riseW * 2.0f, 24.0f + 30.0f * boom);
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (live ? stageOf (size) : "OFF", bar, juce::Justification::centredLeft, false);
    g.drawText ("HALL " + juce::String (C::hallSeconds (settings.hall), 1) + " S", bar, juce::Justification::centred, false);
    g.drawText ("BOOMS " + juce::String (live ? fx.getBoomsStarted() : 0), bar, juce::Justification::centredRight, false);
}

//==============================================================================
ColossusPage::ColossusPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::colossusOn, "ON", "COLOSSUS on or off: the sound made enormous.");
    make (controls, ParamIDs::colossusSize, "SIZE", "From the sound as it is (0: untouched) to colossal: an octave under "
                                                     "it, driven heavier, a vast hall behind and the ground shaking under "
                                                     "each note. The mod matrix can move it: FX Colossus > Size.");
    make (controls, ParamIDs::colossusDepth, "DEPTH", "How loud the octave under the sound is.");
    make (controls, ParamIDs::colossusWeight, "WEIGHT", "How hard the sound and its octave are driven: heavier, denser.");
    make (controls, ParamIDs::colossusHall, "HALL", "How long the vast hall rings: 2 to 10 seconds.");
    make (controls, ParamIDs::colossusBoom, "BOOM", "How deep the impact under each new note (a falling low boom and a "
                                                     "thump). It only sounds while you play.");
    make (controls, ParamIDs::colossusTone, "TONE", "Dark to bright: the octave's top and the hall's.");
    make (controls, ParamIDs::colossusWidth, "WIDTH", "How wide the hall spreads.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> ColossusPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void ColossusPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("COLOSSUS", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the sound made enormous", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void ColossusPage::resized()
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
