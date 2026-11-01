#include "CracklePage.h"

#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

CrackleDisplay::CrackleDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The counter: every click in the last 2.5 seconds, a bar a 20 ms slice (newest at the right), and how "
                "many came in the last second.");
    startTimerHz (25);
}

void CrackleDisplay::timerCallback()
{
    shown = processor.getCrackle().getHistory();
    rate = processor.getCrackle().getRecentRate();
    repaint();
}

void CrackleDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff070507));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    auto area = bounds.reduced (24.0f, 20.0f);
    const auto on = processor.getCrackleSettings().on;

    // The count, big, at the left: clicks in the last second.
    auto reading = area.removeFromLeft (juce::jmin (260.0f, area.getWidth() * 0.3f));
    g.setColour (on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (reading.getHeight() * 0.42f, juce::Font::bold));
    g.drawText (juce::String (juce::roundToInt (rate)), reading.removeFromTop (reading.getHeight() * 0.62f), juce::Justification::centredBottom, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("CLICKS A SECOND", reading.removeFromTop (20.0f), juce::Justification::centred, false);
    area.removeFromLeft (20.0f);

    // The slices: a bar each, its height the clicks in it (on a square-root scale, so single clicks still show).
    auto plot = area.withTrimmedBottom (18.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawHorizontalLine (juce::roundToInt (plot.getBottom()), plot.getX(), plot.getRight());
    int most = 4;
    for (const auto c : shown)
        most = juce::jmax (most, c);
    const auto n = (int) shown.size();
    const auto barWidth = plot.getWidth() / (float) n;
    for (int k = 0; k < n; ++k)
    {
        if (shown[(size_t) k] == 0)
            continue;
        const auto h = plot.getHeight() * std::sqrt ((float) shown[(size_t) k] / (float) most);
        const auto x = plot.getX() + (float) k * barWidth;
        g.setColour ((on ? ParagonColours::accent : ParagonColours::textDim).withAlpha (k == n - 1 ? 0.5f : 0.9f));
        g.fillRect (juce::Rectangle<float> (x + 0.5f, plot.getBottom() - h, juce::jmax (1.0f, barWidth - 1.0f), h));
    }
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("2.5 S AGO", juce::Rectangle<float> (plot.getX(), plot.getBottom() + 4.0f, 100.0f, 14.0f), juce::Justification::centredLeft, false);
    g.drawText ("NOW", juce::Rectangle<float> (plot.getRight() - 60.0f, plot.getBottom() + 4.0f, 60.0f, 14.0f), juce::Justification::centredRight, false);
}

//==============================================================================
CracklePage::CracklePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::crackleOn, "ON", "CRACKLE on or off: a radiation counter's random clicks, added to the sound.");
    make (controls, ParamIDs::crackleRate, "RATE", "How many clicks a second, on average. They fall at random, as a counter's do: never a rhythm "
                                                   "(the mod matrix can move it: FX Crackle > Rate).");
    make (controls, ParamIDs::crackleCluster, "CLUSTER", "How much the clicks bunch into bursts and lulls (the rate wandering up and down, "
                                                         "up to about 12 times either way).");
    make (controls, ParamIDs::crackleTone, "TONE", "Each click's pitch: a dull tick low, a sharp snap high.");
    make (controls, ParamIDs::crackleWidth, "WIDTH", "How far the clicks are spread across left and right (each its own place).");
    make (controls, ParamIDs::crackleFollow, "FOLLOW", "How much the clicks follow the sound: all the way up, they come only while it plays (and "
                                                       "as fast as it's loud), so it still falls silent; at 0 they never stop.");
    make (controls, ParamIDs::crackleLevel, "LEVEL", "How loud the clicks are. The sound passes under them unchanged.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> CracklePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void CracklePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("CRACKLE", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Insert 5: after ECHOES", juce::Rectangle<int> (24, 38, 200, 16), juce::Justification::centredLeft, false);
}

void CracklePage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (190);
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
