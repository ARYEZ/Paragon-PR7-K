#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

// DISK STREAMING for the SFZ oscillator (0.71.0), ported from PR7-D 0.28.0's `DiskStreamer` (its design and code),
// cut down to what a sampled instrument needs: one file, one stereo output a stream (PR7-D's hits had up to 12 mics).
//
//   - Each sample keeps only its first part (its HEAD) in memory, so a note starts at once.
//   - When a note starts, it claims a STREAM: a background thread reads the rest of its file, ahead of where it's
//     playing, into a ring (2^15 frames: 0.68 s at 48 kHz, room for a sample played up to an octave higher).
//   - The audio thread never waits for the disk while playing live: if the disk ever falls behind, the missing
//     samples are silent (and counted). Bouncing (non-realtime) waits for them instead, so a bounce is always whole.
//   - A stream is freed when no note plays it any more.
namespace synth::sfz
{
    // Where a streamed sample's audio comes from.
    struct StreamSource
    {
        juce::File file;
        double sampleRate = 48000.0;
        juce::int64 length = 0;                 // frames in the file
        int headFrames = 0;                     // frames the sample holds in memory (from the start)
        int numChannels = 2;                    // the file's (1: mono, read into both sides)
    };

    class Streamer
    {
    public:
        static constexpr int maxStreams = 128;          // notes at once that stream (a held pedal piano needs many)
        static constexpr int ringFrames = 1 << 15;
        static constexpr int chunkFrames = 2048;

        Streamer();
        ~Streamer();

        // Audio thread: a stream for a note (-1: none free: the note plays its head only); where the note has read
        // up to; a frame of a channel (0: not read yet, and counted, unless `waitForDisk`: bouncing).
        int claim (const StreamSource&) noexcept;
        void consumed (int stream, juce::int64 frame) noexcept;
        float sample (int stream, int channel, juce::int64 frame, bool waitForDisk) noexcept;
        // Audio thread, once a block: the streams no note plays any more are freed.
        void release (const std::array<bool, maxStreams>& inUse) noexcept;

        int getUnderruns() const noexcept { return underruns.load(); }   // samples the disk was late for, live
        int getActiveStreams() const noexcept;
        // Tests: waits (up to `timeoutMs`) until every playing stream is as full as it can be; true if it got there.
        bool waitUntilFilled (int timeoutMs) noexcept;

    private:
        struct Stream
        {
            std::atomic<int> state { 0 };       // 0 free, 1 being set up, 2 playing, 3 released
            const StreamSource* source = nullptr;
            std::atomic<juce::int64> written { 0 };   // frames ready (from the file's start; the head counts)
            std::atomic<juce::int64> read { 0 };      // where the note has read up to
            std::vector<float> ring;            // 2 channels x ringFrames
            std::mutex fillLock;
        };
        std::vector<std::unique_ptr<Stream>> streams;
        std::atomic<int> underruns { 0 };

        class Thread;
        std::unique_ptr<Thread> thread;
        bool fill (Stream&);
        std::mutex readersLock;
        std::map<juce::String, std::unique_ptr<juce::AudioFormatReader>> readers;   // open files (the last 192)
        std::vector<juce::String> readerOrder;
        juce::AudioFormatManager formats;
        juce::AudioBuffer<float> chunk;
        juce::AudioFormatReader* readerFor (const juce::File&);
    };
}
