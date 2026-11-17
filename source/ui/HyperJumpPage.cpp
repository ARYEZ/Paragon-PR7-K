#include "HyperJumpPage.h"

#include "dsp/effects/HyperJump.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

HyperJumpDisplay::HyperJumpDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The build as a warp tunnel closing in, its beats along the bottom lit as they pass, the drop marked at "
                "the end; a flash as it lands.");
    startTimerHz (30);
}

void HyperJumpDisplay::paint (juce::Graphics& g)
{
    using H = effects::HyperJump;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getHyperJumpSettings();
    const auto& fx = processor.getHyperJump();
    const auto live = settings.on && ! fx.isResting();
    const auto progress = live ? fx.getProgress() : -1.0f;
    const auto building = progress >= 0.0f;
    const auto amount = live ? fx.getAmount() : 0.0f;
    const auto flash = live ? fx.getHitLevel() : 0.0f;
    const auto cyan = juce::Colour (0xff27e0f0), magenta = juce::Colour (0xffff2fa8);

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff04060e), view.getCentreX(), view.getCentreY(),
                                             juce::Colour (0xff0d1430), view.getX(), view.getY(), true));
    g.fillRoundedRectangle (view, 6.0f);

    // The tunnel: rings from the edge towards the centre; while building they crowd in and narrow (the image too).
    auto tunnel = view.reduced (view.getWidth() * 0.03f, view.getHeight() * 0.16f).withTrimmedBottom (view.getHeight() * 0.06f);
    const auto centre = tunnel.getCentre();
    const auto squeeze = building ? amount * progress : 0.0f;
    const auto time = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
    const auto speed = 0.4f + 3.0f * squeeze;
    constexpr int rings = 14;
    for (int k = 0; k < rings; ++k)
    {
        auto depth = std::fmod ((float) k / (float) rings + time * speed * 0.25f, 1.0f);
        const auto scale = std::pow (1.0f - depth, 2.2f);
        const auto w = tunnel.getWidth() * scale * (1.0f - 0.55f * squeeze);
        const auto h = tunnel.getHeight() * scale;
        auto ring = juce::Rectangle<float> (w, h).withCentre (centre);
        g.setColour (cyan.interpolatedWith (magenta, squeeze).withAlpha (0.12f + 0.55f * (1.0f - depth)));
        g.drawRoundedRectangle (ring, 4.0f + 10.0f * scale, 1.0f + 1.5f * scale);
    }
    // Streaks out from the centre as the build nears the drop.
    if (squeeze > 0.05f)
    {
        juce::Random random (7);
        for (int k = 0; k < 40; ++k)
        {
            const auto angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
            const auto r0 = std::fmod (random.nextFloat() + time * speed * 0.5f, 1.0f);
            const auto r1 = juce::jmin (1.0f, r0 + 0.25f * squeeze);
            const auto rx = tunnel.getWidth() * 0.5f, ry = tunnel.getHeight() * 0.5f;
            g.setColour (juce::Colours::white.withAlpha (0.5f * squeeze * (1.0f - r0)));
            g.drawLine (centre.x + std::cos (angle) * rx * r0, centre.y + std::sin (angle) * ry * r0,
                        centre.x + std::cos (angle) * rx * r1, centre.y + std::sin (angle) * ry * r1, 1.2f);
        }
    }

    // The build's beats along the bottom, lit as they pass, the drop at the end.
    {
        const auto bars = juce::jmax (1, fx.getBuildBars());
        const auto beats = 4 * bars;
        auto strip = view.reduced (view.getWidth() * 0.04f, 0.0f).removeFromBottom (view.getHeight() * 0.13f).withTrimmedBottom (view.getHeight() * 0.03f);
        auto dropBox = strip.removeFromRight (strip.getHeight() * 2.4f);
        const auto cell = strip.getWidth() / (float) beats;
        for (int b = 0; b < beats; ++b)
        {
            auto box = juce::Rectangle<float> (strip.getX() + (float) b * cell, strip.getY(), cell, strip.getHeight()).reduced (b % 4 == 0 ? 1.5f : 1.0f, 1.0f);
            const auto passed = building && progress * (float) beats >= (float) b;
            const auto now = building && (int) std::floor (progress * (float) beats) == b;
            g.setColour (passed ? cyan.interpolatedWith (magenta, (float) b / (float) juce::jmax (1, beats - 1)) : juce::Colour (0xff1a2236));
            g.fillRect (box);
            if (now)
            {
                g.setColour (juce::Colours::white);
                g.drawRect (box.expanded (1.0f), 1.5f);
            }
        }
        g.setColour (flash > 0.02f ? juce::Colours::white : magenta.withAlpha (building ? 0.9f : 0.4f));
        g.fillRoundedRectangle (dropBox.reduced (2.0f, 1.0f), 3.0f);
        g.setColour (juce::Colours::black);
        g.setFont (juce::FontOptions (juce::jmin (12.0f, dropBox.getHeight() * 0.6f), juce::Font::bold));
        g.drawText ("DROP", dropBox, juce::Justification::centred, false);
    }

    // The drop's flash.
    if (flash > 0.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.0f, 0.8f, flash)));
        g.fillRoundedRectangle (view, 6.0f);
    }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    const auto state = ! settings.on ? juce::String ("OFF") : flash > 0.05f ? juce::String ("DROP")
                     : building ? "BUILDING " + juce::String (juce::roundToInt (100.0f * progress)) + "%" : juce::String ("LANDED");
    g.drawText (state, bar, juce::Justification::centredLeft, false);
    g.drawText ("CLIMB +" + juce::String (juce::roundToInt (live ? fx.getClimb() : 0.0f)) + " ST   CHOP "
                    + (building ? juce::String ("1/") + juce::String (juce::roundToInt (4.0 / H::chopBeats (progress))) : juce::String ("-")),
                bar, juce::Justification::centred, false);
    const auto toDrop = live ? fx.getBeatsToDrop() : -1.0f;
    g.drawText (toDrop >= 0.0f ? "DROP IN " + juce::String (juce::jmax (1, (int) std::ceil (toDrop - 1.0e-3f))) + " BEATS" : H::modeNames[settings.mode],
                bar, juce::Justification::centredRight, false);
}

//==============================================================================
HyperJumpPage::HyperJumpPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::hyperjumpOn, "ON", "HYPERJUMP on or off: a build and a drop in time with the song.");
    make (controls, ParamIDs::hyperjumpAmount, "AMOUNT", "How big the build and the drop are (0: none, the sound untouched). "
                                                          "The mod matrix can move it: FX Hyperjump > Amount.");
    make (controls, ParamIDs::hyperjumpLength, "LENGTH", "How long the build is: 1, 2 or 4 bars, always ending on a bar line.");
    make (controls, ParamIDs::hyperjumpMode, "MODE", "LOOP: a build in the second half of every 2 x LENGTH bars, dropping on "
                                                      "the next. ONCE: one build from the next bar line, then landed until "
                                                      "AMOUNT goes back to 0.");
    make (controls, ParamIDs::hyperjumpRise, "RISE", "How high the sound climbs by the build's last beat, a step each beat "
                                                      "(0 to +12 semitones at full AMOUNT).");
    make (controls, ParamIDs::hyperjumpChop, "CHOP", "How deep the gate cuts, faster as the build goes (quarters, eighths, "
                                                      "sixteenths, thirty-seconds).");
    make (controls, ParamIDs::hyperjumpHit, "HIT", "The drop's boom and burst of space.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> HyperJumpPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void HyperJumpPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("HYPERJUMP", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: a build and a drop in time with the song, landing on the bar line", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void HyperJumpPage::resized()
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
