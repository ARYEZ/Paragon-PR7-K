#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// SAMPLE LIBRARIES (0.71.0, the owner's choice: "GET THE PIANO" in the plugin). Free sample libraries the factory
// presets play, downloaded by PR7-K itself into the presets folder's "Libraries" (never shipped with the plugin, so
// the release stays small): the archive fetched from its publisher, unpacked, and found by the presets'
// "{sfz:<folder>}/<file>" references.
namespace libraries
{
    struct Library
    {
        juce::String name;          // as the plugin says it
        juce::String url;           // the archive (.tar.gz)
        juce::int64 bytes = 0;      // its size (checked: a cut-off download isn't unpacked)
        juce::String folder;        // the folder it unpacks into
        juce::String sfz;           // the instrument file presets name, inside `folder`
        juce::String credit;        // its licence's credit (ABOUT shows it)
        juce::String what;          // what the plugin calls it in short ("PIANO": GET THE PIANO)
        bool exactSize = true;      // `bytes` is exact (a cut-off download is caught by it); else about (progress only)
        int unpackedMb = 0;         // what it takes on disk once unpacked
        // (0.81.0) Paths in the archive (under `folder`) not unpacked: a prefix, or a part of a name (" - Legacy").
        // VCSL's pianos and harpsichords are most of its 6 GB and PR7-K has its own.
        juce::StringArray skipPrefixes, skipContaining;
        bool keeps (const juce::String& pathInFolder) const
        {
            for (const auto& p : skipPrefixes)
                if (pathInFolder.startsWith (p))
                    return false;
            for (const auto& p : skipContaining)
                if (pathInFolder.contains (p))
                    return false;
            return true;
        }
    };

    // The Salamander Grand Piano (Yamaha C5, 16 velocity layers, by Alexander Holm, CC-BY 3.0), the FreePats project's
    // SFZ + FLAC edition (about 740 MB to download, 714 MB unpacked).
    const Library& salamanderGrand();

    // The Sonatina Symphonic Orchestra (0.73.0; Mattias Westlund, Peter Eastman; Creative Commons Sampling Plus 1.0),
    // version 4.0: GitHub's archive of its release (about 1.4 GB; GitHub makes it, so its exact size isn't fixed).
    const Library& sonatinaOrchestra();

    // The Versilian Community Sample Library (0.81.0; Versilian Studios, CC0: public domain), its SFZ edition at a fixed
    // commit: two pipe organs, tubular bells, handbells, a rope-tension field drum and other snares, timpani, gongs,
    // a siren, harps, harpsichords, pianos, recorders, wine glasses and more. GitHub makes the archive (no exact size).
    const Library& versilianCollection();

    // Sam's Sonor (0.81.0; Sam Greene, mapped by kinwie, CC BY-SA 4.0): a Sonor Force 3001 kit, its snare with a left
    // and a right hand for rolls, at a fixed commit of the SFZ Instruments edition.
    const Library& samsSonor();
    // Karoryfer Samples' Big Rusty Drums (an oversized kit), Gogodze Phu Vol I (Ghanaian bobobo drums and a cajon) and
    // Squidpipes (bagpipes) (0.82.0, CC0), each at a fixed commit.
    const Library& bigRustyDrums();
    const Library& gogodzePhu();
    const Library& squidpipes();
    // (0.84.0, for the western bank) Karoryfer Samples' Black And Green Guitars (a Gretsch Anniversary and a Hofner
    // Club, with twang programs), Shinyguitar (an archtop, acoustic and electric) and Meatbass (a 1958 double bass),
    // CC0; itsclipping's ganjo (a 6-string banjo, CC0); FlameStudios' Kay 5-String Banjo (GPL-3.0-or-later, SFZ by
    // S. Christian Collins). Each at a fixed commit of its SFZ Instruments repository.
    const Library& blackAndGreenGuitars();
    const Library& shinyguitar();
    const Library& meatbass();
    const Library& ganjo();
    const Library& kayBanjo();

    // Every library PR7-K can download.
    std::vector<const Library*> all();

    // "{sfz:<folder>}/<sfz>": how presets name the library's instrument; with `sfzPath`, another instrument in it.
    juce::String refFor (const Library&);
    juce::String refFor (const Library&, const juce::String& sfzPath);

    // The library a preset's SFZ reference is in (null: none PR7-K downloads).
    const Library* libraryFor (const juce::String& ref);

    // Unpacks a .tar.gz into `destination` (regular files and folders; links and anything naming a place outside it
    // are skipped). `progress` 0-1 by the compressed bytes read. False (and why, in `error`) if it isn't a readable
    // tar.gz or `cancel` was set.
    // (0.81.0) `keep`, when given, decides per path (as in the archive) whether it's unpacked.
    bool extractTarGz (const juce::File& archive, const juce::File& destination, std::function<void (float)> progress,
                       const std::atomic<bool>* cancel, juce::String& error,
                       std::function<bool (const juce::String&)> keep = {});

    // Fetches and unpacks a library on a thread of its own: `destination` / library.folder once it's done.
    class Download final : private juce::Thread
    {
    public:
        enum class State { idle, downloading, unpacking, done, failed };

        Download();
        ~Download() override;

        // Starts (if not already running). `url` overrides the library's (tests: a file:// URL).
        void start (const Library&, const juce::File& destination, const juce::String& url = {});
        void cancel();

        State getState() const noexcept { return state.load(); }
        const Library& getLibrary() const noexcept { return library; }   // the one it's fetching (or last fetched)
        float getProgress() const noexcept { return progress.load(); }   // of the step it's on, 0-1
        juce::String getMessage() const;                                   // why it failed
        bool isRunning() const noexcept { const auto s = state.load(); return s == State::downloading || s == State::unpacking; }
        bool waitUntilFinished (int timeoutMs);                            // tests
        // Snapshots: as if `library` were at `state`, `progress` of the way (nothing is fetched).
        void showForTest (const Library& l, State s, float p) { library = l; progress = p; state = s; }

    private:
        void run() override;
        void fail (const juce::String&);
        Library library;
        juce::File destination;
        juce::String sourceUrl;
        std::atomic<State> state { State::idle };
        std::atomic<float> progress { 0.0f };
        std::atomic<bool> cancelled { false };
        mutable juce::CriticalSection messageLock;
        juce::String message;
    };
}
