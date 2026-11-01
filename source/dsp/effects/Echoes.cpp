#include "Echoes.h"

namespace effects
{
    namespace
    {
        constexpr double edgeSeconds = 0.005;   // a stretch's edges faded over this, so a cut doesn't click
    }

    void Echoes::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        capacity = (int) std::ceil (maxSeconds * sampleRate) + maxBlockSize + 8;
        for (auto& side : recording)
            side.assign ((size_t) capacity, 0.0f);
        for (auto& side : loopCopy)
            side.assign ((size_t) capacity / 2 + 8, 0.0f);
        reset();
    }

    void Echoes::reset() noexcept
    {
        for (auto& side : recording)
            std::fill (side.begin(), side.end(), 0.0f);
        writeHead = 0;
        ownClock = 0.0;
        lastSegment = -1;
        loopLength = loopRepeatsLeft = 0;
        loopCaught = 0;
        loopPeak = 0.0f;
        ageState = {};
    }

    int Echoes::distanceSamples (int distance, double bpm) const noexcept
    {
        const auto index = juce::jlimit (0, (int) std::size (distanceBeats) - 1, distance);
        auto samples = (double) distanceBeats[(size_t) index] * 60.0 / juce::jmax (20.0, bpm) * sampleRate;
        // REVERSE reaches back two distances and TAPE three: keep that inside the recording.
        while (samples * 3.0 > maxSeconds * sampleRate)
            samples *= 0.5;
        return juce::jmax (1, (int) std::round (samples));
    }

    int Echoes::loopRepeats (float feedback) noexcept
    {
        if (feedback >= 0.99f)
            return 0;   // for ever
        return 1 + juce::roundToInt (juce::jlimit (0.0f, 1.0f, feedback) * 7.0f);
    }

    float Echoes::readAt (int channel, double samplesAgo) const noexcept
    {
        // Linear between the two nearest samples (TAPE reads between samples).
        auto position = (double) writeHead - samplesAgo;
        while (position < 0.0)
            position += capacity;
        const auto i0 = (int) position;
        const auto frac = (float) (position - i0);
        const auto& r = recording[(size_t) channel];
        const auto a = r[(size_t) (i0 % capacity)], b = r[(size_t) ((i0 + 1) % capacity)];
        return a + (b - a) * frac;
    }

    void Echoes::catchLoop (int d) noexcept
    {
        // The last d samples, copied in (at most) two straight runs: it can be seconds of sound, on the audio thread.
        loopLength = d;
        loopPeak = 0.0f;
        const auto start = ((writeHead - d) % capacity + capacity) % capacity;
        const auto first = juce::jmin (d, capacity - start);
        for (size_t c = 0; c < 2; ++c)
        {
            std::copy_n (recording[c].begin() + start, first, loopCopy[c].begin());
            std::copy_n (recording[c].begin(), d - first, loopCopy[c].begin() + first);
            const auto range = juce::FloatVectorOperations::findMinAndMax (loopCopy[c].data(), d);
            loopPeak = juce::jmax (loopPeak, -range.getStart(), range.getEnd());
        }
        loopCaught = writeHead;
    }

    void Echoes::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        if (capacity == 0)
            return;
        if (! s.on && bypass.isSilent())
        {
            if (lastSegment >= 0 || writeHead != 0)
                reset();   // off: start fresh next time (no old past coming back)
            return;
        }

        bypass.process (block, s.on, [&] (juce::dsp::AudioBlock<float> wet)
        {
            const auto numSamples = (int) wet.getNumSamples();
            const auto numChannels = juce::jmin (2, (int) wet.getNumChannels());
            const auto d = distanceSamples (s.distance, bpm);
            const auto stretch = s.mode == Mode::tape ? 2 * d : d;
            const auto edge = juce::jmax (1.0, edgeSeconds * sampleRate);
            const auto mix = juce::jlimit (0.0f, 1.0f, s.mix);
            const auto feedback = s.mode == Mode::loop ? 0.0f : juce::jlimit (0.0f, 0.95f, s.feedback);
            const auto ageCoefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * ageCutoffHz (s.age) / (float) sampleRate);
            const auto drive = 1.0f + 2.0f * juce::jlimit (0.0f, 1.0f, s.age);
            const auto songClock = ppq >= 0.0 ? ppq * 60.0 / juce::jmax (20.0, bpm) * sampleRate : ownClock;

            auto peak = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                const auto time = songClock + i;
                const auto segment = (long long) std::floor (time / stretch);
                const auto position = time - (double) segment * stretch;   // 0 to stretch

                if (s.mode == Mode::loop && segment != lastSegment && lastSegment >= 0)   // (not on the first sample heard)
                {
                    // A new stretch: once the last one caught has repeated enough, catch the stretch just played.
                    if (loopRepeatsLeft > 0)
                        --loopRepeatsLeft;
                    const auto repeats = loopRepeats (s.feedback);
                    // For ever: caught once, but caught again while what it holds is silence (a stretch before
                    // anything was played would otherwise loop silence for ever).
                    if (loopLength != d || (repeats != 0 && loopRepeatsLeft == 0) || (repeats == 0 && (loopLength == 0 || loopPeak < 1.0e-4f)))
                    {
                        catchLoop (d);
                        loopRepeatsLeft = repeats == 0 ? -1 : repeats;
                    }
                }
                lastSegment = segment;

                // The fade at the stretch's edges (ECHO is continuous: none).
                auto fade = 1.0f;
                if (s.mode != Mode::echo)
                    fade = (float) juce::jlimit (0.0, 1.0, juce::jmin (position / edge, ((double) stretch - position) / edge));

                for (int c = 0; c < numChannels; ++c)
                {
                    auto* x = wet.getChannelPointer ((size_t) c);
                    float past = 0.0f;
                    switch (s.mode)
                    {
                        case Mode::echo:    past = readAt (c, (double) d); break;
                        case Mode::reverse: past = readAt (c, 2.0 * position + 1.0); break;          // the last stretch, backwards
                        case Mode::tape:    past = readAt (c, 2.0 * d + position * 0.5); break;   // the last stretch's first half, at half speed
                        case Mode::loop:    past = loopLength > 0 ? loopCopy[(size_t) c][(size_t) juce::jlimit (0, loopLength - 1, (int) position)] : 0.0f; break;
                    }
                    // AGE: darker and a little worn each pass.
                    auto& state = ageState[(size_t) c];
                    state += (past - state) * ageCoefficient;
                    const auto aged = std::tanh (state * drive) / drive * fade;
                    const auto input = x[i];
                    recording[(size_t) c][(size_t) writeHead] = input + feedback * aged;
                    x[i] = input + mix * aged;
                    peak = juce::jmax (peak, std::abs (mix * aged));
                }
                if (numChannels == 1)
                    recording[1][(size_t) writeHead] = recording[0][(size_t) writeHead];
                writeHead = (writeHead + 1) % capacity;
            }
            if (ppq < 0.0)
                ownClock += numSamples;
            else
                ownClock = 0.0;
            for (auto& v : ageState)
                if (std::abs (v) < 1.0e-15f)
                    v = 0.0f;
            const auto time = songClock + numSamples;
            shownSegmentPos.store ((float) (time - std::floor (time / stretch) * stretch), std::memory_order_relaxed);
            shownRead.store (writeHead, std::memory_order_relaxed);
            pastPeak.store (peak, std::memory_order_relaxed);
        });
        if (! s.on && bypass.isSilent())
            pastPeak.store (0.0f, std::memory_order_relaxed);
    }

    Echoes::Picture Echoes::picture (int columns, double bpm, const Settings& s) const
    {
        // The last two stretches (four for TAPE), newest on the left; each column the loudest of its slice. Read without
        // a lock: a display may tear for a frame, the sound never does.
        Picture p;
        if (capacity == 0 || columns <= 0)
            return p;
        const auto d = distanceSamples (s.distance, bpm);
        const auto stretch = s.mode == Mode::tape ? 2 * d : d;
        const auto span = 2 * stretch;
        const auto head = shownRead.load (std::memory_order_relaxed);
        const auto per = juce::jmax (1, span / columns);
        p.levels.resize ((size_t) columns);
        for (int col = 0; col < columns; ++col)
        {
            float peak = 0.0f;
            for (int k = 0; k < per; k += juce::jmax (1, per / 64))
            {
                auto index = head - 1 - col * per - k;
                while (index < 0)
                    index += capacity;
                peak = juce::jmax (peak, std::abs (recording[0][(size_t) (index % capacity)]), std::abs (recording[1][(size_t) (index % capacity)]));
            }
            p.levels[(size_t) col] = peak;
        }
        const auto position = (double) shownSegmentPos.load (std::memory_order_relaxed);
        const auto toShare = [span] (double samplesAgo) { return (float) juce::jlimit (0.0, 1.0, samplesAgo / span); };
        switch (s.mode)
        {
            case Mode::echo:    p.readFrom = p.readTo = toShare (d); break;
            case Mode::reverse: p.readFrom = toShare (2.0 * position); p.readTo = toShare (position + d); p.backwards = true; break;
            case Mode::tape:    p.readFrom = toShare (2.0 * d + position * 0.5); p.readTo = toShare (position + d); break;
            case Mode::loop:
            {
                auto ago = (double) (head - loopCaught);
                while (ago < 0.0)
                    ago += capacity;
                p.readFrom = toShare (ago + d - position);
                p.readTo = toShare (ago);
                break;
            }
        }
        return p;
    }
}
