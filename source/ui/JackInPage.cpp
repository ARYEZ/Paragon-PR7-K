#include "JackInPage.h"

#include "dsp/effects/JackIn.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

JackInDisplay::JackInDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The phrase as slices, two bars of them: dark while untouched, lit by what each became once taken "
                "(REPLAY, REVERSE, STUTTER, UP, DOWN, STOP), the newest outlined.");
    startTimerHz (30);
}

namespace
{
    using Move = effects::JackIn::Move;

    juce::String stageOf (float amount)
    {
        return amount < 0.001f ? "CLEAN" : amount < 0.45f ? "PROBE" : amount < 0.8f ? "BREACH" : "TAKEOVER";
    }

    juce::Colour colourOf (Move m)
    {
        switch (m)
        {
            case Move::replay:  return juce::Colour (0xff27e0f0);   // cyan
            case Move::reverse: return juce::Colour (0xffff2fa8);   // magenta
            case Move::stutter: return juce::Colour (0xfff5e626);   // yellow
            case Move::up:      return juce::Colour (0xff7dff6a);   // green
            case Move::down:    return juce::Colour (0xff8a6bff);   // violet
            case Move::stop:    return juce::Colour (0xffff5a2a);   // red-orange
            case Move::dry:     break;
        }
        return juce::Colour (0xff1b2430);
    }

    const char* shortOf (Move m)
    {
        switch (m)
        {
            case Move::replay:  return "RPL";
            case Move::reverse: return "REV";
            case Move::stutter: return "STT";
            case Move::up:      return "+12";
            case Move::down:    return "-12";
            case Move::stop:    return "STOP";
            case Move::dry:     break;
        }
        return "";
    }
}

void JackInDisplay::paint (juce::Graphics& g)
{
    using J = effects::JackIn;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getJackInSettings();
    const auto& fx = processor.getJackIn();
    const auto live = settings.on && ! fx.isResting();
    const auto amount = settings.on ? fx.getAmount() : 0.0f;

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff070a12), view.getX(), view.getY(),
                                             juce::Colour (0xff150818), view.getRight(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);

    // The grid: two bars of slices.
    auto gridArea = view.reduced (view.getWidth() * 0.04f, view.getHeight() * 0.12f).withTrimmedTop (view.getHeight() * 0.06f);
    const auto perBar = juce::jmax (1, fx.getSlicesPerBar());
    const auto count = live ? fx.getSliceCount() : 0;
    const auto newest = count - 1;
    const auto firstBar = newest >= 0 ? (newest / perBar) * perBar - perBar : -perBar;
    const auto rowGap = gridArea.getHeight() * 0.08f;
    const auto rowHeight = (gridArea.getHeight() - rowGap) * 0.5f;
    const auto cellWidth = gridArea.getWidth() / (float) perBar;
    const auto perBeat = juce::jmax (1, perBar / 4);
    for (int row = 0; row < 2; ++row)
    {
        const auto y = gridArea.getY() + (float) row * (rowHeight + rowGap);
        for (int k = 0; k < perBar; ++k)
        {
            const auto slice = firstBar + row * perBar + k;
            auto cell = juce::Rectangle<float> (gridArea.getX() + (float) k * cellWidth, y, cellWidth, rowHeight).reduced (1.0f + (k % perBeat == 0 ? 0.0f : 0.5f), 1.0f);
            const auto exists = slice >= 0 && slice <= newest && newest - slice < J::maxHistoryKept;
            const auto move = exists ? fx.getMove (slice) : Move::dry;
            const auto age = exists ? (float) (newest - slice) / (float) (2 * perBar) : 1.0f;
            auto colour = colourOf (move);
            if (move != Move::dry)
                colour = colour.withMultipliedBrightness (1.0f - 0.45f * age);
            else if (exists)
                colour = juce::Colour (0xff253244);
            g.setColour (colour);
            g.fillRect (cell);
            if (move != Move::dry && cell.getWidth() > 18.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.75f));
                g.setFont (juce::FontOptions (juce::jmin (11.0f, cell.getWidth() * 0.32f), juce::Font::bold));
                g.drawText (shortOf (move), cell, juce::Justification::centred, false);
            }
            if (exists && slice == newest)
            {
                g.setColour (juce::Colours::white);
                g.drawRect (cell.expanded (1.0f), 2.0f);
            }
        }
        // The beats.
        g.setColour (juce::Colour (0xff27e0f0).withAlpha (0.35f));
        for (int b = 0; b <= 4; ++b)
        {
            const auto x = gridArea.getX() + (float) (b * perBeat) * cellWidth;
            g.drawLine (x, y - 3.0f, x, y + rowHeight + 3.0f, b % 4 == 0 ? 1.5f : 0.8f);
        }
    }

    // Scan lines, heavier as more is taken.
    g.setColour (juce::Colours::black.withAlpha (0.10f + 0.25f * amount));
    for (auto y = view.getY(); y < view.getBottom(); y += 3.0f)
        g.drawHorizontalLine ((int) y, view.getX(), view.getRight());
    // The trace: a jagged line across the bottom, wilder with AMOUNT.
    if (amount > 0.001f)
    {
        juce::Path trace;
        juce::Random random ((juce::int64) count * 7919 + 13);
        const auto baseY = view.getBottom() - view.getHeight() * 0.06f;
        trace.startNewSubPath (view.getX() + 8.0f, baseY);
        for (auto x = view.getX() + 8.0f; x < view.getRight() - 8.0f; x += 6.0f)
            trace.lineTo (x, baseY - (random.nextFloat() - 0.5f) * view.getHeight() * 0.08f * amount);
        g.setColour (juce::Colour (0xffff2fa8).withAlpha (0.7f));
        g.strokePath (trace, juce::PathStrokeType (1.2f));
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (settings.on ? stageOf (amount) : "OFF", bar, juce::Justification::centredLeft, false);
    g.drawText ("SLICE " + J::sliceNames[settings.slice], bar, juce::Justification::centred, false);
    g.drawText ("REACH " + J::reachNames[settings.reach], bar, juce::Justification::centredRight, false);
}

//==============================================================================
JackInPage::JackInPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::jackinOn, "ON", "JACK-IN on or off: something breaks into the sound and takes the phrase over.");
    make (controls, ParamIDs::jackinAmount, "AMOUNT", "How much of the phrase is taken (0: none, the sound untouched; at "
                                                       "full nearly every slice). The mod matrix can move it: FX Jack-In > Amount.");
    make (controls, ParamIDs::jackinSlice, "SLICE", "How the phrase is cut, in time with the song: eighths, sixteenths, "
                                                     "thirty-seconds or sixteenth triplets.");
    make (controls, ParamIDs::jackinReach, "REACH", "How far back a taken slice comes from: the last beat, two beats, bar "
                                                     "or two bars.");
    make (controls, ParamIDs::jackinChaos, "CHAOS", "What a taken slice becomes: low, mostly replays and stutters from whole "
                                                     "beats back (the rhythm holds); high, more reverses and octaves from "
                                                     "anywhere.");
    make (controls, ParamIDs::jackinCrush, "CRUSH", "How often a taken slice is also crushed (held samples, fewer levels), "
                                                     "and how hard.");
    make (controls, ParamIDs::jackinDrops, "DROPS", "How often a beat ends in a tape stop, the sound slowing to nothing.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> JackInPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void JackInPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("JACK-IN", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: something breaks into the sound and takes the phrase over", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void JackInPage::resized()
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
