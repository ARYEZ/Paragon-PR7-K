#pragma once

#include "Parameters.h"

#include <memory>
#include "Rig.h"
#include "dsp/synth/SynthSettings.h"

// What the factory presets and the expansions are built with: a preset under construction, where anything it
// doesn't set stays at its default.
namespace rigbuilder
{
    struct Builder
    {
        // The preset lives on the heap (0.78.0): a Rig is several KB, and the factory and expansion functions hold
        // dozens of builders at once. On the stack they took makeFactoryRigs to 418 KB of a Windows thread's 1 MB, and a
        // deep test crashed there (Linux has 8 MB). Copies copy the preset.
        std::unique_ptr<Rig> storage = std::make_unique<Rig>();
        Rig& rig = *storage;

        Builder (const Builder& other) : storage (std::make_unique<Rig> (*other.storage)), rig (*storage) {}
        Builder& operator= (const Builder& other) { *storage = *other.storage; return *this; }

        Builder (const juce::String& name, int number, const juce::StringArray& tags, const juce::String& notes)
        {
            rig.name = name;
            rig.bank = 0;   // an expansion sets its own bank
            rig.number = number;
            rig.tags = tags;
            rig.notes = notes;
        }

        Builder& set (const juce::String& id, float value) { rig.parameters[id] = value; return *this; }
        Builder& osc (int index, const char* what, float value) { return set (ParamIDs::osc (index, what), value); }

        Builder& filter (synth::FilterType type, float cutoffHz, float resonance, float envelope)
        {
            set (ParamIDs::fltOn, 1.0f);
            set (ParamIDs::fltType, (float) type);
            set (ParamIDs::fltCutoff, cutoffHz);
            set (ParamIDs::fltRes, resonance);
            return set (ParamIDs::fltEnv, envelope);
        }

        Builder& filterEnvelope (float attack, float decay, float sustain, float release)
        {
            set (ParamIDs::fltAttack, attack);
            set (ParamIDs::fltDecay, decay);
            set (ParamIDs::fltSustain, sustain);
            return set (ParamIDs::fltRelease, release);
        }

        Builder& ampEnvelope (float attack, float decay, float sustain, float release)
        {
            set (ParamIDs::ampAttack, attack);
            set (ParamIDs::ampDecay, decay);
            set (ParamIDs::ampSustain, sustain);
            return set (ParamIDs::ampRelease, release);
        }

        Builder& shaper (int k, synth::ShaperModel model, float drive, bool afterFilter = false)
        {
            set (ParamIDs::shp (k, "On"), 1.0f);
            set (ParamIDs::shp (k, "Model"), (float) model);
            set (ParamIDs::shp (k, "Drive"), drive);
            return set (ParamIDs::shp (k, "Place"), afterFilter ? 1.0f : 0.0f);
        }

        Builder& mono (float glideMs)
        {
            set (ParamIDs::playMode, (float) synth::PlayMode::mono);
            set (ParamIDs::legato, 1.0f);
            set (ParamIDs::glideMode, (float) synth::GlideMode::legato);
            return set (ParamIDs::glide, glideMs);
        }
    };

    constexpr auto saw = (float) synth::OscWave::saw, pulse = (float) synth::OscWave::pulse,
                   sine = (float) synth::OscWave::sine;
}
