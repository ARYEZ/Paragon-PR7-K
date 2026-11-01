// THE WAH in PR7-K (0.51.0): on an insert bus, before the DRIVE; heel dark, toe bright on a played note; the EXPR wheel
// (controller 11) rocking it through the mod matrix, and then not swelling the volume; off and unrouted changing
// nothing; the INSERTS view's five units with the WAH on top, and its page showing each mode's controls.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/effects/Wah.h"
#include "ui/FxPage.h"
#include "ui/InsertPages.h"
#include "ui/MainView.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    struct Plugin
    {
        ParagonX1AudioProcessor processor;
        Plugin()
        {
            processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
            processor.prepareToPlay (sampleRate, blockSize);
            processor.seedRandomForTest (1);
            set (ParamIDs::ampSustain, 1.0f);
            set (ParamIDs::osc (0, "Insert"), 1.0f);   // oscillator 1 (a saw) through bus A
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        void route (const char* source, const juce::String& destination, float amount)
        {
            auto m = processor.getModMatrix();
            m.routes.push_back ({ true, source, destination, amount, false, 0.0f, 0.0f });
            processor.setModMatrix (m);
        }
        // `seconds` of a held A2, the left side; `midi` goes with the first block.
        std::vector<float> play (double seconds, juce::MidiBuffer midi = {})
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
                midi.clear();
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
        std::vector<float> note (double seconds)
        {
            processor.getKeyboardState().noteOn (1, 45, 0.8f);
            return play (seconds);
        }
    };

    // Where a sound's energy sits: the level-weighted average of harmonics 1-30 of 110 Hz (in harmonics).
    double brightness (const std::vector<float>& x)
    {
        const auto from = x.size() / 2, length = x.size() - from;
        double sum = 0.0, weighted = 0.0;
        for (int h = 1; h <= 30; ++h)
        {
            const auto level = levelAt (x, 110.0 * h, from, length);
            sum += level;
            weighted += level * h;
        }
        return weighted / juce::jmax (1.0e-12, sum);
    }

    juce::MidiBuffer expression (int value)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 11, value), 0);
        return midi;
    }
}

TEST_CASE ("WAH in the plugin: off changes nothing; on, the heel is dark and the toe bright", "[wah]")
{
    Plugin plain;
    plain.set (ParamIDs::osc (0, "Insert"), 0.0f);
    const auto straight = plain.note (0.5);
    Plugin routed;
    const auto throughBus = routed.note (0.5);
    CHECK (straight == throughBus);   // a bus with nothing on it changes nothing (as before 0.51.0)

    auto with = [] (float position)
    {
        Plugin p;
        p.set (ParamIDs::ins (0, "WahOn"), 1.0f);
        p.set (ParamIDs::ins (0, "WahPosition"), position);
        return brightness (p.note (0.5));
    };
    const auto heel = with (0.0f), toe = with (1.0f), dry = brightness (straight);
    INFO ("energy centre (harmonic): dry " << dry << ", heel " << heel << ", toe " << toe);
    CHECK (heel < dry);
    CHECK (toe > heel * 2.0);
}

TEST_CASE ("WAH: the EXPR wheel rocks it through the matrix, and then leaves the volume alone", "[wah]")
{
    Plugin p;
    p.set (ParamIDs::ins (0, "WahOn"), 1.0f);
    p.set (ParamIDs::ins (0, "WahPosition"), 0.0f);
    p.route ("expression", "ins1WahPosition", 1.0f);
    CHECK (ModDestinations::find ("ins1WahPosition")->group == "FX Insert A");

    p.processor.getKeyboardState().noteOn (1, 45, 0.8f);
    // Before the wheel or a pedal has moved, EXPRESSION reads full, as the EXPR wheel is drawn: the toe.
    p.play (0.3);
    CHECK (p.processor.getInsertWahSettings (0).position > 0.98f);
    p.play (0.3, expression (0));
    CHECK (p.processor.getInsertWahSettings (0).position < 0.02f);
    const auto heel = brightness (p.play (0.5));
    p.play (0.3, expression (127));
    CHECK (p.processor.getInsertWahSettings (0).position > 0.98f);
    const auto toe = brightness (p.play (0.5));
    p.play (0.3, expression (64));
    CHECK (std::abs (p.processor.getInsertWahSettings (0).position - 0.5f) < 0.03f);
    INFO ("heel " << heel << ", toe " << toe);
    CHECK (toe > heel * 2.0);

    // Routed, EXPRESSION isn't also the volume swell (0.49.0): at 0 the synth still plays at full.
    const auto atFull = rms (p.play (0.3), 0, (size_t) (0.3 * sampleRate));
    p.play (0.2, expression (0));
    const auto atZero = rms (p.play (0.3), 0, (size_t) (0.3 * sampleRate));
    INFO ("pedal at mid " << db (atFull) << " dB, at the heel " << db (atZero) << " dB");
    CHECK (atZero > 0.01);   // unrouted, the wheel at 0 would be silence

    // Other buses' pedals stay where their knobs are.
    CHECK (juce::approximatelyEqual (p.processor.getInsertWahSettings (1).position, 0.5f));
}

TEST_CASE ("WAH: AUTO on a bus opens with each note", "[wah]")
{
    Plugin p;
    p.set (ParamIDs::ins (0, "WahOn"), 1.0f);
    p.set (ParamIDs::ins (0, "WahMode"), (float) (int) effects::Wah::Mode::autoWah);
    p.set (ParamIDs::ins (0, "WahPosition"), 0.0f);
    p.set (ParamIDs::ampRelease, 30.0f);
    p.play (0.2);
    CHECK (p.processor.getInsertWahSweep (0) < 0.05f);
    p.note (0.3);
    CHECK (p.processor.getInsertWahSweep (0) > 0.6f);
    p.processor.getKeyboardState().noteOff (1, 45, 0.0f);
    p.play (1.5);
    CHECK (p.processor.getInsertWahSweep (0) < 0.1f);
}

TEST_CASE ("WAH: the INSERTS view has it first of five, and its page shows each mode's controls", "[wah][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::inserts);
    auto& wahUnit = fx.getInsertUnit (0, 4);
    CHECK (wahUnit.getSpec().name == "WAH");
    CHECK (wahUnit.isVisible());
    for (int i = 0; i < 4; ++i)
        CHECK (wahUnit.getBottom() <= fx.getInsertUnit (0, i).getY());   // on top of DRIVE, GLITCH, MOD, LO-FI
    CHECK (fx.getInsertUnit (0, 3).getBottom() <= fx.getHeight());       // all five fit

    fx.show (FxPage::View::insertWah);
    auto& page = fx.getInsertEffectPage (0, 2);
    REQUIRE (page.isVisible());
    const auto shown = [&page]
    {
        juce::StringArray labels;
        for (auto* c : page.getShownControls())
            labels.add (c->getParameterId().fromFirstOccurrenceOf ("ins1", false, false));
        return labels;
    };
    auto set = [&processor] (const char* id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (ParamIDs::ins (0, id));
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    page.resized();
    CHECK (shown().contains ("WahPosition"));
    CHECK (! shown().contains ("WahSensitivity"));
    CHECK (! shown().contains ("WahRate"));
    set ("WahMode", (float) (int) effects::Wah::Mode::autoWah);
    page.resized();
    CHECK (shown().contains ("WahSensitivity"));
    CHECK (shown().contains ("WahDepth"));
    set ("WahMode", (float) (int) effects::Wah::Mode::lfo);
    page.resized();
    CHECK (shown().contains ("WahRate"));
    CHECK (shown().contains ("WahSync"));
    CHECK (! shown().contains ("WahSensitivity"));
    set ("WahSync", 3.0f);
    page.resized();
    CHECK (! shown().contains ("WahRate"));
}
