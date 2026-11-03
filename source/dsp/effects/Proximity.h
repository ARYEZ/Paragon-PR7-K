#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace effects
{
    // PROXIMITY (0.65.0, NEGATIVE SPACE's signature, the owner's name): the sound of playing, heard up close; our own
    // design, every noise made here (no recordings). The sound passes through (only CLOSE touches it); added to it:
    // - NOISE: the touches of playing, from the synth's own notes (keys and arps) and sustain pedal, on their samples:
    //   a felt hammer's thump as a note starts (a low thud falling in pitch with a soft burst of felt, louder with
    //   velocity, lighter and shorter up the keyboard), a key's click as it's let go with the damper settling on the
    //   string after it, the pedal's thud going down and the dampers' soft hush coming back up. TONE: how bright the
    //   felt and clicks are. WIDTH: each note's noises placed where it is on the keyboard (low left, high right).
    // - ROOM: the room's air and the electronics' hum (HUM: off, 50 or 60 Hz with a little of its harmonics), rising
    //   with the music and dying away a couple of seconds after it, so silence stays silent.
    // - CLOSE: the bass a microphone gains close up (a low shelf at 180 Hz, up to +6 dB) on the sound itself.
    // At NOISE, ROOM and CLOSE 0 (or off, once the last noise has died) the sound passes untouched, bit for bit.
    class Proximity
    {
    public:
        static inline const juce::StringArray humNames { "OFF", "50 HZ", "60 HZ" };   // never reorder

        struct Settings
        {
            bool on = false;
            float noise = 0.5f;   // 0 to 1: the touches, -48 dB to -6 dB (0: none)
            float tone = 0.4f;    // 0 to 1: felt and clicks 800 Hz to 6.4 kHz
            float room = 0.3f;    // 0 to 1: the room and hum, -72 dB to -30 dB (0: none)
            int hum = 1;          // humNames
            float close = 0.3f;   // 0 to 1: 0 to +6 dB at 180 Hz
            float width = 0.6f;   // 0 to 1
        };

        enum class Kind : std::uint8_t { press, release, pedalDown, pedalUp };   // as synth::Engine::Touch::Kind

        static float noiseDb (float noise) noexcept { return -48.0f + 42.0f * juce::jlimit (0.0f, 1.0f, noise); }
        static float roomDb (float room) noexcept { return -72.0f + 42.0f * juce::jlimit (0.0f, 1.0f, room); }
        static float closeDb (float close) noexcept { return 6.0f * juce::jlimit (0.0f, 1.0f, close); }
        static float toneHz (float tone) noexcept { return 800.0f * std::pow (8.0f, juce::jlimit (0.0f, 1.0f, tone)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { state = seed == 0 ? 0x2545f491u : seed; }
        // A touch in the coming block, at `sample` into it. Up to 32 a block.
        void addTouch (int sample, float note, float velocity, Kind) noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept;

        // For the page and the unit: the touches so far (a count, and the last 16: note, kind, velocity), the noises'
        // and the room's levels now (dB, -100 silent), CLOSE's boost now (dB).
        struct Seen { float note = 60.0f, velocity = 0.0f; Kind kind = Kind::press; };
        int getTouchCount() const noexcept { return touchCount.load (std::memory_order_relaxed); }
        Seen getTouch (int ago) const noexcept;
        float getNoiseDb() const noexcept { return noiseShown.load (std::memory_order_relaxed); }
        float getRoomDb() const noexcept { return roomShown.load (std::memory_order_relaxed); }
        float getCloseDb() const noexcept { return closeShown.load (std::memory_order_relaxed); }

    private:
        struct Hit
        {
            bool active = false;
            int delay = 0, age = 0;
            float gainLeft = 1.0f, gainRight = 1.0f;
            // The thud: a sine falling from `freq` to `freqEnd`.
            double phase = 0.0;
            float freq = 0.0f, freqEnd = 0.0f, glide = 0.0f, toneAmp = 0.0f, toneEnv = 0.0f, toneDecay = 0.0f;
            // The burst: noise through a band-pass, an attack then a fall.
            float noiseAmp = 0.0f, noiseEnv = 0.0f, noiseDecay = 0.0f, g = 0.0f, k = 1.0f, ic1 = 0.0f, ic2 = 0.0f;
            int attack = 0;
        };
        struct Pending { int sample; float note, velocity; Kind kind; };
        struct Shelf { float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f; };

        float random01() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (state >> 8) * (1.0f / 16777216.0f);
        }
        Hit& freeHit() noexcept;
        void spawn (const Pending&, const Settings&) noexcept;
        void setBand (Hit&, float hz, float k) const noexcept;
        static Shelf shelfFor (float db, double sampleRate) noexcept;

        double sampleRate = 48000.0;
        std::array<Hit, 24> hits {};
        std::array<Pending, 32> pending {};
        int numPending = 0;
        std::uint32_t state = 0x2545f491u;
        float noiseNow = 0.0f, roomNow = 0.0f, closeNow = 0.0f, smoothing = 0.0f;
        float follow = 0.0f, followUp = 0.0f, followDown = 0.0f;
        std::array<float, 2> roomState {};
        double humPhase = 0.0;
        // CLOSE's shelf: each side's last two in and out (the inputs are kept while resting so it wakes without a step).
        std::array<float, 2> x1 {}, x2 {}, y1 {}, y2 {};
        bool shelfRunning = false;
        std::atomic<int> touchCount { 0 };
        std::array<std::atomic<int>, 16> seen {};
        std::atomic<float> noiseShown { -100.0f }, roomShown { -100.0f }, closeShown { 0.0f };
    };
}
