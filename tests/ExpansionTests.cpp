// The built-in EXPANSIONS: themed banks of 15 presets after the user's banks A-E (rigs/Expansions.h,
// rigs/ExpansionRigs.cpp). Their banks and slot names, that a user's preset is never filed in one, that every
// preset's settings and routes are real ones, and that each plays: in time with a song, at a sensible level, clean,
// and silent once its notes and tails have ended. "[.render]" (hidden) writes a clip of each for listening.
#include <catch2/catch_test_macros.hpp>

#include "ModDestinations.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "rigs/Expansions.h"
#include "rigs/FactoryRigs.h"

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
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setIsPlaying (true);
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
            processor.seedRandomForTest (1);
            song.ppq = 0.0;
        }
        // Plays `seconds` with MIDI events (seconds, message) into `out` (left, right), the song running.
        void play (double seconds, std::vector<std::pair<double, juce::MidiMessage>> events, std::vector<float>* left = nullptr,
                   std::vector<float>* right = nullptr, float* loudest = nullptr, bool* finite = nullptr)
        {
            std::sort (events.begin(), events.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
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
        for (const auto* type : { "Lead", "Pad", "Bass", "Keys", "Arp" })
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
        REQUIRE (rigs.size() == 15);
        std::map<juce::String, int> perType;
        for (size_t i = 0; i < rigs.size(); ++i)
        {
            INFO (rigs[i].name);
            CHECK (rigs[i].number == (int) i + 1);
            CHECK (rigs[i].getSlotName() == juce::String (expansions::list[(size_t) x].code) + juce::String (i + 1).paddedLeft ('0', 2));
            CHECK (rigs[i].tags.contains (expansions::list[(size_t) x].name));
            ++perType[typeOf (rigs[i])];
        }
        for (const auto* type : { "Lead", "Pad", "Bass", "Keys", "Arp" })
            CHECK (perType[type] == 3);   // 3 leads, 3 pads, 3 basses, 3 keys, 3 arps
    }
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
    CHECK (saved.rig.getSlotName() == "A43");

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
    p.processor.loadRig (p.entryNamed ("Tight Kit"));
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
        if (only.isNotEmpty() && ! rig.name.containsIgnoreCase (only))
            continue;
        p.load (rig.name);
        // NEON RAIN at 85 BPM in A minor; HELLFORGE at 140 in D, heavier and rhythmic.
        const auto hell = rig.bank == Rig::numBanks + 1;
        p.song.bpm = hell ? 140.0 : 85.0;
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
        if (hell && type == "Lead")
        {
            line ({ { 0, 0.5, 50 }, { 0.5, 0.5, 50 }, { 1, 0.5, 53 }, { 1.5, 0.25, 55 }, { 1.75, 0.25, 56 }, { 2, 1.5, 57 },
                    { 3.5, 0.5, 56 }, { 4, 1, 55 }, { 5, 1, 53 }, { 6, 2, 50 }, { 8, 0.5, 62 }, { 8.5, 0.5, 60 }, { 9, 0.5, 58 },
                    { 9.5, 0.5, 57 }, { 10, 2, 56 }, { 12, 4, 57 } }, 110);
            if (rig.name == "Molten Riff")
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
            else if (rig.name == "Gut Chug")
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
            if (rig.name == "Ritual Bells")
                line ({ { 0, 1, 62 }, { 1.5, 1, 61 }, { 3, 1, 57 }, { 4.5, 1, 58 }, { 6, 1, 62 }, { 7.5, 1, 65 }, { 9, 1, 64 }, { 10.5, 2, 61 } }, 90);
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
            if (rig.name == "Gatling Pulse")
                chord (e, at (0), at (15.9), { 38 }, 110);
            else if (rig.name == "War Machine")
                chord (e, at (0), at (15.9), { 50 }, 110);
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
        p.play (length, e, &left, &right);
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
