#include "Redline.h"

namespace effects
{
    namespace
    {
        constexpr float throttleSeconds = 0.03f;   // THROTTLE's smoothing
        constexpr float revsSeconds = 0.008f;      // the revs' glide (the drop at a gear change eases over ~25 ms)
        constexpr float glideSeconds = 0.03f;      // the throb's pitch gliding to a new note
        constexpr float dipSeconds = 0.06f;        // the gear change's dip
        constexpr float levelSeconds = 0.3f;       // the level kept: power followed over this
        constexpr float gainSeconds = 0.1f;
        constexpr float maxDip = 0.7f;             // at SHIFT 1: about -10 dB
        constexpr float screamRef = 0.3f;          // a 0.3 level comes out of the drive at 0.3
        constexpr float screamLevel = 1.5f;
        constexpr float whineLevel = 0.1f;
        constexpr float whineFifth = 7.0f;         // semitones the whine climbs over a gear
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
    }

    void Redline::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        const auto sr = (float) sampleRate;
        throttleCoef = 1.0f - std::exp (-1.0f / (throttleSeconds * sr));
        revsCoef = 1.0f - std::exp (-1.0f / (revsSeconds * sr));
        logHzCoef = 1.0f - std::exp (-1.0f / (glideSeconds * sr));
        dipFall = std::exp (-1.0f / (dipSeconds * sr));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * sr));
        gainCoef = 1.0f - std::exp (-1.0f / (gainSeconds * sr));
        followUp = 1.0f - std::exp (-1.0f / (0.005f * sr));
        followDown = 1.0f - std::exp (-1.0f / (0.15f * sr));
        highCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 150.0f / sr);
        for (auto& side : sides)
            side.shifter.prepare (sampleRate);
        reset();
    }

    void Redline::reset() noexcept
    {
        // The notes are kept: a reset that forgot them would wake throbbing at the wrong pitch.
        resting = true;
        throttleNow = 0.0f;
        ownBeats = 0.0;
        throbPhase = whinePhase = 0.0;
        logHz = 0.0f;
        revsSmooth = 0.0f;
        lastRevs = 0.0;
        dip = 0.0f;
        powerIn = powerOut = 0.0f;
        gain = 1.0f;
        settledSamples = 0;
        for (auto& side : sides)
        {
            side.shifter.reset();
            side.high = side.low = side.follower = 0.0f;
        }
        throttleShown.store (0.0f);
        revsShown.store (0.0f);
        screamShown.store (-120.0f);
        gainShown.store (0.0f);
    }

    void Redline::setNotes (const float* notes, int count) noexcept
    {
        if (count <= 0)
            return;   // none sounding: keep the last (the level falls with yours)
        auto lowest = notes[0];
        for (int k = 1; k < count; ++k)
            lowest = juce::jmin (lowest, notes[k]);
        lowestNote = lowest;
    }

    void Redline::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.throttle < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.throttle);
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        const auto noteHz = lowestNote >= 0.0f ? 440.0f * std::exp2 ((lowestNote - 69.0f) / 12.0f) : 55.0f;
        if (resting)
        {
            ownBeats += beatsPerSample * n;
            if (target <= 0.0f)
                return;   // untouched, bit for bit
            reset();
            resting = false;
            logHz = std::log (noteHz);
        }
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;
        if (ppq < 0.0 && start == 0.0)
            lastRevs = 0.0;

        const auto sr = (float) sampleRate;
        const auto wantLogHz = std::log (noteHz);
        const auto rise = riseSemitones (s.rise);
        const auto toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (0.45f * sr, toneHz (s.tone)) / sr);
        const auto shift = juce::jlimit (0.0f, 1.0f, s.shift);
        const auto scream = juce::jlimit (0.0f, 1.0f, s.scream);
        const auto whine = juce::jlimit (0.0f, 1.0f, s.whine);
        auto shownScream = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            throttleNow += (target - throttleNow) * throttleCoef;
            if (target <= 0.0f && throttleNow < 1.0e-5f)
                throttleNow = 0.0f;

            // The revs, and the gear change where they fall back.
            const auto revs = revsAt (start + beatsPerSample * i, s.gear);
            if (revs < lastRevs - 0.5)
                dip = 1.0f;
            lastRevs = revs;
            revsSmooth += ((float) revs - revsSmooth) * revsCoef;
            dip *= dipFall;
            const auto r = revsSmooth;
            const auto t = throttleNow;

            logHz += (wantLogHz - logHz) * logHzCoef;
            const auto hz = (double) std::exp (logHz);

            // THROB: the level pulsing at half the note's pitch, a rounded square.
            throbPhase += 0.5 * hz / sampleRate;
            throbPhase -= std::floor (throbPhase);
            const auto q = 0.5f + 0.5f * std::tanh (3.0f * (float) std::sin (twoPi * throbPhase));
            const auto am = 1.0f - throbDepth (s.throb, r) * t * (1.0f - q);

            // WHINE: two octaves over the note, rising a fifth with the revs.
            whinePhase += 4.0 * hz * std::exp2 ((double) r * whineFifth / 12.0) / sampleRate;
            whinePhase -= std::floor (whinePhase);
            const auto whineBase = whine * t * r * whineLevel * (float) std::sin (twoPi * whinePhase);

            const auto dipGain = 1.0f - maxDip * shift * t * juce::jmin (1.0f, dip);
            const auto ratio = 2.0f * std::exp2 (rise * r / 12.0f);
            const auto drive = 1.0f + 15.0f * r * t * scream;
            const auto driveNorm = screamRef / std::tanh (drive * screamRef);
            const auto screamMix = scream * t * r * screamLevel;

            float* io[] = { &left[i], right != nullptr ? &right[i] : nullptr };
            auto inPower = 0.0f, outPower = 0.0f;
            for (int c = 0; c < 2; ++c)
            {
                if (io[c] == nullptr)
                    continue;
                auto& side = sides[(size_t) c];
                const auto dry = *io[c];
                const auto level = std::abs (dry);
                side.follower += (level - side.follower) * (level > side.follower ? followUp : followDown);
                const auto whineOut = whineBase * juce::jmin (1.0f, side.follower / 0.1f);
                // SCREAM: an octave up, climbing, through the drive.
                const auto shifted = side.shifter.processSample (dry, ratio);
                side.high += (shifted - side.high) * highCoef;
                const auto driven = std::tanh (drive * (shifted - side.high)) * driveNorm;
                side.low += (driven - side.low) * toneCoef;
                const auto screamOut = side.low * screamMix;
                shownScream = juce::jmax (shownScream, std::abs (screamOut));
                const auto out = (dry * am + screamOut + whineOut) * dipGain;
                inPower += dry * dry;
                outPower += out * out;
                *io[c] = out * gain;
            }

            // The level kept.
            powerIn += (inPower - powerIn) * levelCoef;
            powerOut += (outPower - powerOut) * levelCoef;
            if (t > 0.0f && powerIn > 1.0e-9f && powerOut > 1.0e-12f)
                gain += (juce::jlimit (0.5f, 1.6f, std::sqrt (powerIn / powerOut)) - gain) * gainCoef;
            else if (t <= 0.0f)
                gain += (1.0f - gain) * gainCoef;
        }

        const auto endBeats = start + beatsPerSample * n;
        throttleShown.store (throttleNow, std::memory_order_relaxed);
        revsShown.store ((float) revsAt (endBeats, s.gear), std::memory_order_relaxed);
        gearShown.store (1 + gearCount (endBeats, s.gear) % 6, std::memory_order_relaxed);
        noteShown.store ((float) std::exp (logHz), std::memory_order_relaxed);
        screamShown.store (juce::Decibels::gainToDecibels (shownScream, -120.0f), std::memory_order_relaxed);
        gainShown.store (juce::Decibels::gainToDecibels (gain), std::memory_order_relaxed);

        const auto settled = target <= 0.0f && throttleNow <= 0.0f && std::abs (gain - 1.0f) < 1.0e-3f;   // the scream and whine are x THROTTLE: at 0 they're silent
        settledSamples = settled ? settledSamples + n : 0;
        if (settled && settledSamples > (int) (0.3 * sampleRate))
        {
            const auto beats = ownBeats;
            reset();
            ownBeats = beats;
        }
    }
}
