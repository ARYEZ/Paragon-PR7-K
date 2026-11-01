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
            if (a.table.numFrames() >= 2 && a.travelSeconds > 0.08)
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
