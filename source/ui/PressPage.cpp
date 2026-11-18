#include "PressPage.h"

#include "dsp/synth/Press.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pink (0xffff2d6e), red (0xffff2a2e), orange (0xffff6a24), silk (0xffe8dcc4);

    juce::String noteName (int note)
    {
        return synth::sirocco::keyNames[((note % 12) + 12) % 12] + juce::String (note / 12 - 1);
    }
}

PressDisplay::PressDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("A koto from above: thirteen strings over their bridges, tuned to KEY and SCALE. The string of the last "
                "note pressed glows and bends under the hand as far as it's pressed now. Along the bottom, the five "
                "styles, and the chosen one's shape: the note struck, then the press on the next WHEN line.");
    startTimerHz (30);
}

void PressDisplay::timerCallback()
{
    const auto count = processor.getPressCount();
    if (count != shownCount)
    {
        shownCount = count;
        flash = 1.0f;
    }
    else
        flash = juce::jmax (0.0f, flash - 0.06f);
    repaint();
}

void PressDisplay::paint (juce::Graphics& g)
{
    namespace pr = synth::press;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getPressSettings();
    const auto live = settings.pressing();
    const auto last = processor.getPressLast();
    const auto lastNote = last >= 0 ? last % 1000 : -1;
    const auto lastStyle = last >= 0 ? last / 1000 : -1;
    const auto now = live ? juce::jmax (0, processor.getPressNow()) : 0;

    auto view = bounds.reduced (2.0f);
    g.setColour (juce::Colour (0xff070304));
    g.fillRoundedRectangle (view, 6.0f);

    // The top bar.
    auto bar = view.reduced (14.0f, 6.0f).removeFromTop (22.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    const auto presses = processor.getPressCount();
    g.drawText (! settings.on ? juce::String ("OFF") : ! live ? juce::String ("RESTING")
                : juce::String (juce::roundToInt (100.0f * settings.amount)) + "% OF NOTES PRESSED"
                      + (lastNote >= 0 ? "   LAST: " + noteName (lastNote) + " " + pr::styleNames[juce::jlimit (0, 3, lastStyle)] : juce::String())
                      + (presses > 0 ? "   " + juce::String (presses) + (presses == 1 ? " PRESS" : " PRESSES") : juce::String()),
                bar, juce::Justification::centredLeft, false);
    g.drawText ("ON THE NEXT " + pr::divisionNames[settings.division] + "   " + pr::stepNames[settings.steps] + "   "
                    + synth::sirocco::keyNames[settings.key] + " " + synth::arp::scaleNames[settings.scale].toUpperCase(),
                bar, juce::Justification::centredRight, false);

    // The koto: a long body, thirteen strings, each over its bridge, tuned up the scale from KEY.
    auto body = view.reduced (14.0f, 0.0f).withTrimmedTop (32.0f).withTrimmedBottom (96.0f);
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a2112), body.getX(), body.getY(),
                                                 juce::Colour (0xff1e1009), body.getX(), body.getBottom(), false));
        g.fillRoundedRectangle (body, 10.0f);
        g.setColour (juce::Colour (0xff5a3720));
        g.drawRoundedRectangle (body, 10.0f, 1.5f);
        // The wood's grain.
        g.setColour (juce::Colour (0x14ffffff));
        for (int k = 1; k < 9; ++k)
        {
            const auto y = body.getY() + body.getHeight() * (float) k / 9.0f;
            g.drawLine (body.getX() + 12.0f, y + 2.0f * std::sin ((float) k), body.getRight() - 12.0f, y - 1.5f * std::cos ((float) k * 1.7f), 0.8f);
        }

        std::array<int, 12> scaleNotes {};
        const auto count = juce::jmax (1, synth::sirocco::scaleNotes (settings.scale, scaleNotes));
        const auto base = 48 + settings.key;
        const auto stringNote = [&] (int k) { return base + 12 * (k / count) + scaleNotes[(size_t) (k % count)]; };
        constexpr int strings = 13;
        const auto top = body.getY() + body.getHeight() * 0.1f, span = body.getHeight() * 0.8f;
        const auto handX = body.getX() + body.getWidth() * 0.22f;    // where the left hand presses
        const auto bridgeX0 = body.getX() + body.getWidth() * 0.34f;  // the bridges, in a slant
        const auto bridgeX1 = body.getX() + body.getWidth() * 0.62f;
        const auto pressedString = [&]
        {
            if (lastNote < 0)
                return -1;
            // The string nearest the note (a note beyond the strings counted an octave in, until it's among them).
            auto note = lastNote;
            while (note < stringNote (0) - 1)
                note += 12;
            while (note > stringNote (strings - 1) + 1)
                note -= 12;
            auto best = -1, bestDistance = 1000;
            for (int k = 0; k < strings; ++k)
                if (const auto d = std::abs (stringNote (k) - note); d < bestDistance)
                {
                    bestDistance = d;
                    best = k;
                }
            return best;
        }();
        for (int k = 0; k < strings; ++k)
        {
            const auto y = top + span * (float) k / (float) (strings - 1);
            const auto lit = k == pressedString && live;
            const auto bend = lit ? juce::jlimit (0.0f, 1.0f, (float) now / 400.0f) : 0.0f;
            const auto bridgeX = bridgeX0 + (bridgeX1 - bridgeX0) * (float) k / (float) (strings - 1);
            // The string, pushed down (towards the next string) left of its bridge as far as it's pressed.
            juce::Path s;
            s.startNewSubPath (body.getX() + 10.0f, y);
            s.quadraticTo (handX, y + bend * span / (float) (strings - 1) * 0.9f, bridgeX, y);
            s.lineTo (body.getRight() - 58.0f, y);
            if (lit)
            {
                for (auto [w, a] : { std::pair { 7.0f, 0.12f }, { 3.5f, 0.3f } })
                {
                    g.setColour (orange.withAlpha (a * (0.4f + 0.6f * juce::jmax (flash, bend))));
                    g.strokePath (s, juce::PathStrokeType (w));
                }
                g.setColour (juce::Colours::white);
            }
            else
                g.setColour (silk.withAlpha (0.55f));
            g.strokePath (s, juce::PathStrokeType (lit ? 1.8f : 1.1f));
            // The bridge (ji): a small inverted V.
            juce::Path ji;
            ji.startNewSubPath (bridgeX - 7.0f, y + 5.0f);
            ji.lineTo (bridgeX, y - 4.0f);
            ji.lineTo (bridgeX + 7.0f, y + 5.0f);
            g.setColour (lit ? orange : juce::Colour (0xffcfc2a6));
            g.strokePath (ji, juce::PathStrokeType (2.0f));
            // The note each string sounds.
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.setColour (lit ? juce::Colours::white : ParagonColours::textDim);
            g.drawText (noteName (stringNote (k)), juce::Rectangle<float> (body.getRight() - 52.0f, y - 7.0f, 40.0f, 14.0f), juce::Justification::centredRight, false);
            // The hand, on the string pressed.
            if (lit && bend > 0.01f)
            {
                const auto hand = juce::Rectangle<float> (26.0f, 14.0f).withCentre ({ handX, y + bend * span / (float) (strings - 1) * 0.5f });
                g.setColour (pink.withAlpha (0.25f + 0.6f * bend));
                g.fillEllipse (hand);
            }
        }
        if (live && lastNote >= 0)
        {
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.setColour (juce::Colours::white);
            g.drawText ("+" + juce::String ((float) now / 100.0f, 2) + " ST", juce::Rectangle<float> (handX - 60.0f, body.getY() + 4.0f, 120.0f, 16.0f),
                        juce::Justification::centred, false);
        }
    }

    // The five styles; the chosen one's shape to its right.
    auto lower = view.reduced (14.0f, 8.0f).removeFromBottom (86.0f);
    auto chips = lower.removeFromLeft (lower.getWidth() * 0.55f);
    const auto chipWidth = chips.getWidth() / 5.0f;
    for (int k = 0; k < 5; ++k)
    {
        const auto r = juce::Rectangle<float> (chips.getX() + (float) k * chipWidth, chips.getY() + 14.0f, chipWidth, chips.getHeight() - 28.0f).reduced (4.0f, 2.0f);
        const auto on = k == settings.style;
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (pink, r.getX(), 0.0f, orange, r.getRight(), 0.0f, false));
            g.fillRoundedRectangle (r, 4.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff1a1315));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colour (0xff3a2e31));
            g.drawRoundedRectangle (r, 4.0f, 1.2f);
        }
        g.setColour (on ? juce::Colours::white : ParagonColours::textDim);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (pr::styleNames[k], r, juce::Justification::centred, false);
    }
    // The shape: the strike at the left, the WHEN line, the press after it (MIXED: each style faint, PRESS bright).
    auto graph = lower.reduced (10.0f, 6.0f);
    g.setColour (juce::Colour (0xff120a0c));
    g.fillRoundedRectangle (graph, 4.0f);
    const auto rise = pr::riseSeconds (settings.speed);
    const auto wait = 0.6 * rise;                 // the gap shown before the line
    const auto seconds = wait + 4.5 * rise;
    const auto xAt = [&] (double t) { return graph.getX() + 6.0f + (graph.getWidth() - 12.0f) * (float) ((t + wait) / seconds); };
    const auto yAt = [&] (float v) { return graph.getBottom() - 8.0f - (graph.getHeight() - 16.0f) * v; };
    g.setColour (juce::Colour (0xff3a2e31));
    g.drawLine (xAt (0.0), graph.getY() + 4.0f, xAt (0.0), graph.getBottom() - 4.0f, 1.0f);
    g.drawLine (graph.getX() + 4.0f, yAt (0.0f), graph.getRight() - 4.0f, yAt (0.0f), 1.0f);
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.setColour (ParagonColours::textDim);
    g.drawText ("STRIKE", juce::Rectangle<float> (graph.getX() + 4.0f, graph.getY() + 2.0f, 60.0f, 12.0f), juce::Justification::centredLeft, false);
    g.drawText (pr::divisionNames[settings.division], juce::Rectangle<float> (xAt (0.0) + 3.0f, graph.getY() + 2.0f, 40.0f, 12.0f), juce::Justification::centredLeft, false);
    for (int style = 0; style < 4; ++style)
    {
        if (settings.style != pr::mixed && style != settings.style)
            continue;
        juce::Path p;
        for (int i = 0; i <= 160; ++i)
        {
            const auto t = -wait + seconds * i / 160.0;
            const auto v = t < 0.0 ? (style == pr::release ? 1.0f : 0.0f) : pr::shapeAt (style, t, rise);
            const auto pt = juce::Point<float> (xAt (t), yAt (v));
            if (i == 0)
                p.startNewSubPath (pt);
            else
                p.lineTo (pt);
        }
        const auto bright = settings.style != pr::mixed || style == pr::hold;
        g.setColour ((bright ? orange : pink).withAlpha (bright ? 1.0f : 0.45f));
        g.strokePath (p, juce::PathStrokeType (bright ? 2.0f : 1.2f));
    }
}

//==============================================================================
PressPage::PressPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::pressOn, "ON", "PRESS on or off: notes pressed up the scale after they're struck, as a koto player presses the string.");
    make (controls, ParamIDs::pressAmount, "AMOUNT", "How many notes are pressed: none at 0 (the sound untouched), every one at the top. "
                                                     "The same passage at the same place in the song presses the same notes. "
                                                     "The mod matrix can move it: FX Press > Amount.");
    make (controls, ParamIDs::pressStyle, "STYLE", "PUSH: up and back. PRESS: up and held. RELEASE: struck pressed, let down to the "
                                                   "note. WOBBLE: a slow swing above the note. MIXED: each note one of those.");
    make (controls, ParamIDs::pressDivision, "WHEN", "The press waits for the next line of this grid after the strike, in time with the song.");
    make (controls, ParamIDs::pressSteps, "STEPS", "How far a press goes: the next note of the scale up, or the one after.");
    make (controls, ParamIDs::pressKey, "KEY", "The key the presses land in.");
    make (controls, ParamIDs::pressScale, "SCALE", "The scale the presses land on (hirajoshi, in and yo are the koto's own).");
    make (controls, ParamIDs::pressSpeed, "SPEED", "How long a press takes, 40 to 400 ms. The mod matrix can move it: FX Press > Speed.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> PressPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void PressPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("PRESS", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: notes pressed up the scale after they're struck, as a koto player presses the string",
                juce::Rectangle<int> (24, 38, 480, 16), juce::Justification::centredLeft, false);
}

void PressPage::resized()
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
