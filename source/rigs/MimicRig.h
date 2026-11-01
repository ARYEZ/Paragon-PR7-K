#pragma once

#include "Rig.h"
#include "dsp/synth/Mimic.h"

// MIMIC (0.44.0): a sound made from an analysis of a recording (dsp/synth/Mimic.h). A clean new sound, set from what
// was measured: oscillator 1 plays the recording's own cycles as a wavetable, walked by TRAVEL from the start of the
// note to where it settles; the AMP follows its loudness; vibrato, a swoop into the note (the PITCH envelope), unison
// and noise (oscillator 2) as heard. The player chooses which of these to take.
namespace mimic
{
    struct Choices
    {
        bool tone = true;       // the wavetable (off: a plain saw, to keep only the movement)
        bool envelope = true;   // the loudness
        bool vibrato = true;
        bool swoop = true;
        bool unison = true;
        bool noise = true;
    };

    Rig makeRig (const synth::mimic::Analysis&, const Choices& = {});
}
