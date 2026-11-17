// GET THE PIANO (0.71.0): downloading a sample library and unpacking it, tested on archives made here (a stand-in
// library with the Salamander's folder and file names and a sine for a piano), fetched from a file:// URL.
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "SampleLibraries.h"
#include "dsp/synth/Sfz.h"
#include "ui/MainView.h"
#include "rigs/FactoryRigs.h"

using Catch::Matchers::WithinAbs;

namespace
{
    struct TempFolder
    {
        TempFolder() : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("pr7k-lib-" + juce::String (juce::Random::getSystemRandom().nextInt64())))
        {
            dir.createDirectory();
        }
        ~TempFolder() { dir.deleteRecursively(); }
        juce::File dir;
    };

    // A tar archive, written here: ustar headers (GNU long names and POSIX path records when asked).
    struct TarWriter
    {
        juce::MemoryOutputStream data;

        void header (const juce::String& name, juce::int64 size, char type)
        {
            char h[512] {};
            const auto utf8 = name.toRawUTF8();
            std::memcpy (h, utf8, juce::jmin<size_t> (99, std::strlen (utf8)));
            std::snprintf (h + 100, 8, "%07o", 0644);
            std::snprintf (h + 108, 8, "%07o", 0);
            std::snprintf (h + 116, 8, "%07o", 0);
            std::snprintf (h + 124, 12, "%011llo", (unsigned long long) size);
            std::snprintf (h + 136, 12, "%011o", 0);
            h[156] = type;
            std::memcpy (h + 257, "ustar", 5);
            h[263] = '0';
            h[264] = '0';
            std::memset (h + 148, ' ', 8);
            unsigned sum = 0;
            for (auto c : h)
                sum += (std::uint8_t) c;
            std::snprintf (h + 148, 8, "%06o", sum);
            data.write (h, 512);
        }
        void pad (juce::int64 size)
        {
            const char zeros[512] {};
            data.write (zeros, (size_t) ((512 - size % 512) % 512));
        }
        void file (const juce::String& name, const juce::MemoryBlock& content, char how = 0)
        {
            if (how == 'L')
            {
                const auto text = name + juce::String::charToString (0);
                header ("././@LongLink", (juce::int64) text.getNumBytesAsUTF8(), 'L');
                data.write (text.toRawUTF8(), text.getNumBytesAsUTF8());
                pad ((juce::int64) text.getNumBytesAsUTF8());
            }
            else if (how == 'x')
            {
                const auto record = "path=" + name + "\n";
                auto line = juce::String (record.length() + 3) + " " + record;
                line = juce::String (line.length()) + " " + record;
                header ("PaxHeader", line.length(), 'x');
                data.write (line.toRawUTF8(), (size_t) line.length());
                pad (line.length());
            }
            header (how != 0 ? juce::String ("short-name") : name, (juce::int64) content.getSize(), '0');
            data.write (content.getData(), content.getSize());
            pad ((juce::int64) content.getSize());
        }
        void folder (const juce::String& name) { header (name, 0, '5'); }

        // Gzipped, to `file` (`cutAt` > 0: only that many bytes, a broken download).
        void write (const juce::File& file, size_t cutAt = 0)
        {
            const char end[1024] {};
            data.write (end, sizeof (end));
            juce::MemoryOutputStream gz;
            {
                juce::GZIPCompressorOutputStream zip (gz, 6, juce::GZIPCompressorOutputStream::windowBitsGZIP);
                zip.write (data.getData(), data.getDataSize());
            }
            file.replaceWithData (gz.getData(), cutAt > 0 ? juce::jmin (cutAt, gz.getDataSize()) : gz.getDataSize());
        }
    };

    juce::MemoryBlock text (const juce::String& s) { return { s.toRawUTF8(), s.getNumBytesAsUTF8() }; }

    // (1.2.0) A RAR 4 archive, written here with its files stored (method 0x30, no compression): the one kind of RAR
    // that can be made without the RAR program, which is closed. dmc_unrar reads it as any other.
    struct RarWriter
    {
        juce::MemoryOutputStream data;

        static std::uint32_t crc32 (const void* bytes, size_t n)
        {
            std::uint32_t crc = 0xffffffffu;
            for (size_t i = 0; i < n; ++i)
            {
                crc ^= ((const std::uint8_t*) bytes)[i];
                for (int k = 0; k < 8; ++k)
                    crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
            }
            return ~crc;
        }
        static void put (std::vector<std::uint8_t>& v, std::uint32_t value, int bytes)
        {
            for (int i = 0; i < bytes; ++i)
                v.push_back ((std::uint8_t) (value >> (8 * i)));
        }
        // A block: its CRC (the low half of the CRC-32 of what follows it), type, flags, size, the rest.
        void block (std::uint8_t type, std::uint16_t flags, const std::vector<std::uint8_t>& rest)
        {
            std::vector<std::uint8_t> h { type };
            put (h, flags, 2);
            put (h, (std::uint32_t) (7 + rest.size()), 2);
            h.insert (h.end(), rest.begin(), rest.end());
            const auto crc = (std::uint16_t) (crc32 (h.data(), h.size()) & 0xffffu);
            data.writeShort ((short) crc);
            data.write (h.data(), h.size());
        }
        RarWriter()
        {
            const std::uint8_t marker[] { 0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00 };
            data.write (marker, sizeof (marker));
            block (0x73, 0, std::vector<std::uint8_t> (6, 0));   // the archive header
        }
        void entry (const juce::String& name, const juce::MemoryBlock& content, bool folder, bool damaged = false)
        {
            std::vector<std::uint8_t> rest;
            const auto size = (std::uint32_t) content.getSize();
            put (rest, size, 4);                                  // packed
            put (rest, size, 4);                                  // unpacked
            rest.push_back (3);                                   // made on Unix (paths with '/')
            put (rest, crc32 (content.getData(), content.getSize()), 4);
            put (rest, 0x5a210000u, 4);                           // a DOS date and time
            rest.push_back (20);                                  // needs RAR 2.0 to unpack
            rest.push_back (0x30);                                // stored
            put (rest, (std::uint32_t) name.getNumBytesAsUTF8(), 2);
            put (rest, folder ? 0x41edu << 16 : 0x81a4u << 16, 4);
            for (size_t i = 0; i < name.getNumBytesAsUTF8(); ++i)
                rest.push_back ((std::uint8_t) name.toRawUTF8()[i]);
            block (0x74, (std::uint16_t) (0x8000 | (folder ? 0x00e0 : 0)), rest);
            auto bytes = content;
            if (damaged && bytes.getSize() > 100)
                bytes[100] = (char) (bytes[100] ^ 0x55);
            data.write (bytes.getData(), bytes.getSize());
        }
        void file (const juce::String& name, const juce::MemoryBlock& content, bool damaged = false) { entry (name, content, false, damaged); }
        void folder (const juce::String& name) { entry (name, {}, true); }
        void write (const juce::File& file)
        {
            block (0x7b, 0x4000, {});   // the end
            file.replaceWithData (data.getData(), data.getDataSize());
        }
    };

    // A one-second middle C, mono 16-bit WAV, as a file's bytes.
    juce::MemoryBlock sineWav()
    {
        juce::MemoryBlock block;
        {
            auto stream = std::make_unique<juce::MemoryOutputStream> (block, false);
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> out (stream.release());
            auto writer = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (16));
            juce::AudioBuffer<float> buffer (1, 48000);
            for (int i = 0; i < 48000; ++i)
                buffer.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 261.6256 * i / 48000.0));
            writer->writeFromAudioSampleBuffer (buffer, 0, 48000);
        }
        return block;
    }

    // The stand-in: the Salamander's folder and SFZ name, a region playing a sine for every key.
    void writeStandIn (const juce::File& archive, size_t cutAt = 0)
    {
        const auto& library = libraries::salamanderGrand();
        TarWriter tar;
        tar.folder (library.folder + "/");
        tar.folder (library.folder + "/samples/");
        tar.file (library.folder + "/" + library.sfz, text ("<region> sample=samples/C4v8.wav lokey=0 hikey=127 pitch_keycenter=60\n"));
        tar.file (library.folder + "/samples/C4v8.wav", sineWav());
        tar.file (library.folder + "/readme.txt", text ("a stand-in"));
        tar.write (archive, cutAt);
    }
}

TEST_CASE ("Libraries: a .tar.gz unpacks, long names too, and nothing lands outside the folder", "[library]")
{
    TempFolder t;
    TarWriter tar;
    tar.folder ("lib/");
    tar.file ("lib/a.txt", text ("first"));
    tar.file ("lib/" + juce::String::repeatedString ("long", 40) + "/b.txt", text ("second"), 'L');
    tar.file ("lib/pax/" + juce::String::repeatedString ("x", 120) + ".txt", text ("third"), 'x');
    tar.file ("../escaped.txt", text ("never"));
    tar.file ("/abs.txt", text ("never"));
    const auto archive = t.dir.getChildFile ("a.tar.gz");
    tar.write (archive);

    const auto into = t.dir.getChildFile ("out");
    juce::String error;
    float last = 0.0f;
    REQUIRE (libraries::extractTarGz (archive, into, [&] (float p) { last = p; }, nullptr, error));
    CHECK (into.getChildFile ("lib/a.txt").loadFileAsString() == "first");
    CHECK (into.getChildFile ("lib/" + juce::String::repeatedString ("long", 40) + "/b.txt").loadFileAsString() == "second");
    CHECK (into.getChildFile ("lib/pax/" + juce::String::repeatedString ("x", 120) + ".txt").loadFileAsString() == "third");
    CHECK (! t.dir.getChildFile ("escaped.txt").exists());
    CHECK (! juce::File ("/abs.txt").exists());
    CHECK_THAT (last, WithinAbs (1.0, 0.01));

    // Not a tar.gz, or cut short: refused with a reason.
    const auto junk = t.dir.getChildFile ("junk.tar.gz");
    junk.replaceWithText ("not an archive");
    CHECK (! libraries::extractTarGz (junk, t.dir.getChildFile ("junk"), {}, nullptr, error));
    CHECK (error.isNotEmpty());
}

TEST_CASE ("Libraries: Karoryfer's three, each pinned to one commit and found by its own reference (0.82.0)", "[library]")
{
    for (const auto* library : { &libraries::bigRustyDrums(), &libraries::gogodzePhu(), &libraries::squidpipes() })
    {
        INFO (library->name);
        CHECK (libraries::libraryFor (libraries::refFor (*library)) == library);
        CHECK (library->url.contains (library->folder.fromLastOccurrenceOf ("-", false, false)));   // the commit
        CHECK (library->url.startsWith ("https://github.com/sfzinstruments/"));
        CHECK (library->keeps (library->sfz));
        CHECK (library->credit.contains ("Karoryfer"));
        CHECK (library->credit.contains ("CC0"));
    }
}

TEST_CASE ("Libraries: the western bank's five, each pinned to one commit and credited (0.84.0)", "[library]")
{
    const std::vector<const libraries::Library*> west { &libraries::blackAndGreenGuitars(), &libraries::shinyguitar(),
                                                        &libraries::meatbass(), &libraries::ganjo(), &libraries::kayBanjo() };
    for (const auto* library : west)
    {
        INFO (library->name);
        CHECK (libraries::libraryFor (libraries::refFor (*library)) == library);
        CHECK (library->url.contains (library->folder.fromLastOccurrenceOf ("-", false, false)));   // the commit
        CHECK (library->url.startsWith ("https://github.com/sfzinstruments/"));
        CHECK (library->keeps (library->sfz));
        CHECK (library->credit.isNotEmpty());
        const auto all = libraries::all();
        CHECK (std::find (all.begin(), all.end(), library) != all.end());
    }
    CHECK (libraries::kayBanjo().credit.contains ("GPL"));
    CHECK_FALSE (libraries::kayBanjo().keeps ("work/00 - original/1f38.wav"));   // the editing stages stay in the archive
    CHECK (libraries::kayBanjo().keeps ("samples/1f38.wav"));
    CHECK (libraries::ganjo().credit.contains ("CC0"));
}

TEST_CASE ("Libraries: a library missing samples counts as not installed, is offered again and unpacks whole (0.89.2)", "[library][plugin]")
{
    // A download that stopped part way left the instrument file but not all its samples: the presets on it played
    // silence (the owner's GUNSLINGER report). Now such a library isn't installed, the oscillator fails (the library is
    // offered again), and a new download unpacks into a folder of its own before moving into place.
    MainView::offerLibrary = false;
    TempFolder t;
    const auto presets = t.dir.getChildFile ("presets");
    presets.createDirectory();
    ParagonX1AudioProcessor processor;
    processor.useRigFolder (presets);
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);
    processor.setNonRealtime (true);
    const auto& library = libraries::salamanderGrand();
    const auto folder = processor.getLibrariesFolder().getChildFile (library.folder);
    folder.getChildFile ("samples").createDirectory();
    folder.getChildFile (library.sfz).replaceWithText ("<region> sample=samples/C4v8.wav lokey=0 hikey=127 pitch_keycenter=60\n");
    CHECK (processor.isLibraryInstalled (library));   // the instrument file is there...

    Rig rig;
    rig.name = "Piano Test";
    rig.parameters[ParamIDs::osc (0, "Wave")] = (float) synth::OscWave::sfz;
    rig.sfz[0] = libraries::refFor (library);
    RigLibrary::Entry entry;
    entry.rig = rig;
    processor.loadRig (entry);
    processor.waitForSfz();
    // ...but its sample isn't: not installed, failed, offered again.
    CHECK (processor.getSfzState (0) == ParagonX1AudioProcessor::SfzState::failed);
    CHECK (processor.getSfzProblem (0).contains ("incomplete"));
    CHECK_FALSE (processor.isLibraryInstalled (library));
    CHECK (processor.missingLibrary (0) == &library);

    // Fetched again: whole, installed, playing; nothing left of the unpacking.
    const auto archive = t.dir.getChildFile ("server/piano.tar.gz");
    archive.getParentDirectory().createDirectory();
    writeStandIn (archive);
    processor.startLibraryDownload (juce::URL (archive).toString (false));
    REQUIRE (const_cast<libraries::Download&> (processor.getLibraryDownload()).waitUntilFinished (30000));
    REQUIRE (processor.getLibraryDownload().getState() == libraries::Download::State::done);
    processor.waitForSfz();
    processor.waitForSfz();
    CHECK (processor.isLibraryInstalled (library));
    CHECK (processor.getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);
    CHECK (folder.getChildFile ("samples/C4v8.wav").existsAsFile());
    for (const auto& f : processor.getLibrariesFolder().findChildFiles (juce::File::findFilesAndDirectories, false))
        CHECK_FALSE (f.getFileName().endsWith (".partial"));
}

TEST_CASE ("Libraries: Black And Green Guitars' audio, altered by GitHub's archive, is fetched again as stored (0.89.1)", "[library][plugin]")
{
    // GitHub makes the archive with `git archive`, which applies the repository's `* text eol=crlf`: a CR goes in
    // before every lone LF byte of every WAV. Here: an archive altered so, and a folder of the stored files standing
    // in for GitHub's raw files.
    const auto& library = libraries::blackAndGreenGuitars();
    CHECK (library.rawBase.startsWith ("https://raw.githubusercontent.com/sfzinstruments/karoryfer.black-and-green-guitars/"));
    CHECK (library.rawBase.contains (library.folder.fromLastOccurrenceOf ("-", false, false)));   // the same commit
    for (const auto* other : libraries::all())
        if (other != &library)
            CHECK (other->rawBase.isEmpty());                // the others' archives are as stored (binary files untouched)

    juce::MemoryBlock stored = sineWav();
    {
        // Lone LFs and a CR LF pair already there (git leaves that one alone).
        auto* b = static_cast<char*> (stored.getData());
        b[200] = '\n';
        b[300] = '\r';
        b[301] = '\n';
    }
    const auto altered = libraries::withCrlf (stored);
    REQUIRE (altered.getSize() > stored.getSize());
    CHECK (libraries::withCrlf (altered) == altered);         // already converted: unchanged
    const auto plain = text ("no line breaks here");
    CHECK (libraries::withCrlf (plain) == plain);

    TempFolder t;
    const auto sample = juce::String ("Samples/green/fb/fb_micro.wav");
    const auto archive = t.dir.getChildFile ("server/bg.tar.gz");
    archive.getParentDirectory().createDirectory();
    {
        TarWriter tar;
        tar.folder (library.folder + "/");
        tar.file (library.folder + "/" + library.sfz, text ("<region> sample=../" + sample + " lokey=0 hikey=127 pitch_keycenter=60\r\n"), 'x');
        tar.file (library.folder + "/" + sample, altered, 'x');
        tar.write (archive);
    }
    const auto raw = t.dir.getChildFile ("raw");
    raw.getChildFile (sample).getParentDirectory().createDirectory();
    raw.getChildFile (sample).replaceWithData (stored.getData(), stored.getSize());
    const auto rawUrl = juce::URL (raw).toString (false) + "/";

    const auto presets = t.dir.getChildFile ("presets");
    presets.createDirectory();
    ParagonX1AudioProcessor processor;
    processor.useRigFolder (presets);
    const auto folder = processor.getLibrariesFolder().getChildFile (library.folder);
    auto& download = processor.getLibraryDownloadForTest();

    // A fresh download: unpacked, then the audio put back as stored, and only then installed.
    download.start (library, processor.getLibrariesFolder(), juce::URL (archive).toString (false), rawUrl);
    REQUIRE (download.waitUntilFinished (60000));
    INFO (download.getMessage());
    REQUIRE (download.getState() == libraries::Download::State::done);
    juce::MemoryBlock got;
    got.reset();   // (loadFileAsData adds to what the block holds)
    REQUIRE (folder.getChildFile (sample).loadFileAsData (got));
    CHECK (got == stored);
    CHECK (folder.getChildFile (libraries::exactMarker).existsAsFile());
    CHECK (processor.isLibraryInstalled (library));

    // An earlier version's download (altered, no marker): not installed, and fetching it again fetches only the
    // audio (the archive's link here leads nowhere).
    folder.getChildFile (libraries::exactMarker).deleteFile();
    folder.getChildFile (sample).replaceWithData (altered.getData(), altered.getSize());
    CHECK_FALSE (processor.isLibraryInstalled (library));
    download.start (library, processor.getLibrariesFolder(), juce::URL (t.dir.getChildFile ("server/missing.tar.gz")).toString (false), rawUrl);
    REQUIRE (download.waitUntilFinished (60000));
    REQUIRE (download.getState() == libraries::Download::State::done);
    got.reset();   // (loadFileAsData adds to what the block holds)
    REQUIRE (folder.getChildFile (sample).loadFileAsData (got));
    CHECK (got == stored);
    CHECK (processor.isLibraryInstalled (library));

    // A stored copy that doesn't match what the archive held: refused, nothing replaced, not installed.
    folder.getChildFile (libraries::exactMarker).deleteFile();
    folder.getChildFile (sample).replaceWithData (altered.getData(), altered.getSize());
    const auto wrong = sineWav();
    raw.getChildFile (sample).replaceWithData (wrong.getData(), wrong.getSize());
    download.start (library, processor.getLibrariesFolder(), {}, rawUrl);
    REQUIRE (download.waitUntilFinished (60000));
    CHECK (download.getState() == libraries::Download::State::failed);
    CHECK (download.getMessage().contains ("didn't match"));
    got.reset();   // (loadFileAsData adds to what the block holds)
    REQUIRE (folder.getChildFile (sample).loadFileAsData (got));
    CHECK (got == altered);
    CHECK_FALSE (processor.isLibraryInstalled (library));
}

TEST_CASE ("Libraries: the Versilian collection and Sam's Sonor; what a library skips isn't unpacked", "[library]")
{
    // 0.81.0: VCSL is about 6 GB, most of it pianos and harpsichords PR7-K has already: they're left in the archive.
    const auto& vcsl = libraries::versilianCollection();
    const auto& sonor = libraries::samsSonor();
    CHECK (libraries::libraryFor (libraries::refFor (vcsl)) == &vcsl);
    CHECK (libraries::libraryFor (libraries::refFor (sonor)) == &sonor);
    CHECK (vcsl.url.contains (vcsl.folder.fromFirstOccurrenceOf ("VCSL-", false, false)));    // pinned to one commit
    CHECK (sonor.url.contains (sonor.folder.fromFirstOccurrenceOf ("SamsSonor-", false, false)));
    CHECK_FALSE (vcsl.keeps ("Chordophones/Zithers/Grand Piano, Steinway B.sfz"));
    CHECK_FALSE (vcsl.keeps ("Chordophones/Zithers/Grand Piano, Steinway B/C4.wav"));
    CHECK_FALSE (vcsl.keeps ("Membranophones/Struck Membranophones/Bass Drum 3 - Legacy.sfz"));
    CHECK (vcsl.keeps (vcsl.sfz));
    CHECK (vcsl.keeps ("Idiophones/Struck Idiophones/Tubular Bells 1.sfz"));
    CHECK (vcsl.keeps ("Membranophones/Struck Membranophones/Snare Drum, Rope Tension.sfz"));
    CHECK (sonor.keeps (sonor.sfz));

    TempFolder t;
    const auto archive = t.dir.getChildFile ("vcsl.tar.gz");
    {
        TarWriter tar;
        tar.folder (vcsl.folder + "/");
        tar.file (vcsl.folder + "/" + vcsl.sfz, text ("<region> sample=x.wav"), 'x');
        tar.file (vcsl.folder + "/Chordophones/Zithers/Grand Piano, Kawai.sfz", text ("<region> sample=y.wav"), 'x');
        tar.file (vcsl.folder + "/Chordophones/Zithers/Grand Piano, Kawai/C4.wav", sineWav(), 'x');
        tar.write (archive);
    }
    juce::String error;
    const auto into = t.dir.getChildFile ("out");
    REQUIRE (libraries::extractTarGz (archive, into, {}, nullptr, error,
                                      [&vcsl] (const juce::String& path)
                                      {
                                          return ! path.startsWith (vcsl.folder + "/") || vcsl.keeps (path.substring (vcsl.folder.length() + 1));
                                      }));
    CHECK (into.getChildFile (vcsl.folder).getChildFile (vcsl.sfz).existsAsFile());
    CHECK_FALSE (into.getChildFile (vcsl.folder).getChildFile ("Chordophones/Zithers/Grand Piano, Kawai.sfz").exists());
    CHECK_FALSE (into.getChildFile (vcsl.folder).getChildFile ("Chordophones/Zithers/Grand Piano, Kawai").exists());
}

TEST_CASE ("Libraries: GET THE PIANO downloads the library and a preset waiting for it plays", "[library][plugin]")
{
    MainView::offerLibrary = false;
    TempFolder t;
    const auto presets = t.dir.getChildFile ("presets");
    presets.createDirectory();
    const auto archive = t.dir.getChildFile ("server/piano.tar.gz");
    archive.getParentDirectory().createDirectory();
    writeStandIn (archive);

    ParagonX1AudioProcessor processor;
    processor.useRigFolder (presets);
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);
    processor.setNonRealtime (true);

    // A factory-style preset naming the library: not here yet, so it waits (silent), and says what's missing.
    const auto& library = libraries::salamanderGrand();
    Rig rig;
    rig.name = "Piano Test";
    rig.parameters[ParamIDs::osc (0, "Wave")] = (float) synth::OscWave::sfz;
    rig.parameters[ParamIDs::osc (0, "Filter")] = 0.0f;
    rig.sfz[0] = libraries::refFor (library);
    RigLibrary::Entry entry;
    entry.rig = rig;
    processor.loadRig (entry);
    processor.waitForSfz();
    CHECK (! processor.isLibraryInstalled (library));
    REQUIRE (processor.missingLibrary (0) == &library);

    processor.startLibraryDownload (juce::URL (archive).toString (false));
    REQUIRE (processor.getLibraryDownload().isRunning());
    REQUIRE (const_cast<libraries::Download&> (processor.getLibraryDownload()).waitUntilFinished (30000));
    INFO (processor.getLibraryDownload().getMessage());
    REQUIRE (processor.getLibraryDownload().getState() == libraries::Download::State::done);
    CHECK (processor.isLibraryInstalled (library));
    CHECK (! processor.getLibrariesFolder().getChildFile (library.folder + ".tar.gz.download").exists());   // the archive deleted
    processor.waitForSfz();   // sees it's in: loads the waiting oscillator
    processor.waitForSfz();
    CHECK (processor.getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);
    CHECK (processor.missingLibrary (0) == nullptr);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    auto peak = 0.0f;
    for (int k = 0; k < 40; ++k)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();
        peak = juce::jmax (peak, buffer.getMagnitude (0, 0, 256));
    }
    CHECK (peak > 0.05f);
}

TEST_CASE ("Libraries: GET THE ORCHESTRA: a second library, found by its presets' references, downloaded and played", "[library][plugin]")
{
    // 0.73.0: the Sonatina Symphonic Orchestra beside the piano. A stand-in with its layout: instruments in folders,
    // samples a folder up ("../Samples/...").
    MainView::offerLibrary = false;
    const auto& sso = libraries::sonatinaOrchestra();
    const auto& piano = libraries::salamanderGrand();
    const juce::String horns = "Sonatina Symphonic Orchestra/Brass - Notation/Horns Sustain.sfz";
    CHECK (sso.sfz == horns);
    CHECK_FALSE (sso.exactSize);   // GitHub's own archive: its size isn't fixed
    CHECK (libraries::libraryFor (libraries::refFor (sso, horns)) == &sso);
    CHECK (libraries::libraryFor (libraries::refFor (piano)) == &piano);
    CHECK (libraries::libraryFor ("{sfz:some other folder}/x.sfz") == nullptr);
    CHECK (libraries::all().size() == 13);   // 0.81.0: the Versilian collection and Sam's Sonor too; 0.82.0: Karoryfer's three; 0.84.0: the western five; 1.2.0: the koto

    TempFolder t;
    const auto presets = t.dir.getChildFile ("presets");
    presets.createDirectory();
    const auto archive = t.dir.getChildFile ("server/sso.tar.gz");
    archive.getParentDirectory().createDirectory();
    {
        TarWriter tar;
        tar.folder (sso.folder + "/");
        tar.file (sso.folder + "/" + horns,
                  text ("<group> loop_mode=loop_continuous\n<region> sample=../Samples/Horns/horns-sus-c4.wav lokey=0 hikey=127 pitch_keycenter=60\n"), 'L');
        tar.file (sso.folder + "/Sonatina Symphonic Orchestra/Samples/Horns/horns-sus-c4.wav", sineWav(), 'L');
        tar.write (archive);
    }

    ParagonX1AudioProcessor processor;
    processor.useRigFolder (presets);
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);
    processor.setNonRealtime (true);

    Rig rig;
    rig.name = "Orchestra Test";
    rig.parameters[ParamIDs::osc (0, "Wave")] = (float) synth::OscWave::sfz;
    rig.parameters[ParamIDs::osc (0, "Filter")] = 0.0f;
    rig.sfz[0] = libraries::refFor (sso, horns);
    RigLibrary::Entry entry;
    entry.rig = rig;
    processor.loadRig (entry);
    processor.waitForSfz();
    CHECK (processor.missingLibrary (0) == &sso);          // the orchestra, not the piano
    CHECK (processor.getSfzName (0).contains ("Horns Sustain"));
    CHECK (ParagonX1AudioProcessor::libraryOf (rig) == &sso);

    processor.startLibraryDownload (sso, juce::URL (archive).toString (false));
    REQUIRE (const_cast<libraries::Download&> (processor.getLibraryDownload()).waitUntilFinished (30000));
    INFO (processor.getLibraryDownload().getMessage());
    REQUIRE (processor.getLibraryDownload().getState() == libraries::Download::State::done);
    CHECK (&processor.getLibraryDownload().getLibrary() != &piano);
    CHECK (processor.isLibraryInstalled (sso));
    CHECK_FALSE (processor.isLibraryInstalled (piano));
    processor.waitForSfz();
    processor.waitForSfz();
    CHECK (processor.getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);
    CHECK (processor.missingLibrary (0) == nullptr);
    {
        const auto inst = processor.getSfzInstrument (0);
        REQUIRE (inst != nullptr);
        REQUIRE (inst->regions.size() == 1);
        CHECK (inst->regions[0].loops());
        CHECK (inst->samples[0]->headFrames == inst->samples[0]->length);   // kept whole for its loop
    }

    // It plays, and its one-second sample loops: still sounding after three seconds held.
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    auto late = 0.0f;
    for (int k = 0; k < 600; ++k)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();
        if (k > 560)
            late = juce::jmax (late, buffer.getMagnitude (0, 0, 256));
    }
    CHECK (late > 0.05f);
}

TEST_CASE ("Libraries: the expansions' pianos play their synthesized fallback until the piano is in", "[library][plugin]")
{
    // 0.72.0: seven expansion presets carry a version on the Salamander (0.73.0: eight, with HYPERDRIVE's Ominous Piano) (Rig::sampled), played once it's installed.
    MainView::offerLibrary = false;
    const auto& library = libraries::salamanderGrand();
    const auto wave = ParamIDs::osc (0, "Wave");
    juce::StringArray withPiano;
    for (const auto& rig : makeExpansionRigs())
    {
        // (0.73.0: HYPERDRIVE's orchestral versions are on the Sonatina Symphonic Orchestra: checked in ExpansionTests.)
        if (rig.sampled == nullptr || ParagonX1AudioProcessor::libraryOf (*rig.sampled) != &library
            || rig.bank == Rig::numBanks + 20    // (0.94.0: SEVERED's Cold Morning Piano, through grand(): checked in ExpansionTests)
            || rig.bank == Rig::numBanks + 26    // (0.74.0: GRAND CINEMA's, layered, are checked in ExpansionTests)
            || rig.bank == Rig::numBanks + 29    // (0.79.0: STOLEN SKY's, through GRAND CINEMA's grand(), too)
            || rig.bank == Rig::numBanks + 30    // (0.80.0: DROWNED DECO's, the same)
            || rig.bank == Rig::numBanks + 31    // (0.81.0: NEW EDEN's, the same)
            || rig.bank == Rig::numBanks + 32    // (0.84.0: GUNSLINGER's honky-tonk, two Salamanders through grand())
            || rig.bank == Rig::numBanks + 35    // (0.89.0: PLAYGROUND's Pawn Shop Piano, the same)
            || rig.bank == Rig::numBanks + 37    // (0.91.0: DARK DESCENT's Rationed Piano, the same)
            || rig.bank == Rig::numBanks + 38    // (0.92.0: HIGHNOON's Player Piano and Unwound Piano, the same)
            || rig.bank == Rig::numBanks + 42)   // (1.0.0: WICKED GAME's Campfire Piano, the same)
            continue;
        withPiano.add (rig.name);
        INFO (rig.name);
        const auto& sampled = *rig.sampled;
        CHECK (sampled.name == rig.name);
        CHECK (sampled.bank == rig.bank);
        CHECK (sampled.number == rig.number);
        CHECK (sampled.sampled == nullptr);
        CHECK (sampled.sfz.size() == 1);
        CHECK (sampled.sfz.at (0) == libraries::refFor (library));
        CHECK (sampled.parameters.at (wave) == (float) synth::OscWave::sfz);
        CHECK (sampled.notes.contains ("Salamander"));
        for (const auto& route : sampled.modulation.routes)
            CHECK (! route.destination.startsWith ("osc"));      // the synthesized piano's routes went with it
        CHECK (sampled.modulation.macroNames == rig.modulation.macroNames);
        // The fallback is the preset as approved: no sampled oscillator.
        CHECK (rig.sfz.empty());
        CHECK ((rig.parameters.count (wave) == 0 || rig.parameters.at (wave) != (float) synth::OscWave::sfz));
    }
    withPiano.sort (true);
    CHECK (withPiano.joinIntoString (", ") == "Distant Piano, Echo Piano, Felt Memory, Home Piano, Lament Piano, Memory Loop, Ominous Piano, Tesseract");

    TempFolder t;
    const auto presets = t.dir.getChildFile ("presets");
    presets.createDirectory();
    const auto archive = t.dir.getChildFile ("server/piano.tar.gz");
    archive.getParentDirectory().createDirectory();
    writeStandIn (archive);

    ParagonX1AudioProcessor processor;
    processor.useRigFolder (presets);
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);
    processor.setNonRealtime (true);
    const auto waveNow = [&] { return processor.getParameters().getParameter (wave)->convertFrom0to1 (
                                          processor.getParameters().getParameter (wave)->getValue()); };

    // Without the piano: the synthesized one, nothing missing (no offer of a sampled oscillator that can't play).
    const auto* lament = processor.getRigLibrary().findFactory ("Lament Piano");
    REQUIRE (lament != nullptr);
    processor.loadRig (*lament);
    processor.waitForSfz();
    CHECK (waveNow() != (float) synth::OscWave::sfz);
    CHECK (processor.missingLibrary (0) == nullptr);

    // The piano arrives: the preset (not edited) becomes its sampled version, and plays it.
    processor.startLibraryDownload (juce::URL (archive).toString (false));
    REQUIRE (const_cast<libraries::Download&> (processor.getLibraryDownload()).waitUntilFinished (30000));
    REQUIRE (processor.getLibraryDownload().getState() == libraries::Download::State::done);
    processor.waitForSfz();
    processor.waitForSfz();
    CHECK (waveNow() == (float) synth::OscWave::sfz);
    CHECK (processor.getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);
    CHECK (processor.getCurrentRig().name == "Lament Piano");

    // Loading another with the piano in: straight to its sampled version.
    processor.loadRig (*processor.getRigLibrary().findFactory ("Tesseract"));
    processor.waitForSfz();
    CHECK (waveNow() == (float) synth::OscWave::sfz);
    CHECK (processor.getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);
    // A preset without one is unchanged.
    processor.loadRig (*processor.getRigLibrary().findFactory ("Fog Glass"));
    CHECK (waveNow() != (float) synth::OscWave::sfz);
}

TEST_CASE ("Libraries: a RAR archive unpacks, nothing lands outside the folder, and a damaged file is caught (1.2.0)", "[library]")
{
    TempFolder t;
    const auto archive = t.dir.getChildFile ("a.rar");
    {
        RarWriter rar;
        rar.folder ("Lib");
        rar.folder ("Lib/KOTO");
        rar.file ("Lib/KOTO/#KOTO.sfz", text ("<region> sample=../Samples/c4.wav\n"));
        rar.file ("Lib/Samples/c4.wav", sineWav());
        rar.file ("Lib/skip/me.txt", text ("left out"));
        rar.file ("../outside.txt", text ("never"));
        rar.write (archive);
    }
    const auto into = t.dir.getChildFile ("out");
    juce::String error;
    float last = 0.0f;
    REQUIRE (libraries::extractRar (archive, into, [&last] (float p) { last = p; }, nullptr, error,
                                    [] (const juce::String& path) { return ! path.startsWith ("Lib/skip/"); },
                                    [] (const juce::String& path) { return "Moved" + path.substring (3); }));
    INFO (error);
    CHECK (into.getChildFile ("Moved/KOTO/#KOTO.sfz").loadFileAsString().startsWith ("<region>"));
    juce::MemoryBlock wav;
    into.getChildFile ("Moved/Samples/c4.wav").loadFileAsData (wav);
    CHECK (wav == sineWav());
    CHECK_FALSE (into.getChildFile ("Moved/skip").exists());
    CHECK_FALSE (t.dir.getChildFile ("outside.txt").exists());
    CHECK_FALSE (into.getChildFile ("outside.txt").exists());
    CHECK (last == 1.0f);

    // A file whose bytes don't match its CRC fails (and isn't left behind).
    const auto damaged = t.dir.getChildFile ("damaged.rar");
    {
        RarWriter rar;
        rar.file ("Lib/Samples/c4.wav", sineWav(), true);
        rar.write (damaged);
    }
    const auto into2 = t.dir.getChildFile ("out2");
    CHECK_FALSE (libraries::extractRar (damaged, into2, {}, nullptr, error));
    CHECK (error.isNotEmpty());
    CHECK_FALSE (into2.getChildFile ("Lib/Samples/c4.wav").exists());

    // Not a RAR at all.
    const auto junk = t.dir.getChildFile ("junk.rar");
    junk.replaceWithText ("<html>nope</html>");
    CHECK_FALSE (libraries::extractRar (junk, t.dir.getChildFile ("out3"), {}, nullptr, error));
}

TEST_CASE ("Libraries: GET THE KOTO fetches its maker's RAR and unpacks it; a refusal from Google Drive says why (1.2.0)", "[library]")
{
    const auto& koto = libraries::unrealKoto();
    CHECK (koto.archive == libraries::Library::Archive::rar);
    CHECK (koto.url.startsWith ("https://drive.usercontent.google.com/download?id="));
    CHECK (koto.url.contains ("confirm=t"));
    CHECK (libraries::libraryFor (libraries::refFor (koto)) == &koto);
    const auto all = libraries::all();
    CHECK (std::find (all.begin(), all.end(), &koto) != all.end());

    TempFolder t;
    const auto server = t.dir.getChildFile ("server");
    server.createDirectory();
    const auto archive = server.getChildFile ("Ui_KOTO.rar");
    {
        RarWriter rar;
        rar.folder (koto.folder);
        rar.file (koto.folder + "/" + koto.sfz, text ("<region> sample=../Samples/c4.wav lokey=0 hikey=127 pitch_keycenter=60\n"));
        rar.file (koto.folder + "/Samples/c4.wav", sineWav());
        rar.write (archive);
    }
    const auto into = t.dir.getChildFile ("Libraries");
    {
        libraries::Download download;
        download.start (koto, into, juce::URL (archive).toString (false));
        REQUIRE (download.waitUntilFinished (30000));
        INFO (download.getMessage());
        CHECK (download.getState() == libraries::Download::State::done);
        CHECK (libraries::isComplete (koto, into.getChildFile (koto.folder)));
        CHECK_FALSE (into.getChildFile (koto.folder + ".rar.download").exists());
        CHECK_FALSE (into.getChildFile (koto.folder + ".partial").exists());
        const auto inst = synth::sfz::load (into.getChildFile (koto.folder).getChildFile (koto.sfz));
        REQUIRE (inst != nullptr);
        CHECK (inst->regions.size() == 1);
    }
    {
        // Google Drive's "too many users have downloaded this file" page instead of the file.
        const auto page = server.getChildFile ("quota.html");
        page.replaceWithText ("<html><title>Google Drive - Quota exceeded</title>Too many users have viewed or downloaded this file recently.</html>");
        const auto into2 = t.dir.getChildFile ("Libraries2");
        libraries::Download download;
        download.start (koto, into2, juce::URL (page).toString (false));
        REQUIRE (download.waitUntilFinished (30000));
        CHECK (download.getState() == libraries::Download::State::failed);
        CHECK (download.getMessage().contains ("Google Drive"));
        CHECK (download.getMessage().contains ("Try again later"));
        CHECK_FALSE (into2.getChildFile (koto.folder).exists());
        CHECK_FALSE (into2.getChildFile (koto.folder + ".rar.download").exists());
    }
}

// (1.2.0) The real koto archive (PARAGON_KOTO_RAR) unpacks as bsdtar's copy of it (PARAGON_KOTO_DIR, its top folder
// inside), file for file.
TEST_CASE ("Libraries: the real koto RAR unpacks as stored", "[.rarcheck]")
{
    const juce::File archive (juce::SystemStats::getEnvironmentVariable ("PARAGON_KOTO_RAR", {}));
    const juce::File reference (juce::SystemStats::getEnvironmentVariable ("PARAGON_KOTO_DIR", {}));
    if (! archive.existsAsFile() || ! reference.isDirectory())
        return;
    TempFolder t;
    juce::String error;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    REQUIRE (libraries::extractRar (archive, t.dir, {}, nullptr, error));
    WARN ("unpacked in " << (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0 << " s");
    auto compared = 0;
    for (const auto& entry : juce::RangedDirectoryIterator (reference, true, "*", juce::File::findFiles))
    {
        const auto relative = entry.getFile().getRelativePathFrom (reference.getParentDirectory());
        const auto ours = t.dir.getChildFile (relative);
        INFO (relative);
        REQUIRE (ours.existsAsFile());
        CHECK (ours.hasIdenticalContentTo (entry.getFile()));
        ++compared;
    }
    WARN (compared << " files compared");
    CHECK (compared > 700);
}

TEST_CASE ("Libraries: a broken download isn't unpacked and leaves nothing behind", "[library]")
{
    TempFolder t;
    const auto archive = t.dir.getChildFile ("cut.tar.gz");
    writeStandIn (archive, 4000);
    libraries::Download download;
    const auto into = t.dir.getChildFile ("Libraries");
    download.start (libraries::salamanderGrand(), into, juce::URL (archive).toString (false));
    REQUIRE (download.waitUntilFinished (30000));
    CHECK (download.getState() == libraries::Download::State::failed);
    CHECK (download.getMessage().isNotEmpty());
    CHECK (! into.getChildFile (libraries::salamanderGrand().folder).exists());
    CHECK (! into.getChildFile (libraries::salamanderGrand().folder + ".tar.gz.download").exists());
}

// The real thing (not run by default; needs the internet): downloads the Salamander Grand from freepats.zenvoid.org
// into PARAGON_LIBRARY_DIR as the plugin would, and checks it unpacked whole.
TEST_CASE ("Libraries: the real Salamander Grand download", "[.librarylive]")
{
    const juce::File into (juce::SystemStats::getEnvironmentVariable ("PARAGON_LIBRARY_DIR", "/tmp/pr7k-libraries"));
    libraries::Download download;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    download.start (libraries::salamanderGrand(), into);
    REQUIRE (download.waitUntilFinished (1800000));
    INFO (download.getMessage());
    REQUIRE (download.getState() == libraries::Download::State::done);
    const auto folder = into.getChildFile (libraries::salamanderGrand().folder);
    const auto samples = folder.getChildFile ("samples").findChildFiles (juce::File::findFiles, false, "*.flac").size();
    WARN ("downloaded and unpacked in " << juce::String ((juce::Time::getMillisecondCounterHiRes() - start) / 1000.0, 0) << " s, "
          << samples << " samples");
    CHECK (samples == 641);
}

// Clips of the piano presets (not run by default): PARAGON_LIBRARIES (a folder holding the Salamander's), PARAGON_RENDER.
// Chords with the pedal changed each bar, then a melody over them; 70 BPM in A minor.
#include "rigs/FactoryRigs.h"
TEST_CASE ("Render the piano presets", "[.pianorender]")
{
    MainView::offerLibrary = false;
    const juce::File source (juce::SystemStats::getEnvironmentVariable ("PARAGON_LIBRARIES", {}));
    const auto& library = libraries::salamanderGrand();
    REQUIRE (source.getChildFile (library.folder).isDirectory());
    const auto folder = juce::File (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER", "/tmp/paragon-render"));
    folder.createDirectory();
    TempFolder presets;
    ParagonX1AudioProcessor processor;
    processor.useRigFolder (presets.dir);
    processor.getLibrariesFolder().createDirectory();
    REQUIRE (source.getChildFile (library.folder).createSymbolicLink (processor.getLibrariesFolder().getChildFile (library.folder), true));
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);

    struct Event { double beat; int type; int note; float velocity; };
    std::vector<Event> events;
    const std::vector<std::vector<int>> chords { { 45, 57, 60, 64 }, { 41, 53, 57, 60 }, { 48, 55, 60, 64 }, { 43, 55, 59, 62 } };
    for (int bar = 0; bar < 4; ++bar)
    {
        events.push_back ({ bar * 4.0 + 0.05, 2, 64, 1.0f });
        events.push_back ({ bar * 4.0 + 3.95, 2, 64, 0.0f });
        for (size_t k = 0; k < 4; ++k)
        {
            events.push_back ({ bar * 4.0 + 0.02 * (double) k, 0, chords[(size_t) bar][k], k == 0 ? 0.6f : 0.5f });
            events.push_back ({ bar * 4.0 + 3.8, 1, chords[(size_t) bar][k], 0.0f });
        }
    }
    const int melody[] = { 76, 74, 72, 71, 72, 74, 76, 79, 77, 76, 74, 72, 74, 71, 69, 69 };
    for (int k = 0; k < 16; ++k)
    {
        events.push_back ({ 16.0 + k, 0, melody[k], 0.45f + 0.04f * (float) (k % 4) });
        events.push_back ({ 16.0 + k + (k % 4 == 3 ? 1.8 : 0.9), 1, melody[k], 0.0f });
    }
    for (int bar = 0; bar < 4; ++bar)
    {
        events.push_back ({ 16.0 + bar * 4.0 + 0.05, 2, 64, 1.0f });
        events.push_back ({ 16.0 + bar * 4.0 + 3.95, 2, 64, 0.0f });
        events.push_back ({ 16.0 + bar * 4.0, 0, chords[(size_t) bar][0], 0.5f });
        events.push_back ({ 16.0 + bar * 4.0 + 3.8, 1, chords[(size_t) bar][0], 0.0f });
    }
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.beat < b.beat; });

    // PARAGON_RENDER_FORTE=1: everything as hard as it goes (the loudest a preset gets).
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER_FORTE", {}) == "1")
        for (auto& e : events)
            if (e.type == 0)
                e.velocity = 1.0f;
    const auto only = juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER_ONLY", {});
    for (const auto& rig : makeFactoryRigs())
    {
        if (rig.sfz.empty() || (only.isNotEmpty() && ! only.containsIgnoreCase (rig.name)))
            continue;
        RigLibrary::Entry entry;
        entry.rig = rig;
        entry.isFactory = true;
        processor.loadRig (entry);
        REQUIRE (processor.waitForSfz (60000));
        processor.prepareToPlay (48000.0, 256);
        processor.seedRandomForTest (1);
        processor.setNonRealtime (true);
        const auto secondsPerBeat = 60.0 / 70.0;
        const auto total = (int) ((32.0 * secondsPerBeat + 5.0) * 48000.0);
        juce::AudioBuffer<float> out (2, total + 256), block (2, 256);
        size_t next = 0;
        for (int at = 0; at < total; at += 256)
        {
            juce::MidiBuffer midi;
            while (next < events.size() && (int) (events[next].beat * secondsPerBeat * 48000.0) < at + 256)
            {
                const auto& e = events[next++];
                const auto offset = juce::jlimit (0, 255, (int) (e.beat * secondsPerBeat * 48000.0) - at);
                midi.addEvent (e.type == 0 ? juce::MidiMessage::noteOn (1, e.note, e.velocity)
                             : e.type == 1 ? juce::MidiMessage::noteOff (1, e.note)
                                           : juce::MidiMessage::controllerEvent (1, 64, e.velocity > 0.5f ? 127 : 0), offset);
            }
            block.clear();
            processor.processBlock (block, midi);
            for (int c = 0; c < 2; ++c)
                out.copyFrom (c, at, block, c, 0, 256);
        }
        out.setSize (2, total, true);
        const auto file = folder.getChildFile (rig.getSlotName() + " " + rig.name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (2).withBitsPerSample (24));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (out, 0, total);
    }
}

// (0.89.1) Unpacks archives made as GitHub makes them (`git archive`: PARAGON_ARCHIVES, a folder of <folder>.tar.gz)
// with the download's own rules into PARAGON_UNPACK, for checking what the owner's download really holds (not run
// by default: GitHub's archive links can't be fetched here).
TEST_CASE ("Libraries unpack as the download does", "[.unpackcheck]")
{
    const juce::File archives (juce::SystemStats::getEnvironmentVariable ("PARAGON_ARCHIVES", {}));
    const juce::File out (juce::SystemStats::getEnvironmentVariable ("PARAGON_UNPACK", {}));
    REQUIRE (archives.isDirectory());
    for (const auto* library : libraries::all())
    {
        const auto archive = archives.getChildFile (library->folder + ".tar.gz");
        if (! archive.existsAsFile())
            continue;
        INFO (library->name);
        juce::String error;
        const auto& lib = *library;
        CHECK (libraries::extractTarGz (archive, out, {}, nullptr, error, [&lib] (const juce::String& path)
                                        { return ! path.startsWith (lib.folder + "/") || lib.keeps (path.substring (lib.folder.length() + 1)); }));
        CHECK (error.isEmpty());
        CHECK (out.getChildFile (lib.folder).getChildFile (lib.sfz).existsAsFile());
    }
}

// (0.89.1) The real repair of Black And Green Guitars from GitHub's raw files (not run by default): PARAGON_LIBRARIES,
// a folder holding the library unpacked from GitHub's archive (its WAVs altered); they're put back as stored.
TEST_CASE ("Libraries: the real Black And Green Guitars repair", "[.rawrepair]")
{
    const juce::File folder (juce::SystemStats::getEnvironmentVariable ("PARAGON_LIBRARIES", {}));
    const auto& library = libraries::blackAndGreenGuitars();
    REQUIRE (folder.getChildFile (library.folder).getChildFile (library.sfz).existsAsFile());
    folder.getChildFile (library.folder).getChildFile (libraries::exactMarker).deleteFile();
    libraries::Download download;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    download.start (library, folder);
    REQUIRE (download.waitUntilFinished (3600000));
    INFO (download.getMessage());
    CHECK (download.getState() == libraries::Download::State::done);
    CHECK (libraries::isComplete (library, folder.getChildFile (library.folder)));
    WARN ("Repaired in " << (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0 << " s");
}
