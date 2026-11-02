#include "AfterglowPage.h"

#include "dsp/effects/Afterglow.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // The cards' own characters: fixed, so the picture is steady (the notes themselves draw theirs as they start).
    constexpr float cardTune[] = { 0.62f, -0.35f, 0.08f, -0.91f, 0.44f, -0.12f, 0.83f, -0.57f };
    constexpr float cardBright[] = { -0.30f, 0.72f, -0.64f, 0.18f, 0.95f, -0.85f, 0.40f, 0.05f };
    constexpr float cardSway[] = { 0.13f, 0.21f, 0.17f, 0.29f, 0.11f, 0.25f, 0.19f, 0.15f };   // Hz
}

AfterglowDisplay::AfterglowDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("Left: eight voice cards, as an old polysynth's: AGE sets each at its own tuning (the needle) and "
                "brightness (the glow), DRIFT sways them slowly. A picture of how far apart they're set, not the notes "
                "playing. Right: the ENSEMBLE's three voices sweeping round, and the TAPE's wow in cents.");
    startTimerHz (30);
}

void AfterglowDisplay::timerCallback()
{
    time += 1.0 / 30.0;
    repaint();
}

void AfterglowDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    // Dusk: deep violet to a warm horizon.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0614), bounds.getX(), bounds.getY(),
                                             juce::Colour (0xff2a0c22), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto& state = processor.getParameters();
    const auto value = [&state] (const char* id) { return state.getRawParameterValue (id)->load(); };
    const auto on = value (ParamIDs::afterglowOn) > 0.5f;
    const auto age = value (ParamIDs::afterglowAge), drift = value (ParamIDs::afterglowDrift);
    const auto& fx = processor.getAfterglow();
    auto area = bounds.reduced (24.0f, 18.0f);

    // Across the top: what it's set to.
    g.setColour (on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("AGE " + juce::String (juce::roundToInt (100.0f * age)) + "%  (+-" + juce::String (juce::roundToInt (age * synth::Settings::Afterglow::ageCents))
                    + " CENTS)   DRIFT " + juce::String (juce::roundToInt (100.0f * drift)) + "%",
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (12.0f);

    auto cards = area.removeFromLeft (area.getWidth() * 0.58f);
    area.removeFromLeft (24.0f);

    // The voice cards.
    const auto gap = 10.0f;
    const auto cardWidth = (cards.getWidth() - 7.0f * gap) / 8.0f;
    const auto range = synth::Settings::Afterglow::ageCents + synth::Settings::Afterglow::driftCents;
    for (int k = 0; k < 8; ++k)
    {
        auto card = juce::Rectangle<float> (cards.getX() + (float) k * (cardWidth + gap), cards.getY(), cardWidth, cards.getHeight());
        auto label = card.removeFromBottom (22.0f);
        const auto sway = (float) std::sin (juce::MathConstants<double>::twoPi * cardSway[k] * time + k);
        const auto cents = on ? age * synth::Settings::Afterglow::ageCents * cardTune[k] + drift * synth::Settings::Afterglow::driftCents * sway : 0.0f;
        const auto bright = on ? juce::jlimit (-1.0f, 1.0f, age * cardBright[k] + 0.6f * drift * sway) : 0.0f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (card, 4.0f);
        // The glow: warmer and taller for a brighter card.
        const auto glow = card.reduced (5.0f).withTrimmedTop ((0.55f - 0.4f * bright) * card.getHeight());
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff5fa8).withAlpha (on ? 0.9f : 0.25f), glow.getX(), glow.getY(),
                                                 juce::Colour (0xff3a1a8a).withAlpha (on ? 0.9f : 0.25f), glow.getX(), glow.getBottom(), false));
        g.fillRoundedRectangle (glow, 3.0f);
        // The needle: in tune at the middle.
        const auto x = card.getCentreX() + card.getWidth() * 0.45f * juce::jlimit (-1.0f, 1.0f, cents / range);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawVerticalLine ((int) card.getCentreX(), card.getY() + 4.0f, card.getY() + 18.0f);
        g.setColour (juce::Colour (0xfffff1a8));
        g.fillRoundedRectangle (x - 2.0f, card.getY() + 3.0f, 4.0f, 18.0f, 2.0f);
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ((cents >= 0.0f ? "+" : "") + juce::String (cents, 1), label, juce::Justification::centred, false);
    }

    // The ensemble: three voices round a circle, at their delays now.
    auto right = area;
    auto circle = right.removeFromTop (right.getHeight() * 0.62f);
    const auto ensemble = fx.getEnsembleNow(), tape = fx.getTapeNow();
    const auto centre = circle.getCentre();
    const auto radius = juce::jmin (circle.getWidth(), circle.getHeight()) * 0.38f;
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawEllipse (centre.x - radius, centre.y - radius, 2.0f * radius, 2.0f * radius, 1.0f);
    for (int v = 0; v < 3; ++v)
    {
        const auto delay = fx.getVoiceDelayMs (v);
        const auto swing = (float) ((delay - effects::Afterglow::baseDelayMs) / (effects::Afterglow::sweepMs + effects::Afterglow::shimmerMs));
        const auto angle = juce::MathConstants<float>::twoPi * (float) v / 3.0f + 0.6f * swing;
        const auto r = radius * (0.75f + 0.25f * swing);
        const auto dot = juce::Point<float> (centre.x + r * std::sin (angle), centre.y - r * std::cos (angle));
        g.setColour (juce::Colour (0xff7ee8ff).withAlpha (0.25f + 0.75f * ensemble));
        g.fillEllipse (dot.x - 7.0f, dot.y - 7.0f, 14.0f, 14.0f);
    }
    g.setColour (ensemble > 0.0f ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("ENSEMBLE " + juce::String (juce::roundToInt (100.0f * ensemble)) + "%", circle.removeFromBottom (18.0f), juce::Justification::centred, false);

    // The tape: the wow now, in cents, as a needle along a line.
    right.removeFromTop (10.0f);
    auto meter = right.removeFromTop (20.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (meter, 4.0f);
    const auto wow = fx.getWowCents();
    const auto x = meter.getCentreX() + meter.getWidth() * 0.45f * juce::jlimit (-1.0f, 1.0f, wow / 12.0f);
    g.setColour (juce::Colour (0xffffb86a));
    g.fillRoundedRectangle (x - 2.0f, meter.getY() + 2.0f, 4.0f, meter.getHeight() - 4.0f, 2.0f);
    g.setColour (tape > 0.0f ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("TAPE " + juce::String (juce::roundToInt (100.0f * tape)) + "%   WOW " + juce::String (wow, 1) + " CENTS",
                right.removeFromTop (22.0f), juce::Justification::centred, false);
}

//==============================================================================
AfterglowPage::AfterglowPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::afterglowOn, "ON", "AFTERGLOW on or off: the glow of an old polysynth, in the notes and after them.");
    make (controls, ParamIDs::afterglowAge, "AGE", "Each note its own tuning, brightness and timing, as an old polysynth's voices: from all alike to "
                                                   "far apart. The mod matrix can move it: FX Afterglow > Age.");
    make (controls, ParamIDs::afterglowDrift, "DRIFT", "Each note's tuning and brightness sway slowly, differently from the others'. The mod matrix can move it.");
    make (controls, ParamIDs::afterglowEnsemble, "ENSEMBLE", "Three chorus voices sweeping round: the lush, wide shimmer of the era's string machines. The mod matrix can move it.");
    make (controls, ParamIDs::afterglowTape, "TAPE", "As if it all went down to tape: a slow pitch sway, warm saturation, a softer top. The mod matrix can move it.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> AfterglowPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void AfterglowPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("AFTERGLOW", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: an old polysynth's glow, each note its own, then ensemble and tape", juce::Rectangle<int> (24, 38, 420, 16),
                juce::Justification::centredLeft, false);
}

void AfterglowPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (440);
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
