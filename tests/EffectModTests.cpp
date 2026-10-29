// 0.25.0: the MOD MATRIX moving the effects' amounts (DELAY, REVERB, MOD, MOD 2, LO-FI). They follow the whole
// synth, so the whole synth's sources move them (macros, wheels, aftertouch, LFOs not set to VOICE), smoothly; a
// note's own source (velocity...) does nothing to them.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ModDestinations.h"
#include "Parameters.h"
#include "PluginProcessor.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double rate = 48000.0;
    constexpr int block = 256;

    struct Plugin
    {
        ParagonX1AudioProcessor processor;
        Plugin()
        {
            processor.setPlayConfigDetails (0, 2, rate, block);
            processor.prepareToPlay (rate, block);
            processor.seedRandomForTest (1);
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        void route (const char* source, const juce::String& destination, float amount, float lagMs = 0.0f)
        {
            auto m = processor.getModMatrix();
            m.routes.push_back ({ true, source, destination, amount, false, lagMs, 0.0f });
            processor.setModMatrix (m);
        }
        // Plays `seconds`; returns the loudest sample.
        float play (double seconds, juce::MidiBuffer first = {})
        {
            juce::AudioBuffer<float> buffer (2, block);
            float loudest = 0.0f;
            for (int b = 0; b < (int) (seconds * rate / block); ++b)
            {
                buffer.clear();
                processor.processBlock (buffer, first);
                first.clear();
                loudest = juce::jmax (loudest, buffer.getMagnitude (0, 0, block), buffer.getMagnitude (1, 0, block));
            }
            return loudest;
        }
        // A short note, then the level of what rings on after it (the reverb's tail).
        float tailAfterNote()
        {
            juce::MidiBuffer on, off;
            on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            play (0.3, on);
            play (0.3, off);   // the note's own release (50 ms) gone
            return play (0.5);
        }
    };
}

TEST_CASE ("Effect destinations: every one is a real parameter, listed under FX in the matrix", "[modulation][effects]")
{
    ParagonX1AudioProcessor processor;
    for (int k = 0; k < ModDestinations::numEffects; ++k)
    {
        const auto* id = ModDestinations::effectIds[k];
        INFO (id);
        CHECK (processor.getParameters().getParameter (id) != nullptr);
        const auto* d = ModDestinations::find (id);
        REQUIRE (d != nullptr);
        CHECK (d->effect == k);
        CHECK (d->group.startsWith ("FX "));
    }
    CHECK (ModDestinations::find (ParamIDs::fltCutoff)->effect == -1);
}

TEST_CASE ("A macro routed to REVERB's MIX brings the reverb in: the tail after a note follows the macro", "[modulation][effects]")
{
    const auto tailWith = [] (float macro, bool routed)
    {
        Plugin p;
        p.set (ParamIDs::ampRelease, 50.0f);
        p.set (ParamIDs::reverbOn, 1.0f);
        p.set (ParamIDs::reverbMix, 0.0f);
        p.set (ParamIDs::macros[0], macro);
        if (routed)
            p.route ("macro1", ParamIDs::reverbMix, 0.6f);
        return p.tailAfterNote();
    };
    const auto none = tailWith (1.0f, false), down = tailWith (0.0f, true), up = tailWith (1.0f, true);
    CHECK (none < 1.0e-4f);   // MIX 0: no reverb
    CHECK (down < 1.0e-4f);   // the macro at 0 moves nothing
    CHECK (up > 0.003f);      // at full: MIX 0.6, the tail rings on
}

TEST_CASE ("Effect modulation is smoothed, follows the knob under it, and lets go when the route goes", "[modulation][effects]")
{
    Plugin p;
    p.set (ParamIDs::reverbMix, 0.2f);
    p.route ("macro1", ParamIDs::reverbMix, 0.5f);
    p.play (0.05);
    CHECK_THAT (p.processor.getReverbSettings().mix, WithinAbs (0.2f, 1.0e-4f));   // the macro at 0: the knob's own
    p.set (ParamIDs::macros[0], 1.0f);
    p.play (block / rate);
    const auto after = p.processor.getReverbSettings().mix;
    CHECK (after > 0.21f);   // on its way...
    CHECK (after < 0.4f);    // ...not jumped (30 ms smoothing: about 16% of the way in one 256-sample block)
    p.play (0.3);
    CHECK_THAT (p.processor.getReverbSettings().mix, WithinAbs (0.7f, 0.005f));

    p.processor.setModMatrix ({});
    p.play (block / rate);
    CHECK_THAT (p.processor.getReverbSettings().mix, WithinAbs (0.2f, 1.0e-4f));   // no route: the knob again
}

TEST_CASE ("The whole synth's sources move the effects; a note's own don't", "[modulation][effects]")
{
    {
        // The mod wheel on DELAY's MIX, channel aftertouch on MOD's DEPTH, LO-FI's AMOUNT on MACRO 2.
        Plugin p;
        p.set (ParamIDs::delayMix, 0.1f);
        p.set (ParamIDs::modDepth, 0.2f);
        p.set (ParamIDs::lofiAmount, 2.0f);
        p.route ("modWheel", ParamIDs::delayMix, 0.4f);
        p.route ("aftertouch", ParamIDs::modDepth, 0.5f);
        p.route ("macro2", ParamIDs::lofiAmount, 0.3f);
        p.set (ParamIDs::macros[1], 1.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
        midi.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 0);
        p.play (0.02, midi);   // the first value: straight there
        p.play (0.3);
        CHECK_THAT (p.processor.getDelaySettings().mix, WithinAbs (0.5f, 0.01f));
        CHECK_THAT (p.processor.getModSettings (0).depth, WithinAbs (0.7f, 0.01f));
        CHECK_THAT (p.processor.getLoFiSettings().amount, WithinAbs (5.0f, 0.05f));   // + 30% of 0-10
    }
    {
        // Velocity can't: the effects are shared by every note. Nor can an LFO set to VOICE; a GLOBAL one can.
        Plugin p;
        p.set (ParamIDs::reverbMix, 0.2f);
        p.set (ParamIDs::delayFeedback, 0.3f);
        p.route ("velocity", ParamIDs::reverbMix, 0.8f);
        p.route ("lfo1", ParamIDs::delayFeedback, 0.5f);   // LFO 1 is VOICE by default
        juce::MidiBuffer on;
        on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
        p.play (0.3, on);
        CHECK_THAT (p.processor.getReverbSettings().mix, WithinAbs (0.2f, 1.0e-4f));
        CHECK_THAT (p.processor.getDelaySettings().feedback, WithinAbs (0.3f, 1.0e-4f));

        p.set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        p.set (ParamIDs::lfo (0, "Rate"), 2.0f);
        float low = 1.0f, high = 0.0f;
        for (int k = 0; k < 60; ++k)
        {
            p.play (0.02);
            const auto feedback = p.processor.getDelaySettings().feedback;
            low = juce::jmin (low, feedback);
            high = juce::jmax (high, feedback);
        }
        CHECK (high - low > 0.2f);   // swinging round the knob as the LFO does
    }
}

TEST_CASE ("Effect routes are kept with a preset", "[modulation][effects][rigs]")
{
    Plugin p;
    p.route ("macro2", ParamIDs::reverbMix, 0.4f);
    const auto rig = p.processor.captureRig();
    const auto back = Rig::fromXml (*rig.toXml());
    REQUIRE (back.has_value());
    REQUIRE (back->modulation.routes.size() == 1);
    CHECK (back->modulation.routes[0].destination == ParamIDs::reverbMix);
}
