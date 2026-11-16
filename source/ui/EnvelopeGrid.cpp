#include "EnvelopeGrid.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "dsp/synth/Envelope.h"

namespace
{
    // PR7-D's neon: pink on the left, red, orange on the right (the family's colours only).
    const juce::Colour pink (0xffff2d6e), red (0xffff2a2e), orange (0xffff6a24);

    juce::ColourGradient neonAcross (juce::Rectangle<float> r, float alpha = 1.0f)
    {
        juce::ColourGradient grad (pink.withMultipliedAlpha (alpha), r.getX(), 0.0f, orange.withMultipliedAlpha (alpha), r.getRight(), 0.0f, false);
        grad.addColour (0.5, red.withMultipliedAlpha (alpha));
        return grad;
    }

    // A neon line: wide faint strokes under a bright core (a glow without a blur).
    template <typename SetColour>
    void neonStroke (juce::Graphics& g, const juce::Path& path, float width, SetColour&& setColour)
    {
        static constexpr std::array<std::pair<float, float>, 4> layers { { { 7.0f, 0.05f }, { 4.0f, 0.10f }, { 2.0f, 0.28f }, { 1.0f, 1.0f } } };
        for (const auto& [scale, alpha] : layers)
        {
            setColour (alpha);
            g.strokePath (path, juce::PathStrokeType (width * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Squeezed time, as the rack's envelope displays: each stage as wide as the square root of its time.
    float stageWidth (float ms) { return std::sqrt (juce::jmax (0.0f, ms)) + 2.0f; }
    const float sustainWidth = stageWidth (600.0f);

    // Glowing text for the readouts.
    void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour colour,
                   float size, juce::Justification justification)
    {
        g.setFont (juce::FontOptions (size, juce::Font::bold));
        g.setColour (colour.withAlpha (0.22f));
        for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
            g.drawText (text, area.translated (dx, dy), justification, false);
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.15f));
        g.drawText (text, area, justification, false);
    }
}

//==============================================================================
EnvelopeGrid::Wall EnvelopeGrid::Wall::make (juce::Rectangle<float> area, const std::array<float, 8>& values)
{
    Wall w;
    w.area = area;
    w.values = values;
    const auto total = stageWidth (values[0]) + stageWidth (values[1]) + stageWidth (values[2]) + sustainWidth + stageWidth (values[4]);
    const auto scale = area.getWidth() / total;
    auto x = area.getX();
    w.xs[0] = x;
    x += stageWidth (values[0]) * scale; w.xs[1] = x;
    x += stageWidth (values[1]) * scale; w.xs[2] = x;
    x += stageWidth (values[2]) * scale; w.xs[3] = x;
    x += sustainWidth * scale;           w.xs[4] = x;
    w.xs[5] = area.getRight();
    return w;
}

juce::Path EnvelopeGrid::Wall::outline() const
{
    juce::Path path;
    path.startNewSubPath (xs[0], yFor (0.0f));
    for (int stage = 1; stage <= 5; ++stage)
    {
        const auto steps = stage == 2 || stage == 4 ? 1 : 32;
        for (int i = 1; i <= steps; ++i)
            path.lineTo (pointAt (stage, (float) i / (float) steps, stage == 4 ? 1.0f : 0.5f));
    }
    return path;
}

juce::Point<float> EnvelopeGrid::Wall::pointAt (int stage, float progress, float breath) const
{
    const auto p = juce::jlimit (0.0f, 1.0f, progress);
    const auto sustain = values[3];
    switch (stage)
    {
        case 1: return { xs[0] + (xs[1] - xs[0]) * p, yFor (synth::bend (p, values[5])) };
        case 2: return { xs[1] + (xs[2] - xs[1]) * p, yFor (1.0f) };
        case 3: return { xs[2] + (xs[3] - xs[2]) * p, yFor (1.0f + (sustain - 1.0f) * synth::bend (p, values[6])) };
        case 4: return { xs[3] + (xs[4] - xs[3]) * juce::jlimit (0.0f, 1.0f, breath), yFor (sustain) };
        case 5: return { xs[4] + (xs[5] - xs[4]) * p, yFor (sustain * (1.0f - synth::bend (p, values[7]))) };
        default: return { xs[0], yFor (0.0f) };
    }
}

//==============================================================================
EnvelopeGrid::EnvelopeGrid (ParagonX1AudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);   // a picture: the controls are in the rack
    startTimerHz (30);
}

std::array<float, 8> EnvelopeGrid::envelopeValues (int which) const
{
    const auto& ids = ParamIDs::filterIds[0];
    const std::array<const char*, 8> filterIds { ids.attack, ids.hold, ids.decay, ids.sustain, ids.release, ids.attackCurve, ids.decayCurve, ids.releaseCurve };
    const std::array<const char*, 8> ampIds { ParamIDs::ampAttack, ParamIDs::ampHold, ParamIDs::ampDecay, ParamIDs::ampSustain, ParamIDs::ampRelease,
                                              ParamIDs::ampAttackCurve, ParamIDs::ampDecayCurve, ParamIDs::ampReleaseCurve };
    std::array<float, 8> values {};
    for (size_t k = 0; k < 8; ++k)
        if (auto* raw = processor.getParameters().getRawParameterValue (which == 0 ? filterIds[k] : ampIds[k]))
            values[k] = raw->load();
    return values;
}

juce::String EnvelopeGrid::readout (int which) const
{
    const auto& ids = ParamIDs::filterIds[0];
    const std::array<const char*, 5> filterIds { ids.attack, ids.hold, ids.decay, ids.sustain, ids.release };
    const std::array<const char*, 5> ampIds { ParamIDs::ampAttack, ParamIDs::ampHold, ParamIDs::ampDecay, ParamIDs::ampSustain, ParamIDs::ampRelease };
    static const std::array<const char*, 5> letters { "A", "H", "D", "S", "R" };
    juce::String text;
    for (size_t k = 0; k < 5; ++k)
    {
        if (k == 1 && envelopeValues (which)[1] <= 0.0f)
            continue;   // HOLD only when it holds
        if (auto* parameter = processor.getParameters().getParameter (which == 0 ? filterIds[k] : ampIds[k]))
            text << (text.isEmpty() ? "" : "   ") << letters[k] << " " << parameter->getCurrentValueAsText();
    }
    return text;
}

juce::String EnvelopeGrid::playText (bool secondLine) const
{
    const auto textOf = [this] (const char* id)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        return parameter != nullptr ? parameter->getCurrentValueAsText().toUpperCase() : juce::String();
    };
    if (secondLine)
    {
        const auto playing = processor.getActiveVoices();
        return textOf (ParamIDs::voices) + " VOICES  \xc2\xb7  " + juce::String (playing) + " PLAYING";
    }
    // LEGATO only means something in a mono mode.
    auto* legato = processor.getParameters().getRawParameterValue (ParamIDs::legato);
    auto* mode = processor.getParameters().getRawParameterValue (ParamIDs::playMode);
    const auto mono = mode != nullptr && mode->load() > 0.5f;
    return textOf (ParamIDs::playMode) + (mono && legato != nullptr && legato->load() > 0.5f ? "  \xc2\xb7  LEGATO" : "");
}

void EnvelopeGrid::resized()
{
    screen = getLocalBounds().toFloat().reduced (1.5f);
    horizon = screen.getY() + screen.getHeight() * 0.27f;
    floorArea = screen.withTop (horizon);
    layoutWalls();
    backgroundScale = 0.0f;   // drawn again at the new size
}

void EnvelopeGrid::layoutWalls()
{
    const auto base = screen.getY() + screen.getHeight() * 0.80f;
    const auto height = screen.getHeight() * 0.42f;
    const auto left = screen.getX() + screen.getWidth() * 0.055f, right = screen.getX() + screen.getWidth() * 0.535f;
    const auto width = screen.getWidth() * 0.41f;
    walls[0] = Wall::make ({ left, base - height, width, height }, envelopeValues (0));
    walls[1] = Wall::make ({ right, base - height, width, height }, envelopeValues (1));
}

void EnvelopeGrid::timerCallback()
{
    // What the still picture shows: the envelopes, the readouts, FILTER on or off.
    std::vector<float> values;
    for (int w = 0; w < 2; ++w)
        for (auto v : envelopeValues (w))
            values.push_back (v);
    juce::StringArray texts { readout (0), readout (1), playText (false), playText (true) };
    auto* on = processor.getParameters().getRawParameterValue (ParamIDs::filterIds[0].on);
    const auto filterNowOn = on == nullptr || on->load() > 0.5f;
    if (values != shownValues || texts != shownTexts || filterNowOn != filterOn)
    {
        shownValues = std::move (values);
        shownTexts = std::move (texts);
        filterOn = filterNowOn;
        layoutWalls();
        backgroundScale = 0.0f;
        repaint();
    }

    // The sparks: where the newest note is in each envelope.
    breathPhase = std::fmod (breathPhase + 1.0f / 30.0f * 0.5f, 1.0f);
    const std::array<int, 2> states { filterOn ? processor.getFilterSpark() : -1, processor.getAmpSpark() };
    for (size_t w = 0; w < 2; ++w)
    {
        auto& spark = sparks[w];
        const auto old = spark.drawnArea;
        const auto state = states[w];
        if (state >= 0)
        {
            const auto stage = state / 10000;
            const auto progress = (float) (state % 10000) / 9999.0f;
            const auto breath = 0.5f + 0.08f * std::sin (juce::MathConstants<float>::twoPi * breathPhase);
            const auto now = walls[w].pointAt (stage, progress, breath);
            if (spark.now.x < 0.0f || spark.now.getDistanceFrom (now) > 0.5f)
            {
                for (int k = (int) spark.trail.size() - 1; k > 0; --k)
                    spark.trail[(size_t) k] = spark.trail[(size_t) k - 1];
                spark.trail[0] = spark.now.x < 0.0f ? now : spark.now;
                spark.trailLength = juce::jmin ((int) spark.trail.size(), spark.trailLength + 1);
            }
            else if (spark.trailLength > 0)
                --spark.trailLength;   // standing still: the trail catches up
            spark.now = now;
            spark.fade = 1.0f;
        }
        else if (spark.fade > 0.0f)
        {
            spark.fade = juce::jmax (0.0f, spark.fade - 0.12f);   // gone in about a quarter of a second
            if (spark.trailLength > 0)
                --spark.trailLength;
            if (spark.fade <= 0.0f)
            {
                spark.now = { -1.0f, -1.0f };
                spark.trailLength = 0;
            }
        }
        else
            continue;

        juce::Rectangle<float> area;
        if (spark.now.x >= 0.0f)
        {
            area = juce::Rectangle<float> (spark.now, spark.now);
            for (int k = 0; k < spark.trailLength; ++k)
                area = area.getUnion (juce::Rectangle<float> (spark.trail[(size_t) k], spark.trail[(size_t) k]));
            area = area.expanded (28.0f);
        }
        spark.drawnArea = area;
        repaint (old.getUnion (area).getSmallestIntegerContainer());
    }
}

void EnvelopeGrid::paint (juce::Graphics& g)
{
    const auto scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! background.isValid() || std::abs (scale - backgroundScale) > 0.01f)
        renderBackground (scale);
    g.drawImage (background, getLocalBounds().toFloat());
    for (int w = 0; w < 2; ++w)
        paintSpark (g, w);
}

void EnvelopeGrid::renderBackground (float scale)
{
    backgroundScale = scale;
    background = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                              juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale));

    constexpr float corner = 11.0f;
    juce::Path glass;
    glass.addRoundedRectangle (screen, corner);

    // The screen: dark glass, a red haze rising to the horizon, the floor.
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (glass);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff060203), 0.0f, screen.getY(),
                                                 juce::Colour (0xff3a0a10), 0.0f, horizon, false));
        g.fillRect (screen.withBottom (horizon));
        g.setColour (juce::Colour (0xff070203));
        g.fillRect (floorArea);

        // The grid: lines running to the vanishing point, cross lines closing up towards the horizon.
        const auto vx = screen.getCentreX();
        const auto depth = floorArea.getHeight();
        for (int k = -40; k <= 40; ++k)
        {
            g.setColour (red.withAlpha (0.42f));
            g.drawLine (vx + (float) k * screen.getWidth() * 0.0047f, horizon,
                        vx + (float) k * screen.getWidth() * 0.047f, floorArea.getBottom() + depth * 0.4f, 1.0f);
        }
        for (int k = 1; k <= 15; ++k)
        {
            const auto f = std::pow ((float) k / 15.0f, 2.2f);
            g.setColour (red.withAlpha (0.15f + 0.4f * f));
            g.drawHorizontalLine (juce::roundToInt (horizon + depth * f), screen.getX(), screen.getRight());
        }
        // A glow over the floor by the horizon.
        juce::ColourGradient haze (red.withAlpha (0.32f), vx, horizon, red.withAlpha (0.0f), vx + screen.getWidth() * 0.5f, horizon, true);
        g.setGradientFill (haze);
        g.fillRect (floorArea);

        // The walls of light.
        for (size_t w = 0; w < 2; ++w)
        {
            const auto& wall = walls[w];
            const auto colour = w == 0 ? pink : orange;
            const auto dim = w == 0 && ! filterOn ? 0.3f : 1.0f;
            const auto base = wall.yFor (0.0f);
            const auto top = wall.outline();
            auto area = top;
            area.lineTo (wall.xs[5], base);
            area.closeSubPath();
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.34f * dim), 0.0f, wall.area.getY(),
                                                     colour.withAlpha (0.0f), 0.0f, base, false));
            g.fillPath (area);
            // A faint reflection on the floor.
            juce::Path reflection (top);
            reflection.applyTransform (juce::AffineTransform::verticalFlip (base * 2.0f)
                                           .followedBy (juce::AffineTransform::scale (1.0f, 0.25f, 0.0f, base)));
            g.setColour (colour.withAlpha (0.22f * dim));
            g.strokePath (reflection, juce::PathStrokeType (1.4f));
            // Each stage's edge, faint, down to the floor.
            g.setColour (colour.withAlpha (0.3f * dim));
            for (int stage = 1; stage <= 4; ++stage)
            {
                const auto x = wall.xs[(size_t) stage];
                const auto p = wall.pointAt (stage, 0.0f, 0.0f);
                g.drawLine (x, p.y, x, base, 1.0f);
            }
            // The base and the top, glowing.
            juce::Path baseLine;
            baseLine.startNewSubPath (wall.xs[0], base);
            baseLine.lineTo (wall.xs[5], base);
            neonStroke (g, baseLine, 2.0f, [&] (float a) { g.setColour (colour.withAlpha (a * dim)); });
            neonStroke (g, top, 2.8f, [&] (float a) { g.setColour (colour.withAlpha (a * dim)); });
            // The stages' corners.
            g.setColour (juce::Colours::white.withAlpha (0.9f * dim));
            for (int stage = 1; stage <= 4; ++stage)
            {
                const auto p = wall.pointAt (stage, stage == 4 ? 1.0f : 0.0f, 1.0f);
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (stage == 4 ? wall.pointAt (5, 0.0f) : p));
            }
        }
    }

    // The horizon and the glass's neon edge.
    juce::Path horizonLine;
    horizonLine.startNewSubPath (screen.getX() + 4.0f, horizon);
    horizonLine.lineTo (screen.getRight() - 4.0f, horizon);
    neonStroke (g, horizonLine, 1.8f, [&] (float a) { g.setGradientFill (neonAcross (screen, a)); });
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    g.fillRoundedRectangle (screen.reduced (2.0f).withHeight (screen.getHeight() * 0.42f), corner - 1.0f);
    neonStroke (g, glass, 1.5f, [&] (float a) { g.setGradientFill (neonAcross (screen, a)); });

    // The readouts.
    const auto line = juce::jlimit (16.0f, 24.0f, screen.getHeight() * 0.048f);
    const auto top = screen.getY() + screen.getHeight() * 0.05f;
    for (size_t w = 0; w < 2; ++w)
    {
        const auto colour = w == 0 ? pink : orange;
        const auto x = walls[w].area.getX();
        glowText (g, w == 0 ? (filterOn ? "FILTER ENVELOPE" : "FILTER ENVELOPE  \xc2\xb7  FILTER OFF") : "AMP ENVELOPE",
                  { x, top, 600.0f, line }, colour.withMultipliedAlpha (w == 0 && ! filterOn ? 0.5f : 1.0f), line * 0.95f, juce::Justification::centredLeft);
        glowText (g, shownTexts[(int) w], { x, top + line * 1.25f, 600.0f, line * 0.85f },
                  colour.withMultipliedAlpha (0.8f), line * 0.78f, juce::Justification::centredLeft);
    }
    const auto right = juce::Rectangle<float> (screen.getRight() - 420.0f - screen.getWidth() * 0.03f, top, 420.0f, line);
    glowText (g, shownTexts[2], right, orange, line * 0.95f, juce::Justification::centredRight);
    glowText (g, shownTexts[3], right.translated (0.0f, line * 1.25f).withHeight (line * 0.85f), orange.withMultipliedAlpha (0.8f),
              line * 0.78f, juce::Justification::centredRight);
}

void EnvelopeGrid::paintSpark (juce::Graphics& g, int which) const
{
    const auto& spark = sparks[(size_t) which];
    if (spark.now.x < 0.0f || spark.fade <= 0.0f)
        return;
    const auto colour = which == 0 ? pink : orange;

    // The trail: a fading line of light behind it.
    auto from = spark.now;
    for (int k = 0; k < spark.trailLength; ++k)
    {
        const auto to = spark.trail[(size_t) k];
        const auto alpha = spark.fade * (1.0f - (float) k / (float) spark.trail.size());
        g.setColour (colour.withAlpha (0.25f * alpha));
        g.drawLine ({ from, to }, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f * alpha));
        g.drawLine ({ from, to }, 2.0f);
        from = to;
    }

    // The spark: a white core in a halo of its wall's colour.
    const auto halo = 24.0f;
    juce::ColourGradient glow (colour.withAlpha (spark.fade), spark.now.x, spark.now.y,
                               colour.withAlpha (0.0f), spark.now.x + halo, spark.now.y, true);
    glow.addColour (0.35, colour.withAlpha (0.55f * spark.fade));
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (halo * 2.0f, halo * 2.0f).withCentre (spark.now));
    g.setColour (colour.interpolatedWith (juce::Colours::white, 0.6f).withAlpha (spark.fade));
    g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (spark.now));
    g.setColour (juce::Colours::white.withAlpha (spark.fade));
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (spark.now));
}
