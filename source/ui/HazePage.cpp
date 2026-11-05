#include "HazePage.h"

#include "dsp/effects/Haze.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

HazeDisplay::HazeDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("Streetlights passing: you at the kerb, your sound the headlights sweeping along the road and back, in "
                "time with your song; nearer is louder and brighter, and the pitch leans up as it comes and down as it goes.");
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

void HazeDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    clock += juce::jlimit (0.0, 0.2, (now - lastTick) / 1000.0);
    lastTick = now;
    repaint();
}

void HazeDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff05040c));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getHazeSettings();
    const auto& fx = processor.getHaze();
    const auto lit = s.on ? 1.0f : 0.3f;
    const auto shift = s.on ? fx.getShiftCents() : 0.0f;
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("A PASS EVERY " + effects::Haze::divisionNames[juce::jlimit (0, 3, s.division)] + "   PITCH " + (shift >= 0.0f ? "+" : "")
                    + juce::String (shift, 1) + " CENTS   LEVEL " + juce::String (s.on ? fx.getLevelDb() : 0.0f, 1) + " DB   DEPTH "
                    + juce::String (juce::roundToInt (100.0f * s.depth)) + "%",
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (8.0f);

    // The road across the middle, its lane line, and streetlights along the far side.
    const auto road = area.withSizeKeepingCentre (area.getWidth(), area.getHeight() * 0.36f).translated (0.0f, -area.getHeight() * 0.08f);
    g.setColour (juce::Colour (0xff15131c));
    g.fillRect (road);
    g.setColour (juce::Colour (0xff2b2834));
    g.drawHorizontalLine ((int) road.getY(), road.getX(), road.getRight());
    g.drawHorizontalLine ((int) road.getBottom(), road.getX(), road.getRight());
    const auto scroll = (float) std::fmod (clock * 60.0, 60.0);
    g.setColour (juce::Colour (0xffd8c060).withAlpha (0.35f * lit));
    for (auto x = road.getX() - scroll; x < road.getRight(); x += 60.0f)
        g.fillRect (juce::jmax (road.getX(), x), road.getCentreY() - 1.5f, juce::jmin (28.0f, road.getRight() - x), 3.0f);
    for (int k = 0; k < 9; ++k)
    {
        const auto x = road.getX() + road.getWidth() * ((float) k + 0.5f) / 9.0f;
        g.setColour (juce::Colour (0xffffb45a).withAlpha (0.12f * lit));
        g.fillEllipse (x - 26.0f, road.getY() - 26.0f, 52.0f, 52.0f);
        g.setColour (juce::Colour (0xffffd08a).withAlpha (0.8f * lit));
        g.fillEllipse (x - 3.0f, road.getY() - 10.0f, 6.0f, 6.0f);
        g.setColour (juce::Colour (0xff3a3540));
        g.drawLine (x, road.getY() - 7.0f, x, road.getY() - 26.0f, 1.5f);
    }

    // You, at the kerb below the middle of the road.
    const auto you = juce::Point<float> (road.getCentreX(), road.getBottom() + area.getHeight() * 0.22f);
    g.setColour (juce::Colours::white.withAlpha (0.85f * lit));
    g.fillEllipse (you.x - 7.0f, you.y - 7.0f, 14.0f, 14.0f);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("YOU", juce::Rectangle<float> (you.x - 40.0f, you.y + 10.0f, 80.0f, 16.0f), juce::Justification::centred, false);

    // The sound: headlights along the road, glowing as they near; the line to you shows how near.
    const auto position = s.on ? fx.getPosition() : (float) -effects::Haze::roadLength;
    const auto x = road.getCentreX() + (float) (position / effects::Haze::roadLength) * road.getWidth() * 0.46f;
    const auto lights = juce::Point<float> (x, road.getCentreY() + road.getHeight() * 0.18f);
    const auto near = (float) (effects::Haze::closest / std::sqrt ((double) position * position + effects::Haze::closest * effects::Haze::closest));
    g.setColour (juce::Colour (0xff8fd0ff).withAlpha (0.25f * near * lit));
    g.drawLine (juce::Line<float> (lights, you), 1.5f);
    for (int r = 5; r >= 1; --r)
    {
        const auto radius = 10.0f + 16.0f * (float) r * (0.4f + near);
        g.setColour (juce::Colour (0xffeaf6ff).withAlpha (0.06f * near * lit * (float) (6 - r)));
        g.fillEllipse (lights.x - radius, lights.y - radius * 0.5f, 2.0f * radius, radius);
    }
    g.setColour (juce::Colours::white.withAlpha (lit));
    g.fillEllipse (lights.x - 12.0f, lights.y - 4.0f, 8.0f, 8.0f);
    g.fillEllipse (lights.x + 4.0f, lights.y - 4.0f, 8.0f, 8.0f);

    g.setColour (juce::Colour (0xff8fd0ff).withAlpha (lit));
    g.drawText (shift > 0.5f ? "COMING: PITCH UP" : shift < -0.5f ? "GOING: PITCH DOWN" : "PASSING",
                juce::Rectangle<float> (area.getX(), area.getBottom() - 16.0f, 300.0f, 16.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
HazePage::HazePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::hazeOn, "ON", "HAZE on or off: your sound sweeping past like streetlights, in time with your song.");
    make (controls, ParamIDs::hazeDivision, "PASS", "How often it passes: every half bar, bar, two bars or four (left to right, then back).");
    make (controls, ParamIDs::hazeDepth, "DEPTH", "How much it moves: nearer louder and brighter, further quieter and darker, leaning to its side (0: your sound untouched). The mod matrix can move it: FX Haze > Depth.");
    make (controls, ParamIDs::hazeDoppler, "DOPPLER", "How far the pitch leans as it passes: up as it comes, down as it goes (up to 40 cents at the steepest). The mod matrix can move it.");
    make (controls, ParamIDs::hazeWidth, "WIDTH", "How far it leans to each side as it passes.");
    make (controls, ParamIDs::hazeTone, "TONE", "How dark it gets far away: very dark, or keeping its shine.");
    make (controls, ParamIDs::hazeWhoosh, "WHOOSH", "A breath of wind as it passes close (only while you play). The mod matrix can move it.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> HazePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void HazePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("HAZE", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: streetlights passing, your sound sweeping by in time with your song", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void HazePage::resized()
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
