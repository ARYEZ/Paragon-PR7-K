#include "Leviathan.h"

#include <cmath>

namespace effects
{
    void Leviathan::prepare (double rate, int)
    {
        sampleRate = rate;
        powerUp = (float) (1.0 - std::exp (-1.0 / (0.04 * rate)));
        powerDown = (float) (1.0 - std::exp (-1.0 / (0.25 * rate)));
        powerSmooth = (float) (1.0 - std::exp (-1.0 / (0.05 * rate)));
        voicePowerRate = (float) (1.0 - std::exp (-1.0 / (0.15 * rate)));
        glide = (float) (1.0 - std::exp (-1.0 / (0.12 * rate)));
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));
        const auto slots = (size_t) std::ceil (maxDelay * rate / controlStep) + 2;
        delayedNote.assign (slots, -1.0f);
        delayedPower.assign (slots, 0.0f);
        reset();
    }

    void Leviathan::reset() noexcept
    {
        for (auto& t : throats)
        {
            t = {};
            t.formants.reset();
        }
        std::fill (delayedNote.begin(), delayedNote.end(), -1.0f);
        std::fill (delayedPower.begin(), delayedPower.end(), 0.0f);
        delayWrite = controlCount = 0;
        heldNote = -1.0f;
        heldPower = 0.0f;
        pitchNow = -1.0f;
        inPower = smoothPower = voicePower = levelNow = mixNow = 0.0f;
        vowelNow = vowelTarget = vowelShownValue = 0.3f;
        vowelTimer = 0.0f;
        countdown = 0;
        sungShown.store (-1.0f, std::memory_order_relaxed);
        levelShown.store (-100.0f, std::memory_order_relaxed);
    }

    void Leviathan::setNotes (const float* notes, int count) noexcept
    {
        if (count <= 0)
            return;   // none sounding: keep the last (its level falls with yours)
        auto lowest = notes[0];
        for (int k = 1; k < count; ++k)
            lowest = juce::jmin (lowest, notes[k]);
        lowestNote = lowest;
    }

    // The two-sample polyBLEP: the saw's jump smoothed so it doesn't fold back.
    float Leviathan::polyBlep (double t, double dt) noexcept
    {
        if (t < dt)
        {
            const auto x = t / dt;
            return (float) (x + x - x * x - 1.0);
        }
        if (t > 1.0 - dt)
        {
            const auto x = (t - 1.0) / dt;
            return (float) (x * x + x + x + 1.0);
        }
        return 0.0f;
    }

    // The bass vowels with every formant scaled by `scale` (a bigger throat), each side a step apart.
    void Leviathan::setVowel (float vowel, float scale) noexcept
    {
        for (int side = 0; side < 2; ++side)
        {
            const auto v = juce::jlimit (0.0f, 1.0f, vowel + (side == 0 ? -0.04f : 0.04f));
            const auto formants = synth::vocal::formantsFor (v, 0.0f);
            auto& c = vowels[(size_t) side];
            for (size_t f = 0; f < (size_t) synth::vocal::numFormants; ++f)
            {
                const auto hz = juce::jlimit (20.0, 0.45 * sampleRate, (double) (formants[f].hz * scale));
                const auto q = hz / juce::jmax (10.0, (double) (formants[f].bandwidthHz * scale));
                const auto g = std::tan (juce::MathConstants<double>::pi * hz / sampleRate);
                const auto k = 1.0 / q;
                const auto a1 = 1.0 / (1.0 + g * (g + k));
                c.a1[f] = (float) a1;
                c.a2[f] = (float) (g * a1);
                c.a3[f] = (float) (g * g * a1);
                c.k[f] = (float) k;
                c.gain[f] = juce::Decibels::decibelsToGain (formants[f].db);
                c.hz[f] = (float) hz;
                c.q[f] = (float) q;
            }
        }
    }

    void Leviathan::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        // MIX under a thousandth counts as 0 (the mod matrix eases a knob down and never lands exactly on 0).
        const auto mixTarget = s.on && s.mix >= 1.0e-3f ? juce::jmin (1.0f, s.mix) : 0.0f;
        if (mixTarget <= 0.0f && isResting())
            return;   // untouched; nothing kept, so it wakes clean

        const auto octaves = (float) (juce::jlimit (0, 1, s.depth) + 1);
        const auto growl = juce::jlimit (0.0f, 1.0f, s.growl);
        const auto scale = sizeScale (s.size);
        const auto driftTime = driftSeconds (s.drift);
        const auto lag = (int) std::round (delaySeconds (s.delay) * sampleRate / controlStep);
        const auto slots = (int) delayedNote.size();
        const auto clickRate = 2.0f + 10.0f * growl;   // a second
        float ghostPower = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            mixNow += (mixTarget - mixNow) * smoothing;
            if (mixTarget <= 0.0f && mixNow < 1.0e-5f)
                mixNow = 0.0f;

            const float in[2] = { left[i], right[i] };
            // Your level: the power smoothed evenly first (a fast rise on the raw power rode a tone's ripple and
            // read 2 dB high), then up quickly and down slowly; under -80 dB it's silence.
            const auto power = 0.5f * (in[0] * in[0] + in[1] * in[1]);
            smoothPower += (power - smoothPower) * powerSmooth;
            inPower += (smoothPower - inPower) * (smoothPower > inPower ? powerUp : powerDown);
            if (inPower < 1.0e-8f)
                inPower = 0.0f;

            // Every control step: the note and the level into DELAY's line, the delayed ones out, the vowel.
            if (--countdown <= 0)
            {
                countdown = controlStep;
                delayedNote[(size_t) delayWrite] = lowestNote;
                delayedPower[(size_t) delayWrite] = inPower;
                const auto read = (delayWrite - lag + 2 * slots) % slots;
                heldNote = delayedNote[(size_t) read];
                heldPower = delayedPower[(size_t) read];
                delayWrite = (delayWrite + 1) % slots;

                vowelTimer -= (float) controlStep / (float) sampleRate;
                if (vowelTimer <= 0.0f)
                {
                    vowelTarget = random01();
                    vowelTimer = driftTime * (0.7f + 0.6f * random01());
                }
                vowelNow += (vowelTarget - vowelNow) * juce::jmin (1.0f, (float) controlStep / (0.5f * driftTime * (float) sampleRate));
                setVowel (vowelNow, scale);
            }

            if (heldNote < 0.0f)
            {
                left[i] = in[0];
                if (stereo)
                    right[i] = in[1];
                continue;
            }
            const auto target = heldNote - 12.0f * octaves;
            pitchNow = pitchNow < 0.0f ? target : pitchNow + (target - pitchNow) * glide;
            waverPhase += (float) (juce::MathConstants<double>::twoPi * 0.23 / sampleRate);
            if (waverPhase > juce::MathConstants<float>::twoPi)
                waverPhase -= juce::MathConstants<float>::twoPi;
            const auto waver = 0.12f * std::sin (waverPhase);   // semitones

            // Throat clicks: short noise bursts at random, more with GROWL.
            const auto clickNow = random01() < growl * clickRate / (float) sampleRate;

            float voice[2] = { 0.0f, 0.0f };
            for (int side = 0; side < 2; ++side)
            {
                auto& t = throats[(size_t) side];
                const auto detune = side == 0 ? -0.04f : 0.04f;   // a few cents apart
                const auto hz = 440.0 * std::pow (2.0, (pitchNow + waver + detune - 69.0) / 12.0);
                const auto dt = juce::jmin (0.45, hz * (double) t.cycleStretch / sampleRate);
                t.phase += dt;
                if (t.phase >= 1.0)
                {
                    t.phase -= 1.0;
                    // A new cycle: GROWL's jitter in its length and level, every other one weaker (creak).
                    t.odd = ! t.odd;
                    t.cycleStretch = 1.0f + growl * 0.06f * (random01() - 0.5f);
                    t.cycleGain = (1.0f - growl * 0.5f * random01()) * (t.odd ? 1.0f - 0.6f * growl : 1.0f);
                }
                auto saw = (float) (2.0 * t.phase - 1.0) - polyBlep (t.phase, dt);
                t.soft += 0.35f * (saw * t.cycleGain - t.soft);   // softened: a throat, not a saw
                if (clickNow)
                    t.clickLeft = 0.003f * (float) sampleRate;
                auto burst = 0.0f;
                if (t.clickLeft > 0.0f)
                {
                    t.clickLeft -= 1.0f;
                    burst = 1.5f * growl * (2.0f * random01() - 1.0f);
                }
                voice[side] = t.formants.process (t.soft + burst, vowels[(size_t) side]);
            }

            // Kept at your level: the voice's own power measured and matched, x MIX.
            const auto raw = 0.5f * (voice[0] * voice[0] + voice[1] * voice[1]);
            voicePower += (raw - voicePower) * voicePowerRate;
            const auto wanted = heldPower > 0.0f && voicePower > 1.0e-9f ? std::sqrt (heldPower / voicePower) : 0.0f;
            levelNow += (juce::jmin (wanted, 30.0f) - levelNow) * smoothing;
            if (heldPower <= 0.0f && levelNow < 1.0e-6f)
                levelNow = 0.0f;
            const auto gain = levelNow * mixNow;
            left[i] = in[0] + gain * voice[0];
            if (stereo)
                right[i] = in[1] + gain * voice[1];
            ghostPower += gain * gain * raw;
        }

        sungShown.store (heldNote < 0.0f || levelNow <= 0.0f ? -1.0f : pitchNow, std::memory_order_relaxed);
        vowelShown.store (vowelNow, std::memory_order_relaxed);
        levelShown.store ((float) (10.0 * std::log10 (ghostPower / (float) juce::jmax (1, n) + 1.0e-12)), std::memory_order_relaxed);

        if (mixNow <= 0.0f)
        {
            // Asleep again: let go of everything so it wakes clean (the note stays known).
            const auto note = lowestNote;
            reset();
            lowestNote = note;
        }
    }
}
