#include "LockstepPage.h"

#include "dsp/effects/Lockstep.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

LockstepDisplay::LockstepDisplay (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("Two bars of steps, the one now lit: each block opens as long as GATE says, the off-beat steps lower by "
                "ACCENT. Below, the ranks fall into line as AMOUNT rises and flash with each stamp.");
    startTimerHz (30);
}

namespace
{
    juce::String stageOf (float amount)
    {
        return amount < 0.001f ? "FREE" : amount < 0.5f ? "RANKS" : amount < 0.999f ? "MARCH" : "DRILLED";
    }
}

void LockstepDisplay::paint (juce::Graphics& g)
{
    using L = effects::Lockstep;
    const auto bounds = getLocalBounds().toFloat();
    const auto settings = processor.getLockstepSettings();
    const auto& fx = processor.getLockstep();
    const auto live = settings.on;
    const auto amount = live ? fx.getAmount() : 0.0f;
    const auto stamp = live ? fx.getStampLevel() : 0.0f;

    // The ground: grey stone, colder as it's drilled.
    auto view = bounds.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff22201f).interpolatedWith (juce::Colour (0xff17191c), amount), view.getX(), view.getY(),
                                             juce::Colour (0xff121213), view.getX(), view.getBottom(), false));
    g.fillRoundedRectangle (view, 6.0f);

    // Two bars of steps.
    const auto beatsPerStep = L::divisionBeats[(size_t) juce::jlimit (0, 3, settings.division)];
    const auto stepsShown = juce::jmax (1, juce::roundToInt (8.0 / beatsPerStep));
    const auto now = live ? fx.getSteps() : 0.0;
    const auto current = (int) std::fmod (std::floor (now), (double) stepsShown);
    auto lane = view.reduced (24.0f, 0.0f).withTrimmedTop (34.0f).withHeight (view.getHeight() * 0.32f);
    const auto stepW = lane.getWidth() / (float) stepsShown;
    const auto gate = L::gateShare (settings.gate), offBeat = L::offBeatLevel (settings.accent);
    const auto red = juce::Colour (0xffc8102e);
    for (int k = 0; k < stepsShown; ++k)
    {
        const auto startBeats = k * beatsPerStep;
        const auto onBeat = std::abs (startBeats - std::round (startBeats)) < 1.0e-6;
        const auto level = onBeat ? 1.0f : offBeat;
        const auto cell = juce::Rectangle<float> (lane.getX() + k * stepW, lane.getY(), stepW, lane.getHeight());
        const auto h = cell.getHeight() * (1.0f - amount * (1.0f - level));
        const auto block = juce::Rectangle<float> (cell.getX() + 2.0f, cell.getBottom() - h,
                                                   juce::jmax (2.0f, (stepW - 4.0f) * (1.0f - amount * (1.0f - gate))), h);
        const auto lit = k == current && live;
        g.setColour ((lit ? red : juce::Colour (0xff8a8580)).withAlpha (lit ? 0.95f : 0.35f + 0.4f * amount));
        g.fillRect (block);
        if (onBeat)
        {
            g.setColour (juce::Colours::white.withAlpha (0.15f));
            g.drawVerticalLine ((int) cell.getX(), cell.getY() - 6.0f, cell.getBottom() + 4.0f);
        }
    }

    // The ranks: rows of figures, scattered when free, in straight lines when drilled.
    juce::Random scatter (11);
    const auto ranks = view.withTrimmedTop (lane.getBottom() - view.getY() + 24.0f).reduced (30.0f, 14.0f);
    constexpr int rows = 3, columns = 14;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < columns; ++c)
        {
            const auto jx = (scatter.nextFloat() - 0.5f) * 26.0f * (1.0f - amount);
            const auto jy = (scatter.nextFloat() - 0.5f) * 18.0f * (1.0f - amount);
            const auto x = ranks.getX() + ranks.getWidth() * (c + 0.5f) / columns + jx;
            const auto y = ranks.getY() + ranks.getHeight() * (r + 0.5f) / rows + jy;
            const auto size = juce::jmin (ranks.getWidth() / columns, ranks.getHeight() / rows) * 0.55f;
            const auto tone = juce::Colour (0xffb9b2a8).interpolatedWith (red, amount * 0.8f).withAlpha (0.5f + 0.4f * amount);
            g.setColour (stamp > 0.02f ? tone.brighter (stamp) : tone);
            g.fillEllipse (x - size * 0.22f, y - size * 0.5f, size * 0.44f, size * 0.44f);           // head
            g.fillRoundedRectangle (x - size * 0.28f, y - size * 0.05f, size * 0.56f, size * 0.6f, 2.0f);   // body
        }

    // The bar across the top.
    auto bar = bounds.reduced (12.0f, 6.0f).removeFromTop (20.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::text);
    g.drawText (live ? stageOf (amount) : "OFF", bar, juce::Justification::centredLeft, false);
    g.drawText ("STEP " + L::divisionNames[juce::jlimit (0, 3, settings.division)] + "  " + juce::String (current + 1) + " / "
                    + juce::String (stepsShown), bar, juce::Justification::centred, false);
    const auto makeUp = live ? fx.getMakeUpDb() : 0.0f;
    g.drawText ("MAKE-UP " + juce::String (makeUp >= 0.0f ? "+" : "") + juce::String (makeUp, 1) + " DB", bar,
                juce::Justification::centredRight, false);
}

//==============================================================================
LockstepPage::LockstepPage (ParagonX1AudioProcessor& p) : display (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::lockstepOn, "ON", "LOCKSTEP on or off: the sound forced into lockstep.");
    make (controls, ParamIDs::lockstepAmount, "AMOUNT", "From free (0: your sound untouched) to drilled: cut into steps in "
                                                         "time with your song, squeezed flat, the boots stamping. The mod "
                                                         "matrix can move it: FX Lockstep > Amount.");
    make (controls, ParamIDs::lockstepDivision, "STEP", "The steps: quarter notes, eighths, sixteenths or eighth triplets.");
    make (controls, ParamIDs::lockstepGate, "GATE", "How long each step stays open: a short stab to almost all of it.");
    make (controls, ParamIDs::lockstepAccent, "ACCENT", "How much lower the steps off the beat are.");
    make (controls, ParamIDs::lockstepSqueeze, "SQUEEZE", "How flat the sound is driven: every note the same.");
    make (controls, ParamIDs::lockstepStamp, "STAMP", "How loud the boots on each step (a crack and a thud). They only "
                                                       "sound while you play.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (display);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

std::vector<ParamControl*> LockstepPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void LockstepPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("LOCKSTEP", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the sound forced into lockstep", juce::Rectangle<int> (24, 38, 440, 16),
                juce::Justification::centredLeft, false);
}

void LockstepPage::resized()
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
