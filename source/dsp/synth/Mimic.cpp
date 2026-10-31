#include "Mimic.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>

namespace synth::mimic
{
    namespace
    {
        constexpr int frameSize = 2048;     // samples per wavetable frame
        constexpr int fftOrder = 11;        // 2048
        constexpr int maxFrames = 64;
        constexpr double frameSpacing = 0.04;   // seconds between frames, at most 64 of them
        using Complex = std::complex<float>;

        float median (std::vector<float> values)
        {
            if (values.empty())
                return 0.0f;
            const auto middle = values.begin() + (std::ptrdiff_t) (values.size() / 2);
            std::nth_element (values.begin(), middle, values.end());
            return *middle;
        }

        // The straight line through (i, values[i]) for i in [from, to): its slope per step.
        float slope (const std::vector<float>& values, int from, int to)
        {
            const auto n = to - from;
            if (n < 2)
                return 0.0f;
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (int i = from; i < to; ++i)
            {
                sx += i;
                sy += values[(size_t) i];
                sxx += (double) i * i;
                sxy += (double) i * values[(size_t) i];
            }
            const auto d = n * sxx - sx * sx;
            return d == 0.0 ? 0.0f : (float) ((n * sxy - sx * sy) / d);
        }

        // Catmull-Rom between samples: x at a fractional position.
        float readCubic (const std::vector<float>& x, double position)
        {
            const auto i = (int) std::floor (position);
            const auto t = (float) (position - i);
            const auto at = [&x] (int k) { return x[(size_t) juce::jlimit (0, (int) x.size() - 1, k)]; };
            const auto p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
            return p1 + 0.5f * t * (p2 - p0 + t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + t * (3.0f * (p1 - p2) + p3 - p0)));
        }

        // YIN's cumulative-mean-normalised difference: the lag (fractional) where `x` best matches itself, between
        // `minLag` and `maxLag`, over `window` samples from `x`. `clarity` 0 to 1 (1 - the dip's depth). 0 if none.
        double yinLag (const float* x, int window, int minLag, int maxLag, float& clarity, std::vector<float>& d)
        {
            d.assign ((size_t) maxLag + 2, 0.0f);
            for (int tau = 1; tau <= maxLag + 1; ++tau)
            {
                float sum = 0.0f;
                for (int j = 0; j < window; ++j)
                {
                    const auto diff = x[j] - x[j + tau];
                    sum += diff * diff;
                }
                d[(size_t) tau] = sum;
            }
            // Normalised: d'(tau) = d(tau) / (mean of d up to tau).
            std::vector<float> n ((size_t) maxLag + 2, 1.0f);
            float running = 0.0f;
            for (int tau = 1; tau <= maxLag + 1; ++tau)
            {
                running += d[(size_t) tau];
                n[(size_t) tau] = running > 0.0f ? d[(size_t) tau] * (float) tau / running : 1.0f;
            }
            int best = -1;
            for (int tau = minLag; tau <= maxLag; ++tau)
                if (n[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 <= maxLag && n[(size_t) tau + 1] < n[(size_t) tau])
                        ++tau;
                    best = tau;
                    break;
                }
            if (best < 0)
            {
                best = minLag;
                for (int tau = minLag; tau <= maxLag; ++tau)
                    if (n[(size_t) tau] < n[(size_t) best])
                        best = tau;
            }
            clarity = juce::jlimit (0.0f, 1.0f, 1.0f - n[(size_t) best]);
            // A parabola through the dip and its neighbours.
            const auto a = n[(size_t) best - 1], b = n[(size_t) best], c = n[(size_t) best + 1];
            const auto denominator = a - 2.0f * b + c;
            const auto shift = denominator > 1.0e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denominator) : 0.0f;
            return best + shift;
        }

        // The lag near `rough` (full rate) where the plain difference is smallest, to a fraction of a sample.
        double refineLag (const std::vector<float>& x, int from, int window, double rough, int reach)
        {
            const auto low = juce::jmax (2, (int) std::floor (rough) - reach), high = (int) std::ceil (rough) + reach;
            if (from < 0 || from + window + high + 1 >= (int) x.size())
                return rough;
            std::vector<double> d ((size_t) (high - low + 3), 0.0);
            for (int tau = low - 1; tau <= high + 1; ++tau)
            {
                double sum = 0.0;
                for (int j = 0; j < window; ++j)
                {
                    const auto diff = x[(size_t) (from + j)] - x[(size_t) (from + j + tau)];
                    sum += diff * diff;
                }
                d[(size_t) (tau - low + 1)] = sum;
            }
            int best = low;
            for (int tau = low; tau <= high; ++tau)
                if (d[(size_t) (tau - low + 1)] < d[(size_t) (best - low + 1)])
                    best = tau;
            const auto a = d[(size_t) (best - low)], b = d[(size_t) (best - low + 1)], c = d[(size_t) (best - low + 2)];
            const auto denominator = a - 2.0 * b + c;
            return best + (denominator > 1.0e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denominator) : 0.0);
        }

        // The harmonics (1 to count) of one cycle of `period` samples starting at `start`, the cycle resampled to
        // frameSize samples first. Element 0 unused.
        std::vector<Complex> cycleHarmonics (const std::vector<float>& x, double start, double period, int count,
                                             juce::dsp::FFT& fft, std::vector<float>& scratch)
        {
            scratch.assign ((size_t) frameSize * 2, 0.0f);
            for (int n = 0; n < frameSize; ++n)
                scratch[(size_t) n] = readCubic (x, start + period * n / frameSize);
            fft.performRealOnlyForwardTransform (scratch.data(), true);
            std::vector<Complex> h ((size_t) count + 1);
            for (int k = 1; k <= count; ++k)
                h[(size_t) k] = { scratch[(size_t) (2 * k)], scratch[(size_t) (2 * k + 1)] };
            return h;
        }

        // Turns harmonics back into one cycle of frameSize samples.
        std::vector<float> cycleFrom (const std::vector<Complex>& h, juce::dsp::FFT& fft, std::vector<float>& scratch)
        {
            scratch.assign ((size_t) frameSize * 2, 0.0f);
            for (int k = 1; k < (int) h.size() && k < frameSize / 2; ++k)
            {
                scratch[(size_t) (2 * k)] = h[(size_t) k].real();
                scratch[(size_t) (2 * k + 1)] = h[(size_t) k].imag();
                scratch[(size_t) (2 * (frameSize - k))] = h[(size_t) k].real();       // the mirror half
                scratch[(size_t) (2 * (frameSize - k) + 1)] = -h[(size_t) k].imag();
            }
            fft.performRealOnlyInverseTransform (scratch.data());
            return { scratch.begin(), scratch.begin() + frameSize };
        }
    }

    juce::String noteName (int note)
    {
        static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((note % 12) + 12) % 12] + juce::String (note / 12 - 1);
    }

    Analysis analyse (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name)
    {
        Analysis a;
        a.name = name;
        a.sampleRate = sampleRate;
        const auto total = juce::jmin (audio.getNumSamples(), (int) (maxSeconds * sampleRate));
        const auto channels = audio.getNumChannels();
        a.seconds = total / sampleRate;
        if (channels == 0 || total < (int) (0.05 * sampleRate))
        {
            a.problem = "The recording is too short (it needs at least a twentieth of a second).";
            return a;
        }

        // Mono, and how wide it is.
        std::vector<float> x ((size_t) total);
        double mid = 0.0, side = 0.0;
        for (int i = 0; i < total; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c)
                sum += audio.getSample (c, i);
            x[(size_t) i] = sum / (float) channels;
            if (channels >= 2)
            {
                const auto l = audio.getSample (0, i), r = audio.getSample (1, i);
                mid += 0.25 * (l + r) * (l + r);
                side += 0.25 * (l - r) * (l - r);
            }
        }
        a.width = mid > 0.0 ? (float) juce::jlimit (0.0, 1.5, std::sqrt (side / mid)) : 0.0f;

        // THE LEVEL every 10 ms (20 ms windows), dB.
        const auto hop = juce::jmax (1, juce::roundToInt (sampleRate * Analysis::hopSeconds));
        const auto hops = total / hop;
        std::vector<float> db ((size_t) hops, -120.0f);
        for (int h = 0; h < hops; ++h)
        {
            const auto from = juce::jmax (0, h * hop - hop / 2), to = juce::jmin (total, h * hop + hop + hop / 2);
            double power = 0.0;
            for (int i = from; i < to; ++i)
                power += (double) x[(size_t) i] * x[(size_t) i];
            power /= juce::jmax (1, to - from);
            db[(size_t) h] = (float) juce::jmax (-120.0, 10.0 * std::log10 (power + 1.0e-14) + 3.01);   // a sine's peak dB
        }
        const auto peakHop = (int) (std::max_element (db.begin(), db.end()) - db.begin());
        const auto peakDb = db[(size_t) peakHop];
        if (peakDb < -60.0f)
        {
            a.problem = "The recording is too quiet (its loudest moment is under -60 dB).";
            return a;
        }
        a.levels.resize ((size_t) hops);
        for (int h = 0; h < hops; ++h)
            a.levels[(size_t) h] = db[(size_t) h] - peakDb;
        const auto& rel = a.levels;
        int startHop = 0, endHop = hops - 1;
        while (startHop < peakHop && rel[(size_t) startHop] < -40.0f)
            ++startHop;
        while (endHop > peakHop && rel[(size_t) endHop] < -50.0f)
            --endHop;
        {
            // The note starts at the first sample a twentieth of the loudest sample's height.
            float loudest = 0.0f;
            for (int i = startHop * hop; i < juce::jmin (total, (peakHop + 1) * hop); ++i)
                loudest = juce::jmax (loudest, std::abs (x[(size_t) i]));
            auto i = juce::jmax (0, startHop * hop - hop);
            while (i < total - 1 && std::abs (x[(size_t) i]) < 0.05f * loudest)
                ++i;
            a.start = i / sampleRate;
        }
        a.end = (endHop + 1) * Analysis::hopSeconds;

        // THE PITCH every 10 ms: YIN at a reduced rate (fundamentals 25 Hz to 2 kHz), refined at the full rate.
        const auto decimation = juce::jmax (1, (int) (sampleRate / 12000.0));
        const auto reducedRate = sampleRate / decimation;
        std::vector<float> reduced ((size_t) (total / decimation));
        for (size_t i = 0; i < reduced.size(); ++i)
        {
            float sum = 0.0f;
            for (int k = 0; k < decimation; ++k)
                sum += x[i * (size_t) decimation + (size_t) k];
            reduced[i] = sum / (float) decimation;
        }
        const auto minLag = juce::jmax (2, (int) (reducedRate / 2000.0));
        const auto maxLag = (int) (reducedRate / 25.0) + 1;
        const auto window = maxLag;
        std::vector<float> hz ((size_t) hops, 0.0f), clarity ((size_t) hops, 0.0f), scratch;
        for (int h = startHop; h <= endHop; ++h)
        {
            if (rel[(size_t) h] < -35.0f)
                continue;
            const auto begin = juce::jmax (0, h * hop / decimation - (window + maxLag) / 2);
            if (begin + window + maxLag + 2 >= (int) reduced.size())
                continue;
            float c = 0.0f;
            const auto lag = yinLag (reduced.data() + begin, window, minLag, maxLag, c, scratch);
            if (c < 0.7f)
                continue;
            const auto fullWindow = juce::jmin (window * decimation, (int) (0.03 * sampleRate));
            const auto period = refineLag (x, begin * decimation, fullWindow, lag * decimation, decimation + 1);
            if (period > 1.0)
            {
                hz[(size_t) h] = (float) (sampleRate / period);
                clarity[(size_t) h] = c;
            }
        }
        std::vector<float> confident, clear;
        for (int h = 0; h < hops; ++h)
            if (clarity[(size_t) h] >= 0.8f)
            {
                confident.push_back (hz[(size_t) h]);
                clear.push_back (clarity[(size_t) h]);
            }
        if (confident.size() < 5)
        {
            a.problem = "No clear pitch found: MIMIC needs a single note (a chord, a drum or a noise has none to follow).";
            return a;
        }
        a.hz = median (confident);
        a.clarity = median (clear);
        const auto midi = 69.0 + 12.0 * std::log2 (a.hz / 440.0);
        a.rootNote = juce::jlimit (0, 127, juce::roundToInt (midi));
        a.cents = (float) ((midi - a.rootNote) * 100.0);
        a.pitches.assign ((size_t) hops, std::numeric_limits<float>::quiet_NaN());
        for (int h = 0; h < hops; ++h)
            if (hz[(size_t) h] > 0.0f && clarity[(size_t) h] >= 0.75f)
                a.pitches[(size_t) h] = (float) (1200.0 * std::log2 (hz[(size_t) h] / a.hz));
        // A wild reading (an octave jump for a moment) is replaced by its neighbours' middle one.
        for (int h = 1; h + 1 < hops; ++h)
            if (! std::isnan (a.pitches[(size_t) h - 1]) && ! std::isnan (a.pitches[(size_t) h + 1]) && ! std::isnan (a.pitches[(size_t) h]))
                if (std::abs (a.pitches[(size_t) h]) > 600.0f && std::abs (a.pitches[(size_t) h - 1]) < 300.0f)
                    a.pitches[(size_t) h] = 0.5f * (a.pitches[(size_t) h - 1] + a.pitches[(size_t) h + 1]);
        const auto hzAt = [&] (double seconds)
        {
            const auto h = juce::jlimit (0, hops - 1, (int) std::round (seconds / Analysis::hopSeconds));
            for (int reach = 0; reach < 20; ++reach)
                for (const auto k : { h - reach, h + reach })
                    if (k >= 0 && k < hops && ! std::isnan (a.pitches[(size_t) k]))
                        return a.hz * std::exp2 (a.pitches[(size_t) k] / 1200.0);
            return a.hz;
        };

        // THE LOUDNESS: attack to 90% of the peak; then either held at a level (sustained) or dying away.
        const auto amplitude = [&] (int h) { return std::pow (10.0f, rel[(size_t) h] / 20.0f); };
        {
            int h = startHop;
            while (h < peakHop && amplitude (h) < 0.9f)
                ++h;
            a.attackMs = (float) juce::jlimit (1.0, 20000.0, ((h + 0.5) * Analysis::hopSeconds - a.start) * 1000.0);
        }
        std::vector<float> after (rel.begin() + peakHop, rel.begin() + endHop + 1);
        const auto held = median (after);
        int releaseHop = peakHop;
        for (int h = peakHop; h <= endHop; ++h)
            if (rel[(size_t) h] >= held - 3.0f)
                releaseHop = h;
        const auto plateauSeconds = (releaseHop - peakHop) * Analysis::hopSeconds;
        const auto plateauSlope = slope (rel, peakHop + (releaseHop - peakHop) / 2, releaseHop + 1) / (float) Analysis::hopSeconds;
        a.sustained = plateauSeconds >= 0.25 && plateauSlope > -6.0f && held > -30.0f;
        // The time a fall at `dbPerSecond` takes over the 87 dB a curve of -1 covers.
        const auto fallMs = [] (float dbPerSecond) { return juce::jlimit (5.0f, 20000.0f, 87.0f / juce::jmax (0.01f, -dbPerSecond) * 1000.0f); };
        int toneEndHop = releaseHop;
        if (a.sustained)
        {
            std::vector<float> latter (rel.begin() + peakHop + (releaseHop - peakHop) / 2, rel.begin() + releaseHop + 1);
            a.sustain = juce::jlimit (0.0f, 1.0f, std::pow (10.0f, median (latter) / 20.0f));
            const auto settled = a.sustain + 0.1f * (1.0f - a.sustain);
            int h = peakHop;
            while (h < releaseHop && amplitude (h) > settled)
                ++h;
            a.decayMs = a.sustain > 0.9f ? 50.0f : juce::jlimit (10.0f, 20000.0f, (float) ((h - peakHop) * Analysis::hopSeconds / 0.23 * 1000.0));
            a.settle = (h + 0.5) * Analysis::hopSeconds;
            // The release: how fast it falls once let go.
            int last = releaseHop;
            while (last < endHop && rel[(size_t) last] > held - 40.0f)
                ++last;
            a.releaseMs = last - releaseHop >= 3 ? fallMs (slope (rel, releaseHop, last + 1) / (float) Analysis::hopSeconds) : 100.0f;
            a.releaseStart = (releaseHop + 0.5) * Analysis::hopSeconds;
        }
        else
        {
            a.sustain = 0.0f;
            int last = peakHop;
            while (last < endHop && rel[(size_t) last] > -40.0f)
                ++last;
            a.decayMs = fallMs (slope (rel, peakHop, last + 1) / (float) Analysis::hopSeconds);
            a.releaseMs = a.decayMs;   // let go early, it keeps dying away at its own pace
            a.settle = (peakHop + 0.5) * Analysis::hopSeconds;
            a.releaseStart = (last + 0.5) * Analysis::hopSeconds;
            toneEndHop = peakHop;
            while (toneEndHop < endHop && rel[(size_t) toneEndHop] > -30.0f)
                ++toneEndHop;
        }

        // The steady part: where vibrato, unison and noise are measured.
        const auto steadyFrom = juce::jmax (a.settle, a.start + 0.05);
        const auto steadyTo = a.sustained ? a.releaseStart : juce::jmin (a.releaseStart, steadyFrom + 0.6);

        // VIBRATO: what's left of the pitch once its slow drift is taken out, if it wobbles regularly (2.5-10 Hz).
        {
            const auto from = (int) (steadyFrom / Analysis::hopSeconds), to = (int) (steadyTo / Analysis::hopSeconds);
            std::vector<float> wobble;
            for (int h = from; h < to; ++h)
            {
                if (std::isnan (a.pitches[(size_t) h]))
                    continue;
                float sum = 0.0f;
                int count = 0;
                for (int k = h - 15; k <= h + 15; ++k)
                    if (k >= from && k < to && ! std::isnan (a.pitches[(size_t) k]))
                    {
                        sum += a.pitches[(size_t) k];
                        ++count;
                    }
                wobble.push_back (a.pitches[(size_t) h] - sum / (float) count);
            }
            // Smoothed over 50 ms: breath and noise jitter the readings from one 10 ms to the next, which would read as
            // a deeper vibrato (a 5 Hz wobble loses only a few percent).
            if (wobble.size() >= 5)
            {
                auto smoothed = wobble;
                for (size_t i = 2; i + 2 < wobble.size(); ++i)
                    smoothed[i] = (wobble[i - 2] + wobble[i - 1] + wobble[i] + wobble[i + 1] + wobble[i + 2]) / 5.0f;
                wobble = std::move (smoothed);
            }
            if (wobble.size() >= 40)
            {
                double power = 0.0;
                for (const auto w : wobble)
                    power += (double) w * w;
                const auto deviation = (float) std::sqrt (power / (double) wobble.size());
                float bestCorrelation = 0.0f;
                int bestLag = 0;
                for (int lag = 10; lag <= 40 && lag < (int) wobble.size() / 2; ++lag)
                {
                    double sum = 0.0;
                    for (size_t i = 0; i + (size_t) lag < wobble.size(); ++i)
                        sum += (double) wobble[i] * wobble[i + (size_t) lag];
                    const auto correlation = (float) (sum / ((double) (wobble.size() - (size_t) lag) * power / (double) wobble.size()));
                    if (correlation > bestCorrelation)
                    {
                        bestCorrelation = correlation;
                        bestLag = lag;
                    }
                }
                if (deviation > 3.0f && bestCorrelation > 0.3f && bestLag > 0)
                {
                    a.vibratoHz = (float) (1.0 / (bestLag * Analysis::hopSeconds));
                    a.vibratoCents = deviation * std::sqrt (2.0f);
                    // How late it starts: the first stretch where the wobble reaches half its depth.
                    for (size_t i = 0; i + 15 < wobble.size(); ++i)
                    {
                        float most = 0.0f;
                        for (size_t k = i; k < i + 15; ++k)
                            most = juce::jmax (most, std::abs (wobble[k]));
                        if (most > 0.5f * a.vibratoCents)
                        {
                            a.vibratoDelayMs = (float) juce::jmax (0.0, (steadyFrom + (double) i * Analysis::hopSeconds - a.start) * 1000.0);
                            break;
                        }
                    }
                }
            }
        }

        // A SWOOP into the note: the pitch starting well off and settling (within 35 cents for 60 ms).
        {
            const auto first = (int) std::ceil (a.start / Analysis::hopSeconds);
            int firstSure = first;
            while (firstSure < endHop && std::isnan (a.pitches[(size_t) firstSure]))
                ++firstSure;
            int settled = firstSure;
            for (; settled < juce::jmin (endHop, firstSure + 60); ++settled)
            {
                bool steady = true;
                for (int k = settled; k < settled + 6 && k <= endHop; ++k)
                    if (std::isnan (a.pitches[(size_t) k]) || std::abs (a.pitches[(size_t) k]) > 35.0f)
                        steady = false;
                if (steady)
                    break;
            }
            float most = 0.0f;
            for (int h = firstSure; h < settled; ++h)
                if (! std::isnan (a.pitches[(size_t) h]))
                    most = juce::jmax (most, std::abs (a.pitches[(size_t) h]));
            if (settled - firstSure >= 2 && most >= 60.0f && settled < firstSure + 60)
            {
                float last = a.pitches[(size_t) firstSure];
                for (int h = first; h <= settled; ++h)
                {
                    if (h >= firstSure && ! std::isnan (a.pitches[(size_t) h]))
                        last = a.pitches[(size_t) h];
                    a.swoop.push_back (h == settled ? 0.0f : last / 100.0f);
                }
            }
        }

        // THE TONE: the sound's own cycles, a frame every 40 ms or so from the start to where the tone settles
        // (held sounds: to where it's let go; dying ones: to 30 dB down).
        juce::dsp::FFT fft (fftOrder);
        std::vector<float> fftScratch;
        a.travelSeconds = juce::jmax (0.0, (toneEndHop + 0.5) * Analysis::hopSeconds - a.start);
        const auto frames = juce::jlimit (1, maxFrames, juce::roundToInt (a.travelSeconds / frameSpacing));
        auto samples = std::make_shared<std::vector<float>>();
        samples->reserve ((size_t) frames * frameSize);
        for (int f = 0; f < frames; ++f)
        {
            auto t = frames == 1 ? a.start + juce::jmin (0.1, 0.5 * (steadyFrom + steadyTo) - a.start)
                                 : a.start + a.travelSeconds * f / (frames - 1);
            const auto localHz = hzAt (t);
            const auto period = sampleRate / localHz;
            const auto cycles = juce::jlimit (2, 6, juce::roundToInt (0.03 * localHz));
            t = juce::jmin (t, (total - 2 - cycles * period) / sampleRate);
            const auto count = juce::jmax (1, juce::jmin (frameSize / 2 - 1, (int) (period / 2.0 * 0.95)));
            std::vector<Complex> sum ((size_t) count + 1);
            std::vector<float> power ((size_t) count + 1, 0.0f);
            for (int c = 0; c < cycles; ++c)
            {
                auto h = cycleHarmonics (x, juce::jmax (0.0, t * sampleRate) + c * period, period, count, fft, fftScratch);
                // Lined up: every cycle turned so its fundamental starts at the same point.
                const auto phase = std::arg (h[1]);
                for (int k = 1; k <= count; ++k)
                {
                    h[(size_t) k] *= std::polar (1.0f, -phase * (float) k);
                    sum[(size_t) k] += h[(size_t) k];
                    power[(size_t) k] += std::norm (h[(size_t) k]);
                }
            }
            // Each harmonic keeps its full level (the cycles' power), at the averaged phase.
            std::vector<Complex> h ((size_t) count + 1);
            for (int k = 1; k <= count; ++k)
            {
                const auto level = std::sqrt (power[(size_t) k] / (float) cycles);
                const auto direction = std::abs (sum[(size_t) k]) > 1.0e-12f ? sum[(size_t) k] / std::abs (sum[(size_t) k]) : Complex (1.0f, 0.0f);
                h[(size_t) k] = level * direction;
            }
            auto cycle = cycleFrom (h, fft, fftScratch);
            double rms = 0.0;
            for (const auto s : cycle)
                rms += (double) s * s;
            rms = std::sqrt (rms / frameSize);
            for (auto& s : cycle)
                samples->push_back (rms > 1.0e-9 ? (float) (s * 0.25 / rms) : 0.0f);
        }
        a.table = { "MIMIC " + name, frameSize, samples };

        // UNISON: copies a little out of tune make each harmonic's level rise and fall (beat), the higher the
        // harmonic the faster. A single oscillator's harmonics hold steady.
        {
            std::vector<std::vector<float>> levels;   // by cycle: harmonics 1 to 12, as shares of the cycle's power
            auto t = steadyFrom;
            while (t < steadyTo && levels.size() < 400)
            {
                const auto period = sampleRate / hzAt (t);
                if (t * sampleRate + 2 * period >= total)
                    break;
                const auto h = cycleHarmonics (x, t * sampleRate, period, 12, fft, fftScratch);
                std::vector<float> share (13, 0.0f);
                float sum = 0.0f;
                for (int k = 1; k <= 12; ++k)
                    sum += (share[(size_t) k] = std::abs (h[(size_t) k]));
                for (auto& s : share)
                    s = sum > 0.0f ? s / sum : 0.0f;
                levels.push_back (std::move (share));
                t += period / sampleRate;
            }
            if (levels.size() >= 20)
            {
                const auto n = (int) levels.size();
                const auto seconds = (double) n / a.hz;
                std::vector<float> spreads, beats, lowRates, highRates;
                for (int k = 2; k <= 10; ++k)
                {
                    std::vector<float> series;
                    for (const auto& l : levels)
                        series.push_back (l[(size_t) k]);
                    const auto mean = std::accumulate (series.begin(), series.end(), 0.0f) / (float) n;
                    if (mean < 0.02f)
                        continue;
                    // Take out a straight-line drift (a slow change of tone isn't beating).
                    const auto s = slope (series, 0, n);
                    for (int i = 0; i < n; ++i)
                        series[(size_t) i] -= mean + s * (float) (i - n / 2);
                    double power = 0.0;
                    int crossings = 0;
                    for (int i = 0; i < n; ++i)
                    {
                        power += (double) series[(size_t) i] * series[(size_t) i];
                        if (i > 0 && (series[(size_t) i] > 0.0f) != (series[(size_t) i - 1] > 0.0f))
                            ++crossings;
                    }
                    spreads.push_back ((float) std::sqrt (power / n) / mean);
                    const auto rate = (float) (crossings / (2.0 * seconds));
                    beats.push_back (rate / (float) k);
                    (k <= 4 ? lowRates : highRates).push_back (rate);
                }
                // Unison's beating speeds up with the harmonic (the 9th beats about three times as fast as the 3rd);
                // noise shakes every harmonic at random, about as fast low as high. Only the first is unison.
                const auto speedsUp = ! lowRates.empty() && ! highRates.empty() && median (highRates) > 1.8f * median (lowRates);
                if (spreads.size() >= 3 && median (spreads) > 0.12f && speedsUp)
                {
                    // The beating's rate, per harmonic, measured on unisons of five copies spread evenly: about the outer
                    // copies' distance from the middle (8 cents read 8, 20 read 23 in the tests).
                    const auto apartHz = median (beats);
                    a.detuneCents = juce::jlimit (3.0f, 100.0f, (float) (1200.0 * std::log2 (1.0 + apartHz / a.hz)));
                }
            }
        }

        // NOISE: what lies between the harmonics, from a long look at the steady part.
        {
            constexpr int longOrder = 13, longSize = 1 << longOrder;
            juce::dsp::FFT longFft (longOrder);
            std::vector<float> spectrum ((size_t) longSize / 2, 0.0f), buffer;
            int looks = 0;
            for (auto t = steadyFrom; t * sampleRate + longSize < total && t < steadyTo && looks < 6; t += 0.1, ++looks)
            {
                buffer.assign ((size_t) longSize * 2, 0.0f);
                for (int i = 0; i < longSize; ++i)
                    buffer[(size_t) i] = x[(size_t) (t * sampleRate) + (size_t) i]
                                         * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) longSize));
                longFft.performFrequencyOnlyForwardTransform (buffer.data());
                for (int b = 0; b < longSize / 2; ++b)
                    spectrum[(size_t) b] += buffer[(size_t) b] * buffer[(size_t) b];
            }
            if (looks > 0)
            {
                const auto binHz = sampleRate / longSize;
                double harmonic = 0.0, between = 0.0, weighted = 0.0;
                for (int b = 1; b < longSize / 2; ++b)
                {
                    const auto f = b * binHz;
                    if (f < 0.5 * a.hz || f > 16000.0)
                        continue;
                    const auto k = std::round (f / a.hz);
                    const auto near = std::abs (f - k * a.hz) < juce::jmax (3.0 * binHz, 0.12 * a.hz);
                    (near ? harmonic : between) += spectrum[(size_t) b];
                    if (! near)
                        weighted += spectrum[(size_t) b] * f;
                }
                // Each harmonic's skirt (the window's leakage) is about this share of it: not noise.
                between = juce::jmax (0.0, between - 0.002 * harmonic);
                a.noise = (float) (between / juce::jmax (1.0e-20, harmonic + between));
                if (a.noise < 0.03f)
                    a.noise = 0.0f;
                else
                {
                    const auto centre = weighted / juce::jmax (1.0e-20, between);
                    a.noiseColour = (float) juce::jlimit (0.0, 1.0, 1.0 - std::log2 (centre / 300.0) / std::log2 (12000.0 / 300.0));
                }
            }
        }

        a.valid = true;
        return a;
    }

    Analysis analyseFile (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->sampleRate <= 0.0)
        {
            Analysis a;
            a.name = file.getFileNameWithoutExtension();
            a.problem = "That file couldn't be read as audio (WAV, AIFF, FLAC, OGG or MP3).";
            return a;
        }
        const auto length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (maxSeconds * reader->sampleRate));
        juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, reader->numChannels), juce::jmax (1, length));
        reader->read (&buffer, 0, length, 0, true, true);
        return analyse (buffer, reader->sampleRate, file.getFileNameWithoutExtension());
    }
}
