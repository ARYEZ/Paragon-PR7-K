#include "dsp/synth/Sfz.h"

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
                    const auto name = trimmed.fromFirstOccurrenceOf ("\"", false, false).upToLastOccurrenceOf ("\"", false, false);
                    result << preprocess (file.getParentDirectory().getChildFile (name.replaceCharacter ('\\', '/')),
                                          defines, depth + 1, problems) << "\n";
                    continue;
                }
                if (! defines.empty() && line.containsChar ('$'))
                {
                    // Longest names first, so $KIT2 isn't read as $KIT then 2.
                    std::vector<std::pair<juce::String, juce::String>> sorted (defines.begin(), defines.end());
                    std::sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
                    for (const auto& [k, v] : sorted)
                        line = line.replace (k, v);
                }
                result << line << "\n";
            }
            return result;
        }

        // The headers and their opcodes, each region with everything it inherits.
        std::vector<Opcodes> parse (const juce::String& text, juce::String& defaultPath)
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
                    control = current;
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
        const char* const leftOut[] = { "loop_mode", "loopmode", "fil_type", "cutoff", "ampeg_attack", "ampeg_decay",
                                        "ampeg_sustain", "sw_last", "sw_lokey", "off_by", "group" };
    }

    int Instrument::select (int key, int velocity127, bool release, int seqCount, float chance, int pedal,
                            const Region** out, int capacity) const noexcept
    {
        int n = 0;
        for (const auto& r : regions)
        {
            if (n >= capacity)
                break;
            if (r.pedalTrigger || r.release != release || ! r.plays (key, velocity127) || pedal < r.loPedal || pedal > r.hiPedal)
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
        const auto opcodes = parse (preprocess (sfzFile, defines, 0, inst->problems), defaultPath);
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
            for (const auto* name : leftOut)
                if (has (op, name) && leftOutSeen.insert (name).second)
                    inst->problems.add ("not played: " + juce::String (name));

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
            r.tuneSemitones = number (op, "tune", 0.0f) / 100.0f + number (op, "transpose", 0.0f);
            r.gain = juce::Decibels::decibelsToGain (number (op, "volume", 0.0f)) * number (op, "amplitude", 100.0f) / 100.0f;
            r.pan = juce::jlimit (-1.0f, 1.0f, number (op, "pan", 0.0f) / 100.0f);
            const auto& sample = *inst->samples[(size_t) index];
            r.offset = (int) juce::jlimit<juce::int64> (0, juce::jmax<juce::int64> (0, sample.length - 1),
                                                         get (op, "offset", "0").getLargeIntValue());
            r.release = get (op, "trigger", "attack").trim().toLowerCase() == "release";
            r.rtDecayDb = juce::jmax (0.0f, number (op, "rt_decay", 0.0f));
            r.velTrack = number (op, "amp_veltrack", 100.0f) / 100.0f;
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
            inst->regions.push_back (r);
        }
        if (progress)
            progress (1.0f);
        if (inst->regions.empty())
            return nullptr;
        return inst;
    }
}
