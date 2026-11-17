#include "CanyonPage.h"

#include "dsp/effects/Canyon.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

CanyonDisplay::CanyonDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The canyon: the sound at the bottom, the near walls either side and the far wall at the end; each wall "
                "lights up as its echo comes back.");
    startTimerHz (30);
}

namespace
{
    juce::String stageOf (float amount)
    {
        return amount < 0.001f ? "OPEN PLAIN" : amount < 0.45f ? "MESA" : amount < 0.8f ? "CANYON" : "GORGE";
    }

    // A rock wall: a jagged column from the bottom of `view` up to `top`, its face towards the middle, lit by `level`.
    void drawWall (juce::Graphics& g, juce::Rectangle<float> view, float edgeX, float faceX, float topY, float level, int seed)
    {
        juce::Path wall;
        wall.startNewSubPath (edgeX, view.getBottom());
        wall.lineTo (edgeX, topY);
        const auto steps = 14;
        juce::Random random (seed);
        for (int k = 0; k <= steps; ++k)
        {
            const auto t = (float) k / (float) steps;
            const auto y = topY + (view.getBottom() - topY) * t;
            const auto jag = (random.nextFloat() - 0.5f) * view.getWidth() * 0.025f;
            const auto x = faceX + (faceX - edgeX) * 0.35f * t + jag;   // wider at the bottom
            wall.lineTo (x, y);
        }
        wall.lineTo (edgeX, view.getBottom());
        wall.closeSubPath();
        const auto rock = juce::Colour (0xff7a3420).interpolatedWith (juce::Colour (0xffe07a3a), juce::jlimit (0.0f, 1.0f, level));
        g.setGradientFill (juce::ColourGradient (rock, faceX, topY, juce::Colour (0xff2a120c), edgeX, view.getBottom(), false));
        g.fillPath (wall);
        // The strata.
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (int s = 1; s < 7; ++s)
        {
            const auto y = topY + (view.getBottom() - topY) * (float) s / 7.0f;
            g.drawLine (edgeX, y, faceX + (faceX - edgeX) * 0.35f * (float) s / 7.0f, y + 3.0f, 1.0f);
        }
    }
}

void CanyonDisplay::paint (juce::Graphics& g)
{
    using C = effects::Canyon;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getCanyonSettings();
    const auto& fx = processor.getCanyon();
    const auto live = settings.on;
    const auto amount = live ? fx.getAmount() : 0.0f;

    auto view = bounds.reduced (2.0f);
    // The sky at sundown, over the far wall.
    const auto horizon = view.getY() + view.getHeight() * 0.42f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1a33), view.getX(), view.getY(),
                                             juce::Colour (0xffd9773a), view.getX(), horizon, false));
    g.fillRoundedRectangle (view, 6.0f);
    // The canyon floor.
    g.setColour (juce::Colour (0xff3a1d12));
    g.fillRect (view.withTop (horizon));

    // The far wall at the end, lit by its reply.
    const auto far = live ? fx.getWallLevel (C::numNear) : 0.0f;
    juce::Path farWall;
    farWall.startNewSubPath (view.getX(), horizon + 6.0f);
    for (int k = 0; k <= 20; ++k)
    {
        const auto x = view.getX() + view.getWidth() * (float) k / 20.0f;
        farWall.lineTo (x, horizon - view.getHeight() * (0.10f + 0.04f * std::sin ((float) k * 1.3f)));
    }
    farWall.lineTo (view.getRight(), horizon + 6.0f);
    farWall.closeSubPath();
    g.setColour (juce::Colour (0xff6a2c1a).interpolatedWith (juce::Colour (0xfff09050), juce::jlimit (0.0f, 1.0f, far)));
    g.fillPath (farWall);

    // How deep the canyon is: the walls close in and rise with AMOUNT.
    const auto spread = view.getWidth() * (0.40f - 0.18f * amount);
    const auto wallTop = horizon - view.getHeight() * (0.05f + 0.30f * amount);
    const auto centreX = view.getCentreX();
    // Near walls: 0 the left, 1 the right, 2 the left again (further down the canyon).
    const auto w0 = live ? fx.getWallLevel (0) : 0.0f, w1 = live ? fx.getWallLevel (1) : 0.0f, w2 = live ? fx.getWallLevel (2) : 0.0f;
    drawWall (g, view, view.getX(), centreX - spread, wallTop, juce::jmax (w0, w2 * 0.8f), 7);
    drawWall (g, view, view.getRight(), centreX + spread, wallTop, w1, 11);

    // The sound at the bottom, and its echoes going out to the walls.
    const auto source = juce::Point<float> (centreX, view.getBottom() - view.getHeight() * 0.1f);
    g.setColour (juce::Colour (0xfff5e3c0));
    g.fillEllipse (source.x - 5.0f, source.y - 5.0f, 10.0f, 10.0f);
    const std::array<float, 3> levels { w0, w1, w2 };
    for (int k = 0; k < 3; ++k)
        if (levels[(size_t) k] > 0.02f)
        {
            const auto radius = view.getHeight() * (0.15f + 0.12f * (float) k);
            g.setColour (juce::Colour (0xfff5e3c0).withAlpha (juce::jlimit (0.0f, 0.6f, levels[(size_t) k])));
            g.drawEllipse (source.x - radius * 1.6f, source.y - radius, radius * 3.2f, radius * 2.0f, 1.5f);
        }
    if (far > 0.02f)
    {
        g.setColour (juce::Colour (0xfff09050).withAlpha (juce::jlimit (0.0f, 0.5f, far * 0.7f)));
        g.drawLine (source.x, source.y, centreX, horizon, 1.5f + 3.0f * far);
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (live ? stageOf (amount) : "OFF", bar, juce::Justification::centredLeft, false);
    g.drawText ("NEAR " + juce::String (juce::roundToInt (1000.0f * C::nearSeconds (settings.size))) + " MS", bar,
                juce::Justification::centred, false);
    const auto farSeconds = live && ! fx.isResting() ? fx.getFarSecondsShown() : 0.0f;
    g.drawText ("FAR " + C::farNames[settings.far] + (farSeconds > 0.0f ? "  " + juce::String (farSeconds, 2) + " S" : juce::String()),
                bar, juce::Justification::centredRight, false);
}

//==============================================================================
CanyonPage::CanyonPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::canyonOn, "ON", "CANYON on or off: the sound's echoes off rock walls.");
    make (controls, ParamIDs::canyonAmount, "AMOUNT", "How much of the sound goes into the canyon (0: none, the sound "
                                                       "untouched). Turned down, the echoes already out there still come "
                                                       "back. The mod matrix can move it: FX Canyon > Amount.");
    make (controls, ParamIDs::canyonSize, "SIZE", "How far the near walls are: the first slapback 45 to 200 ms after the "
                                                   "sound, two more further off.");
    make (controls, ParamIDs::canyonFar, "FAR", "The far wall's reply, in time with the song: a quarter, a half, a bar "
                                                 "or two bars after the sound.");
    make (controls, ParamIDs::canyonRepeats, "REPEATS", "How many times the far wall's reply comes back, bouncing from "
                                                         "side to side and darker each time.");
    make (controls, ParamIDs::canyonTone, "TONE", "Dark to bright: how much top the rock gives back.");
    make (controls, ParamIDs::canyonWidth, "WIDTH", "How far to the sides the walls are.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> CanyonPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void CanyonPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("CANYON", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the sound's echoes off rock walls", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void CanyonPage::resized()
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
