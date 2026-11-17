#include "AftermathPage.h"

#include "dsp/effects/Aftermath.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

AftermathDisplay::AftermathDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("Under the sky, or sealed in: as SEAL rises the walls close in and the ceiling comes down over the stars. "
                "The rings inside are the room answering your sound; the vent glows with the machinery's hum.");
    startTimerHz (30);
}

namespace
{
    juce::String stageOf (float seal)
    {
        return seal < 0.001f ? "OPEN SKY" : seal < 0.5f ? "SHELTER" : seal < 0.999f ? "SEALING IN" : "SEALED";
    }
}

void AftermathDisplay::paint (juce::Graphics& g)
{
    using A = effects::Aftermath;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getAftermathSettings();
    const auto& fx = processor.getAftermath();
    const auto live = settings.on;
    const auto seal = live ? fx.getSeal() : 0.0f;
    const auto room = live ? fx.getRoomLevel() : 0.0f;
    const auto hum = live ? fx.getHumLevel() : 0.0f;

    // The sky, darkening to concrete grey as it's sealed in.
    auto view = bounds.reduced (2.0f);
    const auto skyTop = juce::Colour (0xff0b1a3a).interpolatedWith (juce::Colour (0xff1b1c1f), seal);
    const auto skyLow = juce::Colour (0xff35507a).interpolatedWith (juce::Colour (0xff2a2b2e), seal);
    g.setGradientFill (juce::ColourGradient (skyTop, view.getX(), view.getY(), skyLow, view.getX(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);

    // Stars (fixed places), hidden as the ceiling comes over them.
    juce::Random stars (7);
    const auto ceiling = view.getY() + view.getHeight() * 0.55f * seal;
    for (int k = 0; k < 70; ++k)
    {
        const auto x = view.getX() + stars.nextFloat() * view.getWidth();
        const auto y = view.getY() + stars.nextFloat() * view.getHeight() * 0.6f;
        const auto r = 0.6f + stars.nextFloat() * 1.2f;
        if (y < ceiling)
            continue;
        g.setColour (juce::Colours::white.withAlpha ((0.25f + 0.5f * stars.nextFloat()) * (1.0f - seal)));
        g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
    }

    // The ground.
    const auto horizon = view.getY() + view.getHeight() * 0.72f;
    g.setColour (juce::Colour (0xff101214));
    g.fillRect (juce::Rectangle<float> (view.getX(), horizon, view.getWidth(), view.getBottom() - horizon));

    // The concrete closing in: walls from the sides, the ceiling from the top.
    const auto concrete = juce::Colour (0xff4a4c50), seam = juce::Colour (0xff2c2e31);
    const auto wall = view.getWidth() * 0.3f * seal;
    for (const auto& slab : { juce::Rectangle<float> (view.getX(), view.getY(), wall, view.getHeight()),
                              juce::Rectangle<float> (view.getRight() - wall, view.getY(), wall, view.getHeight()),
                              juce::Rectangle<float> (view.getX(), view.getY(), view.getWidth(), ceiling - view.getY()) })
    {
        if (slab.isEmpty())
            continue;
        g.setColour (concrete.withAlpha (0.92f));
        g.fillRect (slab);
        g.setColour (seam);
        for (auto y = slab.getY() + 18.0f; y < slab.getBottom(); y += 26.0f)
            g.drawHorizontalLine ((int) y, slab.getX(), slab.getRight());
    }

    // The room ringing: rectangles echoing out from the middle to the walls, by the room's level.
    const auto inside = juce::Rectangle<float> (view.getX() + wall, ceiling, view.getWidth() - 2.0f * wall, horizon - ceiling);
    if (seal > 0.0f && ! inside.isEmpty())
    {
        const auto ringing = juce::jlimit (0.0f, 1.0f, room);
        const auto phase = (float) std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.0012, 1.0);
        for (int k = 0; k < 4; ++k)
        {
            const auto t = std::fmod (phase + 0.25f * (float) k, 1.0f);
            g.setColour (juce::Colour (0xffd8c9a8).withAlpha (ringing * (1.0f - t) * 0.6f * seal));
            g.drawRect (inside.reduced (inside.getWidth() * 0.5f * (1.0f - t) * 0.9f, inside.getHeight() * 0.5f * (1.0f - t) * 0.9f), 1.4f);
        }
    }

    // The vent, glowing with the hum.
    if (seal > 0.05f)
    {
        const auto vent = juce::Rectangle<float> (view.getRight() - wall + wall * 0.2f, view.getY() + 26.0f, wall * 0.6f, 22.0f);
        if (vent.getWidth() > 8.0f)
        {
            g.setColour (juce::Colour (0xff1a1b1d));
            g.fillRect (vent);
            g.setColour (juce::Colour (0xffffb04a).withAlpha (0.15f + 0.75f * hum));
            for (auto x = vent.getX() + 3.0f; x < vent.getRight() - 2.0f; x += 5.0f)
                g.drawVerticalLine ((int) x, vent.getY() + 3.0f, vent.getBottom() - 3.0f);
        }
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (live ? stageOf (seal) : "OFF", bar, juce::Justification::centredLeft, false);
    g.drawText ("ROOM RINGS " + juce::String (A::ringSeconds (settings.room, settings.ring), 2) + " S", bar, juce::Justification::centred, false);
    g.drawText (settings.hum > 0.0f ? "HUM " + A::humNames[settings.humHz] + " " + juce::String (juce::roundToInt (A::humDb (settings.hum))) + " DB"
                                    : juce::String ("NO HUM"),
                bar, juce::Justification::centredRight, false);
}

//==============================================================================
AftermathPage::AftermathPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::aftermathOn, "ON", "AFTERMATH on or off: from under an open sky into a sealed concrete room.");
    make (controls, ParamIDs::aftermathSeal, "SEAL", "From open sky (0: your sound untouched) to sealed in: the room closes "
                                                     "in, the walls ring, the top darkens and the machinery hums. The mod "
                                                     "matrix can move it: FX Aftermath > Seal.");
    make (controls, ParamIDs::aftermathRoom, "ROOM", "How big the bunker: a cupboard to a hall of concrete.");
    make (controls, ParamIDs::aftermathRing, "RING", "How long the walls ring after the sound.");
    make (controls, ParamIDs::aftermathDark, "DARK", "How dark the sound gets when sealed in: 6 kHz down to 1.2 kHz.");
    make (controls, ParamIDs::aftermathHum, "HUM", "How loud the machinery (a mains hum and a ventilation shaft's air) when "
                                                   "sealed in. It only sounds while you play.");
    make (controls, ParamIDs::aftermathHumHz, "HUM HZ", "The mains hum: 50 or 60 Hz.");
    make (controls, ParamIDs::aftermathWidth, "WIDTH", "How wide the room.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> AftermathPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void AftermathPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("AFTERMATH", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: from under an open sky into a sealed concrete room", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void AftermathPage::resized()
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
