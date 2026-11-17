#include "GearworksPage.h"

#include "dsp/effects/Gearworks.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // A gear: `teeth` teeth round a hub, turned by `angle`; the tooth at the top (where they mesh) lit when `lit`.
    void drawGear (juce::Graphics& g, juce::Point<float> centre, float radius, int teeth, float angle, juce::Colour metal, float lit)
    {
        const auto toothDepth = radius * 0.18f;
        juce::Path gear;
        const auto points = teeth * 4;
        for (int k = 0; k <= points; ++k)
        {
            const auto a = angle + juce::MathConstants<float>::twoPi * (float) k / (float) points;
            const auto r = (k % 4 == 1 || k % 4 == 2) ? radius : radius - toothDepth;
            const auto p = centre + juce::Point<float> (std::sin (a) * r, -std::cos (a) * r);
            if (k == 0)
                gear.startNewSubPath (p);
            else
                gear.lineTo (p);
        }
        gear.closeSubPath();
        g.setGradientFill (juce::ColourGradient (metal.brighter (0.25f), centre.x, centre.y - radius,
                                                 metal.darker (0.6f), centre.x, centre.y + radius, false));
        g.fillPath (gear);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (gear, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colour (0xff111114));
        g.fillEllipse (juce::Rectangle<float> (radius * 0.5f, radius * 0.5f).withCentre (centre));
        g.setColour (metal.darker (0.2f));
        g.drawEllipse (juce::Rectangle<float> (radius * 0.5f, radius * 0.5f).withCentre (centre), 2.0f);
        if (lit > 0.01f)
        {
            g.setColour (juce::Colour (0xffffb347).withAlpha (juce::jlimit (0.0f, 1.0f, lit)));
            g.fillEllipse (juce::Rectangle<float> (toothDepth * 2.2f, toothDepth * 2.2f)
                               .withCentre (centre + juce::Point<float> (0.0f, -(radius - toothDepth * 0.4f))));
        }
    }
}

GearworksDisplay::GearworksDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("Two gears turning in time with the song: wherever a tooth comes round (lit) the sound is cut and the metal "
                "strikes. Along the bottom a whole turn of steps, the cut ones marked, the one now lit.");
    startTimerHz (30);
}

void GearworksDisplay::paint (juce::Graphics& g)
{
    using G = effects::Gearworks;
    const auto bounds = getLocalBounds().toFloat();
    const auto s = processor.getGearworksSettings();
    const auto& fx = processor.getGearworks();
    const auto live = s.on && ! fx.isResting();
    const auto amount = live ? fx.getAmount() : 0.0f;
    const auto steel = juce::Colour (0xff8a8f99), brass = juce::Colour (0xffb08d57), rust = juce::Colour (0xffc0552a);
    const auto& ratio = G::ratios[(size_t) s.ratio];

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1a18), view.getX(), view.getY(),
                                             juce::Colour (0xff0b0b0c), view.getX(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    // Rivets along the panel's edge.
    g.setColour (steel.withAlpha (0.25f));
    for (int k = 0; k < 24; ++k)
    {
        const auto x = view.getX() + 10.0f + (view.getWidth() - 20.0f) * (float) k / 23.0f;
        g.fillEllipse (x - 2.0f, view.getY() + 6.0f, 4.0f, 4.0f);
        g.fillEllipse (x - 2.0f, view.getBottom() - 10.0f, 4.0f, 4.0f);
    }
    auto inner = view.reduced (view.getWidth() * 0.04f, view.getHeight() * 0.08f).withTrimmedTop (view.getHeight() * 0.07f);

    // THE GEARS: each turns one tooth per `ratio` steps, so their teeth come round to the top on the cut steps.
    auto field = inner.removeFromTop (inner.getHeight() * 0.7f);
    const auto turn = G::turnSteps (s.ratio);
    const auto steps = live ? fx.getTurn() * (float) turn : 0.0f;
    const auto teeth = live ? fx.getTeeth() : 0;
    const auto clank = live ? juce::jlimit (0.0f, 1.0f, fx.getClank() * 6.0f) : 0.0f;
    const auto r1 = field.getHeight() * 0.42f;
    const auto r2 = r1 * std::sqrt ((float) ratio[1] / (float) ratio[0]);
    const auto c1 = juce::Point<float> (field.getCentreX() - r1 * 0.95f, field.getCentreY());
    const auto c2 = juce::Point<float> (c1.x + r1 + r2 - r1 * 0.12f, field.getCentreY());
    const auto teethOn1 = juce::jmax (6, ratio[0] * 3), teethOn2 = juce::jmax (6, ratio[1] * 3);
    // A gear with n teeth turns 1/n of a turn per tooth; here one tooth comes round every `ratio` steps.
    const auto angle1 = juce::MathConstants<float>::twoPi * (steps / (float) ratio[0]) / (float) teethOn1;
    const auto angle2 = -juce::MathConstants<float>::twoPi * (steps / (float) ratio[1]) / (float) teethOn2;
    drawGear (g, c1, r1, teethOn1, angle1, steel.interpolatedWith (rust, 0.3f * amount), (teeth & 1) != 0 ? 0.4f + 0.6f * clank : 0.0f);
    drawGear (g, c2, r2, teethOn2, angle2, brass.interpolatedWith (rust, 0.3f * amount), (teeth & 2) != 0 ? 0.4f + 0.6f * clank : 0.0f);
    // Sparks where the metal strikes.
    if (clank > 0.05f)
    {
        g.setColour (juce::Colour (0xffffd27a).withAlpha (clank));
        const auto at = juce::Point<float> ((c1.x + r1 + c2.x - r2) * 0.5f, field.getCentreY());
        for (int k = 0; k < 7; ++k)
        {
            const auto a = (float) k * 0.9f + steps;
            g.drawLine (at.x, at.y, at.x + std::cos (a) * r1 * 0.35f * clank, at.y + std::sin (a) * r1 * 0.35f * clank, 1.5f);
        }
    }
    // Each gear's tooth count (a tooth every that many steps) in its hub.
    g.setFont (juce::FontOptions (r1 * 0.32f, juce::Font::bold));
    g.setColour (steel.brighter (0.4f));
    g.drawText (juce::String (ratio[0]), juce::Rectangle<float> (r1 * 0.5f, r1 * 0.5f).withCentre (c1), juce::Justification::centred, false);
    g.setColour (brass.brighter (0.4f));
    g.drawText (juce::String (ratio[1]), juce::Rectangle<float> (r1 * 0.5f, r1 * 0.5f).withCentre (c2), juce::Justification::centred, false);

    // THE TURN: every step until the pattern repeats, the cut ones marked, the one now lit.
    inner.removeFromTop (inner.getHeight() * 0.15f);
    const auto cw = inner.getWidth() / (float) turn;
    const auto now = live ? fx.getStep() : -1;
    for (int k = 0; k < turn; ++k)
    {
        const auto t = G::teethAt (k, s.ratio);
        const auto cell = juce::Rectangle<float> (inner.getX() + cw * (float) k, inner.getY(), cw, inner.getHeight() * 0.6f).reduced (1.5f);
        const auto colour = t == 3 ? rust : t == 1 ? steel : t == 2 ? brass : juce::Colour (0xff2a2a2e);
        g.setColour (colour.withAlpha (k == now ? 1.0f : (s.on ? 0.55f : 0.25f)));
        g.fillRoundedRectangle (cell, 2.0f);
        if (k == now)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawRoundedRectangle (cell, 2.0f, 1.2f);
        }
    }
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (G::divisionNames[s.division] + " STEPS, A TURN EVERY " + juce::String (turn), inner.withTrimmedTop (inner.getHeight() * 0.62f),
                juce::Justification::centredLeft, false);

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    const auto cutShare = G::cutShare (s.ratio);
    g.drawText (! s.on ? juce::String ("OFF")
                       : "GEARS " + G::ratioNames[s.ratio] + ": " + juce::String (juce::roundToInt (cutShare * (float) turn)) + " OF "
                             + juce::String (turn) + " STEPS CUT, " + juce::String (fx.getClanks()) + " CLANKS",
                bar, juce::Justification::centredLeft, false);
    g.drawText ("AMOUNT " + juce::String (juce::roundToInt (100.0f * amount)) + "%", bar, juce::Justification::centredRight, false);
}

//==============================================================================
GearworksPage::GearworksPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::gearOn, "ON", "GEARWORKS on or off: the sound caught in the machinery, in time with the song.");
    make (controls, ParamIDs::gearAmount, "AMOUNT", "How far into the machinery: every part below grows with it (0: the sound "
                                                     "untouched). The mod matrix can move it: FX Gearworks > Amount.");
    make (controls, ParamIDs::gearRatio, "RATIO", "The two gears: a tooth every 3 and every 4 steps, 2 and 3, 3 and 5, or 5 and 7. "
                                                   "Together they grind out a pattern that repeats once a whole turn.");
    make (controls, ParamIDs::gearDivision, "DIVISION", "How long a step is: a sixteenth, an eighth or a sixteenth triplet, in time with the song.");
    make (controls, ParamIDs::gearBite, "BITE", "How deep a tooth cuts the sound (the loss made up from the pattern, so the level holds).");
    make (controls, ParamIDs::gearMetal, "METAL", "The sound rung as metal: ring modulation at 2.76 x the lowest note playing.");
    make (controls, ParamIDs::gearGrind, "GRIND", "The teeth on the signal: held samples and fewer bits.");
    make (controls, ParamIDs::gearSteam, "STEAM", "A hiss of steam on every cut, following how loud you play.");
    make (controls, ParamIDs::gearClank, "CLANK", "Each tooth strikes metal: a ringing clank, the two gears at two pitches.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> GearworksPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void GearworksPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("GEARWORKS", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the sound caught in the machinery, in time with the song", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void GearworksPage::resized()
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
    SynthPanel::layoutRow (controlRow, row, 92);
}
