#include "RunnerPage.h"

#include "dsp/synth/Runner.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pink (0xffff2d6e), red (0xffff2a2e), orange (0xffff6a24);
}

RunnerDisplay::RunnerDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The chase: every held note struck again in time with the song, faster with every fifth of AMOUNT. "
                "The corridor flashes with each strike; the gates along the bottom are the five speeds (the lit one is "
                "where AMOUNT is); over them, the beat cut into its strikes, the last one lit.");
    startTimerHz (30);
}

void RunnerDisplay::timerCallback()
{
    const auto strikes = processor.getRunnerStrikes();
    if (strikes != shownStrikes)
    {
        shownStrikes = strikes;
        flash = 1.0f;
    }
    else
        flash = juce::jmax (0.0f, flash - 0.12f);
    repaint();
}

void RunnerDisplay::paint (juce::Graphics& g)
{
    namespace run = synth::runner;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getRunnerSettings();
    const auto speed = settings.on ? settings.speed() : 0;
    const auto live = settings.playing();

    auto view = bounds.reduced (2.0f);
    g.setColour (juce::Colour (0xff070304));
    g.fillRoundedRectangle (view, 6.0f);

    // The top bar: the speed and the settings.
    auto bar = view.reduced (14.0f, 6.0f).removeFromTop (22.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    const auto perBeat = speed > 0 ? juce::roundToInt (1.0 / run::beatsPerStrike (speed)) : 0;
    g.drawText (! settings.on ? juce::String ("OFF") : speed == 0 ? juce::String ("STANDING STILL")
                                                                  : run::speedNames[speed] + "   " + juce::String (perBeat) + (perBeat == 1 ? " STRIKE A BEAT" : " STRIKES A BEAT"),
                bar, juce::Justification::centredLeft, false);
    g.drawText ("ACCENT " + juce::String (juce::roundToInt (100.0f * settings.accent)) + "%   LIFT "
                    + juce::String (juce::roundToInt (100.0f * settings.lift)) + "%", bar, juce::Justification::centredRight, false);

    // The corridor: stone walls running to a vanishing point, lit by the strikes.
    auto scene = view.reduced (14.0f, 0.0f).withTrimmedTop (32.0f).withTrimmedBottom (78.0f);
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (scene.toNearestInt());
        const auto vp = juce::Point<float> (scene.getCentreX(), scene.getY() + scene.getHeight() * 0.42f);
        const auto glow = live ? 0.25f + 0.75f * flash : 0.0f;
        // Floor and ceiling.
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff140809), vp.x, vp.y, juce::Colour (0xff050203), vp.x, scene.getBottom(), false));
        g.fillRect (scene);
        // The walls: each a trapezoid from the near edge to the vanishing point, blocks in rows.
        for (int side = 0; side < 2; ++side)
        {
            const auto nearX = side == 0 ? scene.getX() : scene.getRight();
            const auto farX = vp.x + (side == 0 ? -scene.getWidth() * 0.06f : scene.getWidth() * 0.06f);
            juce::Path wall;
            wall.startNewSubPath (nearX, scene.getY());
            wall.lineTo (farX, vp.y - scene.getHeight() * 0.12f);
            wall.lineTo (farX, vp.y + scene.getHeight() * 0.12f);
            wall.lineTo (nearX, scene.getBottom());
            wall.closeSubPath();
            g.setColour (juce::Colour (0xff1c1416));
            g.fillPath (wall);
            g.setColour (juce::Colour (0xff2e2427));
            for (int row = 1; row < 7; ++row)
            {
                const auto t = (float) row / 7.0f;
                const auto yNear = scene.getY() + scene.getHeight() * t;
                const auto yFar = vp.y - scene.getHeight() * 0.12f + scene.getHeight() * 0.24f * t;
                g.drawLine (nearX, yNear, farX, yFar, 1.0f);
            }
            // The light strips along each wall's foot and top, flashing.
            const auto colour = side == 0 ? pink : orange;
            for (auto [ya, yb] : { std::pair { scene.getY(), vp.y - scene.getHeight() * 0.12f }, { scene.getBottom(), vp.y + scene.getHeight() * 0.12f } })
            {
                for (auto [w, a] : { std::pair { 7.0f, 0.08f }, { 3.5f, 0.2f }, { 1.6f, 1.0f } })
                {
                    g.setColour (colour.withAlpha (a * (0.2f + 0.8f * glow)));
                    g.drawLine (nearX, ya, farX, yb, w);
                }
            }
        }
        // The light at the corridor's end: brighter the faster.
        const auto end = juce::Rectangle<float> (scene.getWidth() * 0.12f, scene.getHeight() * 0.24f).withCentre (vp);
        g.setGradientFill (juce::ColourGradient (red.withAlpha (0.15f + 0.12f * (float) speed + 0.3f * glow), vp.x, vp.y,
                                                 red.withAlpha (0.0f), vp.x + end.getWidth() * 1.6f, vp.y, true));
        g.fillEllipse (end.expanded (end.getWidth() * 0.9f));
        // Footfalls: dashes rushing toward you, as many as the strikes a beat.
        if (live)
            for (int k = 0; k < juce::jmin (perBeat, 16); ++k)
            {
                const auto t = std::fmod ((float) k / (float) juce::jmax (1, perBeat) + 0.35f * flash, 1.0f);
                const auto y = vp.y + (scene.getBottom() - vp.y) * t * t;
                const auto w = 6.0f + 60.0f * t * t;
                g.setColour (orange.withAlpha (0.15f + 0.6f * t));
                g.fillRoundedRectangle (juce::Rectangle<float> (w, 2.0f + 4.0f * t).withCentre ({ vp.x, y }), 2.0f);
            }
    }

    // The beat, cut into its strikes, the last one lit.
    auto lower = view.reduced (14.0f, 8.0f).removeFromBottom (70.0f);
    auto beatRow = lower.removeFromTop (22.0f);
    if (perBeat > 0)
    {
        const auto shown = juce::jmin (perBeat, 16);
        const auto cell = beatRow.getWidth() / (float) shown;
        const auto last = processor.getRunnerBeatPosition();
        for (int k = 0; k < shown; ++k)
        {
            const auto r = juce::Rectangle<float> (beatRow.getX() + (float) k * cell, beatRow.getY(), cell, beatRow.getHeight()).reduced (2.0f, 4.0f);
            const auto lit = live && last >= 0 && (perBeat <= 16 ? last == k : last * shown / perBeat == k);
            g.setColour (lit ? (k == 0 ? juce::Colours::white : orange) : juce::Colour (0xff2a2023));
            g.fillRoundedRectangle (r, 2.0f);
        }
    }

    // The five speeds, as gates; the fifths of the knob between them.
    lower.removeFromTop (8.0f);
    const auto gateWidth = lower.getWidth() / 5.0f;
    for (int k = 1; k <= 5; ++k)
    {
        const auto r = juce::Rectangle<float> (lower.getX() + (float) (k - 1) * gateWidth, lower.getY(), gateWidth, lower.getHeight()).reduced (4.0f, 2.0f);
        const auto on = k == speed;
        if (on)
        {
            juce::ColourGradient grad (pink, r.getX(), 0.0f, orange, r.getRight(), 0.0f, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 4.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff1a1315));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (k < speed ? orange.withAlpha (0.5f) : juce::Colour (0xff3a2e31));
            g.drawRoundedRectangle (r, 4.0f, 1.2f);
        }
        g.setColour (on ? juce::Colours::white : ParagonColours::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (run::speedNames[k], r.withTrimmedBottom (r.getHeight() * 0.4f), juce::Justification::centred, false);
        g.setFont (juce::FontOptions (9.5f));
        g.drawText (juce::String ((k - 1) * 20) + "-" + juce::String (k * 20) + "%", r.withTrimmedTop (r.getHeight() * 0.55f),
                    juce::Justification::centred, false);
    }
}

//==============================================================================
RunnerPage::RunnerPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::runnerOn, "ON", "RUNNER on or off: every held note struck again in time with the song, faster as AMOUNT turns.");
    make (controls, ParamIDs::runnerAmount, "AMOUNT", "How fast: off at 0, then a step every fifth of the knob: quarters, eighths, "
                                                      "sixteenths, 32nds, 64ths. The mod matrix can move it: FX Runner > Amount.");
    make (controls, ParamIDs::runnerAccent, "ACCENT", "How much softer the strikes off the beat are, so the beat stays felt at any speed.");
    make (controls, ParamIDs::runnerLift, "LIFT", "How far the sound dips just before each strike: high, every strike stands apart "
                                                  "(even on a held pad); low, they only push.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> RunnerPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void RunnerPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("RUNNER", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: every held note struck again in time with the song, faster with every fifth of the knob",
                juce::Rectangle<int> (24, 38, 480, 16), juce::Justification::centredLeft, false);
}

void RunnerPage::resized()
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
