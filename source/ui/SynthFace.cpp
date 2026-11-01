#include "SynthFace.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "ui/Finish.h"
#include "ui/LedDisplay.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // The face's layout, in its own units: 1476 x 372 (the space the OSCILLATORS and FILTER panels share).
    constexpr float faceWidth = 1476.0f, faceHeight = 372.0f;
    const juce::Rectangle<float> screenBox { 438.0f, 30.0f, 600.0f, 170.0f };
}

SynthFace::SynthFace (ParagonX1AudioProcessor& processor)
    : cutoff (processor.getParameters(), ParamIDs::fltCutoff, "CUTOFF", "The filter's cutoff (the full FILTER panel is in the rack)."),
      resonance (processor.getParameters(), ParamIDs::fltRes, "RESONANCE", "The filter's resonance."),
      attack (processor.getParameters(), ParamIDs::ampAttack, "ATTACK", "How fast each note's loudness rises (the AMP ENVELOPE's ATTACK)."),
      release (processor.getParameters(), ParamIDs::ampRelease, "RELEASE", "How long each note rings after you let go (the AMP ENVELOPE's RELEASE).")
{
    for (int k = 0; k < 3; ++k)
    {
        macros[(size_t) k] = std::make_unique<ParamControl> (processor.getParameters(), ParamIDs::macros[k], "MACRO " + juce::String (k + 1),
                                                             "This preset's macro " + juce::String (k + 1) + ": its notes say what it does.");
        macros[(size_t) k]->setPedal();
        addAndMakeVisible (*macros[(size_t) k]);
    }
    for (auto* knob : { &cutoff, &resonance, &attack, &release })
    {
        knob->setPedal();
        addAndMakeVisible (*knob);
    }

    for (auto* button : { &previous, &next })
    {
        button->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        addAndMakeVisible (*button);
    }
    previous.setTooltip ("The previous preset");
    next.setTooltip ("The next preset");
    previous.onClick = [this] { if (onPrevious) onPrevious(); };
    next.onClick = [this] { if (onNext) onNext(); };

    startTimerHz (30);
}

SynthFace::~SynthFace() = default;

void SynthFace::setScreen (const Screen& newScreen)
{
    if (newScreen == screen)
        return;
    if (newScreen.notes != screen.notes)
        tickerOffset = 0.0f;
    screen = newScreen;
    repaint (screenArea);
}

void SynthFace::setMacroNames (const std::array<juce::String, 3>& names)
{
    for (int k = 0; k < 3; ++k)
        macros[(size_t) k]->setLabelText (names[(size_t) k]);
}

void SynthFace::timerCallback()
{
    if (screen.notes.isEmpty() || ! isShowing())
        return;
    tickerOffset += 1.2f;   // about 36 pixels a second: easy to read
    repaint (screenArea);
}

void SynthFace::mouseUp (const juce::MouseEvent& e)
{
    if (screenArea.contains (e.getPosition()) && onScreenClicked)
        onScreenClicked();
}

void SynthFace::resized()
{
    const auto sx = (float) getWidth() / faceWidth, sy = (float) getHeight() / faceHeight;
    const auto at = [&] (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (x * sx, y * sy, w * sx, h * sy).toNearestInt();
    };
    screenArea = at (screenBox.getX(), screenBox.getY(), screenBox.getWidth(), screenBox.getHeight());
    previous.setBounds (at (1052.0f, 52.0f, 44.0f, 52.0f));
    next.setBounds (at (1052.0f, 126.0f, 44.0f, 52.0f));

    // The three macros under the screen, the hero knobs either side, lower down, all the same size.
    constexpr float knobW = 140.0f, knobH = 146.0f, macroY = 218.0f, heroY = 196.0f;
    for (int k = 0; k < 3; ++k)
        macros[(size_t) k]->setBounds (at (screenBox.getX() + 30.0f + (float) k * 200.0f, macroY, knobW, knobH));
    cutoff.setBounds (at (60.0f, heroY, knobW, knobH));
    resonance.setBounds (at (230.0f, heroY, knobW, knobH));
    attack.setBounds (at (1106.0f, heroY, knobW, knobH));
    release.setBounds (at (1276.0f, heroY, knobW, knobH));

    if (getWidth() > 0 && getHeight() > 0)
        grain = Finish::makeSatinGrain (getWidth(), getHeight(), 49);
}

void SynthFace::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto sx = bounds.getWidth() / faceWidth, sy = bounds.getHeight() / faceHeight;
    const auto at = [&] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * sx, y * sy, w * sx, h * sy); };

    // The unit's face: dark pearl metal with its satin grain, a soft red glow along the bottom edge (the light
    // under a PR7 unit) and a fine chrome edge, set in a rounded case.
    const auto face = bounds.reduced (1.0f);
    juce::Path shape;
    shape.addRoundedRectangle (face, 12.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 14, { 0, 4 }).drawForPath (g, shape);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1a1f), 0.0f, face.getY(), juce::Colour (0xff0b090c), 0.0f, face.getBottom(), false));
    g.fillPath (shape);
    if (grain.isValid())
    {
        g.saveState();
        g.reduceClipRegion (shape);
        g.setOpacity (0.35f);
        g.drawImageAt (grain, 0, 0);
        g.restoreState();
    }
    {
        juce::ColourGradient glow (juce::Colour (0x00ff2a3a), 0.0f, face.getBottom() - face.getHeight() * 0.35f,
                                   juce::Colour (0x40ff2a3a), 0.0f, face.getBottom(), false);
        g.saveState();
        g.reduceClipRegion (shape);
        g.setGradientFill (glow);
        g.fillRect (face);
        g.restoreState();
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9a8b1), 0.0f, face.getY(), juce::Colour (0xff3a3036), 0.0f, face.getBottom(), false));
    g.strokePath (shape, juce::PathStrokeType (1.5f));

    // The name plate, top left: PARAGON in heavy italic, PR7-K in the red to pink to orange gradient, what it is
    // stamped underneath, as on the other units.
    {
        auto plate = at (40.0f, 26.0f, 360.0f, 100.0f);
        g.setFont (juce::FontOptions (40.0f * sy, juce::Font::bold | juce::Font::italic));
        ParagonLookAndFeel::drawStampedText (g, "PARAGON", plate.removeFromTop (44.0f * sy), juce::Justification::bottomLeft, ParagonColours::text);
        const auto model = plate.removeFromTop (40.0f * sy);
        g.setGradientFill (ParagonLookAndFeel::menuHighlight (model.withWidth (170.0f * sx)));
        g.setFont (juce::FontOptions (36.0f * sy, juce::Font::bold | juce::Font::italic));
        g.drawText ("PR7-K", model, juce::Justification::centredLeft, false);
        g.setFont (juce::FontOptions (10.5f * sy, juce::Font::bold));
        ParagonLookAndFeel::drawStampedText (g, "WAVETABLE SYNTHESIZER", plate, juce::Justification::topLeft, Finish::printedRed());
    }

    // A thin red line under the screen's row and the knobs' captions.
    g.setColour (Finish::printedRed().withAlpha (0.5f));
    g.fillRect (at (438.0f, 212.0f, 600.0f, 1.2f));
    g.setFont (juce::FontOptions (10.0f * sy, juce::Font::bold));
    ParagonLookAndFeel::drawStampedText (g, "FILTER", at (60.0f, 170.0f, 310.0f, 14.0f), juce::Justification::centred, ParagonColours::textDim);
    ParagonLookAndFeel::drawStampedText (g, "AMP", at (1106.0f, 170.0f, 310.0f, 14.0f), juce::Justification::centred, ParagonColours::textDim);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (at (60.0f, 186.0f, 310.0f, 1.0f));
    g.fillRect (at (1106.0f, 186.0f, 310.0f, 1.0f));

    paintScreen (g);
}

void SynthFace::paintScreen (juce::Graphics& g) const
{
    const auto size = (float) screenArea.getHeight() / 120.0f;
    const auto glass = LedDisplay::paintScreen (g, screenArea.toFloat(), juce::jlimit (0.8f, 1.6f, size));
    const auto orange = LedDisplay::ledOrange;
    auto content = glass.reduced (glass.getWidth() * 0.035f, glass.getHeight() * 0.1f);

    // Top line: the slot (SV01) in seven-segment-style weight and the bank, a dot when the sound has changed.
    auto top = content.removeFromTop (content.getHeight() * 0.22f);
    g.setColour (orange);
    g.setFont (juce::FontOptions (top.getHeight() * 0.85f, juce::Font::bold));
    const auto slotText = screen.slot.isNotEmpty() ? screen.slot + "  \xc2\xb7  " + screen.bank : juce::String ("NOT SAVED");
    g.drawText (juce::String (juce::CharPointer_UTF8 (slotText.toRawUTF8())), top, juce::Justification::centredLeft, false);
    if (screen.modified)
    {
        const auto r = top.getHeight() * 0.22f;
        const auto dot = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ top.getRight() - r, top.getCentreY() });
        g.setColour (orange.withAlpha (0.25f));
        g.fillEllipse (dot.expanded (r));
        g.setColour (orange);
        g.fillEllipse (dot);
        g.setFont (juce::FontOptions (top.getHeight() * 0.6f, juce::Font::bold));
        g.drawText ("EDITED", top.withTrimmedRight (r * 3.0f), juce::Justification::centredRight, false);
    }

    // The name, big, with the light bleeding round it.
    auto nameArea = content.removeFromTop (content.getHeight() * 0.62f);
    auto nameFont = juce::FontOptions (nameArea.getHeight() * 0.7f, juce::Font::bold);
    const auto nameWidth = (float) juce::GlyphArrangement::getStringWidthInt (nameFont, screen.name.toUpperCase());
    if (nameWidth > nameArea.getWidth())
        nameFont = nameFont.withHeight (nameFont.getHeight() * nameArea.getWidth() / nameWidth);
    g.setFont (nameFont);
    g.setColour (orange.withAlpha (0.18f));
    for (const auto offset : { -1.5f, 1.5f })
        g.drawText (screen.name.toUpperCase(), nameArea.translated (offset, 0.0f), juce::Justification::centredLeft, false);
    g.setColour (orange);
    g.drawText (screen.name.toUpperCase(), nameArea, juce::Justification::centredLeft, false);

    // The notes, scrolling right to left in a band at the bottom, round and round.
    if (screen.notes.isNotEmpty())
    {
        const auto band = content;
        g.saveState();
        g.reduceClipRegion (band.toNearestInt());
        const auto font = juce::FontOptions (band.getHeight() * 0.62f);
        g.setFont (font);
        const auto text = screen.notes + "      ";
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text);
        const auto offset = width > 0.0f ? std::fmod (tickerOffset, width) : 0.0f;
        g.setColour (orange.withAlpha (0.8f));
        for (auto x = band.getX() - offset; x < band.getRight(); x += width)
            g.drawText (text, juce::Rectangle<float> (x, band.getY(), width, band.getHeight()), juce::Justification::centredLeft, false);
        g.restoreState();
    }

    LedDisplay::paintGlassOverlay (g, glass, juce::jlimit (0.8f, 1.6f, size));
}
