// WAVETABLE PACKS (0.76.0): a preset names a table in a pack on this computer (the presets folder's Libraries)
// instead of carrying it; without the pack it plays its fallback factory table and says what it needs.

#include <catch2/catch_test_macros.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "dsp/synth/Wavetable.h"
#include "rigs/Rig.h"

namespace
{
    struct TempFolder
    {
        TempFolder() : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("pr7k-wtpack-" + juce::String (juce::Random::getSystemRandom().nextInt64())))
        {
            dir.createDirectory();
        }
        ~TempFolder() { dir.deleteRecursively(); }
        juce::File dir;
    };

    // A pack's file as such packs ship them: 32-bit float WAV, 2048 samples a frame. Frame k is the sine of harmonic
    // `harmonics[k]` alone.
    void writePackTable (const juce::File& file, std::initializer_list<int> harmonics)
    {
        file.getParentDirectory().createDirectory();
        juce::AudioBuffer<float> audio (1, 2048 * (int) harmonics.size());
        auto frame = 0;
        for (const auto h : harmonics)
        {
            for (int i = 0; i < 2048; ++i)
                audio.setSample (0, frame * 2048 + i, 0.8f * (float) std::sin (juce::MathConstants<double>::twoPi * h * i / 2048.0));
            ++frame;
        }
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), 48000.0, 1, 32, {}, 0));
        REQUIRE (writer != nullptr);
        stream.release();   // the writer owns it now
        writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }

    struct Plugin
    {
        explicit Plugin (const juce::File& presets)
        {
            processor.useRigFolder (presets);
            processor.setPlayConfigDetails (0, 2, 48000.0, 256);
            processor.prepareToPlay (48000.0, 256);
            processor.setNonRealtime (true);
            processor.seedRandomForTest (1);
        }

        // Holds A3 (220 Hz) for half a second; the left channel.
        std::vector<float> play()
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, 0.9f), 0);
            for (int k = 0; k < 94; ++k)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
                midi.clear();
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 256);
            }
            midi.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
            for (int k = 0; k < 60; ++k)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
                midi.clear();
            }
            return out;
        }

        ParagonX1AudioProcessor processor;
    };

    // How strong `hz` is in the second half of `signal` (a plain DFT bin).
    double strength (const std::vector<float>& signal, double hz)
    {
        const auto from = signal.size() / 2;
        double re = 0.0, im = 0.0;
        for (auto i = from; i < signal.size(); ++i)
        {
            const auto phase = juce::MathConstants<double>::twoPi * hz * (double) i / 48000.0;
            re += signal[i] * std::cos (phase);
            im += signal[i] * std::sin (phase);
        }
        return std::sqrt (re * re + im * im) / (double) (signal.size() - from);
    }

    double db (double ratio) { return 20.0 * std::log10 (juce::jmax (1.0e-12, ratio)); }

    // A preset made on a pack: oscillator 1 a WAVETABLE at INDEX 0, the factory's first table its fallback.
    Rig packPreset()
    {
        Rig rig;
        rig.name = "Pack Test";
        rig.parameters[ParamIDs::osc (0, "Wave")] = (float) synth::OscWave::wavetable;
        rig.parameters[ParamIDs::osc (0, "Table")] = 0.0f;
        rig.parameters[ParamIDs::osc (0, "WtIndex")] = 0.0f;
        rig.parameters[ParamIDs::osc (0, "Filter")] = 0.0f;
        rig.wavetableRefs[0] = ParagonX1AudioProcessor::wavetableRefFor ("Test Pack", "Leads/Third.wav");
        return rig;
    }

    void load (ParagonX1AudioProcessor& processor, const Rig& rig)
    {
        RigLibrary::Entry entry;
        entry.rig = rig;
        processor.loadRig (entry);
    }
}

TEST_CASE ("Wavetable packs: a preset plays the pack's table from Libraries, or its fallback and says what it needs", "[wavetable][library]")
{
    TempFolder t;
    const auto presets = t.dir.getChildFile ("presets");
    presets.createDirectory();

    // Without the pack: the fallback factory table, and the oscillator says which pack and file.
    Plugin missing (presets);
    load (missing.processor, packPreset());
    CHECK (missing.processor.isWavetableMissing (0));
    CHECK (! missing.processor.hasLoadedWavetable (0));
    CHECK (missing.processor.getWavetable (0) == synth::factoryTables::get (0));
    CHECK (ParagonX1AudioProcessor::wavetablePackOf (missing.processor.getWavetableRef (0)) == "Test Pack");
    const auto fallback = missing.play();
    CHECK (strength (fallback, 220.0) > 0.01);   // it plays, not silence

    // The pack in place: its file, read at once. Frame 0 is harmonic 3 alone: 660 Hz, nothing at 220.
    const auto file = presets.getChildFile ("Libraries/Test Pack/Leads/Third.wav");
    writePackTable (file, { 3, 1, 2, 4 });
    CHECK (missing.processor.resolveWavetableRef (missing.processor.getWavetableRef (0)) == file);
    Plugin found (presets);
    load (found.processor, packPreset());
    REQUIRE (! found.processor.isWavetableMissing (0));
    REQUIRE (found.processor.hasLoadedWavetable (0));
    CHECK (found.processor.getWavetable (0)->getSource().numFrames() == 4);
    CHECK (found.processor.getWavetable (0)->getSource().frameSize == 2048);
    const auto played = found.play();
    const auto third = strength (played, 660.0), fundamental = strength (played, 220.0);
    INFO ("660 Hz " << db (third) << " dB, 220 Hz " << db (fundamental) << " dB");
    CHECK (third > 0.01);
    CHECK (db (fundamental / third) < -40.0);

    // The pack put in place after the preset loaded: LOOK FOR THE PACK AGAIN finds it.
    CHECK (missing.processor.retryWavetableRef (0));
    CHECK (! missing.processor.isWavetableMissing (0));
    CHECK (missing.processor.getWavetable (0)->getSource().numFrames() == 4);
}

TEST_CASE ("Wavetable packs: presets, projects and undo keep the reference; nothing leaves the Libraries folder", "[wavetable][library]")
{
    TempFolder t;
    const auto presets = t.dir.getChildFile ("presets");
    presets.createDirectory();
    writePackTable (presets.getChildFile ("Libraries/Test Pack/Leads/Third.wav"), { 3, 1, 2, 4 });

    // The reference in a preset file, both ways.
    const auto xml = packPreset().toXml();
    const auto back = Rig::fromXml (*xml);
    REQUIRE (back.has_value());
    CHECK (back->wavetableRefs == packPreset().wavetableRefs);
    CHECK (back->wavetables.empty());   // a factory preset carries no table

    Plugin plugin (presets);
    load (plugin.processor, packPreset());
    REQUIRE (plugin.processor.hasLoadedWavetable (0));
    const auto table = plugin.processor.getWavetable (0);

    // Undo: back to the pack's table, the same one (not read again).
    plugin.processor.flushUndoStep();
    plugin.processor.chooseFactoryWavetable (0, 2);
    plugin.processor.flushUndoStep();
    CHECK (plugin.processor.getWavetableRef (0).isEmpty());
    plugin.processor.undo();
    CHECK (plugin.processor.getWavetable (0) == table);
    CHECK (plugin.processor.getWavetableRef (0) == packPreset().wavetableRefs.at (0));

    // A project saved with the pack carries the table (it stays on this computer) and the reference.
    juce::MemoryBlock state;
    plugin.processor.getStateInformation (state);
    TempFolder other;
    Plugin elsewhere (other.dir);   // no pack here
    elsewhere.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (elsewhere.processor.hasLoadedWavetable (0));
    CHECK (! elsewhere.processor.isWavetableMissing (0));
    CHECK (elsewhere.processor.getWavetableRef (0) == packPreset().wavetableRefs.at (0));

    // References can't reach outside the pack's folder.
    CHECK (plugin.processor.resolveWavetableRef ("{wt:Test Pack}/../../secret.wav") == juce::File());
    CHECK (plugin.processor.resolveWavetableRef ("{wt:../x}/a.wav") == juce::File());
    CHECK (plugin.processor.resolveWavetableRef ("{wt:}/a.wav") == juce::File());
}
