#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>

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
    };

    // The Salamander Grand Piano (Yamaha C5, 16 velocity layers, by Alexander Holm, CC-BY 3.0), the FreePats project's
    // SFZ + FLAC edition (about 740 MB to download, 714 MB unpacked).
    const Library& salamanderGrand();

    // "{sfz:<folder>}/<sfz>": how presets name the library's instrument.
    juce::String refFor (const Library&);

    // Unpacks a .tar.gz into `destination` (regular files and folders; links and anything naming a place outside it
    // are skipped). `progress` 0-1 by the compressed bytes read. False (and why, in `error`) if it isn't a readable
    // tar.gz or `cancel` was set.
    bool extractTarGz (const juce::File& archive, const juce::File& destination, std::function<void (float)> progress,
                       const std::atomic<bool>* cancel, juce::String& error);

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
        float getProgress() const noexcept { return progress.load(); }   // of the step it's on, 0-1
        juce::String getMessage() const;                                   // why it failed
        bool isRunning() const noexcept { const auto s = state.load(); return s == State::downloading || s == State::unpacking; }
        bool waitUntilFinished (int timeoutMs);                            // tests

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
