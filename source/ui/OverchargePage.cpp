#include "OverchargePage.h"

#include "dsp/effects/Overcharge.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

OverchargeDisplay::OverchargeDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The crystal: it glows as CHARGE fills its first half (the crystal tones and climbing repeats) and as you "
                "play; past halfway it cracks and throws arcs of energy, each flash a burst of the overload (a stutter, "
                "crush or jolt), more often and wilder with CHAOS.");
    startTimerHz (30);
}

void OverchargeDisplay::paint (juce::Graphics& g)
{
    using O = effects::Overcharge;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getOverchargeSettings();
    const auto& fx = processor.getOvercharge();
    const auto live = settings.on;
    const auto charge = live ? fx.getCharge() : 0.0f;
    const auto overload = live ? fx.getOverload() : 0.0f;
    const auto glow = live ? fx.getGlow() : 0.0f;
    const auto flash = live ? fx.getFlash() : 0.0f;
    g.setColour (juce::Colour (0xff05060c));
    g.fillRoundedRectangle (bounds, 6.0f);

    auto view = bounds.reduced (16.0f).withTrimmedTop (30.0f);
    const auto centre = view.getCentre();
    const auto size = juce::jmin (view.getWidth(), view.getHeight()) * 0.34f;
    const auto blue = juce::Colour (0xff6fd8ff), violet = juce::Colour (0xffc46bff), white = juce::Colour (0xffeaf8ff);

    // The halo and rays: brighter with the charge and the playing.
    const auto bright = juce::jlimit (0.0f, 1.0f, 0.15f + 0.5f * charge + 0.6f * glow);
    for (int ring = 4; ring >= 1; --ring)
    {
        const auto r = size * (0.9f + 0.45f * (float) ring);
        g.setColour (blue.withAlpha (0.05f * bright * (5 - ring)));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
    }
    g.setColour (blue.withAlpha (0.25f * bright));
    for (int k = 0; k < 12; ++k)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) k / 12.0f + 0.13f;
        const auto inner = size * 0.9f, outer = size * (1.25f + 0.55f * bright * (k % 2 == 0 ? 1.0f : 0.6f));
        g.drawLine (centre.x + std::cos (a) * inner, centre.y + std::sin (a) * inner,
                    centre.x + std::cos (a) * outer, centre.y + std::sin (a) * outer, 1.2f);
    }

    // The crystal: a long hexagonal shard, lit from inside.
    juce::Path shard;
    shard.startNewSubPath (centre.x, centre.y - size);
    shard.lineTo (centre.x + size * 0.42f, centre.y - size * 0.45f);
    shard.lineTo (centre.x + size * 0.42f, centre.y + size * 0.45f);
    shard.lineTo (centre.x, centre.y + size);
    shard.lineTo (centre.x - size * 0.42f, centre.y + size * 0.45f);
    shard.lineTo (centre.x - size * 0.42f, centre.y - size * 0.45f);
    shard.closeSubPath();
    const auto core = blue.interpolatedWith (violet, overload).interpolatedWith (white, flash * 0.6f);
    g.setGradientFill (juce::ColourGradient (core.withAlpha (0.35f + 0.6f * bright), centre.x, centre.y,
                                             juce::Colour (0xff10131f), centre.x + size, centre.y + size, true));
    g.fillPath (shard);
    g.setColour (core.withAlpha (0.5f + 0.5f * bright));
    g.strokePath (shard, juce::PathStrokeType (1.6f));
    g.setColour (white.withAlpha (0.15f + 0.3f * bright));
    g.drawLine (centre.x, centre.y - size, centre.x, centre.y + size, 1.0f);

    // Past halfway: cracks across it, and arcs jumping off it at each burst.
    if (overload > 0.0f)
    {
        juce::Random r (7);   // the same cracks every frame: more of them as the overload grows
        const auto cracks = 1 + (int) (overload * 7.0f);
        g.setColour (violet.withAlpha (0.4f + 0.5f * overload));
        for (int k = 0; k < cracks; ++k)
        {
            juce::Path crack;
            auto at = juce::Point<float> (centre.x + (r.nextFloat() - 0.5f) * size * 0.5f, centre.y + (r.nextFloat() - 0.5f) * size * 1.2f);
            crack.startNewSubPath (at);
            for (int step = 0; step < 4; ++step)
            {
                at += { (r.nextFloat() - 0.5f) * size * 0.35f, (r.nextFloat() - 0.5f) * size * 0.35f };
                crack.lineTo (at);
            }
            g.strokePath (crack, juce::PathStrokeType (1.0f + overload));
        }
        if (flash > 0.02f)
        {
            juce::Random arcs ((juce::int64) (fx.getBurstCount() * 31 + 1));   // a new pattern each burst
            g.setColour (white.withAlpha (flash));
            for (int k = 0; k < 3 + (int) (settings.chaos * 4.0f); ++k)
            {
                const auto a = arcs.nextFloat() * juce::MathConstants<float>::twoPi;
                auto at = juce::Point<float> (centre.x + std::cos (a) * size * 0.4f, centre.y + std::sin (a) * size * 0.8f);
                juce::Path arc;
                arc.startNewSubPath (at);
                for (int step = 0; step < 5; ++step)
                {
                    at += { std::cos (a) * size * 0.18f + (arcs.nextFloat() - 0.5f) * size * 0.2f,
                            std::sin (a) * size * 0.18f + (arcs.nextFloat() - 0.5f) * size * 0.2f };
                    arc.lineTo (at);
                }
                g.strokePath (arc, juce::PathStrokeType (1.5f));
            }
            static const char* const names[] { "", "STUTTER", "CRUSH", "JOLT" };
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (names[(int) fx.getBurst()], juce::Rectangle<float> (centre.x - 80.0f, centre.y + size * 1.35f, 160.0f, 18.0f),
                        juce::Justification::centred, false);
        }
    }

    // The readings across the top.
    g.setColour (ParagonColours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    auto bar = bounds.reduced (14.0f, 8.0f).removeFromTop (22.0f);
    const auto stage = ! live || settings.charge < 0.001f ? juce::String ("AT REST")
                     : overload > 0.0f ? "OVERLOAD " + juce::String (juce::roundToInt (100.0f * overload)) + "%"
                                       : "CHARGING " + juce::String (juce::roundToInt (100.0f * charge)) + "%";
    g.drawText (stage, bar, juce::Justification::centredLeft, false);
    g.drawText ("GLOW " + juce::String (juce::roundToInt (100.0f * settings.glow)) + "%   CHAOS "
                    + juce::String (juce::roundToInt (100.0f * settings.chaos)) + "%",
                bar, juce::Justification::centred, false);
    g.drawText (juce::String (O::burstRate (O::overloadOf (settings.charge), settings.chaos), 1) + " BURSTS / S", bar,
                juce::Justification::centredRight, false);
}

//==============================================================================
OverchargePage::OverchargePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::overchargeOn, "ON", "OVERCHARGE on or off: unstable magic, charging then overloading.");
    make (controls, ParamIDs::overchargeCharge, "CHARGE", "The first half charges the sound (crystal tones above the notes, "
                                                          "repeats climbing an octave each); the second overloads it in "
                                                          "bursts. At 0 your sound passes untouched. The mod matrix can move "
                                                          "it: FX Overcharge > Charge.");
    make (controls, ParamIDs::overchargeGlow, "GLOW", "How much of the crystal tones ring above your notes.");
    make (controls, ParamIDs::overchargeChaos, "CHAOS", "How wild the overload: how often it bursts and how hard (stutters, "
                                                        "crushed steps, pitch jolts, sparks).");
    make (controls, ParamIDs::overchargeTone, "TONE", "How bright the glow: 3 to 16 kHz.");
    make (controls, ParamIDs::overchargeWidth, "WIDTH", "How far the crystal tones spread across the stereo field.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> OverchargePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void OverchargePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("OVERCHARGE", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: unstable magic, charging then overloading", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void OverchargePage::resized()
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
