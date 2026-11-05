#include "SynthFace.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "ui/Finish.h"
#include "ui/LedDisplay.h"
#include "ui/PedalBody.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // The face's layout, in its own units: 1476 x 372 (the space the OSCILLATORS and FILTER panels share).
    constexpr float faceWidth = 1476.0f, faceHeight = 372.0f;
    const juce::Rectangle<float> screenBox { 438.0f, 30.0f, 600.0f, 170.0f };
}

FaceWheel::FaceWheel (const juce::String& wheelName, float restValue) : name (wheelName), rest (restValue), value (restValue)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

juce::Rectangle<float> FaceWheel::slotArea() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom (22.0f);   // the name
    return area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 46.0f), area.getHeight());
}

void FaceWheel::setValue (float newValue, bool notify)
{
    newValue = juce::jlimit (0.0f, 1.0f, newValue);
    if (juce::approximatelyEqual (newValue, value))
        return;
    value = newValue;
    repaint();
    if (notify && onChange)
        onChange (value);
}

void FaceWheel::mouseDown (const juce::MouseEvent&)
{
    dragging = true;
    dragStartValue = value;
}

void FaceWheel::mouseDrag (const juce::MouseEvent& e)
{
    const auto travel = juce::jmax (40.0f, slotArea().getHeight() * 0.9f);
    setValue (dragStartValue - (float) e.getDistanceFromDragStartY() / travel, true);
}

void FaceWheel::paint (juce::Graphics& g)
{
    // The slot: a dark well cut into the face, a chrome lip round it.
    const auto slot = slotArea();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (slot.translated (0.0f, 2.0f), 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9dde3), slot.getX(), slot.getY(), juce::Colour (0xff3e3238), slot.getX(), slot.getBottom(), false));
    g.fillRoundedRectangle (slot, 7.0f);
    const auto well = slot.reduced (3.0f);
    g.setColour (juce::Colour (0xff050405));
    g.fillRoundedRectangle (well, 5.0f);

    // The wheel (0.50.0, the owner's ask): the tread of PR7-G's expression pedal wrapped round a wheel. Black
    // rubber, ribbed across like the pedal's pads, each rib a lit top edge over a shadow; on the neon finishes
    // (0.74.1, as PR7-G 0.59.0) each rib's top is the neon, pink to red to orange across. Seen side on, so the ribs bunch up and darken
    // towards the top and bottom where the wheel curves away, and they roll as it's turned.
    const auto wheel = well.reduced (4.0f, 6.0f);
    juce::Path tread;
    tread.addRoundedRectangle (wheel, 4.0f);
    const auto neonRibs = Finish::neonRings();
    g.setColour (juce::Colour (0xff1a1618));
    g.fillPath (tread);
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (tread);
        const auto spacing = juce::jmax (2.5f, wheel.getHeight() * 0.025f);   // fine, as the pedal's (its ribs are 1.25% of its height)
        const auto roll = std::fmod (value * wheel.getHeight() * 1.6f, spacing);
        for (auto y = wheel.getY() - spacing + roll; y < wheel.getBottom() + spacing; y += spacing)
        {
            const auto t = juce::jlimit (-1.0f, 1.0f, (y - wheel.getCentreY()) / (wheel.getHeight() * 0.5f));
            const auto curved = wheel.getCentreY() + wheel.getHeight() * 0.5f * std::sin (t * 1.2f) / std::sin (1.2f);
            const auto pitch = spacing * std::cos (t * 1.2f);   // narrower where the curve turns away
            if (neonRibs)
            {
                g.setGradientFill (Finish::neonGradient (wheel.getX(), wheel.getRight()));
                g.fillRect (wheel.getX(), curved, wheel.getWidth(), pitch * 0.3f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillRect (wheel.getX(), curved + pitch * 0.3f, wheel.getWidth(), pitch * 0.2f);
                continue;
            }
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (wheel.getX(), curved, wheel.getWidth(), pitch * 0.18f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (wheel.getX(), curved + pitch * 0.18f, wheel.getWidth(), pitch * 0.3f);
        }

        // Round its curve: shaded to near black at the top and bottom, a soft sheen just above the middle.
        juce::ColourGradient shade (juce::Colours::black.withAlpha (0.6f), 0.0f, wheel.getY(), juce::Colours::black.withAlpha (0.65f), 0.0f, wheel.getBottom(), false);
        shade.addColour (0.18, juce::Colours::black.withAlpha (0.08f));
        shade.addColour (0.4, juce::Colours::white.withAlpha (0.06f));
        shade.addColour (0.6, juce::Colours::black.withAlpha (0.0f));
        shade.addColour (0.85, juce::Colours::black.withAlpha (0.2f));
        g.setGradientFill (shade);
        g.fillPath (tread);
    }
    // The unit's red glow caught along its edge, as on the pedal's pads.
    g.setColour (ParagonColours::glow.withAlpha (0.28f));
    g.strokePath (tread, juce::PathStrokeType (1.2f));

    // The light: a neon line along the slot's side, as high as the wheel is turned.
    const auto lit = well.withTrimmedTop (well.getHeight() * (1.0f - value));
    g.setColour (ParagonColours::accent.withAlpha (0.25f));
    g.fillRect (juce::Rectangle<float> (well.getRight() - 2.5f, lit.getY(), 2.5f, lit.getHeight()).expanded (1.5f, 0.0f));
    g.setColour (ParagonColours::accent);
    g.fillRect (juce::Rectangle<float> (well.getRight() - 2.0f, lit.getY(), 1.5f, lit.getHeight()));

    g.setColour (Finish::ink());
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (name, getLocalBounds().removeFromBottom (18), juce::Justification::centred, false);
}

SynthFace::SynthFace (ParagonX1AudioProcessor& processorToUse)
    : processor (processorToUse),
      cutoff (processor.getParameters(), ParamIDs::fltCutoff, "CUTOFF", "The filter's cutoff (the full FILTER panel is in the rack)."),
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

    // The wheels: MOD sends controller 1 (the keyboard's mod wheel), EXPR controller 11 (an expression pedal).
    modWheel.setTooltip ("The mod wheel (MIDI controller 1): each preset's notes say what it does. Your keyboard's wheel moves it too.");
    exprWheel.setTooltip ("Expression, like a keyboard's expression pedal (MIDI controller 11): it swells the volume, or drives the "
                          "preset's own EXPRESSION routes if it has any. Double-click: back to full.");
    modWheel.onChange = [this] (float v) { processor.setScreenModWheel (v); };
    exprWheel.onChange = [this] (float v) { processor.setScreenExpression (v); };
    addAndMakeVisible (modWheel);
    addAndMakeVisible (exprWheel);
    next.onClick = [this] { if (onNext) onNext(); };

    startTimerHz (30);
    Finish::changes().addChangeListener (this);
}

SynthFace::~SynthFace()
{
    Finish::changes().removeChangeListener (this);
}

void SynthFace::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto& macro : macros)
        macro->setPedal();   // the knobs' names in the new finish's ink
    for (auto* knob : { &cutoff, &resonance, &attack, &release })
        knob->setPedal();
    if (getWidth() > 0 && getHeight() > 0)
        grain = Finish::makeSatinGrain (getWidth() * 2, getHeight() * 2, 49);
    repaint();
}

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
    // The wheels follow the keyboard's own wheel and pedal (not while they're being dragged here).
    if (! modWheel.isBeingDragged())
        modWheel.setValue (processor.getModWheelPosition(), false);
    if (! exprWheel.isBeingDragged())
        exprWheel.setValue (processor.getExpressionPosition(), false);

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
    constexpr float knobW = 140.0f, knobH = 146.0f, macroY = 205.0f, heroY = 190.0f;   // 0.50.0: up a little, clear of the rim
    for (int k = 0; k < 3; ++k)
        macros[(size_t) k]->setBounds (at (screenBox.getX() + 30.0f + (float) k * 200.0f, macroY, knobW, knobH));
    cutoff.setBounds (at (146.0f, heroY, knobW, knobH));
    resonance.setBounds (at (290.0f, heroY, knobW, knobH));
    modWheel.setBounds (at (34.0f, 192.0f, 44.0f, 152.0f));
    exprWheel.setBounds (at (88.0f, 192.0f, 44.0f, 152.0f));
    attack.setBounds (at (1106.0f, heroY, knobW, knobH));
    release.setBounds (at (1276.0f, heroY, knobW, knobH));

    if (getWidth() > 0 && getHeight() > 0)
        grain = Finish::makeSatinGrain (getWidth() * 2, getHeight() * 2, 49);   // twice the size: fine on high-resolution screens, as PR7-G's
}

void SynthFace::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto sx = bounds.getWidth() / faceWidth, sy = bounds.getHeight() / faceHeight;
    const auto at = [&] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * sx, y * sy, w * sx, h * sy); };

    // The unit (0.50.0): drawn by PR7-G's own code (PedalBody::paintUnit, ported), so its pearl black is PR7-G's
    // exactly: the light spilling round it, the thick pearl rim with its front edge, the satin face plate sunk
    // into it with its sheen and vignette. Smaller than PR7-G's floor unit, so a thinner rim and less glow.
    const auto chassis = bounds.reduced (5.0f);
    const auto plate = chassis.reduced (9.0f).withTrimmedBottom (5.0f);
    PedalBody::paintUnit (g, chassis, plate, [this] (juce::Graphics& gg)
                          {
                              if (grain.isValid())
                                  gg.drawImage (grain, getLocalBounds().toFloat());
                          },
                          { 16.0f, 10.0f, 6.0f, 0.35f });

    // The badge, top left: PR7-G's (PARAGON stamped in heavy italic over the PR7-K mark in its slanted, speed-cut
    // letters, the 7 in red), what it is printed in red under it.
    PedalBody::paintBadge (g, at (40.0f, 30.0f, 360.0f, 104.0f), "WAVETABLE SYNTHESIZER");

    // A thin red line under the screen's row and the knobs' captions.
    g.setColour (Finish::printedRed().withAlpha (0.5f));
    g.fillRect (at (438.0f, 204.0f, 600.0f, 1.2f));
    g.setFont (juce::FontOptions (10.0f * sy, juce::Font::bold));
    ParagonLookAndFeel::drawStampedText (g, "WHEELS", at (30.0f, 170.0f, 106.0f, 14.0f), juce::Justification::centred, Finish::inkDim());
    ParagonLookAndFeel::drawStampedText (g, "FILTER", at (146.0f, 170.0f, 284.0f, 14.0f), juce::Justification::centred, Finish::inkDim());
    ParagonLookAndFeel::drawStampedText (g, "AMP", at (1106.0f, 170.0f, 310.0f, 14.0f), juce::Justification::centred, Finish::inkDim());
    g.setColour (Finish::inkDim().withAlpha (0.35f));
    g.fillRect (at (30.0f, 186.0f, 106.0f, 1.0f));
    g.fillRect (at (146.0f, 186.0f, 284.0f, 1.0f));
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

    // 0.73.1: a download in the notes' place: what it's doing, and a bar filling as it goes.
    if (screen.status.isNotEmpty())
    {
        auto band = content;
        g.setColour (orange);
        auto textArea = screen.progress >= 0.0f ? band.removeFromLeft (band.getWidth() * 0.62f) : band;
        auto font = juce::FontOptions (band.getHeight() * 0.62f, juce::Font::bold);
        const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (font, screen.status);
        if (textWidth > textArea.getWidth())
            font = font.withHeight (font.getHeight() * textArea.getWidth() / textWidth);
        g.setFont (font);
        g.drawText (screen.status, textArea, juce::Justification::centredLeft, false);
        if (screen.progress >= 0.0f)
        {
            const auto bar = band.withSizeKeepingCentre (band.getWidth(), band.getHeight() * 0.34f);
            g.setColour (orange.withAlpha (0.2f));
            g.fillRect (bar);
            g.setColour (orange);
            g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, screen.progress)));
        }
    }
    // The notes, scrolling right to left in a band at the bottom, round and round.
    else if (screen.notes.isNotEmpty())
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
