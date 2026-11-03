#include "BlackHolePage.h"

#include "dsp/effects/BlackHole.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

BlackHoleDisplay::BlackHoleDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The haze: each slice of sound circles the dark, set where it was taken from (further out, further back) "
                "and drawn inward as it plays. HOLD sets how long the haze hangs, PULL how it sinks.");
    startTimerHz (30);
}

void BlackHoleDisplay::timerCallback()
{
    level = juce::jmax (processor.getBlackHole().getHazeDb(), level - 1.0f);
    spin += 0.01f;
    repaint();
}

void BlackHoleDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff04030a));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getBlackHoleSettings();
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("HOLD " + juce::String (effects::BlackHole::holdSeconds (s.hold), 1) + " S   PULL "
                    + juce::String (effects::BlackHole::pullSemitones (s.pull), 2) + " ST   MIX " + juce::String (juce::roundToInt (100.0f * s.mix))
                    + "%   HAZE " + (level > -90.0f ? juce::String (level, 1) + " DB" : juce::String ("--")),
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);

    const auto centre = area.getCentre();
    const auto outer = juce::jmin (area.getWidth() * 0.5f, area.getHeight() * 0.9f);
    const auto core = outer * 0.3f;
    const auto glow = s.on ? juce::jlimit (0.15f, 1.0f, (level + 60.0f) / 50.0f) : 0.1f;

    // The ring of light round the dark.
    for (int k = 6; k >= 1; --k)
    {
        const auto r = core * (1.0f + 0.12f * (float) k);
        g.setColour (juce::Colour (0xffffa24a).withAlpha (glow * 0.07f * (float) (7 - k)));
        g.drawEllipse (centre.x - r, centre.y - r * 0.55f, 2.0f * r, 1.1f * r, 3.0f);
    }
    g.setColour (juce::Colours::black);
    g.fillEllipse (centre.x - core, centre.y - core, 2.0f * core, 2.0f * core);
    g.setColour (juce::Colour (0xffffd08a).withAlpha (glow));
    g.drawEllipse (centre.x - core, centre.y - core, 2.0f * core, 2.0f * core, 1.5f);

    // The slices: where each was taken from (further out = further back), drawn inward as it plays.
    const auto& fx = processor.getBlackHole();
    for (int i = 0; i < effects::BlackHole::grainsPerSide; ++i)
    {
        const auto progress = fx.getGrainProgress (i);
        if (progress < 0.0f || ! s.on)
            continue;
        const auto place = fx.getGrainPlace (i);
        const auto start = core * 1.3f + (outer - core * 1.3f) * place;
        const auto r = start - (start - core * 1.05f) * progress;
        const auto angle = juce::MathConstants<float>::twoPi * (place * 3.7f + 0.15f * (float) i) + spin * (1.0f + 2.0f * progress);
        const auto x = centre.x + r * std::cos (angle), y = centre.y + r * 0.55f * std::sin (angle);
        const auto bright = std::sin (juce::MathConstants<float>::pi * progress);   // as the slice's fade
        g.setColour (juce::Colour (0xff9fd8ff).withAlpha (0.25f * bright * glow));
        g.fillEllipse (x - 9.0f, y - 9.0f, 18.0f, 18.0f);
        g.setColour (juce::Colour (0xffeaf6ff).withAlpha (bright * glow));
        g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
    }
}

//==============================================================================
BlackHolePage::BlackHolePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::blackholeOn, "ON", "BLACKHOLE on or off: the last moments of your sound caught and stretched into a haze.");
    make (controls, ParamIDs::blackholeHold, "HOLD", "How long the haze hangs: from a brief shimmer to clouds lasting many seconds. The mod matrix can move it: FX Blackhole > Hold.");
    make (controls, ParamIDs::blackholeSize, "SIZE", "How far back the slices are taken from: just played, or seconds ago.");
    make (controls, ParamIDs::blackholePull, "PULL", "How far the haze sinks in pitch each time round, falling in. The mod matrix can move it.");
    make (controls, ParamIDs::blackholeTone, "TONE", "The haze darker or brighter. The mod matrix can move it.");
    make (controls, ParamIDs::blackholeWidth, "WIDTH", "Each side its own slices (wide), or both alike.");
    make (controls, ParamIDs::blackholeMix, "MIX", "How much haze under your sound (0: none at all). The mod matrix can move it.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> BlackHolePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void BlackHolePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("BLACKHOLE", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the last moments of your sound stretched into a haze, as if time slowed", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void BlackHolePage::resized()
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
