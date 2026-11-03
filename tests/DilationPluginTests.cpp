// DILATION in PR7-K (0.63.0): off and at SLOW 0 change nothing; on and slowed, a held note comes out lower;
// SLOW, TONE and MIX are mod-matrix destinations (FX Dilation), and MACRO 3 can bring it in; the SIGNATURE slot's
// ninth effect and its page.
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

TEST_CASE ("DILATION in the plugin: off and SLOW 0 change nothing; slowed, a held A4 comes out an octave down", "[dilation]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::dilationSlow, 0.5f);   // off: SLOW doesn't matter
    zero.set (ParamIDs::dilationOn, 1.0f);    // on at SLOW 0
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    Plugin p;
    p.set (ParamIDs::dilationOn, 1.0f);
    p.set (ParamIDs::dilationSlow, 0.5f);
    p.set (ParamIDs::dilationRamp, 0.0f);
    const auto out = p.play (1.5, 1.5);
    const auto heard = frequencyOf (out, (size_t) (0.6 * sampleRate), (size_t) (1.4 * sampleRate));
    INFO ("heard " << heard << " Hz");
    CHECK (std::abs (heard - 220.0) < 1.5);
}

TEST_CASE ("DILATION: SLOW, TONE and MIX are mod-matrix destinations (FX Dilation); MACRO 3 brings it in", "[dilation]")
{
    for (const auto* id : { ParamIDs::dilationSlow, ParamIDs::dilationTone, ParamIDs::dilationMix })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Dilation");
    }
    const auto withMacro = [] (float macro)
    {
        Plugin p;
        p.set (ParamIDs::dilationOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::dilationSlow, 0.5f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
        p.set (ParamIDs::macros[2], macro);
        return p.play (1.5, 1.5);
    };
    Plugin plain;
    const auto reference = plain.play (1.5, 1.5);
    CHECK (withMacro (0.0f) == reference);
    const auto slowed = withMacro (1.0f);
    CHECK (std::abs (frequencyOf (slowed, (size_t) (1.0 * sampleRate), (size_t) (1.4 * sampleRate)) - 440.0 * effects::Dilation::speedFor (0.5f)) < 3.0);
}

TEST_CASE ("DILATION: MACRO 3 up and back down: it catches up and leaves the sound untouched again", "[dilation]")
{
    // The matrix eases the macro's route back down and never lands exactly on 0: DILATION must still come back.
    for (const auto snap : { true, false })
    {
        Plugin plain, p;
        p.set (ParamIDs::dilationOn, 1.0f);
        p.set (ParamIDs::dilationReturn, snap ? 1.0f : 0.0f);
        p.set (ParamIDs::dilationRamp, 0.3f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::dilationSlow, 0.5f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
        p.set (ParamIDs::macros[2], 1.0f);
        auto slowed = p.play (1.28, 6.0);   // a held note, slowed for 240 blocks
        p.set (ParamIDs::macros[2], 0.0f);
        const auto after = p.play (4.48, 0.0);   // 840 blocks
        slowed.insert (slowed.end(), after.begin(), after.end());
        plain.processor.getKeyboardState().noteOn (1, 69, 0.9f);
        const auto reference = plain.play (5.76, 0.0);   // the same 1080 blocks
        auto differs = 0.0f;
        REQUIRE (slowed.size() == reference.size());
        for (auto i = (size_t) (4.0 * sampleRate); i < reference.size(); ++i)
            differs = std::max (differs, std::abs (slowed[i] - reference[i]));
        INFO ((snap ? "SNAP" : "GLIDE") << ": behind at the end " << p.processor.getDilation().getBehindSeconds() << " s, largest difference "
                                         << differs);
        CHECK (p.processor.getDilation().getBehindSeconds() == 0.0f);
        CHECK (differs == 0.0f);
    }
}

TEST_CASE ("DILATION: the SIGNATURE slot's ninth effect, and its page", "[dilation][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("DILATION"));
    auto& unit = fx.getModFxUnit (11);
    CHECK (unit.getSpec().name == "DILATION");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getDilationPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 7);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
