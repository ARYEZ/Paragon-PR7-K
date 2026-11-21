#pragma once

#include "Rig.h"

#include <vector>

// THE DISTINCT PASS (1.8.0, the owner's ask: "presets that sound basically identical between some different rigs").
// An audit rendered every expansion preset on one probe and found 57 that measured within 3 (dB of tone, moment by
// moment, plus half the dB of loudness) of a preset in another bank, most built from the same shared recipe (the felt
// and soft pianos) or the same library recording with nearly the same settings. Each was changed to be its own: the
// changes are a table of lines, applied after the banks are built (`apply`), one change a line:
//
//     <preset name> | <version> | <kind> | <what>
//
//   version: synth (the preset as built), lib (its version on a library, `Rig::sampled`), both
//   kind:    p   a setting:            id=value
//            r   a route added:        source>destination=amount
//            x   routes taken out:     source>destination (every route from that source to that destination)
//            s   a library file:       oscillator=file  (oscillator 0-7; the file inside the same library's folder, or a whole
//                                  reference "{sfz:<library folder>}/<file>" for another library's)
//            n   words added to the preset's notes
//
// Blank lines and lines starting with # are skipped. A line naming a preset that doesn't exist, or a library version
// a preset hasn't got, is reported by `apply` (the tests require none).
namespace rigpatch
{
    // Applies `table` to `rigs` (matched by name). Returns the lines it couldn't use, for the tests.
    juce::StringArray apply (std::vector<Rig>& rigs, const juce::String& table);
}
