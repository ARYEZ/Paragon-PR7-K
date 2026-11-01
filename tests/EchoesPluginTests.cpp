// ECHOES in PR7-K (0.53.0): off changes nothing; on, a note played and let go comes back a bar later in time with the
// song; MIX moved by the mod matrix (MACRO 1 here); the MOD / LO-FI view's fourth unit and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/effects/Echoes.h"
#include "ui/FxPage.h"
#include "ui/MainView.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    struct Song final : public juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 120.0;
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setIsPlaying (playing);
            info.setPpqPosition (ppq);
            return info;
        }
    };

    struct Plugin
    {
        ParagonX1AudioProcessor processor;
        Song song;
        Plugin()
        {
            processor.setPlayHead (&song);
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
        // `seconds` with the song running; a note held for the first `held` seconds. Left side.
        std::vector<float> play (double seconds, double held)
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            processor.getKeyboardState().noteOn (1, 60, 0.8f);
            auto released = false;
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                if (! released && b * blockSize >= held * sampleRate)
                {
                    processor.getKeyboardState().noteOff (1, 60, 0.0f);
                    released = true;
                }
                buffer.clear();
                processor.processBlock (buffer, midi);
                song.ppq += blockSize / sampleRate * song.bpm / 60.0;
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
    };
}

TEST_CASE ("ECHOES in the plugin: off changes nothing; on, a note comes back a bar later", "[echoes]")
{
    Plugin plain, off;
    off.set (ParamIDs::echoMix, 1.0f);   // off: its settings don't matter
    CHECK (plain.play (3.0, 0.5) == off.play (3.0, 0.5));

    Plugin p;
    p.set (ParamIDs::echoOn, 1.0f);
    p.set (ParamIDs::echoMode, (float) (int) effects::Echoes::Mode::echo);
    p.set (ParamIDs::echoDistance, 2.0f);   // 1 bar = 2 s at 120 BPM
    p.set (ParamIDs::echoMix, 1.0f);
    p.set (ParamIDs::echoFeedback, 0.0f);
    const auto out = p.play (3.5, 0.5);
    const auto first = rms (out, (size_t) (0.1 * sampleRate), (size_t) (0.4 * sampleRate));
    const auto gap = rms (out, (size_t) (1.0 * sampleRate), (size_t) (1.9 * sampleRate));
    const auto back = rms (out, (size_t) (2.1 * sampleRate), (size_t) (2.4 * sampleRate));
    INFO ("played " << db (first) << " dB, silence " << db (gap) << " dB, a bar later " << db (back) << " dB");
    CHECK (gap < first * 0.01);
    CHECK (std::abs (db (back) - db (first)) < 3.0);   // AGE 0.3 darkens it a little
}

TEST_CASE ("ECHOES: MIX is a mod-matrix destination (FX Echoes > Mix)", "[echoes]")
{
    REQUIRE (ModDestinations::find (ParamIDs::echoMix) != nullptr);
    CHECK (ModDestinations::find (ParamIDs::echoMix)->group == "FX Echoes");
    Plugin p;
    p.set (ParamIDs::echoOn, 1.0f);
    p.set (ParamIDs::echoMix, 0.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro1", ParamIDs::echoMix, 0.8f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
    p.set (ParamIDs::macros[0], 1.0f);
    p.play (0.2, 0.1);
    CHECK (p.processor.getEchoesSettings().mix > 0.75f);
    p.set (ParamIDs::macros[0], 0.0f);
    p.play (0.3, 0.0);
    CHECK (p.processor.getEchoesSettings().mix < 0.05f);
}

TEST_CASE ("ECHOES: the MOD / LO-FI view's fourth unit, and its page", "[echoes][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    auto& unit = fx.getModFxUnit (3);
    CHECK (unit.getSpec().name == "ECHOES");
    CHECK (unit.isVisible());
    CHECK (unit.getBottom() <= fx.getHeight());
    CHECK (fx.getModFxUnit (2).getBottom() <= unit.getY());

    unit.getSpec().onEdit();   // what EDIT (or the name) does
    auto& page = fx.getEchoesPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 6);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
