#include "WobblePage.h"

#include "dsp/effects/Wobble.h"
#include "dsp/synth/VoiceFilter.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

WobbleDisplay::WobbleDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The pattern's two bars: the wobble opening and closing (the cutoff, low at the foot, high at the top), "
                "each beat named with its speed and where the song is now lit; on the right TALK's vowel, OO shut to EH open.");
    startTimerHz (30);
}

namespace
{
    // 20 Hz to 20 kHz on a log scale, 0 at the foot.
    float heightOf (float hz) noexcept { return std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f); }
}

void WobbleDisplay::paint (juce::Graphics& g)
{
    using W = effects::Wobble;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getWobbleSettings();
    const auto& fx = processor.getWobble();
    const auto live = settings.on && ! fx.isResting();
    const auto amount = live ? fx.getAmount() : 0.0f;
    const auto beatNow = live ? fx.getBeat() : -1.0f;
    const auto pink = juce::Colour (0xffff3aa8), violet = juce::Colour (0xff8a5cff), lime = juce::Colour (0xffb6ff3a);

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0716), view.getX(), view.getY(),
                                             juce::Colour (0xff060d10), view.getRight(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    auto inner = view.reduced (view.getWidth() * 0.03f, view.getHeight() * 0.12f).withTrimmedTop (view.getHeight() * 0.05f);
    auto pathArea = inner.removeFromLeft (inner.getWidth() * 0.74f);
    inner.removeFromLeft (inner.getWidth() * 0.12f);
    auto vowelArea = inner;
    auto labels = pathArea.removeFromBottom (16.0f);

    // Octave lines and the frame.
    g.setColour (juce::Colour (0xff22182c));
    for (float hz = 40.0f; hz < 20000.0f; hz *= 2.0f)
        g.drawHorizontalLine ((int) (pathArea.getBottom() - heightOf (hz) * pathArea.getHeight()), pathArea.getX(), pathArea.getRight());
    g.setColour (juce::Colour (0xff3a2a48));
    g.drawRect (pathArea, 1.0f);

    // The beats: the one playing lit, each named with its speed, bar lines brighter.
    const auto beatWidth = pathArea.getWidth() / (float) W::patternBeats;
    for (int b = 0; b < W::patternBeats; ++b)
    {
        const auto column = juce::Rectangle<float> (pathArea.getX() + beatWidth * (float) b, pathArea.getY(), beatWidth, pathArea.getHeight());
        const auto playing = beatNow >= (float) b && beatNow < (float) (b + 1);
        if (playing)
        {
            g.setColour (pink.withAlpha (0.12f + 0.18f * amount));
            g.fillRect (column);
        }
        g.setColour (b % 4 == 0 ? juce::Colour (0xff5a4070) : juce::Colour (0xff2e2238));
        g.drawVerticalLine ((int) column.getX(), pathArea.getY(), pathArea.getBottom());
        g.setColour (playing ? pink : ParagonColours::textDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (W::rateName (W::patterns[(size_t) settings.pattern][(size_t) b]),
                    labels.withX (column.getX()).withWidth (beatWidth), juce::Justification::centred, false);
    }

    // The wobble's cutoff across the two bars, as the knobs set it (the shut floor and the open ceiling dashed).
    {
        juce::Path path;
        constexpr int points = 960;
        for (int k = 0; k <= points; ++k)
        {
            const auto beats = (double) W::patternBeats * k / points;
            const auto hz = W::wobbleHz (W::openAt (juce::jmin (beats, W::patternBeats - 1.0e-6), settings.pattern, settings.shape), settings);
            const juce::Point<float> at (pathArea.getX() + pathArea.getWidth() * (float) k / points,
                                         pathArea.getBottom() - heightOf (hz) * pathArea.getHeight());
            if (k == 0)
                path.startNewSubPath (at);
            else
                path.lineTo (at);
        }
        g.setColour (violet.withAlpha (settings.on ? 0.95f : 0.35f));
        g.strokePath (path, juce::PathStrokeType (1.8f));
    }

    // Where the song is now.
    if (beatNow >= 0.0f)
    {
        const auto x = pathArea.getX() + beatWidth * beatNow;
        const auto hz = fx.getCutoffHz();
        const auto y = pathArea.getBottom() - heightOf (hz) * pathArea.getHeight();
        g.setColour (lime.withAlpha (0.4f + 0.6f * amount));
        g.drawVerticalLine ((int) x, pathArea.getY(), pathArea.getBottom());
        g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
    }

    // TALK's vowels: OO (shut) at the foot to EH (open), the one sounding lit by how much TALK there is.
    {
        g.setColour (juce::Colour (0xff3a2a48));
        g.drawRoundedRectangle (vowelArea, 4.0f, 1.0f);
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("TALK", vowelArea.withHeight (12.0f).translated (0.0f, -14.0f), juce::Justification::centred, false);
        const auto vowelNow = W::vowelAt (live ? fx.getOpen() : 0.0f) * 4.0f;   // 0 OO .. 4 EE
        const auto talk = settings.talk * amount;
        const auto step = vowelArea.getHeight() / 4.0f;
        for (int v = 0; v < 4; ++v)
        {
            const auto row = juce::Rectangle<float> (vowelArea.getX(), vowelArea.getBottom() - step * (float) (v + 1), vowelArea.getWidth(), step);
            const auto near = juce::jmax (0.0f, 1.0f - std::abs (vowelNow - (float) v));
            if (near > 0.0f && talk > 0.0f)
            {
                g.setColour (pink.withAlpha (near * (0.25f + 0.6f * talk)));
                g.fillRoundedRectangle (row.reduced (3.0f), 3.0f);
            }
            g.setColour (near > 0.5f && talk > 0.0f ? juce::Colours::white : ParagonColours::textDim);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (synth::vocal::vowelNames[v], row, juce::Justification::centred, false);
        }
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (! settings.on ? juce::String ("OFF") : W::patternNames[settings.pattern] + "   " + W::shapeNames[settings.shape],
                bar, juce::Justification::centredLeft, false);
    const auto cutoff = live ? fx.getCutoffHz() : 20000.0f;
    g.drawText ("CUTOFF " + (cutoff >= 1000.0f ? juce::String (cutoff / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (cutoff)) + " Hz"),
                bar, juce::Justification::centred, false);
    g.drawText ("LEVEL KEPT " + juce::String (live ? fx.getGainDb() : 0.0f, 1) + " dB", bar, juce::Justification::centredRight, false);
}

//==============================================================================
WobblePage::WobblePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::wobbleOn, "ON", "WOBBLE on or off: the dubstep wobble, locked to the song.");
    make (controls, ParamIDs::wobbleAmount, "AMOUNT", "How much: the filter comes in from fully open to the wobble, TALK, DRIVE "
                                                       "and the resonance with it (0: the sound untouched). The mod matrix can "
                                                       "move it: FX Wobble > Amount.");
    make (controls, ParamIDs::wobblePattern, "PATTERN", "The wobble's speed beat by beat over two bars: STEADY eighths, HALF-TIME "
                                                         "quarters, TRIPLETS, DROP (eighths into a fill), ROLLS (speeding up), "
                                                         "TALKER, STUTTER, BOUNCE.");
    make (controls, ParamIDs::wobbleShape, "SHAPE", "SMOOTH (the classic wub), SNAP (open at once, falling fast) or RISE (opening "
                                                     "across each wobble: the \"yoi\").");
    make (controls, ParamIDs::wobbleLow, "LOW", "Where the filter shuts: 40 Hz to 800 Hz.");
    make (controls, ParamIDs::wobbleRange, "RANGE", "How far it opens above LOW: 2 to 7 octaves.");
    make (controls, ParamIDs::wobbleResonance, "RESONANCE", "How much the filter sings as it moves.");
    make (controls, ParamIDs::wobbleTalk, "TALK", "A vowel filter moving with the wobble, OO shut to EH open: the talking growl.");
    make (controls, ParamIDs::wobbleDrive, "DRIVE", "Grit going in, under the wobble.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> WobblePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void WobblePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("WOBBLE", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the dubstep wobble, locked to the song and changing speed beat by beat", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void WobblePage::resized()
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
