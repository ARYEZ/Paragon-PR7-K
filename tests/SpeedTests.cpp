// Speed-ups that must not change the sound (0.68.0): each heavy effect rendered on a held chord, seeded, written
// out as raw floats so a build before a change and a build after can be compared sample for sample, and timed.
// Hidden ("[.speed]"): run with PARAGON_SPEED_DIR=<folder> to write the renders; the times are printed.
#include "SynthTestUtils.h"

#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    struct Scenario
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    // Renders `seconds` of a held A minor chord (saws, four voices of unison) with `params` set; returns left
    // then right, and the time it took.
    std::pair<std::vector<float>, double> render (const Scenario& scenario, double seconds)
    {
        ParagonX1AudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
        processor.seedRandomForTest (1);
        const auto set = [&processor] (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set (ParamIDs::osc (0, "UniVoices"), 4.0f);
        set (ParamIDs::osc (0, "UniMix"), 0.6f);
        for (const auto& [id, value] : scenario.params)
            set (id, value);
        for (const auto note : { 57, 60, 64, 69 })
            processor.getKeyboardState().noteOn (1, note, 0.9f);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        const auto started = std::chrono::steady_clock::now();
        for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            out.insert (out.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + blockSize);
        }
        return { out, std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count() };
    }
}

TEST_CASE ("Speed-ups: renders of each heavy effect to compare before and after, and their times", "[.speed]")
{
    const std::vector<Scenario> scenarios {
        { "plain", {} },
        { "dominion", { { ParamIDs::domOn, 1.0f }, { ParamIDs::domDepth, 10.0f }, { ParamIDs::domMix, 100.0f } } },
        { "dominion-ms", { { ParamIDs::domOn, 1.0f }, { ParamIDs::domDepth, 14.0f }, { ParamIDs::domMix, 100.0f }, { ParamIDs::domStereo, 1.0f }, { ParamIDs::domMode, 1.0f } } },
        { "limiter-tp", { { ParamIDs::limitOn, 1.0f }, { ParamIDs::limitTruePeak, 1.0f }, { ParamIDs::limitGain, 12.0f }, { ParamIDs::limitCeiling, -1.0f } } },
        { "infernal", { { ParamIDs::infernalOn, 1.0f }, { ParamIDs::infernalMix, 1.0f }, { ParamIDs::infernalDrive, 0.7f }, { ParamIDs::infernalNoise, 0.5f }, { ParamIDs::infernalFeedback, 0.6f } } },
        { "infernal-nobloom", { { ParamIDs::infernalOn, 1.0f }, { ParamIDs::infernalMix, 1.0f }, { ParamIDs::infernalDrive, 0.7f }, { ParamIDs::infernalNoise, 0.5f }, { ParamIDs::infernalFeedback, 0.0f } } },
        { "spectre", { { ParamIDs::spectreOn, 1.0f }, { ParamIDs::spectreMix, 0.6f }, { ParamIDs::spectreDrift, 0.0f } } },
        { "spectre-drift", { { ParamIDs::spectreOn, 1.0f }, { ParamIDs::spectreMix, 0.6f }, { ParamIDs::spectreDrift, 0.7f } } },
    };
    const auto folder = juce::SystemStats::getEnvironmentVariable ("PARAGON_SPEED_DIR", {});
    for (const auto& scenario : scenarios)
    {
        const auto [out, seconds] = render (scenario, 10.0);
        WARN (scenario.name << ": " << seconds / 10.0 * 100.0 << "% of a core");
        if (folder.isNotEmpty())
        {
            juce::File file (folder + "/" + scenario.name + ".raw");
            file.getParentDirectory().createDirectory();
            file.replaceWithData (out.data(), out.size() * sizeof (float));
        }
    }
}
