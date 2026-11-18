#include "PolymeterPage.h"

#include "dsp/synth/Polymeter.h"
#include "dsp/synth/Sirocco.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    namespace pm = synth::polymeter;
    // The rings' colours, 4 (the pulse) to 7: sand, ember, rose, a pale sky.
    const juce::Colour ringColours[pm::numMeters] { juce::Colour (0xffffb347), juce::Colour (0xffff6a24), juce::Colour (0xffff2d6e), juce::Colour (0xff8fd3ff) };

    juce::String noteName (int note)
    {
        return synth::sirocco::keyNames[((note % 12) + 12) % 12] + juce::String (note / 12 - 1);
    }
}

PolymeterDisplay::PolymeterDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The meters as a wheel and as lanes: 4 inside (the pulse), then 3, 5 and 7, each a dot for every strike of "
                "the cycle, all lining up at its start. AMOUNT brings the rings in one at a time; each is named with the "
                "lowest note playing it (a lone note plays every ring). The hand and the line show where in the cycle "
                "the song is.");
    startTimerHz (30);
}

void PolymeterDisplay::timerCallback()
{
    for (int j = 0; j < pm::numMeters; ++j)
    {
        const auto hits = processor.getPolyMeterHits (j);
        if (hits != shownHits[(size_t) j])
        {
            shownHits[(size_t) j] = hits;
            flash[(size_t) j] = 1.0f;
        }
        else
            flash[(size_t) j] = juce::jmax (0.0f, flash[(size_t) j] - 0.12f);
    }
    repaint();
}

void PolymeterDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getPolymeterSettings();
    const auto live = settings.playing();
    const auto active = live ? settings.activeMeters() : 0;
    const auto phaseRaw = processor.getPolyPhase();
    const auto phase = live && phaseRaw >= 0 ? (float) phaseRaw / 10000.0f : 0.0f;
    const auto per = settings.timesPerBeat();

    auto view = bounds.reduced (2.0f);
    g.setColour (juce::Colour (0xff070304));
    g.fillRoundedRectangle (view, 6.0f);

    // The top bar.
    auto bar = view.reduced (14.0f, 6.0f).removeFromTop (22.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    juce::String meterText;
    for (int j = 0; j < active; ++j)
        meterText << (j > 0 ? " AGAINST " : "") << pm::meters[(size_t) j];
    const auto strikes = processor.getPolyStrikes();
    g.drawText (! settings.on ? juce::String ("OFF") : ! live ? juce::String ("RESTING")
                : meterText + (strikes > 0 ? "   " + juce::String (strikes) + (strikes == 1 ? " STRIKE" : " STRIKES") : juce::String()),
                bar, juce::Justification::centredLeft, false);
    g.drawText ("A CYCLE OF " + pm::cycleNames[settings.cycle] + "   " + pm::densityNames[settings.density], bar, juce::Justification::centredRight, false);

    auto content = view.reduced (14.0f, 0.0f).withTrimmedTop (32.0f).withTrimmedBottom (10.0f);
    auto wheelArea = content.removeFromLeft (juce::jmin (content.getHeight(), content.getWidth() * 0.42f));
    content.removeFromLeft (18.0f);

    // The wheel.
    {
        const auto centre = wheelArea.getCentre();
        const auto outer = wheelArea.getHeight() * 0.46f;
        const auto inner = outer * 0.3f;
        g.setColour (juce::Colour (0xff120a0c));
        g.fillEllipse (juce::Rectangle<float> (outer * 2.1f, outer * 2.1f).withCentre (centre));
        for (int j = 0; j < pm::numMeters; ++j)
        {
            const auto in = j < active;
            const auto radius = inner + (outer - inner) * (float) j / (float) (pm::numMeters - 1);
            const auto colour = ringColours[j];
            const auto f = in ? flash[(size_t) j] : 0.0f;
            g.setColour (in ? colour.withAlpha (0.35f + 0.4f * f) : juce::Colour (0xff2a2023));
            g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), in ? 1.6f + 1.4f * f : 1.0f);
            const auto count = pm::meters[(size_t) j] * per;
            for (int k = 0; k < count; ++k)
            {
                const auto a = juce::MathConstants<float>::twoPi * (float) k / (float) count;
                const auto pt = centre + juce::Point<float> (std::sin (a), -std::cos (a)) * radius;
                const auto main = k % per == 0;
                const auto size = (main ? 7.0f : 4.0f) * (in ? 1.0f + 0.35f * f : 0.8f);
                // A dot the hand has just passed glows.
                auto since = phase - (float) k / (float) count;
                if (since < 0.0f)
                    since += 1.0f;
                const auto glow = in ? juce::jmax (0.0f, 1.0f - since * 8.0f) : 0.0f;
                g.setColour (in ? colour.interpolatedWith (juce::Colours::white, glow * 0.7f) : juce::Colour (0xff3a2e31));
                g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (pt));
            }
            // The ring's meter and its lowest note, at its top left.
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.setColour (in ? colour : ParagonColours::textDim.withAlpha (0.5f));
            const auto note = in ? processor.getPolyMeterNote (j) : -1;
            const auto label = juce::String (pm::meters[(size_t) j]) + (note >= 0 ? " " + noteName (note) : juce::String());
            const auto lp = centre + juce::Point<float> (-0.7071f, -0.7071f) * radius;
            g.drawText (label, juce::Rectangle<float> (lp.x - 58.0f, lp.y - 12.0f, 54.0f, 12.0f), juce::Justification::centredRight, false);
        }
        // The hand.
        if (live)
        {
            const auto a = juce::MathConstants<float>::twoPi * phase;
            const auto tip = centre + juce::Point<float> (std::sin (a), -std::cos (a)) * (outer + 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawLine ({ centre, tip }, 1.6f);
        }
        g.setColour (live ? juce::Colours::white : ParagonColours::textDim);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));
    }

    // The lanes: each meter across the cycle, its strikes as ticks, the line where the song is.
    {
        auto lanes = content.withTrimmedTop (6.0f).withTrimmedBottom (6.0f);
        const auto laneHeight = lanes.getHeight() / (float) pm::numMeters;
        const auto left = lanes.getX() + 58.0f, right = lanes.getRight() - 6.0f;
        for (int j = 0; j < pm::numMeters; ++j)
        {
            const auto in = j < active;
            const auto lane = juce::Rectangle<float> (lanes.getX(), lanes.getY() + laneHeight * (float) j, lanes.getWidth(), laneHeight).reduced (0.0f, 4.0f);
            const auto colour = ringColours[j];
            g.setColour (juce::Colour (0xff120a0c));
            g.fillRoundedRectangle (lane.withLeft (left - 4.0f), 4.0f);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.setColour (in ? colour : ParagonColours::textDim.withAlpha (0.5f));
            g.drawText (juce::String (pm::meters[(size_t) j]), lane.withWidth (22.0f), juce::Justification::centredLeft, false);
            const auto note = in ? processor.getPolyMeterNote (j) : -1;
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText (note >= 0 ? noteName (note) : juce::String (in ? "-" : ""), lane.withX (lane.getX() + 20.0f).withWidth (34.0f), juce::Justification::centredLeft, false);
            const auto count = pm::meters[(size_t) j] * per;
            for (int k = 0; k < count; ++k)
            {
                const auto x = left + (right - left) * (float) k / (float) count;
                const auto main = k % per == 0;
                const auto h = lane.getHeight() * (main ? 0.72f : 0.42f);
                g.setColour (in ? colour.withAlpha (main ? 1.0f : 0.6f) : juce::Colour (0xff3a2e31));
                g.fillRoundedRectangle (juce::Rectangle<float> (main ? 4.0f : 2.5f, h).withCentre ({ x + 2.0f, lane.getCentreY() }), 1.2f);
            }
        }
        if (live)
        {
            const auto x = left + (right - left) * phase;
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawLine (x, lanes.getY(), x, lanes.getBottom(), 1.4f);
        }
    }
}

//==============================================================================
PolymeterPage::PolymeterPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::polyOn, "ON", "POLYMETER on or off: the held notes struck again in several meters at once, in time with the song.");
    make (controls, ParamIDs::polyAmount, "AMOUNT", "The meters come in one at a time: 4 alone, then 4 against 3 from a quarter, 5 from half, "
                                                    "7 from three quarters. Nothing at 0 (the sound untouched). "
                                                    "The mod matrix can move it: FX Polymeter > Amount.");
    make (controls, ParamIDs::polyCycle, "CYCLE", "The span the meters share, lining up at its start: a beat to two bars.");
    make (controls, ParamIDs::polyDensity, "DENSITY", "Every meter struck 1, 2 or 4 times as often.");
    make (controls, ParamIDs::polyAccent, "ACCENT", "The subdivisions softer than each meter's own beats. The mod matrix can move it: FX Polymeter > Accent.");
    make (controls, ParamIDs::polyLift, "LIFT", "How far the sound is lifted just before each strike, so a held sound is struck too. "
                                                "The mod matrix can move it: FX Polymeter > Lift.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> PolymeterPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void PolymeterPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("POLYMETER", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the held notes struck again in several meters at once, in time with the song",
                juce::Rectangle<int> (24, 38, 480, 16), juce::Justification::centredLeft, false);
}

void PolymeterPage::resized()
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
