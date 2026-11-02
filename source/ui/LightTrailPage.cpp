#include "LightTrailPage.h"

#include "dsp/effects/LightTrail.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

LightTrailDisplay::LightTrailDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The trail as it's set: each repeat a light, further along in time, up or down by RISE, dimmer as it "
                "fades (LENGTH), swinging side to side with SPREAD; cooler as TONE darkens it.");
    startTimerHz (30);
}

void LightTrailDisplay::timerCallback()
{
    level = juce::jmax (processor.getLightTrail().getTrailDb(), level - 1.5f);
    repaint();
}

void LightTrailDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    // A dark grid, as a game board seen from above.
    g.setColour (juce::Colour (0xff03070c));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff0d2a3a));
    for (auto x = bounds.getX() + 24.0f; x < bounds.getRight(); x += 32.0f)
        g.drawVerticalLine ((int) x, bounds.getY() + 2.0f, bounds.getBottom() - 2.0f);
    for (auto y = bounds.getY() + 24.0f; y < bounds.getBottom(); y += 32.0f)
        g.drawHorizontalLine ((int) y, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getLightTrailSettings();
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText (effects::LightTrail::divisionNames[s.division] + "   " + (s.riseSemitones > 0.0f ? "+" : "") + juce::String (s.riseSemitones, 1)
                    + " ST A REPEAT   MIX " + juce::String (juce::roundToInt (100.0f * s.mix)) + "%   TRAIL "
                    + (level > -90.0f ? juce::String (level, 1) + " DB" : juce::String ("--")),
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (10.0f);

    // The repeats: up to 24, while they're louder than -60 dB.
    const auto feedback = effects::LightTrail::feedbackFor (s.length);
    const auto bright = juce::jlimit (0.0f, 1.0f, s.tone);
    const auto midY = area.getCentreY();
    const auto stepX = area.getWidth() / 25.0f;
    // How many repeats show (while louder than -60 dB, at most 24), and a semitone's height so the whole trail fits.
    auto shown = 0;
    for (auto g0 = feedback; shown < 24 && g0 >= 0.001f; g0 *= feedback)
        ++shown;
    const auto reach = juce::jmax (12.0f, std::abs (s.riseSemitones) * (float) juce::jmax (1, shown));
    const auto pitchY = area.getHeight() * 0.45f / reach;
    juce::Path trail;
    trail.startNewSubPath (area.getX(), midY);
    auto gain = 1.0f;
    for (int k = 1; k <= 24; ++k)
    {
        gain *= feedback;
        if (gain < 0.001f)
            break;
        const auto semis = s.riseSemitones * (float) k;
        const auto swing = (k % 2 == 1 ? 1.0f : -1.0f) * s.spread * 6.0f;
        const auto x = area.getX() + (float) k * stepX;
        const auto y = midY - semis * pitchY + swing;
        trail.lineTo (x, y);
        const auto colour = juce::Colour (0xff3ef0ff).interpolatedWith (juce::Colour (0xffff8a3a), 1.0f - bright)
                                .withAlpha (s.on ? juce::jlimit (0.1f, 1.0f, gain) : 0.2f);
        const auto r = 4.0f + 6.0f * gain;
        g.setColour (colour.withMultipliedAlpha (0.35f));
        g.fillEllipse (x - 2.0f * r, y - 2.0f * r, 4.0f * r, 4.0f * r);
        g.setColour (colour);
        g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
    }
    g.setColour (juce::Colour (0xff3ef0ff).withAlpha (s.on ? 0.5f : 0.15f));
    g.strokePath (trail, juce::PathStrokeType (2.0f));
    // The note itself, where the trail starts.
    g.setColour (juce::Colours::white.withAlpha (s.on ? 0.9f : 0.3f));
    g.fillEllipse (area.getX() - 7.0f, midY - 7.0f, 14.0f, 14.0f);
}

//==============================================================================
LightTrailPage::LightTrailPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::trailOn, "ON", "LIGHTTRAIL on or off: every note leaves a glowing trail behind it.");
    make (controls, ParamIDs::trailDivision, "DIVISION", "How far apart the repeats come, in time with your song.");
    make (controls, ParamIDs::trailLength, "LENGTH", "How long the trail lasts: from a short flick to a long streak. The mod matrix can move it: FX Lighttrail > Length.");
    make (controls, ParamIDs::trailRise, "RISE", "How far each repeat steps in pitch: up, the trail climbs; down, it falls away; 0, it stays on the note. The mod matrix can move it.");
    make (controls, ParamIDs::trailTone, "TONE", "Each repeat darker (low) or keeping its shine (high). The mod matrix can move it.");
    make (controls, ParamIDs::trailSpread, "SPREAD", "How far each repeat crosses to the other side, so the trail swings across.");
    make (controls, ParamIDs::trailMix, "MIX", "How loud the trail is under your sound (0: no trail at all). The mod matrix can move it.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> LightTrailPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void LightTrailPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("LIGHTTRAIL", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: every note leaves a glowing trail, in time with your song", juce::Rectangle<int> (24, 38, 420, 16),
                juce::Justification::centredLeft, false);
}

void LightTrailPage::resized()
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
