#include "SubmergePage.h"

#include "dsp/effects/Submerge.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

SubmergeDisplay::SubmergeDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("A ballroom window going under: the water rises up the glass with DEPTH and the light fades to the sea's. "
                "The bubbles are the ones rising in your sound; the glass sways as the pressure bends the pitch.");
    startTimerHz (30);
}

namespace
{
    juce::String stageOf (float depth)
    {
        return depth < 0.001f ? "DRY LAND" : depth < 0.45f ? "SHALLOWS" : depth < 0.999f ? "UNDER" : "SEA FLOOR";
    }
}

void SubmergeDisplay::paint (juce::Graphics& g)
{
    using S = effects::Submerge;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getSubmergeSettings();
    const auto& fx = processor.getSubmerge();
    const auto live = settings.on;
    const auto depth = live ? fx.getDepth() : 0.0f;
    const auto bend = live ? fx.getBendCents() : 0.0f;
    const auto sea = live ? fx.getSeaLevel() : 0.0f;

    // The ballroom: warm gold, fading to the sea's green-blue as it goes under.
    auto view = bounds.reduced (2.0f);
    const auto roomTop = juce::Colour (0xff3a2a14).interpolatedWith (juce::Colour (0xff04161c), depth);
    const auto roomLow = juce::Colour (0xff1c140b).interpolatedWith (juce::Colour (0xff010709), depth);
    g.setGradientFill (juce::ColourGradient (roomTop, view.getX(), view.getY(), roomLow, view.getX(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);

    // Deco rays fanning up behind the window, dimming as it sinks.
    const auto centre = juce::Point<float> (view.getCentreX(), view.getBottom());
    for (int k = 0; k < 13; ++k)
    {
        const auto angle = juce::MathConstants<float>::pi * (0.12f + 0.76f * (float) k / 12.0f);
        const auto end = centre.translated (-std::cos (angle) * view.getWidth(), -std::sin (angle) * view.getHeight() * 1.3f);
        g.setColour (juce::Colour (0xffd9b25a).withAlpha (0.10f * (1.0f - depth)));
        g.drawLine ({ centre, end }, 1.2f);
    }

    // The arched window, swaying a little with the pressure's bend.
    const auto sway = juce::jlimit (-1.0f, 1.0f, bend / S::maxCents) * 6.0f;
    auto glass = view.withSizeKeepingCentre (view.getHeight() * 1.1f, view.getHeight() * 0.86f).translated (sway, 6.0f);
    juce::Path arch;
    const auto archTop = glass.getY() + glass.getWidth() * 0.5f;
    arch.startNewSubPath (glass.getX(), glass.getBottom());
    arch.lineTo (glass.getX(), archTop);
    arch.addCentredArc (glass.getCentreX(), archTop, glass.getWidth() * 0.5f, glass.getWidth() * 0.5f, 0.0f,
                        -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
    arch.lineTo (glass.getRight(), glass.getBottom());
    arch.closeSubPath();
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (arch);
        // The night outside, then the water rising up the glass.
        g.setColour (juce::Colour (0xff0b1424));
        g.fillRect (glass);
        const auto waterline = glass.getBottom() - (glass.getHeight() + 4.0f) * depth;
        const auto water = juce::Rectangle<float> (glass.getX() - 10.0f, waterline, glass.getWidth() + 20.0f, glass.getBottom() - waterline + 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1f6a74).withAlpha (0.9f), 0.0f, waterline,
                                                 juce::Colour (0xff031316), 0.0f, glass.getBottom(), false));
        g.fillRect (water);
        // The sea's wash glowing in the depths.
        g.setColour (juce::Colour (0xff4fd0c8).withAlpha (0.35f * sea * depth));
        g.fillEllipse (glass.getCentreX() - glass.getWidth() * 0.4f, glass.getBottom() - glass.getHeight() * 0.25f,
                       glass.getWidth() * 0.8f, glass.getHeight() * 0.4f);
        // A wavy surface line.
        if (depth > 0.0f && depth < 1.0f)
        {
            juce::Path surface;
            const auto t = (float) (juce::Time::getMillisecondCounterHiRes() * 0.002);
            for (auto x = glass.getX(); x <= glass.getRight(); x += 4.0f)
            {
                const auto y = waterline + 2.0f * std::sin (0.08f * x + t);
                if (x == glass.getX()) surface.startNewSubPath (x, y); else surface.lineTo (x, y);
            }
            g.setColour (juce::Colour (0xffbfeff0).withAlpha (0.6f));
            g.strokePath (surface, juce::PathStrokeType (1.2f));
        }
        // The bubbles, rising from the bottom by their age, at their place across.
        for (int k = 0; k < S::maxBubbles; ++k)
        {
            const auto rise = live ? fx.getBubbleRise (k) : -1.0f;
            if (rise < 0.0f)
                continue;
            const auto x = glass.getCentreX() + fx.getBubblePan (k) * glass.getWidth() * 0.42f;
            const auto y = glass.getBottom() - 8.0f - rise * (glass.getBottom() - juce::jmax (glass.getY(), waterline) - 8.0f);
            const auto r = 2.0f + 4.0f * (1.0f - rise);
            g.setColour (juce::Colour (0xffd8fbff).withAlpha (0.85f * (1.0f - 0.6f * rise)));
            g.drawEllipse (x - r, y - r, 2.0f * r, 2.0f * r, 1.3f);
        }
    }
    // The window's brass frame and its deco glazing bars.
    const auto brass = juce::Colour (0xffc9a14a).interpolatedWith (juce::Colour (0xff4f6a5c), depth);
    g.setColour (brass);
    g.strokePath (arch, juce::PathStrokeType (3.0f));
    g.setColour (brass.withAlpha (0.7f));
    g.drawLine (glass.getCentreX(), glass.getY(), glass.getCentreX(), glass.getBottom(), 1.5f);
    for (int k = 1; k < 4; ++k)
    {
        const auto y = archTop + (glass.getBottom() - archTop) * (float) k / 4.0f;
        g.drawLine (glass.getX(), y, glass.getRight(), y, 1.2f);
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (live ? stageOf (depth) : "OFF", bar, juce::Justification::centredLeft, false);
    g.drawText ("PRESSURE " + juce::String (bend >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (bend)) + " CENTS", bar,
                juce::Justification::centred, false);
    g.drawText ("BUBBLES " + juce::String (S::bubbleRate (settings.bubbles, depth), 1) + " A SECOND", bar,
                juce::Justification::centredRight, false);
}

//==============================================================================
SubmergePage::SubmergePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::submergeOn, "ON", "SUBMERGE on or off: the sound sinking under water.");
    make (controls, ParamIDs::submergeDepth, "DEPTH", "From dry land (0: your sound untouched) to the sea floor: the top goes, "
                                                       "the pressure bends the pitch, bubbles rise and the sea washes round. "
                                                       "The mod matrix can move it: FX Submerge > Depth.");
    make (controls, ParamIDs::submergeMurk, "MURK", "How dark the water at the bottom: 4 kHz down to 400 Hz.");
    make (controls, ParamIDs::submergeCurrent, "CURRENT", "How far the pressure bends the pitch (unevenly, never in a "
                                                           "steady rhythm): up to about 20 cents either way.");
    make (controls, ParamIDs::submergeBubbles, "BUBBLES", "How many bubbles rise (each a little rising note). They only "
                                                           "rise while you play.");
    make (controls, ParamIDs::submergeSea, "SEA", "How much of the sea's dark wash surrounds the sound.");
    make (controls, ParamIDs::submergeWidth, "WIDTH", "How wide the bubbles and the sea.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> SubmergePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void SubmergePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("SUBMERGE", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the sound sinking under water", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void SubmergePage::resized()
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
