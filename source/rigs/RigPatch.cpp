#include "RigPatch.h"

namespace rigpatch
{
    namespace
    {
        // One change to one rig; false if it can't be made.
        bool change (Rig& rig, const juce::String& kind, const juce::String& what)
        {
            if (kind == "p")
            {
                if (! what.containsChar ('='))
                    return false;
                rig.parameters[what.upToFirstOccurrenceOf ("=", false, false).trim()] = what.fromFirstOccurrenceOf ("=", false, false).trim().getFloatValue();
                return true;
            }
            if (kind == "r")
            {
                const auto source = what.upToFirstOccurrenceOf (">", false, false).trim();
                const auto rest = what.fromFirstOccurrenceOf (">", false, false);
                const auto destination = rest.upToFirstOccurrenceOf ("=", false, false).trim();
                if (source.isEmpty() || destination.isEmpty() || ! rest.containsChar ('='))
                    return false;
                rig.modulation.routes.push_back ({ true, source, destination, rest.fromFirstOccurrenceOf ("=", false, false).trim().getFloatValue(), false, 0.0f, 0.0f });
                return true;
            }
            if (kind == "x")
            {
                const auto source = what.upToFirstOccurrenceOf (">", false, false).trim();
                const auto destination = what.fromFirstOccurrenceOf (">", false, false).trim();
                auto& routes = rig.modulation.routes;
                const auto before = routes.size();
                routes.erase (std::remove_if (routes.begin(), routes.end(), [&] (const ModRouteData& r)
                                              { return r.source == source && r.destination == destination; }), routes.end());
                return routes.size() < before;
            }
            if (kind == "s")
            {
                const auto osc = what.upToFirstOccurrenceOf ("=", false, false).trim().getIntValue();
                const auto file = what.fromFirstOccurrenceOf ("=", false, false).trim();
                const auto found = rig.sfz.find (osc);
                if (file.isEmpty())
                    return false;
                if (file.startsWith ("{sfz:"))
                {
                    rig.sfz[osc] = file;   // a whole reference: another library's file
                    return true;
                }
                if (found == rig.sfz.end())
                {
                    // A new oscillator on the same library as another of the preset's.
                    if (rig.sfz.empty())
                        return false;
                    const auto& any = rig.sfz.begin()->second;
                    rig.sfz[osc] = any.upToFirstOccurrenceOf ("}/", true, false) + file;
                    return true;
                }
                found->second = found->second.upToFirstOccurrenceOf ("}/", true, false) + file;
                return true;
            }
            if (kind == "n")
            {
                rig.notes = rig.notes.trimEnd() + " " + what;
                return true;
            }
            return false;
        }
    }

    juce::StringArray apply (std::vector<Rig>& rigs, const juce::String& table)
    {
        juce::StringArray unused;
        for (const auto& rawLine : juce::StringArray::fromLines (table))
        {
            const auto line = rawLine.trim();
            if (line.isEmpty() || line.startsWithChar ('#'))
                continue;
            const auto fields = juce::StringArray::fromTokens (line, "|", "");
            if (fields.size() < 4)
            {
                unused.add (line);
                continue;
            }
            const auto name = fields[0].trim(), version = fields[1].trim(), kind = fields[2].trim();
            const auto what = fields.joinIntoString ("|", 3).trim();
            bool done = false;
            for (auto& rig : rigs)
            {
                if (rig.name != name)
                    continue;
                bool ok = true;
                if (version == "synth" || version == "both")
                    ok = change (rig, kind, what) && ok;
                if (version == "lib" || version == "both")
                {
                    if (rig.sampled == nullptr)
                        ok = false;
                    else
                    {
                        auto copy = std::make_shared<Rig> (*rig.sampled);
                        ok = change (*copy, kind, what) && ok;
                        rig.sampled = copy;
                    }
                }
                done = ok;
                break;
            }
            if (! done)
                unused.add (line);
        }
        return unused;
    }
}
