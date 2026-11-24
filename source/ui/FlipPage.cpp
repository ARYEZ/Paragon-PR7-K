#include "FlipPage.h"

#include "dsp/effects/Flip.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // A grey 90s sampler: its body, a green LCD, rubber pads lit orange, pitched pads amber, reversed ones blue.
    const juce::Colour body (0xff3a3c40), bodyEdge (0xff1c1d20), lcd (0xff9fbf4a), lcdInk (0xff1d2a10),
                       pad (0xff24262a), lit (0xffff7a1a), pitched (0xffffc23d), backwards (0xff4fb6ff);
}

FlipDisplay::FlipDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The sampler: two beats of the bar you just played are caught and played back on the pads, in time with the "
                "song. The LCD says how this bar flips (LOOP, UP, DOWN, REVERSE, CHOP, STUTTER); each pad lights as its cell "
                "plays (amber pitched, blue backwards). The same place in the song always flips the same way.");
    startTimerHz (30);
}

void FlipDisplay::timerCallback() { repaint(); }

void FlipDisplay::paint (juce::Graphics& g)
{
    using F = effects::Flip;
    const auto settings = processor.getFlipSettings();
    const auto& fx = processor.getFlip();
    const auto kind = settings.on ? fx.getKindNow() : -1;
    const auto cellNow = settings.on ? fx.getCellNow() : -1;
    const auto count = settings.on ? fx.getCellCount() : 0;

    auto view = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (body);
    g.fillRoundedRectangle (view, 8.0f);
    g.setColour (bodyEdge);
    g.drawRoundedRectangle (view.reduced (2.0f), 7.0f, 2.0f);
    auto inner = view.reduced (16.0f, 12.0f);
    auto lane = inner.removeFromBottom (juce::jmin (46.0f, inner.getHeight() * 0.2f));
    inner.removeFromBottom (10.0f);

    // The LCD.
    auto left = inner.removeFromLeft (inner.getWidth() * 0.44f);
    auto screen = left.removeFromTop (left.getHeight() * 0.55f).reduced (0.0f, 4.0f);
    g.setColour (bodyEdge);
    g.fillRoundedRectangle (screen.expanded (4.0f), 4.0f);
    g.setColour (lcd);
    g.fillRect (screen);
    g.setColour (lcdInk);
    auto text = screen.reduced (10.0f, 8.0f);
    g.setFont (juce::FontOptions ("Courier New", juce::jmin (13.0f, text.getHeight() * 0.18f), juce::Font::bold));
    g.drawText (! settings.on ? juce::String ("OFF") : settings.amount < 0.001f ? juce::String ("RESTING") : juce::String ("SEQ  BAR FLIP"),
                text.removeFromTop (text.getHeight() * 0.22f), juce::Justification::centredLeft, false);
    g.setFont (juce::FontOptions ("Courier New", juce::jmin (34.0f, text.getHeight() * 0.42f), juce::Font::bold));
    g.drawText (kind >= 0 ? F::flipNames[kind] : juce::String ("--"), text.removeFromTop (text.getHeight() * 0.6f),
                juce::Justification::centredLeft, false);
    g.setFont (juce::FontOptions ("Courier New", juce::jmin (12.0f, text.getHeight() * 0.5f), juce::Font::bold));
    const auto semis = cellNow >= 0 ? fx.getCellSemitones (cellNow) : 0.0f;
    g.drawText ("PAD " + (cellNow >= 0 ? juce::String (cellNow % 16 + 1) : juce::String ("-")) + "   "
                    + (semis > 0.0f ? "+" : "") + juce::String (juce::roundToInt (semis)) + " ST"
                    + (cellNow >= 0 && fx.isCellReversed (cellNow) ? "   REV" : ""),
                text, juce::Justification::centredLeft, false);
    // Under the LCD: the sampler's colour and how many pads so far.
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (juce::String (juce::roundToInt (F::bitsFor (settings.crunch))) + " BIT   "
                    + juce::String (F::rateFor (settings.crunch) / 1000.0, 2) + " KHZ   PADS " + juce::String (fx.getFlips()),
                left.removeFromTop (24.0f), juce::Justification::centredLeft, false);

    // The pads.
    auto grid = inner.reduced (inner.getWidth() * 0.06f, 0.0f);
    const auto side = juce::jmin (grid.getWidth(), grid.getHeight());
    grid = grid.withSizeKeepingCentre (side, side);
    const auto cell = side / 4.0f;
    for (int k = 0; k < 16; ++k)
    {
        auto r = juce::Rectangle<float> (grid.getX() + cell * (float) (k % 4), grid.getBottom() - cell * (float) (k / 4 + 1), cell, cell).reduced (cell * 0.08f);
        const auto on = cellNow >= 0 && cellNow % 16 == k;
        auto colour = pad;
        if (on)
            colour = fx.isCellReversed (cellNow) ? backwards : std::abs (semis) > 0.01f ? pitched : lit;
        g.setColour (colour);
        g.fillRoundedRectangle (r, cell * 0.08f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r, cell * 0.08f, 1.2f);
        g.setColour (on ? bodyEdge : ParagonColours::textDim.withAlpha (0.6f));
        g.setFont (juce::FontOptions (juce::jmin (11.0f, cell * 0.18f), juce::Font::bold));
        g.drawText (juce::String (k + 1), r.reduced (5.0f), juce::Justification::topLeft, false);
    }

    // This bar's cells.
    g.setColour (bodyEdge);
    g.fillRoundedRectangle (lane, 4.0f);
    const auto w = lane.getWidth() / 16.0f;
    for (int k = 0; k < 16; ++k)
    {
        g.setColour (juce::Colours::white.withAlpha (k % 4 == 0 ? 0.1f : 0.04f));
        g.fillRect (lane.getX() + w * (float) k + 1.0f, lane.getY(), w - 2.0f, lane.getHeight());
    }
    for (int k = 0; k < count; ++k)
    {
        auto r = juce::Rectangle<float> (lane.getX() + w * fx.getCellStart (k) + 1.5f, lane.getY() + 5.0f,
                                         juce::jmax (2.0f, w * fx.getCellLength (k) - 3.0f), lane.getHeight() - 10.0f);
        const auto colour = fx.isCellReversed (k) ? backwards : std::abs (fx.getCellSemitones (k)) > 0.01f ? pitched : lit;
        g.setColour (colour.withAlpha (k == cellNow ? 1.0f : 0.35f));
        g.fillRoundedRectangle (r, 2.0f);
    }
    g.setColour (ParagonColours::text);
    const auto x = lane.getX() + w * juce::jlimit (0.0f, 16.0f, fx.getBarPosition());
    g.drawVerticalLine ((int) x, lane.getY() - 3.0f, lane.getBottom() + 3.0f);
}

//==============================================================================
FlipPage::FlipPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::flipOn, "ON", "FLIP on or off: a 90s sampler flipping a beat of what you just played, in time with the song.");
    make (controls, ParamIDs::flipAmount, "AMOUNT", "How much of the song is flipped: up to a third, a stutter into the bar line every "
                                                    "other bar; to two thirds, the last two beats of every bar; above, whole bars. "
                                                    "The mod matrix can move it: FX Flip > Amount.");
    make (controls, ParamIDs::flipPitch, "PITCH", "How far UP and DOWN pitch the pads (UP takes this and two semitones more in turn).");
    make (controls, ParamIDs::flipCrunch, "CRUNCH", "The sampler's colour: at the middle 12 bits at 26 kHz (as in the sketch), down to "
                                                    "clean or up to 8 bits at 13 kHz.");
    make (controls, ParamIDs::flipPump, "PUMP", "A compressor ducking the flips on every beat and letting them back over an eighth.");
    make (controls, ParamIDs::flipMix, "MIX", "At 100% the flips replace what you played where they come in; lower, they sit over it.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> FlipPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void FlipPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("FLIP", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: a 90s sampler flipping a beat of what you just played, in time with the song",
                juce::Rectangle<int> (24, 38, 480, 16), juce::Justification::centredLeft, false);
}

void FlipPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (500);
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
    SynthPanel::layoutRow (controlRow, row, 96);
}
