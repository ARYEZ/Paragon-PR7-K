#include "Downpour.h"

namespace effects
{
    namespace
    {
        constexpr float followRef = 0.18f;        // as CRACKLE's: about -15 dBFS lets the full rain through
        constexpr float patterMakeUp = 5.0f;      // a tap's strength into the band-pass (measured: see DownpourTests)
        constexpr float patterHz = 4500.0f;
        constexpr float dropRise = 2.0f / 12.0f;  // a drop starts two semitones under its note...
        constexpr double riseSeconds = 0.005;     // ...and rises onto it this fast (a time constant)
        constexpr double attackSeconds = 0.0004;
        constexpr double tapSeconds = 0.0005;     // the landing's tap, a time constant
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
    }

    float Downpour::patterRate (float rain) noexcept
    {
        // 0 no rain at all; then rising with RAIN's square: 40 a second at 0.1, 1000 at 0.5, 4000 at the top.
        const auto r = juce::jlimit (0.0f, 1.0f, rain);
        return 4000.0f * r * r;
    }

    float Downpour::dropRate (float rain, float dropsAmount) noexcept
    {
        // No rain, no drops; up to 12 a second at full RAIN and DROPS.
        return 12.0f * juce::jlimit (0.0f, 1.0f, dropsAmount) * std::sqrt (juce::jlimit (0.0f, 1.0f, rain));
    }

    void Downpour::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        output.setSize (2, juce::jmax (1, maxBlockSize));
        glide = 1.0f - (float) std::exp (-1.0 / (riseSeconds * sampleRate));
        tapDecay = (float) std::exp (-1.0 / (tapSeconds * sampleRate));
        followAttack = 1.0f - (float) std::exp (-1.0 / (0.002 * sampleRate));
        followRelease = 1.0f - (float) std::exp (-1.0 / (0.15 * sampleRate));
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize), 1 };
        for (auto& t : patterTone)
        {
            t.prepare (spec);
            t.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            t.setCutoffFrequency (juce::jmin (patterHz, (float) (0.4 * sampleRate)));
            t.setResonance (0.6f);
        }
        onGain.reset (sampleRate, 0.02);
        reset();
        clearMarks();
    }

    void Downpour::reset() noexcept
    {
        for (auto& d : drops)
            d = {};
        for (auto& t : patterTone)
            t.reset();
        distanceState = {};
        follower = 0.0f;
        onGain.setCurrentAndTargetValue (0.0f);
        output.clear();
        rendered = 0;
        silent = true;
        quietSamples = 0;
    }

    void Downpour::clearMarks() noexcept
    {
        for (auto& m : markSample)
            m.store (-1, std::memory_order_relaxed);
    }

    void Downpour::setNotes (const float* newNotes, int count) noexcept
    {
        if (count <= 0)
            return;
        numNotes = juce::jmin (count, maxNotes);
        std::copy_n (newNotes, numNotes, notes.begin());
    }

    void Downpour::startDrop (const Settings& s) noexcept
    {
        Drop* free = nullptr;
        for (auto& d : drops)
            if (! d.active)
            {
                free = &d;
                break;
            }
        if (free == nullptr)
            return;   // a storm: lost among the others
        auto& d = *free;

        // Its note: one sounding now, OCTAVE octaves up, now and then one more; kept where a drop can ring.
        auto note = numNotes > 0 ? notes[(size_t) juce::jmin (numNotes - 1, (int) (random01() * (float) numNotes))] : 72.0f + 24.0f * random01();
        note += 12.0f * (float) (juce::jlimit (0, octaveNames.size() - 1, s.octave) + 1);
        if (random01() < 0.2f)
            note += 12.0f;
        auto hz = 440.0f * std::exp2 ((note - 69.0f) / 12.0f);
        while (hz < 600.0f)
            hz *= 2.0f, note += 12.0f;
        while (hz > 0.42f * (float) sampleRate)
            hz *= 0.5f, note -= 12.0f;

        const auto r = random01();
        const auto strength = 0.3f + 0.7f * r * r;
        d.active = true;
        d.target = twoPi * hz / (float) sampleRate;
        d.freq = d.target * std::exp2 (-dropRise);
        d.phase = 0.0f;
        d.amp = strength;
        const auto ring = 0.08f + 0.22f * random01() * (0.5f + 0.5f * strength);   // seconds to -60 dB
        d.decay = (float) std::pow (0.001, 1.0 / (ring * sampleRate));
        d.attack = 0.0f;
        d.attackStep = 1.0f / (float) juce::jmax (1.0, attackSeconds * sampleRate);
        d.tap = strength * 0.6f * (1.0f - juce::jlimit (0.0f, 1.0f, s.distance));
        const auto pan = (random01() * 2.0f - 1.0f) * juce::jlimit (0.0f, 1.0f, s.width);
        const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        d.gainL = std::cos (angle) * juce::MathConstants<float>::sqrt2;
        d.gainR = std::sin (angle) * juce::MathConstants<float>::sqrt2;

        totalDrops.fetch_add (1, std::memory_order_relaxed);
        const auto k = (markIndex.load (std::memory_order_relaxed) + 1) % numMarks;
        markNote[(size_t) k].store (note, std::memory_order_relaxed);
        markPan[(size_t) k].store (pan, std::memory_order_relaxed);
        markStrength[(size_t) k].store (strength, std::memory_order_relaxed);
        markSample[(size_t) k].store (clock.load (std::memory_order_relaxed), std::memory_order_relaxed);
        markIndex.store (k, std::memory_order_relaxed);
        if (numDropNotes < (int) dropNotes.size())
            dropNotes[(size_t) numDropNotes++] = note;
    }

    void Downpour::render (const juce::dsp::AudioBlock<const float>& in, const Settings& s) noexcept
    {
        const auto n = juce::jmin ((int) in.getNumSamples(), output.getNumSamples());
        const auto numChannels = juce::jmin (2, (int) in.getNumChannels());
        rendered = n;
        onGain.setTargetValue (s.on ? 1.0f : 0.0f);
        const auto anyDrop = std::any_of (drops.begin(), drops.end(), [] (const Drop& d) { return d.active; });
        if (n <= 0 || numChannels == 0 || (! s.on && ! onGain.isSmoothing() && ! anyDrop))
        {
            // Off and done: nothing made, nothing kept ringing.
            if (! silent)
                reset();
            silent = true;
            rendered = juce::jmax (0, n);
            currentPatter.store (0.0f, std::memory_order_relaxed);
            currentDrops.store (0.0f, std::memory_order_relaxed);
            clock.fetch_add (juce::jmax (0, n), std::memory_order_relaxed);
            return;
        }

        const auto follow = juce::jlimit (0.0f, 1.0f, s.follow);
        const auto distance = juce::jlimit (0.0f, 1.0f, s.distance);
        const auto patterBase = patterRate (s.rain);
        const auto dropBase = dropRate (s.rain, s.drops);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        const auto lowPass = 1.0f - (float) std::exp (-twoPi * juce::jmin (16000.0f * std::pow (0.1f, distance), (float) (0.45 * sampleRate)) / (float) sampleRate);
        const auto gain = juce::Decibels::decibelsToGain (s.levelDb) * (1.0f - 0.35f * distance);
        const auto* inL = in.getChannelPointer (0);
        const auto* inR = numChannels > 1 ? in.getChannelPointer (1) : nullptr;
        auto* outL = output.getWritePointer (0);
        auto* outR = output.getWritePointer (1);
        float loudest = 0.0f, lastPatter = 0.0f, lastDrops = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const auto x = juce::jmax (std::abs (inL[i]), inR != nullptr ? std::abs (inR[i]) : 0.0f);
            follower += (x - follower) * (x > follower ? followAttack : followRelease);
            const auto heard = follower < 1.0e-4f ? 0.0f : juce::jmin (1.0f, follower / followRef);   // under -80 dB: nothing
            const auto share = s.on ? (1.0f - follow) + follow * heard : 0.0f;
            lastPatter = patterBase * share;
            lastDrops = dropBase * share;

            // The patter: a tap now and then (most soft, a few hard), each its own place.
            float tapL = 0.0f, tapR = 0.0f;
            if (lastPatter > 0.0f && random01() < lastPatter / (float) sampleRate)
            {
                const auto r = random01();
                const auto a = (0.15f + 0.85f * r * r * r) * patterMakeUp;
                const auto pan = (random01() * 2.0f - 1.0f) * width;
                tapL = a * (1.0f - pan);
                tapR = a * (1.0f + pan);
                totalPatter.fetch_add (1, std::memory_order_relaxed);
            }
            if (lastDrops > 0.0f && random01() < lastDrops / (float) sampleRate)
                startDrop (s);

            // The drops ringing.
            float dropL = 0.0f, dropR = 0.0f;
            for (auto& d : drops)
                if (d.active)
                {
                    d.freq += (d.target - d.freq) * glide;
                    d.phase += d.freq;
                    if (d.phase > juce::MathConstants<float>::pi)
                        d.phase -= twoPi;
                    d.attack = juce::jmin (1.0f, d.attack + d.attackStep);
                    const auto v = juce::dsp::FastMathApproximations::sin (d.phase) * d.amp * d.attack
                                 + (random01() * 2.0f - 1.0f) * d.tap;
                    dropL += v * d.gainL;
                    dropR += v * d.gainR;
                    d.amp *= d.decay;
                    d.tap *= tapDecay;
                    if (d.amp < 1.0e-4f)
                        d.active = false;
                }

            const auto l = patterTone[0].processSample (0, tapL) + dropL;
            const auto r = patterTone[1].processSample (0, tapR) + dropR;
            // DISTANCE: two one-pole low-passes in a row (12 dB an octave).
            distanceState[0] += (l - distanceState[0]) * lowPass;
            distanceState[1] += (r - distanceState[1]) * lowPass;
            distanceState[2] += (distanceState[0] - distanceState[2]) * lowPass;
            distanceState[3] += (distanceState[1] - distanceState[3]) * lowPass;
            const auto g = gain * onGain.getNextValue();
            outL[i] = distanceState[2] * g;
            outR[i] = distanceState[3] * g;
            loudest = juce::jmax (loudest, std::abs (outL[i]), std::abs (outR[i]));
        }

        // Nothing falling, nothing ringing and the filters all but still: let them rest at exactly zero (so a
        // preset's tail ends in true silence).
        const auto ringing = std::any_of (drops.begin(), drops.end(), [] (const Drop& d) { return d.active; });
        quietSamples = (lastPatter <= 0.0f && lastDrops <= 0.0f && ! ringing && loudest < 1.0e-6f) ? quietSamples + n : 0;
        if (quietSamples > (int) (0.05 * sampleRate))
        {
            for (auto& t : patterTone)
                t.reset();
            distanceState = {};
            output.clear (0, n);
            loudest = 0.0f;
        }
        silent = loudest == 0.0f;
        currentPatter.store (lastPatter, std::memory_order_relaxed);
        currentDrops.store (lastDrops, std::memory_order_relaxed);
        clock.fetch_add (n, std::memory_order_relaxed);
    }

    void Downpour::addTo (juce::dsp::AudioBlock<float> block) const noexcept
    {
        if (silent)
            return;
        const auto n = juce::jmin ((int) block.getNumSamples(), rendered);
        const auto numChannels = (int) block.getNumChannels();
        if (numChannels >= 2)
        {
            juce::FloatVectorOperations::add (block.getChannelPointer (0), output.getReadPointer (0), n);
            juce::FloatVectorOperations::add (block.getChannelPointer (1), output.getReadPointer (1), n);
        }
        else if (numChannels == 1)
        {
            auto* out = block.getChannelPointer (0);
            for (int i = 0; i < n; ++i)
                out[i] += 0.5f * (output.getSample (0, i) + output.getSample (1, i));
        }
    }

    std::array<Downpour::DropMark, Downpour::numMarks> Downpour::getRecentDrops() const noexcept
    {
        std::array<DropMark, numMarks> out {};
        const auto newest = markIndex.load (std::memory_order_relaxed);
        for (int k = 0; k < numMarks; ++k)
        {
            const auto j = (size_t) ((newest + 1 + k) % numMarks);
            out[(size_t) k] = { markNote[j].load (std::memory_order_relaxed), markPan[j].load (std::memory_order_relaxed),
                                markStrength[j].load (std::memory_order_relaxed), markSample[j].load (std::memory_order_relaxed) };
        }
        return out;
    }

    int Downpour::takeDropNotes (float* out, int capacity) noexcept
    {
        const auto count = juce::jmin (capacity, numDropNotes);
        std::copy_n (dropNotes.begin(), count, out);
        numDropNotes = 0;
        return count;
    }
}
