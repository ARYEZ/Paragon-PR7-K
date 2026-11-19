#include "SampleLibraries.h"

#include "RarArchive.h"

#include <cstring>

namespace libraries
{
    const Library& salamanderGrand()
    {
        static const Library library {
            "Salamander Grand Piano",
            "https://freepats.zenvoid.org/Piano/SalamanderGrandPiano/SalamanderGrandPiano-SFZ+FLAC-V3+20200602.tar.gz",
            741757374,
            "SalamanderGrandPiano-SFZ+FLAC-V3+20200602",
            "SalamanderGrandPiano-V3+20200602.sfz",
            "Salamander Grand Piano by Alexander Holm (CC-BY 3.0), FreePats edition by Roberto (freepats.zenvoid.org)",
            "PIANO",
            true,
            714
        };
        return library;
    }

    const Library& sonatinaOrchestra()
    {
        static const Library library {
            "Sonatina Symphonic Orchestra",
            "https://github.com/peastman/sso/archive/refs/tags/v4.0.tar.gz",
            1377280000,                 // (measured from the same tag's git archive: GitHub's is about this)
            "sso-4.0",
            "Sonatina Symphonic Orchestra/Brass - Notation/Horns Sustain.sfz",
            "Sonatina Symphonic Orchestra by Mattias Westlund and Peter Eastman (Creative Commons Sampling Plus 1.0), "
            "github.com/peastman/sso",
            "ORCHESTRA",
            false,
            1420
        };
        return library;
    }

    const Library& versilianCollection()
    {
        static const Library library {
            "Versilian Community Sample Library",
            "https://github.com/sgossner/VCSL/archive/dfcf4a4918771eee884b96ad4493de82ef84daf6.tar.gz",
            3900000000,
            "VCSL-dfcf4a4918771eee884b96ad4493de82ef84daf6",
            "Aerophones/Edge-blown Aerophones/Pipe Organ - Loud.sfz",
            "Versilian Community Sample Library by Versilian Studios (CC0; its pipe organ sampled by Simon Dalzell of Ivy Audio), github.com/sgossner/VCSL",
            "COLLECTION",
            false,
            2500,
            { "Chordophones/Zithers/Grand Piano", "Chordophones/Zithers/Upright Piano", "Chordophones/Zithers/Harpsichord",
              "Electrophones/", "Aerophones/Reed Aerophones/", "Assets/" },
            { " - Legacy" }
        };
        return library;
    }

    const Library& samsSonor()
    {
        static const Library library {
            "Sam's Sonor",
            "https://github.com/sfzinstruments/SamsSonor/archive/ef3e32058924ed1c3335e86d03094d3f310a9104.tar.gz",
            36400000,
            "SamsSonor-ef3e32058924ed1c3335e86d03094d3f310a9104",
            "SamsSonor.sfz",
            "Sam's Sonor by Sam Greene, SFZ mapping by kinwie (CC BY-SA 4.0), github.com/sfzinstruments/SamsSonor",
            "DRUM KIT",
            false,
            36
        };
        return library;
    }

    // Karoryfer Samples' free libraries (0.82.0, for SANDSEA; CC0: public domain), each at a fixed commit of its
    // GitHub repository.
    const Library& bigRustyDrums()
    {
        static const Library library {
            "Big Rusty Drums",
            "https://github.com/sfzinstruments/karoryfer.big-rusty-drums/archive/f07ce00df34a46b6b08375be56fe116cf15782bc.tar.gz",
            730000000,
            "karoryfer.big-rusty-drums-f07ce00df34a46b6b08375be56fe116cf15782bc",
            "Programs/01-full.sfz",
            "Big Rusty Drums by Karoryfer Samples (CC0), github.com/sfzinstruments/karoryfer.big-rusty-drums",
            "BIG DRUMS",
            false,
            710
        };
        return library;
    }

    const Library& gogodzePhu()
    {
        static const Library library {
            "Gogodze Phu Vol I",
            "https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-i/archive/7452f62060cd3a468c78bc361d96a4dfb30879c9.tar.gz",
            85000000,
            "karoryfer.gogodze-phu-vol-i-7452f62060cd3a468c78bc361d96a4dfb30879c9",
            "Programs/Bobobo.sfz",
            "Gogodze Phu Vol I by Karoryfer Samples (CC0), github.com/sfzinstruments/karoryfer.gogodze-phu-vol-i",
            "HAND DRUMS",
            false,
            90
        };
        return library;
    }

    const Library& squidpipes()
    {
        static const Library library {
            "Squidpipes",
            "https://github.com/sfzinstruments/karoryfer.squidpipes/archive/b258528c8f49d6389ec2b4ec04a8b10013169dd9.tar.gz",
            50000000,
            "karoryfer.squidpipes-b258528c8f49d6389ec2b4ec04a8b10013169dd9",
            "Programs/01-squidpipes.sfz",
            "Squidpipes by Karoryfer Samples (CC0), github.com/sfzinstruments/karoryfer.squidpipes",
            "BAGPIPES",
            false,
            53
        };
        return library;
    }

    // The western bank's guitars, bass and banjos (0.84.0), each at a fixed commit of its GitHub repository.
    const Library& blackAndGreenGuitars()
    {
        static const Library library {
            "Black And Green Guitars",
            "https://github.com/sfzinstruments/karoryfer.black-and-green-guitars/archive/b3b3249d37dc977a1a297bd2dc053e6d9b6b805c.tar.gz",
            560000000,
            "karoryfer.black-and-green-guitars-b3b3249d37dc977a1a297bd2dc053e6d9b6b805c",
            "Programs/04-green_twang.sfz",
            "Black And Green Guitars by Karoryfer Samples (CC0), github.com/sfzinstruments/karoryfer.black-and-green-guitars",
            "TWANG GUITARS",
            false,
            600,
            {}, {},
            // (0.89.1) Its archive alters the WAVs (see `rawBase`): each is fetched again as stored.
            "https://raw.githubusercontent.com/sfzinstruments/karoryfer.black-and-green-guitars/b3b3249d37dc977a1a297bd2dc053e6d9b6b805c/"
        };
        return library;
    }

    const Library& shinyguitar()
    {
        static const Library library {
            "Shinyguitar",
            "https://github.com/sfzinstruments/karoryfer.shinyguitar/archive/57243cca85277dbcc120ce17c6178032f93c80f3.tar.gz",
            440000000,
            "karoryfer.shinyguitar-57243cca85277dbcc120ce17c6178032f93c80f3",
            "Programs/main.sfz",
            "Shinyguitar by Karoryfer Samples (CC0), github.com/sfzinstruments/karoryfer.shinyguitar",
            "ARCHTOP GUITAR",
            false,
            480
        };
        return library;
    }

    const Library& meatbass()
    {
        static const Library library {
            "Meatbass",
            "https://github.com/sfzinstruments/karoryfer.meatbass/archive/ac9e859564bda286ab5ec672d00ff1aa2fef2895.tar.gz",
            280000000,
            "karoryfer.meatbass-ac9e859564bda286ab5ec672d00ff1aa2fef2895",
            "Programs/04_pizz.sfz",
            "Meatbass by Karoryfer Samples (CC0), github.com/sfzinstruments/karoryfer.meatbass",
            "UPRIGHT BASS",
            false,
            310
        };
        return library;
    }

    const Library& ganjo()
    {
        static const Library library {
            "ganjo",
            "https://github.com/sfzinstruments/ganjo/archive/ccff5cd5cd3b513873a48994c07724d9d3c39e1c.tar.gz",
            25000000,
            "ganjo-ccff5cd5cd3b513873a48994c07724d9d3c39e1c",
            "ganjo.sfz",
            "ganjo by itsclipping (CC0), github.com/sfzinstruments/ganjo",
            "GUITAR BANJO",
            false,
            27
        };
        return library;
    }

    const Library& kayBanjo()
    {
        static const Library library {
            "Kay 5-String Banjo",
            "https://github.com/sfzinstruments/FlameStudios.Kay5StringBanjo/archive/a773f1625db53a8a4d1bc10098d867561e21fd09.tar.gz",
            400000000,
            "FlameStudios.Kay5StringBanjo-a773f1625db53a8a4d1bc10098d867561e21fd09",
            "FS KayBanjo (release mw1).sfz",
            "Kay 5-String Banjo by FlameStudios, SFZ by S. Christian Collins (GPL-3.0-or-later), github.com/sfzinstruments/FlameStudios.Kay5StringBanjo",
            "BANJO",
            false,
            21,
            { "work/" }   // the author's editing stages (386 MB); the samples played are in samples/ (20 MB)
        };
        return library;
    }

    const Library& unrealKoto()
    {
        static const Library library = []
        {
            Library l;
            l.name = "Unreal Instruments Koto";
            // The maker's own link (unreal-instruments.wixsite.com/unreal-instruments/koto): Google Drive asks a browser
            // to confirm a file it can't scan for viruses; confirm=t says yes at once.
            l.url = "https://drive.usercontent.google.com/download?id=1gJLM4OhADQoTSusjra23xM1NIb2s5WFK&export=download&confirm=t";
            l.bytes = 222005533;            // as of November 2026 (about: the maker may update it; each file's CRC is checked)
            l.folder = "Ui_KOTO";
            l.sfz = "KOTO/#KOTO.sfz";
            l.credit = "13Strings KOTO by Unreal Instruments (free, no credit asked), unreal-instruments.wixsite.com";
            l.what = "KOTO";
            l.exactSize = false;
            l.unpackedMb = 282;
            l.archive = Library::Archive::rar;
            return l;
        }();
        return library;
    }

    const Library& epicChoir()
    {
        static const Library library = []
        {
            Library l;
            l.name = "epic-choir";
            const juce::String base = "https://github.com/thepok/epic-choir/releases/download/v1.0/epic-choir-v1.0-";
            l.url = base + "S.zip";
            l.moreUrls = { base + "A.zip", base + "T.zip", base + "B.zip" };
            l.bytes = 695596194;            // the four together (each about 170 MB)
            l.folder = "epic-choir";
            l.sfz = "S_ah.sfz";
            l.alsoNeeds = { "A_ah.sfz", "T_ah.sfz", "B_ah.sfz" };
            l.credit = "epic-choir by thepok (CC BY 4.0): choir samples derived from VocalSet (Wilkins, Seetharaman, Wahl, Pardo 2018), "
                       "CC BY 4.0, doi.org/10.5281/zenodo.1442513";
            l.what = "CHOIR";
            l.exactSize = false;
            l.unpackedMb = 740;
            l.archive = Library::Archive::zip;
            l.flat = true;
            return l;
        }();
        return library;
    }

    std::vector<const Library*> all()
    {
        return { &salamanderGrand(), &sonatinaOrchestra(), &versilianCollection(), &samsSonor(),
                 &bigRustyDrums(), &gogodzePhu(), &squidpipes(),
                 &blackAndGreenGuitars(), &shinyguitar(), &meatbass(), &ganjo(), &kayBanjo(), &unrealKoto(), &epicChoir() };
    }

    juce::String refFor (const Library& library)
    {
        return refFor (library, library.sfz);
    }

    juce::String refFor (const Library& library, const juce::String& sfzPath)
    {
        return "{sfz:" + library.folder + "}/" + sfzPath;
    }

    const Library* libraryFor (const juce::String& ref)
    {
        for (const auto* library : all())
            if (ref.startsWith ("{sfz:" + library->folder + "}/"))
                return library;
        return nullptr;
    }

    namespace
    {
        // A tar header's number: octal text (GNU's base-256 form for big files too).
        juce::int64 tarNumber (const char* field, int length)
        {
            if ((std::uint8_t) field[0] & 0x80)
            {
                juce::int64 value = 0;
                for (int i = 1; i < length; ++i)
                    value = (value << 8) | (std::uint8_t) field[i];
                return value;
            }
            juce::int64 value = 0;
            for (int i = 0; i < length && field[i] != 0; ++i)
                if (field[i] >= '0' && field[i] <= '7')
                    value = value * 8 + (field[i] - '0');
            return value;
        }

        juce::String tarText (const char* field, int length)
        {
            int n = 0;
            while (n < length && field[n] != 0)
                ++n;
            return juce::String::fromUTF8 (field, n);
        }

        // A path from the archive, inside `destination`, or none if it would lead anywhere else.
        juce::File safeChild (const juce::File& destination, juce::String path)
        {
            path = path.replaceCharacter ('\\', '/');
            while (path.startsWith ("./"))
                path = path.substring (2);
            if (path.isEmpty() || path.startsWithChar ('/') || path.containsChar (':'))
                return {};
            for (const auto& part : juce::StringArray::fromTokens (path, "/", {}))
                if (part == "..")
                    return {};
            return destination.getChildFile (path);
        }
    }

    bool extractTarGz (const juce::File& archive, const juce::File& destination, std::function<void (float)> progress,
                       const std::atomic<bool>* cancel, juce::String& error, std::function<bool (const juce::String&)> keep,
                       std::function<juce::String (const juce::String&)> place)
    {
        auto file = std::make_unique<juce::FileInputStream> (archive);
        if (! file->openedOk())
        {
            error = "Couldn't open " + archive.getFileName();
            return false;
        }
        const auto total = juce::jmax<juce::int64> (1, file->getTotalLength());
        auto* raw = file.get();
        juce::GZIPDecompressorInputStream tar (file.release(), true, juce::GZIPDecompressorInputStream::gzipFormat);
        if (! destination.createDirectory())
        {
            error = "Couldn't make " + destination.getFullPathName();
            return false;
        }
        std::vector<char> buffer (1 << 16);
        juce::String longName;
        auto files = 0;
        for (;;)
        {
            if (cancel != nullptr && cancel->load())
            {
                error = "Cancelled";
                return false;
            }
            char header[512];
            if (tar.read (header, 512) != 512)
                break;
            if (std::all_of (header, header + 512, [] (char c) { return c == 0; }))
                break;   // the end (two empty blocks)
            if (std::memcmp (header + 257, "ustar", 5) != 0 && tarNumber (header + 148, 8) == 0)
            {
                error = archive.getFileName() + " isn't a tar archive";
                return false;
            }
            const auto size = tarNumber (header + 124, 12);
            const auto type = header[156];
            auto name = tarText (header, 100);
            if (std::memcmp (header + 257, "ustar", 5) == 0)
                if (const auto prefix = tarText (header + 345, 155); prefix.isNotEmpty())
                    name = prefix + "/" + name;
            if (longName.isNotEmpty())
            {
                name = longName;
                longName = {};
            }
            const auto padded = (size + 511) / 512 * 512;

            if (type == 'L' || type == 'x' || type == 'g')
            {
                // A long name (GNU) or extended header (POSIX: "length path=...\n"): for the next entry.
                juce::MemoryBlock data;
                if (size > (1 << 20) || (juce::int64) tar.readIntoMemoryBlock (data, (juce::pointer_sized_int) padded) != padded)
                {
                    error = "A damaged entry in " + archive.getFileName();
                    return false;
                }
                const auto* bytes = (const char*) data.getData();
                auto length = (int) size;
                while (length > 0 && bytes[length - 1] == 0)   // (a long name ends with a NUL)
                    --length;
                const auto text = juce::String::fromUTF8 (bytes, length);
                if (type == 'L')
                    longName = text;
                else if (type == 'x')
                    for (const auto& line : juce::StringArray::fromLines (text))
                        if (line.contains (" path="))
                            longName = line.fromFirstOccurrenceOf (" path=", false, false);
                continue;
            }

            auto target = safeChild (destination, place ? place (name) : name);
            if (keep && ! keep (name))
                target = juce::File();   // read past, not written
            if (type == '5')
            {
                if (target != juce::File())
                    target.createDirectory();
            }
            else if (type == '0' || type == 0)
            {
                std::unique_ptr<juce::FileOutputStream> out;
                if (target != juce::File())
                {
                    target.getParentDirectory().createDirectory();
                    target.deleteFile();
                    out = target.createOutputStream();
                    if (out == nullptr || ! out->openedOk())
                    {
                        error = "Couldn't write " + target.getFullPathName();
                        return false;
                    }
                }
                for (juce::int64 left = size; left > 0;)
                {
                    const auto n = (int) juce::jmin<juce::int64> (left, (juce::int64) buffer.size());
                    if (tar.read (buffer.data(), n) != n)
                    {
                        error = archive.getFileName() + " ends early";
                        return false;
                    }
                    if (out != nullptr && ! out->write (buffer.data(), (size_t) n))
                    {
                        error = "Couldn't write " + target.getFullPathName() + " (is the disk full?)";
                        return false;
                    }
                    left -= n;
                }
                tar.skipNextBytes (padded - size);
                ++files;
            }
            else
                tar.skipNextBytes (padded);   // links and the like: left out
            if (progress)
                progress ((float) juce::jlimit (0.0, 1.0, (double) raw->getPosition() / (double) total));
        }
        if (files == 0)
        {
            error = archive.getFileName() + " holds no files";
            return false;
        }
        return true;
    }

    bool extractRar (const juce::File& archive, const juce::File& destination, std::function<void (float)> progress,
                     const std::atomic<bool>* cancel, juce::String& error, std::function<bool (const juce::String&)> keep,
                     std::function<juce::String (const juce::String&)> place)
    {
        RarArchive rar;
        // (UTF-8 paths: dmc_unrar opens them through the Windows API on Windows)
        if (const auto why = rar.open (archive.getFullPathName().toStdString(), [cancel] { return cancel != nullptr && cancel->load(); });
            ! why.empty())
        {
            error = archive.getFileName() + " isn't a readable RAR archive (" + juce::String (why) + ")";
            return false;
        }
        if (! destination.createDirectory())
        {
            error = "Couldn't make " + destination.getFullPathName();
            return false;
        }
        const auto count = rar.count();
        juce::int64 total = 0, done = 0;
        for (size_t i = 0; i < count; ++i)
            if (! rar.isFolder (i))
                total += (juce::int64) rar.size (i);
        auto files = 0;
        for (size_t i = 0; i < count; ++i)
        {
            if (cancel != nullptr && cancel->load())
            {
                error = "Cancelled";
                return false;
            }
            const auto path = juce::String::fromUTF8 (rar.name (i).c_str()).replaceCharacter ('\\', '/');
            if (path.isEmpty() || rar.isFolder (i))
                continue;
            const auto bytes = (juce::int64) rar.size (i);
            auto target = safeChild (destination, place ? place (path) : path);
            if ((keep && ! keep (path)) || target == juce::File())
            {
                done += bytes;   // (left out: nothing written)
                continue;
            }
            target.getParentDirectory().createDirectory();
            target.deleteFile();
            auto out = target.createOutputStream();
            if (out == nullptr || ! out->openedOk())
            {
                error = "Couldn't write " + target.getFullPathName();
                return false;
            }
            const auto why = rar.extract (i, [&out] (const void* data, size_t n) { return out->write (data, n); });
            out.reset();
            if (! why.empty())
            {
                target.deleteFile();
                error = why == "cancelled" ? juce::String ("Cancelled")
                      : why == "write failed" ? "Couldn't write " + target.getFullPathName() + " (is the disk full?)"
                      : path + ": " + juce::String (why);
                return false;
            }
            ++files;
            done += bytes;
            if (progress)
                progress ((float) juce::jlimit (0.0, 1.0, (double) done / (double) juce::jmax<juce::int64> (1, total)));
        }
        if (files == 0)
        {
            error = archive.getFileName() + " holds no files";
            return false;
        }
        return true;
    }

    bool extractZip (const juce::File& archive, const juce::File& destination, std::function<void (float)> progress,
                     const std::atomic<bool>* cancel, juce::String& error, std::function<bool (const juce::String&)> keep,
                     std::function<juce::String (const juce::String&)> place)
    {
        juce::ZipFile zip (archive);
        if (zip.getNumEntries() == 0)
        {
            error = archive.getFileName() + " isn't a readable zip archive";
            return false;
        }
        if (! destination.createDirectory())
        {
            error = "Couldn't make " + destination.getFullPathName();
            return false;
        }
        juce::int64 total = 0, done = 0;
        for (int i = 0; i < zip.getNumEntries(); ++i)
            total += zip.getEntry (i)->uncompressedSize;
        std::vector<char> buffer (1 << 16);
        auto files = 0;
        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            if (cancel != nullptr && cancel->load())
            {
                error = "Cancelled";
                return false;
            }
            const auto* entry = zip.getEntry (i);
            const auto path = entry->filename.replaceCharacter ('\\', '/');
            if (path.isEmpty() || path.endsWithChar ('/') || entry->isSymbolicLink)
                continue;
            auto target = safeChild (destination, place ? place (path) : path);
            if ((keep && ! keep (path)) || target == juce::File())
            {
                done += entry->uncompressedSize;
                continue;
            }
            std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
            if (in == nullptr)
            {
                error = path + " couldn't be read from the archive";
                return false;
            }
            target.getParentDirectory().createDirectory();
            target.deleteFile();
            auto out = target.createOutputStream();
            if (out == nullptr || ! out->openedOk())
            {
                error = "Couldn't write " + target.getFullPathName();
                return false;
            }
            juce::int64 written = 0;
            for (;;)
            {
                if (cancel != nullptr && cancel->load())
                {
                    out.reset();
                    target.deleteFile();
                    error = "Cancelled";
                    return false;
                }
                const auto n = in->read (buffer.data(), (int) buffer.size());
                if (n <= 0)
                    break;
                if (! out->write (buffer.data(), (size_t) n))
                {
                    out.reset();
                    target.deleteFile();
                    error = "Couldn't write " + target.getFullPathName() + " (is the disk full?)";
                    return false;
                }
                written += n;
                if (progress)
                    progress ((float) juce::jlimit (0.0, 1.0, (double) (done + written) / (double) juce::jmax<juce::int64> (1, total)));
            }
            out.reset();
            if (written != entry->uncompressedSize)
            {
                target.deleteFile();
                error = path + " is damaged in the archive (" + juce::String (written) + " of " + juce::String (entry->uncompressedSize) + " bytes)";
                return false;
            }
            ++files;
            done += entry->uncompressedSize;
        }
        if (files == 0)
        {
            error = archive.getFileName() + " holds no files";
            return false;
        }
        if (progress)
            progress (1.0f);
        return true;
    }

    //==========================================================================
    Download::Download() : juce::Thread ("PR7-K library download") {}

    Download::~Download()
    {
        cancelled = true;
        stopThread (10000);
    }

    bool isComplete (const Library& library, const juce::File& folder)
    {
        for (const auto& needed : library.alsoNeeds)
            if (! folder.getChildFile (needed).existsAsFile())
                return false;
        return folder.getChildFile (library.sfz).existsAsFile()
            && (library.rawBase.isEmpty() || folder.getChildFile (exactMarker).existsAsFile());
    }

    juce::MemoryBlock withCrlf (const juce::MemoryBlock& in)
    {
        juce::MemoryOutputStream out (in.getSize() + in.getSize() / 64 + 16);
        const auto* b = static_cast<const char*> (in.getData());
        for (size_t i = 0; i < in.getSize(); ++i)
        {
            if (b[i] == '\n' && (i == 0 || b[i - 1] != '\r'))
                out.writeByte ('\r');
            out.writeByte (b[i]);
        }
        return out.getMemoryBlock();
    }

    void Download::start (const Library& toFetch, const juce::File& folder, const juce::String& url, const juce::String& raw,
                          const juce::StringArray& more)
    {
        if (isRunning() || isThreadRunning())
            return;
        library = toFetch;
        destination = folder;
        sourceUrl = url.isNotEmpty() ? url : toFetch.url;
        moreSources = url.isNotEmpty() ? more : toFetch.moreUrls;
        rawUrl = raw.isNotEmpty() ? raw : toFetch.rawBase;
        cancelled = false;
        progress = 0.0f;
        {
            const juce::ScopedLock lock (messageLock);
            message = {};
        }
        state = State::downloading;
        startThread (juce::Thread::Priority::low);
    }

    void Download::cancel()
    {
        cancelled = true;
    }

    juce::String Download::getMessage() const
    {
        const juce::ScopedLock lock (messageLock);
        return message;
    }

    bool Download::waitUntilFinished (int timeoutMs)
    {
        return waitForThreadToExit (timeoutMs) && ! isRunning();
    }

    void Download::fail (const juce::String& why)
    {
        {
            const juce::ScopedLock lock (messageLock);
            message = cancelled.load() ? juce::String ("Cancelled") : why;
        }
        state = State::failed;
    }

    void Download::run()
    {
        // (0.89.1) A `rawBase` library already unpacked but not yet put back as stored (an earlier version's
        // download): only its audio is fetched again.
        if (rawUrl.isNotEmpty())
        {
            const auto folder = destination.getChildFile (library.folder);
            if (folder.getChildFile (library.sfz).existsAsFile() && ! folder.getChildFile (exactMarker).existsAsFile())
            {
                state = State::unpacking;
                progress = 0.0f;
                if (fetchExact())
                {
                    progress = 1.0f;
                    state = State::done;
                }
                return;
            }
        }

        destination.createDirectory();
        const auto isRar = library.archive == Library::Archive::rar;
        const auto isZip = library.archive == Library::Archive::zip;
        const auto extension = isRar ? juce::String (".rar") : isZip ? juce::String (".zip") : juce::String (".tar.gz");
        juce::StringArray sources { sourceUrl };
        sources.addArray (moreSources);
        const auto parts = sources.size();
        // Unpacked beside the library as "<folder>.partial", then renamed into place whole (0.89.2: unpacked
        // straight into place, a stop part way (the host closed, two copies of the plugin at once, a full disk) left
        // a folder holding its instrument but not all its samples, which counted as installed and played silence).
        // A sibling, not a folder holding the library: that added the long folder name to every path a second time
        // and passed Windows' 260-character limit (0.89.2's first build failed so). (1.6.0) A library in several
        // archives (epic-choir's sections) has each fetched and unpacked in turn into the same place.
        const auto partial = destination.getChildFile (library.folder + ".partial");
        partial.deleteRecursively();
        for (int part = 0; part < parts; ++part)
        {
            const auto url = sources[part];
            const auto archive = destination.getChildFile (library.folder + (parts > 1 ? "." + juce::String (part + 1) : juce::String())
                                                           + extension + ".download");
            archive.deleteFile();
            state = State::downloading;
            // 1. The archive, read in pieces (so it can be cancelled and its progress shown).
            {
                int status = 0;
                std::unique_ptr<juce::InputStream> in (juce::URL (url).createInputStream (
                    juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (20000)
                        .withStatusCode (&status)
                        .withNumRedirectsToFollow (5)));
                if (in == nullptr || (status != 0 && status != 200))
                {
                    partial.deleteRecursively();
                    return fail ("Couldn't reach " + juce::URL (url).getDomain() + (status != 0 ? " (" + juce::String (status) + ")" : juce::String())
                                 + ". Check the internet connection and try again.");
                }
                auto out = archive.createOutputStream();
                if (out == nullptr)
                {
                    partial.deleteRecursively();
                    return fail ("Couldn't write to " + destination.getFullPathName());
                }
                const auto expected = in->getTotalLength() > 0 ? in->getTotalLength() : library.bytes / parts;
                std::vector<char> buffer (1 << 16);
                juce::int64 got = 0;
                for (;;)
                {
                    if (cancelled.load() || threadShouldExit())
                    {
                        out.reset();
                        archive.deleteFile();
                        partial.deleteRecursively();
                        return fail ("Cancelled");
                    }
                    const auto n = in->read (buffer.data(), (int) buffer.size());
                    if (n <= 0)
                        break;
                    if (! out->write (buffer.data(), (size_t) n))
                    {
                        out.reset();
                        archive.deleteFile();
                        partial.deleteRecursively();
                        return fail ("Couldn't save the download (is the disk full? It needs about " + juce::String (2 * library.bytes / 1000000000.0 + 0.1, 1)
                                 + " GB free while it unpacks).");
                    }
                    got += n;
                    progress = (float) (((double) part + juce::jlimit (0.0, 1.0, (double) got / (double) juce::jmax<juce::int64> (1, expected)))
                                        / (double) parts);
                }
                out.reset();
                // (1.2.0) Google Drive answers with a web page, not the file, when too many people fetched it lately.
                if (isRar)
                {
                    juce::FileInputStream head (archive);
                    char magic[4] {};
                    if (head.read (magic, 4) != 4 || std::memcmp (magic, "Rar!", 4) != 0)
                    {
                        const auto page = archive.getSize() < 1000000 ? archive.loadFileAsString() : juce::String();
                        archive.deleteFile();
                        partial.deleteRecursively();
                        return fail (page.containsIgnoreCase ("quota") || page.containsIgnoreCase ("too many users")
                                         ? juce::String ("Google Drive, where its maker keeps it, is holding back downloads of this file: too many "
                                                         "people fetched it lately. Try again later (it can take up to a day).")
                                         : juce::String ("The download wasn't the library (its maker's link may have changed). Try again later."));
                    }
                }
                if (library.exactSize && library.bytes > 0 && got != library.bytes && url == library.url && parts == 1)
                {
                    archive.deleteFile();
                    partial.deleteRecursively();
                    return fail ("The download stopped part way (" + juce::String (got / 1000000) + " of " + juce::String (library.bytes / 1000000)
                                 + " MB). Try again.");
                }
            }

            // 2. Unpacked into "<folder>.partial", then the archive goes.
            state = State::unpacking;
            juce::String error;
            const auto& lib = library;
            const auto unpack = isRar ? extractRar : isZip ? extractZip : extractTarGz;
            const auto ok = unpack (archive, destination,
                                    [this, part, parts] (float p) { progress = ((float) part + p) / (float) parts; }, &cancelled, error,
                                    [&lib] (const juce::String& path)
                                    {
                                        // Only the library's own folder (anything else in the archive is left out).
                                        if (lib.flat)
                                            return lib.keeps (path);
                                        return path.startsWith (lib.folder + "/") && lib.keeps (path.substring (lib.folder.length() + 1));
                                    },
                                    [&lib] (const juce::String& path)
                                    {
                                        return lib.flat ? lib.folder + ".partial/" + path : lib.folder + ".partial" + path.substring (lib.folder.length());
                                    });
            archive.deleteFile();
            if (! ok)
            {
                partial.deleteRecursively();
                return fail ("Couldn't unpack it: " + error);
            }
        }
        state = State::unpacking;
        for (const auto& needed : library.alsoNeeds)
            if (! partial.getChildFile (needed).existsAsFile())
            {
                partial.deleteRecursively();
                return fail ("The download doesn't hold " + needed);
            }
        if (! partial.getChildFile (library.sfz).existsAsFile())
        {
            partial.deleteRecursively();
            return fail ("The download doesn't hold " + library.sfz);
        }
        destination.getChildFile (library.folder).deleteRecursively();   // a folder left from an earlier try
        if (! partial.moveFileTo (destination.getChildFile (library.folder)))
        {
            partial.deleteRecursively();
            return fail ("Couldn't move the library into " + destination.getFullPathName());
        }
        // 3. (0.89.1) Audio the archive altered, fetched again as stored.
        if (rawUrl.isNotEmpty())
        {
            progress = 0.0f;
            if (! fetchExact())
                return;
        }
        progress = 1.0f;
        state = State::done;
    }

    bool Download::fetchExact()
    {
        const auto folder = destination.getChildFile (library.folder);
        juce::Array<juce::File> audio;
        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles))
        {
            const auto extension = entry.getFile().getFileExtension().toLowerCase();
            if (extension == ".wav" || extension == ".flac" || extension == ".ogg" || extension == ".aif" || extension == ".aiff")
                audio.add (entry.getFile());
        }
        auto checked = 0;
        for (const auto& file : audio)
        {
            if (cancelled.load() || threadShouldExit())
            {
                fail ("Cancelled");
                return false;
            }
            juce::MemoryBlock have;
            if (! file.loadFileAsData (have))
            {
                fail ("Couldn't read " + file.getFullPathName());
                return false;
            }
            // Only a file holding a CR LF pair can have been altered (a CR goes in before each lone LF).
            const auto* b = static_cast<const char*> (have.getData());
            auto altered = false;
            for (size_t i = 1; i < have.getSize() && ! altered; ++i)
                altered = b[i] == '\n' && b[i - 1] == '\r';
            if (altered)
            {
                const auto relative = file.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
                juce::StringArray parts;
                for (const auto& part : juce::StringArray::fromTokens (relative, "/", {}))
                    parts.add (juce::URL::addEscapeChars (part, false));
                const auto url = rawUrl + parts.joinIntoString ("/");
                int status = 0;
                std::unique_ptr<juce::InputStream> in (juce::URL (url).createInputStream (
                    juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (20000)
                        .withStatusCode (&status)
                        .withNumRedirectsToFollow (5)));
                if (in == nullptr || (status != 0 && status != 200))
                {
                    fail ("Couldn't fetch " + relative + " from " + juce::URL (url).getDomain()
                          + (status != 0 ? " (" + juce::String (status) + ")" : juce::String()) + ". Check the internet connection and try again.");
                    return false;
                }
                juce::MemoryBlock stored;
                in->readIntoMemoryBlock (stored);
                // The check: git's change applied to what came back must give exactly the archive's copy.
                if (withCrlf (stored) != have)
                {
                    fail ("A sample didn't match its stored copy (" + relative + "). Try again.");
                    return false;
                }
                if (! file.replaceWithData (stored.getData(), stored.getSize()))
                {
                    fail ("Couldn't write " + file.getFullPathName());
                    return false;
                }
            }
            progress = (float) ++checked / (float) juce::jmax (1, audio.size());
        }
        folder.getChildFile (exactMarker).replaceWithText ("The audio here was checked against the stored copies by Paragon PR7-K.\n");
        return true;
    }
}
