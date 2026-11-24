#include "ScratchPage.h"

#include "dsp/effects/Scratch.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // A warehouse at 3 a.m.: black, gunmetal, acid green and a warning red.
    const juce::Colour deck (0xff0a0b0d), metal (0xff2a2e33), chrome (0xffb8c0c8), acid (0xff9dff2e), warning (0xffff3b2f);
    constexpr double turnsPerSecond = 100.0 / 3.0 / 60.0;   // 33 1/3: the record's mark turns this fast for a second of sound
}

ScratchDisplay::ScratchDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The deck: the record turns as the hand moves it (its red mark is where the needle is from the hit it's cutting "
                "up), the crossfader on the right open or cut. Along the bottom, this bar's routine: each move in the sixteenths "
                "it takes, the one playing lit. AMOUNT sets how much of the song the DJ takes; the same place in the song is "
                "always the same routine.");
    startTimerHz (30);
}

void ScratchDisplay::timerCallback() { repaint(); }

void ScratchDisplay::paint (juce::Graphics& g)
{
    using S = effects::Scratch;
    const auto settings = processor.getScratchSettings();
    const auto& fx = processor.getScratch();
    const auto move = settings.on ? fx.getMoveNow() : -1;

    auto view = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (deck);
    g.fillRoundedRectangle (view, 6.0f);
    g.setColour (metal);
    g.drawRoundedRectangle (view.reduced (3.0f), 5.0f, 1.5f);

    // The top bar.
    auto bar = view.reduced (14.0f, 6.0f).removeFromTop (22.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (! settings.on ? juce::String ("OFF") : move >= 0 ? "SCRATCHING: " + S::moveNames[move]
                : settings.amount < 0.001f ? juce::String ("RESTING") : juce::String ("WAITING FOR THE NEXT CUT"),
                bar, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.drawText ("MOVES " + juce::String (fx.getMoveCount()) + "   DIRT " + juce::String (juce::roundToInt (settings.dirt * 100.0f))
                    + "%   MIX " + juce::String (juce::roundToInt (settings.mix * 100.0f)) + "%",
                bar, juce::Justification::centredRight, false);

    auto body = view.reduced (16.0f, 0.0f).withTrimmedTop (32.0f).withTrimmedBottom (10.0f);
    auto lane = body.removeFromBottom (juce::jmin (54.0f, body.getHeight() * 0.24f));
    body.removeFromBottom (8.0f);

    // The turntable.
    const auto platterSize = juce::jmin (body.getHeight(), body.getWidth() * 0.42f);
    auto platterArea = body.removeFromLeft (platterSize + 30.0f).withSizeKeepingCentre (platterSize, platterSize);
    const auto c = platterArea.getCentre();
    const auto radius = platterSize * 0.5f;
    g.setColour (metal);
    g.fillEllipse (platterArea.expanded (6.0f));
    g.setColour (juce::Colour (0xff050506));
    g.fillEllipse (platterArea);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (float r = radius * 0.4f; r < radius * 0.97f; r += 3.0f)
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.6f);   // the grooves
    const auto angle = (float) (fx.getRecordSeconds() * turnsPerSecond * juce::MathConstants<double>::twoPi) * (move >= 0 ? 1.0f : 0.0f);
    const auto label = radius * 0.36f;
    g.setColour (move >= 0 ? acid : acid.withAlpha (0.45f));
    g.fillEllipse (c.x - label, c.y - label, label * 2.0f, label * 2.0f);
    g.setColour (deck);
    g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
    // The mark: where the needle is from the hit.
    {
        juce::Path mark;
        mark.addRectangle (-2.5f, -radius * 0.97f, 5.0f, radius * 0.6f);
        g.setColour (warning);
        g.fillPath (mark, juce::AffineTransform::rotation (angle).translated (c));
    }
    // The tonearm, resting on the record's outer grooves.
    {
        const juce::Point<float> pivot (platterArea.getRight() + 12.0f, platterArea.getY() + radius * 0.15f);
        const juce::Point<float> tip (c.x + radius * 0.55f, c.y - radius * 0.62f);
        g.setColour (chrome);
        g.drawLine ({ pivot, tip }, 4.0f);
        g.fillEllipse (pivot.x - 8.0f, pivot.y - 8.0f, 16.0f, 16.0f);
        g.setColour (chrome.darker (0.4f));
        g.fillRect (juce::Rectangle<float> (14.0f, 9.0f).withCentre (tip));
    }

    // The mixer: the crossfader, open (lit on the right) or cut.
    auto mixer = body.reduced (body.getWidth() * 0.08f, body.getHeight() * 0.1f);
    g.setColour (metal);
    g.fillRoundedRectangle (mixer, 6.0f);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("CROSSFADER", mixer.removeFromTop (22.0f), juce::Justification::centred, false);
    auto moveArea = mixer.removeFromTop (mixer.getHeight() * 0.45f);
    g.setColour (move >= 0 ? acid : ParagonColours::textDim.withAlpha (0.4f));
    g.setFont (juce::FontOptions (juce::jmin (34.0f, moveArea.getHeight() * 0.6f), juce::Font::bold | juce::Font::italic));
    g.drawText (move >= 0 ? S::moveNames[move] : juce::String ("---"), moveArea, juce::Justification::centred, false);
    auto slot = mixer.withSizeKeepingCentre (mixer.getWidth() * 0.8f, 8.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (slot, 3.0f);
    const auto open = move >= 0 ? fx.getFader() : 0.0f;
    const auto capX = slot.getX() + slot.getWidth() * (0.15f + 0.7f * open);
    g.setColour (open > 0.5f ? acid : warning);
    g.fillRoundedRectangle (juce::Rectangle<float> (26.0f, 30.0f).withCentre ({ capX, slot.getCentreY() }), 4.0f);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("CUT", slot.withY (slot.getBottom() + 18.0f).withHeight (12.0f), juce::Justification::centredLeft, false);
    g.drawText ("OPEN", slot.withY (slot.getBottom() + 18.0f).withHeight (12.0f), juce::Justification::centredRight, false);

    // This bar's routine.
    const auto cellW = lane.getWidth() / 16.0f;
    for (int k = 0; k < 16; ++k)
    {
        g.setColour (juce::Colours::white.withAlpha (k % 4 == 0 ? 0.12f : 0.05f));
        g.fillRect (lane.getX() + cellW * (float) k + 1.0f, lane.getY(), cellW - 2.0f, lane.getHeight());
    }
    const auto playing = fx.getBarPosition();
    for (int k = 0; k < (settings.on ? fx.getPlannedCount() : 0); ++k)
    {
        const auto start = fx.getPlannedStart (k), length = fx.getPlannedLength (k), kind = fx.getPlannedMove (k);
        auto cell = juce::Rectangle<float> (lane.getX() + cellW * (float) start + 2.0f, lane.getY() + 4.0f,
                                            cellW * (float) length - 4.0f, lane.getHeight() - 8.0f);
        const auto now = playing >= (float) start && playing < (float) (start + length);
        g.setColour ((kind == (int) S::Move::rewind ? warning : acid).withAlpha (now ? 0.95f : 0.35f));
        g.fillRoundedRectangle (cell, 3.0f);
        g.setColour (now ? deck : ParagonColours::text);
        g.setFont (juce::FontOptions (juce::jmin (11.0f, cell.getHeight() * 0.4f), juce::Font::bold));
        g.drawFittedText (S::moveNames[kind], cell.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.6f);
    }
    g.setColour (ParagonColours::text);
    const auto x = lane.getX() + cellW * juce::jlimit (0.0f, 16.0f, playing);
    g.drawVerticalLine ((int) x, lane.getY() - 3.0f, lane.getBottom() + 3.0f);
}

//==============================================================================
ScratchPage::ScratchPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::scratchOn, "ON", "SCRATCH on or off: a turntablist cutting up what you just played, in time with the song.");
    make (controls, ParamIDs::scratchAmount, "AMOUNT", "How much of the song the DJ takes: up to a third, a fill at the end of every "
                                                       "other bar; to two thirds, the last two beats of every bar; above, whole bars. "
                                                       "The mod matrix can move it: FX Scratch > Amount.");
    make (controls, ParamIDs::scratchStroke, "STROKE", "How far the hand pulls the record each stroke: short is tight and high, long "
                                                       "drags out more of the hit and dips lower.");
    make (controls, ParamIDs::scratchDirt, "DIRT", "The mixer pushed into the red: at 0 the drive you heard in the sketch, up to four times "
                                                   "harder with a second fuzz stage and more bite.");
    make (controls, ParamIDs::scratchMix, "MIX", "At 100% the scratches replace what you played while they cut in; lower, they sit over "
                                                 "it, down to 0% where your playing carries on at full under them.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> ScratchPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void ScratchPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("SCRATCH", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: a turntablist cutting up what you just played, in time with the song",
                juce::Rectangle<int> (24, 38, 480, 16), juce::Justification::centredLeft, false);
}

void ScratchPage::resized()
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
