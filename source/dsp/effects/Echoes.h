#pragma once

#include "EffectUtils.h"

#include <array>
#include <vector>

namespace effects
{
    // ECHOES (0.53.0, the owner's name; the signature of the PARADOX expansion): what you played comes back from the
    // past. Our own design. It keeps a running recording of the last seconds of the sound and, mixed under what's
    // playing now, plays it back from a set distance in the song (a beat to eight bars), in time with the song:
    //
    //  - ECHO:    the sound from DISTANCE ago, exactly (a long, bar-synced delay); FEEDBACK makes echoes of echoes.
    //  - REVERSE: the song cut into stretches DISTANCE long; each one, while the next plays, comes back backwards.
    //  - TAPE:    the song cut into stretches twice DISTANCE long; each one's first half (where its phrase starts) comes
    //             back at half speed, an octave down, over the whole of the next.
    //  - LOOP:    a stretch is caught and repeated, LOOP times or for ever, while you play on over it; then the next.
    //
    // AGE wears each pass back down (darker and a little saturated), so older echoes sound older. MIX is how loud the
    // past comes back under the present (the mod matrix can move it: the EXPR wheel, MACRO 3...). Stereo: each side its
    // own. Stretch edges are faded over 5 ms so cuts don't click.
    class Echoes
    {
    public:
        // Saved by number: only append.
        enum class Mode { echo, reverse, tape, loop };
        static inline const juce::StringArray modeNames { "Echo", "Reverse", "Tape", "Loop" };
        // DISTANCE, in beats (4 to a bar). Saved by number: only append.
        static constexpr float distanceBeats[] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };
        static inline const juce::StringArray distanceNames { "1 beat", "2 beats", "1 bar", "2 bars", "4 bars", "8 bars" };
        static constexpr double maxSeconds = 32.0;   // the recording kept (TAPE reaches back 3 distances: longer ones are halved to fit)

        struct Settings
        {
            bool on = false;
            Mode mode = Mode::echo;
            int distance = 2;          // distanceNames: 1 bar
            float mix = 0.5f;          // 0 to 1: how loud the past comes back
            float feedback = 0.3f;     // 0 to 1: echoes of echoes; in LOOP, how many times a stretch repeats (1 = for ever)
            float age = 0.3f;          // 0 to 1: how much each pass back is worn down
        };

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        // `ppq`: the song position at the block's start in beats (negative while stopped: then it keeps its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;

        // How far back DISTANCE reaches at `bpm`, in samples (halved until it fits the recording).
        int distanceSamples (int distance, double bpm) const noexcept;
        // LOOP's repeats for a FEEDBACK (1 to 8; 0 = for ever, at the top).
        static int loopRepeats (float feedback) noexcept;
        // AGE's low-pass corner: 18 kHz new to 1.8 kHz old.
        static float ageCutoffHz (float age) noexcept { return 18000.0f * std::pow (0.1f, juce::jlimit (0.0f, 1.0f, age)); }

        // For the page: the recording's level over its last `count` stretches of `blockSeconds`, and where the
        // stretch now coming back sits (0 = now, 1 = the oldest shown).
        struct Picture { std::vector<float> levels; float readFrom = 0.0f, readTo = 0.0f; bool backwards = false; };
        Picture picture (int columns, double bpm, const Settings&) const;
        // What came back in the last block, at its loudest (after MIX): the unit's ladder.
        float getPastPeak() const noexcept { return pastPeak.load (std::memory_order_relaxed); }

    private:
        float readAt (int channel, double samplesAgo) const noexcept;   // the recording, `samplesAgo` before the write head
        void catchLoop (int length) noexcept;                            // LOOP: copy the last `length` samples
        double sampleRate = 48000.0;
        BypassFade bypass;
        std::array<std::vector<float>, 2> recording;   // ring buffers
        std::array<std::vector<float>, 2> loopCopy;    // LOOP's caught stretch
        int capacity = 0, writeHead = 0;
        double ownClock = 0.0;            // samples counted while the song is stopped
        long long lastSegment = -1;
        int loopLength = 0, loopRepeatsLeft = 0, loopCaught = 0;
        float loopPeak = 0.0f;
        std::array<float, 2> ageState {};
        std::atomic<int> shownRead { 0 };
        std::atomic<float> shownSegmentPos { 0.0f };
        std::atomic<float> pastPeak { 0.0f };
    };
}
