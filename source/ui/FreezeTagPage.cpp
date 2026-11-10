#include "FreezeTagPage.h"

#include "dsp/effects/FreezeTag.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

FreezeTagDisplay::FreezeTagDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The cycle, beat by beat: green while the sound plays, red where it freezes, the beat now lit; the figure "
                "runs on green and stands frozen on red, shivering with TREMBLE, knocked flat when a freeze is caught.");
    startTimerHz (30);
}

namespace
{
    // A playground figure at `foot` (its feet), `size` tall: running (legs and arms swinging with `swing`) or still.
    void paintFigure (juce::Graphics& g, juce::Point<float> foot, float size, float swing, bool fallen, juce::Colour colour)
    {
        juce::Path body;
        const auto head = size * 0.16f;
        if (fallen)
        {
            const auto y = foot.y - head * 0.6f;
            body.addEllipse (foot.x + size * 0.36f, y - head * 0.5f, head, head);
            body.startNewSubPath (foot.x - size * 0.4f, y);
            body.lineTo (foot.x + size * 0.36f, y);
            body.startNewSubPath (foot.x - size * 0.1f, y);
            body.lineTo (foot.x + size * 0.05f, y - size * 0.18f);
        }
        else
        {
            const auto hip = juce::Point<float> (foot.x, foot.y - size * 0.45f);
            const auto neck = juce::Point<float> (foot.x, foot.y - size * 0.8f);
            body.addEllipse (neck.x - head * 0.5f, neck.y - head * 1.05f, head, head);
            body.startNewSubPath (neck);
            body.lineTo (hip);
            for (const auto side : { -1.0f, 1.0f })
            {
                body.startNewSubPath (hip);
                body.lineTo (hip.x + side * swing * size * 0.28f, foot.y);
                body.startNewSubPath (neck.x, neck.y + size * 0.08f);
                body.lineTo (neck.x - side * swing * size * 0.26f, neck.y + size * 0.32f);
            }
        }
        g.setColour (colour);
        g.strokePath (body, juce::PathStrokeType (size * 0.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void FreezeTagDisplay::paint (juce::Graphics& g)
{
    using F = effects::FreezeTag;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getFreezeTagSettings();
    const auto& fx = processor.getFreezeTag();
    const auto live = settings.on && ! fx.isResting();
    const auto amount = live ? fx.getAmount() : 0.0f;
    const auto gate = live ? fx.getGate() : 0.0f;
    const auto green = juce::Colour (0xff2fd27a), red = juce::Colour (0xffff3b5c), pink = juce::Colour (0xffff7aa8);

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0f1a14), view.getX(), view.getY(),
                                             juce::Colour (0xff1a0d12), view.getRight(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    auto inner = view.reduced (view.getWidth() * 0.04f, view.getHeight() * 0.1f).withTrimmedTop (view.getHeight() * 0.06f);

    // THE PLAYGROUND: a sand line, the figure in the middle.
    auto field = inner.removeFromTop (inner.getHeight() * 0.62f);
    const auto ground = field.getBottom() - field.getHeight() * 0.08f;
    g.setColour (juce::Colour (0xff3a3020));
    g.fillRect (field.withTop (ground));
    const auto size = field.getHeight() * 0.62f;
    const auto t = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const auto frozenNow = gate > 0.5f && amount > 0.0f;
    const auto fallen = frozenNow && fx.wasCaught();
    const auto swing = frozenNow ? 0.0f : (float) std::sin (t * 9.0);
    const auto shiver = frozenNow ? settings.tremble * size * 0.012f * (float) std::sin (t * 2.0 * juce::MathConstants<double>::pi * 5.5) : 0.0f;
    const auto runX = frozenNow ? field.getCentreX() : field.getCentreX() + (float) std::sin (t * 0.7) * field.getWidth() * 0.25f;
    paintFigure (g, { runX + shiver, ground }, size, swing * (live ? 1.0f : 0.3f), fallen,
                 (frozenNow ? (fallen ? red : pink) : green).withAlpha (live ? 1.0f : 0.4f));
    g.setFont (juce::FontOptions (size * 0.22f, juce::Font::bold));
    g.setColour ((frozenNow ? red : green).withAlpha (live ? 0.9f : 0.3f));
    g.drawText (! live ? "OFF" : fallen ? "CAUGHT!" : frozenNow ? "FREEZE!" : "GO!", field.removeFromTop (size * 0.3f),
                juce::Justification::centred, false);

    // THE CYCLE: a block a beat, the frozen ones red.
    inner.removeFromTop (inner.getHeight() * 0.15f);
    const auto beats = (int) F::cycleBeats[(size_t) settings.cycle];
    const auto hold = F::holdBeats (settings.cycle, settings.hold);
    const auto now = live ? fx.getPosition() * (float) beats : -1.0f;
    const auto w = inner.getWidth() / (float) beats;
    for (int b = 0; b < beats; ++b)
    {
        const auto cell = juce::Rectangle<float> (inner.getX() + w * (float) b, inner.getY(), w, inner.getHeight() * 0.7f).reduced (2.0f);
        const auto isFrozen = (double) b >= (double) beats - hold;
        const auto lit = now >= (float) b && now < (float) (b + 1);
        g.setColour ((isFrozen ? red : green).withAlpha (lit ? 0.95f : (settings.on ? 0.3f : 0.12f)));
        g.fillRoundedRectangle (cell, 3.0f);
        if (b % 4 == 0)
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (juce::String (b / 4 + 1), cell.translated (0.0f, cell.getHeight() + 2.0f).withHeight (11.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (! settings.on ? juce::String ("OFF") : "PLAY, THEN FREEZE " + F::holdNames[settings.hold] + " IN " + F::cycleNames[settings.cycle],
                bar, juce::Justification::centredLeft, false);
    g.drawText ("FREEZES " + juce::String (fx.getFreezes()), bar, juce::Justification::centred, false);
    g.drawText ("AMOUNT " + juce::String (juce::roundToInt (100.0f * amount)) + "%", bar, juce::Justification::centredRight, false);
}

//==============================================================================
FreezeTagPage::FreezeTagPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::freezeOn, "ON", "FREEZE TAG on or off: in time with the song the sound plays, then freezes.");
    make (controls, ParamIDs::freezeAmount, "AMOUNT", "How frozen: the sound cut and its ghost holding the moment (0: the "
                                                       "sound untouched). The mod matrix can move it: FX Freeze Tag > Amount.");
    make (controls, ParamIDs::freezeCycle, "CYCLE", "How long one round of the game lasts: 1, 2 or 4 bars, in time with the song.");
    make (controls, ParamIDs::freezeHold, "HOLD", "How long it freezes at the end of each round: 1, 2 or 4 beats.");
    make (controls, ParamIDs::freezeSlice, "SLICE", "How much of the last moment the ghost holds: 40 ms (a buzz) to 250 ms "
                                                     "(a smear of the last note).");
    make (controls, ParamIDs::freezeTremble, "TREMBLE", "The ghost can't keep still: its pitch wavers and its level shakes.");
    make (controls, ParamIDs::freezeFrost, "FROST", "The ghost darkens as it freezes.");
    make (controls, ParamIDs::freezeCaught, "CAUGHT", "The chance a freeze is caught moving: the ghost sinks an octave and crumbles.");
    make (controls, ParamIDs::freezeLevel, "LEVEL", "How loud the ghost is.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> FreezeTagPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void FreezeTagPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("FREEZE TAG", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: in time with the song the sound plays, then freezes", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void FreezeTagPage::resized()
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
