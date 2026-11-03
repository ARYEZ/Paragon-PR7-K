#include "RemnantPage.h"

#include "dsp/effects/Remnant.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

RemnantDisplay::RemnantDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("Dreams within dreams: your sound is the bright ring outside; each layer is a ring further in, turning "
                "slower the deeper it goes, lit by how loud it is now. The sweep round each ring is that layer renewing "
                "itself from the present.");
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

void RemnantDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    clock += juce::jlimit (0.0, 0.2, (now - lastTick) / 1000.0);
    lastTick = now;
    repaint();
}

void RemnantDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff05040a));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getRemnantSettings();
    const auto& fx = processor.getRemnant();
    const auto layers = juce::jlimit (1, effects::Remnant::maxLayers, s.layers + 1);
    const auto speed = effects::Remnant::speedFor (s.step);
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    juce::String text = juce::String (layers) + (layers == 1 ? " LAYER" : " LAYERS") + "   EACH "
                        + effects::Remnant::stepNames[juce::jlimit (0, 2, s.step)] + " DOWN ("
                        + juce::String (effects::Remnant::semitonesFor (s.step), 1) + " ST, x" + juce::String (speed, 2) + " SPEED)   LENGTH "
                        + juce::String (effects::Remnant::lengthSeconds (s.length), 2) + " S   MIX " + juce::String (juce::roundToInt (100.0f * s.mix)) + "%";
    g.drawText (text, area.removeFromTop (24.0f), juce::Justification::centredLeft, false);

    const auto centre = area.getCentre();
    const auto outer = juce::jmin (area.getWidth() * 0.5f, area.getHeight() * 0.5f) - 4.0f;
    const auto lit = s.on ? 1.0f : 0.3f;
    const auto ringColour = [] (int k) { return k == 0 ? juce::Colour (0xffffc070) : juce::Colour (0xff8a7dff).interpolatedWith (juce::Colour (0xff4ad0ff), (float) (k - 1) / 2.0f); };

    // Rings from outside in: 0 your sound, 1-3 the layers.
    for (int k = 0; k <= effects::Remnant::maxLayers; ++k)
    {
        const auto r = outer * (1.0f - 0.22f * (float) k);
        const auto inUse = k == 0 || k <= layers;
        const auto level = k == 0 ? 0.0f : fx.getLayerDb (k - 1);
        const auto glow = k == 0 ? 0.8f : (s.on && inUse ? juce::jlimit (0.12f, 1.0f, (level + 60.0f) / 45.0f) : 0.06f);
        const auto colour = ringColour (k);
        g.setColour (colour.withAlpha (glow * 0.25f * lit));
        g.drawEllipse (centre.x - r, centre.y - r, 2.0f * r, 2.0f * r, outer * 0.09f);
        g.setColour (colour.withAlpha (glow * lit));
        g.drawEllipse (centre.x - r, centre.y - r, 2.0f * r, 2.0f * r, 1.5f);
        if (! inUse)
            continue;
        // A marker turning at this level's speed (each deeper one STEP slower), with a trail.
        const auto turn = (float) (juce::MathConstants<double>::twoPi * std::fmod (clock * 0.25 * std::pow ((double) speed, (double) k), 1.0));
        for (int t = 6; t >= 0; --t)
        {
            const auto a = turn - 0.09f * (float) t;
            const auto x = centre.x + r * std::sin (a), y = centre.y - r * std::cos (a);
            const auto size = (t == 0 ? 9.0f : 6.0f) * (1.0f - 0.1f * (float) t);
            g.setColour (colour.withAlpha (lit * glow * (t == 0 ? 1.0f : 0.35f * (1.0f - (float) t / 7.0f))));
            g.fillEllipse (x - size * 0.5f, y - size * 0.5f, size, size);
        }
        if (k > 0)
        {
            // The layer renewing: its head's way through LENGTH, swept round the ring.
            juce::Path sweep;
            const auto phase = fx.getLayerPhase (k - 1);
            sweep.addCentredArc (centre.x, centre.y, r - outer * 0.06f, r - outer * 0.06f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * phase, true);
            g.setColour (colour.withAlpha (0.45f * lit * glow));
            g.strokePath (sweep, juce::PathStrokeType (2.0f));
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText ("LAYER " + juce::String (k) + "   " + juce::String (juce::roundToInt (effects::Remnant::semitonesFor (s.step) * (float) k)) + " ST"
                            + (s.on && level > -90.0f ? "   " + juce::String (juce::roundToInt (level)) + " DB" : juce::String()),
                        juce::Rectangle<float> (centre.x + outer + 16.0f, centre.y - 26.0f + 20.0f * (float) (k - 1), 220.0f, 16.0f),
                        juce::Justification::centredLeft, false);
        }
    }
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (ringColour (0).withAlpha (0.9f * lit));
    g.drawText ("OUTSIDE: YOUR SOUND", juce::Rectangle<float> (area.getX(), area.getBottom() - 34.0f, 260.0f, 16.0f), juce::Justification::centredLeft, false);
    g.setColour (ringColour (2).withAlpha (lit));
    g.drawText ("INWARD: EACH LAYER DEEPER, SLOWER", juce::Rectangle<float> (area.getX(), area.getBottom() - 16.0f, 300.0f, 16.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
RemnantPage::RemnantPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::remnantOn, "ON", "REMNANT on or off: layers of what you play beneath it, each deeper one slower and lower. Your sound itself is never changed.");
    make (controls, ParamIDs::remnantLayers, "LAYERS", "How many layers beneath your sound (1 to 3). Each is a dream of the one above it: slower and lower again.");
    make (controls, ParamIDs::remnantStep, "STEP", "How much slower and lower each layer is than the one above: an octave (half speed), a fifth or a fourth.");
    make (controls, ParamIDs::remnantLength, "LENGTH", "How long a stretch of the past each layer replays (0.4 to 4 s) before renewing itself, smoothly, from the present.");
    make (controls, ParamIDs::remnantTaper, "TAPER", "How much quieter each deeper layer is (0 to 12 dB a layer). The mod matrix can move it.");
    make (controls, ParamIDs::remnantTone, "TONE", "How dark the layers are (each deeper one darker still). The mod matrix can move it.");
    make (controls, ParamIDs::remnantWidth, "WIDTH", "The layers leaning to alternate sides.");
    make (controls, ParamIDs::remnantMix, "MIX", "How loud the layers are beneath your sound (0: none at all). The mod matrix can move it: FX Remnant > Mix.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> RemnantPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void RemnantPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("REMNANT", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: dreams within dreams, layers of your sound beneath it, each deeper one slower", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void RemnantPage::resized()
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
