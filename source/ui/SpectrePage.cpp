#include "SpectrePage.h"

#include "dsp/effects/Spectre.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

SpectreDisplay::SpectreDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The ghost behind your sound: the line is your sound's outline across the bands (low left, high right); "
                "the pale veil is its ghost, the same outline on breath, without the pitch, each band drifting to its side.");
    veil.fill (-100.0f);
    outline.fill (-100.0f);
    startTimerHz (30);
}

void SpectreDisplay::timerCallback()
{
    const auto& fx = processor.getSpectre();
    const auto on = processor.getSpectreSettings().on;
    for (int b = 0; b < effects::Spectre::numBands; ++b)
    {
        const auto ghost = on ? fx.getGhostBandDb (b) : -100.0f;
        const auto in = on ? fx.getBandDb (b) : -100.0f;
        veil[(size_t) b] += (ghost - veil[(size_t) b]) * (ghost > veil[(size_t) b] ? 0.5f : 0.08f);
        outline[(size_t) b] += (in - outline[(size_t) b]) * 0.5f;
    }
    repaint();
}

void SpectreDisplay::paint (juce::Graphics& g)
{
    using S = effects::Spectre;
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff04060a));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getSpectreSettings();
    const auto lit = s.on ? 1.0f : 0.3f;
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    const auto st = S::shiftSemitones (s.shift);
    g.drawText ("TRAILING " + juce::String (juce::roundToInt (S::trailMs (s.trail))) + " MS   LINGERING " + juce::String (S::lingerSeconds (s.linger), 1)
                    + " S   SHIFT " + (st > 0.05f ? "+" : "") + juce::String (st, 1) + " ST   MIX " + juce::String (juce::roundToInt (100.0f * s.mix)) + "%",
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (8.0f);
    auto labels = area.removeFromBottom (16.0f);

    // A soft grid: the bands' places, every octave marked.
    const auto mid = area.getCentreY();
    const auto xOf = [&area] (int band) { return area.getX() + area.getWidth() * ((float) band + 0.5f) / (float) S::numBands; };
    g.setFont (juce::FontOptions (11.0f));
    for (int b = 0; b < S::numBands; b += 3)
    {
        g.setColour (juce::Colour (0xff11161e));
        g.drawVerticalLine ((int) xOf (b), area.getY(), area.getBottom());
        const auto hz = S::bandHz (b);
        g.setColour (ParagonColours::textDim.withAlpha (lit));
        g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + "k" : juce::String (juce::roundToInt (hz)), juce::Rectangle<float> (xOf (b) - 24.0f, labels.getY(), 48.0f, 16.0f),
                    juce::Justification::centred, false);
    }
    g.setColour (juce::Colour (0xff1a2230));
    g.drawHorizontalLine ((int) mid, area.getX(), area.getRight());

    // Heights from dB: -70 dB nothing, -10 dB the full half height.
    const auto heightOf = [&area] (float dbValue) { return area.getHeight() * 0.48f * juce::jlimit (0.0f, 1.0f, (dbValue + 70.0f) / 60.0f); };

    // The veil: each band a soft column above and below the line, leaning to where it has drifted.
    const auto drift = s.drift;
    for (int pass = 0; pass < 3; ++pass)
    {
        juce::Path veilPath;
        const auto spread = 1.0f + 0.35f * (float) pass;
        const auto alpha = (pass == 0 ? 0.32f : pass == 1 ? 0.14f : 0.07f) * lit;
        veilPath.startNewSubPath (area.getX(), mid);
        for (int b = 0; b < S::numBands; ++b)
        {
            const auto lean = drift * processor.getSpectre().getDriftOf (b) * area.getWidth() * 0.012f;
            veilPath.lineTo (xOf (b) + lean, mid - heightOf (veil[(size_t) b]) * spread);
        }
        veilPath.lineTo (area.getRight(), mid);
        for (int b = S::numBands - 1; b >= 0; --b)
        {
            const auto lean = drift * processor.getSpectre().getDriftOf (b) * area.getWidth() * 0.012f;
            veilPath.lineTo (xOf (b) - lean, mid + heightOf (veil[(size_t) b]) * spread * 0.8f);
        }
        veilPath.closeSubPath();
        g.setColour (juce::Colour (0xffcfe8ff).withAlpha (alpha));
        g.fillPath (veilPath.createPathWithRoundedCorners (18.0f));
    }

    // The sound's own outline: a thin warm line above the centre.
    juce::Path line;
    for (int b = 0; b < S::numBands; ++b)
    {
        const juce::Point<float> pt (xOf (b), mid - heightOf (outline[(size_t) b]));
        if (b == 0)
            line.startNewSubPath (pt);
        else
            line.lineTo (pt);
    }
    g.setColour (juce::Colour (0xffffb45a).withAlpha (0.8f * lit));
    g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xffffb45a).withAlpha (lit));
    g.drawText ("YOUR SOUND", juce::Rectangle<float> (area.getX(), area.getY(), 200.0f, 16.0f), juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xffcfe8ff).withAlpha (lit));
    g.drawText ("ITS GHOST, ON BREATH", juce::Rectangle<float> (area.getRight() - 220.0f, area.getY(), 220.0f, 16.0f), juce::Justification::centredRight, false);
}

//==============================================================================
SpectrePage::SpectrePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::spectreOn, "ON", "SPECTRE on or off: a whispered ghost of your sound following it.");
    make (controls, ParamIDs::spectreMix, "MIX", "How loud the ghost is, under your sound (0: your sound untouched). The mod matrix can move it: FX Spectre > Mix.");
    make (controls, ParamIDs::spectreTrail, "TRAIL", "How far behind your sound the ghost follows: with it, or up to 800 ms behind.");
    make (controls, ParamIDs::spectreLinger, "LINGER", "How long the ghost stays after the sound has gone (the time to fade 60 dB). The mod matrix can move it.");
    make (controls, ParamIDs::spectreAir, "AIR", "The breath it's made of: hollow and whistling, or wide and breathy.");
    make (controls, ParamIDs::spectreDrift, "DRIFT", "How far the ghost wanders between the sides, each part on its own slow swing.");
    make (controls, ParamIDs::spectreShift, "SHIFT", "Its outline moved: a smaller ghost (up, to an octave) or a larger one (down). The mod matrix can move it.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> SpectrePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void SpectrePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("SPECTRE", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: a whispered ghost following your sound, its outline without the pitch", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void SpectrePage::resized()
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
