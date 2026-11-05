// OVERCHARGE in PR7-K (0.77.0): off and at CHARGE 0 change nothing; on, a held note gains crystal tones above it; CHARGE,
// GLOW and CHAOS are mod-matrix destinations (FX Overcharge), and MACRO 3 charges it and lets it go, untouched again
// once settled; the SIGNATURE slot's nineteenth effect and its page.
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

}

TEST_CASE ("OVERCHARGE in the plugin: off and at CHARGE 0 change nothing; on, a held note glows above itself", "[overcharge]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::overchargeCharge, 1.0f);   // off: CHARGE doesn't matter
    zero.set (ParamIDs::overchargeOn, 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    Plugin p;
    p.set (ParamIDs::overchargeOn, 1.0f);
    p.set (ParamIDs::overchargeCharge, 0.45f);
    p.set (ParamIDs::overchargeGlow, 1.0f);
    const auto out = p.play (1.5, 1.0);
    const auto from = (size_t) (0.4 * sampleRate), length = (size_t) (0.5 * sampleRate);
    // The crystals ring an octave and two octaves up, on the saw's 2nd and 4th harmonics, which they lift. (The octave
    // and a fifth, 659.26 Hz, sits 0.7 Hz off the saw's 3rd harmonic: the two beat, so it isn't measured here.)
    for (const auto hz : { 440.0, 880.0 })
    {
        const auto before = levelAt (reference[0], hz, from, length), after = levelAt (out[0], hz, from, length);
        INFO (hz << " Hz: " << db (before) << " dB alone, " << db (after) << " dB charged");
        CHECK (db (after) > db (before) + 3.0);
    }
    const auto level = db (rms (out[0], from, from + length) / rms (reference[0], from, from + length));
    INFO ("level change " << level << " dB");
    CHECK (std::abs (level) < 2.5);
}

TEST_CASE ("OVERCHARGE: CHARGE, GLOW and CHAOS are mod-matrix destinations (FX Overcharge); MACRO 3 charges it and lets it go", "[overcharge]")
{
    for (const auto* id : { ParamIDs::overchargeCharge, ParamIDs::overchargeGlow, ParamIDs::overchargeChaos })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Overcharge");
    }
    const auto routed = [] (Plugin& p)
    {
        p.set (ParamIDs::overchargeOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::overchargeCharge, 1.0f, false, 0.0f, 0.0f });
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
    for (auto i = (size_t) (4.0 * sampleRate); i < out[0].size(); ++i)
        differs = std::max (differs, std::abs (out[0][i] - plainOut[0][i]));
    CHECK (differs == 0.0f);
}

TEST_CASE ("OVERCHARGE: the SIGNATURE slot's nineteenth effect, and its page", "[overcharge][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("OVERCHARGE"));
    CHECK (signatureNames().indexOf ("OVERCHARGE") == 18);
    auto& unit = fx.getModFxUnit (21);
    CHECK (unit.getSpec().name == "OVERCHARGE");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getOverchargePage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 6);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
