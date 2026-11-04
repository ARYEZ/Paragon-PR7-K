// The built-in EXPANSIONS: themed banks of 15 presets after the user's banks A-E (rigs/Expansions.h,
// rigs/ExpansionRigs.cpp). Their banks and slot names, that a user's preset is never filed in one, that every
// preset's settings and routes are real ones, and that each plays: in time with a song, at a sensible level, clean,
// and silent once its notes and tails have ended. "[.render]" (hidden) writes a clip of each for listening.
#include <catch2/catch_test_macros.hpp>
#include <map>

#include "ModDestinations.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "rigs/Expansions.h"
#include "rigs/FactoryRigs.h"
#include "ui/RigBrowser.h"
#include "dsp/synth/Arpeggiator.h"

#include <chrono>

namespace
{
    constexpr double rate = 48000.0;
    constexpr int block = 256;

    struct TempFolder
    {
        TempFolder() { folder.deleteRecursively(); }
        ~TempFolder() { folder.deleteRecursively(); }
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ParagonX1ExpansionTests")
                                .getChildFile (juce::Uuid().toString());
    };

    struct Song final : juce::AudioPlayHead
    {
        double bpm = 85.0, ppq = 0.0;
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setIsPlaying (playing);
            info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
            return info;
        }
    };

    struct Plugin
    {
        TempFolder temp;
        ParagonX1AudioProcessor processor;
        Song song;
        Plugin()
        {
            processor.setPlayConfigDetails (0, 2, rate, block);
            processor.prepareToPlay (rate, block);
            processor.useRigFolder (temp.folder);
            processor.setPlayHead (&song);
        }
        const RigLibrary::Entry& entryNamed (const juce::String& name)
        {
            for (const auto& entry : processor.getRigLibrary().getEntries())
                if (entry.rig.name == name)
                    return entry;
            FAIL ("No preset called " << name);
            return processor.getRigLibrary().getEntries().front();
        }
        void load (const juce::String& name)
        {
            processor.loadRig (entryNamed (name));
            processor.prepareToPlay (rate, block);   // fresh: no tails from the last one
            processor.finishTableBuilds();           // FFT-edited waves are built in the background
            processor.seedRandomForTest (1);
            song.ppq = 0.0;
            song.playing = true;
        }
        // Plays `seconds` with MIDI events (seconds, message) into `out` (left, right), the song running.
        void play (double seconds, std::vector<std::pair<double, juce::MidiMessage>> events, std::vector<float>* left = nullptr,
                   std::vector<float>* right = nullptr, float* loudest = nullptr, bool* finite = nullptr,
                   std::function<void (double)> beforeBlock = {})
        {
            std::stable_sort (events.begin(), events.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });   // stable: a note-off before a note-on at the same time stays first
            juce::AudioBuffer<float> buffer (2, block);
            juce::MidiBuffer midi;
            size_t next = 0;
            const auto blocks = (int) std::ceil (seconds * rate / block);
            for (int b = 0; b < blocks; ++b)
            {
                midi.clear();
                const auto start = (juce::int64) b * block;
                while (next < events.size() && (juce::int64) std::llround (events[next].first * rate) < start + block)
                {
                    midi.addEvent (events[next].second, (int) juce::jlimit<juce::int64> (0, block - 1, std::llround (events[next].first * rate) - start));
                    ++next;
                }
                buffer.clear();
                if (beforeBlock)
                    beforeBlock ((double) start / rate);   // (0.63.0: a knob moved during the clip)
                processor.processBlock (buffer, midi);
                song.ppq += block / rate * song.bpm / 60.0;
                for (int i = 0; i < block; ++i)
                {
                    const auto l = buffer.getSample (0, i), r = buffer.getSample (1, i);
                    if (left != nullptr) { left->push_back (l); right->push_back (r); }
                    if (loudest != nullptr) *loudest = juce::jmax (*loudest, std::abs (l), std::abs (r));
                    if (finite != nullptr) *finite = *finite && std::isfinite (l) && std::isfinite (r);
                }
            }
        }
    };

    std::vector<Rig> expansionRigs (int expansion)
    {
        std::vector<Rig> rigs;
        for (auto& rig : makeExpansionRigs())
            if (rig.bank == Rig::numBanks + expansion)
                rigs.push_back (rig);
        return rigs;
    }

    juce::String typeOf (const Rig& rig)
    {
        for (const auto* type : { "Lead", "Pad", "Bass", "Keys", "Arp", "Pluck" })
            if (rig.tags[0] == type)
                return type;
        return {};
    }

    using Events = std::vector<std::pair<double, juce::MidiMessage>>;
    void chord (Events& e, double on, double off, std::initializer_list<int> notes, int velocity = 100)
    {
        for (const auto n : notes)
        {
            e.push_back ({ on, juce::MidiMessage::noteOn (1, n, (juce::uint8) velocity) });
            e.push_back ({ off, juce::MidiMessage::noteOff (1, n) });
        }
    }
}

TEST_CASE ("Each expansion is a bank of its own after E: named, with two-letter slot names", "[rigs][expansions]")
{
    REQUIRE (expansions::list.size() >= 1);
    CHECK (Rig::numBanksWithExpansions() == Rig::numBanks + (int) expansions::list.size());
    CHECK (Rig::bankName (Rig::numBanks) == "NEON RAIN");
    CHECK (Rig::bankName (Rig::numBanks - 1) == "E");
    CHECK (Rig::isExpansionBank (Rig::numBanks));
    CHECK_FALSE (Rig::isExpansionBank (Rig::numBanks - 1));

    for (int x = 0; x < (int) expansions::list.size(); ++x)
    {
        const auto rigs = expansionRigs (x);
        INFO (expansions::list[(size_t) x].name);
        REQUIRE (rigs.size() == 18);
        std::map<juce::String, int> perType;
        for (size_t i = 0; i < rigs.size(); ++i)
        {
            INFO (rigs[i].name);
            CHECK (rigs[i].number == (int) i + 1);
            CHECK (rigs[i].getSlotName() == juce::String (expansions::list[(size_t) x].code) + juce::String (i + 1).paddedLeft ('0', 2));
            CHECK (rigs[i].tags.contains (expansions::list[(size_t) x].name));
            ++perType[typeOf (rigs[i])];
        }
        // 3 leads, 3 pads, 3 basses, 3 keys, 3 arps, 3 plucks (0.70.0, slots 16-18); CHROMEFURY is lead-heavy at the
        // owner's ask: 6, 3, 3, 1, 2, 3.
        const auto leadHeavy = juce::String (expansions::list[(size_t) x].name) == "CHROMEFURY";
        CHECK (perType["Lead"] == (leadHeavy ? 6 : 3));
        CHECK (perType["Pad"] == 3);
        CHECK (perType["Bass"] == 3);
        CHECK (perType["Keys"] == (leadHeavy ? 1 : 3));
        CHECK (perType["Arp"] == (leadHeavy ? 2 : 3));
        CHECK (perType["Pluck"] == 3);
        for (size_t i = 15; i < rigs.size(); ++i)
            CHECK (typeOf (rigs[i]) == "Pluck");
    }
}

TEST_CASE ("Every factory and expansion preset has a name of its own", "[rigs][expansions]")
{
    // Two presets of one name can't be told apart in the browser or found by name (NIGHTDRIVE's first arp was called
    // Pulse Runner, as STARCOURT's SC08 already was).
    std::map<juce::String, juce::String> seen;
    auto rigs = makeFactoryRigs();
    for (auto& rig : makeExpansionRigs())
        rigs.push_back (rig);
    for (const auto& rig : rigs)
    {
        INFO (rig.name << " (" << rig.getSlotName() << ")");
        const auto [where, isNew] = seen.emplace (rig.name, rig.getSlotName());
        if (! isNew)
            FAIL_CHECK ("also " << where->second);
    }
    CHECK (seen.size() == rigs.size());
}

TEST_CASE ("Every expansion preset's settings, routes and macros are real ones", "[rigs][expansions]")
{
    ParagonX1AudioProcessor processor;
    juce::StringArray names;
    for (const auto& rig : makeFactoryRigs())
        names.add (rig.name);

    for (const auto& rig : makeExpansionRigs())
    {
        INFO (rig.name);
        CHECK_FALSE (names.contains (rig.name));   // unique across the factory presets and every expansion
        names.add (rig.name);
        CHECK (rig.notes.length() > 100);           // says what it is and what the macros do
        for (const auto& [id, value] : rig.parameters)
        {
            INFO (id);
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.getParameters().getParameter (id));
            REQUIRE (parameter != nullptr);
            CHECK_FALSE (ParamIDs::notInRigs.contains (id));
            const auto range = parameter->getNormalisableRange();
            CHECK (value >= range.start);   // a choice looked up by a name that isn't there would be -1
            CHECK (value <= range.end);
        }
        REQUIRE_FALSE (rig.modulation.routes.empty());
        for (const auto& r : rig.modulation.routes)
        {
            INFO (r.source << " -> " << r.destination);
            CHECK (synth::mod::sourceFromId (r.source) != synth::mod::Source::none);
            CHECK (ModDestinations::find (r.destination) != nullptr);
        }
        // MACRO 2 is SPACE (0.25.0): it moves REVERB's MIX.
        CHECK (rig.modulation.macroNames[1] == "SPACE");
        CHECK (std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(),
                            [] (const ModRouteData& r) { return r.source == "macro2" && r.destination == ParamIDs::reverbMix; }));
        for (int m = 0; m < 3; ++m)
        {
            CHECK (rig.modulation.macroNames[(size_t) m].isNotEmpty());
            const auto macro = "macro" + juce::String (m + 1);
            CHECK (std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(),
                                [&] (const ModRouteData& r) { return r.source == macro; }));   // each macro moves something
        }
    }
}

TEST_CASE ("NEON RAIN: MACRO 3 is STORM on every preset, DOWNPOUR silent at its left", "[rigs][expansions][downpour]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 0)   // NEON RAIN: the first expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "STORM");
        CHECK (rig.notes.contains ("STORM ("));
        CHECK (rig.parameters.at (ParamIDs::downpourOn) == 1.0f);
        CHECK (rig.parameters.at (ParamIDs::downpourRain) == 0.0f);   // no rain until STORM brings it: the sound as approved
        const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [] (const auto& r)
                                         { return r.source == "macro3" && r.destination == ParamIDs::downpourRain && r.amount > 0.5f; });
        CHECK (routed);
    }
    CHECK (count == 18);
}

TEST_CASE ("SANDSEA: MACRO 3 is SIROCCO on every preset, no answers at its left", "[rigs][expansions][sirocco]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 3)   // SANDSEA: the fourth expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "SIROCCO");
        CHECK (rig.notes.contains ("SIROCCO ("));
        CHECK (rig.parameters.at (ParamIDs::siroccoOn) == 1.0f);
        CHECK (rig.parameters.at (ParamIDs::siroccoLevel) == 0.0f);   // no answers until the macro brings them: the sound as approved
        CHECK (rig.parameters.at (ParamIDs::siroccoKey) == 2.0f);     // D
        const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [] (const auto& r)
                                         { return r.source == "macro3" && r.destination == ParamIDs::siroccoLevel && r.amount > 0.2f; });
        CHECK (routed);
    }
    CHECK (count == 18);
}

TEST_CASE ("SIMULACRUM: MACRO 3 is DILATE on every preset, time unslowed at its left", "[rigs][expansions][dilation]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 6)   // SIMULACRUM: the seventh expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "DILATE");
        CHECK (rig.notes.contains ("DILATE (time slowing down, plus "));
        CHECK (rig.parameters.at (ParamIDs::dilationOn) == 1.0f);
        CHECK (rig.parameters.at (ParamIDs::dilationSlow) == 0.0f);   // not slowed until the macro: the sound as approved
        const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [] (const auto& r)
                                         { return r.source == "macro3" && r.destination == ParamIDs::dilationSlow && r.amount >= 0.25f; });
        CHECK (routed);
    }
    CHECK (count == 18);
}

TEST_CASE ("DREAMSTATE: MACRO 3 is DREAM on every preset, no layers at its left", "[rigs][expansions][remnant]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 7)   // DREAMSTATE: the eighth expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "DREAM");
        CHECK (rig.notes.contains ("DREAM (layers beneath, each deeper one slower, plus "));
        CHECK (rig.parameters.at (ParamIDs::remnantOn) == 1.0f);
        CHECK (rig.parameters.at (ParamIDs::remnantMix) == 0.0f);   // no layers until the macro: the sound as approved
        const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [] (const auto& r)
                                         { return r.source == "macro3" && r.destination == ParamIDs::remnantMix && r.amount >= 0.2f; });
        CHECK (routed);
    }
    CHECK (count == 18);
}

TEST_CASE ("NEGATIVE SPACE: MACRO 3 is CLOSER on every preset, nothing added at its left", "[rigs][expansions][proximity]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 8)   // NEGATIVE SPACE: the ninth expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "CLOSER");
        CHECK (rig.notes.contains ("CLOSER (the sound of playing, up close, plus "));
        CHECK (rig.parameters.at (ParamIDs::proximityOn) == 1.0f);
        for (const auto* id : { ParamIDs::proximityNoise, ParamIDs::proximityRoom, ParamIDs::proximityClose })
        {
            CHECK (rig.parameters.at (id) == 0.0f);   // nothing until the macro: the sound as approved
            const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [id] (const auto& r)
                                             { return r.source == "macro3" && r.destination == id && r.amount >= 0.1f; });
            CHECK (routed);
        }
    }
    CHECK (count == 18);
}

TEST_CASE ("NIGHTDRIVE: MACRO 3 is HAZE on every preset, no movement at its left", "[rigs][expansions][haze]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 9)   // NIGHTDRIVE: the tenth expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "HAZE");
        CHECK (rig.notes.contains ("HAZE (streetlights passing, plus "));
        CHECK (rig.parameters.at (ParamIDs::hazeOn) == 1.0f);
        CHECK (rig.parameters.at (ParamIDs::hazeDepth) == 0.0f);   // no movement until the macro: the sound as approved
        const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [] (const auto& r)
                                         { return r.source == "macro3" && r.destination == ParamIDs::hazeDepth && r.amount >= 0.25f; });
        CHECK (routed);
    }
    CHECK (count == 18);
}

TEST_CASE ("UNSPOKEN: MACRO 3 is LEVIATHAN on every preset, no voice at its left", "[rigs][expansions][leviathan]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 10)   // UNSPOKEN: the eleventh expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "LEVIATHAN");
        CHECK (rig.notes.contains ("LEVIATHAN (a vast voice singing beneath it, plus "));
        CHECK (rig.parameters.at (ParamIDs::leviathanOn) == 1.0f);
        CHECK (rig.parameters.at (ParamIDs::leviathanMix) == 0.0f);   // no voice until the macro: the sound as approved
        const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [] (const auto& r)
                                         { return r.source == "macro3" && r.destination == ParamIDs::leviathanMix && r.amount >= 0.15f; });
        CHECK (routed);
    }
    CHECK (count == 18);
}

TEST_CASE ("CYBERSHRINE: MACRO 3 is SPECTRE on every preset, no ghost at its left", "[rigs][expansions][spectre]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 11)   // CYBERSHRINE: the twelfth expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "SPECTRE");
        CHECK (rig.notes.contains ("SPECTRE (a whispered ghost following it, plus "));
        CHECK (rig.parameters.at (ParamIDs::spectreOn) == 1.0f);
        CHECK (rig.parameters.at (ParamIDs::spectreMix) == 0.0f);   // no ghost until the macro: the sound as approved
        const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [] (const auto& r)
                                         { return r.source == "macro3" && r.destination == ParamIDs::spectreMix && r.amount >= 0.15f; });
        CHECK (routed);
    }
    CHECK (count == 18);
}

TEST_CASE ("ATOMIC AGE: MACRO 3 is SPRING on every preset, the tank brought in by the macro", "[rigs][expansions][nuclearspring]")
{
    int count = 0;
    for (const auto& rig : makeExpansionRigs())
    {
        if (rig.bank != Rig::numBanks + 24)   // ATOMIC AGE: the twenty-fifth expansion
            continue;
        ++count;
        INFO (rig.name);
        CHECK (rig.modulation.macroNames[2] == "SPRING");
        CHECK (rig.notes.contains ("SPRING ("));
        CHECK (rig.parameters.at (ParamIDs::springOn) == 1.0f);
        CHECK (rig.parameters.at (ParamIDs::springMix) == 0.0f);
        const auto routed = std::any_of (rig.modulation.routes.begin(), rig.modulation.routes.end(), [] (const auto& r)
                                         { return r.source == "macro3" && r.destination == ParamIDs::springMix && r.amount >= 0.3f; });
        CHECK (routed);
    }
    CHECK (count == 18);
}

TEST_CASE ("Expansion presets come after the user's banks and can't be filed into: a copy goes in A", "[rigs][expansions]")
{
    Plugin p;
    const auto& entries = p.processor.getRigLibrary().getEntries();
    REQUIRE (entries.back().rig.bank == Rig::numBanksWithExpansions() - 1);
    CHECK (entries.back().isFactory);

    p.load ("Brass Requiem");
    CHECK (p.processor.getCurrentRig().name == "Brass Requiem");
    REQUIRE (p.processor.saveCurrentRigAs ("My Brass"));
    const auto& saved = p.entryNamed ("My Brass");
    CHECK (saved.rig.bank == 0);
    CHECK (saved.rig.getSlotName() == "A48");   // after bank A's factory presets (A01-A47)

    Rig details = saved.rig;
    details.bank = Rig::numBanks;   // asked for an expansion's bank
    REQUIRE (p.processor.updateRigDetails (saved, details));
    CHECK (p.entryNamed ("My Brass").rig.bank == Rig::numBanks - 1);

    // A file never holds an expansion's bank.
    Rig odd;
    odd.bank = Rig::numBanks;
    const auto back = Rig::fromXml (*odd.toXml());
    REQUIRE (back.has_value());
    CHECK (back->bank == Rig::numBanks - 1);

    // < > step on into the expansions after the last user bank, and round.
    p.processor.loadRig (p.entryNamed ("Starlight Piano"));
    p.processor.stepRig (1);
    CHECK (p.processor.getCurrentRig().name == "My Brass");
    p.processor.stepRig (1);
    CHECK (p.processor.getCurrentRig().name == expansionRigs (0).front().name);
}

TEST_CASE ("Every expansion preset plays in time with a song: it sounds, stays clean, and ends", "[rigs][expansions]")
{
    Plugin p;
    for (const auto& rig : makeExpansionRigs())
    {
        INFO (rig.name);
        p.load (rig.name);
        Events e;
        const auto bass = typeOf (rig) == "Bass", lead = typeOf (rig) == "Lead";
        if (bass || lead)
            chord (e, 0.0, 5.0, { lead ? 64 : 40 });
        else
            chord (e, 0.0, 5.0, { 57, 60, 64 });   // A minor
        float loudest = 0.0f;
        bool finite = true;
        const auto started = std::chrono::steady_clock::now();
        p.play (5.0, e, nullptr, nullptr, &loudest, &finite);
        const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
        std::vector<float> left, right;
        p.song.playing = false;   // the song stopped too: a drum sequence plays for as long as the song does
        p.play (14.0, {}, &left, &right, nullptr, &finite);

        CHECK (finite);
        CHECK (juce::Decibels::gainToDecibels (loudest) > -24.0f);   // it sounds
        CHECK (juce::Decibels::gainToDecibels (loudest) < 1.0f);     // and doesn't clip (OUTPUT at its -6 dB)
        float tail = 0.0f;
        for (size_t i = left.size() - (size_t) rate; i < left.size(); ++i)
            tail = juce::jmax (tail, std::abs (left[i]), std::abs (right[i]));
        CHECK (tail < 1.0e-4f);                                      // silent 13 s after the keys are let go
        CHECK (p.processor.getActiveVoices() == 0);
        CHECK (seconds < 5.0 * 0.5);                                 // and plays in well under real time (here, in a debug-free build)
    }
}

// Not run with the others: writes a clip of each expansion preset as a WAV, for listening (the MP3s sent to the owner
// are made from these). Our own phrases, around 85 BPM in A minor. PARAGON_RENDER names the folder.
TEST_CASE ("Render a clip of each expansion preset", "[.render]")
{
    const auto folder = juce::File (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER", "/tmp/paragon-render"));
    folder.createDirectory();
    Plugin p;
    // PARAGON_RENDER_ONLY: just the presets whose names contain it; PARAGON_RENDER_PARAMS: "id=value,..." set after
    // loading (to hear what a setting does).
    const auto only = juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER_ONLY", {});
    const auto overrides = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER_PARAMS", {}), ",", {});
    for (const auto& rig : makeExpansionRigs())
    {
        if (only == "NEW PLUCKS" && rig.number < 16)   // the plucks of 0.70.0 (slots 16-18) alone
            continue;
        if (only.isNotEmpty() && only != "NEW PLUCKS" && ! rig.name.containsIgnoreCase (only) && ! rig.tags.contains (only, true))   // a name, or a bank (its tag)
            continue;
        p.load (rig.name);
        // NEON RAIN at 85 BPM in A minor; HELLFORGE at 140 in D, heavier and rhythmic.
        const auto hell = rig.bank == Rig::numBanks + 1;
        const auto star = rig.bank == Rig::numBanks + 2;   // STARCOURT: 100 BPM in C minor
        const auto sand = rig.bank == Rig::numBanks + 3;   // SANDSEA: 90 BPM in D, Phrygian dominant
        const auto data = rig.bank == Rig::numBanks + 4;   // DATASTREAM: 120 BPM in E minor
        const auto last = rig.bank == Rig::numBanks + 5;   // LAST LIGHT: 70 BPM in A minor, slow and sad
        const auto sim = rig.bank == Rig::numBanks + 6;    // SIMULACRUM: 130 BPM in E minor (E Phrygian for the runs)
        const auto dream = rig.bank == Rig::numBanks + 7;  // DREAMSTATE: 80 BPM in G minor, slow and heavy
        const auto negative = rig.bank == Rig::numBanks + 8;   // NEGATIVE SPACE: 85 BPM in D minor
        const auto night = rig.bank == Rig::numBanks + 9;      // NIGHTDRIVE: 100 BPM in A minor, synthwave
        const auto unspoken = rig.bank == Rig::numBanks + 10;  // UNSPOKEN: 70 BPM in B minor, slow and suspended
        const auto shrine = rig.bank == Rig::numBanks + 11;    // CYBERSHRINE: 72 BPM in D minor, ceremonial
        const auto stowaway = rig.bank == Rig::numBanks + 12;  // STOWAWAY: 60 BPM round A, half-step clashes, sparse
        const auto fury = rig.bank == Rig::numBanks + 13;      // CHROMEFURY: 140 BPM in D minor, Phrygian bite
        const auto ascent = rig.bank == Rig::numBanks + 14;    // ASCENSION: 80 BPM in D minor, Dorian chant
        const auto refract = rig.bank == Rig::numBanks + 15;   // REFRACTION: 75 BPM in E minor, uneasy half steps
        const auto psi = rig.bank == Rig::numBanks + 16;       // PSI STORM: 120 BPM, pelog on D (D Eb F A Bb)
        const auto blood = rig.bank == Rig::numBanks + 17;     // BLOODWAVE: 120 BPM in F minor, darksynth
        const auto dust = rig.bank == Rig::numBanks + 18;      // DUSTFALL: 70 BPM in D minor, vast and cold
        const auto super = rig.bank == Rig::numBanks + 19;     // SUPERPOSITION: 90 BPM in C minor, cold and cerebral
        const auto sev = rig.bank == Rig::numBanks + 20;       // SEVERED: 96 BPM, D minor slipping into bright F major
        const auto sling = rig.bank == Rig::numBanks + 21;     // SLINGBLADE: 90 BPM in C minor, heavy to soaring
        const auto para = rig.bank == Rig::numBanks + 22;      // PARADOX: 80 BPM in E minor, with gaps for ECHOES to fill
        const auto melt = rig.bank == Rig::numBanks + 23;      // MELTDOWN: 66 BPM in D minor, slow, with half-step clusters
        const auto atomic = rig.bank == Rig::numBanks + 24;    // ATOMIC AGE: 116 BPM in C major, a cheerful lounge, C6 Fmaj7 Dm7 G7
        p.song.bpm = hell ? 140.0 : star ? 100.0 : sand ? 90.0 : data ? 120.0 : last ? 70.0 : sim ? 130.0 : dream ? 80.0
                   : night ? 100.0 : unspoken ? 70.0 : shrine ? 72.0 : stowaway ? 60.0 : fury ? 140.0 : ascent ? 80.0 : refract ? 75.0 : psi ? 120.0 : blood ? 120.0 : dust ? 70.0 : super ? 90.0 : sev ? 96.0 : sling ? 90.0 : para ? 80.0 : melt ? 66.0 : atomic ? 116.0 : 85.0;
        const auto beat = 60.0 / p.song.bpm;
        for (const auto& item : overrides)
            if (auto* parameter = p.processor.getParameters().getParameter (item.upToFirstOccurrenceOf ("=", false, false)))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (item.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        Events e;
        const auto type = typeOf (rig);
        double length = 16.0 * beat;
        const auto at = [&] (double beats) { return beats * beat; };
        const auto pressure = [&] (double from, double to, int peak)
        {
            for (int k = 0; k <= 16; ++k)   // a press after the note, up and down again
            {
                const auto t = from + (to - from) * k / 16.0;
                const auto v = (int) std::round (peak * std::sin (juce::MathConstants<double>::pi * k / 16.0));
                e.push_back ({ at (t), juce::MidiMessage::channelPressureChange (1, v) });
            }
        };
        const auto wheel = [&] (double from, double to, int peak)
        {
            for (int k = 0; k <= 16; ++k)
            {
                const auto t = from + (to - from) * k / 16.0;
                e.push_back ({ at (t), juce::MidiMessage::controllerEvent (1, 1, (int) std::round (peak * std::sin (juce::MathConstants<double>::pi * k / 16.0))) });
            }
        };

        struct N { double on, len; int note; };
        const auto line = [&] (std::initializer_list<N> notes, int velocity = 100)
        {
            for (const auto& n : notes)
                chord (e, at (n.on), at (n.on + n.len), { n.note }, velocity);
        };
        const auto pedal = [&] (double down, double up)   // the sustain pedal
        {
            e.push_back ({ at (down), juce::MidiMessage::controllerEvent (1, 64, 127) });
            e.push_back ({ at (up), juce::MidiMessage::controllerEvent (1, 64, 0) });
        };
        const auto style = juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER_STYLE", {});
        if (type == "Keys" && style == "velocities")
        {
            // A keys preset's range: A1, A2, A3, A4, A5 and A6, each soft, medium and hard, two beats apart.
            int k = 0;
            for (const auto note : { 33, 45, 57, 69, 81, 93 })
                for (const auto velocity : { 35, 75, 120 })
                {
                    chord (e, at (k * 2.0), at (k * 2.0 + 1.6), { note }, velocity);
                    ++k;
                }
            length = (k * 2.0 + 3.0) * beat;
        }
        else if (type == "Pluck")
        {
            // Every bank's plucks (0.70.0): a riff in the bank's key, short notes with gaps (where the centrepiece is
            // heard), accents for the velocity, four bars over a falling bass of the scale, a three-note chord, then
            // a last note left to ring and a bar and a half of tail.
            const int bankIndex = rig.bank - Rig::numBanks;
            struct Key { int root; std::vector<int> scale; };
            const std::vector<int> minor { 0, 2, 3, 5, 7, 8, 10 }, major { 0, 2, 4, 5, 7, 9, 11 },
                                   phrygianDominant { 0, 1, 4, 5, 7, 8, 10 }, pelog { 0, 1, 3, 7, 8 };
            static const int roots[] { 9, 2, 0, 2, 4, 9, 4, 7, 2, 9, 11, 2, 9, 2, 2, 4, 2, 5, 2, 0, 2, 0, 4, 2, 0 };
            const auto root = 57 + ((roots[juce::jlimit (0, 24, bankIndex)] - 9 + 12) % 12);   // A3 to G#4
            const auto& scale = bankIndex == 3 ? phrygianDominant : bankIndex == 16 ? pelog : bankIndex == 24 ? major : minor;
            const auto degree = [&] (int d)
            {
                const auto n = (int) scale.size();
                const auto octave = (d >= 0 ? d / n : -((-d + n - 1) / n));
                return root + 12 * octave + scale[(size_t) (d - octave * n)];
            };
            const int riff[] { 0, -1, 2, 4, -1, 2, 1, 4, 7, -1, 4, 2, -1, 1, 2, -1 };      // sixteenths, -1 a rest
            const int accent[] { 115, 0, 80, 100, 0, 75, 85, 105, 120, 0, 85, 80, 0, 75, 95, 0 };
            const int shift[] { 0, -2, 3, 1 };                                                // each bar's step of the scale
            for (int bar = 0; bar < 4; ++bar)
                for (int k = 0; k < 16; ++k)
                    if (riff[k] >= 0 && (bar < 3 || k < 12))
                    {
                        const auto start = bar * 4.0 + k * 0.25;
                        chord (e, at (start), at (start + 0.2), { degree (riff[k] + shift[bar]) }, accent[k]);
                    }
            chord (e, at (15.0), at (15.4), { degree (shift[3]), degree (shift[3] + 2), degree (shift[3] + 4) }, 105);
            chord (e, at (16.0), at (16.3), { degree (7) }, 110);                             // left to ring
            length = 22.0 * beat;
        }
        else if (sling)
        {
            // The EXPR wheel rocked heel to toe and back (controller 11), for the presets whose wah it works.
            const auto rock = [&] (double from, double to)
            {
                for (int k = 0; k <= 24; ++k)
                {
                    const auto t = from + (to - from) * k / 24.0;
                    const auto v = (int) std::round (127.0 * std::sin (juce::MathConstants<double>::pi * k / 24.0));
                    e.push_back ({ at (t), juce::MidiMessage::controllerEvent (1, 11, v) });
                }
            };
            const auto onExpr = rig.name == "Drillbit Scream" || rig.name == "Dawnbreaker" || rig.name == "Fleet Assault";
            // The pedal at the heel for those; for the rest all the way up (unrouted, controller 11 is the volume swell,
            // and the clip before may have left it down).
            e.push_back ({ 0.0, juce::MidiMessage::controllerEvent (1, 11, onExpr ? 0 : 127) });
            if (type == "Lead")
            {
                // A defiant line in C minor: a riff, its answer climbing, a long held note with the wah rocked.
                line ({ { 0, 0.5, 60 }, { 0.5, 0.5, 63 }, { 1, 0.5, 65 }, { 1.5, 0.5, 67 }, { 2, 1.5, 70 }, { 3.5, 0.5, 67 },
                        { 4, 0.5, 68 }, { 4.5, 0.5, 67 }, { 5, 0.5, 65 }, { 5.5, 0.5, 63 }, { 6, 2, 65 },
                        { 8, 0.5, 67 }, { 8.5, 0.5, 70 }, { 9, 0.5, 72 }, { 9.5, 0.5, 74 }, { 10, 2, 75 }, { 12, 4, 72 } }, 105);
                if (onExpr)
                {
                    rock (0.0, 4.0);
                    rock (6.0, 8.0);
                    rock (12.0, 14.0);
                    rock (14.0, 16.0);
                }
                else
                    wheel (12.0, 16.0, 100);
                length = 20.0 * beat;
            }
            else if (type == "Pad")
            {
                // C minor, A flat, E flat, then G major: the long climb.
                chord (e, at (0), at (3.9), { 36, 48, 55, 60, 63 }, 90);
                chord (e, at (4), at (7.9), { 32, 48, 56, 60, 63 }, 90);
                chord (e, at (8), at (11.9), { 39, 51, 55, 58, 63 }, 90);
                chord (e, at (12), at (15.8), { 31, 50, 55, 59, 62 }, 90);
                wheel (8.0, 15.0, 100);
                length = 22.0 * beat;
            }
            else if (type == "Bass")
            {
                constexpr int roots[] = { 36, 32, 39, 31 };
                if (rig.name == "Pick And Hammer")   // re-struck eighths, harder on the beat: each opens the auto-wah
                {
                    for (int k = 0; k < 32; ++k)
                        if (k % 8 != 7)
                            line ({ { k * 0.5, 0.4, roots[k / 8] } }, k % 2 == 0 ? 120 : 80);
                }
                else
                    line ({ { 0, 3.9, roots[0] }, { 4, 3.9, roots[1] }, { 8, 3.9, roots[2] }, { 12, 3.9, roots[3] } }, 105);
                wheel (8.0, 15.0, 90);
                length = 19.0 * beat;
            }
            else if (type == "Keys")
            {
                const std::initializer_list<int> chords[] = { { 48, 55, 60, 63 }, { 44, 56, 60, 63 }, { 51, 55, 58, 63 }, { 43, 55, 59, 62 } };
                if (rig.name == "Victory Bells")   // single strikes, then the chord at the end
                {
                    line ({ { 0, 2, 60 }, { 2, 2, 67 }, { 4, 2, 63 }, { 6, 2, 72 }, { 8, 2, 55 }, { 10, 2, 62 } }, 100);
                    chord (e, at (12), at (15), { 48, 55, 59, 62 }, 110);
                }
                else
                {
                    int k = 0;
                    for (const auto& notes : chords)
                    {
                        chord (e, at (k * 4.0), at (k * 4.0 + 3.0), notes, 90);
                        ++k;
                    }
                    line ({ { 1, 1, 75 }, { 2, 1, 74 }, { 5, 1, 72 }, { 6, 1, 68 }, { 9, 1, 70 }, { 13, 2, 71 } }, 85);
                }
                length = 20.0 * beat;
            }
            else
            {
                // Arps: a C minor chord, then A flat (Siege Clock holds just the root).
                if (rig.name == "Siege Clock")
                {
                    chord (e, at (0), at (7.9), { 60 }, 100);
                    chord (e, at (8), at (15.9), { 56 }, 100);
                }
                else
                {
                    chord (e, at (0), at (7.9), { 60, 63, 67 }, 100);
                    chord (e, at (8), at (15.9), { 56, 60, 63 }, 100);
                }
                if (onExpr)
                {
                    rock (0.0, 4.0);
                    rock (8.0, 12.0);
                }
                length = 19.0 * beat;
            }
        }
        else if (atomic)
        {
            // Short notes and gaps throughout, so NUCLEARSPRING's drip and boing can be heard between them.
            if (type == "Lead")
            {
                // A cheerful tune in C major, a leap up, a held note, an answer.
                line ({ { 0, 0.5, 67 }, { 0.5, 0.5, 69 }, { 1, 1, 72 }, { 2, 0.5, 76 }, { 2.5, 0.5, 74 }, { 3, 1, 72 },
                        { 4, 2.5, 69 }, { 8, 0.5, 65 }, { 8.5, 0.5, 67 }, { 9, 1, 69 }, { 10, 0.5, 74 }, { 10.5, 0.5, 72 },
                        { 11, 1, 71 }, { 12, 3, 72 } }, 100);
                wheel (12.0, 15.0, 90);
                length = 19.0 * beat;
            }
            else if (type == "Pad")
            {
                chord (e, at (0), at (3.9), { 48, 60, 64, 67, 69 }, 90);    // C6
                chord (e, at (4), at (7.9), { 41, 57, 60, 64, 65 }, 90);    // Fmaj7
                chord (e, at (8), at (11.9), { 38, 57, 60, 62, 65 }, 90);   // Dm7
                chord (e, at (12), at (15.6), { 43, 59, 62, 65, 67 }, 90);  // G7
                wheel (8.0, 15.0, 90);
                length = 21.0 * beat;
            }
            else if (type == "Bass")
            {
                // A walking line, a note a beat, with a stop at the end.
                line ({ { 0, 0.9, 36 }, { 1, 0.9, 40 }, { 2, 0.9, 43 }, { 3, 0.9, 45 }, { 4, 0.9, 41 }, { 5, 0.9, 45 },
                        { 6, 0.9, 48 }, { 7, 0.9, 47 }, { 8, 0.9, 38 }, { 9, 0.9, 41 }, { 10, 0.9, 45 }, { 11, 0.9, 42 },
                        { 12, 0.9, 43 }, { 13, 0.9, 47 }, { 14, 0.9, 41 }, { 15, 1.5, 36 } }, 105);
                wheel (8.0, 14.0, 90);
                length = 19.0 * beat;
            }
            else if (type == "Keys")
            {
                // Comping: chords on the beat and the off-beat, then a held chord.
                const auto comp = [&] (double bar, std::initializer_list<int> notes)
                {
                    chord (e, at (bar), at (bar + 0.8), notes, 95);
                    chord (e, at (bar + 1.5), at (bar + 1.9), notes, 85);
                    chord (e, at (bar + 3.0), at (bar + 3.4), notes, 85);
                };
                comp (0.0, { 60, 64, 67, 69 });
                comp (4.0, { 57, 60, 64, 65 });
                comp (8.0, { 57, 60, 62, 65 });
                chord (e, at (12), at (15), { 59, 62, 65, 67 }, 95);
                length = 19.0 * beat;
            }
            else
            {
                chord (e, at (0), at (3.9), { 60, 64, 67, 69 }, 100);
                chord (e, at (4), at (7.9), { 57, 60, 64, 65 }, 100);
                chord (e, at (8), at (11.9), { 57, 60, 62, 65 }, 100);
                chord (e, at (12), at (15.9), { 59, 62, 65, 67 }, 100);
                length = 19.0 * beat;
            }
        }
        else if (melt)
        {
            if (type == "Lead")
            {
                // A grieving line in D minor, a half-step sigh, a long held note.
                line ({ { 0, 2, 62 }, { 2, 1, 65 }, { 3, 1, 64 }, { 4, 3, 62 }, { 8, 1.5, 69 }, { 9.5, 0.5, 70 }, { 10, 2, 69 },
                        { 12, 4, 65 } }, 95);
                wheel (12.0, 16.0, 90);
                length = 20.0 * beat;
            }
            else if (type == "Pad")
            {
                // D minor with a clashing half step, then Bb over D.
                chord (e, at (0), at (7.8), { 38, 50, 57, 62, 65 }, 90);
                chord (e, at (8), at (15.8), { 38, 50, 58, 62, 63 }, 90);
                wheel (8.0, 15.0, 90);
                length = 22.0 * beat;
            }
            else if (type == "Bass")
            {
                line ({ { 0, 3.9, 26 }, { 4, 3.9, 27 }, { 8, 3.9, 26 }, { 12, 3.9, 22 } }, 105);
                wheel (8.0, 15.0, 90);
                length = 20.0 * beat;
            }
            else if (type == "Keys")
            {
                line ({ { 0, 2, 62 }, { 2, 2, 69 }, { 4, 2, 65 }, { 6, 2, 63 } }, 90);
                chord (e, at (8), at (12), { 50, 57, 62, 65 }, 90);
                chord (e, at (12), at (15), { 46, 58, 62, 63 }, 90);
                length = 20.0 * beat;
            }
            else
            {
                chord (e, at (0), at (7.9), { 62, 65, 69 }, 100);
                chord (e, at (8), at (15.9), { 62, 63, 70 }, 100);
                length = 19.0 * beat;
            }
        }
        else if (para)
        {
            // Each phrase followed by room for ECHOES to bring it back (a bar or two later, backwards, slowed, looped).
            if (type == "Lead")
            {
                // A sad line in E minor, a bar's rest, its answer, a held note.
                line ({ { 0, 1, 64 }, { 1, 1, 67 }, { 2, 1.5, 71 }, { 3.5, 0.5, 69 }, { 4, 2, 67 }, { 6, 2, 66 },
                        { 12, 1, 64 }, { 13, 1, 62 }, { 14, 2, 60 }, { 16, 4, 59 } }, 95);
                wheel (16.0, 20.0, 90);
                length = 28.0 * beat;
            }
            else if (type == "Pad")
            {
                // E minor, C major, then held while the past comes back.
                chord (e, at (0), at (7.8), { 40, 52, 59, 64, 67 }, 90);
                chord (e, at (8), at (15.8), { 36, 52, 55, 60, 64 }, 90);
                wheel (8.0, 15.0, 90);
                length = 26.0 * beat;
            }
            else if (type == "Bass")
            {
                line ({ { 0, 3.9, 28 }, { 4, 3.9, 24 }, { 8, 3.9, 31 }, { 12, 3.9, 30 } }, 105);
                wheel (8.0, 15.0, 90);
                length = 22.0 * beat;
            }
            else if (type == "Keys")
            {
                // Broken chords, a bar's rest, then two chords held.
                line ({ { 0, 1, 52 }, { 1, 1, 59 }, { 2, 1, 64 }, { 3, 1, 67 }, { 4, 1, 48 }, { 5, 1, 55 }, { 6, 1, 60 }, { 7, 1, 64 } }, 85);
                chord (e, at (12), at (15.5), { 43, 55, 59, 62 }, 85);
                chord (e, at (16), at (19.5), { 47, 54, 59, 63 }, 85);
                length = 28.0 * beat;
            }
            else
            {
                // Arps: E minor, then C major, a bar's rest.
                chord (e, at (0), at (7.9), { 64, 67, 71 }, 100);
                chord (e, at (8), at (15.9), { 60, 64, 67 }, 100);
                length = 24.0 * beat;
            }
        }
        else if (sev && type == "Lead")
        {
            // A cheerful phrase in F major, then the same shape turned to D minor, ending on a held note.
            line ({ { 0, 0.5, 65 }, { 0.5, 0.5, 69 }, { 1, 0.5, 72 }, { 1.5, 0.5, 69 }, { 2, 1, 70 }, { 3, 1, 67 },
                    { 4, 0.5, 62 }, { 4.5, 0.5, 65 }, { 5, 0.5, 69 }, { 5.5, 0.5, 65 }, { 6, 1, 64 }, { 7, 1, 61 },
                    { 8, 1, 62 }, { 9, 1, 65 }, { 10, 2, 64 }, { 12, 4, 62 } }, 100);
            wheel (12.0, 16.0, 100);
            length = 20.0 * beat;
        }
        else if (sev && type == "Pad")
        {
            // F major, then D minor: the bright surface and what's under it.
            chord (e, at (0), at (7.8), { 41, 53, 57, 60, 65 }, 90);
            chord (e, at (8), at (15.8), { 38, 50, 57, 62, 65 }, 90);
            wheel (10.0, 15.0, 100);
            length = 22.0 * beat;
        }
        else if (sev && type == "Bass")
        {
            // Held notes (the step sequencer and GLITCH make the rhythm), then the plain ones re-struck: D, Bb, F, A.
            if (rig.name == "Break Room Bass")
            {
                constexpr int roots[] = { 38, 34, 41, 33 };
                for (int k = 0; k < 16; ++k)
                    if (k % 4 != 3)
                        line ({ { k * 1.0, 0.6, roots[k / 4] } }, k % 2 == 0 ? 110 : 90);
            }
            else
                line ({ { 0, 3.9, 38 }, { 4, 3.9, 34 }, { 8, 3.9, 41 }, { 12, 3.9, 33 } }, 105);
            wheel (8.0, 15.0, 90);
            length = 19.0 * beat;
        }
        else if (sev && type == "Keys")
        {
            // The melancholy piano figure: Dm, Bb, F, A, with a high answer.
            const std::initializer_list<int> chords[] = { { 50, 57, 62, 65 }, { 46, 58, 62, 65 }, { 53, 57, 60, 65 }, { 45, 57, 61, 64 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                chord (e, at (c * 4.0), at (c * 4.0 + 3.0), notes, 90);
                ++c;
            }
            line ({ { 1, 1, 74 }, { 2, 1, 72 }, { 5, 1, 74 }, { 6, 1, 70 }, { 9, 1, 72 }, { 13, 2, 73 } }, 80);
            length = 20.0 * beat;
        }
        else if (sev)
        {
            chord (e, at (0), at (7.9), { 50, 53, 57 }, 100);
            chord (e, at (8), at (15.9), { 53, 57, 60 }, 100);
            length = 19.0 * beat;
        }
        else if (super && type == "Lead")
        {
            // A searching line in C minor: a phrase, the same phrase changed, then a long held note.
            line ({ { 0, 1, 60 }, { 1, 0.5, 63 }, { 1.5, 0.5, 67 }, { 2, 2, 65 }, { 4, 1, 60 }, { 5, 0.5, 63 }, { 5.5, 0.5, 68 },
                    { 6, 2, 67 }, { 8, 1, 70 }, { 9, 1, 68 }, { 10, 2, 67 }, { 12, 4, 72 } }, 100);
            wheel (12.0, 16.0, 100);
            length = 20.0 * beat;
        }
        else if (super && type == "Pad")
        {
            // C minor, then A flat major over C: two versions of the same place.
            chord (e, at (0), at (7.8), { 36, 48, 55, 63, 67 }, 90);
            chord (e, at (8), at (15.8), { 32, 48, 56, 63, 68 }, 90);
            wheel (10.0, 15.0, 100);
            length = 22.0 * beat;
        }
        else if (super && type == "Bass")
        {
            if (rig.name == "Probability Bass")   // held notes: TRAVEL does the movement
                line ({ { 0, 3.8, 36 }, { 4, 3.8, 32 }, { 8, 3.8, 39 }, { 12, 3.8, 34 } }, 105);
            else   // a pulse on the beat, with gaps, so Event Horizon's fall is heard
            {
                constexpr int roots[] = { 36, 32, 39, 34 };
                for (int k = 0; k < 16; ++k)
                    if (k % 4 != 3)
                        line ({ { k * 1.0, 0.7, roots[k / 4] } }, k % 2 == 0 ? 110 : 90);
            }
            wheel (8.0, 15.0, 90);
            length = 19.0 * beat;
        }
        else if (super && type == "Keys")
        {
            // Cm, Ab, Eb, Bb: sparse chords and a high answer.
            const std::initializer_list<int> chords[] = { { 48, 55, 60, 63 }, { 44, 56, 60, 63 }, { 51, 55, 58, 63 }, { 46, 58, 62, 65 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                chord (e, at (c * 4.0), at (c * 4.0 + 3.0), notes, 90);
                ++c;
            }
            line ({ { 1, 1, 72 }, { 5, 1, 75 }, { 9, 1, 70 }, { 13, 2, 74 } }, 80);
            length = 20.0 * beat;
        }
        else if (super)
        {
            chord (e, at (0), at (7.9), { 48, 51, 55 }, 100);
            chord (e, at (8), at (15.9), { 48, 51, 56 }, 100);
            length = 19.0 * beat;
        }
        else if (dust && type == "Lead")
        {
            // A slow, lonely line in D minor, rising to a long held note (the wheel late in it).
            line ({ { 0, 1.5, 62 }, { 1.5, 0.5, 65 }, { 2, 2, 69 }, { 4, 1, 67 }, { 5, 1, 65 }, { 6, 2, 64 },
                    { 8, 1.5, 62 }, { 9.5, 0.5, 65 }, { 10, 2, 70 }, { 12, 4, 69 } }, 100);
            wheel (12.0, 16.0, 100);
            length = 20.0 * beat;
        }
        else if (dust && type == "Pad")
        {
            // D minor, then B flat over D: slow and wide.
            chord (e, at (0), at (7.8), { 38, 50, 57, 62, 65 }, 90);
            chord (e, at (8), at (15.8), { 34, 50, 58, 62, 65 }, 90);
            wheel (10.0, 15.0, 100);
            length = 22.0 * beat;
        }
        else if (dust && type == "Bass")
        {
            // Long low notes: D, B flat, G, A.
            line ({ { 0, 3.5, 26 }, { 4, 3.5, 22 }, { 8, 3.5, 31 }, { 12, 3.5, 33 } }, 110);
            wheel (8.0, 15.0, 90);
            length = 19.0 * beat;
        }
        else if (dust && type == "Keys")
        {
            // Sparse chords and a high answer: Dm, B flat, Gm, A.
            const std::initializer_list<int> chords[] = { { 50, 57, 62, 65 }, { 46, 58, 62, 65 }, { 43, 58, 62, 67 }, { 45, 57, 61, 64 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                chord (e, at (c * 4.0), at (c * 4.0 + 3.0), notes, 90);
                ++c;
            }
            line ({ { 1, 1, 74 }, { 5, 1, 77 }, { 9, 1, 74 }, { 13, 2, 73 } }, 80);
            length = 20.0 * beat;
        }
        else if (dust)
        {
            chord (e, at (0), at (7.9), { 50, 53, 57 }, 100);
            chord (e, at (8), at (15.9), { 50, 53, 58 }, 100);
            length = 19.0 * beat;
        }
        else if (blood && type == "Lead")
        {
            // A darksynth line in F minor: driving eighths, a climb to a held high note, then a release (Siren
            // Scream dives on each release).
            line ({ { 0, 0.5, 65 }, { 0.5, 0.5, 68 }, { 1, 0.5, 72 }, { 1.5, 0.5, 70 }, { 2, 1, 68 }, { 3, 1, 67 },
                    { 4, 0.5, 65 }, { 4.5, 0.5, 68 }, { 5, 0.5, 72 }, { 5.5, 0.5, 75 }, { 6, 2, 73 }, { 8, 1, 72 },
                    { 9, 1, 70 }, { 10, 2, 68 }, { 12, 4, 77 } }, 105);
            wheel (12.0, 16.0, 100);
            length = 20.0 * beat;
        }
        else if (blood && type == "Pad")
        {
            // F minor, D flat, B flat minor, C: the dark loop.
            const std::initializer_list<int> chords[] = { { 41, 53, 56, 60, 65 }, { 37, 53, 56, 61, 65 }, { 34, 53, 58, 61, 65 }, { 36, 52, 55, 60, 64 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                chord (e, at (c * 4.0), at (c * 4.0 + 3.9), notes, 90);
                ++c;
            }
            wheel (8.0, 15.0, 100);
            length = 22.0 * beat;
        }
        else if (blood && type == "Bass")
        {
            if (rig.name == "Pump Bass")   // held notes: the pump does the rhythm
                line ({ { 0, 4, 29 }, { 4, 4, 25 }, { 8, 4, 34 }, { 12, 4, 36 } }, 105);
            else   // driving eighths
            {
                constexpr int roots[] = { 29, 25, 34, 36 };
                for (int k = 0; k < 32; ++k)
                    line ({ { k * 0.5, 0.4, roots[k / 8] + (k % 4 == 3 ? 12 : 0) } }, k % 2 == 0 ? 110 : 90);
            }
            wheel (8.0, 15.0, 90);
            length = 19.0 * beat;
        }
        else if (blood && type == "Keys")
        {
            if (rig.name == "Club Stabs")   // one key a stab (the chorder makes the chord)
            {
                constexpr int roots[] = { 53, 49, 58, 48 };
                for (int c = 0; c < 4; ++c)
                    for (const auto offset : { 0.0, 1.5, 2.5 })
                        line ({ { c * 4.0 + offset, 0.3, roots[c] } }, 110);
            }
            else
            {
                const std::initializer_list<int> chords[] = { { 53, 56, 60 }, { 49, 53, 56 }, { 46, 49, 53 }, { 48, 52, 55 } };
                int c = 0;
                for (const auto& notes : chords)
                {
                    chord (e, at (c * 4.0), at (c * 4.0 + 1.8), notes, 95);
                    chord (e, at (c * 4.0 + 2.0), at (c * 4.0 + 3.8), notes, 80);
                    ++c;
                }
                line ({ { 0, 2, 72 }, { 4, 2, 68 }, { 8, 2, 70 }, { 12, 3, 67 } }, 90);
            }
            wheel (8.0, 15.0, 100);
            length = 20.0 * beat;
        }
        else if (blood)
        {
            chord (e, at (0), at (7.9), { 65, 68, 72 }, 100);
            chord (e, at (8), at (15.9), { 61, 65, 68 }, 100);
            length = 19.0 * beat;
        }
        else if (psi && type == "Lead")
        {
            // A calling line in pelog on D (D Eb F A Bb), rising to a long held note (so Psychic Scream can build).
            line ({ { 0, 1, 62 }, { 1, 1, 63 }, { 2, 2, 65 }, { 4, 1, 69 }, { 5, 1, 70 }, { 6, 2, 69 }, { 8, 1, 65 },
                    { 9, 1, 63 }, { 10, 2, 62 }, { 12, 1, 65 }, { 13, 1, 69 }, { 14, 10, 74 } }, 100);
            wheel (18.0, 24.0, 100);
            length = 28.0 * beat;
        }
        else if (psi && type == "Pad")
        {
            // Open fifths on D, then on B flat: the chant's drone, then the shift.
            chord (e, at (0), at (11.8), { 38, 45, 50, 57, 62, 69 }, 90);
            chord (e, at (12), at (23.8), { 34, 41, 46, 53, 62, 65 }, 90);
            wheel (14.0, 22.0, 100);
            length = 30.0 * beat;
        }
        else if (psi && type == "Bass")
        {
            if (rig.name == "Night Ride")
                line ({ { 0, 8, 38 }, { 8, 4, 34 }, { 12, 4, 33 } }, 110);
            else
                line ({ { 0, 2, 38 }, { 2, 1, 39 }, { 3, 1, 41 }, { 4, 3, 38 }, { 8, 2, 45 }, { 10, 2, 46 }, { 12, 4, 38 } }, 105);
            wheel (8.0, 15.0, 90);
            length = 20.0 * beat;
        }
        else if (psi && type == "Keys")
        {
            if (rig.name == "Temple Gong")
                line ({ { 0, 4, 38 }, { 4, 4, 45 }, { 8, 4, 34 }, { 12, 6, 38 } }, 110);
            else   // a running gamelan figure in pelog on D, in eighths
            {
                constexpr int figure[] = { 62, 63, 65, 69, 70, 69, 65, 63, 62, 65, 63, 62, 57, 58, 57, 62 };
                for (int k = 0; k < 16; ++k)
                    line ({ { k * 0.5, 0.45, figure[k] } }, k % 4 == 0 ? 110 : 90);
                for (int k = 0; k < 8; ++k)
                    line ({ { 8.0 + k, 0.9, figure[k * 2] } }, 100);
            }
            length = 22.0 * beat;
        }
        else if (psi)
        {
            if (rig.name == "Interlock")
                chord (e, at (0), at (15.9), { 62 }, 100);   // one key: the root
            else
            {
                chord (e, at (0), at (7.9), { 62, 65, 69 }, 100);
                chord (e, at (8), at (15.9), { 58, 62, 65 }, 100);
            }
            length = 20.0 * beat;
        }
        else if (refract && type == "Lead")
        {
            // A slow, searching line in E minor that slips by half steps (the F natural, the D sharp), long notes to
            // hear each sound change as it's held.
            line ({ { 0, 3, 64 }, { 3, 1, 65 }, { 4, 4, 67 }, { 8, 2, 71 }, { 10, 1, 72 }, { 11, 1, 71 }, { 12, 2, 67 },
                    { 14, 1, 66 }, { 15, 1, 63 }, { 16, 4, 64 } }, 95);
            wheel (16.0, 20.0, 100);
            length = 24.0 * beat;
        }
        else if (refract && type == "Pad")
        {
            // E minor, then a C major chord with an F natural in it (the half step against E): beautiful, then wrong.
            chord (e, at (0), at (7.8), { 40, 52, 59, 64, 67, 71 }, 90);
            chord (e, at (8), at (15.8), { 36, 52, 55, 60, 65, 72 }, 90);
            wheel (10.0, 15.0, 100);
            length = 24.0 * beat;
        }
        else if (refract && type == "Bass")
        {
            line ({ { 0, 6, 28 }, { 6, 2, 29 }, { 8, 4, 24 }, { 12, 4, 27 } }, 100);
            wheel (8.0, 15.0, 90);
            length = 20.0 * beat;
        }
        else if (refract && type == "Keys")
        {
            if (rig.name == "Folk Pluck")
            {
                // Fingerpicked: a low string and broken chords in eighths, Em - C - Am - B.
                const int roots[] = { 40, 36, 45, 47 };
                const std::initializer_list<int> shapes[] = { { 52, 55, 59, 64 }, { 52, 55, 60, 64 }, { 52, 57, 60, 64 }, { 51, 54, 59, 63 } };
                for (int c = 0; c < 4; ++c)
                {
                    line ({ { c * 4.0, 2.0, roots[c] } }, 80);
                    double t = c * 4.0 + 0.5;
                    for (const auto note : shapes[c])
                    {
                        line ({ { t, 1.4, note } }, 65);
                        t += 0.5;
                    }
                    line ({ { c * 4.0 + 2.5, 1.4, *(shapes[c].begin() + 2) } }, 60);
                }
                length = 20.0 * beat;
            }
            else
            {
                line ({ { 0, 2, 64 }, { 2, 2, 71 }, { 4, 3, 67 }, { 8, 2, 76 }, { 10, 2, 75 }, { 12, 4, 64 } }, 95);
                chord (e, at (0), at (3.8), { 52 }, 70);
                chord (e, at (8), at (11.8), { 48 }, 70);
                length = 22.0 * beat;
            }
        }
        else if (refract)
        {
            chord (e, at (0), at (7.9), { 64, 67, 71 }, 100);
            chord (e, at (8), at (15.9), { 60, 64, 65 }, 100);
            length = 21.0 * beat;
        }
        else if (ascent && type == "Lead")
        {
            // A chant-like line in D Dorian: stepwise, rising to the fifth and settling, with a long held note.
            line ({ { 0, 2, 62 }, { 2, 1, 64 }, { 3, 1, 65 }, { 4, 3, 67 }, { 7, 1, 65 }, { 8, 2, 69 }, { 10, 1, 71 },
                    { 11, 1, 69 }, { 12, 4, 62 } }, rig.name == "Soprano Call" ? 100 : 95);
            if (rig.name == "Soprano Call")
                line ({ { 16, 3, 74 } }, 100);
            wheel (8.0, 15.0, 100);
            length = 22.0 * beat;
        }
        else if (ascent && type == "Pad")
        {
            // Wide voicings, so the basses sing low and the sopranos high: D minor, then B flat.
            chord (e, at (0), at (7.8), { 38, 50, 57, 62, 65, 69 }, 92);
            chord (e, at (8), at (15.8), { 34, 50, 58, 62, 65, 70 }, 92);
            if (rig.name != "Monk Chant")
                wheel (10.0, 15.0, 100);
            length = 24.0 * beat;
        }
        else if (ascent && type == "Bass")
        {
            if (rig.name == "Battle Strings")
                line ({ { 0, 8, 38 }, { 8, 4, 34 }, { 12, 4, 36 } }, 110);
            else
                line ({ { 0, 6, 38 }, { 6, 2, 36 }, { 8, 4, 34 }, { 12, 4, 33 } }, 100);
            if (rig.name == "Deep Brass")
                pressure (8.0, 15.0, 100);
            length = 21.0 * beat;
        }
        else if (ascent && type == "Keys")
        {
            if (rig.name == "Choir Stabs")
            {
                const std::initializer_list<int> chords[] = { { 38, 50, 57, 62, 69 }, { 34, 46, 58, 62, 70 }, { 36, 48, 55, 60, 67 }, { 33, 45, 57, 61, 69 } };
                int c = 0;
                for (const auto& notes : chords)
                {
                    for (const auto offset : { 0.0, 1.5, 3.0 })
                        chord (e, at (c * 4 + offset), at (c * 4 + offset + 0.5), notes, offset == 0.0 ? 115 : 100);
                    ++c;
                }
            }
            else if (rig.name == "Ancient Bells")
                line ({ { 0, 2, 50 }, { 3, 2, 57 }, { 6, 2, 53 }, { 9, 2, 55 }, { 12, 3, 50 } }, 100);
            else
            {
                const int bass[] = { 38, 34, 41, 33 };
                const std::initializer_list<int> hands[] = { { 50, 53, 57 }, { 50, 53, 58 }, { 48, 53, 57 }, { 49, 52, 57 } };
                for (int c = 0; c < 4; ++c)
                {
                    chord (e, at (c * 4), at (c * 4 + 3.9), { bass[c] }, 60);
                    chord (e, at (c * 4 + 1), at (c * 4 + 3.9), hands[c], 50);
                    pedal (c * 4 + 0.05, c * 4 + 3.95);
                }
                line ({ { 0, 2, 69 }, { 2, 2, 67 }, { 4, 3, 65 }, { 8, 2, 64 }, { 10, 2, 65 }, { 12, 4, 62 } }, 65);
            }
            length = 22.0 * beat;
        }
        else if (ascent)
        {
            chord (e, at (0), at (7.9), { 62, 65, 69 }, 100);
            chord (e, at (8), at (15.9), { 58, 62, 65 }, 100);
            length = 21.0 * beat;
        }
        else if (fury && type == "Lead")
        {
            // A screaming riff in D minor: a run up through the flat second, a held high note with the wheel,
            // a bend dive at the end.
            line ({ { 0, 0.5, 62 }, { 0.5, 0.5, 63 }, { 1, 0.5, 65 }, { 1.5, 0.5, 67 }, { 2, 2, 69 }, { 4, 0.75, 70 },
                    { 4.75, 0.75, 69 }, { 5.5, 0.5, 67 }, { 6, 2, 69 }, { 8, 1, 74 }, { 9, 1, 72 }, { 10, 4, 74 },
                    { 14, 2, 69 } }, 110);
            wheel (10.0, 14.0, 110);
            for (int k = 0; k <= 16; ++k)   // a whammy dive on the last note
                e.push_back ({ at (14.5 + k * 1.2 / 16.0), juce::MidiMessage::pitchWheel (1, 8192 - k * 512) });
            e.push_back ({ at (16.5), juce::MidiMessage::pitchWheel (1, 8192) });
            length = 19.0 * beat;
        }
        else if (fury && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 38, 50, 57, 62 }, 95);
            chord (e, at (8), at (15.8), { 39, 51, 58, 63 }, 95);   // the flat second, a Phrygian lift
            wheel (8.0, 15.0, 100);
            length = 22.0 * beat;
        }
        else if (fury && type == "Bass")
        {
            if (rig.name == "Piston Drive")
                line ({ { 0, 8, 26 }, { 8, 4, 27 }, { 12, 4, 24 } }, 110);
            else
                line ({ { 0, 0.75, 26 }, { 0.75, 0.75, 26 }, { 1.5, 0.5, 27 }, { 2, 1.5, 26 }, { 4, 0.75, 26 }, { 4.75, 0.75, 26 },
                        { 5.5, 0.5, 29 }, { 6, 1.5, 27 }, { 8, 0.75, 26 }, { 8.75, 0.75, 26 }, { 9.5, 0.5, 27 }, { 10, 1.5, 26 },
                        { 12, 2, 24 }, { 14, 2, 27 } }, 110);
            wheel (8.0, 15.0, 110);
            length = 19.0 * beat;
        }
        else if (fury && type == "Keys")
        {
            const std::initializer_list<int> chords[] = { { 50, 57, 62, 65 }, { 51, 58, 63, 67 }, { 48, 55, 60, 64 }, { 50, 57, 62, 65 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                for (const auto offset : { 0.0, 0.75, 1.5, 3.0 })
                    chord (e, at (c * 4 + offset), at (c * 4 + offset + 0.4), notes, offset == 0.0 ? 115 : 100);
                ++c;
            }
            length = 19.0 * beat;
        }
        else if (fury)
        {
            chord (e, at (0), at (7.9), { 62, 65, 69 }, 105);
            chord (e, at (8), at (15.9), { 63, 67, 70 }, 105);
            length = 19.0 * beat;
        }
        else if (stowaway && type == "Lead")
        {
            // Sparse and uneasy: A, a half step up and back, a tritone, long silences between.
            line ({ { 0, 2.5, 69 }, { 3, 1, 70 }, { 4, 3, 69 }, { 8, 2, 75 }, { 10.5, 1.5, 74 }, { 13, 3, 69 } }, 90);
            wheel (8.0, 15.0, 100);
            length = 21.0 * beat;
        }
        else if (stowaway && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 45, 52, 57, 60 }, 85);    // A minor, open
            chord (e, at (8), at (15.8), { 46, 52, 58, 61 }, 85);   // B flat over the E: unresolved
            wheel (10.0, 15.0, 100);
            length = 23.0 * beat;
        }
        else if (stowaway && type == "Bass")
        {
            if (rig.name == "Steam Bass")
                line ({ { 0, 1, 33 }, { 1.5, 0.5, 33 }, { 2, 1, 34 }, { 4, 1, 33 }, { 5.5, 0.5, 39 }, { 6, 2, 33 },
                        { 8, 1, 33 }, { 9.5, 0.5, 33 }, { 10, 1, 34 }, { 12, 1, 33 }, { 13.5, 0.5, 39 }, { 14, 2, 34 } }, 105);
            else
                line ({ { 0, 8, 33 }, { 8, 8, 34 } }, 100);
            if (rig.name == "Dread Brass")
                wheel (8.0, 15.0, 100);
            length = 22.0 * beat;
        }
        else if (stowaway && type == "Keys")
        {
            if (rig.name == "Data Chatter")
                for (int k = 0; k < 24; ++k)
                    chord (e, at (k * 0.5 + (k % 3 == 2 ? 0.25 : 0.0)), at (k * 0.5 + 0.2), { 69 + (k * 5) % 12 }, 90);
            else
            {
                chord (e, at (0), at (3.5), { 45, 52, 57 }, 80);
                line ({ { 4, 2, 70 }, { 6, 2, 69 } }, 70);
                chord (e, at (8), at (11.5), { 46, 52, 58 }, 80);
                line ({ { 12, 3, 63 } }, 70);
            }
            length = 20.0 * beat;
        }
        else if (stowaway)
        {
            if (rig.name == "Proximity Ping")
                chord (e, at (0), at (15.9), { 81 }, 100);
            else
            {
                chord (e, at (0), at (7.9), { 57, 60, 64 }, 95);
                chord (e, at (8), at (15.9), { 58, 61, 64 }, 95);
            }
            length = 20.0 * beat;
        }
        else if (shrine && type == "Lead")
        {
            // D minor with a folk colour: a keening line that leans on the second and the fourth.
            line ({ { 0, 2, 69 }, { 2, 1, 67 }, { 3, 1, 69 }, { 4, 3, 72 }, { 7, 1, 70 }, { 8, 2, 69 }, { 10, 1, 64 },
                    { 11, 1, 65 }, { 12, 4, 62 } }, 95);
            wheel (8.0, 15.0, 100);
            length = 20.0 * beat;
        }
        else if (shrine && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 50, 57, 62, 65 }, 90);   // D minor (open)
            chord (e, at (8), at (15.8), { 46, 58, 62, 65 }, 90);  // B flat
            if (rig.name != "Ancient Chorus")
                wheel (8.0, 15.0, 100);
            length = 24.0 * beat;
        }
        else if (shrine && type == "Bass")
        {
            if (rig.name == "Temple Drum")
                line ({ { 0, 1, 38 }, { 2, 1, 38 }, { 2.5, 1, 33 }, { 4, 1, 38 }, { 6, 1, 34 }, { 8, 1, 38 }, { 10, 1, 38 },
                        { 10.5, 1, 33 }, { 12, 1, 31 }, { 14, 1, 33 } }, 110);
            else
                line ({ { 0, 8, 38 }, { 8, 4, 34 }, { 12, 4, 33 } }, 105);
            length = 20.0 * beat;
        }
        else if (shrine && type == "Keys")
        {
            const std::initializer_list<int> chords[] = { { 50, 57, 62, 65 }, { 46, 58, 62, 65 }, { 48, 55, 60, 64 }, { 45, 57, 61, 64 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                chord (e, at (c * 4), at (c * 4 + 2.8), notes, 85);
                line ({ { c * 4 + 2.0, 1.5, c % 2 == 0 ? 74 : 72 } }, 70);
                ++c;
            }
            length = 22.0 * beat;
        }
        else if (shrine)
        {
            chord (e, at (0), at (7.9), { 62, 65, 69 }, 95);
            chord (e, at (8), at (15.9), { 58, 62, 65 }, 95);
            length = 21.0 * beat;
        }
        else if (unspoken && type == "Lead")
        {
            // B minor, slow: a questioning line that rises and settles back where it began.
            line ({ { 0, 3, 66 }, { 3, 1, 69 }, { 4, 4, 71 }, { 8, 2, 74 }, { 10, 2, 73 }, { 12, 4, 66 } }, 95);
            if (rig.name != "Glass Voice")
                wheel (8.0, 15.0, 100);
            else
                wheel (4.0, 8.0, 90);
            length = 21.0 * beat;
        }
        else if (unspoken && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 47, 54, 59, 62 }, 90);   // B minor
            chord (e, at (8), at (15.8), { 43, 55, 59, 62 }, 90);  // G major
            if (rig.name == "Low Mist")
                wheel (8.0, 15.0, 110);
            length = 24.0 * beat;
        }
        else if (unspoken && type == "Bass")
        {
            line ({ { 0, 8, 35 }, { 8, 4, 31 }, { 12, 4, 30 } }, 105);
            if (rig.name == "Tidal Swell")
                pressure (8.0, 15.0, 100);
            else if (rig.name == "Vast Hull")
                wheel (6.0, 14.0, 110);
            length = 21.0 * beat;
        }
        else if (unspoken && type == "Keys")
        {
            // Sparse: B minor, G, D, F sharp, a note answered by silence.
            const std::initializer_list<int> chords[] = { { 47, 59, 62, 66 }, { 43, 59, 62, 67 }, { 50, 57, 62, 66 }, { 42, 58, 61, 66 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                chord (e, at (c * 4), at (c * 4 + 2.8), notes, 80);
                line ({ { c * 4 + 2.0, 1.5, c % 2 == 0 ? 74 : 73 } }, 65);
                if (rig.name == "Memory Loop")
                    pedal (c * 4 + 0.05, c * 4 + 3.95);
                ++c;
            }
            length = 22.0 * beat;
        }
        else if (unspoken)
        {
            chord (e, at (0), at (7.9), { 59, 62, 66 }, 95);
            chord (e, at (8), at (15.9), { 55, 59, 62 }, 95);
            length = 21.0 * beat;
        }
        else if (night && type == "Lead")
        {
            // A minor: a hook over Am - F - C - G.
            line ({ { 0, 1.5, 69 }, { 1.5, 0.5, 72 }, { 2, 2, 76 }, { 4, 1.5, 77 }, { 5.5, 0.5, 76 }, { 6, 2, 72 },
                    { 8, 1.5, 72 }, { 9.5, 0.5, 74 }, { 10, 2, 76 }, { 12, 1, 74 }, { 13, 1, 72 }, { 14, 2, 71 } }, 100);
            if (rig.name != "Night Cruise")
                wheel (8.0, 14.0, 100);
            length = 19.0 * beat;
        }
        else if (night && type == "Pad")
        {
            const std::initializer_list<int> chords[] = { { 45, 57, 60, 64 }, { 41, 57, 60, 65 }, { 48, 55, 60, 64 }, { 43, 55, 59, 62 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                chord (e, at (c * 4), at (c * 4 + 3.9), notes, 95);
                ++c;
            }
            if (rig.name == "Midnight Strings")
                pressure (8.0, 15.0, 100);
            length = 21.0 * beat;
        }
        else if (night && type == "Bass")
        {
            const int roots[] = { 33, 29, 36, 31 };
            if (rig.name == "Octave Drive")
                for (int c = 0; c < 4; ++c)
                    chord (e, at (c * 4), at (c * 4 + 3.9), { roots[c] }, 110);
            else
                for (int c = 0; c < 4; ++c)
                    for (const auto [on, len] : { std::pair { 0.0, 1.4 }, std::pair { 1.5, 0.4 }, std::pair { 2.0, 1.4 }, std::pair { 3.5, 0.4 } })
                        chord (e, at (c * 4 + on), at (c * 4 + on + len), { roots[c] + (on == 3.5 ? 12 : 0) }, on == 0.0 ? 110 : 95);
            length = 18.0 * beat;
        }
        else if (night && type == "Keys")
        {
            const std::initializer_list<int> chords[] = { { 57, 60, 64, 69 }, { 53, 57, 60, 65 }, { 55, 60, 64, 67 }, { 55, 59, 62, 67 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                if (rig.name == "Retro Stab")
                    for (const auto offset : { 0.0, 1.5, 3.0 })
                        chord (e, at (c * 4 + offset), at (c * 4 + offset + 0.6), notes, offset == 0.0 ? 110 : 95);
                else
                {
                    chord (e, at (c * 4), at (c * 4 + 1.9), notes, 95);
                    chord (e, at (c * 4 + 2), at (c * 4 + 3.9), notes, 80);
                }
                ++c;
            }
            length = 20.0 * beat;
        }
        else if (night)
        {
            chord (e, at (0), at (7.9), { 57, 60, 64 }, 100);
            chord (e, at (8), at (15.9), { 53, 57, 60 }, 100);
            length = 20.0 * beat;
        }
        else if (negative && type == "Keys")
        {
            // A quiet, lonely piano: D minor, B flat, F, A in the left hand, a sparse falling line over it, the pedal
            // changed with each chord; softer than LAST LIGHT's clip.
            const int bass[] = { 38, 34, 41, 33 };
            const std::initializer_list<int> hands[] = { { 50, 53, 57 }, { 50, 53, 58 }, { 48, 53, 57 }, { 49, 52, 57 } };
            for (int c = 0; c < 4; ++c)
            {
                chord (e, at (c * 4), at (c * 4 + 3.9), { bass[c] }, 55);
                int k = 0;
                for (const auto n : hands[c])
                    chord (e, at (c * 4 + 1 + k * 0.5), at (c * 4 + 1.4 + k++ * 0.5), { n }, 45);
                pedal (c * 4 + 0.05, c * 4 + 3.95);
            }
            line ({ { 0, 2, 69 }, { 2, 1, 65 }, { 3, 1, 64 }, { 4, 3, 62 }, { 7, 1, 65 }, { 8, 2, 69 }, { 10, 1, 70 },
                    { 11, 1, 69 }, { 12, 4, 64 } }, 65);
            pedal (16.05, 19.0);
            if (rig.name == "Soft Felt Piano")
                wheel (12.0, 16.0, 90);
            length = 22.0 * beat;
        }
        else if (negative && type == "Lead")
        {
            line ({ { 0, 2, 62 }, { 2, 1, 65 }, { 3, 1, 64 }, { 4, 3, 62 }, { 7, 1, 57 }, { 8, 2, 58 }, { 10, 1, 57 },
                    { 11, 1, 55 }, { 12, 4, 57 } }, 90);
            if (rig.name == "Hollow Signal")
                wheel (8.0, 14.0, 110);
            length = 20.0 * beat;
        }
        else if (negative && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 38, 50, 53, 57 }, 90);   // D minor
            chord (e, at (8), at (15.8), { 34, 50, 53, 58 }, 90);  // B flat
            length = 24.0 * beat;
        }
        else if (negative && type == "Bass")
        {
            if (rig.name == "Pulse Bass")
            {
                chord (e, at (0), at (7.9), { 38 }, 110);
                chord (e, at (8), at (15.9), { 34 }, 110);
            }
            else
                line ({ { 0, 4, 26 }, { 4, 4, 22 }, { 8, 4, 29 }, { 12, 4, 21 } }, 105);
            length = 20.0 * beat;
        }
        else if (negative)
        {
            chord (e, at (0), at (7.9), { 62, 65, 69 }, 95);
            chord (e, at (8), at (15.9), { 58, 62, 65 }, 95);
            length = 20.0 * beat;
        }
        else if (dream && type == "Lead")
        {
            // G minor: G Bb D, the flat sixth (Eb) and the raised seventh (F#) for the pull.
            line ({ { 0, 2, 67 }, { 2, 1, 70 }, { 3, 1, 69 }, { 4, 3, 67 }, { 7, 1, 66 }, { 8, 2, 67 }, { 10, 1, 63 },
                    { 11, 1, 62 }, { 12, 4, 62 } }, 95);
            if (rig.name == "Fading Horn")
                pressure (8.0, 12.0, 110);
            length = 20.0 * beat;
        }
        else if (dream && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 43, 55, 58, 62 }, 95);   // G minor
            chord (e, at (8), at (15.8), { 39, 55, 58, 63 }, 95);  // E flat
            if (rig.name == "Folding City")
                wheel (6.0, 15.0, 127);
            length = 24.0 * beat;
        }
        else if (dream && type == "Bass")
        {
            if (rig.name == "Colossus Blast")
            {
                chord (e, at (0), at (3.5), { 31 }, 120);
                chord (e, at (8), at (11.5), { 27 }, 120);
                wheel (8.0, 12.0, 110);
            }
            else if (rig.name == "Ostinato Bass")
            {
                chord (e, at (0), at (7.9), { 31 }, 110);
                chord (e, at (8), at (15.9), { 27 }, 110);
            }
            else
                line ({ { 0, 6, 31 }, { 8, 6, 27 } }, 105);
            length = 20.0 * beat;
        }
        else if (dream && type == "Keys")
        {
            if (rig.name == "Echo Piano")
            {
                line ({ { 0, 2, 67 }, { 2, 2, 70 }, { 4, 2, 74 }, { 6, 2, 72 }, { 8, 3, 70 }, { 11, 1, 69 }, { 12, 4, 67 } }, 75);
                chord (e, at (0), at (7.9), { 43, 50 }, 60);
                chord (e, at (8), at (15.9), { 39, 46 }, 60);
                pedal (0.05, 7.95);
                pedal (8.05, 15.95);
            }
            else if (rig.name == "Glass Memory")
                line ({ { 0, 1, 79 }, { 1, 1, 82 }, { 2, 2, 86 }, { 4, 1, 84 }, { 5, 1, 82 }, { 6, 2, 81 }, { 8, 4, 79 } }, 90);
            else
            {
                chord (e, at (0), at (3.8), { 55, 58, 62 }, 90);
                chord (e, at (4), at (7.8), { 51, 55, 58 }, 90);
                chord (e, at (8), at (11.8), { 50, 54, 57 }, 90);
                chord (e, at (12), at (15.8), { 55, 58, 62 }, 90);
            }
            length = 20.0 * beat;
        }
        else if (dream)
        {
            if (rig.name == "Ostinato Strings")
                wheel (6.0, 15.0, 120);
            chord (e, at (0), at (7.9), { 55, 58, 62 }, 100);
            chord (e, at (8), at (15.9), { 51, 55, 58 }, 100);
            length = 20.0 * beat;
        }
        else if (sim && type == "Lead")
        {
            // E minor, the flat second (F) for menace.
            line ({ { 0, 1.5, 64 }, { 1.5, 0.5, 65 }, { 2, 2, 64 }, { 4, 1, 67 }, { 5, 1, 71 }, { 6, 2, 72 }, { 8, 1, 71 },
                    { 9, 1, 67 }, { 10, 1, 65 }, { 11, 1, 67 }, { 12, 4, 64 } }, 100);
            if (rig.name != "Phantom Line")
                wheel (8.0, 14.0, 110);
            length = 19.0 * beat;
        }
        else if (sim && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 40, 52, 55, 59 }, 95);   // E minor
            chord (e, at (8), at (15.8), { 41, 53, 57, 60 }, 95);  // F: the flat second
            length = 22.0 * beat;
        }
        else if (sim && type == "Bass")
        {
            if (rig.name == "Industrial Pulse")
            {
                chord (e, at (0), at (7.9), { 28 }, 110);
                chord (e, at (8), at (11.9), { 29 }, 110);
                chord (e, at (12), at (15.9), { 31 }, 110);
            }
            else
            {
                const int notes[] = { 28, 28, 31, 28, 29, 28, 35, 33 };
                for (int bar = 0; bar < 4; ++bar)
                    for (int k = 0; k < 8; ++k)
                        chord (e, at (bar * 4 + k * 0.5), at (bar * 4 + k * 0.5 + 0.4), { notes[k] }, k % 2 == 0 ? 110 : 90);
                if (rig.name == "Breakbeat Bass")
                    wheel (8.0, 16.0, 120);
            }
            length = 18.0 * beat;
        }
        else if (sim && type == "Keys")
        {
            const std::initializer_list<int> chords[] = { { 64, 67, 71 }, { 65, 69, 72 }, { 64, 67, 71 }, { 62, 65, 69 } };
            int c = 0;
            for (const auto& notes : chords)
            {
                for (const auto offset : { 0.0, 1.5, 3.0 })
                    chord (e, at (c * 4 + offset), at (c * 4 + offset + 1.0), notes, offset == 0.0 ? 105 : 85);
                ++c;
            }
            if (rig.name == "Warped Keys")
                wheel (6.0, 14.0, 127);
            length = 19.0 * beat;
        }
        else if (sim)
        {
            if (rig.name == "Rain Cascade")
                chord (e, at (0), at (15.9), { 64 }, 100);
            else if (rig.name == "System Breach")
                chord (e, at (0), at (15.9), { 64, 67 }, 105);
            else
                chord (e, at (0), at (15.9), { 64, 67, 71 }, 100);
            length = 19.0 * beat;
        }
        else if (last && type == "Keys" && rig.name != "Music Box Elegy")
        {
            // A slow, sad piano: Am, F, C, Em in the left hand, a falling melody over it, the pedal changed with each chord.
            const int bass[] = { 45, 41, 48, 40 };
            const std::initializer_list<int> hands[] = { { 57, 60, 64 }, { 57, 60, 65 }, { 55, 60, 64 }, { 55, 59, 64 } };
            for (int c = 0; c < 4; ++c)
            {
                chord (e, at (c * 4), at (c * 4 + 3.9), { bass[c] }, 70);
                int k = 0;
                for (const auto n : hands[c])
                    chord (e, at (c * 4 + 1 + k * 0.5), at (c * 4 + 1.4 + k++ * 0.5), { n }, 55);
                pedal (c * 4 + 0.05, c * 4 + 3.95);
            }
            line ({ { 0, 2, 76 }, { 2, 1, 74 }, { 3, 1, 72 }, { 4, 3, 72 }, { 7, 1, 71 }, { 8, 2, 72 }, { 10, 1, 74 },
                    { 11, 1, 76 }, { 12, 4, 71 } }, 80);
            pedal (16.05, 19.0);
            if (rig.name == "Felt Memory")
                wheel (12.0, 16.0, 90);
            length = 22.0 * beat;
        }
        else if (last && type == "Keys")
        {
            line ({ { 0, 0.5, 81 }, { 0.5, 0.5, 84 }, { 1, 0.5, 88 }, { 2, 1, 86 }, { 3, 1, 84 }, { 4, 0.5, 83 }, { 4.5, 0.5, 84 },
                    { 5, 1, 81 }, { 6, 2, 79 }, { 8, 0.5, 81 }, { 8.5, 0.5, 76 }, { 9, 3, 81 } }, 85);
            length = 16.0 * beat;
        }
        else if (last && type == "Lead")
        {
            line ({ { 0, 2, 69 }, { 2, 1, 72 }, { 3, 1, 71 }, { 4, 3, 69 }, { 7, 1, 64 }, { 8, 2, 65 }, { 10, 1, 64 },
                    { 11, 1, 62 }, { 12, 4, 64 } }, 90);
            if (rig.name == "Cello Drift")
                pressure (8.0, 12.0, 110);
            length = 20.0 * beat;
        }
        else if (last && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 45, 57, 60, 64 }, 90);   // A minor
            chord (e, at (8), at (15.8), { 41, 57, 60, 65 }, 90);  // F
            if (rig.name == "Cathedral Organ")
                wheel (2.0, 15.0, 127);
            if (rig.name == "Horizon Strings")
                pressure (8.0, 15.0, 100);
            length = 24.0 * beat;
        }
        else if (last && type == "Bass")
        {
            if (rig.name == "Heartbeat")
                for (int bar = 0; bar < 4; ++bar)
                {
                    chord (e, at (bar * 4), at (bar * 4 + 0.3), { 33 }, 110);
                    chord (e, at (bar * 4 + 0.6), at (bar * 4 + 0.9), { 33 }, 80);
                }
            else
                line ({ { 0, 4, 33 }, { 4, 4, 29 }, { 8, 4, 36 }, { 12, 4, 28 } }, 100);
            length = 20.0 * beat;
        }
        else if (last)
        {
            chord (e, at (0), at (7.9), { 57, 60, 64 }, 85);
            chord (e, at (8), at (15.9), { 53, 57, 60 }, 85);
            length = 20.0 * beat;
        }
        else if (data && type == "Lead")
        {
            line ({ { 0, 1, 64 }, { 1, 0.5, 67 }, { 1.5, 0.5, 69 }, { 2, 2, 71 }, { 4, 1, 74 }, { 5, 1, 71 }, { 6, 2, 69 },
                    { 8, 1, 67 }, { 9, 1, 69 }, { 10, 1, 71 }, { 11, 1, 74 }, { 12, 4, 76 } }, 100);
            length = 19.0 * beat;
        }
        else if (data && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 52, 59, 64, 67 }, 95);   // E minor
            chord (e, at (8), at (15.8), { 48, 55, 60, 64 }, 95);  // C
            length = 22.0 * beat;
        }
        else if (data && type == "Bass")
        {
            if (rig.name == "Pulse Engine")
            {
                chord (e, at (0), at (7.9), { 28 }, 105);
                chord (e, at (8), at (15.9), { 24 }, 105);
            }
            else if (rig.name == "Bitfall Bass")
            {
                const int notes[] = { 28, 28, 40, 28, 31, 28, 33, 31 };
                for (int bar = 0; bar < 4; ++bar)
                    for (int k = 0; k < 8; ++k)
                        chord (e, at (bar * 4 + k * 0.5), at (bar * 4 + k * 0.5 + 0.4), { notes[k] }, k % 2 == 0 ? 110 : 90);
                wheel (8.0, 15.0, 120);
            }
            else
                line ({ { 0, 4, 28 }, { 4, 4, 24 }, { 8, 4, 26 }, { 12, 4, 23 } }, 105);
            length = 18.0 * beat;
        }
        else if (data && type == "Keys")
        {
            if (rig.name == "Bit Chimes")
                line ({ { 0, 0.5, 76 }, { 0.75, 0.5, 79 }, { 1.5, 0.5, 83 }, { 3, 0.5, 81 }, { 4, 0.5, 79 }, { 4.75, 0.5, 76 },
                        { 5.5, 0.5, 74 }, { 7, 1, 76 } }, 90);
            else
            {
                const std::initializer_list<int> chords[] = { { 64, 67, 71 }, { 60, 64, 67 }, { 59, 62, 67 }, { 62, 66, 69 } };
                int c = 0;
                for (const auto& notes : chords)
                {
                    if (rig.name == "Brass Protocol")
                        for (const auto offset : { 0.0, 1.5, 2.5 })
                            chord (e, at (c * 4 + offset), at (c * 4 + offset + 0.7), notes, offset == 0.0 ? 115 : 95);
                    else
                        chord (e, at (c * 4), at (c * 4 + 3.8), notes, 95);
                    ++c;
                }
            }
            length = 19.0 * beat;
        }
        else if (data)
        {
            if (rig.name == "Circuit Run")
                chord (e, at (0), at (15.9), { 64 }, 100);
            else
                chord (e, at (0), at (15.9), { 64, 67, 71 }, 100);
            length = 19.0 * beat;
        }
        else if (sand && type == "Lead")
        {
            // D Phrygian dominant: D Eb F# G A Bb C.
            line ({ { 0, 1, 62 }, { 1, 0.5, 63 }, { 1.5, 0.5, 62 }, { 2, 2, 66 }, { 4, 1, 67 }, { 5, 1, 69 }, { 6, 2, 70 },
                    { 8, 1, 69 }, { 9, 1, 67 }, { 10, 1, 66 }, { 11, 1, 63 }, { 12, 4, 62 } }, 100);
            if (rig.name == "Desert Voice")
                wheel (6.0, 11.0, 110);
            length = 19.0 * beat;
        }
        else if (sand && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 38, 45, 50, 54 }, 95);   // D, a low fifth and the major third
            chord (e, at (8), at (15.8), { 39, 46, 51, 55 }, 95);  // E flat: the flat second
            length = 22.0 * beat;
        }
        else if (sand && type == "Bass")
        {
            if (rig.name == "Quake Pulse")
            {
                chord (e, at (0), at (3), { 26 }, 120);
                chord (e, at (8), at (11), { 27 }, 115);
            }
            else if (rig.name == "Iron Ritual")
            {
                const int notes[] = { 38, 38, 39, 38, 42, 38, 39, 38 };
                for (int bar = 0; bar < 4; ++bar)
                    for (int k = 0; k < 8; ++k)
                        chord (e, at (bar * 4 + k * 0.5), at (bar * 4 + k * 0.5 + 0.35), { notes[k] }, k % 2 == 0 ? 110 : 90);
            }
            else
                line ({ { 0, 6, 26 }, { 6, 2, 27 }, { 8, 6, 26 }, { 14, 2, 30 } }, 105);
            length = 18.0 * beat;
        }
        else if (sand && type == "Keys")
        {
            if (rig.name == "Ritual Gong")
            {
                chord (e, at (0), at (3), { 38 }, 115);
                chord (e, at (8), at (11), { 33 }, 110);
            }
            else
                line ({ { 0, 1, 62 }, { 1, 0.5, 63 }, { 1.5, 0.5, 66 }, { 2, 1, 67 }, { 3, 1, 69 }, { 4, 0.5, 70 }, { 4.5, 0.5, 69 },
                        { 5, 1, 67 }, { 6, 2, 66 }, { 8, 1, 63 }, { 9, 1, 62 }, { 10, 3, 62 } }, rig.name == "Copper Bells" ? 85 : 100);
            length = 19.0 * beat;
        }
        else if (sand)
        {
            if (rig.name == "Sand Ripple")
                chord (e, at (0), at (15.9), { 62 }, 100);
            else if (rig.name == "War Pulse")
                chord (e, at (0), at (15.9), { 50, 57 }, 105);
            else
                chord (e, at (0), at (15.9), { 50, 54, 57 }, 100);
            length = 19.0 * beat;
        }
        else if (star && type == "Lead")
        {
            line ({ { 0, 1, 60 }, { 1, 1, 63 }, { 2, 2, 67 }, { 4, 1, 65 }, { 5, 1, 63 }, { 6, 2, 62 }, { 8, 1, 60 }, { 9, 1, 63 },
                    { 10, 1, 67 }, { 11, 1, 70 }, { 12, 4, 72 } }, 100);
            length = 19.0 * beat;
        }
        else if (star && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 48, 55, 60, 63 }, 90);   // C minor
            chord (e, at (8), at (15.8), { 44, 56, 60, 63 }, 90);  // A flat
            length = 22.0 * beat;
        }
        else if (star && type == "Bass")
        {
            if (rig.name == "Eighth Note Pulse")
            {
                const int notes[] = { 36, 36, 48, 36, 36, 46, 36, 43 };
                for (int bar = 0; bar < 4; ++bar)
                    for (int k = 0; k < 8; ++k)
                        chord (e, at (bar * 4 + k * 0.5), at (bar * 4 + k * 0.5 + 0.4), { notes[k] - (bar == 2 ? 4 : 0) }, k % 2 == 0 ? 110 : 90);
            }
            else
                line ({ { 0, 2, 36 }, { 2, 2, 39 }, { 4, 3, 32 }, { 7, 1, 34 }, { 8, 2, 36 }, { 10, 2, 43 }, { 12, 4, 36 } }, 100);
            length = 18.0 * beat;
        }
        else if (star && type == "Keys")
        {
            {
                const std::initializer_list<int> chords[] = { { 60, 63, 67 }, { 56, 60, 63 }, { 55, 58, 63 }, { 58, 62, 65 } };
                int c = 0;
                for (const auto& notes : chords)
                {
                    if (rig.name == "Brass '84" || rig.name == "Neon Stabs")
                        for (const auto offset : { 0.0, 1.5, 2.5 })
                            chord (e, at (c * 4 + offset), at (c * 4 + offset + 0.6), notes, offset == 0.0 ? 115 : 95);
                    else
                        chord (e, at (c * 4), at (c * 4 + 3.8), notes, 95);
                    ++c;
                }
            }
            length = 19.0 * beat;
        }
        else if (star)
        {
            chord (e, at (0), at (15.9), { 60, 63, 67 }, 100);
            length = 19.0 * beat;
        }
        else if (hell && type == "Lead")
        {
            line ({ { 0, 0.5, 50 }, { 0.5, 0.5, 50 }, { 1, 0.5, 53 }, { 1.5, 0.25, 55 }, { 1.75, 0.25, 56 }, { 2, 1.5, 57 },
                    { 3.5, 0.5, 56 }, { 4, 1, 55 }, { 5, 1, 53 }, { 6, 2, 50 }, { 8, 0.5, 62 }, { 8.5, 0.5, 60 }, { 9, 0.5, 58 },
                    { 9.5, 0.5, 57 }, { 10, 2, 56 }, { 12, 4, 57 } }, 110);
            if (rig.name == "Hellfire Lead" || rig.name == "Whammy Wraith")
                for (int k = 0; k <= 16; ++k)   // a whammy dive on the last note
                    e.push_back ({ at (13.5 + k * 0.1), juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, 8192 - k * 512)) });
            else
                wheel (8.0, 12.0, 110);
            e.push_back ({ at (16.5), juce::MidiMessage::pitchWheel (1, 8192) });
            length = 19.0 * beat;
        }
        else if (hell && type == "Pad")
        {
            chord (e, at (0), at (7.8), { 38, 45, 50, 53 }, 100);   // D minor, low
            chord (e, at (8), at (15.8), { 39, 46, 51, 55 }, 100);  // E flat: the dark half-step up
            length = 22.0 * beat;
        }
        else if (hell && type == "Bass")
        {
            if (rig.name == "Tectonic Drop")
            {
                chord (e, at (0), at (3), { 26 }, 120);
                chord (e, at (8), at (11), { 21 }, 120);
                length = 18.0 * beat;
            }
            else if (rig.name == "Gut Chug" || rig.name == "Sine Engine")
            {
                // A syncopated sixteenth-note chug on low D, a half-step up and the octave for accents.
                const int bar[16] = { 26, 26, 26, 0, 26, 26, 0, 27, 26, 26, 26, 0, 26, 0, 38, 38 };
                for (int b = 0; b < 4; ++b)
                    for (int k = 0; k < 16; ++k)
                        if (bar[k] != 0 && ! (b == 3 && k >= 12))
                            chord (e, at (b * 4 + k * 0.25), at (b * 4 + k * 0.25 + 0.2), { bar[k] }, k % 4 == 0 ? 120 : 95);
                chord (e, at (15), at (16), { 26 }, 120);
                length = 18.0 * beat;
            }
            else
            {
                line ({ { 0, 4, 26 }, { 4, 2, 29 }, { 6, 2, 27 }, { 8, 4, 26 }, { 12, 2, 31 }, { 14, 2, 29 } }, 110);
                length = 18.0 * beat;
            }
        }
        else if (hell && type == "Keys")
        {
            if (rig.name == "Anvil Stab")
            {
                // Stabs: short chords on a syncopated rhythm, D minor then E flat, the last let ring.
                const double hits[] = { 0, 0.75, 1.5, 2.5, 3.0 };
                for (int bar = 0; bar < 4; ++bar)
                    for (const auto h : hits)
                    {
                        if (bar % 2 == 0)
                            chord (e, at (bar * 4 + h), at (bar * 4 + h + 0.2), { 50, 53, 57 }, h == 0 ? 120 : 100);
                        else
                            chord (e, at (bar * 4 + h), at (bar * 4 + h + 0.2), { 51, 55, 58 }, h == 0 ? 120 : 100);
                    }
                chord (e, at (16), at (17.5), { 50, 53, 57 }, 120);
            }
            else if (rig.name == "Iron Riff" && juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER_STYLE", {}) == "eighths")
            {
                // PARAGON_RENDER_STYLE=eighths: straight eighth notes, one finger, the power chords changing each half bar.
                const int bars[4][8] = { { 38, 38, 38, 38, 38, 38, 38, 38 }, { 38, 38, 38, 38, 41, 41, 43, 43 },
                                         { 39, 39, 39, 39, 39, 39, 39, 39 }, { 38, 38, 38, 38, 36, 36, 38, 38 } };
                for (int bar = 0; bar < 4; ++bar)
                    for (int k = 0; k < 8; ++k)
                        chord (e, at (bar * 4 + k * 0.5), at (bar * 4 + k * 0.5 + 0.4), { bars[bar][k] }, k % 2 == 0 ? 115 : 100);
                chord (e, at (16), at (18), { 38 }, 120);   // the last chord let ring
            }
            else if (rig.name == "Iron Riff")
            {
                // One finger: palm-muted chugs on low D, then power chords let ring.
                for (int bar = 0; bar < 4; ++bar)
                {
                    const auto b0 = bar * 4.0;
                    for (const auto k : { 0.0, 0.25, 0.5, 1.0, 1.25 })
                        chord (e, at (b0 + k), at (b0 + k + 0.18), { 38 }, k == 0.0 ? 120 : 95);
                    const int ring[] = { 41, 43, 39, 36 };
                    chord (e, at (b0 + 1.5), at (b0 + 2.9), { ring[bar] }, 115);
                    for (const auto k : { 3.0, 3.25, 3.5 })
                        chord (e, at (b0 + k), at (b0 + k + 0.18), { 38 }, 95);
                }
            }
            else
            {
                const std::initializer_list<int> chords[] = { { 50, 53, 57 }, { 46, 50, 53 }, { 48, 52, 55 }, { 45, 49, 52 } };
                int c = 0;
                for (const auto& notes : chords)
                {
                    chord (e, at (c * 4), at (c * 4 + 3.8), notes, 100);
                    ++c;
                }
            }
            length = 19.0 * beat;
        }
        else if (hell)
        {
            if (rig.name == "Gatling Pulse" || rig.name == "Chainsaw Pulse")
                chord (e, at (0), at (15.9), { 38 }, 110);
            else
                chord (e, at (0), at (15.9), { 50, 53, 57 }, 100);
            length = 19.0 * beat;
        }
        else if (type == "Lead")
        {
            // A slow, rising-and-falling line in A minor, the long notes overlapping a little (legato, glide).
            const N leadLine[] = { { 0, 3.1, 64 }, { 3, 1.1, 67 }, { 4, 2.1, 69 }, { 6, 2.1, 67 }, { 8, 3.1, 72 }, { 11, 1.1, 71 },
                               { 12, 4.0, 69 } };
            for (const auto& n : leadLine)
                chord (e, at (n.on), at (n.on + n.len), { n.note }, 96);
            if (rig.name == "Smoke And Reeds")
                wheel (8.0, 12.0, 110);
            else
                pressure (8.5, 11.0, 100);
            length = 20.0 * beat;
        }
        else if (type == "Pad")
        {
            chord (e, at (0), at (7.8), { 45, 57, 60, 64 }, 90);     // Am
            chord (e, at (8), at (15.8), { 41, 57, 60, 65 }, 90);    // F/A over F
            pressure (9.0, 14.0, 90);
            length = 24.0 * beat;
        }
        else if (type == "Bass")
        {
            if (rig.name == "Distant Thunder")
            {
                chord (e, at (0), at (3.5), { 33 }, 120);
                chord (e, at (8), at (11.5), { 29 }, 110);
                length = 20.0 * beat;
            }
            else
            {
                const N bassLine[] = { { 0, 1.5, 33 }, { 1.5, 0.5, 33 }, { 2, 1.0, 45 }, { 3, 1.0, 43 }, { 4, 1.5, 41 },
                                   { 5.5, 0.5, 41 }, { 6, 1.0, 40 }, { 7, 1.0, 38 }, { 8, 1.5, 33 }, { 9.5, 0.5, 36 },
                                   { 10, 1.05, 40 }, { 11, 1.0, 43 }, { 12, 3.5, 33 } };
                for (const auto& n : bassLine)
                    chord (e, at (n.on), at (n.on + n.len), { n.note }, 100);
                length = 18.0 * beat;
            }
        }
        else if (type == "Keys")
        {
            chord (e, at (0), at (3.8), { 57, 60, 64, 67 }, 90);          // Am7
            chord (e, at (4), at (7.8), { 53, 57, 60, 64 }, 80);          // Fmaj7
            chord (e, at (8), at (11.8), { 50, 53, 57, 60 }, 95);         // Dm7
            chord (e, at (12), at (13.8), { 52, 55, 59, 62 }, 85);        // Em7
            chord (e, at (14), at (15.8), { 52, 56, 59, 62 }, 100);       // E7
            if (rig.name == "Glass Chimes")
            {
                e.clear();
                const int notes[] = { 76, 72, 69, 71, 64, 67, 69, 72 };
                for (int k = 0; k < 8; ++k)
                    chord (e, at (k * 1.5), at (k * 1.5 + 1.0), { notes[k] }, 70 + 7 * (k % 3));
            }
            else if (rig.name == "Night Shift Keys")
                pressure (8.5, 11.5, 100);
            length = 22.0 * beat;
        }
        else
        {
            chord (e, at (0), at (15.9), { 57, 60, 64 }, 100);   // hold A minor
            length = 22.0 * beat;
        }

        std::vector<float> left, right;
        // PARAGON_RENDER_SWEEP=macro3: MACRO 3 up from a quarter of the clip to two thirds, then down again (to hear
        // DILATION slide into slow motion and come back, 0.63.0).
        std::function<void (double)> sweep;
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER_SWEEP", {}) == "macro3")
            sweep = [&p, length] (double seconds)
            {
                auto* macro = p.processor.getParameters().getParameter (ParamIDs::macros[2]);
                const auto up = seconds >= 0.25 * length && seconds < 0.66 * length;
                if (macro->getValue() != (up ? 1.0f : 0.0f))
                    macro->setValueNotifyingHost (up ? 1.0f : 0.0f);
            };
        p.play (length, e, &left, &right, nullptr, nullptr, sweep);
        juce::AudioBuffer<float> buffer (2, (int) left.size());
        for (size_t i = 0; i < left.size(); ++i)
        {
            buffer.setSample (0, (int) i, left[i]);
            buffer.setSample (1, (int) i, right[i]);
        }
        const auto file = folder.getChildFile (rig.getSlotName() + " " + rig.name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (32)
                                                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}

TEST_CASE ("The arp randomizer's added scales: Phrygian dominant, double harmonic and pelog", "[arp][expansions]")
{
    const auto notesOf = [] (const char* name)
    {
        const auto scale = synth::arp::scaleNames.indexOf (name);
        REQUIRE (scale >= 0);
        std::vector<int> notes;
        for (int n = 0; n < 12; ++n)
            if (synth::arp::inScale (scale, n))
                notes.push_back (n);
        return notes;
    };
    CHECK (notesOf ("Phrygian Dominant") == std::vector<int> { 0, 1, 4, 5, 7, 8, 10 });
    CHECK (notesOf ("Double Harmonic") == std::vector<int> { 0, 1, 4, 5, 7, 8, 11 });
    CHECK (notesOf ("Pelog") == std::vector<int> { 0, 1, 3, 7, 8 });
    CHECK (synth::arp::scaleNames.size() == (int) synth::arp::scaleMasks.size());
    CHECK (synth::arp::scaleNames.indexOf ("Whole Tone") == 13);   // the old ones keep their places
}

TEST_CASE ("Expansions: the browser's list scrolls, so every bank can be reached however many there are", "[expansions][rigs]")
{
    ParagonX1AudioProcessor processor;
    RigBrowser browser (processor);
    browser.setSize (1576, 670);   // as on the PRESETS page

    const auto& view = browser.getExpansionView();
    const auto count = (int) expansions::list.size();
    REQUIRE (view.getHeight() > 100);
    // Every button a full row, none squeezed.
    for (int k = 0; k < count; ++k)
        CHECK (browser.getExpansionButton (k)->getHeight() >= 20);

    // Choosing the last bank scrolls it into view.
    browser.showFilter (0, Rig::numBanks + count - 1);
    const auto last = browser.getExpansionButton (count - 1)->getBounds();
    CHECK (view.getViewArea().contains (last));
}

