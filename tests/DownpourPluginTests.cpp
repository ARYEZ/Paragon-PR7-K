// DOWNPOUR in PR7-K (0.57.0): off changes nothing; on, the drops land on the notes played (keys, a chord), the rain
// stops when the sound does, SPACE feeds the REVERB; RAIN, DROPS, DISTANCE and LEVEL as matrix destinations (MACRO 3
// here, as NEON RAIN's STORM does); the MOD / LO-FI view's sixth unit and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/FxPage.h"
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
            set (ParamIDs::ampRelease, 5.0f);
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        // `seconds`, `notes` held for the first `held` seconds. Left side.
        std::vector<float> play (double seconds, double held, std::initializer_list<int> notes = { 60 })
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            if (held > 0.0)
                for (const auto n : notes)
                    processor.getKeyboardState().noteOn (1, n, 0.8f);
            auto released = held <= 0.0;
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                if (! released && b * blockSize >= held * sampleRate)
                {
                    for (const auto n : notes)
                        processor.getKeyboardState().noteOff (1, n, 0.0f);
                    released = true;
                }
                for (int c = 0; c < 2; ++c)
                    juce::FloatVectorOperations::clear (buffer.getWritePointer (c), blockSize);
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
        std::vector<float> dropNotes()
        {
            std::vector<float> out;
            for (const auto& d : processor.getDownpour().getRecentDrops())
                if (d.sample >= 0)
                    out.push_back (d.note);
            return out;
        }
    };
}

TEST_CASE ("DOWNPOUR in the plugin: off changes nothing; on, drops on the chord, silence after", "[downpour]")
{
    Plugin plain, off;
    off.set (ParamIDs::downpourRain, 1.0f);   // off: its settings don't matter
    CHECK (plain.play (2.0, 0.5) == off.play (2.0, 0.5));

    Plugin p;
    p.set (ParamIDs::downpourOn, 1.0f);
    p.set (ParamIDs::downpourRain, 1.0f);
    p.set (ParamIDs::downpourDrops, 1.0f);
    p.set (ParamIDs::downpourOctave, 1.0f);   // +2 octaves
    p.play (3.0, 3.5, { 57, 60, 64 });         // A minor, held through
    const auto notes = p.dropNotes();
    INFO ("drops seen: " << notes.size() << " of " << p.processor.getDownpour().getTotalDrops());
    REQUIRE (notes.size() >= 20);
    for (const auto n : notes)
    {
        const auto pc = ((int) std::lround (n)) % 12;
        INFO ("a drop on " << n);
        CHECK ((pc == 9 || pc == 0 || pc == 4));   // A, C or E
        CHECK (n >= 57.0f + 24.0f);
    }
    for (const auto n : { 57, 60, 64 })
        p.processor.getKeyboardState().noteOff (1, n, 0.0f);
    p.play (3.0, 0.0);
    const auto tail = p.play (2.0, 0.0);
    CHECK (*std::max_element (tail.begin(), tail.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) == 0.0f);
}

TEST_CASE ("DOWNPOUR: SPACE sends the rain into the REVERB", "[downpour]")
{
    // The REVERB on but nothing of the synth sent to it: whatever it gives back is the rain's.
    const auto reverbOfRain = [] (float space)
    {
        Plugin p;
        p.set (ParamIDs::reverbOn, 1.0f);
        p.set (ParamIDs::reverbMix, 1.0f);
        p.set (ParamIDs::osc (0, "SendReverb"), 0.0f);
        p.set (ParamIDs::downpourOn, 1.0f);
        p.set (ParamIDs::downpourRain, 1.0f);
        p.set (ParamIDs::downpourSpace, space);
        p.set (ParamIDs::downpourLevel, -6.0f);
        p.play (2.0, 2.5);
        return p.processor.getReturnPeakDb (1);
    };
    const auto dry = reverbOfRain (0.0f), wet = reverbOfRain (1.0f);
    INFO ("the REVERB's return: SPACE 0 " << dry << " dB, SPACE 1 " << wet << " dB");
    CHECK (dry < -90.0f);
    CHECK (wet > -50.0f);
}

TEST_CASE ("DOWNPOUR: RAIN, DROPS, DISTANCE and LEVEL are mod-matrix destinations (FX Downpour)", "[downpour]")
{
    for (const auto* id : { ParamIDs::downpourRain, ParamIDs::downpourDrops, ParamIDs::downpourDistance, ParamIDs::downpourLevel })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Downpour");
    }
    Plugin p;
    p.set (ParamIDs::downpourOn, 1.0f);
    p.set (ParamIDs::downpourRain, 0.1f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::downpourRain, 0.8f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
    p.set (ParamIDs::macros[2], 1.0f);
    p.play (0.3, 0.2);
    CHECK (p.processor.getDownpourSettings().rain > 0.85f);
    p.set (ParamIDs::macros[2], 0.0f);
    p.play (0.3, 0.0);
    CHECK (p.processor.getDownpourSettings().rain < 0.15f);
}

TEST_CASE ("DOWNPOUR: the MOD / LO-FI view's SIGNATURE slot, and its page", "[downpour][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (2);   // 0.58.0: in the SIGNATURE slot
    auto& unit = fx.getModFxUnit (5);
    CHECK (unit.getSpec().name == "DOWNPOUR");
    CHECK (unit.isVisible());
    CHECK (unit.getBottom() <= fx.getHeight());
    CHECK (fx.getModFxUnit (2).getBottom() <= unit.getY());

    unit.getSpec().onEdit();
    auto& page = fx.getDownpourPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 9);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
