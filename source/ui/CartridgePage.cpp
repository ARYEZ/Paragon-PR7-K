#include "CartridgePage.h"

#include "dsp/synth/Cartridge.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // The console's palette: a dark screen, a hot pink, an electric blue, a lime and a pale screen white.
    const juce::Colour screen (0xff0b0716), pink (0xffff3d8b), blue (0xff35c4ff), lime (0xffb7ff3d), pale (0xffe8ecff);

    struct Stage { const char* name; float from; juce::Colour colour; };
    const Stage stages[] = { { "TUNED", 0.001f, blue }, { "TAKING TURNS", synth::cartridge::arpFrom, lime },
                             { "CRUNCH", 0.5f, pink }, { "60 HZ FRAMES", synth::cartridge::framesFrom, pale } };

    // A rectangle drawn as chunky pixels (snapped to a grid `px` wide).
    void pixelBox (juce::Graphics& g, juce::Rectangle<float> r, float px)
    {
        const auto snap = [px] (float v) { return std::round (v / px) * px; };
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (snap (r.getX()), snap (r.getY()), snap (r.getRight()), snap (r.getBottom())));
    }
}

CartridgeDisplay::CartridgeDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The chip's four limits, lit as AMOUNT passes each: TUNED (its coarse tuning and 16 steps of loudness), TAKING "
                "TURNS (a chord played one note at a time, in time with the song at RATE), CRUNCH (its coarse output) and 60 HZ "
                "FRAMES (the turns at the console's frame rate, CHANNELS notes at most, glitches). The blocks are the notes in "
                "the chip; the lit one is sounding now.");
    startTimerHz (30);
}

void CartridgeDisplay::timerCallback()
{
    const auto count = processor.getCartridge().getGlitches();
    if (count != shownGlitches)
    {
        shownGlitches = count;
        shudder = 1.0f;
    }
    else
        shudder = juce::jmax (0.0f, shudder - 0.08f);
    repaint();
}

void CartridgeDisplay::paint (juce::Graphics& g)
{
    namespace cart = synth::cartridge;
    const auto settings = processor.getCartridgeSettings();
    const auto live = settings.active();
    const auto notes = live ? juce::jmax (0, processor.getCartridgeNotes()) : 0;
    const auto turn = live ? processor.getCartridgeTurn() : -1;

    auto view = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (screen);
    g.fillRoundedRectangle (view, 6.0f);
    g.setColour (pink.withAlpha (0.4f));
    g.drawRoundedRectangle (view.reduced (3.0f), 5.0f, 1.0f);
    const auto px = juce::jmax (3.0f, std::round (view.getHeight() / 70.0f));
    const auto shift = std::round (shudder * px * 2.0f) * ((shownGlitches & 1) != 0 ? 1.0f : -1.0f);   // a glitch shakes it

    // The top bar.
    auto bar = view.reduced (14.0f, 6.0f).removeFromTop (22.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    int reached = -1;
    for (int k = 0; k < 4; ++k)
        reached = live && settings.amount >= stages[k].from ? k : reached;
    g.drawText (! settings.on ? juce::String ("OFF") : reached < 0 ? juce::String ("RESTING")
                : "STAGE " + juce::String (reached + 1) + ": " + stages[reached].name + "   " + juce::String (notes)
                      + (notes == 1 ? " NOTE" : " NOTES") + " IN THE CHIP",
                bar, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.drawText ("RATE " + cart::rateNames[settings.rate] + "   CHANNELS " + juce::String (settings.channels) + "   GLITCHES "
                    + juce::String (shownGlitches),
                bar, juce::Justification::centredRight, false);

    auto body = view.reduced (18.0f, 0.0f).withTrimmedTop (36.0f).withTrimmedBottom (12.0f).translated (shift, 0.0f);

    // The stages: a level bar in four parts, filled to AMOUNT.
    auto stageRow = body.removeFromTop (body.getHeight() * 0.42f);
    auto barArea = stageRow.removeFromBottom (stageRow.getHeight() * 0.42f);
    const auto at = [&] (float amount) { return barArea.getX() + barArea.getWidth() * amount; };
    g.setColour (pale.withAlpha (0.08f));
    pixelBox (g, barArea, px);
    for (int k = 0; k < 4; ++k)
    {
        const auto from = stages[k].from, to = k < 3 ? stages[k + 1].from : 1.0f;
        auto part = juce::Rectangle<float>::leftTopRightBottom (at (k == 0 ? 0.0f : from), barArea.getY(), at (to), barArea.getBottom());
        const auto lit = live && settings.amount >= from;
        if (lit)
        {
            const auto end = juce::jmin (to, settings.amount);
            g.setColour (stages[k].colour.withAlpha (0.85f));
            pixelBox (g, part.withRight (at (end)).reduced (0.0f, px), px);
        }
        g.setColour (stages[k].colour.withAlpha (lit ? 1.0f : 0.3f));
        g.setFont (juce::FontOptions (juce::jmin (15.0f, stageRow.getHeight() * 0.32f), juce::Font::bold));
        g.drawText (stages[k].name, part.withY (stageRow.getY()).withHeight (stageRow.getHeight()), juce::Justification::centredLeft, false);
        g.drawVerticalLine ((int) part.getX(), barArea.getY(), barArea.getBottom());
    }

    // The channels: a block per note in the chip, the one sounding now lit; dim ones mark the room left at the top stage.
    body.removeFromTop (body.getHeight() * 0.14f);
    const auto slots = settings.frames() ? settings.channels : juce::jmax (notes, 4);
    const auto gap = px * 2.0f;
    const auto w = juce::jmin (body.getHeight() * 1.4f, (body.getWidth() - gap * (float) (slots - 1)) / (float) juce::jmax (1, slots));
    for (int k = 0; k < slots; ++k)
    {
        auto block = juce::Rectangle<float> (body.getX() + (w + gap) * (float) k, body.getY(), w, body.getHeight());
        const auto inChip = k < notes;
        const auto sounding = inChip && (! settings.turns() || (turn >= 0 && k == turn % juce::jmax (1, notes)));
        g.setColour ((sounding ? lime : inChip ? blue : pale).withAlpha (sounding ? 0.95f : inChip ? 0.4f : 0.07f));
        pixelBox (g, block, px);
        g.setColour (screen);
        g.setFont (juce::FontOptions (juce::jmin (16.0f, block.getHeight() * 0.3f), juce::Font::bold));
        if (inChip)
            g.drawText (juce::String (k + 1), block, juce::Justification::centred, false);
    }

    // Scanlines over it all.
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    for (auto y = view.getY(); y < view.getBottom(); y += px)
        g.fillRect (view.getX() + 4.0f, y, view.getWidth() - 8.0f, 1.0f);
}

//==============================================================================
CartridgePage::CartridgePage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::cartOn, "ON", "CARTRIDGE on or off: the synth forced through the limits of an old game console's sound chip.");
    make (controls, ParamIDs::cartAmount, "AMOUNT", "How far into the chip: at 0 the sound untouched; then its coarse tuning and stepped "
                                                    "loudness, from a quarter chords taken in turns, from half the crunch, from three "
                                                    "quarters the 60 Hz frames, the channel limit and the glitches. "
                                                    "The mod matrix can move it: FX Cartridge > Amount.");
    make (controls, ParamIDs::cartRate, "RATE", "How fast the notes of a chord take turns (in time with the song), until the top stage "
                                                "runs them at the console's 60 Hz frames.");
    make (controls, ParamIDs::cartChannels, "CHANNELS", "How many notes the chip sounds at once at the top stage: a newer note takes the "
                                                        "oldest's place.");
    make (controls, ParamIDs::cartCrunch, "CRUNCH", "How coarse the chip's output gets (the sound read at a lower rate and fewer levels).");
    make (controls, ParamIDs::cartGlitch, "GLITCH", "How often the cartridge 'wiggles' at the top stage: a frame of sound repeated.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> CartridgePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void CartridgePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("CARTRIDGE", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the synth forced through an old game console's sound chip, a limit at a time",
                juce::Rectangle<int> (24, 38, 480, 16), juce::Justification::centredLeft, false);
}

void CartridgePage::resized()
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
