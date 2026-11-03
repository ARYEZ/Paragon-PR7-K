#pragma once

#include <juce_core/juce_core.h>

#include "dsp/synth/Modulation.h"

#include <string_view>
#include <vector>

// What the MOD MATRIX can move (stage 4a): every knob the engine follows
// live while a note plays, by its parameter ID, with the setting it moves
// and how the menu names it. SKEW, PINCH and the shapers' EQ joined in 0.14.0
// (a modulated note works out its own curve and EQ). Not: the FFT editor's
// SWEEP, FLIP and VSTRENGTH (baked into tables built in the background), the
// LFOs', envelopes' and step sequencer's timing (they run once for all
// notes, or are worked out before the routes), and anything chosen from a
// menu or switched.
namespace ModDestinations
{
    struct Destination
    {
        juce::String parameterId;
        juce::String group;     // "Osc 1", "Filter", "Shaper 2", "Amp"
        juce::String name;      // "Volume", "Cutoff"...
        synth::mod::Target target;
        int effect = -1;        // 0.25.0: an effect's amount (its index in `effectIds`), not a note's setting
    };

    // 0.25.0: the effects' amounts the matrix can move (DELAY, REVERB, MOD, MOD 2, LO-FI). They follow the whole
    // synth, not a note, so only the whole synth's sources move them (the macros and buttons, the mod wheel and pitch
    // bend, channel aftertouch, breath, expression, the sustain pedal, and LFOs and envelopes not set to VOICE; see
    // Engine::wholeSynthValue). A route to one from a note's own source (velocity, keytrack...) does nothing.
    inline constexpr const char* effectIds[] = { "delayMix", "delayFeedback", "delayTone", "delayToReverb",
                                                 "reverbMix", "reverbSize", "reverbTone", "reverbPreDelay",
                                                 "modMix", "modDepth", "modRate", "mod2Mix", "mod2Depth", "mod2Rate",
                                                 "lofiMix", "lofiAmount",
                                                 // 0.51.0: each insert bus's WAH pedal (the EXPR wheel's job)
                                                 "ins1WahPosition", "ins2WahPosition", "ins3WahPosition", "ins4WahPosition",
                                                 // 0.53.0: ECHOES: how loud the past comes back, its echoes of echoes, how worn
                                                 "echoMix", "echoFeedback", "echoAge",
                                                 // 0.56.0: CRACKLE: how many clicks, how bunched, how loud
                                                 "crackleRate", "crackleCluster", "crackleLevel",
                                                 // 0.57.0: DOWNPOUR: how hard it rains, how many drops, how far, how loud
                                                 "downpourRain", "downpourDrops", "downpourDistance", "downpourLevel",
                                                 // 0.58.0: SIROCCO: how loud the answers (layer by layer), how detuned, how spread
                                                 "siroccoLevel", "siroccoDetune", "siroccoSpread",
                                                 // 0.59.0: INFERNAL: how hard, how much, the noise's growl, the bloom
                                                 "infernalDrive", "infernalMix", "infernalNoise", "infernalFeedback",
                                                 // 0.60.0: AFTERGLOW: each note's character and wander, the ensemble, the tape
                                                 "afterglowAge", "afterglowDrift", "afterglowEnsemble", "afterglowTape",
                                                 // 0.61.0: LIGHTTRAIL: how long, how far each repeat steps, its shine, how much
                                                 "trailLength", "trailRise", "trailTone", "trailMix",
                                                 // 0.62.0: BLACKHOLE: how long the haze hangs, how it sinks, its shade, how much
                                                 "blackholeHold", "blackholePull", "blackholeTone", "blackholeMix",
                                                 // 0.63.0: DILATION: how slow, how dark, how much
                                                 "dilationSlow", "dilationTone", "dilationMix",
                                                 // 0.64.0: REMNANT: how loud the layers, how dark, how much quieter each deeper one
                                                 "remnantMix", "remnantTone", "remnantTaper",
                                                 // 0.65.0: PROXIMITY: the touches, the room, how close
                                                 "proximityNoise", "proximityRoom", "proximityClose",
                                                 // 0.66.0: HAZE: how much, the pitch's bend, the wind
                                                 "hazeDepth", "hazeDoppler", "hazeWhoosh",
                                                 // 0.67.0: SPECTRE: how loud the ghost, how long it lingers, its outline moved
                                                 "spectreMix", "spectreLinger", "spectreShift",
                                                 // 0.68.0: LEVIATHAN: how loud the voice, how rough, how fast its vowel wanders
                                                 "leviathanMix", "leviathanGrowl", "leviathanDrift",
                                                 // 0.69.0: NUCLEARSPRING: how much tank, how drippy, how hard a hit kicks it
                                                 "springMix", "springDrip", "springBoing" };
    inline constexpr int numEffects = (int) std::size (effectIds);
    // A source only a note has (it can't move an effect). LFOs and envelopes set to VOICE are a note's too, but that's
    // a setting, checked as they play.
    inline bool isNoteOnly (synth::mod::Source s) noexcept
    {
        using S = synth::mod::Source;
        return s == S::velocity || s == S::keytrack || s == S::random || s == S::pitchEnvelope || s == S::stepSeq;
    }
    consteval int effect (std::string_view id)
    {
        for (int i = 0; i < numEffects; ++i)
            if (std::string_view (effectIds[i]) == id)
                return i;
        throw "not an effect destination";
    }

    const std::vector<Destination>& all();
    const Destination* find (const juce::String& parameterId);
}
