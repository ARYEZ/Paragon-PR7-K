#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // AFTERGLOW's effect half (0.60.0, STARCOURT's signature, the owner's name): the glow of a 1980s polysynth after
    // its voices, our own design. (Its other half, each note's own tuning, filter and timing, is in the synth engine:
    // synth::AfterglowSettings.)
    //  - ENSEMBLE: three chorus voices a third of a cycle apart, each swept slowly with a quick shimmer on top, as the
    //    string machines and polysynth ensembles of the era did; the right side runs a sixth of a cycle on from the
    //    left, so a centred sound spreads wide. ENSEMBLE blends it in.
    //  - TAPE: wow (a slow pitch sway) with a little flutter, gentle saturation and a softened top, as if the whole
    //    sound went down to a cassette. TAPE sets how much of all three; at 0 the signal isn't delayed at all, so
    //    turning it up from 0 moves smoothly.
    // At ENSEMBLE and TAPE 0 (or off, once settled) the sound passes exactly as it was.
    class Afterglow
    {
    public:
        struct Settings
        {
            bool on = false;
            float ensemble = 0.0f;   // 0 to 1
            float tape = 0.0f;       // 0 to 1
        };

        static constexpr double baseDelayMs = 7.0;     // the ensemble's middle delay
        static constexpr double sweepMs = 2.4;         // ...its slow sweep, either way
        static constexpr double shimmerMs = 0.22;      // ...its quick shimmer
        static constexpr double sweepHz = 0.63, shimmerHz = 5.7;
        static constexpr double wowMs = 1.6;           // TAPE at 1: the wow's depth (about 9 cents at 0.55 Hz)
        static constexpr double wowHz = 0.55, flutterHz = 7.3, flutterMs = 0.06;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;

        bool isResting() const noexcept { return ensembleNow <= 0.0f && tapeNow <= 0.0f; }
        // For the page: the three ensemble voices' delays now (ms), the wow's pitch now (cents) and how hard the tape works (0-1).
        float getVoiceDelayMs (int v) const noexcept { return voiceShown[(size_t) juce::jlimit (0, 2, v)].load (std::memory_order_relaxed); }
        float getWowCents() const noexcept { return wowShown.load (std::memory_order_relaxed); }
        float getEnsembleNow() const noexcept { return ensembleShown.load (std::memory_order_relaxed); }
        float getTapeNow() const noexcept { return tapeShown.load (std::memory_order_relaxed); }

    private:
        struct Line
        {
            std::vector<float> data;
            int write = 0;
            void push (float x) noexcept { data[(size_t) write] = x; write = write + 1 == (int) data.size() ? 0 : write + 1; }
            float read (double samplesAgo) const noexcept;   // samplesAgo >= 0 (0: the sample just pushed)
        };

        double sampleRate = 48000.0;
        std::array<Line, 2> ensembleLines, tapeLines;
        std::array<float, 2> toneState {};
        double sweepPhase = 0.0, shimmerPhase = 0.0, wowPhase = 0.0, flutterPhase = 0.0;
        float ensembleNow = 0.0f, tapeNow = 0.0f, smoothing = 0.0f;
        std::array<std::atomic<float>, 3> voiceShown {};
        std::atomic<float> wowShown { 0.0f }, ensembleShown { 0.0f }, tapeShown { 0.0f };
    };
}
