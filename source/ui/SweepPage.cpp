#include "SweepPage.h"

#include "dsp/effects/Sweep.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

#include <complex>

AnalogSweepDisplay::AnalogSweepDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The sweep's path through two cycles (the cutoff, low at the foot, high at the top) with the cutoff now "
                "across it; on the right the filter's response now, its resonance rising with the sweep.");
    startTimerHz (30);
}

namespace
{
    // 20 Hz to 20 kHz on a log scale, 0 at the foot.
    float heightOf (float hz) noexcept { return std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f); }

    // The filter's response (dB) at `hz` with the cutoff at `cutoff` and the resonant stage's k (analogue prototypes).
    float responseDb (float hz, float cutoff, float k, bool bandPass) noexcept
    {
        const std::complex<float> s (0.0f, hz / cutoff);
        const auto resonant = 1.0f / (s * s + k * s + 1.0f);
        const auto h = bandPass ? k * s * resonant : resonant / (s * s + 1.41421356f * s + 1.0f);
        return juce::Decibels::gainToDecibels (std::abs (h), -60.0f);
    }
}

void AnalogSweepDisplay::paint (juce::Graphics& g)
{
    using W = effects::Sweep;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getSweepSettings();
    const auto& fx = processor.getSweep();
    const auto live = settings.on && ! fx.isResting();
    const auto amount = live ? fx.getAmount() : 0.0f;
    const auto amber = juce::Colour (0xffffa53a), orange = juce::Colour (0xffff5a2a), teal = juce::Colour (0xff3ad6c8);

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff120a06), view.getX(), view.getY(),
                                             juce::Colour (0xff06100f), view.getRight(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    auto inner = view.reduced (view.getWidth() * 0.03f, view.getHeight() * 0.12f).withTrimmedTop (view.getHeight() * 0.05f);
    auto pathArea = inner.removeFromLeft (inner.getWidth() * 0.64f);
    inner.removeFromLeft (inner.getWidth() * 0.06f);
    auto responseArea = inner;

    // Octave lines, 20 Hz to 20 kHz.
    for (auto* area : { &pathArea, &responseArea })
    {
        g.setColour (juce::Colour (0xff2a2016));
        for (float hz = 40.0f; hz < 20000.0f; hz *= 2.0f)
        {
            const auto y = area->getBottom() - heightOf (hz) * area->getHeight();
            g.drawHorizontalLine ((int) y, area->getX(), area->getRight());
        }
        g.setColour (juce::Colour (0xff3b2c1c));
        g.drawRect (*area, 1.0f);
    }

    // The sweep's path through two cycles (UP-DOWN: two ups and two downs).
    {
        const auto cycleBeats = W::lengthBeats[(size_t) settings.length] * (settings.shape == 2 ? 2.0 : 1.0);
        juce::Path path;
        constexpr int points = 240;
        for (int k = 0; k <= points; ++k)
        {
            const auto beats = 2.0 * cycleBeats * k / points;
            const auto hz = W::sweepHz (W::positionAt (beats, settings.shape, settings.length), settings);
            const juce::Point<float> at (pathArea.getX() + pathArea.getWidth() * (float) k / points,
                                         pathArea.getBottom() - heightOf (hz) * pathArea.getHeight());
            if (k == 0)
                path.startNewSubPath (at);
            else
                path.lineTo (at);
        }
        g.setColour (amber.withAlpha (settings.on ? 0.85f : 0.3f));
        g.strokePath (path, juce::PathStrokeType (2.0f));
        // Bar lines.
        g.setColour (amber.withAlpha (0.15f));
        const auto bars = 2.0 * cycleBeats / 4.0;
        for (int b = 1; b < (int) bars; ++b)
        {
            const auto x = pathArea.getX() + pathArea.getWidth() * (float) b / (float) bars;
            g.drawVerticalLine ((int) x, pathArea.getY(), pathArea.getBottom());
        }
    }

    // The cutoff now, across the path.
    const auto cutoff = live ? fx.getCutoffHz() : 20000.0f;
    {
        const auto y = pathArea.getBottom() - heightOf (cutoff) * pathArea.getHeight();
        g.setColour (orange.withAlpha (0.35f + 0.65f * amount));
        g.drawLine (pathArea.getX(), y, pathArea.getRight(), y, 2.0f);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (cutoff >= 1000.0f ? juce::String (cutoff / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (cutoff)) + " Hz",
                    juce::Rectangle<float> (pathArea.getX() + 4.0f, y - 14.0f, 80.0f, 12.0f), juce::Justification::centredLeft, false);
    }

    // The response now, frequency up the side so it lines up with the path.
    {
        const auto resonance = settings.resonance * (0.3f + 0.7f * (live ? fx.getPosition() : 0.0f)) * amount;
        const auto k = 2.0f - 1.9f * resonance;
        juce::Path curve;
        constexpr int points = 160;
        for (int i = 0; i <= points; ++i)
        {
            const auto height = (float) i / points;
            const auto hz = 20.0f * std::pow (1000.0f, height);
            const auto db = juce::jlimit (-48.0f, 24.0f, responseDb (hz, cutoff, k, settings.filter == 1));
            const juce::Point<float> at (responseArea.getX() + responseArea.getWidth() * (db + 48.0f) / 72.0f,
                                         responseArea.getBottom() - height * responseArea.getHeight());
            if (i == 0)
                curve.startNewSubPath (at);
            else
                curve.lineTo (at);
        }
        g.setColour (teal.withAlpha (settings.on ? 0.9f : 0.3f));
        g.strokePath (curve, juce::PathStrokeType (1.8f));
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("RESPONSE", responseArea.withHeight (12.0f).translated (0.0f, -14.0f), juce::Justification::centredLeft, false);
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (! settings.on ? juce::String ("OFF") : W::shapeNames[settings.shape] + " OVER " + W::lengthNames[settings.length],
                bar, juce::Justification::centredLeft, false);
    g.drawText (W::filterNames[settings.filter] + "   RES " + juce::String (juce::roundToInt (100.0f * settings.resonance)) + "%",
                bar, juce::Justification::centred, false);
    g.drawText ("LEVEL KEPT " + juce::String (live ? fx.getGainDb() : 0.0f, 1) + " dB", bar, juce::Justification::centredRight, false);
}

//==============================================================================
SweepPage::SweepPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::sweepOn, "ON", "SWEEP on or off: a resonant analogue sweep locked to the song.");
    make (controls, ParamIDs::sweepAmount, "AMOUNT", "How much: the filter comes in from fully open to the sweep, the "
                                                      "resonance and phaser with it (0: the sound untouched). The mod matrix "
                                                      "can move it: FX Sweep > Amount.");
    make (controls, ParamIDs::sweepLength, "LENGTH", "How long one sweep takes: half a bar to eight bars, in time with the song.");
    make (controls, ParamIDs::sweepShape, "SHAPE", "UP rises and starts again; DOWN falls; UP-DOWN rises over LENGTH and falls "
                                                    "over the next.");
    make (controls, ParamIDs::sweepFilter, "FILTER", "LOW-PASS (24 dB, the classic sweep) or BAND-PASS (a narrower, vocal sweep).");
    make (controls, ParamIDs::sweepCenter, "CENTER", "The middle of the sweep: 150 Hz to 4 kHz.");
    make (controls, ParamIDs::sweepRange, "RANGE", "How wide the sweep goes: 2 to 9 octaves.");
    make (controls, ParamIDs::sweepResonance, "RESONANCE", "How much the filter sings, rising with the sweep to all of it at "
                                                            "the top.");
    make (controls, ParamIDs::sweepPhaser, "PHASER", "A phaser riding the sweep, its notches following the cutoff.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> SweepPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void SweepPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("SWEEP", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: a resonant analogue sweep locked to the song", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void SweepPage::resized()
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
