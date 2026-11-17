#include "SortPage.h"

#include "dsp/effects/Sort.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

SortDisplay::SortDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("Two rows of index cards. Above, the last window as it was played: a card a slice, its mark as high as "
                "its note (a blank card: silence). Below, the same cards re-filed in ORDER, the one playing now lit. "
                "What you hear is always a window behind what you play.");
    startTimerHz (30);
}

namespace
{
    juce::String stageOf (float amount)
    {
        return amount < 0.001f ? "LIVE" : amount < 0.5f ? "TIDY" : amount < 0.85f ? "FILED" : "SORTED";
    }

    juce::String nameOf (float note)
    {
        return note < 0.0f ? juce::String() : juce::MidiMessage::getMidiNoteName (juce::roundToInt (note), true, true, 4);
    }
}

void SortDisplay::paint (juce::Graphics& g)
{
    using S = effects::Sort;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getSortSettings();
    const auto& fx = processor.getSort();
    const auto filed = fx.getWindowsFiled() > 0;
    const auto count = filed ? juce::jlimit (1, S::maxSlices, fx.getSliceCount()) : 8;
    const auto place = settings.on && ! fx.isResting() ? fx.getPlace() : -1;
    const auto amount = settings.on ? fx.getAmount() : 0.0f;
    const auto desk = juce::Colour (0xff1d2622), card = juce::Colour (0xffe8ece4), ink = juce::Colour (0xff27302c),
               mark = juce::Colour (0xff3f8f6e), lit = juce::Colour (0xffb8ffd8);

    // The desk under fluorescent light.
    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (desk.brighter (0.25f), view.getCentreX(), view.getY(), desk.darker (0.4f),
                                             view.getCentreX(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    g.setColour (lit.withAlpha (0.06f));
    g.fillRect (view.withHeight (3.0f).translated (0.0f, 4.0f));

    // The notes the cards span, for the marks' heights.
    auto lowest = 127.0f, highest = 0.0f;
    for (int k = 0; k < count; ++k)
        if (const auto n = filed ? fx.getNote (k) : -1.0f; n >= 0.0f)
        {
            lowest = juce::jmin (lowest, n);
            highest = juce::jmax (highest, n);
        }
    if (lowest > highest)
    {
        lowest = 55.0f;
        highest = 72.0f;
    }
    const auto span = juce::jmax (6.0f, highest - lowest);

    auto area = view.reduced (view.getWidth() * 0.04f, view.getHeight() * 0.06f).withTrimmedTop (22.0f);
    const auto labelWidth = 86.0f;
    const auto rowGap = area.getHeight() * 0.1f;
    const auto rowHeight = (area.getHeight() - rowGap) * 0.5f;
    for (int row = 0; row < 2; ++row)
    {
        auto band = juce::Rectangle<float> (area.getX(), area.getY() + (float) row * (rowHeight + rowGap), area.getWidth(), rowHeight);
        auto label = band.removeFromLeft (labelWidth);
        g.setColour (card.withAlpha (0.7f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (row == 0 ? "AS PLAYED" : "FILED", label.removeFromTop (16.0f), juce::Justification::centredLeft, false);
        g.setFont (juce::FontOptions (9.5f));
        g.setColour (card.withAlpha (0.45f));
        g.drawText (row == 0 ? "the window before" : S::orderNames[settings.order], label.removeFromTop (14.0f),
                    juce::Justification::centredLeft, false);

        const auto cardWidth = band.getWidth() / (float) count;
        for (int j = 0; j < count; ++j)
        {
            const auto slice = row == 0 ? j : (filed ? fx.getOrder (j) : j);
            const auto note = filed ? fx.getNote (slice) : -1.0f;
            auto c = juce::Rectangle<float> (band.getX() + (float) j * cardWidth, band.getY(), cardWidth, band.getHeight())
                         .reduced (juce::jmin (3.0f, cardWidth * 0.08f), 2.0f);
            const auto playing = row == 1 && j == place;
            g.setColour (playing ? lit : card.withAlpha (note < 0.0f ? 0.35f : 0.92f));
            g.fillRoundedRectangle (c, 2.5f);
            // The card's tab, numbered as it was played.
            g.setColour (ink.withAlpha (0.55f));
            g.setFont (juce::FontOptions (juce::jmin (10.0f, cardWidth * 0.32f), juce::Font::bold));
            g.drawText (juce::String (slice + 1), c.removeFromTop (14.0f), juce::Justification::centred, false);
            if (note >= 0.0f)
            {
                auto field = c.reduced (c.getWidth() * 0.18f, 4.0f).withTrimmedBottom (cardWidth > 34.0f ? 14.0f : 0.0f);
                const auto y = field.getBottom() - field.getHeight() * (note - lowest) / span;
                g.setColour (playing ? ink : mark);
                g.fillRect (juce::Rectangle<float> (field.getX(), y - 2.0f, field.getWidth(), 4.0f));
                g.setColour (mark.withAlpha (0.25f));
                g.fillRect (juce::Rectangle<float> (field.getX(), y + 2.0f, field.getWidth(), field.getBottom() - y));
                if (cardWidth > 34.0f)
                {
                    g.setColour (ink);
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawText (nameOf (note), c.removeFromBottom (14.0f), juce::Justification::centred, false);
                }
            }
        }
    }

    // Across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (settings.on ? stageOf (amount) : "OFF", bar, juce::Justification::centredLeft, false);
    g.drawText ("ORDER " + S::orderNames[settings.order] + "   SLICE " + S::sliceNames[settings.slice], bar,
                juce::Justification::centred, false);
    g.drawText ("WINDOW " + S::lengthNames[settings.length], bar, juce::Justification::centredRight, false);
}

//==============================================================================
SortPage::SortPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::sortOn, "ON", "SORT on or off: the phrase cut into pieces in time with the song and played back a "
                                       "window later, re-filed in order.");
    make (controls, ParamIDs::sortAmount, "AMOUNT", "From the live sound (0) to the re-filed one (full). The mod matrix can "
                                                     "move it: FX Sort > Amount.");
    make (controls, ParamIDs::sortSlice, "SLICE", "How the phrase is cut, in time with the song: quarters, eighths, "
                                                   "sixteenths or eighth triplets.");
    make (controls, ParamIDs::sortLength, "LENGTH", "How much is re-filed at a time: half a bar, a bar or two bars. What "
                                                     "comes back is always that far behind.");
    make (controls, ParamIDs::sortOrder, "ORDER", "UP: lowest note first. DOWN: highest first. PEAK: rising to the highest "
                                                   "and back. LOUDEST: loudest first. SHUFFLE: a new order every window.");
    make (controls, ParamIDs::sortGaps, "GAPS", "Where the silent pieces go: AT END (the notes close up) or IN PLACE (the "
                                                 "rests stay where you played them).");
    make (controls, ParamIDs::sortEdge, "EDGE", "The crossfade between pieces: crisp (1 ms) to smeared (30 ms).");
    make (controls, ParamIDs::sortStrike, "STRIKE", "A hit at the start of each piece and a fall after it, so the "
                                                     "pieces cut from ringing notes sound struck again.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> SortPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void SortPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("SORT", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the phrase you played, re-filed in order", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void SortPage::resized()
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
