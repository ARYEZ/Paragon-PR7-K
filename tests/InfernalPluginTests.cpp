// INFERNAL in PR7-K (0.59.0): off and MIX 0 change nothing; on, the synth comes out destroyed (harmonics a clean sine
// never had) with its sub clean, and goes silent after; the REVERB hears the destroyed sound; DRIVE, MIX, NOISE and
// FEEDBACK are matrix destinations; the SIGNATURE slot's fifth effect and its page.
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
        std::vector<float> play (double seconds, double held, int note = 45)
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

TEST_CASE ("INFERNAL in the plugin: off and MIX 0 change nothing; on, a clean sine comes out destroyed, its sub clean", "[infernal]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::infernalMix, 1.0f);    // off: MIX doesn't matter
    zero.set (ParamIDs::infernalOn, 1.0f);    // on at MIX 0
    zero.set (ParamIDs::infernalMix, 0.0f);
    const auto reference = plain.play (1.0, 0.5);
    CHECK (off.play (1.0, 0.5) == reference);
    CHECK (zero.play (1.0, 0.5) == reference);

    Plugin p;
    p.set (ParamIDs::infernalOn, 1.0f);
    p.set (ParamIDs::infernalMix, 1.0f);
    p.set (ParamIDs::infernalTight, 0.0f);   // 30 Hz: everything destroyed
    const auto out = p.play (1.6, 2.0, 57);   // A3, 220 Hz
    const Spectrum spectrum (out, (size_t) (0.1 * sampleRate));
    const auto first = spectrum.at (220.0), third = spectrum.at (660.0);
    INFO ("220 Hz " << first << " dB, 660 Hz " << third << " dB");
    CHECK (third > first - 25.0);   // a clean sine has no 3rd harmonic

    // Let go: silent.
    p.processor.getKeyboardState().noteOff (1, 57, 0.0f);
    p.play (2.5, 0.0);
    const auto tail = p.play (1.0, 0.0);
    CHECK (*std::max_element (tail.begin(), tail.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) == 0.0f);
}

TEST_CASE ("INFERNAL: the REVERB hears the destroyed sound", "[infernal]")
{
    // A sine sent to the REVERB at full; its return's harmonics show what it heard.
    const auto reverbOf = [] (bool destroyed)
    {
        Plugin p;
        p.set (ParamIDs::reverbOn, 1.0f);
        p.set (ParamIDs::reverbMix, 1.0f);
        p.set (ParamIDs::infernalOn, destroyed ? 1.0f : 0.0f);
        p.set (ParamIDs::infernalMix, 1.0f);
        p.set (ParamIDs::infernalTight, 0.0f);
        const auto all = p.play (2.0, 1.0, 57);
        // After the note: only the reverb's tail is left.
        std::vector<float> tail (all.begin() + (long) (1.2 * sampleRate), all.end());
        tail.resize ((size_t) Spectrum::size, 0.0f);
        const Spectrum spectrum (tail, 0);
        return spectrum.at (660.0) - spectrum.at (220.0);
    };
    const auto clean = reverbOf (false), hot = reverbOf (true);
    INFO ("the reverb tail's 3rd harmonic against its fundamental: clean " << clean << " dB, destroyed " << hot << " dB");
    CHECK (hot > clean + 20.0);
}

TEST_CASE ("INFERNAL: DRIVE, MIX, NOISE and FEEDBACK are mod-matrix destinations (FX Infernal); the SIGNATURE slot's fifth", "[infernal][ui]")
{
    for (const auto* id : { ParamIDs::infernalDrive, ParamIDs::infernalMix, ParamIDs::infernalNoise, ParamIDs::infernalFeedback })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Infernal");
    }
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("INFERNAL"));
    auto& unit = fx.getModFxUnit (7);
    CHECK (unit.getSpec().name == "INFERNAL");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getInfernalPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 11);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
