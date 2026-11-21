#include "TalkBackPage.h"

#include "dsp/effects/TalkBack.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour gold (0xffffb43c), pink (0xffff2d6e), orange (0xffff6a24), bronze (0xff8a5a2b);
}

TalkBackDisplay::TalkBackDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The conversation: CALL is the last phrase you played, ANSWER the drum's strokes playing it back on the "
                "next bar line, each as high or low as your note was, the line bending as the drum's pitch glides and a "
                "long note's squeeze lifts it. The drum glows with each stroke.");
    startTimerHz (30);
}

void TalkBackDisplay::timerCallback()
{
    const auto strokes = processor.getTalkBack().getStrokeCount();
    if (strokes != shownStrokes)
    {
        shownStrokes = strokes;
        glow = 1.0f;
    }
    else
        glow = juce::jmax (0.0f, glow - 0.09f);
    repaint();
}

void TalkBackDisplay::paint (juce::Graphics& g)
{
    using T = effects::TalkBack;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getTalkBackSettings();
    const auto& fx = processor.getTalkBack();
    const auto picture = fx.getPicture();

    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff120a05), view.getX(), view.getY(), juce::Colour (0xff050302), view.getX(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);

    // The top bar.
    auto bar = view.reduced (14.0f, 6.0f).removeFromTop (22.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    const auto listening = settings.on && settings.amount >= 0.001f;
    g.drawText (! settings.on ? juce::String ("OFF") : ! listening ? juce::String ("LISTENING TO NOTHING (AMOUNT 0)")
                    : fx.isAnswering() ? juce::String ("ANSWERING") : juce::String ("LISTENING: PLAY A PHRASE, THEN REST"),
                bar, juce::Justification::centredLeft, false);
    g.drawText ("ANSWERS " + juce::String (juce::roundToInt (100.0f * T::answerChance (settings.amount))) + "% OF PHRASES   OWN TOUCHES "
                    + juce::String (juce::roundToInt (100.0f * T::ornamentChance (settings.amount))) + "%   WAIT " + T::waitNames[settings.wait],
                bar, juce::Justification::centredRight, false);

    auto scene = view.reduced (14.0f, 0.0f).withTrimmedTop (34.0f).withTrimmedBottom (10.0f);
    auto drumArea = scene.removeFromRight (juce::jmin (220.0f, scene.getWidth() * 0.26f));
    scene.removeFromRight (16.0f);

    // CALL and ANSWER: the phrase's span in beats (whole units), the same scale for both.
    auto span = (double) picture.unit;
    for (int k = 0; k < picture.notes; ++k)
        span = juce::jmax (span, (double) (picture.noteBeat[(size_t) k] + picture.noteLength[(size_t) k]));
    for (int k = 0; k < picture.strokes; ++k)
        span = juce::jmax (span, (double) picture.strokeBeat[(size_t) k] + 0.5);
    span = std::ceil (span / picture.unit) * picture.unit;
    const auto lane = [&] (juce::Rectangle<float> r, const juce::String& title, juce::Colour colour)
    {
        g.setColour (juce::Colour (0xff1b120b));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (0xff3a2a1c));
        for (int b = 0; b <= (int) span; ++b)
        {
            const auto x = r.getX() + r.getWidth() * (float) b / (float) span;
            g.drawVerticalLine (juce::roundToInt (x), r.getY(), r.getBottom());
        }
        g.setColour (colour.withAlpha (0.8f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (title, r.reduced (6.0f, 3.0f), juce::Justification::topLeft, false);
    };
    const auto yFor = [] (juce::Rectangle<float> r, float semis) { return r.getCentreY() - r.getHeight() * 0.38f * juce::jlimit (-1.0f, 1.0f, semis / 12.0f); };
    auto callArea = scene.removeFromTop (scene.getHeight() * 0.5f).reduced (0.0f, 4.0f);
    auto answerArea = scene.reduced (0.0f, 4.0f);
    lane (callArea, "CALL", gold);
    lane (answerArea, "ANSWER", orange);
    const auto xFor = [&] (juce::Rectangle<float> r, double beat) { return r.getX() + r.getWidth() * (float) (beat / span); };
    for (int k = 0; k < picture.notes; ++k)
    {
        const auto x0 = xFor (callArea, picture.noteBeat[(size_t) k]);
        const auto x1 = xFor (callArea, picture.noteBeat[(size_t) k] + juce::jmax (0.125f, picture.noteLength[(size_t) k]));
        const auto y = yFor (callArea, picture.noteValue[(size_t) k]);
        g.setColour (gold);
        g.fillRoundedRectangle (juce::Rectangle<float> (x0, y - 3.5f, juce::jmax (4.0f, x1 - x0 - 2.0f), 7.0f), 3.0f);
    }
    if (picture.strokes > 0)
    {
        juce::Path voice;
        for (int k = 0; k < picture.strokes; ++k)
        {
            const auto x = xFor (answerArea, picture.strokeBeat[(size_t) k]);
            const auto y = yFor (answerArea, picture.strokeSemis[(size_t) k]);
            if (k == 0)
                voice.startNewSubPath (x, y);
            else
                voice.lineTo (x, y);
        }
        g.setColour (orange.withAlpha (0.45f));
        g.strokePath (voice, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
        for (int k = 0; k < picture.strokes; ++k)
        {
            const auto x = xFor (answerArea, picture.strokeBeat[(size_t) k]);
            const auto y = yFor (answerArea, picture.strokeSemis[(size_t) k]);
            g.setColour (pink);
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }));
        }
    }
    else if (picture.notes > 0)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("not answered (AMOUNT under halfway answers that share of phrases)", answerArea, juce::Justification::centred, false);
    }

    // The talking drum: an hourglass, its cords pulled in as the pitch rises.
    {
        const auto squeeze = juce::jlimit (0.0f, 1.0f, 0.5f + fx.getPitchNow() / 24.0f);
        auto d = drumArea.reduced (drumArea.getWidth() * 0.12f, drumArea.getHeight() * 0.06f);
        const auto waist = d.getWidth() * (0.32f - 0.12f * squeeze);
        const auto top = d.getY(), bottom = d.getBottom(), cx = d.getCentreX();
        const auto headW = d.getWidth() * 0.5f;
        juce::Path body;
        body.startNewSubPath (cx - headW, top);
        body.quadraticTo (cx - waist, d.getCentreY(), cx - headW, bottom);
        body.lineTo (cx + headW, bottom);
        body.quadraticTo (cx + waist, d.getCentreY(), cx + headW, top);
        body.closeSubPath();
        g.setGradientFill (juce::ColourGradient (bronze.brighter (0.3f + 0.5f * glow), cx - headW, top, bronze.darker (0.6f), cx + headW, bottom, false));
        g.fillPath (body);
        // The cords.
        g.setColour (juce::Colour (0xffd9c39a).withAlpha (0.55f + 0.4f * glow));
        for (int k = 0; k < 9; ++k)
        {
            const auto t = (float) k / 8.0f;
            const auto xt = cx - headW + 2.0f * headW * t;
            const auto xm = cx + (xt - cx) * (waist / headW);
            g.drawLine (xt, top + 4.0f, xm, d.getCentreY(), 1.2f);
            g.drawLine (xm, d.getCentreY(), xt, bottom - 4.0f, 1.2f);
        }
        // The heads, lit by the stroke.
        for (const auto y : { top, bottom })
        {
            const auto head = juce::Rectangle<float> (2.0f * headW, d.getHeight() * 0.07f).withCentre ({ cx, y });
            g.setColour (juce::Colour (0xffe8d4ae).withAlpha (0.7f));
            g.fillEllipse (head);
            g.setColour (gold.withAlpha (0.9f * glow));
            g.fillEllipse (head.expanded (6.0f * glow));
        }
        g.setColour (ParagonColours::text);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (juce::String (juce::roundToInt (settings.pitchHz * std::exp2 (fx.getPitchNow() / 12.0f))) + " Hz   "
                        + juce::String (fx.getAnswers()) + " ANSWERS",
                    drumArea.removeFromBottom (16.0f), juce::Justification::centred, false);
    }
}

//==============================================================================
TalkBackPage::TalkBackPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::talkOn, "ON", "TALKBACK on or off: a talking drum answers each phrase you play, on the next bar line, following your melody.");
    make (top, ParamIDs::talkWait, "WAIT", "When the drum answers: at the NEXT BAR line after you rest, or the NEXT BEAT (snappier, for short calls).");
    make (controls, ParamIDs::talkAmount, "AMOUNT", "How much it answers: up to halfway, that share of your phrases; past it every phrase, and the "
                                                    "drummer adds flams and closing tags of his own and bends more. The mod matrix can move it: FX Talkback > Amount.");
    make (controls, ParamIDs::talkPitch, "PITCH", "The drum's middle note, where your phrase's middle note lands.");
    make (controls, ParamIDs::talkRange, "RANGE", "How much of each step of your melody the drum follows (a talking drum's squeeze covers less than a "
                                                  "melody: about two thirds sounds like speech).");
    make (controls, ParamIDs::talkBend, "BEND", "How far a long note bends up as it's held, the player squeezing the cords.");
    make (controls, ParamIDs::talkTone, "TONE", "Dark and soft (a padded stick, a long ring) to bright and crisp (the stick's crack, a short ring).");
    make (controls, ParamIDs::talkSpace, "SPACE", "How much of the drum goes into the preset's REVERB.");
    make (controls, ParamIDs::talkPan, "PAN", "Where the drum stands.");
    make (controls, ParamIDs::talkLevel, "LEVEL", "How loud the drum is. Your sound passes under it unchanged.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> TalkBackPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void TalkBackPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("TALKBACK", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: a talking drum answers each phrase you play on the next bar, following its rise and fall",
                juce::Rectangle<int> (24, 38, 520, 16), juce::Justification::centredLeft, false);
}

void TalkBackPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (540);
    back.setBounds (bar.removeFromRight (150).withSizeKeepingCentre (150, 30));
    bar.removeFromRight (20);
    top[0]->setBounds (bar.removeFromLeft (70));
    bar.removeFromLeft (10);
    top[1]->setBounds (bar.removeFromLeft (130));
    area.removeFromTop (8);
    auto controlRow = area.removeFromBottom (ParamControl::knobHeight);
    area.removeFromBottom (10);
    display.setBounds (area);
    std::vector<juce::Component*> row;
    for (auto& c : controls)
        row.push_back (c.get());
    SynthPanel::layoutRow (controlRow, row, 96);
}
