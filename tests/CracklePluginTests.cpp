// CRACKLE in PR7-K (0.56.0): off changes nothing; on, clicks come while a note plays and stop with it (FOLLOW); RATE
// moved by the mod matrix (MACRO 3 here, as MELTDOWN's DOSE does); the MOD / LO-FI view's fifth unit and its page.
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
        // `seconds`, a note held for the first `held` seconds. Left side.
        std::vector<float> play (double seconds, double held)
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            if (held > 0.0)
                processor.getKeyboardState().noteOn (1, 60, 0.8f);
            auto released = held <= 0.0;
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                if (! released && b * blockSize >= held * sampleRate)
                {
                    processor.getKeyboardState().noteOff (1, 60, 0.0f);
                    released = true;
                }
                buffer.clear();
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
    };
}

TEST_CASE ("CRACKLE in the plugin: off changes nothing; on, clicks come with the note and stop with it", "[crackle]")
{
    Plugin plain, off;
    off.set (ParamIDs::crackleRate, 500.0f);   // off: its settings don't matter
    CHECK (plain.play (2.0, 0.5) == off.play (2.0, 0.5));

    Plugin p;
    p.set (ParamIDs::crackleOn, 1.0f);
    p.set (ParamIDs::crackleRate, 100.0f);
    p.set (ParamIDs::crackleCluster, 0.0f);
    const auto before = p.processor.getCrackle().getTotalClicks();
    p.play (2.0, 2.5);   // held through (let go just after)
    const auto during = p.processor.getCrackle().getTotalClicks() - before;
    p.processor.getKeyboardState().noteOff (1, 60, 0.0f);
    INFO ("clicks in two seconds of a held note: " << during << " (about 200)");
    CHECK (during > 120);
    CHECK (during < 280);

    // Let go: within a second the clicks have stopped, and the output goes silent.
    p.play (1.0, 0.0);
    const auto after = p.processor.getCrackle().getTotalClicks();
    const auto tail = p.play (2.0, 0.0);
    CHECK (p.processor.getCrackle().getTotalClicks() == after);
    CHECK (*std::max_element (tail.begin(), tail.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) == 0.0f);
}

TEST_CASE ("CRACKLE: RATE, CLUSTER and LEVEL are mod-matrix destinations (FX Crackle)", "[crackle]")
{
    for (const auto* id : { ParamIDs::crackleRate, ParamIDs::crackleCluster, ParamIDs::crackleLevel })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Crackle");
    }
    Plugin p;
    p.set (ParamIDs::crackleOn, 1.0f);
    p.set (ParamIDs::crackleRate, 2.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::crackleRate, 0.6f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
    p.set (ParamIDs::macros[2], 1.0f);
    p.play (0.3, 0.2);
    CHECK (p.processor.getCrackleSettings().rate > 50.0f);   // 60% further up its log range from 2 a second
    p.set (ParamIDs::macros[2], 0.0f);
    p.play (0.3, 0.0);
    CHECK (p.processor.getCrackleSettings().rate < 2.5f);
}

TEST_CASE ("CRACKLE: the MOD / LO-FI view's fifth unit, and its page", "[crackle][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    auto& unit = fx.getModFxUnit (4);
    CHECK (unit.getSpec().name == "CRACKLE");
    CHECK (unit.isVisible());
    CHECK (unit.getBottom() <= fx.getHeight());
    for (int k = 1; k < 5; ++k)
        CHECK (fx.getModFxUnit (k - 1).getBottom() <= fx.getModFxUnit (k).getY());

    unit.getSpec().onEdit();
    auto& page = fx.getCracklePage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 7);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
