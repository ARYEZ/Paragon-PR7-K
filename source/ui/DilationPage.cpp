#include "DilationPage.h"

#include "dsp/effects/Dilation.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr double turnSeconds = 8.0;   // the clock's turn: REACH's longest, so the gap never laps
}

DilationDisplay::DilationDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("Time: the dim hand keeps real time, the bright one is the sound you hear, trailing behind as it slows "
                "(ghost hands behind it the slower it goes). It can't fall further behind than REACH (the marked arc): "
                "there it fades back to the present.");
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

void DilationDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    realTime += juce::jlimit (0.0, 0.2, (now - lastTick) / 1000.0);
    lastTick = now;
    repaint();
}

void DilationDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff02060a));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getDilationSettings();
    const auto& fx = processor.getDilation();
    const auto speed = s.on ? fx.getSpeed() : 1.0f;
    const auto behind = s.on ? fx.getBehindSeconds() : 0.0f;
    const auto reach = effects::Dilation::reachSeconds (s.reach);
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    const auto semis = 12.0f * std::log2 (juce::jmax (0.01f, speed));
    g.drawText ("SPEED x" + juce::String (speed, 2) + "   " + (semis > 0.05f ? "+" : "") + juce::String (std::abs (semis) < 0.05f ? 0.0f : semis, 1)
                    + " ST   " + juce::String (behind, 2) + " S BEHIND   REACH " + juce::String (reach, 1) + " S   "
                    + effects::Dilation::returnNames[juce::jlimit (0, 1, s.returnMode)] + (fx.isCatchingUp() && s.on ? "   CATCHING UP" : ""),
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);

    const auto centre = area.getCentre();
    const auto radius = juce::jmin (area.getWidth() * 0.5f, area.getHeight() * 0.5f) - 4.0f;
    const auto angleOf = [] (double seconds) { return (float) (juce::MathConstants<double>::twoPi * std::fmod (seconds / turnSeconds, 1.0)); };
    const auto pointAt = [centre] (float angle, float r) { return centre + juce::Point<float> (r * std::sin (angle), -r * std::cos (angle)); };
    const auto lit = s.on ? 1.0f : 0.3f;
    const auto cool = juce::Colour (0xff7fe8ff);

    // The face: rings and ticks.
    g.setColour (juce::Colour (0xff0c2633));
    g.fillEllipse (centre.x - radius, centre.y - radius, 2.0f * radius, 2.0f * radius);
    g.setColour (cool.withAlpha (0.35f * lit));
    g.drawEllipse (centre.x - radius, centre.y - radius, 2.0f * radius, 2.0f * radius, 1.5f);
    for (int k = 0; k < 32; ++k)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) k / 32.0f;
        const auto inner = radius * (k % 4 == 0 ? 0.86f : 0.92f);
        g.drawLine ({ pointAt (a, inner), pointAt (a, radius * 0.97f) }, k % 4 == 0 ? 2.0f : 1.0f);
    }

    const auto real = angleOf (realTime);
    // REACH: the arc behind real time it can't fall past.
    {
        juce::Path arc;
        const auto span = juce::MathConstants<float>::twoPi * (float) (reach / turnSeconds);
        arc.addCentredArc (centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f, real - span, real, true);
        g.setColour (juce::Colour (0xffff5a4a).withAlpha (0.18f * lit));
        g.strokePath (arc, juce::PathStrokeType (radius * 0.08f));
        // How far behind now: the lit part of it.
        juce::Path gap;
        gap.addCentredArc (centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f,
                           real - juce::MathConstants<float>::twoPi * (float) (behind / turnSeconds), real, true);
        g.setColour (cool.withAlpha (0.55f * lit));
        g.strokePath (gap, juce::PathStrokeType (radius * 0.08f));
    }

    // Real time: the dim hand.
    g.setColour (juce::Colours::white.withAlpha (0.25f * lit));
    g.drawLine ({ centre, pointAt (real, radius * 0.9f) }, 2.0f);

    // The sound you hear: the bright hand, behind by how far it has fallen, with ghosts behind it as it slows.
    const auto heard = real - juce::MathConstants<float>::twoPi * (float) (behind / turnSeconds);
    const auto ghosts = juce::roundToInt (8.0f * (1.0f - juce::jmin (1.0f, speed)) / 0.75f);
    for (int k = ghosts; k >= 1; --k)
    {
        const auto a = heard - 0.06f * (float) k;
        g.setColour (cool.withAlpha (0.5f * lit * (1.0f - (float) k / (float) (ghosts + 1)) * 0.5f));
        g.drawLine ({ centre, pointAt (a, radius * 0.72f) }, 3.0f);
    }
    g.setColour (cool.withAlpha (lit));
    g.drawLine ({ centre, pointAt (heard, radius * 0.72f) }, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (lit));
    g.fillEllipse (centre.x - 6.0f, centre.y - 6.0f, 12.0f, 12.0f);

    // Labels either side of the clock.
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.45f * lit));
    g.drawText ("DIM HAND: REAL TIME", juce::Rectangle<float> (area.getX(), area.getBottom() - 34.0f, 240.0f, 16.0f), juce::Justification::centredLeft, false);
    g.setColour (cool.withAlpha (lit));
    g.drawText ("BRIGHT HAND: WHAT YOU HEAR", juce::Rectangle<float> (area.getX(), area.getBottom() - 16.0f, 240.0f, 16.0f), juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xffff7a6a).withAlpha (0.8f * lit));
    g.drawText ("RED ARC: REACH", juce::Rectangle<float> (area.getRight() - 240.0f, area.getBottom() - 16.0f, 240.0f, 16.0f), juce::Justification::centredRight, false);
}

//==============================================================================
DilationPage::DilationPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::dilationOn, "ON", "DILATION on or off: time slowing down. Off, it catches up first (by RETURN), then leaves your sound alone.");
    make (controls, ParamIDs::dilationSlow, "SLOW", "How slow: the sound played back slower, pitch and speed dropping together like a tape winding down (to a quarter speed, two octaves down). The mod matrix can move it: FX Dilation > Slow.");
    make (controls, ParamIDs::dilationRamp, "RAMP", "How long the slide into slow motion (and back) takes: a sudden lurch, or a slow wind-down.");
    make (controls, ParamIDs::dilationReturn, "RETURN", "How time catches up when SLOW comes back: GLIDE speeds up past real time (the pitch rises a little) until it's caught up; SNAP jumps straight back to the present.");
    make (controls, ParamIDs::dilationReach, "REACH", "How far behind real time it can fall (1 to 8 seconds). There it fades back to the present and starts slowing again.");
    make (controls, ParamIDs::dilationTone, "TONE", "Slowed tape loses its top: low, the slower it goes the darker it gets; high, no darkening. The mod matrix can move it.");
    make (controls, ParamIDs::dilationMix, "MIX", "The slowed sound against the sound as played (full: only the slowed one). The mod matrix can move it.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> DilationPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void DilationPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("DILATION", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: time slowing down, your sound dragged behind real time", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void DilationPage::resized()
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
