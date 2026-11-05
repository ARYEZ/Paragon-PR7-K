#include "BloomPage.h"

#include "dsp/effects/Bloom.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

BloomDisplay::BloomDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The piano's strings from above, low notes on the left: the ones ringing along with what you play shimmer "
                "(RESONANCE, RING), and the glow behind them is the hall opening out (SIZE, SWELL).");
    startTimerHz (30);
}

void BloomDisplay::timerCallback()
{
    phase += 0.9f;
    if (phase > 1000.0f)
        phase -= 1000.0f;
    repaint();
}

void BloomDisplay::paint (juce::Graphics& g)
{
    using B = effects::Bloom;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getBloomSettings();
    const auto& fx = processor.getBloom();
    const auto live = settings.on;
    g.setColour (juce::Colour (0xff0a0705));
    g.fillRoundedRectangle (bounds, 6.0f);
    auto view = bounds.reduced (12.0f).withTrimmedTop (32.0f);

    // The hall: a warm glow rising behind the strings with the swell and the hall's level.
    const auto hall = live ? juce::jlimit (0.0f, 1.0f, (fx.getHallDb() + 60.0f) / 50.0f) : 0.0f;
    const auto glow = hall * (0.3f + 0.7f * fx.getSwell());
    if (glow > 0.01f)
    {
        const auto centre = juce::Point<float> (view.getCentreX(), view.getBottom());
        juce::ColourGradient fill (juce::Colour (0xffffb050).withAlpha (0.45f * glow), centre,
                                   juce::Colours::transparentBlack, centre.translated (0.0f, -view.getHeight() * (0.4f + 0.8f * glow)), true);
        g.setGradientFill (fill);
        g.fillRect (view);
    }

    // The piano's frame: a soundboard outline and the keyboard's range A0 to C8 along the bottom.
    const auto xOf = [&view] (float pitch) { return view.getX() + view.getWidth() * (pitch - 21.0f) / 87.0f; };
    g.setColour (juce::Colour (0xff5a4026));
    g.drawRoundedRectangle (view, 8.0f, 1.5f);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.0f));
    for (int octave = 1; octave <= 8; ++octave)
    {
        const auto x = xOf ((float) (12 * octave + 12));
        g.drawText ("C" + juce::String (octave), juce::Rectangle<float> (x - 12.0f, view.getBottom() - 14.0f, 24.0f, 12.0f),
                    juce::Justification::centred, false);
    }
    // Every string, dim; the ones the effect has tuned lit by how much they ring, shimmering.
    for (int pitch = 21; pitch <= 108; ++pitch)
    {
        const auto x = xOf ((float) pitch);
        const auto length = view.getHeight() * (0.85f - 0.55f * (float) (pitch - 21) / 87.0f);   // long bass strings, short treble
        g.setColour (juce::Colour (0xff3a2a1a));
        g.drawLine (x, view.getBottom() - 18.0f, x, view.getBottom() - 18.0f - length, 1.0f);
    }
    auto ringing = 0;
    for (int k = 0; k < B::numStrings; ++k)
    {
        const auto pitch = live ? fx.getStringPitch (k) : -1.0f;
        const auto level = live ? fx.getStringLevel (k) : 0.0f;
        if (pitch < 0.0f || level < 0.01f)
            continue;
        ++ringing;
        const auto x = xOf (pitch);
        const auto length = view.getHeight() * (0.85f - 0.55f * (pitch - 21.0f) / 87.0f);
        const auto bottom = view.getBottom() - 18.0f, top = bottom - length;
        juce::Path string;
        string.startNewSubPath (x, bottom);
        for (int s = 1; s <= 24; ++s)
        {
            const auto t = (float) s / 24.0f;
            const auto sway = 3.5f * level * std::sin (juce::MathConstants<float>::pi * t) * std::sin (0.6f * phase + pitch);
            string.lineTo (x + sway, bottom - length * t);
        }
        g.setColour (juce::Colour (0xffffd28a).withAlpha (0.25f * level));
        g.strokePath (string, juce::PathStrokeType (5.0f));
        g.setColour (juce::Colour (0xffffe6b8).withAlpha (0.4f + 0.6f * level));
        g.strokePath (string, juce::PathStrokeType (1.6f));
        juce::ignoreUnused (top);
    }

    // The readings across the top.
    g.setColour (ParagonColours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    auto bar = bounds.reduced (14.0f, 8.0f).removeFromTop (22.0f);
    g.drawText ("IN BLOOM " + juce::String (juce::roundToInt (100.0f * (live ? fx.getAmount() : 0.0f))) + "%", bar, juce::Justification::centredLeft, false);
    g.drawText ("STRINGS RINGING " + juce::String (ringing) + "   SWELL " + juce::String (juce::roundToInt (100.0f * (live ? fx.getSwell() : 0.0f))) + "%",
                bar, juce::Justification::centred, false);
    g.drawText ("HALL " + (live && fx.getHallDb() > -90.0f ? juce::String (juce::roundToInt (fx.getHallDb())) + " dB" : juce::String ("-")),
                bar, juce::Justification::centredRight, false);
}

//==============================================================================
BloomPage::BloomPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::bloomOn, "ON", "BLOOM on or off: the piano's strings ringing along and a hall opening out.");
    make (controls, ParamIDs::bloomAmount, "AMOUNT", "How much: 0 close and dry (your sound untouched), all the way up vast. "
                                                     "The mod matrix can move it: FX Bloom > Amount.");
    make (controls, ParamIDs::bloomResonance, "STRINGS", "How loud the strings ringing along are (sympathetic resonance). The mod matrix can move it.");
    make (controls, ParamIDs::bloomRing, "RING", "How long the strings ring on after the notes: 1.5 to 12 seconds.");
    make (controls, ParamIDs::bloomSize, "SIZE", "How long the hall's tail is: 2 to 12 seconds. The mod matrix can move it.");
    make (controls, ParamIDs::bloomSwell, "SWELL", "How slowly the hall opens as you hold: 0.15 to 4 seconds.");
    make (controls, ParamIDs::bloomTone, "TONE", "How bright the hall is: 1.5 to 15 kHz.");
    make (controls, ParamIDs::bloomWidth, "WIDTH", "How wide the hall spreads.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> BloomPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void BloomPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("BLOOM", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the strings ringing along, a hall opening out", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void BloomPage::resized()
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
