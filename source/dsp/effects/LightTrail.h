#pragma once

#include "PitchShifter.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // LIGHTTRAIL (0.61.0, DATASTREAM's signature, the owner's name): every note leaves a glowing trail behind it, our
    // own design. A delay in time with the song whose repeats each step in pitch (RISE semitones a repeat: up the trail
    // climbs, down it falls away), move across the stereo field one after another (SPREAD) and fade (LENGTH), each pass
    // a little darker or keeping its shine (TONE). The trail is added to the sound, which passes unchanged; at MIX 0
    // (or off, once settled) nothing at all is added.
    //
    // Into the trail go the sound and the repeat fed back (x LENGTH's feedback), together through the pitch step (PR7-G's
    // PitchShifter, only while RISE isn't 0), the tone, a soft limit (so a rising trail can't run away) and SPREAD's
    // crossing to the other side; one step of DIVISION later they come back as the next repeat. So every repeat, the
    // first too, is one step further along.
    class LightTrail
    {
    public:
        static inline const juce::StringArray divisionNames { "1/32", "1/16 T", "1/16", "1/8 T", "1/8", "1/4" };   // never reorder
        static constexpr std::array<double, 6> divisionBeats { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 1.0 };

        struct Settings
        {
            bool on = false;
            int division = 2;         // divisionNames: 1/16
            float length = 0.5f;      // 0 to 1: the feedback, 0 to 0.9
            float riseSemitones = 0.0f;   // -12 to +12, each repeat
            float tone = 0.6f;        // 0 dark (each pass 1.5 kHz) to 1 bright (16 kHz)
            float spread = 0.7f;      // 0 to 1: how far each repeat crosses to the other side
            float mix = 0.0f;         // 0 to 1
        };

        static float feedbackFor (float length) noexcept { return 0.9f * juce::jlimit (0.0f, 1.0f, length); }
        static float toneHz (float tone) noexcept { return 1500.0f * std::pow (16000.0f / 1500.0f, juce::jlimit (0.0f, 1.0f, tone)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm) noexcept;
        bool isResting() const noexcept { return mixNow <= 0.0f && trailPower < 1.0e-12f; }

        // For the page: the trail's level now (dB, -100 when silent).
        float getTrailDb() const noexcept { return trailShown.load (std::memory_order_relaxed); }

    private:
        double sampleRate = 48000.0;
        std::array<std::vector<float>, 2> lines;
        int writeIndex = 0;
        std::array<PitchShifter, 2> shifters;
        std::array<float, 2> toneState {};
        float mixNow = 0.0f, smoothing = 0.0f, delayNow = -1.0f;
        float trailPower = 0.0f;
        std::atomic<float> trailShown { -100.0f };
    };
}
