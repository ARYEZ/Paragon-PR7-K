#pragma once

#include "EffectUtils.h"

#include <array>
#include <atomic>

namespace effects
{
    // DOWNPOUR (0.57.0, the NEON RAIN expansion's signature, the owner's ask and name): rain on the glass, added to the
    // sound. Our own design (not a recording, nor a model of one). Two layers, both falling at random moments (each
    // sample the same small chance, as CRACKLE's clicks: no rhythm):
    //
    //  - PATTER: thousands of tiny droplets, each a single tap through a band-pass round 4.5 kHz, so together they
    //            make the hiss of rain on a window, grainy rather than smooth. RAIN sets how many: none at 0 (no rain:
    //            nothing at all), 40 a second at 0.1 (a few spots), 1000 at 0.5, 4000 at the top (a downpour).
    //  - DROPS:  now and then a big drop rings out: a sine starting two semitones low and rising onto its note in a few
    //            milliseconds (a falling drop's bubble rises in pitch: what makes it sound like water rather than a
    //            bell), dying away over 80-300 ms, with a soft tap as it lands. Each drop's note is one of the notes
    //            sounding now (keys, arps, releasing notes), OCTAVE octaves up (now and then one more), so the rain
    //            plays your chord. DROPS sets how many: up to 12 a second at full RAIN (none
    //            without rain).
    //  - DISTANCE: on the glass (bright, the taps sharp) to out in the street (dull: a low-pass from 16 kHz down to
    //            1.6 kHz, the taps gone, a little quieter).
    //  - SPACE:  how much of the rain goes into the REVERB's send (the preset's own reverb), so far rain sits in the room.
    //  - WIDTH:  how far each droplet and drop is placed across left and right (each its own place).
    //  - FOLLOW: as CRACKLE's: at 1 the rain falls only while the sound plays (and as hard as it's loud), so a preset
    //            still falls silent; at 0 it never stops.
    //  - LEVEL:  how loud the rain is. The sound passes under it unchanged.
    //
    // It works in two steps so the processor can put it where it wants: `render` listens to the sound (for FOLLOW) and
    // makes this block's rain into its own buffer; `addTo` adds that rain to a block (and `getOutput` reads it for the
    // reverb's send).
    class Downpour
    {
    public:
        static inline const juce::StringArray octaveNames { "+1 OCTAVE", "+2 OCTAVES", "+3 OCTAVES" };   // never reorder

        struct Settings
        {
            bool on = false;
            float rain = 0.5f;       // 0 to 1: how hard it rains (the patter's density, and the drops with it)
            float drops = 0.4f;      // 0 to 1: how many tuned drops
            int octave = 1;          // octaveNames: the drops 1, 2 or 3 octaves over the notes
            float distance = 0.3f;   // 0 on the glass to 1 out in the street
            float space = 0.4f;      // 0 to 1: into the reverb's send
            float width = 0.8f;      // 0 to 1
            float follow = 1.0f;     // 0 to 1
            float levelDb = -18.0f;  // -48 to +6
        };

        static constexpr int maxNotes = 16;
        static constexpr int maxDrops = 12;

        // The patter's droplets a second and the drops a second, for given settings (FOLLOW at full).
        static float patterRate (float rain) noexcept;
        static float dropRate (float rain, float drops) noexcept;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { state = seed == 0 ? 0x9e3779b9u : seed; }

        // The notes sounding now (MIDI note numbers, fractions allowed), the drops' choice. With none, the last ones
        // given stay (a releasing chord's rain keeps its notes).
        void setNotes (const float* notes, int count) noexcept;

        // This block's rain (up to the prepared block size), listening to `in` for FOLLOW. Then `addTo` / `getOutput`.
        void render (const juce::dsp::AudioBlock<const float>& in, const Settings&) noexcept;
        void addTo (juce::dsp::AudioBlock<float> block) const noexcept;
        const float* getOutput (int channel) const noexcept { return output.getReadPointer (channel); }
        int getRenderedSamples() const noexcept { return rendered; }
        bool isSilent() const noexcept { return silent; }

        // For the page and the unit.
        struct DropMark { float note = 0.0f, pan = 0.0f, strength = 0.0f; long long sample = -1; };
        static constexpr int numMarks = 32;
        std::array<DropMark, numMarks> getRecentDrops() const noexcept;   // newest last; sample -1 where none yet
        long long getSampleClock() const noexcept { return clock.load (std::memory_order_relaxed); }
        double getSampleRate() const noexcept { return sampleRate; }
        float getCurrentPatterRate() const noexcept { return currentPatter.load (std::memory_order_relaxed); }
        float getCurrentDropRate() const noexcept { return currentDrops.load (std::memory_order_relaxed); }
        long long getTotalDrops() const noexcept { return totalDrops.load (std::memory_order_relaxed); }
        long long getTotalPatter() const noexcept { return totalPatter.load (std::memory_order_relaxed); }
        // The note of each drop started since the last call to `takeDropNotes` (for tests: up to 256 kept).
        int takeDropNotes (float* out, int capacity) noexcept;

    private:
        float random01() noexcept   // xorshift32, 0 to 1
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (state >> 8) * (1.0f / 16777216.0f);
        }
        void startDrop (const Settings&) noexcept;
        void clearMarks() noexcept;

        double sampleRate = 48000.0;
        std::uint32_t state = 0x9e3779b9u;
        juce::AudioBuffer<float> output;
        int rendered = 0;
        bool silent = true;

        std::array<float, maxNotes> notes {};
        int numNotes = 0;

        struct Drop
        {
            bool active = false;
            float phase = 0.0f, freq = 0.0f, target = 0.0f;   // radians per sample
            float amp = 0.0f, decay = 1.0f, attack = 0.0f, attackStep = 0.0f;
            float tap = 0.0f;                                  // the landing's noise, falling fast
            float gainL = 0.0f, gainR = 0.0f;
        };
        std::array<Drop, maxDrops> drops {};
        float glide = 0.0f, tapDecay = 0.0f;   // per sample

        // The patter: taps into a band-pass per side; then the DISTANCE low-pass per side.
        std::array<juce::dsp::StateVariableTPTFilter<float>, 2> patterTone;
        std::array<float, 4> distanceState {};   // two poles a side
        float follower = 0.0f, followAttack = 0.0f, followRelease = 0.0f;
        juce::SmoothedValue<float> onGain;   // 20 ms in and out
        int quietSamples = 0;

        // Display and tests.
        std::array<std::atomic<float>, numMarks> markNote {}, markPan {}, markStrength {};
        std::array<std::atomic<long long>, numMarks> markSample {};
        std::atomic<int> markIndex { 0 };
        std::atomic<long long> clock { 0 }, totalDrops { 0 }, totalPatter { 0 };
        std::atomic<float> currentPatter { 0.0f }, currentDrops { 0.0f };
        std::array<float, 256> dropNotes {};
        int numDropNotes = 0;
    };
}
