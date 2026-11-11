#include "CrossroadsPage.h"

#include "dsp/effects/Crossroads.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

CrossroadsDisplay::CrossroadsDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The road forks: the light way on the left, the dark way on the right; the traveller walks the way the "
                "sound is on now. Along the bottom the stretches of light and dark, in time with the song, the one now lit.");
    startTimerHz (30);
}

void CrossroadsDisplay::paint (juce::Graphics& g)
{
    namespace C = synth::crossroads;
    const auto bounds = getLocalBounds().toFloat();
    const auto pull = processor.getCrossroadsPull();
    const auto& fx = processor.getCrossroads();
    const auto live = pull.on && ! fx.isResting();
    const auto amount = live ? fx.getAmount() : 0.0f;
    const auto darkness = live ? fx.getDarkness() : 0.0f;
    const auto gold = juce::Colour (0xffffd27a), red = juce::Colour (0xffd8263e), ash = juce::Colour (0xff6b6470);

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2210).interpolatedWith (juce::Colour (0xff0d0d10), darkness * 0.6f),
                                             view.getX(), view.getY(),
                                             juce::Colour (0xff1c0608).interpolatedWith (juce::Colour (0xff0d0d10), (1.0f - darkness) * 0.6f),
                                             view.getRight(), view.getY(), false));
    g.fillRoundedRectangle (view, 6.0f);
    auto inner = view.reduced (view.getWidth() * 0.04f, view.getHeight() * 0.08f).withTrimmedTop (view.getHeight() * 0.07f);

    // THE FORK: the road from the bottom middle splitting to the left (light) and right (dark).
    auto field = inner.removeFromTop (inner.getHeight() * 0.7f);
    const auto base = juce::Point<float> (field.getCentreX(), field.getBottom());
    const auto split = juce::Point<float> (field.getCentreX(), field.getY() + field.getHeight() * 0.55f);
    const auto leftEnd = juce::Point<float> (field.getX() + field.getWidth() * 0.12f, field.getY() + field.getHeight() * 0.08f);
    const auto rightEnd = juce::Point<float> (field.getRight() - field.getWidth() * 0.12f, field.getY() + field.getHeight() * 0.08f);
    // The skies: a sun over the light way, a red moon over the dark.
    const auto glow = 0.25f + 0.75f * (1.0f - darkness) * (live ? 1.0f : 0.3f);
    g.setColour (gold.withAlpha (0.18f * glow));
    g.fillEllipse (juce::Rectangle<float> (field.getWidth() * 0.22f, field.getWidth() * 0.22f).withCentre (leftEnd));
    g.setColour (gold.withAlpha (0.8f * glow));
    g.fillEllipse (juce::Rectangle<float> (field.getWidth() * 0.06f, field.getWidth() * 0.06f).withCentre (leftEnd));
    const auto dim = 0.25f + 0.75f * darkness * (live ? 1.0f : 0.3f);
    g.setColour (red.withAlpha (0.18f * dim));
    g.fillEllipse (juce::Rectangle<float> (field.getWidth() * 0.22f, field.getWidth() * 0.22f).withCentre (rightEnd));
    g.setColour (red.withAlpha (0.85f * dim));
    g.fillEllipse (juce::Rectangle<float> (field.getWidth() * 0.06f, field.getWidth() * 0.06f).withCentre (rightEnd));
    const auto road = [&g] (juce::Point<float> a, juce::Point<float> b, float width, juce::Colour colour)
    {
        juce::Path p;
        p.startNewSubPath (a);
        p.lineTo (b);
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    const auto w = field.getHeight() * 0.07f;
    road (base, split, w * 1.3f, ash.withAlpha (0.8f));
    road (split, leftEnd, w, gold.withAlpha (0.3f + 0.6f * (1.0f - darkness) * (live ? 1.0f : 0.4f)));
    road (split, rightEnd, w, red.withAlpha (0.3f + 0.6f * darkness * (live ? 1.0f : 0.4f)));

    // The traveller: up the road and along the way the sound is on, how far by AMOUNT.
    const auto along = 0.25f + 0.6f * amount;
    const auto onWay = (darkness > 0.5f ? split + (rightEnd - split) * along : split + (leftEnd - split) * along);
    const auto at = live ? onWay : base + (split - base) * 0.5f;
    g.setColour ((darkness > 0.5f ? red : gold).brighter (0.3f).withAlpha (live ? 1.0f : 0.4f));
    g.fillEllipse (juce::Rectangle<float> (w * 1.6f, w * 1.6f).withCentre (at));
    g.setFont (juce::FontOptions (field.getHeight() * 0.11f, juce::Font::bold));
    g.setColour (gold.withAlpha (live ? 0.4f + 0.6f * (1.0f - darkness) : 0.25f));
    g.drawText ("LIGHT", field.withWidth (field.getWidth() * 0.4f).withTrimmedTop (field.getHeight() * 0.5f), juce::Justification::centred, false);
    g.setColour (red.brighter (0.2f).withAlpha (live ? 0.4f + 0.6f * darkness : 0.25f));
    g.drawText ("DARK", field.withTrimmedLeft (field.getWidth() * 0.6f).withTrimmedTop (field.getHeight() * 0.5f), juce::Justification::centred, false);

    // THE STRETCHES: four pairs of light and dark, the one now lit.
    inner.removeFromTop (inner.getHeight() * 0.18f);
    constexpr int cells = 8;
    const auto now = live ? fx.getPosition() * 2.0f : -1.0f;   // 0-2 through a light + dark pair
    const auto cw = inner.getWidth() / (float) cells;
    for (int k = 0; k < cells; ++k)
    {
        const auto isDark = k % 2 == 1;
        const auto cell = juce::Rectangle<float> (inner.getX() + cw * (float) k, inner.getY(), cw, inner.getHeight() * 0.6f).reduced (2.0f);
        const auto lit = k < 2 && now >= (float) k && now < (float) (k + 1);
        g.setColour ((isDark ? red : gold).withAlpha (lit ? 0.95f : (pull.on ? 0.3f : 0.12f)));
        g.fillRoundedRectangle (cell, 3.0f);
    }
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (C::lengthNames[pull.length] + " A SIDE", inner.withTrimmedTop (inner.getHeight() * 0.62f), juce::Justification::centredLeft, false);

    // The bar across the top: what the dark side does to the notes now.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    const auto steps = pull.on ? C::stepsFor (pull.amount, pull.pull) : 0;
    const auto key = synth::sirocco::keyNames[pull.key] + " " + synth::arp::scaleNames[pull.scale].toUpperCase();
    g.drawText (! pull.on ? juce::String ("OFF")
                          : steps == 0 ? "THE DARK SIDE COLOURS THE SOUND (PULLS FROM 50%)"
                                       : "THE DARK SIDE PULLS EVERY NOTE " + juce::String (steps) + (steps == 1 ? " STEP" : " STEPS") + " DOWN, IN " + key,
                bar, juce::Justification::centredLeft, false);
    g.drawText ("AMOUNT " + juce::String (juce::roundToInt (100.0f * amount)) + "%", bar, juce::Justification::centredRight, false);
}

//==============================================================================
CrossroadsPage::CrossroadsPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::crossOn, "ON", "CROSSROADS on or off: the sound pulled between light and dark in time with the song.");
    make (controls, ParamIDs::crossAmount, "AMOUNT", "How hard each side pulls. Up to halfway the two sides colour the sound; "
                                                      "from halfway the dark side also pulls every note down the scale, in key, "
                                                      "further as it rises (0: the sound untouched). The mod matrix can move it: "
                                                      "FX Crossroads > Amount.");
    make (controls, ParamIDs::crossLength, "LENGTH", "How long each side lasts before the other takes over: 1/2 bar to 4 bars, in time with the song.");
    make (controls, ParamIDs::crossKey, "KEY", "The key the pulled notes land in.");
    make (controls, ParamIDs::crossScale, "SCALE", "The scale the pulled notes land in: they always land on its notes.");
    make (controls, ParamIDs::crossPull, "PULL", "How far down the dark side pulls at the top of AMOUNT: a STEP, a THIRD or a FIFTH (in scale steps).");
    make (controls, ParamIDs::crossBend, "BEND", "How the notes get there: a quick slide (15 ms) to a slow bend (400 ms).");
    make (controls, ParamIDs::crossLight, "LIGHT", "How bright the light side: a shimmer an octave up and air on top.");
    make (controls, ParamIDs::crossDark, "DARK", "How dark the dark side: a growl an octave down, the sound darkened and roughened.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> CrossroadsPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void CrossroadsPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("CROSSROADS", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the sound pulled between light and dark, in time with the song", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void CrossroadsPage::resized()
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
