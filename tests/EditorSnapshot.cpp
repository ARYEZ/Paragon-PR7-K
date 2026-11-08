#include <catch2/catch_test_macros.hpp>

#include "Parameters.h"
#include "PluginEditor.h"
#include "dsp/effects/Wah.h"
#include "dsp/effects/Echoes.h"
#include "PluginProcessor.h"
#include "SampleLibraries.h"
#include "dsp/effects/Drive.h"
#include "dsp/effects/LoFi.h"
#include "dsp/effects/Reverb.h"
#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/RigBrowser.h"
#include "dsp/synth/Mimic.h"

// Hidden test (the "[.]" tag): renders the editor to a PNG for reviewing the
// UI without a DAW. Run with:
//   PARAGON_SNAPSHOT=editor.png ./ParagonX1Tests "[snapshot]"
// Options (environment variables):
//   PARAGON_SNAPSHOT_WIDTH=n         the window's width (default 1600; 2560 for a sharp 2x look)
//   PARAGON_SNAPSHOT_PAGE=presets    the preset browser (with two example user presets), or =settings
//   PARAGON_SNAPSHOT_BANK=n          with PAGE=presets: showing bank n (5: the first expansion), its first preset loaded
//   PARAGON_SNAPSHOT_PRESET=name     that factory preset loaded
//   PARAGON_SNAPSHOT_DOWNLOAD=0.34   the orchestra shown downloading (1.5: unpacking, half way)
//   PARAGON_SNAPSHOT_OSC=n           oscillator tab n (1 to 8) selected
//   PARAGON_SNAPSHOT_OSCPAGE=voices  the oscillator's VOICES page (unison, vibrato, chorder); =shape its SHAPE page; =fm its FM page; =fft its FFT page; =draw its DRAW page; =travel its TRAVEL page (PARAGON_SNAPSHOT_TRAVEL=1: oscillator 1 with a demo envelope and pattern)
//   PARAGON_SNAPSHOT_FILTERPAGE=n     the FILTER panel's SHP n tab (shaper n's page)
//   PARAGON_SNAPSHOT_FINISH=white|black|neon   Pearl White, Pearl Black or Pearl White + Neon (0.74.1);
//   PARAGON_SNAPSHOT_RACKVIEW=1      the SYNTH page with the rack shown (the OSCILLATORS and FILTER panels; 0.49.0);
//                                    on by itself for OSC, OSCPAGE, FILTERPAGE, FFT and TRAVEL. Otherwise the front panel.
//   PARAGON_SNAPSHOT_FFT=band:dB,...  a curve drawn for oscillator 1; PARAGON_SNAPSHOT_FFT_VIEW=harmonic|bins
//   PARAGON_SNAPSHOT_PARAMS=id=value,...   parameters set (real units)
//   PARAGON_SNAPSHOT_PAGE=fx         THE RACK (PARAGON_SNAPSHOT_RACK=1: at work on a chord, the sends too; PARAGON_SNAPSHOT_FX=eq|dominion|comp|limiter|sends|delay|reverb|modfx|mod|mod2|lofi|inserts|insdrive|insglitch|insmod|inslofi: that view, the inserts' on bus PARAGON_SNAPSHOT_BUS=1-4)
//   PARAGON_SNAPSHOT_LIBRARIES=<folder>  the sample libraries (the Salamander's folder in it) for the piano presets
//   PARAGON_SNAPSHOT_SFZ=<file>      oscillator 1 playing that SFZ instrument (the rack shown, its key map)
//   PARAGON_SNAPSHOT_WTREF=<ref>     a preset whose oscillator 1 names that pack table ("{wt:<pack>}/<file>"; 0.76.0)
//   PARAGON_SNAPSHOT_NOTES=1         a chord held on the keyboard (so the PLAY panel counts notes, and keys light)
TEST_CASE ("Render the editor to a PNG", "[.][snapshot]")
{
    const auto env = [] (const char* name) { return juce::SystemStats::getEnvironmentVariable (name, {}); };
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "editor.png");

    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);

    const auto rigFolder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ParagonX1SnapshotRigs");
    rigFolder.deleteRecursively();
    processor.useRigFolder (rigFolder);

    const auto page = env ("PARAGON_SNAPSHOT_PAGE");
    if (page == "presets")
    {
        processor.loadRig (*processor.getRigLibrary().findFactory ("Warm Pad"));
        processor.saveCurrentRigAs ("My Pad");
        processor.saveCurrentRigAs ("Night Drive Lead");
        if (const auto bank = env ("PARAGON_SNAPSHOT_BANK"); bank.isNotEmpty())
            for (const auto& entry : processor.getRigLibrary().getEntries())
                if (entry.rig.bank == bank.getIntValue())
                {
                    processor.loadRig (entry);
                    break;
                }
    }

    // PARAGON_SNAPSHOT_FINISH=white|black|neon: the unit's finish (0.74.1), not remembered.
    if (const auto finish = env ("PARAGON_SNAPSHOT_FINISH"); finish.isNotEmpty())
        Finish::set (finish == "white" ? Finish::Type::pearlWhite : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlBlack, false);

    MainView::offerLibrary = false;   // (no window asking to download the piano)
    // PARAGON_SNAPSHOT_LIBRARIES: a folder holding the Salamander's, linked in so the piano presets play.
    if (const juce::File libraries (env ("PARAGON_SNAPSHOT_LIBRARIES")); libraries.getChildFile (libraries::salamanderGrand().folder).isDirectory())
    {
        processor.getLibrariesFolder().createDirectory();
        libraries.getChildFile (libraries::salamanderGrand().folder)
            .createSymbolicLink (processor.getLibrariesFolder().getChildFile (libraries::salamanderGrand().folder), true);
    }
    // PARAGON_SNAPSHOT_DOWNLOAD=0.34: the screen and header as the orchestra downloads (a fraction: downloading; 1+f: unpacking).
    if (const auto value = env ("PARAGON_SNAPSHOT_DOWNLOAD"); value.isNotEmpty())
    {
        const auto f = value.getFloatValue();
        processor.getLibraryDownloadForTest().showForTest (libraries::sonatinaOrchestra(),
            f < 1.0f ? libraries::Download::State::downloading : libraries::Download::State::unpacking, f < 1.0f ? f : f - 1.0f);
    }
    if (const auto name = env ("PARAGON_SNAPSHOT_PRESET"); name.isNotEmpty())
    {
        const auto* entry = processor.getRigLibrary().findFactory (name);
        REQUIRE (entry != nullptr);
        processor.loadRig (*entry);
        processor.waitForSfz();
    }

    if (const auto ref = env ("PARAGON_SNAPSHOT_WTREF"); ref.isNotEmpty())
    {
        RigLibrary::Entry entry;
        entry.rig.name = "Pack Table";
        entry.rig.parameters[ParamIDs::osc (0, "Wave")] = (float) synth::OscWave::wavetable;
        entry.rig.wavetableRefs[0] = ref;
        processor.loadRig (entry);
    }

    for (const auto& pair : juce::StringArray::fromTokens (env ("PARAGON_SNAPSHOT_PARAMS"), ",", {}))
    {
        const auto id = pair.upToFirstOccurrenceOf ("=", false, false).trim();
        auto* parameter = processor.getParameters().getParameter (id);
        INFO (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    }

    // A chord held, played for a moment so the notes are sounding.
    if (env ("PARAGON_SNAPSHOT_NOTES") == "1")
    {
        for (const auto note : { 48, 55, 60, 64 })
            processor.getKeyboardState().noteOn (1, note, 0.8f);
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        for (int block = 0; block < 40; ++block)
            processor.processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    if (page == "fx")
    {
        // THE RACK (stage 7a). PARAGON_SNAPSHOT_RACK=1: all four units at work on "Unison Strings" holding a chord
        // (ECHELON's low cut, a dip, a dynamic presence band and an air shelf; DOMINION; CONSTRICTOR; the LIMITER
        // on MODERN with TRUE PEAK, 6 dB in), the editor's timers run so the ladders and screens read it.
        // PARAGON_SNAPSHOT_FX=eq|dominion|comp|limiter opens that unit's page.
        view->showPage (MainView::Page::fx);
        if (env ("PARAGON_SNAPSHOT_RACK") == "1")
        {
            processor.loadRig (*processor.getRigLibrary().findFactory ("Unison Strings"));
            const auto set = [&] (const juce::String& id, float value)
            {
                auto* parameter = processor.getParameters().getParameter (id);
                REQUIRE (parameter != nullptr);
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            // (PARAGON_SNAPSHOT_PARAMS again: loading the preset undid them.)
            for (const auto& pair : juce::StringArray::fromTokens (env ("PARAGON_SNAPSHOT_PARAMS"), ",", {}))
                set (pair.upToFirstOccurrenceOf ("=", false, false).trim(), pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            struct Band { eq::Shape shape; float hz, db, q, dynamic; };
            const Band bands[] = { { eq::Shape::lowCut, 40.0f, 0.0f, 0.7f, 0.0f }, { eq::Shape::bell, 280.0f, -3.0f, 1.2f, 0.0f },
                                   { eq::Shape::bell, 3200.0f, 1.5f, 1.5f, -4.0f }, { eq::Shape::highShelf, 11000.0f, 2.0f, 0.7f, 0.0f } };
            for (int i = 0; i < 4; ++i)
            {
                set (ParamIDs::eqBand (i, "Used"), 1.0f);
                set (ParamIDs::eqBand (i, "Shape"), (float) (int) bands[i].shape);
                set (ParamIDs::eqBand (i, "Freq"), bands[i].hz);
                set (ParamIDs::eqBand (i, "Gain"), bands[i].db);
                set (ParamIDs::eqBand (i, "Q"), bands[i].q);
                set (ParamIDs::eqBand (i, "Dyn"), bands[i].dynamic);
            }
            set (ParamIDs::eqOn, 1.0f);
            set (ParamIDs::domOn, 1.0f);
            set (ParamIDs::domDepth, 7.0f);
            set (ParamIDs::compOn, 1.0f);
            set (ParamIDs::compThreshold, -26.0f);
            set (ParamIDs::compRatio, 3.0f);
            set (ParamIDs::limitOn, 1.0f);
            set (ParamIDs::limitTruePeak, 1.0f);
            set (ParamIDs::limitGain, 6.0f);
            // The SENDS (7b): DELAY in ping-pong eighths dotted, REVERB a hall; oscillator 2 (an octave up) sends less.
            set (ParamIDs::delayOn, 1.0f);
            set (ParamIDs::delayPingPong, 1.0f);
            set (ParamIDs::delaySync, 7.0f);
            set (ParamIDs::delayFeedback, 0.45f);
            set (ParamIDs::delayToReverb, 0.3f);
            set (ParamIDs::reverbOn, 1.0f);
            set (ParamIDs::reverbType, (float) (int) effects::Reverb::Type::hall);
            set (ParamIDs::reverbSize, 0.65f);
            set (ParamIDs::osc (1, "On"), 1.0f);
            set (ParamIDs::osc (1, "Transpose"), 12.0f);
            set (ParamIDs::osc (1, "SendDelay"), 0.4f);
            set (ParamIDs::osc (1, "SendReverb"), 0.6f);
            // MOD / LO-FI (7c): the Vortex in STEREO, AEON after it, a gently worn cassette.
            set (ParamIDs::modOn, 1.0f);
            set (ParamIDs::modStereo, 1.0f);
            set (ParamIDs::modMix, 0.6f);
            set (ParamIDs::mod2On, 1.0f);
            set (ParamIDs::mod2Depth, 0.6f);
            set (ParamIDs::mod2Res, 6.5f);
            set (ParamIDs::lofiOn, 1.0f);
            set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette);
            set (ParamIDs::lofiAmount, 3.0f);
            set (ParamIDs::lofiNoise, 2.0f);
            // THE INSERTS (7d): oscillator 2 through bus A: SKYLINE driven, a SEQUENCE glitch.
            set (ParamIDs::osc (1, "Insert"), 1.0f);
            set (ParamIDs::ins (0, "DriveOn"), 1.0f);
            set (ParamIDs::ins (0, "DriveModel"), (float) (int) effects::Drive::Model::skyline);
            set (ParamIDs::ins (0, "DriveGain"), 7.0f);
            set (ParamIDs::ins (0, "GlitchOn"), 1.0f);
            set (ParamIDs::ins (0, "GlitchMix"), 0.6f);
            // 0.53.0: ECHOES, the last bar coming back backwards.
            set (ParamIDs::echoOn, 1.0f);
            set (ParamIDs::echoMode, (float) (int) effects::Echoes::Mode::reverse);
            set (ParamIDs::echoDistance, 1.0f);
            set (ParamIDs::echoMix, 0.4f);
            // 0.56.0: CRACKLE, a steady counter following the sound.
            set (ParamIDs::crackleOn, 1.0f);
            set (ParamIDs::crackleRate, 60.0f);
            set (ParamIDs::crackleCluster, 0.7f);
            set (ParamIDs::crackleLevel, -18.0f);
            // 0.57.0: DOWNPOUR, steady rain, its drops on the chord, into the reverb.
            set (ParamIDs::downpourOn, 1.0f);
            set (ParamIDs::downpourRain, 0.6f);
            set (ParamIDs::downpourDrops, 0.9f);
            set (ParamIDs::downpourLevel, -14.0f);
            // 0.58.0: SIROCCO, the chord answered a 3rd up in C major, two voices.
            set (ParamIDs::siroccoOn, 1.0f);
            set (ParamIDs::siroccoScale, 1.0f);
            set (ParamIDs::siroccoVoices, 2.0f);
            set (ParamIDs::siroccoLevel, 1.0f);
            // 0.59.0: INFERNAL, the chord destroyed: AMP and FUZZ, some noise.
            set (ParamIDs::infernalOn, 1.0f);
            set (ParamIDs::infernalMix, 0.5f);
            set (ParamIDs::infernalNoise, 0.5f);
            // 0.60.0: AFTERGLOW, WORN.
            set (ParamIDs::afterglowOn, 1.0f);
            set (ParamIDs::afterglowAge, 0.6f);
            set (ParamIDs::afterglowDrift, 0.5f);
            set (ParamIDs::afterglowEnsemble, 0.65f);
            set (ParamIDs::afterglowTape, 0.4f);
            // 0.61.0: LIGHTTRAIL, climbing a fifth a repeat.
            set (ParamIDs::trailOn, 1.0f);
            set (ParamIDs::trailRise, 7.0f);
            set (ParamIDs::trailLength, 0.6f);
            set (ParamIDs::trailMix, 0.5f);
            // 0.62.0: BLACKHOLE, a deep haze.
            set (ParamIDs::blackholeOn, 1.0f);
            set (ParamIDs::blackholeHold, 0.6f);
            set (ParamIDs::blackholeMix, 0.5f);
            // 0.63.0: DILATION, half speed (the chord falling behind real time).
            set (ParamIDs::dilationOn, 1.0f);
            set (ParamIDs::dilationSlow, 0.5f);
            set (ParamIDs::dilationRamp, 0.3f);
            // 0.64.0: REMNANT, three layers beneath.
            set (ParamIDs::remnantOn, 1.0f);
            set (ParamIDs::remnantLayers, 2.0f);
            set (ParamIDs::remnantMix, 0.6f);
            // 0.65.0: PROXIMITY, close up.
            set (ParamIDs::proximityOn, 1.0f);
            set (ParamIDs::proximityNoise, 0.6f);
            // 0.66.0: HAZE, lights passing.
            set (ParamIDs::hazeOn, 1.0f);
            set (ParamIDs::hazeDepth, 0.6f);
            // 0.67.0: SPECTRE, its ghost.
            set (ParamIDs::spectreOn, 1.0f);
            set (ParamIDs::spectreMix, 0.6f);
            // 0.68.0: LEVIATHAN, its voice.
            set (ParamIDs::leviathanOn, 1.0f);
            set (ParamIDs::leviathanMix, 0.6f);
            // 0.69.0: NUCLEARSPRING, its tank.
            set (ParamIDs::springOn, 1.0f);
            set (ParamIDs::springMix, 0.6f);
                        // 0.51.0: its WAH in AUTO, opened by the chord.
            set (ParamIDs::ins (0, "WahOn"), 1.0f);
            set (ParamIDs::ins (0, "WahMode"), (float) (int) effects::Wah::Mode::autoWah);
            set (ParamIDs::ins (0, "WahPosition"), 0.15f);
            set (ParamIDs::ins (0, "WahDepth"), 0.7f);
            set (ParamIDs::ins (0, "WahResonance"), 7.0f);
            for (const auto note : { 48, 55, 60, 64, 67 })
                processor.getKeyboardState().noteOn (1, note, 0.85f);
            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            for (int block = 0; block < 700; ++block)
            {
                processor.processBlock (buffer, midi);
                if (block > 600 && block % 10 == 0)   // the timers fire only when they're due
                {
                    juce::Thread::sleep (35);
                    juce::Timer::callPendingTimersSynchronously();
                }
            }
        }
        const auto fx = env ("PARAGON_SNAPSHOT_FX");
        auto& fxPage = view->getFxPage();
        if (const auto bus = env ("PARAGON_SNAPSHOT_BUS").getIntValue(); bus >= 1 && bus <= 4)
            fxPage.getInsertStrip().setBus (bus - 1);
        fxPage.show (fx == "eq" ? FxPage::View::eq : fx == "dominion" ? FxPage::View::dominion : fx == "comp" ? FxPage::View::comp
                     : fx == "limiter" ? FxPage::View::limiter : fx == "sends" ? FxPage::View::sends : fx == "delay" ? FxPage::View::delay
                     : fx == "reverb" ? FxPage::View::reverb : fx == "modfx" ? FxPage::View::modFx : fx == "mod" ? FxPage::View::mod
                     : fx == "mod2" ? FxPage::View::mod2 : fx == "lofi" ? FxPage::View::loFi : fx == "inserts" ? FxPage::View::inserts
                     : fx == "insdrive" ? FxPage::View::insertDrive : fx == "insglitch" ? FxPage::View::insertGlitch
                     : fx == "insmod" ? FxPage::View::insertMod : fx == "inslofi" ? FxPage::View::insertLoFi
                     : fx == "inswah" ? FxPage::View::insertWah : fx == "echoes" ? FxPage::View::echoes : fx == "crackle" ? FxPage::View::crackle : fx == "downpour" ? FxPage::View::downpour : fx == "sirocco" ? FxPage::View::sirocco : fx == "infernal" ? FxPage::View::infernal : fx == "afterglow" ? FxPage::View::afterglow : fx == "lighttrail" ? FxPage::View::lightTrail : fx == "blackhole" ? FxPage::View::blackHole : fx == "dilation" ? FxPage::View::dilation : fx == "remnant" ? FxPage::View::remnant : fx == "proximity" ? FxPage::View::proximity : fx == "haze" ? FxPage::View::haze : fx == "spectre" ? FxPage::View::spectre : fx == "leviathan" ? FxPage::View::leviathan : fx == "nuclearspring" ? FxPage::View::nuclearSpring : fx == "lightspeed" ? FxPage::View::lightSpeed : fx == "bloom" ? FxPage::View::bloom : fx == "legion" ? FxPage::View::legion : fx == "overcharge" ? FxPage::View::overcharge : fx == "aftermath" ? FxPage::View::aftermath : fx == "submerge" ? FxPage::View::submerge : fx == "lockstep" ? FxPage::View::lockstep : fx == "colossus" ? FxPage::View::colossus : fx == "canyon" ? FxPage::View::canyon : fx == "jackin" ? FxPage::View::jackIn : FxPage::View::rack);
        // 0.58.0: the SIGNATURE slot's effect shown (0 ECHOES, 1 CRACKLE, 2 DOWNPOUR, 3 SIROCCO, 4 INFERNAL, 5 AFTERGLOW, 6 LIGHTTRAIL, 7 BLACKHOLE, 8 DILATION, 9 REMNANT, 10 PROXIMITY, 11 HAZE, 12 SPECTRE, 13 LEVIATHAN, 14 NUCLEARSPRING).
        if (const auto signature = env ("PARAGON_SNAPSHOT_SIGNATURE"); signature.isNotEmpty())
            fxPage.showSignature (signature.getIntValue());
        // A display reads its bands and analyser on its own timer, but only while it's on screen, which a snapshot
        // never is (as PR7-E's snapshots, refresh them by hand). ECHELON with its dynamic band 3 selected.
        if (fx == "eq")
        {
            fxPage.getEqPage().getDisplay().refresh();
            fxPage.getEqPage().getDisplay().select (2);
            fxPage.getEqPage().refreshControls();
            if (env ("PARAGON_SNAPSHOT_MATCH") == "1")
            {
                fxPage.getEqPage().showMatch (true);
                fxPage.getEqPage().getMatchPanel().refresh();
            }
        }
        if (fx == "dominion")
            fxPage.getDominionPage().getDisplay().refresh();
        if (fx == "comp")
            fxPage.getCompPage().getDisplay().refresh();
        if (fx == "limiter")
            fxPage.getLimiterPage().refresh();
    }
    view->setRackShownForTest (env ("PARAGON_SNAPSHOT_RACKVIEW") == "1" || env ("PARAGON_SNAPSHOT_OSC").isNotEmpty()
                               || env ("PARAGON_SNAPSHOT_OSCPAGE").isNotEmpty() || env ("PARAGON_SNAPSHOT_FILTERPAGE").isNotEmpty()
                               || env ("PARAGON_SNAPSHOT_FFT").isNotEmpty() || env ("PARAGON_SNAPSHOT_TRAVEL").isNotEmpty()
                               || env ("PARAGON_SNAPSHOT_SFZ").isNotEmpty());
    if (const auto sfz = env ("PARAGON_SNAPSHOT_SFZ"); sfz.isNotEmpty())
    {
        processor.loadSfz (0, juce::File (sfz));
        processor.waitForSfz();
        view->getOscillatorPanel().selectOscillator (0);
        view->getOscillatorPanel().getSfzChooser().refresh();
    }
    if (const auto osc = env ("PARAGON_SNAPSHOT_OSC").getIntValue(); osc > 0)
        view->getOscillatorPanel().selectOscillator (osc - 1);
    if (env ("PARAGON_SNAPSHOT_TRAVEL") == "1")
    {
        // Oscillator 1's TRAVEL with a drawn envelope, a pattern and strength curves.
        auto matrix = processor.getModMatrix();
        auto& t = matrix.travel[0];
        t.envelope = { { { 0.0f, 0.0f, 0.0f }, { 0.3f, 1.0f, -0.4f }, { 0.7f, 0.35f, 0.3f }, { 1.0f, 0.8f, 0.0f } }, -1, -1, -1 };
        using M = synth::travel::StepMode;
        const synth::travel::Step steps[] = { { M::normal, 1.0f, 1 }, { M::normal, 2.0f, 1 }, { M::trigger, 4.0f, 1 }, { M::reverse, 1.0f, 1 },
                                              { M::pingPongHalf, 2.0f, 2 }, { M::high, 1.0f, 1 }, { M::silence, 1.0f, 1 }, { M::low, 1.0f, 1 } };
        for (int k = 0; k < 8; ++k)
        {
            t.pattern.steps[(size_t) k] = steps[k];
            t.pattern.strength[(size_t) k] = { k % 2 == 0 ? 1.0f : 0.2f, 0.6f, k % 2 == 0 ? 0.3f : 1.0f, k == 4, k == 2 ? -0.6f : 0.0f };
        }
        processor.setModMatrix (matrix);
        view->getOscillatorPanel().selectOscillator (0);
    }
    if (env ("PARAGON_SNAPSHOT_OSCPAGE") == "travel")
        view->getOscillatorPanel().showPage (OscillatorPanel::Page::travel);
    if (env ("PARAGON_SNAPSHOT_OSCPAGE") == "voices")
        view->getOscillatorPanel().showPage (OscillatorPanel::Page::voices);
    if (env ("PARAGON_SNAPSHOT_OSCPAGE") == "shape")
        view->getOscillatorPanel().showPage (OscillatorPanel::Page::shape);
    if (env ("PARAGON_SNAPSHOT_OSCPAGE") == "fm")
        view->getOscillatorPanel().showPage (OscillatorPanel::Page::fm);
    if (env ("PARAGON_SNAPSHOT_OSCPAGE") == "fft")
        view->getOscillatorPanel().showPage (OscillatorPanel::Page::fft);
    if (env ("PARAGON_SNAPSHOT_PAGE") == "mod")
        view->showPage (MainView::Page::modulation);
    if (env ("PARAGON_SNAPSHOT_PAGE") == "arp")
    {
        // ARP 1 on, oscillators 1 and 2 routed to it, with a pattern that shows every kind of block.
        view->showPage (MainView::Page::arp);
        auto& page = view->getArpPage();
        if (env ("PARAGON_SNAPSHOT_ARP") == "1")
        {
            page.route (0, true);
            page.route (1, true);
            if (auto* on = processor.getParameters().getParameter (ParamIDs::arp (0, "On")))
                on->setValueNotifyingHost (1.0f);
            synth::arp::Pattern p;
            const int transposes[] = { 0, 12, 7, 0, 3, 12, 19, 0, -5, 0, 7, 12, 0, 5, 7, 24 };
            for (int k = 0; k < 16; ++k)
            {
                auto& b = p.blocks[(size_t) k];
                b.on = k != 6 && k != 13;
                b.transpose = transposes[k];
                b.velocity = k % 4 == 0 ? 1.0f : 0.6f + 0.05f * (float) (k % 3);
                b.length = k == 8 ? 2 : 1;
                b.slide = k == 3 || k == 11;
                b.ratchet = k == 15 ? 3 : (k == 5 ? 2 : 1);
            }
            page.getEditor().setShown (p, true);
        }
        if (env ("PARAGON_SNAPSHOT_ARP") == "2")
        {
            // CHORDS mode (5b): a chord progression with tones switched off and sub notes, and a randomizer set up.
            page.route (0, true);
            auto& parameters = processor.getParameters();
            parameters.getParameter (ParamIDs::arp (0, "On"))->setValueNotifyingHost (1.0f);
            auto* mode = parameters.getParameter (ParamIDs::arp (0, "Mode"));
            mode->setValueNotifyingHost (mode->convertTo0to1 ((float) (int) synth::arp::Mode::chords));
            auto* source = parameters.getParameter (ParamIDs::arp (0, "NoteSource"));
            source->setValueNotifyingHost (source->convertTo0to1 (2.0f));
            synth::arp::Pattern p;
            const char* chords[] = { "Min7", "Min7", "Maj7", "Maj7", "Sus4", "7", "Min9", "Power" };
            const int transposes[] = { 0, 0, 8, 8, 5, 7, 3, 0 };
            for (int k = 0; k < 8; ++k)
            {
                auto& b = p.blocks[(size_t) (k * 2)];
                b.on = true;
                b.length = k % 2 == 0 ? 2 : 1;
                b.transpose = transposes[k];
                b.chord = synth::arp::chordNames.indexOf (chords[k]);
                b.velocity = k % 2 == 0 ? 1.0f : 0.7f;
                b.sub = k % 4 == 0;
                b.subTranspose = k == 4 ? -24 : -12;
                b.chordMask = k == 3 ? 0x1f & ~0x4 : 0x1f;
            }
            page.getEditor().setShown (p, true);
            auto r = synth::arp::Randomizer {};
            r.scale = synth::arp::scaleNames.indexOf ("Dorian");
            r.autoDice = true;
            r.seed = 42;
            r.rollLengths = false;
            auto matrix = processor.getModMatrix();
            matrix.arpRandomizers[0] = r;
            processor.setModMatrix (matrix);
        }
        page.selectArp (0);
    }
    if (const auto lfo = env ("PARAGON_SNAPSHOT_LFO").getIntValue(); lfo > 0)
        view->getModPage().getModulatorPanel().selectLfo (lfo - 1);
    if (const auto envelope = env ("PARAGON_SNAPSHOT_ENV").getIntValue(); envelope > 0)
        view->getModPage().getModulatorPanel().selectEnvelope (envelope - 1);
    if (env ("PARAGON_SNAPSHOT_PITCH") == "1")
        view->getModPage().getModulatorPanel().selectPitch();
    if (env ("PARAGON_SNAPSHOT_MIDI") == "1")
    {
        // A few MIDI links, a knob learning, and a route made by dropping LFO 1 on the cutoff.
        MidiLinks links;
        links.link (74, ParamIDs::fltCutoff);
        links.link (71, ParamIDs::fltRes);
        links.link (1, ParamIDs::osc (0, "WtIndex"));
        links.link (7, ParamIDs::ampVolume);
        processor.setMidiLinks (links);
        processor.startMidiLearn (ParamIDs::fltDrive);
        processor.controllerForTest (74, 0.6f);
        view->addRoute ("lfo1", ParamIDs::fltCutoff);
        view->getSourceStrip().refresh();
        if (env ("PARAGON_SNAPSHOT_PAGE") == "settings")
        {
            view->showPage (MainView::Page::synth);
            view->showPage (MainView::Page::settings);
        }
    }
    if (env ("PARAGON_SNAPSHOT_STEP") == "2")   // the STEP tab as the sound has it
        view->getModPage().getModulatorPanel().selectStepSeq();
    if (env ("PARAGON_SNAPSHOT_STEP") == "1")
    {
        // A pattern with heights, a rest and linked steps (on both rows, for STEREO).
        auto matrix = processor.getModMatrix();
        for (auto& row : matrix.stepPattern.rows)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = row[(size_t) k];
                step.level = k % 4 == 0 ? 1.0f : k % 4 == 2 ? 0.7f : 0.45f;
                step.on = k % 8 != 7;
                step.tied = k % 8 == 4;
            }
        matrix.stepPattern.rows[1][0].on = false;
        processor.setModMatrix (matrix);
        view->getModPage().getModulatorPanel().selectStepSeq();
    }
    if (const auto shape = env ("PARAGON_SNAPSHOT_SHAPE").getIntValue(); shape > 0)
        view->getModPage().getModulatorPanel().selectShape (shape - 1);
    if (const auto page = env ("PARAGON_SNAPSHOT_FILTERPAGE").getIntValue(); page > 0)
        view->getFilterPanel().showPage (page);
    if (env ("PARAGON_SNAPSHOT_OSCPAGE") == "draw")
        view->getOscillatorPanel().showPage (OscillatorPanel::Page::draw);
    if (const auto curve = env ("PARAGON_SNAPSHOT_FFT"); curve.isNotEmpty())
    {
        // A curve for oscillator 1 ("band:dB band:dB ...", as presets keep it), and its view (FFT_VIEW=harmonic|bins).
        processor.setSpectrum (0, synth::SpectrumEdit::decode (curve.replaceCharacter (',', ' ')));
        processor.finishTableBuilds();
    }
    if (env ("PARAGON_SNAPSHOT_FFT_VIEW") == "harmonic")
        view->getOscillatorPanel().getSpectrumEditor().setView (SpectrumEditor::View::harmonic);
    if (env ("PARAGON_SNAPSHOT_FFT_VIEW") == "bins")
        view->getOscillatorPanel().getSpectrumEditor().setView (SpectrumEditor::View::bins);
    if (page == "presets")
    {
        view->showPage (MainView::Page::presets);
        // BANK=n: the browser showing bank n (5 is the first expansion, NEON RAIN), its first preset loaded.
        if (const auto bank = env ("PARAGON_SNAPSHOT_BANK"); bank.isNotEmpty())
        {
            std::function<RigBrowser* (juce::Component&)> findBrowser = [&] (juce::Component& c) -> RigBrowser*
            {
                if (auto* b = dynamic_cast<RigBrowser*> (&c))
                    return b;
                for (auto* child : c.getChildren())
                    if (auto* found = findBrowser (*child))
                        return found;
                return nullptr;
            };
            auto* browser = findBrowser (*view);
            REQUIRE (browser != nullptr);
            browser->showFilter (0, bank.getIntValue());
        }
    }
    else if (page == "settings")
        view->showPage (MainView::Page::settings);
    else if (page == "mimic")
    {
        // MIMIC's panel, having heard a made note: G3, a held, darkening tone with vibrato from half a second, a
        // little breath, stereo.
        view->showPage (MainView::Page::presets);
        std::function<RigBrowser* (juce::Component&)> findBrowser = [&] (juce::Component& c) -> RigBrowser*
        {
            if (auto* b = dynamic_cast<RigBrowser*> (&c))
                return b;
            for (auto* child : c.getChildren())
                if (auto* found = findBrowser (*child))
                    return found;
            return nullptr;
        };
        auto* browser = findBrowser (*view);
        REQUIRE (browser != nullptr);
        constexpr double rate = 48000.0;
        juce::AudioBuffer<float> note (2, (int) (2.6 * rate));
        juce::Random random (5);
        double phase = 0.0;
        for (int i = 0; i < note.getNumSamples(); ++i)
        {
            const auto t = i / rate;
            const auto hz = 196.0 * std::exp2 ((t > 0.5 ? 22.0 * std::sin (juce::MathConstants<double>::twoPi * 5.2 * t) : 0.0) / 1200.0);
            phase += hz / rate;
            double sum = 0.0;
            for (int k = 1; k * hz < 20000.0 && k <= 40; ++k)
                sum += std::sin (juce::MathConstants<double>::twoPi * k * phase) / k * std::exp (-(k - 1) * t * 0.6);
            const auto level = t < 0.12 ? t / 0.12 : t < 1.9 ? 0.55 + 0.45 * std::exp (-(t - 0.12) / 0.15)
                                                            : (0.55 + 0.45 * std::exp (-1.78 / 0.15)) * std::pow (10.0, -100.0 * (t - 1.9) / 20.0);
            const auto breath = 0.5 * (random.nextDouble() * 2.0 - 1.0);
            note.setSample (0, i, (float) (0.3 * level * (sum + breath)));
            note.setSample (1, i, (float) (0.3 * level * (sum * 0.9 + breath)));
        }
        processor.setMimicReference (note, rate);
        browser->showMimic();
        browser->getMimicPanel().setAnalysis (synth::mimic::analyse (note, rate, "Breathy Lead G3"));
        // MIMIC=1: made and refined (part 2): the match, and the made sound's loudness over the recording's.
        if (env ("PARAGON_SNAPSHOT_MIMIC") == "1")
        {
            browser->getMimicPanel().makeSound();
            browser->getMimicPanel().refine();
            browser->getMimicPanel().finishRefining();
        }
    }

    const auto width = juce::jmax (960, juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_WIDTH", "1600").getIntValue());
    editor->setSize (width, width * 9 / 16);
    editor->resized();

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}

// PARAGON_SNAPSHOT=menu.png ./ParagonX1Tests "[snapshot-menu]": a drop-down
// menu as the plugin draws it (menus open in their own window, so the editor
// snapshot can't show one): the FILTER's types, LP 24 ticked, one highlighted.
TEST_CASE ("Render a drop-down menu to a PNG", "[.][snapshot-menu]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "menu.png");
    ParagonLookAndFeel look;
    const auto& items = synth::filterTypeNames;
    constexpr int scale = 2, width = 150, itemHeight = 26;
    juce::Image image (juce::Image::ARGB, width * scale, (itemHeight * items.size() + 8) * scale, true);
    juce::Graphics g (image);
    g.addTransform (juce::AffineTransform::scale ((float) scale));
    look.drawPopupMenuBackground (g, width, itemHeight * items.size() + 8);
    for (int i = 0; i < items.size(); ++i)
        look.drawPopupMenuItem (g, { 0, 4 + i * itemHeight, width, itemHeight }, false, true, i == 5, items[i] == "LP 24", false,
                                items[i], {}, nullptr, nullptr);

    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}
