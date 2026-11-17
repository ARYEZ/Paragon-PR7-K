#include "NeonKeyboard.h"

namespace
{
    const juce::Colour pink (0xffff2d6e), red (0xffff2a2e), orange (0xffff6a24);
}

NeonKeyboard::NeonKeyboard (juce::MidiKeyboardState& state, Orientation orientation) : juce::MidiKeyboardComponent (state, orientation)
{
    // The keys draw everything themselves: the background behind them is the strip's dark.
    setColour (whiteNoteColourId, juce::Colour (0xff0a0608));
    setColour (blackNoteColourId, juce::Colour (0xff0a080a));
    setColour (keySeparatorLineColourId, juce::Colour (0xff0a0608));
    setColour (shadowColourId, juce::Colours::black.withAlpha (0.35f));
}

juce::Colour NeonKeyboard::neonAt (float position) noexcept
{
    const auto p = juce::jlimit (0.0f, 1.0f, position);
    return p < 0.5f ? pink.interpolatedWith (red, p * 2.0f) : red.interpolatedWith (orange, (p - 0.5f) * 2.0f);
}

float NeonKeyboard::positionOf (juce::Rectangle<float> area) const noexcept
{
    return getWidth() > 0 ? area.getCentreX() / (float) getWidth() : 0.5f;
}

void NeonKeyboard::drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                                  juce::Colour, juce::Colour)
{
    const auto neon = neonAt (positionOf (area));
    auto key = area.reduced (1.0f, 0.0f).withTrimmedBottom (1.0f);
    const auto radius = juce::jmin (3.0f, key.getWidth() * 0.15f);

    // The body: dim at the back, glowing toward the front; played, nearly white-hot.
    auto back = neon.darker (1.4f), front = neon;
    if (isDown)
    {
        back = neon.interpolatedWith (juce::Colour (0xfffff0e8), 0.25f);
        front = neon.interpolatedWith (juce::Colour (0xfffff0e8), 0.7f);
    }
    else if (isOver)
    {
        back = back.brighter (0.25f);
        front = front.brighter (0.15f);
    }
    juce::ColourGradient body (back, 0.0f, key.getY(), front, 0.0f, key.getBottom(), false);
    body.addColour (0.55, back.interpolatedWith (front, 0.35f));
    g.setGradientFill (body);
    g.fillRoundedRectangle (key, radius);

    // The front lip: a bright line, a glow above it.
    const auto lip = key.removeFromBottom (juce::jmax (2.0f, key.getHeight() * 0.04f));
    g.setColour ((isDown ? juce::Colours::white : neon.brighter (0.5f)).withAlpha (0.9f));
    g.fillRoundedRectangle (lip.reduced (2.0f, 0.0f), 1.0f);
    g.setGradientFill (juce::ColourGradient (neon.brighter (0.4f).withAlpha (isDown ? 0.0f : 0.35f), 0.0f, lip.getY(),
                                             neon.withAlpha (0.0f), 0.0f, lip.getY() - key.getHeight() * 0.25f, false));
    g.fillRect (key.withTop (lip.getY() - key.getHeight() * 0.25f).withBottom (lip.getY()).reduced (1.0f, 0.0f));

    // The C keys named, small and dark at the front.
    if (midiNoteNumber % 12 == 0)
    {
        g.setColour (juce::Colour (0xff1a080c).withAlpha (isDown ? 0.75f : 0.6f));
        g.setFont (juce::FontOptions (juce::jmin (11.0f, area.getWidth() * 0.4f), juce::Font::bold));
        g.drawText (juce::MidiMessage::getMidiNoteName (midiNoteNumber, true, true, getOctaveForMiddleC()),
                    area.withTrimmedBottom (area.getHeight() * 0.06f).withTrimmedLeft (3.0f), juce::Justification::bottomLeft, false);
    }
}

void NeonKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    const auto neon = neonAt (positionOf (area));
    const auto radius = juce::jmin (3.0f, area.getWidth() * 0.18f);
    // Its shadow on the glowing keys, then the key: black, a neon rim, a lit lip at its front.
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (area.expanded (1.5f, 0.0f).withTrimmedTop (-1.0f).withHeight (area.getHeight() + 2.0f), radius + 1.0f);
    if (isDown)
    {
        g.setGradientFill (juce::ColourGradient (neon.darker (0.6f), 0.0f, area.getY(), neon.interpolatedWith (juce::Colours::white, 0.5f), 0.0f,
                                                 area.getBottom(), false));
        g.fillRoundedRectangle (area, radius);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (isOver ? 0xff221a1e : 0xff0c0a0c), 0.0f, area.getY(),
                                                 juce::Colour (isOver ? 0xff2e2226 : 0xff171214), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, radius);
        g.setColour (juce::Colour (0xff2e282c));   // the glint down its left side
        g.fillRect (area.reduced (area.getWidth() * 0.18f, 0.0f).withWidth (1.2f).withTrimmedTop (3.0f).withTrimmedBottom (area.getHeight() * 0.18f));
    }
    g.setColour (neon.withAlpha (isDown ? 0.9f : 0.55f));
    g.drawRoundedRectangle (area.reduced (0.5f), radius, 1.0f);
    g.setColour (neon.brighter (0.3f).withAlpha (isDown ? 1.0f : 0.8f));
    g.fillRect (area.reduced (area.getWidth() * 0.2f, 0.0f).removeFromBottom (area.getHeight() * 0.06f).translated (0.0f, -area.getHeight() * 0.05f));
}
