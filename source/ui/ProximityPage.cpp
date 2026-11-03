#include "ProximityPage.h"

#include "dsp/effects/Proximity.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr int lowNote = 21, highNote = 108;   // A0 to C8
    bool isBlack (int note) { const auto k = note % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
}

ProximityDisplay::ProximityDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The sound of playing, up close: each touch lights its key (presses by how hard, releases small), the pedal "
                "lights while it's down, and the microphone hangs nearer the more CLOSE is up. The faint line along the "
                "bottom is the room and its hum.");
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

void ProximityDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = (float) juce::jlimit (0.0, 0.2, (now - lastTick) / 1000.0);
    clock += dt;
    lastTick = now;
    const auto& fx = processor.getProximity();
    const auto count = fx.getTouchCount();
    for (int ago = juce::jmin (16, count - lastCount) - 1; ago >= 0; --ago)
    {
        const auto t = fx.getTouch (ago);
        if (t.kind == effects::Proximity::Kind::pedalDown || t.kind == effects::Proximity::Kind::pedalUp)
            pedalDown = t.kind == effects::Proximity::Kind::pedalDown;
        else if (processor.getProximitySettings().on)
            ripples.push_back ({ t.note, t.velocity, (int) t.kind, 0.0f });
    }
    lastCount = count;
    for (auto& r : ripples)
        r.age += dt;
    ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age > 1.2f; }), ripples.end());
    if (ripples.size() > 48)
        ripples.erase (ripples.begin(), ripples.begin() + (long) (ripples.size() - 48));
    repaint();
}

void ProximityDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07060a));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto s = processor.getProximitySettings();
    const auto& fx = processor.getProximity();
    const auto lit = s.on ? 1.0f : 0.3f;
    auto area = bounds.reduced (24.0f, 18.0f);
    g.setColour (s.on ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    const auto noiseText = s.noise <= 0.0f ? juce::String ("OFF") : juce::String (effects::Proximity::noiseDb (s.noise), 0) + " DB";
    const auto roomText = s.room <= 0.0f ? juce::String ("OFF") : juce::String (effects::Proximity::roomDb (s.room), 0) + " DB";
    g.drawText ("TOUCHES " + noiseText + "   ROOM " + roomText + "   HUM " + effects::Proximity::humNames[juce::jlimit (0, 2, s.hum)]
                    + "   CLOSE +" + juce::String (effects::Proximity::closeDb (s.close), 1) + " DB",
                area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (8.0f);

    // The room's hum along the bottom.
    auto humArea = area.removeFromBottom (22.0f);
    {
        const auto roomLevel = juce::jlimit (0.0f, 1.0f, (fx.getRoomDb() + 90.0f) / 50.0f);
        juce::Path wave;
        for (int x = 0; x <= (int) humArea.getWidth(); x += 3)
        {
            const auto ph = (double) x / 24.0 + clock * 6.0;
            const auto y = humArea.getCentreY() - roomLevel * humArea.getHeight() * 0.4f * (float) (std::sin (ph) + 0.35 * std::sin (2.0 * ph));
            if (x == 0) wave.startNewSubPath (humArea.getX(), y); else wave.lineTo (humArea.getX() + (float) x, y);
        }
        g.setColour (juce::Colour (0xff8fd0ff).withAlpha ((0.15f + 0.6f * roomLevel) * lit));
        g.strokePath (wave, juce::PathStrokeType (1.2f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("ROOM", humArea.withWidth (60.0f), juce::Justification::centredLeft, false);
    }
    area.removeFromBottom (8.0f);

    // The pedal.
    auto pedalArea = area.removeFromBottom (26.0f).withSizeKeepingCentre (90.0f, 22.0f);
    g.setColour ((pedalDown && s.on ? juce::Colour (0xffffb45a) : juce::Colour (0xff3a3540)).withAlpha (lit));
    g.fillRoundedRectangle (pedalArea, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("PEDAL", pedalArea, juce::Justification::centred, false);
    area.removeFromBottom (10.0f);

    // The keyboard: white keys across, black keys over them.
    auto keys = area.removeFromBottom (juce::jmin (110.0f, area.getHeight() * 0.45f));
    int whites = 0;
    for (int n = lowNote; n <= highNote; ++n)
        whites += isBlack (n) ? 0 : 1;
    const auto whiteWidth = keys.getWidth() / (float) whites;
    std::array<float, 128> keyX {}, keyLight {};
    for (const auto& r : ripples)
    {
        const auto n = juce::jlimit (0, 127, juce::roundToInt (r.note));
        const auto strength = (r.kind == 0 ? 0.4f + 0.6f * r.velocity : 0.35f) * std::exp (-3.0f * r.age);
        keyLight[(size_t) n] = juce::jmax (keyLight[(size_t) n], strength);
    }
    int w = 0;
    for (int n = lowNote; n <= highNote; ++n)
        if (! isBlack (n))
        {
            const auto x = keys.getX() + (float) w * whiteWidth;
            keyX[(size_t) n] = x + whiteWidth * 0.5f;
            const auto l = keyLight[(size_t) n] * lit;
            g.setColour (juce::Colour (0xffd8d2c8).withAlpha (0.35f + 0.6f * lit).interpolatedWith (juce::Colour (0xffffb45a), l));
            g.fillRect (x + 0.5f, keys.getY(), whiteWidth - 1.0f, keys.getHeight());
            ++w;
        }
    w = 0;
    for (int n = lowNote; n <= highNote; ++n)
    {
        if (! isBlack (n))
        {
            ++w;
            continue;
        }
        const auto x = keys.getX() + (float) w * whiteWidth - whiteWidth * 0.32f;
        keyX[(size_t) n] = x + whiteWidth * 0.32f;
        const auto l = keyLight[(size_t) n] * lit;
        g.setColour (juce::Colour (0xff15131a).interpolatedWith (juce::Colour (0xffffb45a), l));
        g.fillRect (x, keys.getY(), whiteWidth * 0.64f, keys.getHeight() * 0.62f);
    }

    // The ripples: each touch rising from its key.
    for (const auto& r : ripples)
    {
        const auto n = juce::jlimit (0, 127, juce::roundToInt (r.note));
        const auto x = keyX[(size_t) n];
        const auto radius = 6.0f + 70.0f * r.age * (r.kind == 0 ? 1.0f : 0.5f);
        const auto alpha = (r.kind == 0 ? 0.3f + 0.7f * r.velocity : 0.35f) * std::exp (-2.5f * r.age) * lit;
        g.setColour ((r.kind == 0 ? juce::Colour (0xffffb45a) : juce::Colour (0xff8fd0ff)).withAlpha (alpha));
        g.drawEllipse (x - radius, keys.getY() - radius * 0.5f, 2.0f * radius, radius, 1.5f);
    }

    // The microphone over the keys: lower and nearer the more CLOSE is up.
    const auto close = juce::jlimit (0.0f, 1.0f, s.close);
    const auto micY = area.getY() + (keys.getY() - area.getY() - 40.0f) * (0.1f + 0.85f * close);
    const auto micX = area.getCentreX();
    g.setColour (juce::Colour (0xff6a6470).withAlpha (lit));
    g.drawLine (micX, area.getY(), micX, micY, 2.0f);
    g.setColour (juce::Colour (0xffc8c2cc).withAlpha (lit));
    g.fillRoundedRectangle (micX - 9.0f, micY, 18.0f, 30.0f, 8.0f);
    g.setColour (juce::Colour (0xff3a3540).withAlpha (lit));
    for (int k = 1; k < 4; ++k)
        g.drawHorizontalLine ((int) (micY + 6.0f * (float) k), micX - 7.0f, micX + 7.0f);
    g.setColour (juce::Colour (0xffffb45a).withAlpha (0.18f * close * lit));
    g.fillEllipse (micX - 70.0f * (1.2f - close), micY + 30.0f, 140.0f * (1.2f - close), (keys.getY() - micY - 30.0f) * 1.1f);
}

//==============================================================================
ProximityPage::ProximityPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::proximityOn, "ON", "PROXIMITY on or off: the sound of playing, heard up close (every noise made by the synth, no recordings).");
    make (controls, ParamIDs::proximityNoise, "NOISE", "The touches of playing: the felt hammer's thump as each note starts, the key's click and the damper as you let go, the pedal going down and up. The mod matrix can move it: FX Proximity > Noise.");
    make (controls, ParamIDs::proximityTone, "TONE", "How bright the felt and the clicks are: soft and muffled, or crisp.");
    make (controls, ParamIDs::proximityRoom, "ROOM", "The room's air and the electronics' hum, rising with the music and fading a couple of seconds after it. The mod matrix can move it.");
    make (controls, ParamIDs::proximityHum, "HUM", "The hum under the room: off, or mains hum at 50 or 60 Hz.");
    make (controls, ParamIDs::proximityClose, "CLOSE", "The extra low end a microphone picks up close to the instrument (up to +6 dB around 180 Hz). The mod matrix can move it.");
    make (controls, ParamIDs::proximityWidth, "WIDTH", "Each note's noises placed where it is on the keyboard: low notes left, high notes right.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> ProximityPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void ProximityPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("PROXIMITY", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the sound of playing, heard up close", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void ProximityPage::resized()
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
