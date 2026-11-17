#include "RedlinePage.h"

#include "dsp/effects/Redline.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

RedlineDisplay::RedlineDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The tachometer: the revs climbing across each gear, in time with the song, and dropping back at the "
                "gear change; the gear beside it; on the right the engine: the throb at half the note's pitch, the "
                "scream and the whine.");
    startTimerHz (30);
}

namespace
{
    juce::String noteName (float hz)
    {
        if (hz <= 0.0f)
            return "-";
        const auto midi = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
        return juce::MidiMessage::getMidiNoteName (midi, true, true, 4);
    }
}

void RedlineDisplay::paint (juce::Graphics& g)
{
    using R = effects::Redline;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getRedlineSettings();
    const auto& fx = processor.getRedline();
    const auto live = settings.on && ! fx.isResting();
    const auto throttle = live ? fx.getThrottle() : 0.0f;
    const auto revs = live ? fx.getRevs() : 0.0f;
    const auto red = juce::Colour (0xffff3020), amber = juce::Colour (0xffffa53a), chrome = juce::Colour (0xffd8dde4);

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff140806), view.getX(), view.getY(),
                                             juce::Colour (0xff080808), view.getRight(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    auto inner = view.reduced (view.getWidth() * 0.03f, view.getHeight() * 0.1f).withTrimmedTop (view.getHeight() * 0.06f);

    // THE TACHOMETER: 0 to 8 (x1000 RPM) over 240 degrees, the last fifth red.
    auto dialArea = inner.removeFromLeft (inner.getWidth() * 0.46f);
    const auto size = juce::jmin (dialArea.getWidth(), dialArea.getHeight());
    const auto dial = dialArea.withSizeKeepingCentre (size, size);
    const auto centre = dial.getCentre();
    const auto radius = size * 0.46f;
    constexpr auto from = -juce::MathConstants<float>::pi * 2.0f / 3.0f, to = juce::MathConstants<float>::pi * 2.0f / 3.0f;
    const auto angleOf = [&] (float rpm) { return from + (to - from) * juce::jlimit (0.0f, 1.0f, rpm / 8.0f); };
    const auto pointAt = [&] (float angle, float r) { return juce::Point<float> (centre.x + r * std::sin (angle), centre.y - r * std::cos (angle)); };
    g.setColour (juce::Colour (0xff1c1c1e));
    g.fillEllipse (dial.reduced (size * 0.02f));
    g.setColour (chrome.withAlpha (0.5f));
    g.drawEllipse (dial.reduced (size * 0.02f), 2.0f);
    {
        juce::Path zone;
        zone.addCentredArc (centre.x, centre.y, radius * 0.86f, radius * 0.86f, 0.0f, angleOf (6.4f), to, true);
        g.setColour (red.withAlpha (0.75f));
        g.strokePath (zone, juce::PathStrokeType (size * 0.05f));
    }
    g.setFont (juce::FontOptions (size * 0.075f, juce::Font::bold));
    for (int k = 0; k <= 8; ++k)
    {
        const auto a = angleOf ((float) k);
        g.setColour (k >= 7 ? red : chrome);
        g.drawLine ({ pointAt (a, radius * 0.92f), pointAt (a, radius * 0.78f) }, 2.0f);
        const auto at = pointAt (a, radius * 0.64f);
        g.drawText (juce::String (k), juce::Rectangle<float> (size * 0.12f, size * 0.09f).withCentre (at), juce::Justification::centred, false);
        if (k < 8)
        {
            const auto half = angleOf ((float) k + 0.5f);
            g.setColour (chrome.withAlpha (0.4f));
            g.drawLine ({ pointAt (half, radius * 0.92f), pointAt (half, radius * 0.84f) }, 1.0f);
        }
    }
    const auto rpm = live ? 0.8f + 7.2f * revs * throttle : 0.0f;
    {
        const auto a = angleOf (rpm);
        g.setColour (red.withAlpha (0.25f + 0.75f * throttle));
        g.drawLine ({ pointAt (a + juce::MathConstants<float>::pi, radius * 0.14f), pointAt (a, radius * 0.86f) }, size * 0.018f);
        g.setColour (chrome);
        g.fillEllipse (juce::Rectangle<float> (size * 0.07f, size * 0.07f).withCentre (centre));
        g.setFont (juce::FontOptions (size * 0.06f, juce::Font::bold));
        g.setColour (ParagonColours::textDim);
        g.drawText ("x1000 RPM", juce::Rectangle<float> (size * 0.4f, size * 0.07f).withCentre (pointAt (juce::MathConstants<float>::pi, radius * 0.4f)),
                    juce::Justification::centred, false);
        g.setColour (live ? amber : ParagonColours::textDim);
        g.setFont (juce::FontOptions (size * 0.1f, juce::Font::bold));
        g.drawText (live ? "GEAR " + juce::String (fx.getGear()) : "OFF",
                    juce::Rectangle<float> (size * 0.5f, size * 0.12f).withCentre (pointAt (juce::MathConstants<float>::pi, radius * 0.66f)),
                    juce::Justification::centred, false);
    }

    // THE ENGINE: the throb (a rounded square at half the note's pitch), the scream, the whine.
    inner.removeFromLeft (inner.getWidth() * 0.06f);
    auto engine = inner;
    const auto rows = engine.getHeight() / 3.0f;
    const auto bar = [&] (juce::Rectangle<float> row, const juce::String& title, const juce::String& value, float share, juce::Colour colour)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (title, row.removeFromTop (16.0f), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::text);
        g.drawText (value, row.withHeight (16.0f).translated (0.0f, -16.0f), juce::Justification::centredRight, false);
        auto track = row.removeFromTop (juce::jmin (14.0f, row.getHeight() * 0.3f));
        g.setColour (juce::Colour (0xff222224));
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * juce::jlimit (0.0f, 1.0f, share)), 3.0f);
        return row;
    };
    {
        auto row = engine.removeFromTop (rows).reduced (0.0f, 4.0f);
        const auto depth = R::throbDepth (settings.throb, revs) * throttle;
        auto rest = bar (row, "THROB", noteName (live ? fx.getNoteHz() * 0.5f : 0.0f) + "  " + juce::String (juce::roundToInt (100.0f * depth)) + "%", depth / 0.95f, amber);
        // Four cycles of the throb, as deep as it is now.
        juce::Path wave;
        const auto w = rest.reduced (0.0f, 4.0f);
        for (int k = 0; k <= 160; ++k)
        {
            const auto phase = 4.0f * (float) k / 160.0f;
            const auto q = 0.5f + 0.5f * std::tanh (3.0f * std::sin (juce::MathConstants<float>::twoPi * phase));
            const auto level = 1.0f - depth * (1.0f - q);
            const juce::Point<float> at (w.getX() + w.getWidth() * (float) k / 160.0f, w.getBottom() - level * w.getHeight());
            if (k == 0)
                wave.startNewSubPath (at);
            else
                wave.lineTo (at);
        }
        g.setColour (amber.withAlpha (live ? 0.9f : 0.3f));
        g.strokePath (wave, juce::PathStrokeType (1.6f));
    }
    {
        auto row = engine.removeFromTop (rows).reduced (0.0f, 4.0f);
        const auto db = live ? fx.getScreamDb() : -120.0f;
        bar (row, "SCREAM", db > -90.0f ? juce::String (db, 1) + " dB  +" + juce::String (R::riseSemitones (settings.rise) * revs, 1) + " st" : "-",
             (db + 48.0f) / 48.0f, red);
    }
    {
        auto row = engine.removeFromTop (rows).reduced (0.0f, 4.0f);
        const auto share = settings.whine * throttle * revs;
        bar (row, "WHINE", juce::String (juce::roundToInt (100.0f * share)) + "%", share, chrome);
    }

    // The bar across the top.
    auto top = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (! settings.on ? juce::String ("OFF") : "A GEAR EVERY " + R::gearNames[settings.gear], top, juce::Justification::centredLeft, false);
    g.drawText ("NOTE " + noteName (live ? fx.getNoteHz() : 0.0f), top, juce::Justification::centred, false);
    g.drawText ("LEVEL KEPT " + juce::String (live ? fx.getGainDb() : 0.0f, 1) + " dB", top, juce::Justification::centredRight, false);
}

//==============================================================================
RedlinePage::RedlinePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::redlineOn, "ON", "REDLINE on or off: the sound as a revving engine in time with the song.");
    make (controls, ParamIDs::redlineThrottle, "THROTTLE", "How far the throttle opens: the throb, the scream and the whine "
                                                           "with it (0: the sound untouched). The mod matrix can move it: FX "
                                                           "Redline > Throttle.");
    make (controls, ParamIDs::redlineGear, "GEAR", "How long each gear lasts: the revs climb across it and drop back on "
                                                    "its line, half a bar to four bars, in time with the song.");
    make (controls, ParamIDs::redlineThrob, "THROB", "The cylinders: the sound pulsing at half the note's pitch, a growl an "
                                                      "octave down, deeper as the revs climb.");
    make (controls, ParamIDs::redlineScream, "SCREAM", "The engine screaming: the sound an octave up through a hard drive, "
                                                        "louder and harder as the revs climb.");
    make (controls, ParamIDs::redlineRise, "RISE", "How far the scream climbs over a gear: 0 to 12 semitones.");
    make (controls, ParamIDs::redlineWhine, "WHINE", "A supercharger's whine two octaves over the note, rising a fifth with "
                                                      "the revs.");
    make (controls, ParamIDs::redlineTone, "TONE", "The scream's top: 2 kHz (dark) to 12 kHz (fizzing).");
    make (controls, ParamIDs::redlineShift, "SHIFT", "How hard the gear change hits: the level's dip as the revs drop back.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> RedlinePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void RedlinePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("REDLINE", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the sound as a revving engine, in time with the song", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void RedlinePage::resized()
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
