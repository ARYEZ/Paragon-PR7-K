#include "MimicRig.h"

#include "Parameters.h"
#include "RigBuilder.h"
#include "dsp/effects/Reverb.h"

using namespace rigbuilder;

namespace mimic
{
    Rig makeRig (const synth::mimic::Analysis& a, const Choices& choices)
    {
        const auto note = synth::mimic::noteName (a.rootNote);
        juce::StringArray made;
        Builder b ("Mimic " + a.name, 1, { "Mimic" }, {});
        b.osc (0, "PhaseRand", 0.0f).osc (0, "Fine", (float) juce::roundToInt (a.cents));

        if (choices.tone && a.table.isValid())
        {
            b.rig.wavetables[0] = a.table;
            b.osc (0, "Wave", (float) synth::OscWave::wavetable).osc (0, "WtIndex", 0.0f);
            if (a.movementSeconds > 0.0 && a.table.numFrames() >= 2)
            {
                // A SLOW MOVEMENT (1.8.0): MOD ENVELOPE 1 moves INDEX through the attack's frames over the time they
                // took, then round the movement's frames once a round, looping as long as the note sounds (and on
                // through its release). The round's last frame is its first, so the loop doesn't jump.
                const auto last = (float) (a.table.numFrames() - 1);
                const auto roundStart = (float) a.attackFrames / last;
                const auto attack = (float) juce::jmax (0.0, a.travelSeconds);
                ModShapeData shape;
                shape.points.push_back ({ 0.0f, 0.0f, 0.0f });
                if (a.attackFrames > 0)
                    shape.points.push_back ({ attack, roundStart, 0.0f });
                if (choices.movement)
                {
                    shape.points.push_back ({ attack + (float) a.movementSeconds, 1.0f, 0.0f });
                    shape.loopEnd = (int) shape.points.size() - 1;
                    shape.loopStart = shape.loopEnd - 1;
                }
                b.rig.modulation.envelopes[0] = shape;
                b.set (ParamIDs::env (0, "Mode"), (float) (choices.movement ? synth::mod::EnvelopeMode::loop : synth::mod::EnvelopeMode::oneShot))
                 .set (ParamIDs::env (0, "Sync"), 0.0f).set (ParamIDs::env (0, "Speed"), 1.0f);
                b.rig.modulation.routes.push_back ({ true, "env1", ParamIDs::osc (0, "WtIndex"), 1.0f, false, 0.0f, 0.0f });
                if (choices.movement)
                    made.add ("its own waves and their slow movement (a round every " + juce::String (a.movementSeconds, 2) + " s, looped)");
                else
                    made.add ("its own waves (" + juce::String (a.attackFrames + 1) + " frames)");
            }
            else if (a.table.numFrames() >= 2 && a.travelSeconds > 0.08)
            {
                // TRAVEL walks the frames once, from the first (the note's start) to the last (where it settles),
                // over the time they took in the recording, and stays there.
                b.osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::oneShot)
                 .osc (0, "TrSync", (float) synth::mod::LfoSync::free)
                 .osc (0, "TrRate", (float) juce::jlimit (0.02, 40.0, 1.0 / a.travelSeconds))
                 .osc (0, "TrStrength", 1.0f).osc (0, "TrSmooth", 0.0f);
                made.add ("its own waves (" + juce::String (a.table.numFrames()) + " frames over "
                          + juce::String (a.travelSeconds, 2) + " s)");
            }
            else
                made.add ("its own wave");
        }

        if (choices.envelope)
        {
            b.ampEnvelope (a.attackMs, a.decayMs, a.sustain, a.releaseMs)
             .set (ParamIDs::ampDecayCurve, -1.0f).set (ParamIDs::ampReleaseCurve, -1.0f);
            made.add (a.sustained ? "its loudness" : "its loudness (dying away)");
        }

        if (choices.vibrato && a.vibratoCents > 0.0f)
        {
            b.osc (0, "VibRate", a.vibratoHz).osc (0, "VibAmount", juce::jmin (100.0f, a.vibratoCents))
             .osc (0, "VibFade", juce::jmin (10000.0f, a.vibratoDelayMs));
            made.add ("vibrato");
        }

        if (choices.swoop && ! a.swoop.empty())
        {
            float most = 0.0f;
            for (const auto s : a.swoop)
                most = juce::jmax (most, std::abs (s));
            const auto range = juce::jlimit (1.0f, 24.0f, std::ceil (most));
            b.set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, range)
             .set (ParamIDs::pitchEnvMode, (float) synth::mod::EnvelopeMode::oneShot);
            ModShapeData shape;
            const auto step = juce::jmax (1, (int) a.swoop.size() / 48);   // at most 64 points
            for (size_t i = 0; i < a.swoop.size(); i += (size_t) step)
                shape.points.push_back ({ (float) ((double) i * synth::mimic::Analysis::hopSeconds),
                                          juce::jlimit (0.0f, 1.0f, 0.5f + a.swoop[i] / (2.0f * range)), 0.0f });
            shape.points.push_back ({ (float) ((double) a.swoop.size() * synth::mimic::Analysis::hopSeconds), 0.5f, 0.0f });
            b.rig.modulation.pitchEnvelope = shape;
            made.add ("the swoop into the note");
        }

        if (choices.unison && a.detuneCents > 0.0f)
        {
            b.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.8f)
             .osc (0, "UniDetune", std::sqrt (juce::jlimit (0.0f, 100.0f, a.detuneCents) / 100.0f))
             .osc (0, "UniSpread", juce::jlimit (0.2f, 1.0f, a.width));
            made.add ("unison");
        }

        // A WIDE sound (1.8.0): one in which each side carries its own version of the tone (unison copies spread
        // hard, the middle nearly empty: side as loud as mid or louder) can't be made by one oscillator's unison,
        // whose first copy always sits in the middle and ties the sides together. So the tone is played twice:
        // oscillator 1 to the left and a copy on oscillator 3 to the right, each with its own unison and starting
        // phase, so the sides go their own ways as the recording's did. How far apart: for two unrelated sources
        // panned +-p with PR7-K's balance (the far side at 1 - p), the sides' likeness is 2 (1 - p) / ((1 - p)^2 + 1);
        // the width measured (side against mid) gives the likeness wanted, (1 - w^2) / (1 + w^2).
        if (choices.unison && a.detuneCents > 0.0f && a.width >= wideFrom && choices.tone && a.table.isValid())
        {
            const auto w = juce::jmin (1.5f, a.width);
            const auto likeness = (1.0f - w * w) / (1.0f + w * w);
            const auto q = likeness <= 0.0f ? 0.0f : (1.0f - std::sqrt (1.0f - likeness * likeness)) / likeness;
            const auto pan = juce::jlimit (0.0f, 1.0f, 1.0f - q);
            const auto from = ParamIDs::osc (0, ""), to = ParamIDs::osc (2, "");
            std::vector<std::pair<juce::String, float>> copies;
            for (const auto& [id, value] : b.rig.parameters)
                if (id.startsWith (from) && ! juce::CharacterFunctions::isDigit (id[from.length()]))
                    copies.push_back ({ to + id.substring (from.length()), value });
            for (const auto& [id, value] : copies)
                b.rig.parameters[id] = value;
            b.rig.wavetables[2] = b.rig.wavetables[0];
            const auto routes = b.rig.modulation.routes;
            for (auto r : routes)
                if (r.destination.startsWith (from) && ! juce::CharacterFunctions::isDigit (r.destination[from.length()]))
                {
                    r.destination = to + r.destination.substring (from.length());
                    b.rig.modulation.routes.push_back (r);
                }
            // ON too: oscillator 1 is on by default and so isn't among the settings copied; oscillator 3 isn't.
            b.osc (2, "On", 1.0f).osc (0, "Pan", -pan).osc (2, "Pan", pan).osc (0, "PhaseRand", 1.0f).osc (2, "PhaseRand", 1.0f)
             .osc (0, "UniSpread", 0.0f).osc (2, "UniSpread", 0.0f);
            made.add ("its width (the tone on each side, " + juce::String (juce::roundToInt (pan * 100.0f)) + "% apart)");
        }

        if (choices.noise && a.noise > 0.0f)
        {
            // Oscillator 2 plays only noise, its level against the tone as measured (a guess at how loud our noise
            // is against a wavetable: MIMIC's second part will measure it).
            const auto against = std::sqrt (a.noise / juce::jmax (0.05f, 1.0f - a.noise));
            b.osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", a.noiseColour)
             .osc (1, "NoiseWidth", 2.0f).osc (1, "Volume", juce::jlimit (0.0f, 2.0f, against));
            made.add ("noise");
        }

        // MACRO 2 SPACE, as on the factory sounds: a hall that comes in as it's turned up.
        b.set (ParamIDs::reverbOn, 1.0f).set (ParamIDs::reverbType, (float) (int) effects::Reverb::Type::hall)
         .set (ParamIDs::reverbSize, 0.6f).set (ParamIDs::reverbMix, 0.0f);
        b.rig.modulation.routes.push_back ({ true, "macro2", ParamIDs::reverbMix, 0.3f, false, 0.0f, 0.0f });
        b.rig.modulation.macroNames[1] = "SPACE";

        b.rig.notes = "Made by MIMIC from \"" + a.name + "\". Play it at " + note
                    + " (the note it was recorded at: further away, its tone moves with the keys). Taken: "
                    + (made.isEmpty() ? juce::String ("nothing") : made.joinIntoString (", ")) + ".";
        return b.rig;
    }
}
