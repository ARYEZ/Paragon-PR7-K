// THE WAH (0.51.0): what it does to the sound. Where its peak sits for each pedal position and range, how RESONANCE
// sharpens it, the low end BODY keeps, AUTO opening with level, the LFO's rate and sync, the sides kept apart, MIX,
// off changing nothing, and its loudness against the dry sound.
#include "SynthTestUtils.h"

#include "dsp/effects/Wah.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    using Wah = effects::Wah;

    // A sine through a wah (both sides), its level after it settles, in dB against what went in.
    double gainAt (Wah::Settings s, double hz, float amplitude = 0.25f)
    {
        Wah wah;
        wah.prepare (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> out;
        const auto total = (int) (0.4 * sampleRate);
        for (int start = 0; start < total; start += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const auto x = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (start + i) / sampleRate);
                buffer.setSample (0, i, x);
                buffer.setSample (1, i, x);
            }
            juce::dsp::AudioBlock<float> block (buffer);
            wah.process (block, s, 120.0, -1.0);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        const auto from = (size_t) (0.2 * sampleRate);
        return db (rms (out, from, out.size()) / (amplitude / std::sqrt (2.0)));
    }

    // The frequency (on a 1/24-octave grid) where the response peaks.
    double peakHz (const Wah::Settings& s)
    {
        auto best = -1000.0, bestHz = 0.0;
        for (auto hz = 60.0; hz < 8000.0; hz *= std::pow (2.0, 1.0 / 24.0))
            if (const auto g = gainAt (s, hz); g > best)
            {
                best = g;
                bestHz = hz;
            }
        return bestHz;
    }

    Wah::Settings pedal (float position, Wah::Range range = Wah::Range::classic)
    {
        Wah::Settings s;
        s.on = true;
        s.mode = Wah::Mode::pedal;
        s.range = range;
        s.position = position;
        return s;
    }

    // A bright saw (naive is fine here: it's measured broadband) through a wah, the left side out.
    std::vector<float> sawThrough (Wah::Settings s, double hz, double seconds, float amplitude = 0.5f, double ppqStart = -1.0)
    {
        Wah wah;
        wah.prepare (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> out;
        auto phase = 0.0;
        const auto total = (int) (seconds * sampleRate);
        for (int start = 0; start < total; start += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const auto x = amplitude * (float) (2.0 * phase - 1.0);
                phase += hz / sampleRate;
                phase -= std::floor (phase);
                buffer.setSample (0, i, x);
                buffer.setSample (1, i, x);
            }
            juce::dsp::AudioBlock<float> block (buffer);
            const auto ppq = ppqStart < 0.0 ? -1.0 : ppqStart + start / sampleRate * 2.0;   // 120 BPM
            wah.process (block, s, 120.0, ppq);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        return out;
    }
}

TEST_CASE ("WAH: the peak follows the pedal, heel to toe, in each range", "[wah]")
{
    for (const auto range : { Wah::Range::classic, Wah::Range::wide, Wah::Range::low })
        for (const auto position : { 0.0f, 0.5f, 1.0f })
        {
            const auto expected = Wah::frequencyFor (range, position);
            const auto found = peakHz (pedal (position, range));
            INFO ("range " << (int) range << " position " << position << ": expected " << expected << " Hz, found " << found);
            CHECK (std::abs (std::log2 (found / expected)) < 0.1);   // within about a semitone
        }
    CHECK (juce::approximatelyEqual (Wah::frequencyFor (Wah::Range::classic, 0.0f), 350.0f));
    CHECK (juce::approximatelyEqual (Wah::frequencyFor (Wah::Range::classic, 1.0f), 2200.0f));
}

TEST_CASE ("WAH: the peak stands out, sharper with RESONANCE; BODY keeps the low end", "[wah]")
{
    auto soft = pedal (0.5f), sharp = pedal (0.5f);
    soft.resonance = 0.0f;
    sharp.resonance = 10.0f;
    const auto centre = Wah::frequencyFor (Wah::Range::classic, 0.5f);
    // An octave above the peak, the sharp one has fallen further below its own peak.
    const auto softDrop = gainAt (soft, centre) - gainAt (soft, centre * 2.0);
    const auto sharpDrop = gainAt (sharp, centre) - gainAt (sharp, centre * 2.0);
    INFO ("an octave above the peak: soft " << softDrop << " dB down, sharp " << sharpDrop);
    CHECK (sharpDrop > softDrop + 6.0);
    CHECK (gainAt (sharp, centre) > 10.0);   // the peak is a boost

    // Far below the sweep, a pure band-pass would be gone; the wah keeps some body (the low-pass share).
    const auto low = gainAt (pedal (1.0f), 60.0);
    INFO ("60 Hz with the pedal at the toe: " << low << " dB");
    CHECK (low > -10.0);
    CHECK (low < -4.0);
    // The LOW range keeps more.
    CHECK (gainAt (pedal (1.0f, Wah::Range::low), 40.0) > low);
}

// A saw's energy is mostly its fundamental, so its plain level (RMS) falls toward the toe, where the band is far above
// it; the ear, more sensitive up there, hears the toe as the brighter, louder end. What's checked: switching the wah on
// doesn't drop the sound away.
TEST_CASE ("WAH: a low saw comes out about as loud as it went in", "[wah]")
{
    const auto dry = rms (sawThrough ({}, 110.0, 0.5), (size_t) (0.25 * sampleRate), (size_t) (0.5 * sampleRate));
    auto level = [&] (float position)
    {
        const auto out = sawThrough (pedal (position), 110.0, 0.5);
        return db (rms (out, (size_t) (0.25 * sampleRate), out.size()) / dry);
    };
    const auto heel = level (0.0f), middle = level (0.5f), toe = level (1.0f);
    INFO ("heel " << heel << " dB, middle " << middle << " dB, toe " << toe << " dB");
    CHECK (std::abs (middle) < 3.0);
    CHECK (std::abs (heel) < 4.0);
    CHECK (toe > -6.0);
}

TEST_CASE ("WAH: AUTO opens with how loud you play, then falls back", "[wah]")
{
    Wah::Settings s;
    s.on = true;
    s.mode = Wah::Mode::autoWah;
    s.position = 0.0f;
    s.depth = 1.0f;
    s.sensitivity = 5.0f;

    Wah wah;
    wah.prepare (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto run = [&] (float amplitude, double seconds)
    {
        for (int start = 0; start < (int) (seconds * sampleRate); start += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
                for (int c = 0; c < 2; ++c)
                    buffer.setSample (c, i, amplitude * (float) std::sin (0.05 * (start + i)));
            juce::dsp::AudioBlock<float> block (buffer);
            wah.process (block, s, 120.0, -1.0);
        }
        return wah.getSweep();
    };
    const auto quiet = run (0.02f, 0.3);
    const auto loud = run (0.5f, 0.3);
    const auto after = run (0.0f, 1.0);
    INFO ("quiet " << quiet << ", loud " << loud << ", a second after " << after);
    CHECK (quiet < 0.45f);   // a quiet note opens it less than halfway
    CHECK (loud > 0.8f);
    CHECK (after < 0.1f);

    // DEPTH limits how far it opens.
    s.depth = 0.4f;
    wah.reset();
    CHECK (run (0.5f, 0.3) < 0.42f);
}

TEST_CASE ("WAH: the LFO sweeps at its rate, or in time with the song", "[wah]")
{
    auto s = pedal (0.0f);
    s.mode = Wah::Mode::lfo;
    s.depth = 1.0f;
    s.rateHz = 2.0f;
    Wah wah;
    wah.prepare (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    std::vector<float> sweeps;
    const auto blocks = (int) (2.0 * sampleRate / blockSize);
    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        juce::dsp::AudioBlock<float> block (buffer);
        wah.process (block, s, 120.0, -1.0);
        sweeps.push_back (wah.getSweep());
    }
    // Count the times it rises through the middle: 2 Hz over 2 s is 4.
    int crossings = 0;
    for (size_t i = 1; i < sweeps.size(); ++i)
        crossings += sweeps[i - 1] < 0.5f && sweeps[i] >= 0.5f ? 1 : 0;
    CHECK (crossings == 4);
    CHECK (*std::max_element (sweeps.begin(), sweeps.end()) > 0.95f);
    CHECK (*std::min_element (sweeps.begin(), sweeps.end()) < 0.05f);

    // Synced to a 1/4 at 120 BPM while the song plays: at the song's beat 1.5 it's halfway, at the toe.
    s.sync = 3;   // 1/4
    wah.reset();
    buffer.clear();
    for (int b = 0; b < 8; ++b)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        wah.process (block, s, 120.0, 1.5);   // the song held at beat 1.5 (each block starts there)
    }
    CHECK (wah.getSweep() > 0.95f);
}

TEST_CASE ("WAH: each side its own; MIX; off changes nothing", "[wah]")
{
    Wah wah;
    wah.prepare (sampleRate, blockSize);
    auto s = pedal (0.5f);
    juce::AudioBuffer<float> buffer (2, blockSize);
    auto rightPeak = 0.0f;
    for (int b = 0; b < 40; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            buffer.setSample (0, i, 0.5f * (float) std::sin (0.07 * (b * blockSize + i)));
            buffer.setSample (1, i, 0.0f);
        }
        juce::dsp::AudioBlock<float> block (buffer);
        wah.process (block, s, 120.0, -1.0);
        rightPeak = juce::jmax (rightPeak, buffer.getMagnitude (1, 0, blockSize));
    }
    CHECK (rightPeak == 0.0f);   // a silent side stays silent

    // MIX 0 is the dry sound; off (after its fade) is exactly the input.
    auto dryMix = pedal (0.5f);
    dryMix.mix = 0.0f;
    CHECK (std::abs (gainAt (dryMix, 300.0)) < 0.05);
    const auto in = sawThrough ({}, 110.0, 0.2);
    std::vector<float> reference;
    {
        auto phase = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
        {
            reference.push_back (0.5f * (float) (2.0 * phase - 1.0));
            phase += 110.0 / sampleRate;
            phase -= std::floor (phase);
        }
    }
    CHECK (in == reference);
}
