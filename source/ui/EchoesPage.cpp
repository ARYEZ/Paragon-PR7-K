#include "EchoesPage.h"

#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    using Mode = effects::Echoes::Mode;
    constexpr int columns = 360;
}

EchoesDisplay::EchoesDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The past: the last two stretches of what's been played, newest at NOW on the left. Lit: the part coming back "
                "under you now (the arrow shows which way it plays).");
    startTimerHz (20);
}

void EchoesDisplay::timerCallback()
{
    shown = processor.getEchoesPicture (columns);
    repaint();
}

void EchoesDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff070507));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    const auto area = bounds.reduced (24.0f, 30.0f);
    const auto settings = processor.getEchoesSettings();
    const auto xFor = [&] (float share) { return area.getX() + area.getWidth() * share; };

    // The stretches: the boundary in the middle (two stretches are drawn), and their names.
    g.setColour (ParagonColours::panelEdge);
    g.drawVerticalLine (juce::roundToInt (xFor (0.5f)), area.getY(), area.getBottom());
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::textDim);
    g.drawText ("NOW", juce::Rectangle<float> (area.getX() - 20.0f, area.getBottom() + 6.0f, 60.0f, 14.0f), juce::Justification::centredLeft, false);
    // Labels in beats or bars ago (4 beats to a bar).
    const auto beats = effects::Echoes::distanceBeats[(size_t) juce::jlimit (0, (int) std::size (effects::Echoes::distanceBeats) - 1, settings.distance)];
    const auto stretchBeats = settings.mode == Mode::tape ? 2.0f * beats : beats;
    const auto ago = [] (float b)
    {
        if (b >= 4.0f && std::fmod (b, 4.0f) == 0.0f)
            return juce::String (juce::roundToInt (b / 4.0f)) + (b == 4.0f ? " BAR AGO" : " BARS AGO");
        return juce::String (juce::roundToInt (b)) + (b == 1.0f ? " BEAT AGO" : " BEATS AGO");
    };
    g.drawText (ago (stretchBeats), juce::Rectangle<float> (xFor (0.5f) - 80.0f, area.getBottom() + 6.0f, 160.0f, 14.0f), juce::Justification::centred, false);
    g.drawText (ago (2.0f * stretchBeats), juce::Rectangle<float> (area.getRight() - 140.0f, area.getBottom() + 6.0f, 160.0f, 14.0f),
                juce::Justification::centredRight, false);

    // What's been played: a mirrored level strip, newest on the left.
    if (shown.levels.size() > 1)
    {
        const auto n = (int) shown.levels.size();
        // Scaled to the loudest moment shown (so quiet playing still draws), on a square-root curve like a meter.
        auto loudest = 1.0e-4f;
        for (const auto level : shown.levels)
            loudest = juce::jmax (loudest, level);
        juce::Path wave;
        const auto middle = area.getCentreY();
        const auto half = area.getHeight() * 0.42f;
        const auto height = [&] (int k) { return half * std::sqrt (juce::jlimit (0.0f, 1.0f, shown.levels[(size_t) k] / loudest)); };
        wave.startNewSubPath (area.getX(), middle);
        for (int k = 0; k < n; ++k)
            wave.lineTo (xFor ((float) k / (float) (n - 1)), middle - height (k));
        for (int k = n - 1; k >= 0; --k)
            wave.lineTo (xFor ((float) k / (float) (n - 1)), middle + height (k));
        wave.closeSubPath();
        g.setColour (ParagonColours::textDim.withAlpha (0.45f));
        g.fillPath (wave);
    }

    // The part coming back now, lit, with the way it's read.
    const auto from = juce::jmin (shown.readFrom, shown.readTo), to = juce::jmax (shown.readFrom, shown.readTo);
    const auto lit = juce::Rectangle<float>::leftTopRightBottom (xFor (from) - 1.0f, area.getY(), xFor (to) + 1.0f, area.getBottom());
    g.setColour (ParagonColours::accent.withAlpha (settings.on ? 0.22f : 0.08f));
    g.fillRect (lit);
    g.setColour (settings.on ? ParagonColours::accent : ParagonColours::textDim);
    g.drawVerticalLine (juce::roundToInt (xFor (shown.readFrom)), area.getY(), area.getBottom());
    const auto label = settings.mode == Mode::echo ? juce::String ("ECHO") : settings.mode == Mode::reverse ? juce::String ("BACKWARDS")
                     : settings.mode == Mode::tape ? juce::String ("HALF SPEED") : juce::String ("CAUGHT");
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (label, juce::Rectangle<float> (xFor (shown.readFrom) - 70.0f, area.getY() - 22.0f, 140.0f, 16.0f), juce::Justification::centred, false);
}

//==============================================================================
EchoesPage::EchoesPage (ParagonX1AudioProcessor& p) : processor (p), display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::echoOn, "ON", "ECHOES on or off: the past played back under the present, in time with your song.");
    make (top, ParamIDs::echoMode, "MODE", "ECHO: the sound from DISTANCE ago, exactly. REVERSE: each stretch comes back backwards while the next plays. "
                                           "TAPE: each stretch's first half comes back at half speed, an octave down. LOOP: a stretch is caught "
                                           "and repeated while you play on.");
    make (controls, ParamIDs::echoDistance, "DISTANCE", "How far back, in beats and bars of your song.");
    make (controls, ParamIDs::echoMix, "MIX", "How loud the past comes back under what you're playing (the mod matrix can move it: FX Echoes > Mix).");
    make (controls, ParamIDs::echoFeedback, "FEEDBACK", "Echoes of echoes: each pass back is fed in again. In LOOP, REPEATS: how many times a "
                                                       "stretch repeats before the next is caught (all the way up: for ever).");
    make (controls, ParamIDs::echoAge, "AGE", "How much each pass back is worn: darker and a little saturated, so older echoes sound older.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
    startTimerHz (5);
}

void EchoesPage::timerCallback()
{
    // FEEDBACK is REPEATS in LOOP.
    const auto mode = (int) processor.getEchoesSettings().mode;
    if (mode != shownMode)
    {
        shownMode = mode;
        controls[2]->setLabelText (mode == (int) Mode::loop ? "REPEATS" : "FEEDBACK");
    }
}

std::vector<ParamControl*> EchoesPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void EchoesPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("ECHOES", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Insert 4: after LO-FI", juce::Rectangle<int> (24, 38, 200, 16), juce::Justification::centredLeft, false);
}

void EchoesPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (190);
    back.setBounds (bar.removeFromRight (150).withSizeKeepingCentre (150, 30));
    bar.removeFromRight (20);
    top[0]->setBounds (bar.removeFromLeft (70));
    bar.removeFromLeft (10);
    top[1]->setBounds (bar.removeFromLeft (200));
    area.removeFromTop (8);
    auto controlRow = area.removeFromBottom (ParamControl::knobHeight);
    area.removeFromBottom (10);
    display.setBounds (area);
    std::vector<juce::Component*> row;
    for (auto& c : controls)
        row.push_back (c.get());
    SynthPanel::layoutRow (controlRow, row, 120);
}
