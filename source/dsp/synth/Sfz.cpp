#include "dsp/synth/Sfz.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace synth::sfz
{
    namespace
    {
        using Opcodes = std::map<juce::String, juce::String>;

        // Ported from PR7-D 0.19.0's Instrument.cpp (noteNumber, preprocess, parse): c4 = 60, SFZ's middle C.
        int noteNumber (const juce::String& text)
        {
            const auto t = text.trim().toLowerCase();
            if (t.isEmpty())
                return -1;
            if (juce::CharacterFunctions::isDigit (t[0]) || t[0] == '-')
                return t.getIntValue();
            const juce::String letters ("c d ef g a b");
            const auto pos = letters.indexOfChar (t[0]);
            if (pos < 0 || t[0] == ' ')
                return -1;
            auto i = 1, semis = pos;
            if (i < t.length() && t[i] == '#') { ++semis; ++i; }
            else if (i < t.length() && t[i] == 'b') { --semis; ++i; }
            const auto octave = t.substring (i).getIntValue();
            return juce::jlimit (0, 127, (octave + 1) * 12 + semis);
        }

        // The file's text, comments out, #include read in (up to 8 deep), #define put in.
        juce::String preprocess (const juce::File& file, std::map<juce::String, juce::String>& defines, int depth,
                                 juce::StringArray& problems)
        {
            if (depth > 8 || ! file.existsAsFile())
            {
                problems.add ("missing: " + file.getFullPathName());
                return {};
            }
            const auto text = file.loadFileAsString();
            juce::String out;
            out.preallocateBytes ((size_t) text.getNumBytesAsUTF8() + 16);
            for (int i = 0; i < text.length();)
            {
                if (text[i] == '/' && i + 1 < text.length() && text[i + 1] == '*')
                {
                    const auto end = text.indexOf (i + 2, "*/");
                    i = end < 0 ? text.length() : end + 2;
                }
                else if (text[i] == '/' && i + 1 < text.length() && text[i + 1] == '/')
                {
                    while (i < text.length() && text[i] != '\n')
                        ++i;
                }
                else
                    out << text[i++];
            }
            // The defines put into a line: longest names first, so $KIT2 isn't read as $KIT then 2.
            const auto expand = [&defines] (juce::String line)
            {
                if (defines.empty() || ! line.containsChar ('$'))
                    return line;
                std::vector<std::pair<juce::String, juce::String>> sorted (defines.begin(), defines.end());
                std::sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
                for (const auto& [k, v] : sorted)
                    line = line.replace (k, v);
                return line;
            };
            juce::String result;
            for (auto line : juce::StringArray::fromLines (out))
            {
                const auto trimmed = line.trim();
                if (trimmed.startsWith ("#define"))
                {
                    const auto rest = trimmed.substring (7).trim();
                    defines[rest.upToFirstOccurrenceOf (" ", false, false).trim()]
                        = rest.fromFirstOccurrenceOf (" ", false, false).trim();
                    continue;
                }
                if (trimmed.startsWith ("#include"))
                {
                    // 0.76.0: the name's defines put in too ("$DIR/$DYN.txt": SSO's grand piano includes its velocity
                    // layers so, and loaded nothing before).
                    const auto name = expand (trimmed.fromFirstOccurrenceOf ("\"", false, false).upToLastOccurrenceOf ("\"", false, false));
                    result << preprocess (file.getParentDirectory().getChildFile (name.replaceCharacter ('\\', '/')),
                                          defines, depth + 1, problems) << "\n";
                    continue;
                }
                line = expand (line);
                result << line << "\n";
            }
            return result;
        }

        // The headers and their opcodes, each region with everything it inherits.
        std::vector<Opcodes> parse (const juce::String& text, juce::String& defaultPath, Opcodes* controlOut = nullptr)
        {
            std::vector<Opcodes> regions;
            Opcodes control, global, master, group, current;
            juce::String header;
            const auto flush = [&]
            {
                if (header == "region")
                {
                    Opcodes r;
                    for (const auto* level : { &global, &master, &group, &current })
                        for (const auto& [k, v] : *level)
                            r[k] = v;
                    regions.push_back (std::move (r));
                }
                else if (header == "control")
                    for (const auto& [k, v] : current)   // (each included file has its own: keep them all)
                        control[k] = v;
                else if (header == "global")
                    global = current;
                else if (header == "master")
                    master = current;
                else if (header == "group")
                    group = current;
                current.clear();
            };
            for (const auto& line : juce::StringArray::fromLines (text))
            {
                int i = 0;
                const auto n = line.length();
                while (i < n)
                {
                    while (i < n && juce::CharacterFunctions::isWhitespace (line[i]))
                        ++i;
                    if (i >= n)
                        break;
                    if (line[i] == '<')
                    {
                        const auto end = line.indexOfChar (i, '>');
                        if (end < 0)
                            break;
                        flush();
                        header = line.substring (i + 1, end).trim().toLowerCase();
                        if (header == "master")
                            group.clear();
                        if (header == "global")
                            master.clear(), group.clear();
                        i = end + 1;
                        continue;
                    }
                    const auto eq = line.indexOfChar (i, '=');
                    if (eq < 0)
                        break;
                    const auto key = line.substring (i, eq).trim().toLowerCase();
                    // The value: to the next "name=" or '<' on the line (a sample's path may have spaces).
                    const auto j = eq + 1;
                    auto valueEnd = n;
                    for (auto k = j; k < n; ++k)
                    {
                        if (line[k] == '<')
                        {
                            valueEnd = k;
                            break;
                        }
                        if (line[k] == '=')
                        {
                            auto back = k - 1;
                            while (back > j && ! juce::CharacterFunctions::isWhitespace (line[back]))
                                --back;
                            valueEnd = back;
                            break;
                        }
                    }
                    current[key] = line.substring (j, valueEnd).trim();
                    i = juce::jmax (valueEnd, j);
                }
            }
            flush();
            defaultPath = control.count ("default_path") ? control["default_path"] : juce::String();
            if (controlOut != nullptr)
                *controlOut = control;
            return regions;
        }

        juce::String get (const Opcodes& op, const char* key, const juce::String& otherwise = {})
        {
            const auto it = op.find (key);
            return it != op.end() ? it->second : otherwise;
        }
        bool has (const Opcodes& op, const char* key) { return op.find (key) != op.end(); }
        float number (const Opcodes& op, const char* key, float otherwise)
        {
            const auto it = op.find (key);
            return it != op.end() ? it->second.getFloatValue() : otherwise;
        }

        // Opcodes worth telling the user we don't play (their sound is shaped by the synth's own sections instead).
        const char* const leftOut[] = { "ampeg_attack", "ampeg_decay", "ampeg_sustain", "off_by", "group", "amplfo_depth" };

        // A sample read whole into memory (a looped one: its loop must be there to go back to).
        void readWhole (Sample& s, juce::AudioFormatManager& formats)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (s.file));
            if (reader == nullptr || s.length > (juce::int64) std::numeric_limits<int>::max())
                return;
            const auto frames = (int) s.length;
            const auto channels = juce::jlimit (1, 2, (int) reader->numChannels);
            juce::AudioBuffer<float> buffer (channels, frames);
            reader->read (&buffer, 0, frames, 0, true, channels > 1);
            for (int c = 0; c < 2; ++c)
            {
                const auto* from = buffer.getReadPointer (juce::jmin (c, channels - 1));
                s.head[c].assign (from, from + frames);
            }
            s.headFrames = frames;
            s.stream.reset();
        }
    }

    int Instrument::select (int key, int velocity127, bool release, int seqCount, float chance, int pedal,
                            const Region** out, int capacity, int switchKey) const noexcept
    {
        int n = 0;
        for (const auto& r : regions)
        {
            if (n >= capacity)
                break;
            if (r.pedalTrigger || r.release != release || ! r.plays (key, velocity127) || pedal < r.loPedal || pedal > r.hiPedal)
                continue;
            if (r.crossfade (velocity127) <= 0.0f)   // faded out at this velocity (0.73.0)
                continue;
            if (r.switchKey >= 0 && r.switchKey != switchKey)   // another key switch's (0.75.0)
                continue;
            if (r.seqLength > 1 && (seqCount % r.seqLength) + 1 != r.seqPosition)
                continue;
            if (chance < r.loRand || chance >= r.hiRand)   // chance is under 1, so HIRAND 1 always passes
                continue;
            out[n++] = &r;
        }
        return n;
    }

    int Instrument::selectPedal (int pedal, float chance, const Region** out, int capacity) const noexcept
    {
        int n = 0;
        for (const auto& r : regions)
            if (n < capacity && r.pedalTrigger && pedal >= r.onLo && pedal <= r.onHi && chance >= r.loRand && chance < r.hiRand)
                out[n++] = &r;
        return n;
    }

    std::shared_ptr<const Instrument> load (const juce::File& sfzFile, std::function<void (float)> progress,
                                            const std::atomic<bool>* cancel)
    {
        auto inst = std::make_shared<Instrument>();
        inst->file = sfzFile;
        inst->name = sfzFile.getFileNameWithoutExtension();

        std::map<juce::String, juce::String> defines;
        juce::String defaultPath;
        Opcodes control;
        const auto opcodes = parse (preprocess (sfzFile, defines, 0, inst->problems), defaultPath, &control);
        if (const auto it = control.find ("set_cc1"); it != control.end())
            inst->setCc1 = juce::jlimit (0, 127, it->second.getIntValue());
        // Every other controller's starting value (0.82.0): SET_CCn, 0 where not given. Only the mod wheel and the pedal
        // move live; the rest stay where the file sets them, and the regions are chosen and levelled by those values.
        std::array<int, 128> ccStart {};
        for (const auto& [name, value] : control)
            if (name.startsWith ("set_cc"))
                if (const auto cc = name.fromFirstOccurrenceOf ("set_cc", false, false).getIntValue(); cc >= 0 && cc < 128)
                    ccStart[(size_t) cc] = juce::jlimit (0, 127, value.getIntValue());
        const auto folder = sfzFile.getParentDirectory();

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::map<juce::String, int> sampleIndex;            // path -> index, -1 if it didn't load
        std::set<juce::String> leftOutSeen;

        for (size_t k = 0; k < opcodes.size(); ++k)
        {
            if (cancel != nullptr && cancel->load())
                return nullptr;
            if (progress)
                progress ((float) k / (float) juce::jmax<size_t> (1, opcodes.size()));
            const auto& op = opcodes[k];
            const auto samplePath = get (op, "sample");
            if (samplePath.isEmpty() || samplePath.startsWithChar ('*'))   // "*sine" and the like: generated, not read
                continue;
            // LOCCn / HICCn for controllers other than the wheel and the pedal (0.82.0): a region outside the range at
            // the controller's starting value never plays (a mixer's channel turned off, another articulation): left out
            // before its sample is read.
            {
                auto outOfRange = false;
                for (const auto& [name, value] : op)
                {
                    const auto low = name.startsWith ("locc"), high = name.startsWith ("hicc");
                    if (! low && ! high)
                        continue;
                    const auto cc = name.substring (4).getIntValue();
                    if (cc == 1 || cc == 64 || cc < 0 || cc > 127 || ! name.substring (4).containsOnly ("0123456789"))
                        continue;
                    const auto limit = value.getIntValue();
                    outOfRange = outOfRange || (low ? ccStart[(size_t) cc] < limit : ccStart[(size_t) cc] > limit);
                }
                if (outOfRange)
                    continue;
            }
            for (const auto* name : leftOut)
                if (has (op, name) && leftOutSeen.insert (name).second)
                    inst->problems.add ("not played: " + juce::String (name));

            // LOOP_MODE (0.73.0): what the region asks for; without it, the file's own loop if it has one.
            const auto loopMode = get (op, "loop_mode", get (op, "loopmode")).trim().toLowerCase();
            const auto relative = (defaultPath + samplePath).replaceCharacter ('\\', '/');
            const auto file = folder.getChildFile (relative);
            const auto path = file.getFullPathName();
            int index = -1;
            if (const auto it = sampleIndex.find (path); it != sampleIndex.end())
                index = it->second;
            else
            {
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
                if (reader != nullptr && reader->lengthInSamples > 0)
                {
                    auto s = std::make_unique<Sample>();
                    s->file = file;
                    s->sampleRate = reader->sampleRate > 0 ? reader->sampleRate : 48000.0;
                    s->length = reader->lengthInSamples;
                    s->headFrames = (int) juce::jmin<juce::int64> (s->length, (juce::int64) std::ceil (headSeconds * s->sampleRate));
                    if (reader->metadataValues.getValue ("NumSampleLoops", "0").getIntValue() > 0)
                    {
                        s->fileLoopStart = reader->metadataValues.getValue ("Loop0Start", "-1").getLargeIntValue();
                        s->fileLoopEnd = reader->metadataValues.getValue ("Loop0End", "-1").getLargeIntValue();
                    }
                    const auto channels = juce::jlimit (1, 2, (int) reader->numChannels);
                    juce::AudioBuffer<float> buffer (channels, s->headFrames);
                    reader->read (&buffer, 0, s->headFrames, 0, true, channels > 1);
                    for (int c = 0; c < 2; ++c)
                    {
                        const auto* from = buffer.getReadPointer (juce::jmin (c, channels - 1));
                        s->head[c].assign (from, from + s->headFrames);
                    }
                    if (s->length > s->headFrames)
                    {
                        s->stream = std::make_unique<StreamSource>();
                        s->stream->file = file;
                        s->stream->sampleRate = s->sampleRate;
                        s->stream->length = s->length;
                        s->stream->headFrames = s->headFrames;
                        s->stream->numChannels = channels;
                        inst->streamed = true;
                    }
                    index = (int) inst->samples.size();
                    inst->samples.push_back (std::move (s));
                }
                else
                    inst->problems.add ("couldn't read: " + relative);
                sampleIndex[path] = index;
            }
            if (index < 0)
                continue;

            Region r;
            r.sample = index;
            {
                auto& sample = *inst->samples[(size_t) index];
                const auto fileLoops = sample.fileLoopStart >= 0 && sample.fileLoopEnd > sample.fileLoopStart;
                if (loopMode == "loop_continuous" || loopMode == "loop_sustain" || (loopMode.isEmpty() && fileLoops))
                {
                    r.loop = loopMode == "loop_sustain" ? Region::Loop::sustain : Region::Loop::continuous;
                    r.loopStart = has (op, "loop_start") ? get (op, "loop_start").getLargeIntValue()
                                : has (op, "loopstart") ? get (op, "loopstart").getLargeIntValue()
                                : fileLoops ? sample.fileLoopStart : 0;
                    r.loopEnd = has (op, "loop_end") ? get (op, "loop_end").getLargeIntValue()
                              : has (op, "loopend") ? get (op, "loopend").getLargeIntValue()
                              : fileLoops ? sample.fileLoopEnd : sample.length - 1;
                    r.loopStart = juce::jlimit<juce::int64> (0, juce::jmax<juce::int64> (0, sample.length - 1), r.loopStart);
                    r.loopEnd = juce::jlimit<juce::int64> (0, juce::jmax<juce::int64> (0, sample.length - 1), r.loopEnd);
                    if (r.loops() && sample.headFrames < sample.length)
                    {
                        readWhole (sample, formats);
                        if (sample.headFrames < sample.length)
                            r.loop = Region::Loop::none;   // (too long to hold: plays through)
                    }
                }
            }
            r.xfinLo = juce::jlimit (0, 127, get (op, "xfin_lovel", "0").getIntValue());
            r.xfinHi = juce::jlimit (0, 127, get (op, "xfin_hivel", "0").getIntValue());
            r.xfoutLo = juce::jlimit (0, 127, get (op, "xfout_lovel", "127").getIntValue());
            r.xfoutHi = juce::jlimit (0, 127, get (op, "xfout_hivel", "127").getIntValue());
            r.xfPower = get (op, "xf_velcurve", "power").trim().toLowerCase() != "gain";
            // THE MOD WHEEL (0.75.0).
            r.ccInLo = juce::jlimit (0, 127, get (op, "xfin_locc1", "0").getIntValue());
            r.ccInHi = juce::jlimit (0, 127, get (op, "xfin_hicc1", "0").getIntValue());
            r.ccOutLo = juce::jlimit (0, 127, get (op, "xfout_locc1", "127").getIntValue());
            r.ccOutHi = juce::jlimit (0, 127, get (op, "xfout_hicc1", "127").getIntValue());
            r.ccPower = get (op, "xf_cccurve", "power").trim().toLowerCase() != "gain";
            r.gainCcDb = number (op, "gain_cc1", 0.0f);
            // FILTER (0.75.0): only where a CUTOFF is given (SFZ: no cutoff, no filter).
            if (has (op, "cutoff"))
            {
                const auto type = get (op, "fil_type", "lpf_2p").trim().toLowerCase();
                using F = Region::Filter;
                r.filter = type == "lpf_1p" ? F::lowpass1 : type == "hpf_1p" ? F::highpass1
                         : type == "hpf_2p" ? F::highpass2 : F::lowpass2;
                if (type != "lpf_1p" && type != "lpf_2p" && type != "hpf_1p" && type != "hpf_2p" && leftOutSeen.insert ("fil_type").second)
                    inst->problems.add ("filter type " + type + " played as lpf_2p");
                r.cutoff = juce::jmax (1.0f, number (op, "cutoff", 20000.0f));
                r.cutoffCcCents = number (op, "cutoff_cc1", 0.0f);
                r.filterKeytrack = number (op, "fil_keytrack", 0.0f);
                r.filterVeltrack = number (op, "fil_veltrack", 0.0f);
                if (has (op, "fil_keycenter"))
                    if (const auto centre = noteNumber (get (op, "fil_keycenter")); centre >= 0)
                        r.filterKeyCenter = (float) centre;
                r.filterQ = 0.70710678f * std::pow (10.0f, number (op, "resonance", 0.0f) / 20.0f);
            }
            // KEY SWITCHES (0.75.0).
            if (has (op, "sw_last"))
            {
                r.switchKey = noteNumber (get (op, "sw_last"));
                if (r.switchKey >= 0)
                {
                    const auto lo = has (op, "sw_lokey") ? noteNumber (get (op, "sw_lokey")) : r.switchKey;
                    const auto hi = has (op, "sw_hikey") ? noteNumber (get (op, "sw_hikey")) : r.switchKey;
                    inst->switchLo = inst->switchLo < 0 ? juce::jmax (0, lo) : juce::jmin (inst->switchLo, juce::jmax (0, lo));
                    inst->switchHi = juce::jmax (inst->switchHi, hi);
                    if (inst->switchDefault < 0 && has (op, "sw_default"))
                        inst->switchDefault = noteNumber (get (op, "sw_default"));
                    if (inst->switchLabel (r.switchKey).isEmpty())
                        inst->switchLabels.push_back ({ r.switchKey, get (op, "sw_label", juce::MidiMessage::getMidiNoteName (r.switchKey, true, true, 4)) });
                }
            }
            if (const auto key = noteNumber (get (op, "key")); key >= 0)
            {
                r.loKey = r.hiKey = key;
                r.keyCenter = (float) key;
            }
            if (has (op, "lokey"))
                r.loKey = juce::jmax (0, noteNumber (get (op, "lokey")));
            if (has (op, "hikey"))
                r.hiKey = juce::jmin (127, noteNumber (get (op, "hikey")));
            if (has (op, "pitch_keycenter"))
            {
                const auto centre = noteNumber (get (op, "pitch_keycenter"));
                if (centre >= 0)
                    r.keyCenter = (float) centre;
            }
            r.loVel = juce::jlimit (0, 127, get (op, "lovel", "1").getIntValue());
            r.hiVel = juce::jlimit (0, 127, get (op, "hivel", "127").getIntValue());
            r.keytrack = number (op, "pitch_keytrack", 100.0f) / 100.0f;
            r.tuneSemitones = (number (op, "tune", 0.0f) + number (op, "group_tune", 0.0f)) / 100.0f + number (op, "transpose", 0.0f);
            // VOLUME, and (0.75.0) the level a <group>, <master> or <global> sets for all of its regions, which adds to it.
            const auto volumeDb = number (op, "volume", 0.0f) + number (op, "group_volume", 0.0f) + number (op, "master_volume", 0.0f)
                                + number (op, "global_volume", 0.0f);
            r.gain = juce::Decibels::decibelsToGain (volumeDb) * number (op, "amplitude", 100.0f) / 100.0f;
            // Other controllers at their starting values (0.82.0): AMPLITUDE_CCn / _ONCCn (% at 127, multiplying),
            // GAIN / VOLUME_ONCCn (dB at 127), TUNE_CCn (cents at 127), PAN_CCn (% at 127). Curves are taken as straight.
            auto panPercent = number (op, "pan", 0.0f);
            for (const auto& [name, value] : op)
            {
                const auto ccOf = [&name] (const char* prefix) -> int
                {
                    if (! name.startsWith (prefix))
                        return -1;
                    const auto digits = name.fromFirstOccurrenceOf (prefix, false, false);
                    return digits.isNotEmpty() && digits.containsOnly ("0123456789") ? digits.getIntValue() : -1;
                };
                const auto amount = value.getFloatValue();
                auto cc = -1;
                if ((cc = juce::jmax (ccOf ("amplitude_cc"), ccOf ("amplitude_oncc"))) > 1 && cc < 128 && cc != 64)
                    r.gain *= amount / 100.0f * (float) ccStart[(size_t) cc] / 127.0f;
                else if ((cc = juce::jmax (juce::jmax (ccOf ("gain_cc"), ccOf ("gain_oncc")), ccOf ("volume_oncc"))) > 1 && cc < 128 && cc != 64)
                    r.gain *= juce::Decibels::decibelsToGain (amount * (float) ccStart[(size_t) cc] / 127.0f);
                else if ((cc = juce::jmax (ccOf ("tune_cc"), ccOf ("tune_oncc"))) > 1 && cc < 128 && cc != 64)
                    r.tuneSemitones += amount * (float) ccStart[(size_t) cc] / 127.0f / 100.0f;
                else if ((cc = juce::jmax (ccOf ("pan_cc"), ccOf ("pan_oncc"))) > 1 && cc < 128 && cc != 64)
                    panPercent += amount * (float) ccStart[(size_t) cc] / 127.0f;
            }
            r.pan = juce::jlimit (-1.0f, 1.0f, panPercent / 100.0f);
            const auto& sample = *inst->samples[(size_t) index];
            r.offset = (int) juce::jlimit<juce::int64> (0, juce::jmax<juce::int64> (0, sample.length - 1),
                                                         get (op, "offset", "0").getLargeIntValue());
            r.release = get (op, "trigger", "attack").trim().toLowerCase() == "release";
            r.rtDecayDb = juce::jmax (0.0f, number (op, "rt_decay", 0.0f));
            r.velTrack = number (op, "amp_veltrack", 100.0f) / 100.0f;
            for (const auto& [name, value] : op)   // AMP_VELCURVE_N (0.81.0)
                if (name.startsWith ("amp_velcurve_"))
                    r.velCurve.emplace_back (juce::jlimit (0, 127, name.fromFirstOccurrenceOf ("amp_velcurve_", false, false).getIntValue()),
                                             juce::jmax (0.0f, value.getFloatValue()));
            std::sort (r.velCurve.begin(), r.velCurve.end());
            r.seqLength = juce::jmax (1, get (op, "seq_length", "1").getIntValue());
            r.seqPosition = juce::jlimit (1, r.seqLength, get (op, "seq_position", "1").getIntValue());
            r.loRand = number (op, "lorand", 0.0f);
            r.hiRand = number (op, "hirand", 1.0f);
            r.loPedal = juce::jlimit (0, 127, get (op, "locc64", "0").getIntValue());
            r.releaseSeconds = has (op, "ampeg_release") ? juce::jmax (0.0f, number (op, "ampeg_release", 0.0f)) : -1.0f;
            if (has (op, "on_locc64") || has (op, "on_hicc64"))
            {
                // The pedal's own sound: no key plays it (its LOKEY / HIKEY are usually -1).
                r.pedalTrigger = true;
                r.onLo = juce::jlimit (0, 127, get (op, "on_locc64", "0").getIntValue());
                r.onHi = juce::jlimit (0, 127, get (op, "on_hicc64", "127").getIntValue());
                r.loKey = 0;
                r.hiKey = 127;
                inst->hasPedalSounds = true;
            }
            else
            {
                // Triggers from other controllers aren't played.
                auto otherController = false;
                for (const auto& [name, value] : op)
                    otherController = otherController || name.startsWith ("on_locc") || name.startsWith ("on_hicc");
                if (otherController)
                {
                    if (leftOutSeen.insert ("on_locc").second)
                        inst->problems.add ("not played: triggers from controllers other than the pedal");
                    continue;
                }
            }
            r.hiPedal = juce::jlimit (0, 127, get (op, "hicc64", "127").getIntValue());
            if (r.loKey > r.hiKey || r.loVel > r.hiVel)
                continue;
            inst->hasRelease = inst->hasRelease || r.release;
            inst->longestSeconds = juce::jmax (inst->longestSeconds, (double) sample.length / sample.sampleRate);
            if (! r.release && ! r.pedalTrigger)
            {
                inst->loKey = juce::jmin (inst->loKey, r.loKey);
                inst->hiKey = juce::jmax (inst->hiKey, r.hiKey);
            }
            inst->wheelDynamics = inst->wheelDynamics || r.fadesWithWheel() || r.cutoffCcCents != 0.0f;
            inst->regions.push_back (r);
        }
        // KEY SWITCHES: the first one down, the default or the lowest; in key order for the menu.
        std::sort (inst->switchLabels.begin(), inst->switchLabels.end());
        if (inst->switchDefault < 0 && ! inst->switchLabels.empty())
            inst->switchDefault = inst->switchLabels.front().first;
        // Other controllers' crossfades and gains aren't played.
        for (const auto& op : opcodes)
            for (const auto& [name, value] : op)
                if ((name.startsWith ("xfin_locc") || name.startsWith ("xfout_locc") || name.startsWith ("cutoff_cc"))
                    && ! name.endsWith ("cc1") && leftOutSeen.insert ("othercc").second)
                    inst->problems.add ("held at their starting values: controllers other than the mod wheel and the pedal");
        if (progress)
            progress (1.0f);
        // (a sample first read as a head and then kept whole for a loop no longer streams)
        inst->streamed = std::any_of (inst->samples.begin(), inst->samples.end(), [] (const auto& s) { return s->stream != nullptr; });
        if (inst->regions.empty())
            return nullptr;
        return inst;
    }
}
