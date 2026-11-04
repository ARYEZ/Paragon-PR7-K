// LIGHTSPEED in PR7-K (0.73.0): off and at JUMP 0 change nothing; on and jumped, a held note gains its streaks (the
// note a fifth and an octave up at RISE 0.5); JUMP, RISE and WHOOSH are mod-matrix destinations (FX Lightspeed), and
// MACRO 3 takes it into the jump and back, untouched again once settled; the SIGNATURE slot's sixteenth effect and
// its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/FxPage.h"
#include "ui/MainView.h"
#include "ui/RackSpecs.h"

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
            set (ParamIDs::osc (0, "Wave"), (float) (int) synth::OscWave::sine);
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        std::vector<float> play (double seconds, double held, int note = 57)
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            if (held > 0.0)
                processor.getKeyboardState().noteOn (1, note, 0.9f);
            auto released = held <= 0.0;
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                if (! released && b * blockSize >= held * sampleRate)
                {
                    processor.getKeyboardState().noteOff (1, note, 0.0f);
                    released = true;
                }
                for (int c = 0; c < 2; ++c)
                    juce::FloatVectorOperations::clear (buffer.getWritePointer (c), blockSize);
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
    };
}

TEST_CASE ("LIGHTSPEED in the plugin: off and at JUMP 0 change nothing; jumped, a held note gains its streaks", "[lightspeed]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::jumpAmount, 1.0f);   // off: JUMP doesn't matter
    zero.set (ParamIDs::jumpOn, 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    // A sine A3 (220 Hz), RISE 0.5: the streaks at +4, +8 and +12 semitones; the octave (440 Hz) clearly there.
    Plugin p;
    p.set (ParamIDs::jumpOn, 1.0f);
    p.set (ParamIDs::jumpAmount, 1.0f);
    p.set (ParamIDs::jumpRise, 0.5f);
    p.set (ParamIDs::jumpWhoosh, 0.0f);
    const auto out = p.play (3.0, 3.0);
    Plugin fresh;   // (plain has already played a note: its phases would differ)
    const auto dry = fresh.play (3.0, 3.0);
    // What it added: the jumped note less the plain one ducked by a third (the whoosh is off), a fair share of the
    // note's own level; and it's the streaks, not the note: little of it at the note's own pitch.
    std::vector<float> added (out.size());
    for (size_t i = 0; i < out.size(); ++i)
        added[i] = out[i] - 0.67f * dry[i];
    const auto from = (size_t) (2.0 * sampleRate), to = (size_t) (3.0 * sampleRate);
    INFO ("added " << db (rms (added, from, to)) << " dB against the note's " << db (rms (dry, from, to)) << " dB");
    CHECK (db (rms (added, from, to) / rms (dry, from, to)) > -12.0);
    CHECK (db (levelAt (added, 220.0, from, to - from) / levelAt (dry, 220.0, from, to - from)) < -30.0);
}

TEST_CASE ("LIGHTSPEED: JUMP, RISE and WHOOSH are mod-matrix destinations (FX Lightspeed); MACRO 3 jumps and comes back", "[lightspeed]")
{
    for (const auto* id : { ParamIDs::jumpAmount, ParamIDs::jumpRise, ParamIDs::jumpWhoosh })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Lightspeed");
    }
    const auto routed = [] (Plugin& p)
    {
        p.set (ParamIDs::jumpOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::jumpAmount, 1.0f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
    };
    Plugin plain, at0, at1;
    routed (at0);
    routed (at1);
    at1.set (ParamIDs::macros[2], 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (at0.play (1.5, 1.0) == reference);
    CHECK (at1.play (1.5, 1.0) != reference);

    // Up and back down (the matrix eases it, never quite to 0): untouched again once the jump has settled.
    Plugin p, still;
    routed (p);
    p.set (ParamIDs::macros[2], 1.0f);
    auto out = p.play (1.28, 8.0);
    p.set (ParamIDs::macros[2], 0.0f);
    const auto after = p.play (6.4, 0.0);
    out.insert (out.end(), after.begin(), after.end());
    still.processor.getKeyboardState().noteOn (1, 57, 0.9f);
    const auto plainOut = still.play (7.68, 0.0);
    REQUIRE (out.size() == plainOut.size());
    auto differs = 0.0f;
    for (auto i = (size_t) (5.5 * sampleRate); i < out.size(); ++i)
        differs = std::max (differs, std::abs (out[i] - plainOut[i]));
    CHECK (differs == 0.0f);
}

TEST_CASE ("LIGHTSPEED: the SIGNATURE slot's sixteenth effect, and its page", "[lightspeed][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("LIGHTSPEED"));
    CHECK (signatureNames().indexOf ("LIGHTSPEED") == 15);
    auto& unit = fx.getModFxUnit (18);
    CHECK (unit.getSpec().name == "LIGHTSPEED");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getLightSpeedPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 7);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
