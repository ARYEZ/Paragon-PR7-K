#include "TranceGatePage.h"

#include "dsp/effects/TranceGate.h"
#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    using TG = effects::TranceGate;
    const juce::Colour pink (0xffff3d6e), orange (0xffff8a2a), deep (0xff120a10);

    float valueOf (ParagonX1AudioProcessor& p, const juce::String& id)
    {
        auto* parameter = p.getParameters().getParameter (id);
        return parameter != nullptr ? parameter->convertFrom0to1 (parameter->getValue()) : 0.0f;
    }
}

//==============================================================================
TranceGateEditor::TranceGateEditor (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The pattern: a column a step, as tall as its level. Click a step to open or close it; drag across steps to "
                "paint them the same. Hold SHIFT (or drag with the right button) to set a step's level by height: a soft "
                "step lets some sound through. Steps past LENGTH are dimmed. With STEREO the lower row is the right side.");
    startTimerHz (30);
}

int TranceGateEditor::rows() const
{
    return juce::roundToInt (valueOf (processor, ParamIDs::tgStereo)) == 1 ? 2 : 1;
}

juce::Rectangle<float> TranceGateEditor::cellBounds (int row, int step) const
{
    auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    area.removeFromLeft (22.0f);   // the row names
    const auto n = rows();
    const auto rowHeight = (area.getHeight() - (n - 1) * 10.0f) / (float) n;
    const auto top = area.getY() + row * (rowHeight + 10.0f);
    const auto width = area.getWidth() / (float) TG::maxSteps;
    return { area.getX() + width * step, top, width, rowHeight };
}

std::pair<int, int> TranceGateEditor::cellAt (juce::Point<float> point) const
{
    for (int row = 0; row < rows(); ++row)
        for (int step = 0; step < TG::maxSteps; ++step)
            if (cellBounds (row, step).contains (point))
                return { row, step };
    return { -1, -1 };
}

float TranceGateEditor::stepLevel (int row, int step) const
{
    return valueOf (processor, ParamIDs::tgStep (row, step));
}

void TranceGateEditor::setStep (int row, int step, float level)
{
    if (auto* parameter = processor.getParameters().getParameter (ParamIDs::tgStep (row, step)))
    {
        if (std::find (gesturing.begin(), gesturing.end(), parameter) == gesturing.end())
        {
            parameter->beginChangeGesture();
            gesturing.push_back (parameter);
        }
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (juce::jlimit (0.0f, 1.0f, level)));
    }
}

void TranceGateEditor::applyAt (juce::Point<float> point, bool height)
{
    const auto [row, step] = cellAt (point);
    if (row < 0)
        return;
    if (height)
    {
        const auto cell = cellBounds (row, step);
        setStep (row, step, juce::jlimit (0.0f, 1.0f, (cell.getBottom() - point.y) / cell.getHeight()));
    }
    else
        setStep (row, step, paintValue);
    repaint();
}

void TranceGateEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto [row, step] = cellAt (e.position);
    if (row < 0)
        return;
    byHeight = e.mods.isShiftDown() || e.mods.isRightButtonDown();
    paintValue = stepLevel (row, step) > 0.0f ? 0.0f : 1.0f;   // a click toggles; a drag paints the same
    applyAt (e.position, byHeight);
}

void TranceGateEditor::mouseDrag (const juce::MouseEvent& e)
{
    applyAt (e.position, byHeight);
}

void TranceGateEditor::mouseUp (const juce::MouseEvent&)
{
    for (auto* parameter : gesturing)
        parameter->endChangeGesture();
    gesturing.clear();
}

void TranceGateEditor::timerCallback()
{
    // Redrawn while shown: the step playing, and steps changed from elsewhere (automation, a preset, undo).
    shownStep = processor.getTranceGate().getStep();
    if (isShowing())
        repaint();
}

void TranceGateEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (deep);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto length = juce::jlimit (1, TG::maxSteps, juce::roundToInt (valueOf (processor, ParamIDs::tgLength)));
    const auto playing = processor.getTranceGate().getStep();
    const auto n = rows();
    for (int row = 0; row < n; ++row)
    {
        const auto first = cellBounds (row, 0);
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (n == 1 ? "L+R" : (row == 0 ? "L" : "R"),
                    juce::Rectangle<float> (first.getX() - 24.0f, first.getY(), 22.0f, first.getHeight()), juce::Justification::centred, false);
        for (int step = 0; step < TG::maxSteps; ++step)
        {
            const auto cell = cellBounds (row, step);
            const auto inside = step < length;
            const auto beat = step % 4 == 0;
            // The groove's grid: each beat's first step a little lighter.
            g.setColour (juce::Colours::white.withAlpha (inside ? (beat ? 0.07f : 0.035f) : 0.015f));
            g.fillRect (cell.reduced (1.5f, 0.0f));
            const auto level = stepLevel (row, step);
            if (level > 0.0f)
            {
                auto bar = cell.reduced (2.5f, 2.0f);
                bar = bar.withTop (bar.getBottom() - bar.getHeight() * level);
                g.setGradientFill (juce::ColourGradient (orange.withAlpha (inside ? 1.0f : 0.25f), bar.getX(), bar.getY(),
                                                         pink.withAlpha (inside ? 0.85f : 0.2f), bar.getX(), cell.getBottom(), false));
                g.fillRect (bar);
            }
            if (step == playing && inside)
            {
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawRect (cell.reduced (1.0f, 0.0f), 2.0f);
            }
        }
        // LENGTH's end.
        const auto edge = cellBounds (row, length - 1).getRight();
        g.setColour (pink.withAlpha (0.7f));
        g.drawLine (edge, first.getY(), edge, first.getBottom(), 1.5f);
    }
}

//==============================================================================
TranceGatePage::TranceGatePage (ParagonX1AudioProcessor& p) : processor (p), editor (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::tgOn, "ON", "TRANCE GATE on or off: the whole sound, reverb and echoes included, chopped in time "
                                     "with the song by the pattern below.");
    make (controls, ParamIDs::tgMix, "MIX", "How deep it cuts: at 100% the closed steps are silent, lower and they only dip. "
                                            "The mod matrix can move it: FX Trance Gate > Mix.");
    make (controls, ParamIDs::tgDivision, "STEP", "Each step's length, in time with the song.");
    make (controls, ParamIDs::tgLength, "LENGTH", "How many steps before the pattern comes round (1 to 32).");
    make (controls, ParamIDs::tgGate, "GATE", "How much of each step is open: short for staccato chops, 100% to let open steps "
                                              "run into each other. The mod matrix can move it: FX Trance Gate > Gate.");
    make (controls, ParamIDs::tgAttack, "ATTACK", "How fast each step opens: a click-free chop at the left, a soft swell to the right.");
    make (controls, ParamIDs::tgRelease, "RELEASE", "How fast each step closes: a hard cut at the left, a pumping fade to the right.");
    make (controls, ParamIDs::tgSwing, "SWING", "Every second step starts later, up to half a step: a shuffled gate.");
    make (controls, ParamIDs::tgStereo, "STEREO", "LINKED: one pattern on both sides. STEREO: the lower row is the right "
                                                  "side's own pattern, so the gate can bounce between the speakers.");
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            addAndMakeVisible (*c);
    addAndMakeVisible (editor);
    startFrom.setTooltip ("Start from one of the gate's patterns (it replaces both rows; PING-PONG also sets STEREO).");
    startFrom.onClick = [this]
    {
        juce::PopupMenu menu;
        const auto& list = TG::patterns();
        for (int k = 0; k < (int) list.size(); ++k)
            menu.addItem (k + 1, list[(size_t) k].name);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (startFrom),
                            [safe = juce::Component::SafePointer<TranceGatePage> (this)] (int chosen)
                            {
                                if (safe != nullptr && chosen > 0)
                                    safe->loadPattern (chosen - 1);
                            });
    };
    addAndMakeVisible (startFrom);
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (back);
}

void TranceGatePage::loadPattern (int index)
{
    const auto& list = TG::patterns();
    const auto& pattern = list[(size_t) juce::jlimit (0, (int) list.size() - 1, index)];
    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.getParameters().getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    };
    for (int step = 0; step < TG::maxSteps; ++step)
    {
        set (ParamIDs::tgStep (0, step), pattern.left[(size_t) (step % 16)]);
        set (ParamIDs::tgStep (1, step), pattern.right[(size_t) (step % 16)]);
    }
    if (pattern.stereo)
        set (ParamIDs::tgStereo, 1.0f);
    editor.repaint();
}

std::vector<ParamControl*> TranceGatePage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &controls })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void TranceGatePage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 160.0f, 24.0f)));
    g.drawText ("TRANCE GATE", juce::Rectangle<int> (24, 14, 220, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Signature: the whole sound chopped in time with the song, the reverb and echoes too",
                juce::Rectangle<int> (24, 38, 480, 16), juce::Justification::centredLeft, false);
}

void TranceGatePage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (500);
    back.setBounds (bar.removeFromRight (150).withSizeKeepingCentre (150, 30));
    bar.removeFromRight (12);
    startFrom.setBounds (bar.removeFromRight (120).withSizeKeepingCentre (120, 30));
    bar.removeFromRight (20);
    top[0]->setBounds (bar.removeFromLeft (70));
    area.removeFromTop (8);
    auto controlRow = area.removeFromBottom (ParamControl::knobHeight);
    area.removeFromBottom (10);
    editor.setBounds (area);
    std::vector<juce::Component*> row;
    for (auto& c : controls)
        row.push_back (c.get());
    SynthPanel::layoutRow (controlRow, row, 96);
}
