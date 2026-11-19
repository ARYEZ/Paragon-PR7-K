// TRANCE GATE (1.7.0): the stepped gate in time with the song at the very end of the effects. Off and MIX 0 untouched bit
// for bit; each step opens and closes on its own time at any tempo and while stopped; MIX, the step levels, GATE,
// SWING, LENGTH and the left / right patterns do what they say; the edges are click-free; turned down it rests
// untouched; in the plugin it chops the reverb's tail too; its matrix destinations, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/TranceGate.h"
#include "ui/RackSpecs.h"
#include "ui/TranceGatePage.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    using TG = effects::TranceGate;

    TG::Settings settingsWith (std::initializer_list<float> left, std::initializer_list<float> right = {})
    {
        TG::Settings s;
        s.on = true;
        s.mix = 1.0f;
        s.attack = 0.0f;    // as sharp as it goes, so a step's level reads straight off
        s.release = 0.0f;
        s.gate = 1.0f;
        int k = 0;
        for (const auto v : left)
            s.left[(size_t) k++] = v;
        s.length = juce::jmax (1, k);
        k = 0;
        for (const auto v : right)
            s.right[(size_t) k++] = v;
        return s;
    }

    // The gate's gain over time: a constant 1 through it, both sides (rendered in blocks; ppq < 0: its own count).
    std::array<std::vector<float>, 2> gains (TG& gate, const TG::Settings& s, double seconds, double bpm, double ppqStart)
    {
        std::array<std::vector<float>, 2> out;
        const auto total = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int start = 0; start < total; start += blockSize)
        {
            const auto n = juce::jmin (blockSize, total - start);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    buffer.setSample (c, i, 1.0f);
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) n);
            const auto ppq = ppqStart < 0.0 ? -1.0 : ppqStart + bpm / 60.0 * start / sampleRate;
            gate.process (block, s, bpm, ppq);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    out[(size_t) c].push_back (buffer.getSample (c, i));
        }
        return out;
    }

    float at (const std::vector<float>& x, double seconds) { return x[(size_t) juce::jlimit (0.0, (double) x.size() - 1, seconds * sampleRate)]; }
}

TEST_CASE ("TRANCE GATE off, or at MIX 0, leaves the sound untouched bit for bit", "[trancegate]")
{
    TG gate;
    gate.prepare (sampleRate, blockSize);
    juce::Random random (3);
    for (const auto off : { true, false })
    {
        auto s = settingsWith ({ 1, 0, 1, 0 });
        s.on = ! off;
        s.mix = off ? 1.0f : 0.0f;
        juce::AudioBuffer<float> buffer (2, blockSize), copy (2, blockSize);
        for (int b = 0; b < 40; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (c, i, random.nextFloat() - 0.5f);
            copy.makeCopyOf (buffer);
            juce::dsp::AudioBlock<float> block (buffer);
            gate.process (block, s, 120.0, b * 0.1);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    REQUIRE (buffer.getSample (c, i) == copy.getSample (c, i));
        }
        CHECK (gate.isResting());
    }
}

TEST_CASE ("TRANCE GATE: each step opens and closes on its own time, at any tempo and while stopped", "[trancegate]")
{
    // A pattern open, shut, open, open: at 1/16 each step is a sixteenth of the song.
    const auto s = settingsWith ({ 1, 0, 1, 1 });
    for (const auto bpm : { 120.0, 90.0, 140.0 })
    {
        INFO ("BPM " << bpm);
        TG gate;
        gate.prepare (sampleRate, blockSize);
        const auto step = 60.0 / bpm / 4.0;
        const auto g = gains (gate, s, 3.0, bpm, 0.0);
        for (int k = 4; k < 16; ++k)   // after MIX's first 30 ms
        {
            const auto expected = (k % 4) == 1 ? 0.0f : 1.0f;
            CHECK (std::abs (at (g[0], (k + 0.7) * step) - expected) < 0.02f);
            CHECK (std::abs (at (g[1], (k + 0.7) * step) - expected) < 0.02f);
        }
    }
    // While the song is stopped it keeps its own count at the tempo.
    TG gate;
    gate.prepare (sampleRate, blockSize);
    const auto g = gains (gate, s, 3.0, 120.0, -1.0);
    const auto step = 0.125;
    for (int k = 4; k < 16; ++k)
        CHECK (std::abs (at (g[0], (k + 0.7) * step) - ((k % 4) == 1 ? 0.0f : 1.0f)) < 0.02f);
}

TEST_CASE ("TRANCE GATE: MIX sets how deep it cuts, a step's level how much it lets through", "[trancegate]")
{
    auto s = settingsWith ({ 1, 0, 0.5f, 0.25f });
    s.mix = 0.6f;
    TG gate;
    gate.prepare (sampleRate, blockSize);
    const auto g = gains (gate, s, 2.0, 120.0, 0.0);
    const auto step = 0.125;
    // gain = 1 - MIX + MIX x level.
    CHECK (std::abs (at (g[0], 8.7 * step) - 1.0f) < 0.02f);
    CHECK (std::abs (at (g[0], 9.7 * step) - 0.4f) < 0.02f);
    CHECK (std::abs (at (g[0], 10.7 * step) - (0.4f + 0.6f * 0.5f)) < 0.02f);
    CHECK (std::abs (at (g[0], 11.7 * step) - (0.4f + 0.6f * 0.25f)) < 0.02f);
}

TEST_CASE ("TRANCE GATE: GATE shortens each open step; LENGTH sets when the pattern comes round", "[trancegate]")
{
    auto s = settingsWith ({ 1, 1, 1, 1, 0, 0, 0, 0 });
    s.gate = 0.25f;
    s.length = 3;   // only the first three steps: every step open
    TG gate;
    gate.prepare (sampleRate, blockSize);
    const auto g = gains (gate, s, 2.0, 120.0, 0.0);
    const auto step = 0.125;
    for (int k = 4; k < 12; ++k)
    {
        CHECK (at (g[0], (k + 0.15) * step) > 0.98f);   // open for its first quarter
        CHECK (at (g[0], (k + 0.8) * step) < 0.02f);    // shut for the rest
    }
}

TEST_CASE ("TRANCE GATE: SWING starts every second step up to half a step late", "[trancegate]")
{
    auto s = settingsWith ({ 0, 1 });
    s.swing = 1.0f;   // the second step of each pair starts at 1.5 steps
    TG gate;
    gate.prepare (sampleRate, blockSize);
    const auto g = gains (gate, s, 2.0, 120.0, 0.0);
    const auto step = 0.125;
    for (int pair = 2; pair < 6; ++pair)
    {
        const auto start = pair * 2.0 * step;
        CHECK (at (g[0], start + 1.3 * step) < 0.02f);   // still the first step (late by swing)
        CHECK (at (g[0], start + 1.7 * step) > 0.98f);   // the second, open
    }
}

TEST_CASE ("TRANCE GATE: STEREO gives the right side its own pattern; LINKED both the left's", "[trancegate]")
{
    auto s = settingsWith ({ 1, 0 }, { 0, 1 });
    const auto step = 0.125;
    for (const auto stereo : { 0, 1 })
    {
        s.stereo = stereo;
        TG gate;
        gate.prepare (sampleRate, blockSize);
        const auto g = gains (gate, s, 2.0, 120.0, 0.0);
        for (int k = 4; k < 12; ++k)
        {
            const auto leftOpen = (k % 2) == 0;
            CHECK (std::abs (at (g[0], (k + 0.7) * step) - (leftOpen ? 1.0f : 0.0f)) < 0.02f);
            const auto rightOpen = stereo == 1 ? ! leftOpen : leftOpen;
            CHECK (std::abs (at (g[1], (k + 0.7) * step) - (rightOpen ? 1.0f : 0.0f)) < 0.02f);
        }
    }
}

TEST_CASE ("TRANCE GATE: ATTACK and RELEASE keep the edges click-free and shape them", "[trancegate]")
{
    auto s = settingsWith ({ 1, 0 });
    s.attack = 0.15f;    // about 1.9 ms
    s.release = 0.3f;    // about 5.5 ms
    TG gate;
    gate.prepare (sampleRate, blockSize);
    const auto g = gains (gate, s, 2.0, 120.0, 0.0);
    auto biggest = 0.0f;
    for (size_t i = (size_t) (0.1 * sampleRate); i + 1 < g[0].size(); ++i)
        biggest = juce::jmax (biggest, std::abs (g[0][i + 1] - g[0][i]));
    CHECK (biggest < 0.03f);   // no jump of more than 3% in a sample
    // A slow RELEASE leaves sound well into the closed step.
    s.release = 1.0f;   // 300 ms
    TG slow;
    slow.prepare (sampleRate, blockSize);
    const auto h = gains (slow, s, 2.0, 120.0, 0.0);
    CHECK (at (h[0], 9.9 * 0.125) > 0.6f);
    CHECK (at (g[0], 9.9 * 0.125) < 0.01f);
}

TEST_CASE ("TRANCE GATE turned down rests untouched", "[trancegate]")
{
    auto s = settingsWith ({ 1, 0, 1, 0 });
    TG gate;
    gate.prepare (sampleRate, blockSize);
    gains (gate, s, 1.0, 120.0, 0.0);
    CHECK (! gate.isResting());
    s.mix = 0.0f;
    gains (gate, s, 0.5, 120.0, 1.0);
    CHECK (gate.isResting());
    const auto g = gains (gate, s, 0.5, 120.0, 2.0);
    for (const auto v : g[0])
        REQUIRE (v == 1.0f);
}

TEST_CASE ("TRANCE GATE in the plugin: chops the sound and the reverb's tail; matrix destinations, the SIGNATURE slot and its page", "[trancegate][plugin]")
{
    for (const auto* id : { ParamIDs::tgMix, ParamIDs::tgGate })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Trance Gate");
    }
    CHECK (signatureNames()[38] == "TRANCE GATE");
    CHECK (signatureNames().size() == 39);

    // The share of 5 ms windows that are near silent (60 dB under the loudest) over a stretch.
    const auto shareShut = [] (const std::vector<float>& x)
    {
        const int window = (int) (0.005 * sampleRate);
        std::vector<float> rms;
        for (size_t i = 0; i + (size_t) window <= x.size(); i += (size_t) window)
        {
            double sum = 0.0;
            for (int k = 0; k < window; ++k)
                sum += (double) x[i + (size_t) k] * x[i + (size_t) k];
            rms.push_back ((float) std::sqrt (sum / window));
        }
        const auto loudest = *std::max_element (rms.begin(), rms.end());
        int shut = 0;
        for (const auto r : rms)
            shut += r < loudest * 0.001f ? 1 : 0;
        return (float) shut / (float) rms.size();
    };
    const auto render = [&] (bool gateOn, bool tailOnly)
    {
        ParagonX1AudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
        processor.seedRandomForTest (1);
        const auto set = [&processor] (const char* id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set (ParamIDs::reverbOn, 1.0f);
        set (ParamIDs::reverbMix, 0.6f);
        set (ParamIDs::reverbSize, 0.9f);
        set (ParamIDs::tgOn, gateOn ? 1.0f : 0.0f);
        set (ParamIDs::tgGate, 0.5f);
        set (ParamIDs::tgAttack, 0.1f);
        set (ParamIDs::tgRelease, 0.0f);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        processor.getKeyboardState().noteOn (1, 57, 0.9f);
        for (int b = 0; b < (int) (3.0 * sampleRate / blockSize); ++b)
        {
            if (b == (int) (0.5 * sampleRate / blockSize))
                processor.getKeyboardState().noteOff (1, 57, 0.0f);   // let go: from here on, the reverb's tail
            buffer.clear();
            processor.processBlock (buffer, midi);
            const auto t = (double) b * blockSize / sampleRate;
            if (! tailOnly || t > 1.2)
                for (int i = 0; i < blockSize; ++i)
                    out.push_back (buffer.getSample (0, i));
        }
        return out;
    };
    const auto tailOpen = shareShut (render (false, true));
    const auto tailGated = shareShut (render (true, true));
    INFO ("near-silent share of the reverb's tail: " << tailOpen << " without, " << tailGated << " with the gate");
    CHECK (tailOpen < 0.05f);     // the tail rings on smoothly
    CHECK (tailGated > 0.4f);     // the gate chops it: CLASSIC's shut steps and each open step's second half

    ParagonX1AudioProcessor processor;
    TranceGatePage page (processor);
    page.setSize (1200, 600);
    CHECK (page.getShownControls().size() == 9);
    page.loadPattern (7);   // PING-PONG: sets STEREO, the right row the left's opposite
    const auto value = [&processor] (const juce::String& id)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        return parameter->convertFrom0to1 (parameter->getValue());
    };
    CHECK (value (ParamIDs::tgStereo) == 1.0f);
    CHECK (value (ParamIDs::tgStep (0, 0)) == 1.0f);
    CHECK (value (ParamIDs::tgStep (1, 0)) == 0.0f);
    CHECK (value (ParamIDs::tgStep (1, 17)) == 1.0f);
    // A click on the editor's first step closes it; a second opens it again.
    auto& editor = page.getEditor();
    CHECK (editor.rows() == 2);
    const auto cell = editor.cellBounds (0, 0).getCentre();
    CHECK (editor.cellAt (cell) == std::pair<int, int> (0, 0));
    const auto click = [&editor, cell]
    {
        const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), cell, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                  &editor, &editor, juce::Time::getCurrentTime(), cell, juce::Time::getCurrentTime(), 1, false);
        editor.mouseDown (e);
        editor.mouseUp (e);
    };
    click();
    CHECK (value (ParamIDs::tgStep (0, 0)) == 0.0f);
    click();
    CHECK (value (ParamIDs::tgStep (0, 0)) == 1.0f);
}
