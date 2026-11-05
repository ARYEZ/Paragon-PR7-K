#include "LightSpeedPage.h"

#include "dsp/effects/LightSpeed.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

LightSpeedDisplay::LightSpeedDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The view from the cockpit: the stars stretch into streaks as the jump spools up (JUMP, RISE), and the "
                "glow is the whoosh rushing past.");
    for (int k = 0; k < 140; ++k)
        stars.push_back ({ random.nextFloat() * juce::MathConstants<float>::twoPi, random.nextFloat(), 0.4f + random.nextFloat() });
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

void LightSpeedDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = (float) juce::jlimit (0.0, 0.2, (now - lastTick) / 1000.0);
    lastTick = now;
    const auto spool = processor.getLightSpeedSettings().on ? processor.getLightSpeed().getSpool() : 0.0f;
    // Stars drift out from the centre, faster the further into the jump.
    for (auto& s : stars)
    {
        s.distance += dt * s.speed * (0.04f + 2.2f * spool) * (0.3f + s.distance);
        if (s.distance > 1.2f)
        {
            s.distance = 0.02f + 0.1f * random.nextFloat();
            s.angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        }
    }
    repaint();
}

void LightSpeedDisplay::paint (juce::Graphics& g)
{
    using L = effects::LightSpeed;
    const auto bounds = getLocalBounds().toFloat();
    // The jump's own glide is smooth already: drawn from it directly (the timer only moves the stars).
    const auto live = processor.getLightSpeedSettings().on;
    const auto spool = live ? processor.getLightSpeed().getSpool() : 0.0f;
    const auto whoosh = live ? juce::jlimit (0.0f, 1.0f, (processor.getLightSpeed().getWhooshDb() + 60.0f) / 50.0f) : 0.0f;
    g.setColour (juce::Colour (0xff04060c));
    g.fillRoundedRectangle (bounds, 6.0f);
    auto view = bounds.reduced (10.0f).withTrimmedTop (34.0f);
    const auto centre = view.getCentre();
    const auto reach = 0.5f * std::hypot (view.getWidth(), view.getHeight());

    // The whoosh: a pale blue glow from the centre.
    if (whoosh > 0.01f)
    {
        juce::ColourGradient glowFill (juce::Colour (0xff8fd0ff).withAlpha (0.28f * whoosh), centre,
                                       juce::Colours::transparentBlack, centre.translated (reach * 0.8f, 0.0f), true);
        g.setGradientFill (glowFill);
        g.fillRect (view);
    }
    // The stars, each a streak as long as the jump is far along.
    g.saveState();
    g.reduceClipRegion (view.toNearestInt());
    for (const auto& s : stars)
    {
        const auto direction = juce::Point<float> (std::cos (s.angle), std::sin (s.angle));
        const auto head = centre + direction * (s.distance * reach);
        const auto length = (0.004f + 0.7f * std::pow (spool, 1.3f)) * reach * (0.3f + s.distance);
        const auto tail = head - direction * length;
        const auto bright = juce::jlimit (0.15f, 1.0f, 0.25f + s.distance);
        g.setColour (juce::Colour (0xffdcecff).withAlpha (bright));
        g.drawLine ({ tail, head }, spool > 0.05f ? 1.2f + spool : 1.6f);
    }
    g.restoreState();

    // The readings across the top.
    const auto settings = processor.getLightSpeedSettings();
    const auto& fx = processor.getLightSpeed();
    g.setColour (ParagonColours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    auto bar = bounds.reduced (14.0f, 8.0f).removeFromTop (22.0f);
    g.drawText ("JUMP " + juce::String (juce::roundToInt (100.0f * (settings.on ? fx.getSpool() : 0.0f))) + "%", bar, juce::Justification::centredLeft, false);
    g.drawText ("STREAKS +" + juce::String (settings.on ? fx.getPitchSemitones() : 0.0f, 1) + " st  (TOP AT FULL +"
                    + juce::String (L::riseSemitones (settings.rise), 0) + ")",
                bar, juce::Justification::centred, false);
    g.drawText ("WHOOSH " + (settings.on && fx.getWhooshDb() > -90.0f ? juce::String (juce::roundToInt (fx.getWhooshDb())) + " dB" : juce::String ("-")),
                bar, juce::Justification::centredRight, false);
}

//==============================================================================
LightSpeedPage::LightSpeedPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::jumpOn, "ON", "LIGHTSPEED on or off: the jump to lightspeed.");
    make (controls, ParamIDs::jumpAmount, "JUMP", "How far into the jump: 0 at rest (your sound untouched), all the way up at lightspeed. "
                                                  "The mod matrix can move it: FX Lightspeed > Jump.");
    make (controls, ParamIDs::jumpRise, "RISE", "How high the streaks climb at full JUMP: the top one up to two octaves. The mod matrix can move it.");
    make (controls, ParamIDs::jumpStreak, "STREAKS", "How loud the streaks are: the sound stretched and pitched up past you.");
    make (controls, ParamIDs::jumpWhoosh, "WHOOSH", "How loud the rushing air is (it follows your music, sweeping up as you jump). The mod matrix can move it.");
    make (controls, ParamIDs::jumpWidth, "WIDTH", "How far the streaks fly out to the sides.");
    make (controls, ParamIDs::jumpTone, "TONE", "How bright the streaks are: 2 to 16 kHz, brighter still as the jump goes on.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> LightSpeedPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void LightSpeedPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("LIGHTSPEED", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the jump to lightspeed, streaks and rushing air", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void LightSpeedPage::resized()
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
