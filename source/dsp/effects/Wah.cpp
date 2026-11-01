#include "Wah.h"

#include "Delay.h"

#include <complex>

namespace effects
{
    namespace
    {
        constexpr int retuneEvery = 16;   // samples between filter updates (the sweep is smoothed, so this doesn't step)
        // peakBoost (Wah.h): a wah's peak stands well up out of the sound (PR7-G's +3.5 dB peak at a fixed Q read a
        // little polite on a synth). makeUp: the whole wet sound up 6 dB, since a saw's energy is mostly in its
        // fundamental, which the swept band leaves out; without it switching the wah on dropped a low note by about
        // 7.5 dB (at the pedal's middle). Together: the peak about +13 dB, BODY's low end about -7 dB at the toe.
        constexpr float sweepSeconds = 0.015f;   // smooths a stepped controller (a MIDI pedal) into a continuous sweep
        constexpr float autoAttackSeconds = 0.004f;

        float coefficient (double sampleRate, float seconds, int samples = 1) noexcept
        {
            return 1.0f - std::exp (-(float) samples / (seconds * (float) sampleRate));
        }
    }

    Wah::Voicing Wah::voicingFor (Range range) noexcept
    {
        switch (range)
        {
            case Range::wide: return { 220.0f, 3500.0f, 0.15f };   // a synth's wider sweep, up into the bite
            case Range::low:  return { 90.0f, 900.0f, 0.35f };     // a bass wah: lower, with more of the bottom kept
            case Range::classic:
            default:          return { 350.0f, 2200.0f, 0.22f };   // the classic pedal's range (PR7-G's)
        }
    }

    float Wah::frequencyFor (Range range, float position) noexcept
    {
        const auto v = voicingFor (range);
        return v.heelHz * std::pow (v.toeHz / v.heelHz, juce::jlimit (0.0f, 1.0f, position));
    }

    float Wah::responseDb (Range range, float sweepPosition, float resonance, float hz) noexcept
    {
        // The state-variable filter's analog prototype at s = j w (w = hz over the centre): band-pass s / D, low-pass
        // 1 / D, D = s^2 + k s + 1.
        const auto voicing = voicingFor (range);
        const auto w = (double) hz / (double) frequencyFor (range, sweepPosition);
        const auto k = 1.0 / (double) qFor (resonance);
        const std::complex<double> s (0.0, w);
        const auto d = s * s + k * s + 1.0;
        const auto wet = (double) makeUp * ((double) peakBoost * k * s / d + (double) voicing.body / d);
        return (float) juce::Decibels::gainToDecibels (std::abs (wet), -120.0);
    }

    void Wah::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        reset();
    }

    void Wah::reset() noexcept
    {
        sides = {};
        sweep = -1.0f;
        envelope = 0.0f;
        lfoPhase = 0.0;
    }

    void Wah::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        if (! s.on && bypass.isSilent())
        {
            // Off: start fresh next time (no old ringing, AUTO not still open).
            if (sweep >= 0.0f)
                reset();
            return;
        }

        bypass.process (block, s.on, [&] (juce::dsp::AudioBlock<float> wetBlock)
        {
            const auto numSamples = (int) wetBlock.getNumSamples();
            const auto numChannels = juce::jmin (2, (int) wetBlock.getNumChannels());
            const auto voicing = voicingFor (s.range);
            const auto k = 1.0f / qFor (s.resonance);
            const auto position = juce::jlimit (0.0f, 1.0f, s.position);
            const auto depth = juce::jlimit (0.0f, 1.0f, s.depth);
            const auto mix = juce::jlimit (0.0f, 1.0f, s.mix);
            const auto level = juce::Decibels::decibelsToGain (s.levelDb);

            const auto attack = coefficient (sampleRate, autoAttackSeconds);
            const auto release = coefficient (sampleRate, 0.6f * std::pow (0.04f / 0.6f, juce::jlimit (0.0f, 10.0f, s.speed) / 10.0f));
            const auto sensitivityGain = std::pow (10.0f, (juce::jlimit (0.0f, 10.0f, s.sensitivity) - 2.0f) * 0.3f);

            // The LFO: in time with the song while it plays (from its position), else free at RATE or the tempo.
            const auto syncIndex = juce::jlimit (0, (int) std::size (Delay::syncBeats) - 1, s.sync);
            const auto beats = (double) Delay::syncBeats[(size_t) syncIndex];
            const auto cyclesPerSample = syncIndex > 0 ? juce::jmax (1.0, bpm) / 60.0 / beats / sampleRate
                                                       : (double) juce::jlimit (0.01f, 20.0f, s.rateHz) / sampleRate;
            if (s.mode == Mode::lfo && syncIndex > 0 && ppq >= 0.0)
                lfoPhase = ppq / beats - std::floor (ppq / beats);

            for (int start = 0; start < numSamples; start += retuneEvery)
            {
                const auto count = juce::jmin (retuneEvery, numSamples - start);

                // Where the sweep wants to be for these samples.
                auto target = position;
                if (s.mode == Mode::autoWah)
                {
                    for (int i = start; i < start + count; ++i)
                    {
                        auto x = std::abs (wetBlock.getSample (0, i));
                        if (numChannels > 1)
                            x = juce::jmax (x, std::abs (wetBlock.getSample (1, i)));   // both sides open together
                        envelope += (x - envelope) * (x > envelope ? attack : release);
                    }
                    // A square-root curve, so quieter playing still moves it (as PR7-G's auto-wah).
                    target = position + depth * std::sqrt (juce::jlimit (0.0f, 1.0f, envelope * sensitivityGain));
                }
                else if (s.mode == Mode::lfo)
                {
                    const auto wave = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) lfoPhase);   // heel first
                    target = position + depth * wave;
                    lfoPhase += cyclesPerSample * count;
                    lfoPhase -= std::floor (lfoPhase);
                }
                target = juce::jlimit (0.0f, 1.0f, target);
                if (sweep < 0.0f)
                    sweep = target;
                else
                    sweep += (target - sweep) * coefficient (sampleRate, sweepSeconds, count);

                const auto hz = juce::jmin (voicing.heelHz * std::pow (voicing.toeHz / voicing.heelHz, sweep), 0.45f * (float) sampleRate);
                const auto g = std::tan (juce::MathConstants<float>::pi * hz / (float) sampleRate);
                const auto a1 = 1.0f / (1.0f + g * (g + k));
                const auto a2 = g * a1, a3 = g * a2;

                for (int c = 0; c < numChannels; ++c)
                {
                    auto& side = sides[(size_t) c];
                    auto* x = wetBlock.getChannelPointer ((size_t) c);
                    for (int i = start; i < start + count; ++i)
                    {
                        // A zero-delay state-variable filter (Simper's): v1 the band-pass (its peak is Q, so k x v1
                        // is unity at the centre), v2 the low-pass.
                        const auto v3 = x[i] - side.s2;
                        const auto v1 = a1 * side.s1 + a2 * v3;
                        const auto v2 = side.s2 + a2 * side.s1 + a3 * v3;
                        side.s1 = 2.0f * v1 - side.s1;
                        side.s2 = 2.0f * v2 - side.s2;
                        const auto wet = makeUp * (peakBoost * k * v1 + voicing.body * v2);
                        x[i] = (x[i] + (wet - x[i]) * mix) * level;
                    }
                }
            }
            for (auto& side : sides)
                for (auto* state : { &side.s1, &side.s2 })
                    if (std::abs (*state) < 1.0e-15f)
                        *state = 0.0f;   // no denormals while it rings down
            shownSweep.store (sweep, std::memory_order_relaxed);
        });
    }
}
