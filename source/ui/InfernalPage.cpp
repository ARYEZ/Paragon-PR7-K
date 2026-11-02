#include "InfernalPage.h"

#include "dsp/effects/Infernal.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

InfernalDisplay::InfernalDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The four chains side by side, each lit to its level now: FUZZ, CRUSH, AMP + CAB, NOISE (pitched to "
                "your notes). Under TIGHT the lows stay clean; the bloom is FEEDBACK swelling on held notes.");
    startTimerHz (30);
}

void InfernalDisplay::timerCallback()
{
    const auto& fx = processor.getInfernal();
    for (int k = 0; k < 4; ++k)
        held[(size_t) k] = juce::jmax (fx.getChainDb (k), held[(size_t) k] - 1.5f);   // falls back about 45 dB a second
    bloom = fx.getBloom();
    reduction = fx.getGainReductionDb();
    repaint();
}

void InfernalDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    // Embers: near black, warming toward the bottom.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff080405), bounds.getX(), bounds.getY(),
                                             juce::Colour (0xff1c0806), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    const auto s = processor.getInfernalSettings();
    auto area = bounds.reduced (24.0f, 18.0f);

    // At the top: how the sound is split.
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("CLEAN UNDER " + juce::String (juce::roundToInt (effects::Infernal::tightHz (s.tight))) + " HZ   |   DESTROYED OVER IT  +"
                    + juce::String (juce::roundToInt (42.0f * s.drive)) + " DB",
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("MIX " + juce::String (juce::roundToInt (100.0f * s.mix)) + "%   GLUE " + juce::String (reduction, 1) + " DB   BLOOM "
                    + juce::String (juce::roundToInt (100.0f * bloom)) + "%",
                area.removeFromTop (18.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (14.0f);

    // The four chains, side by side: a bar each from -48 dB to +6 dB.
    const char* names[] = { "FUZZ", "CRUSH", "AMP + CAB", "NOISE" };
    const float blends[] = { s.fuzz, s.crush, s.amp, s.noise };
    const auto gap = 18.0f;
    const auto columnWidth = (area.getWidth() - 3.0f * gap) / 4.0f;
    for (int k = 0; k < 4; ++k)
    {
        auto column = juce::Rectangle<float> (area.getX() + (float) k * (columnWidth + gap), area.getY(), columnWidth, area.getHeight());
        auto label = column.removeFromBottom (40.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (column, 4.0f);
        const auto fill = juce::jlimit (0.0f, 1.0f, (held[(size_t) k] + 48.0f) / 54.0f);
        const auto lit = column.reduced (6.0f).withTrimmedTop ((1.0f - fill) * (column.getHeight() - 12.0f));
        if (fill > 0.0f && s.on)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd04a), lit.getX(), lit.getY(),
                                                     juce::Colour (0xffd8241c), lit.getX(), column.getBottom(), false));
            g.fillRoundedRectangle (lit, 3.0f);
        }
        g.setColour (blends[k] > 0.0f && s.on ? ParagonColours::accent : ParagonColours::textDim);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (names[k], label.removeFromTop (20.0f), juce::Justification::centred, false);
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("BLEND " + juce::String (juce::roundToInt (100.0f * blends[k])) + "%", label, juce::Justification::centred, false);
    }
}

//==============================================================================
InfernalPage::InfernalPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::infernalOn, "ON", "INFERNAL on or off: your sound through four chains of destruction at once.");
    make (controls, ParamIDs::infernalDrive, "DRIVE", "How hard every chain is pushed (0 to +42 dB). The mod matrix can move it: FX Infernal > Drive.");
    make (controls, ParamIDs::infernalTight, "TIGHT", "Under this the lows stay clean (the sub under the dirt); over it, destroyed.");
    make (controls, ParamIDs::infernalFuzz, "FUZZ", "How much of the hard, buzzing fuzz.");
    make (controls, ParamIDs::infernalCrush, "CRUSH", "How much of the bit and sample-rate destruction.");
    make (controls, ParamIDs::infernalAmp, "AMP", "How much of the guitar amp into a speaker cabinet: what makes a lead sound played on a guitar.");
    make (controls, ParamIDs::infernalNoise, "NOISE", "How much noise, pulsed at the pitch of your notes so it growls in tune with the riff.");
    make (controls, ParamIDs::infernalFeedback, "FEEDBACK", "The AMP's bloom: an octave over held notes swelling in, as a guitar feeds back.");
    make (controls, ParamIDs::infernalCab, "CAB", "The cabinet: darker to brighter.");
    make (controls, ParamIDs::infernalMix, "MIX", "The destroyed sound against the dry (0: the dry sound exactly). The mod matrix can move it.");
    make (controls, ParamIDs::infernalLevel, "LEVEL", "How loud the destroyed part is.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> InfernalPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void InfernalPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("INFERNAL", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: four chains of destruction at once, before the sends", juce::Rectangle<int> (24, 38, 360, 16),
                juce::Justification::centredLeft, false);
}

void InfernalPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (390);
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
