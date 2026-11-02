// AFTERGLOW in PR7-K (0.60.0): off, or on at 0, every sample as before; on, a held sine is detuned by AGE and spread
// wide by ENSEMBLE; AGE, DRIFT, ENSEMBLE and TAPE are mod-matrix destinations (FX Afterglow), and MACRO 3 moves AGE;
// the SIGNATURE slot's sixth effect and its page.
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
        std::array<std::vector<float>, 2> play (double seconds, double held, int note = 69)
        {
            std::array<std::vector<float>, 2> out;
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
                for (int c = 0; c < 2; ++c)
                    out[(size_t) c].insert (out[(size_t) c].end(), buffer.getReadPointer (c), buffer.getReadPointer (c) + blockSize);
            }
            return out;
        }
    };

    double sideAgainstMid (const std::array<std::vector<float>, 2>& x, size_t from)
    {
        double mid = 0.0, side = 0.0;
        for (auto i = from; i < x[0].size(); ++i)
        {
            const auto m = 0.5 * (x[0][i] + x[1][i]), s = 0.5 * (x[0][i] - x[1][i]);
            mid += m * m;
            side += s * s;
        }
        return 10.0 * std::log10 (side / mid + 1.0e-30);
    }
}

TEST_CASE ("AFTERGLOW in the plugin: off, or on at 0, changes nothing; on, the note is its own and the sound wide", "[afterglow]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::afterglowAge, 1.0f);   // off: the amounts don't matter
    off.set (ParamIDs::afterglowEnsemble, 1.0f);
    off.set (ParamIDs::afterglowTape, 1.0f);
    zero.set (ParamIDs::afterglowOn, 1.0f);   // on at 0
    const auto reference = plain.play (1.0, 0.5);
    CHECK (off.play (1.0, 0.5) == reference);
    CHECK (zero.play (1.0, 0.5) == reference);

    Plugin p;
    p.set (ParamIDs::afterglowOn, 1.0f);
    p.set (ParamIDs::afterglowAge, 1.0f);
    p.set (ParamIDs::afterglowEnsemble, 1.0f);
    const auto out = p.play (2.0, 2.0);
    INFO ("side against mid with ENSEMBLE: " << sideAgainstMid (out, (size_t) (0.5 * sampleRate)) << " dB; without: "
                                           << sideAgainstMid (reference, (size_t) (0.1 * sampleRate)) << " dB");
    CHECK (sideAgainstMid (out, (size_t) (0.5 * sampleRate)) > -15.0);
    CHECK (sideAgainstMid (reference, (size_t) (0.1 * sampleRate)) < -100.0);

    // Let go: silent.
    p.processor.getKeyboardState().noteOff (1, 69, 0.0f);
    p.play (1.0, 0.0);
    const auto tail = p.play (0.5, 0.0);
    CHECK (*std::max_element (tail[0].begin(), tail[0].end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) == 0.0f);
}

TEST_CASE ("AFTERGLOW: AGE, DRIFT, ENSEMBLE and TAPE are mod-matrix destinations (FX Afterglow); MACRO 3 moves AGE", "[afterglow]")
{
    for (const auto* id : { ParamIDs::afterglowAge, ParamIDs::afterglowDrift, ParamIDs::afterglowEnsemble, ParamIDs::afterglowTape })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Afterglow");
    }
    // AFTERGLOW on at AGE 0 with MACRO 3 routed to AGE: at the macro's left, the sound as without; turned up, the notes apart.
    const auto withMacro = [] (float macro)
    {
        Plugin p;
        p.set (ParamIDs::afterglowOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::afterglowAge, 1.0f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
        p.set (ParamIDs::macros[2], macro);
        return p.play (0.6, 0.5);
    };
    Plugin plain;
    const auto reference = plain.play (0.6, 0.5);
    CHECK (withMacro (0.0f)[0] == reference[0]);
    CHECK (withMacro (1.0f)[0] != reference[0]);
}

TEST_CASE ("AFTERGLOW: the SIGNATURE slot's sixth effect, and its page", "[afterglow][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("AFTERGLOW"));
    auto& unit = fx.getModFxUnit (8);
    CHECK (unit.getSpec().name == "AFTERGLOW");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getAfterglowPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 5);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
