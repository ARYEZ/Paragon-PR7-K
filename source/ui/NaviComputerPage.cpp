#include "NaviComputerPage.h"

#include "dsp/synth/NaviComputer.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

NaviComputerDisplay::NaviComputerDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The pattern's 16 steps: how far each moves the notes (up above the line, down below, in steps of the "
                "scale), lit where AMOUNT lets them move, the step playing outlined.");
    startTimerHz (30);
}

namespace
{
    // A move as the musician would say it.
    juce::String moveName (int move7)
    {
        if (move7 == 0)
            return "ROOT";
        const auto sign = move7 > 0 ? "+" : "-";
        const auto m = std::abs (move7);
        if (m % 7 == 0)
            return juce::String (sign) + (m == 7 ? "8VA" : juce::String (m / 7) + " 8VA");
        static const char* degrees[] { "", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH" };
        return juce::String (sign) + (m > 7 ? "8VA " : "") + degrees[m % 7];
    }
}

void NaviComputerDisplay::paint (juce::Graphics& g)
{
    namespace navi = synth::navi;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getNaviSettings();
    const auto stepNow = settings.moving() ? processor.getNaviStep() : -1;
    const auto& pattern = navi::patterns[(size_t) settings.pattern];
    const auto green = juce::Colour (0xff7dff6a), cyan = juce::Colour (0xff27e0f0);

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050c0a), view.getX(), view.getY(),
                                             juce::Colour (0xff0a1a1e), view.getRight(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    // A faint grid, as a navigation screen's.
    g.setColour (green.withAlpha (0.06f));
    for (auto x = view.getX(); x < view.getRight(); x += 16.0f)
        g.drawVerticalLine ((int) x, view.getY(), view.getBottom());
    for (auto y = view.getY(); y < view.getBottom(); y += 16.0f)
        g.drawHorizontalLine ((int) y, view.getX(), view.getRight());

    auto area = view.reduced (view.getWidth() * 0.04f, view.getHeight() * 0.1f).withTrimmedTop (view.getHeight() * 0.08f);
    auto labels = area.removeFromBottom (18.0f);
    constexpr float span = 14.0f;   // the tallest move drawn (two octaves)
    const auto mid = area.getY() + area.getHeight() * (span / (span + 7.0f));
    const auto unit = (mid - area.getY()) / span;
    const auto cell = area.getWidth() / (float) navi::numSteps;
    g.setColour (green.withAlpha (0.45f));
    g.drawHorizontalLine ((int) mid, area.getX(), area.getRight());
    for (int k = 0; k < navi::numSteps; ++k)
    {
        const auto move = pattern[(size_t) k];
        const auto active = settings.on && navi::stepActive (k, settings.amount);
        auto column = juce::Rectangle<float> (area.getX() + (float) k * cell, area.getY(), cell, area.getHeight()).reduced (cell * 0.14f, 0.0f);
        const auto height = juce::jmax (3.0f, (float) std::abs (move) * unit);
        auto barBox = move >= 0 ? column.withTop (mid - height).withBottom (mid) : column.withTop (mid).withHeight (height);
        if (move == 0)
            barBox = column.withTop (mid - 1.5f).withHeight (3.0f);
        g.setColour (active ? green.interpolatedWith (cyan, (float) k / 15.0f) : juce::Colour (0xff1c2e2c));
        g.fillRect (barBox);
        if (k == stepNow)
        {
            g.setColour (juce::Colours::white);
            g.drawRect (column.expanded (2.0f, 0.0f), 1.5f);
        }
        g.setColour (active ? ParagonColours::text : ParagonColours::textDim.withAlpha (0.6f));
        g.setFont (juce::FontOptions (juce::jmin (9.5f, cell * 0.24f), juce::Font::bold));
        g.drawText (moveName (move), juce::Rectangle<float> (column.getX() - 4.0f, labels.getY(), column.getWidth() + 8.0f, labels.getHeight()),
                    juce::Justification::centred, false);
        if (k % 4 == 0)
        {
            g.setColour (green.withAlpha (0.3f));
            g.drawVerticalLine ((int) (area.getX() + (float) k * cell), area.getY(), area.getBottom());
        }
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    const auto moving = juce::jlimit (0, navi::numSteps, (int) std::floor (juce::jlimit (0.0f, 1.0f, settings.amount) * navi::numSteps + 1.0e-4f));
    g.drawText (! settings.on ? juce::String ("OFF") : navi::patternNames[settings.pattern] + "  " + juce::String (moving) + "/16 STEPS",
                bar, juce::Justification::centredLeft, false);
    g.drawText (synth::sirocco::keyNames[settings.key] + " " + synth::arp::scaleNames[settings.scale].toUpperCase(), bar, juce::Justification::centred, false);
    g.drawText (navi::divisionNames[settings.division] + (settings.chaos > 0.005f ? "   CHAOS " + juce::String (juce::roundToInt (100.0f * settings.chaos)) + "%" : juce::String()),
                bar, juce::Justification::centredRight, false);
}

//==============================================================================
NaviComputerPage::NaviComputerPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::naviOn, "ON", "NAVICOMPUTER on or off: every note moved by a pattern in key, in time with the song.");
    make (controls, ParamIDs::naviAmount, "AMOUNT", "How many of the 16 steps move the notes (0: none, the notes untouched; "
                                                     "a little: an occasional jump; full: the whole pattern). The mod matrix "
                                                     "can move it: FX Navicomputer > Amount.");
    make (controls, ParamIDs::naviPattern, "PATTERN", "The moves: up the chord, down it, fifths, octaves, a trill, a gallop, "
                                                       "a machine's line, a slow call.");
    make (controls, ParamIDs::naviDivision, "DIVISION", "How long each step is: sixteenths, eighths, sixteenth triplets or quarters.");
    make (controls, ParamIDs::naviKey, "KEY", "The key the moves keep to.");
    make (controls, ParamIDs::naviScale, "SCALE", "The scale the moves keep to: each note moves by its own steps of it, so a "
                                                   "chord stays in key.");
    make (controls, ParamIDs::naviChaos, "CHAOS", "That share of the steps given another move, a new choice every bar.");
    make (controls, ParamIDs::naviGlide, "GLIDE", "Slides between the moves rather than jumping.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> NaviComputerPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void NaviComputerPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("NAVICOMPUTER", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: every note moved by a pattern in key, in time with the song", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void NaviComputerPage::resized()
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
    SynthPanel::layoutRow (controlRow, row, 96);
}
