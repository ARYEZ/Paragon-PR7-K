#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // REMNANT (0.64.0, DREAMSTATE's signature, the owner's name): dreams within dreams; our own design. Your sound
    // passes untouched; under it, up to three layers of what you just played, each a level deeper: slower and lower
    // than the one above, all running at once. Unlike DILATION (which replaces the sound with one slowed stream that
    // slides in and catches up), nothing here slides or catches up: each layer runs at one fixed speed for ever.
    //
    // Each layer is a dream of the one above it: layer 1 hears your sound, layer 2 hears layer 1, layer 3 hears layer
    // 2. Each keeps the last few seconds of what it hears and plays them back at STEP's speed (an octave: half speed;
    // a fifth: two thirds; a fourth: three quarters), so layer k sounds STEP x k down and is stretched k times over.
    // A layer reads with two heads half a LENGTH apart, each fading in and out over LENGTH (raised cosine: the two add
    // to exactly one), every head starting from (just behind) the present: so a layer never falls further behind than
    // LENGTH's worth and renews itself smoothly, never with a jump. A fresh head starts where its waveform lines up
    // with the other head's (searched over the last 20 ms): without that, two heads on a held note could land out of
    // step and cancel (the first build lost 20 dB on a sine, the deeper layers nearly all of it). TAPER makes each deeper layer quieter, TONE darker; WIDTH leans
    // the layers to alternate sides (each side kept its own); MIX how loud the layers are under your sound (0: nothing
    // at all, bit for bit).
    class Remnant
    {
    public:
        static inline const juce::StringArray layerNames { "1", "2", "3" };                // never reorder
        static inline const juce::StringArray stepNames { "OCTAVE", "FIFTH", "FOURTH" };  // never reorder
        static constexpr int maxLayers = 3;

        struct Settings
        {
            bool on = false;
            int layers = 1;        // layerNames: 2 layers
            int step = 0;          // stepNames
            float length = 0.5f;   // 0 to 1: each stretch of the past, 0.4 s to 4 s
            float taper = 0.3f;    // 0 to 1: each deeper layer 0 to 12 dB quieter
            float tone = 0.5f;     // 0 to 1
            float width = 0.6f;    // 0 to 1
            float mix = 0.0f;      // 0 to 1
        };

        static float speedFor (int step) noexcept { return step == 1 ? 2.0f / 3.0f : step == 2 ? 0.75f : 0.5f; }
        static float semitonesFor (int step) noexcept { return 12.0f * std::log2 (speedFor (step)); }
        static float lengthSeconds (float length) noexcept { return 0.4f * std::pow (10.0f, juce::jlimit (0.0f, 1.0f, length)); }
        static float taperDb (float taper) noexcept { return -12.0f * juce::jlimit (0.0f, 1.0f, taper); }
        static float toneHz (float tone) noexcept { return 1500.0f * std::pow (16000.0f / 1500.0f, juce::jlimit (0.0f, 1.0f, tone)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return mixNow <= 0.0f; }

        // For the page and the unit: each layer's level now (dB, -100 when silent), how far through its head 0 is
        // (0-1), and how far behind the present its heads are reading (seconds).
        float getLayerDb (int layer) const noexcept { return layerShown[(size_t) juce::jlimit (0, maxLayers - 1, layer)].load (std::memory_order_relaxed); }
        float getLayerPhase (int layer) const noexcept { return phaseShown[(size_t) juce::jlimit (0, maxLayers - 1, layer)].load (std::memory_order_relaxed); }
        float getTotalDb() const noexcept { return totalShown.load (std::memory_order_relaxed); }

    private:
        struct Head
        {
            double lag = 0.0;   // samples behind the newest written
            int age = 0;
        };
        struct Layer
        {
            std::array<std::vector<float>, 2> line;   // what this layer hears
            std::array<Head, 2> heads;
            std::array<float, 2> toneState {};
            float power = 0.0f;
        };

        float read (const std::vector<float>& line, double back) const noexcept;
        double alignedStart (const Layer&, double otherLag, double speed) const noexcept;
        void clearLayers() noexcept;

        double sampleRate = 48000.0;
        std::array<Layer, maxLayers> layers;
        int writeIndex = 0, lineSize = 0, headLength = 0;
        float mixNow = 0.0f, smoothing = 0.0f;
        std::array<std::atomic<float>, maxLayers> layerShown {}, phaseShown {};
        std::atomic<float> totalShown { -100.0f };
    };
}
