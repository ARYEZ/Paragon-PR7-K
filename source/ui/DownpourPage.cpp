#include "DownpourPage.h"

#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

DownpourDisplay::DownpourDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The window: the patter as streaks, each drop a ripple where it lands (across by its place, up by its "
                "note, named while fresh). At the left, the drops a second and the last drop's note.");
    startTimerHz (30);
}

juce::String DownpourDisplay::noteName (float note)
{
    return juce::MidiMessage::getMidiNoteName (juce::roundToInt (note), true, true, 4);
}

void DownpourDisplay::timerCallback()
{
    const auto& rain = processor.getDownpour();
    shown = rain.getRecentDrops();
    now = rain.getSampleClock();
    patter = rain.getCurrentPatterRate();
    dropRate = rain.getCurrentDropRate();
    ++frame;
    repaint();
}

void DownpourDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto on = processor.getDownpourSettings().on;
    // The glass: night blue at the top to near black, the plugin's edge.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1020), bounds.getX(), bounds.getY(),
                                             juce::Colour (0xff050407), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    auto area = bounds.reduced (24.0f, 20.0f);
    const auto accent = on ? ParagonColours::accent : ParagonColours::textDim;

    // At the left: the drops a second, big, and the last drop's note.
    auto reading = area.removeFromLeft (juce::jmin (240.0f, area.getWidth() * 0.26f));
    g.setColour (accent);
    g.setFont (juce::FontOptions (reading.getHeight() * 0.36f, juce::Font::bold));
    g.drawText (juce::String (dropRate, dropRate < 10.0f ? 1 : 0), reading.removeFromTop (reading.getHeight() * 0.5f), juce::Justification::centredBottom, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("DROPS A SECOND", reading.removeFromTop (20.0f), juce::Justification::centred, false);
    reading.removeFromTop (10.0f);
    const auto& last = shown.back();
    g.setColour (on && last.sample >= 0 ? juce::Colours::white.withAlpha (0.85f) : ParagonColours::textDim);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (last.sample >= 0 ? noteName (last.note) : juce::String ("--"), reading.removeFromTop (28.0f), juce::Justification::centred, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("LAST DROP", reading.removeFromTop (18.0f), juce::Justification::centred, false);
    g.drawText (juce::String (juce::roundToInt (patter)) + " TAPS A SECOND", reading.removeFromTop (20.0f), juce::Justification::centred, false);
    area.removeFromLeft (16.0f);

    // The pane.
    const auto pane = area;
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (pane, 4.0f);
    g.saveState();
    g.reduceClipRegion (pane.toNearestInt());

    // The patter: short slanted streaks, as many as are falling (a fresh scatter each frame).
    juce::Random scatter ((juce::int64) frame * 7919);
    const auto streaks = juce::jmin (260, (int) std::round (patter * 0.06f));
    g.setColour (juce::Colour (0xff9fb8ff).withAlpha (on ? 0.35f : 0.15f));
    for (int k = 0; k < streaks; ++k)
    {
        const auto x = pane.getX() + scatter.nextFloat() * pane.getWidth();
        const auto y = pane.getY() + scatter.nextFloat() * pane.getHeight();
        const auto len = 6.0f + 10.0f * scatter.nextFloat();
        g.drawLine (x, y, x - len * 0.25f, y + len, 1.0f);
    }

    // The drops: a ripple each, growing and fading over 1.5 s; across by its place, up by its note (C5 to C9).
    const auto rate = processor.getDownpour().getSampleRate();
    for (const auto& d : shown)
    {
        if (d.sample < 0)
            continue;
        const auto age = (float) ((double) (now - d.sample) / rate);
        if (age < 0.0f || age > 1.5f)
            continue;
        const auto x = pane.getX() + pane.getWidth() * (0.5f + 0.45f * d.pan);
        const auto y = pane.getBottom() - pane.getHeight() * juce::jlimit (0.05f, 0.95f, (d.note - 72.0f) / 48.0f);
        const auto fade = 1.0f - age / 1.5f;
        const auto radius = 4.0f + 46.0f * std::sqrt (age / 1.5f) * (0.6f + 0.4f * d.strength);
        g.setColour (accent.withAlpha (fade * (on ? 0.9f : 0.4f)));
        g.drawEllipse (x - radius, y - radius * 0.45f, radius * 2.0f, radius * 0.9f, 1.6f);
        if (radius > 14.0f)
        {
            const auto inner = radius * 0.55f;
            g.setColour (accent.withAlpha (fade * 0.45f));
            g.drawEllipse (x - inner, y - inner * 0.45f, inner * 2.0f, inner * 0.9f, 1.0f);
        }
        if (age < 0.12f)   // the strike
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f * (1.0f - age / 0.12f)));
            g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }
        if (age < 0.5f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f * (1.0f - age / 0.5f)));
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (noteName (d.note), juce::Rectangle<float> (x + 8.0f, y - 22.0f, 50.0f, 14.0f), juce::Justification::centredLeft, false);
        }
    }
    g.restoreState();

    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("LEFT", juce::Rectangle<float> (pane.getX() + 6.0f, pane.getBottom() - 16.0f, 60.0f, 14.0f), juce::Justification::centredLeft, false);
    g.drawText ("RIGHT", juce::Rectangle<float> (pane.getRight() - 66.0f, pane.getBottom() - 16.0f, 60.0f, 14.0f), juce::Justification::centredRight, false);
    g.drawText ("HIGHER NOTES UP", juce::Rectangle<float> (pane.getX() + 6.0f, pane.getY() + 4.0f, 160.0f, 14.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
DownpourPage::DownpourPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::downpourOn, "ON", "DOWNPOUR on or off: rain on the glass, its drops ringing on the notes you play.");
    make (controls, ParamIDs::downpourRain, "RAIN", "How hard it rains: a few spots to a downpour (the patter, and the drops with it). "
                                                    "The mod matrix can move it: FX Downpour > Rain.");
    make (controls, ParamIDs::downpourDrops, "DROPS", "How many big drops ring out, each on one of the notes sounding (up to 12 a second "
                                                      "at full RAIN).");
    make (controls, ParamIDs::downpourOctave, "OCTAVE", "How far over your notes the drops ring (now and then one octave more).");
    make (controls, ParamIDs::downpourDistance, "DISTANCE", "On the glass (bright, the taps sharp) to out in the street (dull, soft).");
    make (controls, ParamIDs::downpourSpace, "SPACE", "How much of the rain goes into the REVERB's send, the preset's own room (the REVERB must be on).");
    make (controls, ParamIDs::downpourWidth, "WIDTH", "How far the drops and the patter are spread across left and right.");
    make (controls, ParamIDs::downpourFollow, "FOLLOW", "How much the rain follows the sound: all the way up, it falls only while it plays "
                                                        "(and as hard as it's loud), so it still falls silent; at 0 it never stops.");
    make (controls, ParamIDs::downpourLevel, "LEVEL", "How loud the rain is. The sound passes under it unchanged.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> DownpourPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void DownpourPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("DOWNPOUR", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Insert 6: rain into the REVERB, added after CRACKLE", juce::Rectangle<int> (24, 38, 320, 16), juce::Justification::centredLeft, false);
}

void DownpourPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (330);
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
    SynthPanel::layoutRow (controlRow, row, 120);
}
