#include "RagtimePage.h"

#include "dsp/synth/Ragtime.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

RagtimeDisplay::RagtimeDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The paper roll: a bar of 16 steps, the bass's strikes on the lower lane and the chord's on the upper "
                "(with STRIDE off, every hole strikes everything). The biggest holes are the main beats, then the "
                "syncopations, then the pickups; AMOUNT punches them in that order. The step playing is lit.");
    startTimerHz (30);
}

void RagtimeDisplay::paint (juce::Graphics& g)
{
    namespace rag = synth::ragtime;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getRagtimeSettings();
    const auto stepNow = settings.playing() ? processor.getRagtimeStep() : -1;
    const auto tiers = settings.on ? rag::tiersAt (settings.amount) : 0;
    const auto paper = juce::Colour (0xffe9dcbc), ink = juce::Colour (0xff2a1d12), brass = juce::Colour (0xffd9a441);

    // The cabinet, and the roll across it.
    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a160b), view.getX(), view.getY(),
                                             juce::Colour (0xff120904), view.getRight(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);
    auto roll = view.reduced (view.getWidth() * 0.035f, view.getHeight() * 0.16f).withTrimmedTop (view.getHeight() * 0.05f);
    auto labels = roll.removeFromBottom (18.0f);
    g.setGradientFill (juce::ColourGradient (paper, roll.getX(), roll.getY(), paper.darker (0.18f), roll.getX(), roll.getBottom(), false));
    g.fillRect (roll);
    // The roll's spools (brass bars at the ends) and its sprocket edge.
    g.setColour (brass);
    g.fillRect (roll.withWidth (5.0f).translated (-5.0f, 0.0f));
    g.fillRect (roll.withX (roll.getRight()).withWidth (5.0f));
    g.setColour (ink.withAlpha (0.25f));
    for (auto x = roll.getX() + 6.0f; x < roll.getRight() - 4.0f; x += 12.0f)
    {
        g.fillEllipse (x, roll.getY() + 3.0f, 3.0f, 3.0f);
        g.fillEllipse (x, roll.getBottom() - 6.0f, 3.0f, 3.0f);
    }

    const auto cell = roll.getWidth() / (float) rag::numSteps;
    const auto laneChord = roll.getY() + roll.getHeight() * 0.33f, laneBass = roll.getY() + roll.getHeight() * 0.7f;
    g.setColour (ink.withAlpha (0.18f));
    g.drawHorizontalLine ((int) laneChord, roll.getX(), roll.getRight());
    g.drawHorizontalLine ((int) laneBass, roll.getX(), roll.getRight());
    for (int k = 0; k < rag::numSteps; ++k)
    {
        const auto x = roll.getX() + ((float) k + 0.5f) * cell;
        if (k % 4 == 0)
        {
            g.setColour (ink.withAlpha (0.3f));
            g.drawVerticalLine ((int) (roll.getX() + (float) k * cell), roll.getY() + 8.0f, roll.getBottom() - 8.0f);
        }
        if (k == stepNow)
        {
            g.setColour (brass.withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> (roll.getX() + (float) k * cell, roll.getY(), cell, roll.getHeight()));
        }
        const auto hit = rag::hitAt (settings.pattern, k);
        if (hit.who != rag::none)
        {
            const auto size = juce::jmin (cell * 0.62f, roll.getHeight() * 0.22f) * (hit.tier == 1 ? 1.0f : hit.tier == 2 ? 0.72f : 0.5f);
            const auto punched = hit.tier <= tiers;
            const auto who = settings.stride ? hit.who : (int) rag::both;
            for (const auto lane : { rag::chord, rag::bass })
            {
                if ((who & lane) == 0)
                    continue;
                const auto y = lane == rag::chord ? laneChord : laneBass;
                const auto hole = juce::Rectangle<float> (size * 1.4f, size).withCentre ({ x, y });
                if (punched)
                {
                    g.setColour (k == stepNow ? juce::Colour (0xffffe9a8) : juce::Colour (0xff150b05));
                    g.fillRoundedRectangle (hole, size * 0.25f);
                }
                else
                {
                    g.setColour (ink.withAlpha (0.35f));
                    g.drawRoundedRectangle (hole, size * 0.25f, 1.0f);
                }
            }
        }
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (juce::jmin (9.5f, cell * 0.3f), juce::Font::bold));
        if (k % 4 == 0)
            g.drawText (juce::String (k / 4 + 1), juce::Rectangle<float> (roll.getX() + (float) k * cell, labels.getY(), cell, labels.getHeight()),
                        juce::Justification::centred, false);
    }
    g.setColour (ink.withAlpha (0.55f));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (settings.stride ? "CHORD" : "", juce::Rectangle<float> (roll.getX() + 8.0f, laneChord - 22.0f, 80.0f, 12.0f), juce::Justification::centredLeft, false);
    g.drawText (settings.stride ? "BASS" : "", juce::Rectangle<float> (roll.getX() + 8.0f, laneBass - 22.0f, 80.0f, 12.0f), juce::Justification::centredLeft, false);

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    static const char* tierNames[] { "SILENT", "THE BEATS", "+ SYNCOPATION", "THE WHOLE ROLL" };
    g.drawText (! settings.on ? juce::String ("OFF") : rag::patternNames[settings.pattern] + "  " + tierNames[tiers], bar, juce::Justification::centredLeft, false);
    g.drawText (settings.stride ? "STRIDE: BASS AND CHORD APART" : "EVERY HOLE STRIKES ALL", bar, juce::Justification::centred, false);
    g.drawText (rag::divisionNames[settings.division] + "   SWING " + juce::String (juce::roundToInt (100.0f * settings.swing)) + "%",
                bar, juce::Justification::centredRight, false);
}

//==============================================================================
RagtimePage::RagtimePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::ragtimeOn, "ON", "RAGTIME on or off: every held note struck again on a player piano's roll, in time with the song.");
    make (controls, ParamIDs::ragtimeAmount, "AMOUNT", "How much of the roll plays: the main beats, then the syncopations, then the "
                                                        "pickups (0: none, the notes untouched). The mod matrix can move it: "
                                                        "FX Ragtime > Amount.");
    make (controls, ParamIDs::ragtimePattern, "PATTERN", "The roll: STRIDE (oom-pah), RAG (the chord in 3 + 3 + 2), CAKEWALK "
                                                          "(short-long-short), MARCH (the bass on every beat), ROLL (every step).");
    make (controls, ParamIDs::ragtimeDivision, "DIVISION", "How long each step is: sixteenths, eighths, sixteenth triplets or 32nds.");
    make (controls, ParamIDs::ragtimeSwing, "SWING", "Every second step late, by up to a third of a step.");
    make (controls, ParamIDs::ragtimeAccent, "ACCENT", "How much softer the unaccented strikes are than the main beats.");
    make (controls, ParamIDs::ragtimeLift, "LIFT", "How far the damper lifts the sound just before each strike: high, every "
                                                    "strike stands apart (even on a held pad); low, they only push.");
    make (controls, ParamIDs::ragtimeStride, "STRIDE", "On: the lowest note you hold is the left hand (the bass's holes), the "
                                                        "rest the right (the chord's). Off: every hole strikes everything.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> RagtimePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void RagtimePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("RAGTIME", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: every held note struck again on a player piano's roll, in time with the song", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void RagtimePage::resized()
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
    SynthPanel::layoutRow (controlRow, row, 96);
}
