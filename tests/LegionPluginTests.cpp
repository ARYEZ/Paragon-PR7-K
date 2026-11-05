// LEGION in PR7-K (0.76.0): off and at SIZE 0 change nothing; on, a held note gains players (wider); SIZE, SPREAD and
// DETUNE are mod-matrix destinations (FX Legion), and MACRO 3 brings the section in and back, untouched again once
// settled; the SIGNATURE slot's eighteenth effect and its page.
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
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        std::array<std::vector<float>, 2> play (double seconds, double held, int note = 57)
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
                buffer.clear();
                processor.processBlock (buffer, midi);
                for (int c = 0; c < 2; ++c)
                    out[(size_t) c].insert (out[(size_t) c].end(), buffer.getReadPointer (c), buffer.getReadPointer (c) + blockSize);
            }
            return out;
        }
    };

    double sideShare (const std::array<std::vector<float>, 2>& x, size_t from, size_t to)
    {
        double mid = 0.0, side = 0.0;
        for (auto i = from; i < to; ++i)
        {
            mid += std::pow (0.5 * (x[0][i] + x[1][i]), 2.0);
            side += std::pow (0.5 * (x[0][i] - x[1][i]), 2.0);
        }
        return side / std::max (1.0e-12, mid);
    }
}

TEST_CASE ("LEGION in the plugin: off and at SIZE 0 change nothing; on, a centred note spreads into a section", "[legion]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::legionSize, 1.0f);   // off: SIZE doesn't matter
    zero.set (ParamIDs::legionOn, 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    Plugin p;
    p.set (ParamIDs::legionOn, 1.0f);
    p.set (ParamIDs::legionSize, 1.0f);
    const auto out = p.play (1.5, 1.0);
    const auto from = (size_t) (0.3 * sampleRate), to = (size_t) (0.9 * sampleRate);
    INFO ("side / mid: alone " << sideShare (reference, from, to) << ", with LEGION " << sideShare (out, from, to));
    CHECK (sideShare (reference, from, to) < 1.0e-6);   // the default sound is centred
    CHECK (sideShare (out, from, to) > 0.05);
    // Bigger, not louder.
    const auto level = db (rms (out[0], from, to) / rms (reference[0], from, to));
    INFO ("level change " << level << " dB");
    CHECK (std::abs (level) < 2.0);
}

TEST_CASE ("LEGION: SIZE, SPREAD and DETUNE are mod-matrix destinations (FX Legion); MACRO 3 brings the section and takes it back", "[legion]")
{
    for (const auto* id : { ParamIDs::legionSize, ParamIDs::legionSpread, ParamIDs::legionDetune })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Legion");
    }
    const auto routed = [] (Plugin& p)
    {
        p.set (ParamIDs::legionOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::legionSize, 1.0f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
    };
    Plugin plain, at0, at1;
    routed (at0);
    routed (at1);
    at1.set (ParamIDs::macros[2], 1.0f);
    const auto reference = plain.play (1.0, 0.8);
    CHECK (at0.play (1.0, 0.8) == reference);
    CHECK (at1.play (1.0, 0.8) != reference);

    // Up and back down (the matrix eases it, never quite to 0): untouched again once it has settled.
    Plugin p, still;
    routed (p);
    p.set (ParamIDs::macros[2], 1.0f);
    auto out = p.play (1.28, 8.0);
    p.set (ParamIDs::macros[2], 0.0f);
    const auto after = p.play (3.84, 0.0);
    for (int c = 0; c < 2; ++c)
        out[(size_t) c].insert (out[(size_t) c].end(), after[(size_t) c].begin(), after[(size_t) c].end());
    still.processor.getKeyboardState().noteOn (1, 57, 0.9f);
    const auto plainOut = still.play (5.12, 0.0);
    REQUIRE (out[0].size() == plainOut[0].size());
    auto differs = 0.0f;
    for (auto i = (size_t) (3.5 * sampleRate); i < out[0].size(); ++i)
        differs = std::max (differs, std::abs (out[0][i] - plainOut[0][i]));
    CHECK (differs == 0.0f);
}

TEST_CASE ("LEGION: the SIGNATURE slot's eighteenth effect, and its page", "[legion][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("LEGION"));
    CHECK (signatureNames().indexOf ("LEGION") == 17);
    auto& unit = fx.getModFxUnit (20);
    CHECK (unit.getSpec().name == "LEGION");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getLegionPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 6);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
