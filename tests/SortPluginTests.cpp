// SORT in PR7-K (0.93.0): off and at AMOUNT 0 change nothing; a falling phrase played on the synth comes back a bar
// later rising (filed by the synth's own notes); AMOUNT, EDGE and STRIKE are mod-matrix destinations (FX Sort), MACRO 3
// re-files and lets go, untouched again; the SIGNATURE slot's thirty-third effect and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "ui/RackSpecs.h"
#include "ui/SortPage.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    constexpr int perSlice = 12000;   // an eighth at 120 BPM

    double hzOf (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

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
        // Plays `notes` an eighth each (each held for most of its eighth), round and round, for `seconds`.
        std::vector<float> play (const std::vector<int>& notes, double seconds, double macroDownAt = 1.0e9)
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            int playing = -1;
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                const auto at = b * blockSize;
                if ((double) at >= macroDownAt * sampleRate)
                    set (ParamIDs::macros[2], 0.0f);
                const auto slice = at / perSlice;
                const auto into = at - slice * perSlice;
                if (into < blockSize && playing < 0)
                {
                    playing = notes[(size_t) slice % notes.size()];
                    processor.getKeyboardState().noteOn (1, playing, 0.9f);
                }
                else if (into >= perSlice * 3 / 4 && playing >= 0)
                {
                    processor.getKeyboardState().noteOff (1, playing, 0.0f);
                    playing = -1;
                }
                buffer.clear();
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
    };

    double toneIn (const std::vector<float>& x, size_t from, size_t to, double hz)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / sampleRate;
        double s1 = 0.0, s2 = 0.0;
        for (auto i = from; i < to; ++i)
        {
            const auto s0 = (double) x[i] + 2.0 * std::cos (w) * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return std::sqrt (s1 * s1 + s2 * s2 - 2.0 * std::cos (w) * s1 * s2);
    }

    int noteAt (const std::vector<float>& x, int slice, const std::vector<int>& candidates)
    {
        const auto from = (size_t) (slice * perSlice + 1500), to = from + 6000;
        auto best = -1;
        auto bestLevel = 0.0;
        for (auto c : candidates)
            if (const auto level = toneIn (x, from, to, hzOf (c)); level > bestLevel)
            {
                bestLevel = level;
                best = c;
            }
        return best;
    }

    const std::vector<int> falling { 72, 71, 69, 67, 65, 64, 62, 60 };
}

TEST_CASE ("SORT in the plugin: off and AMOUNT 0 change nothing; a falling phrase comes back rising", "[sort][plugin]")
{
    Plugin plain;
    const auto reference = plain.play (falling, 4.0);
    // The plain synth plays the phrase as given.
    for (int k = 0; k < 8; ++k)
        CHECK (noteAt (reference, k, falling) == falling[(size_t) k]);

    Plugin resting;
    resting.set (ParamIDs::sortOn, 1.0f);
    resting.set (ParamIDs::sortStrike, 1.0f);
    CHECK (resting.play (falling, 4.0) == reference);

    Plugin sorted;
    sorted.set (ParamIDs::sortOn, 1.0f);
    sorted.set (ParamIDs::sortAmount, 1.0f);
    sorted.set (ParamIDs::sortStrike, 0.0f);
    const auto out = sorted.play (falling, 4.0);
    // The second bar: the first bar's notes, lowest first.
    for (int k = 0; k < 8; ++k)
    {
        INFO ("place " << k);
        CHECK (noteAt (out, 8 + k, falling) == falling[(size_t) (7 - k)]);
    }
    CHECK (sorted.processor.getSort().getWindowsFiled() >= 1);
}

TEST_CASE ("SORT in the plugin: MACRO 3 re-files through the matrix and lets go, untouched again", "[sort][plugin]")
{
    for (const auto* id : { ParamIDs::sortAmount, ParamIDs::sortEdge, ParamIDs::sortStrike })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Sort");
    }

    Plugin plain;
    const auto reference = plain.play (falling, 7.0);

    Plugin p;
    p.set (ParamIDs::sortOn, 1.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::sortAmount, 1.0f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
    p.set (ParamIDs::macros[2], 1.0f);
    const auto out = p.play (falling, 7.0, 4.2);
    double moved = 0.0;
    for (auto i = (size_t) (2.0 * sampleRate); i < (size_t) (4.0 * sampleRate); ++i)
        moved += std::abs (out[i] - reference[i]);
    CHECK (moved > 1.0);
    CHECK (p.processor.getSort().isResting());
    const auto late = (size_t) (5.0 * sampleRate);
    CHECK (std::equal (out.begin() + (long) late, out.end(), reference.begin() + (long) late));
}

TEST_CASE ("SORT: the SIGNATURE slot's thirty-third effect, with its page", "[sort][ui]")
{
    CHECK (signatureNames()[32] == "SORT");
    ParagonX1AudioProcessor processor;
    SortPage page (processor);
    page.setSize (1200, 600);
    CHECK (page.getShownControls().size() == 8);
    for (auto* c : page.getShownControls())
        CHECK (! c->getBounds().isEmpty());
}
