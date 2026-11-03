#include "NuclearSpringPage.h"

#include "dsp/effects/NuclearSpring.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

NuclearSpringDisplay::NuclearSpringDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The spring tank opened up: three springs a side, shaking as your sound runs through them; the frame "
                "flashes when a hard hit kicks the tank (BOING).");
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

void NuclearSpringDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    clock += juce::jlimit (0.0, 0.2, (now - lastTick) / 1000.0);
    lastTick = now;
    const auto& fx = processor.getNuclearSpring();
    const auto on = processor.getNuclearSpringSettings().on;
    for (int side = 0; side < 2; ++side)
        for (int k = 0; k < 3; ++k)
        {
            const auto target = on ? juce::jlimit (0.0f, 1.0f, fx.getSwing (side, k) * 8.0f) : 0.0f;
            auto& v = shake[(size_t) side][(size_t) k];
            v += (target - v) * (target > v ? 0.5f : 0.1f);
        }
    const auto kick = on ? (fx.getKick() - 1.0f) / 3.0f : 0.0f;
    flash = juce::jmax (kick, flash * 0.85f);
    repaint();
}

void NuclearSpringDisplay::paint (juce::Graphics& g)
{
    using N = effects::NuclearSpring;
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0a08));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getNuclearSpringSettings();
    const auto lit = s.on ? 1.0f : 0.3f;
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("DECAY " + juce::String (N::decaySeconds (s.decay), 1) + " S   DRIP " + juce::String (juce::roundToInt (100.0f * s.drip))
                    + "%   SIZE x" + juce::String (N::sizeScale (s.size), 2) + "   TONE " + juce::String (N::toneHz (s.tone) / 1000.0f, 1) + " KHZ   MIX "
                    + juce::String (juce::roundToInt (100.0f * s.mix)) + "%",
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (10.0f);

    // The tank: a brushed-steel pan, its frame lit by a kick.
    const auto tank = area.reduced (area.getWidth() * 0.06f, area.getHeight() * 0.04f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d42).withMultipliedBrightness (0.6f + 0.4f * lit), tank.getX(), tank.getY(),
                                             juce::Colour (0xff1c1e22), tank.getX(), tank.getBottom(), false));
    g.fillRoundedRectangle (tank, 10.0f);
    g.setColour (juce::Colour (0xffff9a3c).withAlpha ((0.25f + 0.75f * flash) * lit));
    g.drawRoundedRectangle (tank, 10.0f, 2.0f + 4.0f * flash);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int k = 0; k < 6; ++k)   // rivets
    {
        const auto x = tank.getX() + 14.0f + (tank.getWidth() - 28.0f) * (float) (k % 3) / 2.0f;
        const auto y = k < 3 ? tank.getY() + 12.0f : tank.getBottom() - 12.0f;
        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    }

    // Six springs, three a side (left above, right below), each a coil between two mounts, longer for longer
    // springs, shaking with what runs through it.
    const auto mountLeft = tank.getX() + tank.getWidth() * 0.1f;
    const auto scale = N::sizeScale (s.size) / 1.4f;
    for (int side = 0; side < 2; ++side)
        for (int k = 0; k < 3; ++k)
        {
            const auto row = side * 3 + k;
            const auto y = tank.getY() + tank.getHeight() * (0.14f + 0.145f * (float) row);
            const auto length = tank.getWidth() * 0.8f * (0.7f + 0.1f * (float) k) * (0.6f + 0.4f * scale);
            const auto x0 = mountLeft, x1 = mountLeft + length;
            const auto amount = shake[(size_t) side][(size_t) k];
            const auto turns = 26 + 4 * k;
            juce::Path coil;
            for (int i = 0; i <= turns * 8; ++i)
            {
                const auto t = (float) i / (float) (turns * 8);
                const auto wobble = amount * 7.0f * std::sin ((float) clock * (14.0f + 3.0f * (float) row) + t * 9.0f) * std::sin (t * juce::MathConstants<float>::pi);
                const auto px = x0 + (x1 - x0) * t;
                const auto py = y + wobble + 6.0f * std::sin (t * (float) turns * juce::MathConstants<float>::twoPi);
                if (i == 0) coil.startNewSubPath (px, py); else coil.lineTo (px, py);
            }
            g.setColour (juce::Colour (0xffd9d4c8).withAlpha ((0.45f + 0.55f * amount) * lit));
            g.strokePath (coil, juce::PathStrokeType (1.4f));
            g.setColour (juce::Colour (0xff8a7a5c).withAlpha (lit));
            g.fillRoundedRectangle (x0 - 10.0f, y - 8.0f, 10.0f, 16.0f, 2.0f);
            g.fillRoundedRectangle (x1, y - 8.0f, 10.0f, 16.0f, 2.0f);
        }
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (ParagonColours::textDim.withAlpha (lit));
    g.drawText ("LEFT", juce::Rectangle<float> (tank.getX() + 6.0f, tank.getY() + tank.getHeight() * 0.27f, 60.0f, 14.0f), juce::Justification::centredLeft, false);
    g.drawText ("RIGHT", juce::Rectangle<float> (tank.getX() + 6.0f, tank.getY() + tank.getHeight() * 0.71f, 60.0f, 14.0f), juce::Justification::centredLeft, false);
    if (flash > 0.2f)
    {
        g.setColour (juce::Colour (0xffff9a3c).withAlpha (lit * flash));
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText ("BOING", tank.removeFromRight (120.0f).removeFromTop (30.0f), juce::Justification::centred, false);
    }
}

//==============================================================================
NuclearSpringPage::NuclearSpringPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::springOn, "ON", "NUCLEARSPRING on or off: a vintage spring reverb tank, drippy and twangy.");
    make (controls, ParamIDs::springMix, "MIX", "How much of the tank, under your sound (0: your sound untouched). The mod matrix can move it: FX Nuclearspring > Mix.");
    make (controls, ParamIDs::springDecay, "DECAY", "How long the tank rings: 0.8 to 5 seconds to fade away.");
    make (controls, ParamIDs::springDrip, "DRIP", "How drippy and twangy: from a plain echo to the wet chirp of a spring. The mod matrix can move it.");
    make (controls, ParamIDs::springTone, "TONE", "How bright the tank is: springs are dark, 2 to 8 kHz.");
    make (controls, ParamIDs::springSize, "SIZE", "The springs' length: a small amp's short tank to a long organ tank.");
    make (controls, ParamIDs::springBoing, "BOING", "How hard a sudden hit kicks the tank: the clang of a spring reverb knocked. The mod matrix can move it.");
    make (controls, ParamIDs::springWidth, "WIDTH", "How far apart the two sides' tanks are: wide, or the tail in the middle.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> NuclearSpringPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void NuclearSpringPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("NUCLEARSPRING", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: a vintage spring reverb tank, drippy and twangy", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void NuclearSpringPage::resized()
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
