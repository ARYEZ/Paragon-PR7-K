#include "LeviathanPage.h"

#include "dsp/effects/Leviathan.h"
#include "dsp/synth/VoiceFilter.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

LeviathanDisplay::LeviathanDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The voice beneath: a vast shape in the dark, swelling as it sings, rougher with GROWL; the note it sings "
                "and its vowel (OO to EE, wandering on its own) are shown as they move.");
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

void LeviathanDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    clock += juce::jlimit (0.0, 0.2, (now - lastTick) / 1000.0);
    lastTick = now;
    const auto on = processor.getLeviathanSettings().on;
    const auto level = on ? processor.getLeviathan().getLevelDb() : -100.0f;
    const auto target = juce::jlimit (0.0f, 1.0f, (level + 54.0f) / 48.0f);
    swell += (target - swell) * (target > swell ? 0.25f : 0.05f);
    repaint();
}

void LeviathanDisplay::paint (juce::Graphics& g)
{
    using L = effects::Leviathan;
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050b14), bounds.getCentreX(), bounds.getY(), juce::Colour (0xff010205),
                                             bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getLeviathanSettings();
    const auto& fx = processor.getLeviathan();
    const auto lit = s.on ? 1.0f : 0.3f;
    const auto sung = s.on ? fx.getSungNote() : -1.0f;
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    const auto octaves = juce::jlimit (0, 1, s.depth) + 1;
    juce::String heading = sung < 0.0f ? juce::String ("SILENT: PLAY TO WAKE IT")
                                       : "SINGING " + juce::MidiMessage::getMidiNoteName (juce::roundToInt (sung), true, true, 4) + " ("
                                             + juce::String (440.0 * std::pow (2.0, (sung - 69.0) / 12.0), 1) + " HZ), "
                                             + L::depthNames[octaves - 1] + " BENEATH "
                                             + juce::MidiMessage::getMidiNoteName (juce::roundToInt (sung) + 12 * octaves, true, true, 4);
    g.drawText (heading + "   GROWL " + juce::String (juce::roundToInt (100.0f * s.growl)) + "%   MIX " + juce::String (juce::roundToInt (100.0f * s.mix)) + "%",
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (8.0f);
    auto strip = area.removeFromBottom (34.0f);

    // Faint currents in the dark.
    for (int k = 0; k < 7; ++k)
    {
        juce::Path current;
        const auto y = area.getY() + area.getHeight() * ((float) k + 0.5f) / 7.0f;
        for (int x = 0; x <= 60; ++x)
        {
            const auto px = area.getX() + area.getWidth() * (float) x / 60.0f;
            const auto py = y + 6.0f * (float) std::sin (clock * 0.3 + x * 0.21 + k * 1.7);
            if (x == 0) current.startNewSubPath (px, py); else current.lineTo (px, py);
        }
        g.setColour (juce::Colour (0xff1c3550).withAlpha (0.25f * lit));
        g.strokePath (current, juce::PathStrokeType (1.0f));
    }

    // The voice: a vast slow shape, its outline rippling (rougher with GROWL), swelling with its level.
    const auto centre = area.getCentre().translated (0.0f, area.getHeight() * 0.05f);
    const auto width = area.getWidth() * (0.42f + 0.12f * swell);
    const auto height = area.getHeight() * (0.16f + 0.22f * swell);
    for (int layer = 3; layer >= 0; --layer)
    {
        juce::Path body;
        const auto grow = 1.0f + 0.18f * (float) layer;
        for (int i = 0; i <= 120; ++i)
        {
            const auto a = juce::MathConstants<double>::twoPi * i / 120.0;
            const auto ripple = 1.0 + 0.06 * std::sin (a * 5.0 + clock * 0.7) + 0.03 * s.growl * std::sin (a * 23.0 + clock * 9.0);
            const auto px = centre.x + (float) (std::cos (a) * width * 0.5 * grow * ripple);
            const auto py = centre.y + (float) (std::sin (a) * height * 0.5 * grow * ripple);
            if (i == 0) body.startNewSubPath (px, py); else body.lineTo (px, py);
        }
        body.closeSubPath();
        g.setColour (juce::Colour (0xff4f8fbf).withAlpha ((layer == 0 ? 0.28f : 0.07f) * lit * (0.4f + 0.6f * swell)));
        g.fillPath (body);
    }
    // Its eye: a dim light, brighter as it sings.
    g.setColour (juce::Colour (0xffbfe6ff).withAlpha ((0.15f + 0.6f * swell) * lit));
    g.fillEllipse (centre.x + width * 0.28f, centre.y - height * 0.12f, 7.0f, 7.0f);

    // The vowel along the bottom, OO to EE, marked where it is.
    const auto vowel = s.on ? fx.getVowel() : 0.3f;
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    for (int k = 0; k < synth::vocal::numVowels; ++k)
    {
        const auto x = strip.getX() + strip.getWidth() * (float) k / (float) (synth::vocal::numVowels - 1);
        g.setColour (ParagonColours::textDim.withAlpha (lit));
        g.drawText (synth::vocal::vowelNames[k], juce::Rectangle<float> (x - 20.0f, strip.getBottom() - 16.0f, 40.0f, 16.0f), juce::Justification::centred, false);
    }
    const auto line = strip.withHeight (2.0f).withY (strip.getY() + 8.0f);
    g.setColour (juce::Colour (0xff1c3550).withAlpha (lit));
    g.fillRect (line);
    g.setColour (juce::Colour (0xffbfe6ff).withAlpha (lit));
    g.fillEllipse (line.getX() + line.getWidth() * vowel - 6.0f, line.getCentreY() - 6.0f, 12.0f, 12.0f);
}

//==============================================================================
LeviathanPage::LeviathanPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::leviathanOn, "ON", "LEVIATHAN on or off: a vast voice singing far beneath what you play.");
    make (controls, ParamIDs::leviathanMix, "MIX", "How loud the voice is: at full as loud as your sound (0: your sound untouched). The mod matrix can move it: FX Leviathan > Mix.");
    make (controls, ParamIDs::leviathanDepth, "DEPTH", "How far beneath the lowest note you play it sings: one octave or two.");
    make (controls, ParamIDs::leviathanDrift, "DRIFT", "How often its vowel moves, on its own: slowly (every 6 s) or restlessly (under a second). The mod matrix can move it.");
    make (controls, ParamIDs::leviathanGrowl, "GROWL", "How rough its throat is: a smooth hum, or creaking and clicking. The mod matrix can move it.");
    make (controls, ParamIDs::leviathanDelay, "DELAY", "How long it takes to answer: at once, or up to 1.5 s behind you.");
    make (controls, ParamIDs::leviathanSize, "SIZE", "How big its throat is: every vowel's resonances lowered up to nearly twice as deep.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> LeviathanPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void LeviathanPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("LEVIATHAN", juce::Rectangle<int> (24, 14, 200, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: a vast voice singing far beneath what you play", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void LeviathanPage::resized()
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
