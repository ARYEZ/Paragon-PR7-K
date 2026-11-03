// SPECTRE in PR7-K (0.67.0): off and at MIX 0 change nothing; on, a held note gains its ghost and, after the note,
// the ghost lingers; MIX, LINGER and SHIFT are mod-matrix destinations (FX Spectre), and MACRO 3 brings it in and
// out; the SIGNATURE slot's thirteenth effect and its page.
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
            set (ParamIDs::ampRelease, 5.0f);
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        std::vector<float> play (double seconds, double held, int note = 69)
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

TEST_CASE ("SPECTRE in the plugin: off and at MIX 0 change nothing; on, a note gains its ghost, which lingers after", "[spectre]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::spectreMix, 1.0f);   // off: MIX doesn't matter
    zero.set (ParamIDs::spectreOn, 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    Plugin p;
    p.set (ParamIDs::osc (0, "Wave"), (float) (int) synth::OscWave::saw);
    plain.set (ParamIDs::osc (0, "Wave"), (float) (int) synth::OscWave::saw);
    p.set (ParamIDs::spectreOn, 1.0f);
    p.set (ParamIDs::spectreMix, 1.0f);
    p.set (ParamIDs::spectreTrail, 0.0f);
    p.set (ParamIDs::spectreLinger, 1.0f);   // 4 s to fade 60 dB
    const auto out = p.play (3.0, 1.0);
    const auto dry = plain.play (3.0, 1.0);
    const auto level = [] (const std::vector<float>& x, double from, double to) { return db (rms (x, (size_t) (from * sampleRate), (size_t) (to * sampleRate))); };
    INFO ("after the note (1.3-1.6 s): with SPECTRE " << level (out, 1.3, 1.6) << " dB, without " << level (dry, 1.3, 1.6) << " dB");
    CHECK (level (out, 1.3, 1.6) > level (dry, 1.3, 1.6) + 20.0);
}

TEST_CASE ("SPECTRE: MIX, LINGER and SHIFT are mod-matrix destinations (FX Spectre); MACRO 3 brings it in and out", "[spectre]")
{
    for (const auto* id : { ParamIDs::spectreMix, ParamIDs::spectreLinger, ParamIDs::spectreShift })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Spectre");
    }
    const auto routed = [] (Plugin& p)
    {
        p.set (ParamIDs::spectreOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::spectreMix, 0.7f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
    };
    Plugin plain, at0, at1;
    routed (at0);
    routed (at1);
    at1.set (ParamIDs::macros[2], 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (at0.play (1.5, 1.0) == reference);
    CHECK (at1.play (1.5, 1.0) != reference);

    // Up and back down (the matrix eases it, never quite to 0): untouched again.
    Plugin p, still;
    routed (p);
    p.set (ParamIDs::macros[2], 1.0f);
    auto out = p.play (1.28, 6.0);
    p.set (ParamIDs::macros[2], 0.0f);
    const auto after = p.play (4.48, 0.0);
    out.insert (out.end(), after.begin(), after.end());
    still.processor.getKeyboardState().noteOn (1, 69, 0.9f);
    const auto plainOut = still.play (5.76, 0.0);
    REQUIRE (out.size() == plainOut.size());
    auto differs = 0.0f;
    for (auto i = (size_t) (3.0 * sampleRate); i < out.size(); ++i)
        differs = std::max (differs, std::abs (out[i] - plainOut[i]));
    CHECK (differs == 0.0f);
}

TEST_CASE ("SPECTRE: the SIGNATURE slot's thirteenth effect, and its page", "[spectre][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("SPECTRE"));
    auto& unit = fx.getModFxUnit (15);
    CHECK (unit.getSpec().name == "SPECTRE");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getSpectrePage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 7);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
