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
                                                 "lofiMix", "lofiAmount" };
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
