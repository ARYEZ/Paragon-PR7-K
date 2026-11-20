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

        // A SLOW MOVEMENT (1.8.0): the brightness (the spectrum's centre, in octaves) and the level every 50 ms between
        // `from` and `to` (seconds), and whether either goes round at a steady pace between 0.6 and 15 s a round: the
        // lag where the series best matches itself (normalised over the overlap), at least 0.3 alike, with at least
        // 2.2 rounds heard. The shortest lag nearly as alike as the best is taken (a round, not two), then made exact
        // from the furthest whole number of rounds that still match.
        struct Movement { double seconds = 0.0; float strength = 0.0f, octaves = 0.0f, db = 0.0f; };

        Movement findMovement (const std::vector<float>& x, double sampleRate, double from, double to)
        {
            Movement found;
            constexpr double step = 0.05;
            constexpr int order = 11, size = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> work ((size_t) size * 2), window ((size_t) size);
            for (int i = 0; i < size; ++i)
                window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
            std::vector<double> bright, level;
            for (auto t = from; (t + step) * sampleRate + size < (double) x.size() && t < to; t += step)
            {
                const auto at = (size_t) (t * sampleRate);
                std::fill (work.begin(), work.end(), 0.0f);
                for (int i = 0; i < size; ++i)
                    work[(size_t) i] = x[at + (size_t) i] * window[(size_t) i];
                fft.performFrequencyOnlyForwardTransform (work.data(), true);
                double power = 0.0, weighted = 0.0;
                for (int k = 1; k < size / 2; ++k)
                {
                    const auto p = (double) work[(size_t) k] * work[(size_t) k];
                    power += p;
                    weighted += p * k;
                }
                if (power < 1.0e-12)
                    return found;   // a gap: not one held sound
                bright.push_back (std::log2 (weighted / power * sampleRate / size));
                level.push_back (10.0 * std::log10 (power));
            }
            const auto n = (int) bright.size();
            if (n * step < 0.6 * 2.2)
                return found;

            // The lag (in steps) where a series best matches itself, its likeness, and the likeness at every lag.
            const auto likeness = [n] (std::vector<double> v)
            {
                const auto mean = std::accumulate (v.begin(), v.end(), 0.0) / n;
                for (auto& e : v)
                    e -= mean;
                std::vector<double> r ((size_t) n, 0.0);
                for (int lag = 1; lag < n; ++lag)
                {
                    double xy = 0.0, xx = 0.0, yy = 0.0;
                    for (int i = 0; i + lag < n; ++i)
                    {
                        xy += v[(size_t) i] * v[(size_t) (i + lag)];
                        xx += v[(size_t) i] * v[(size_t) i];
                        yy += v[(size_t) (i + lag)] * v[(size_t) (i + lag)];
                    }
                    r[(size_t) lag] = xx > 0.0 && yy > 0.0 ? xy / std::sqrt (xx * yy) : 0.0;
                }
                return r;
            };
            const auto shortest = (int) std::ceil (0.6 / step);
            const auto longest = juce::jmin ((int) (15.0 / step), (int) (n / 2.2));
            if (longest <= shortest + 2)
                return found;
            double bestR = 0.0;
            int bestLag = 0;
            std::vector<double> r;
            for (const auto* series : { &bright, &level })
            {
                const auto rs = likeness (*series);
                // Its peaks (local highs) in the range.
                double top = 0.0;
                for (int lag = shortest; lag <= longest; ++lag)
                    if (rs[(size_t) lag] > rs[(size_t) (lag - 1)] && rs[(size_t) lag] >= rs[(size_t) (lag + 1)])
                        top = juce::jmax (top, rs[(size_t) lag]);
                for (int lag = shortest; lag <= longest; ++lag)
                    if (rs[(size_t) lag] > rs[(size_t) (lag - 1)] && rs[(size_t) lag] >= rs[(size_t) (lag + 1)]
                        && rs[(size_t) lag] >= 0.85 * top)
                    {
                        if (rs[(size_t) lag] > bestR)
                        {
                            bestR = rs[(size_t) lag];
                            bestLag = lag;
                            r = rs;
                        }
                        break;   // the shortest of the nearly best
                    }
            }
            if (bestR < 0.3 || bestLag == 0)
                return found;
            // A fraction of a step (the peak's parabola), then from the furthest whole number of rounds still alike.
            const auto peakAt = [&r, n] (double near)
            {
                const auto reach = juce::jmax (2, (int) (near * 0.08));
                auto best = juce::roundToInt (near);
                for (int lag = juce::jmax (2, best - reach); lag <= juce::jmin (n - 2, best + reach); ++lag)
                    if (r[(size_t) lag] > r[(size_t) best])
                        best = lag;
                best = juce::jlimit (1, n - 2, best);
                const auto a = r[(size_t) (best - 1)], b = r[(size_t) best], c = r[(size_t) (best + 1)];
                const auto d = a - 2.0 * b + c;
                return std::make_pair (best + (d < -1.0e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / d) : 0.0), b);
            };
            auto lag = peakAt (bestLag).first;
            for (int k = 2; k * lag < n * 0.75; ++k)
            {
                const auto [at, likenessThere] = peakAt (k * lag);
                if (likenessThere < 0.25)
                    break;
                lag = at / k;
            }
            found.seconds = lag * step;
            found.strength = (float) bestR;

            // How far it swings: the series folded onto one round (its average at each point), peak to peak.
            const auto swing = [&] (const std::vector<double>& v)
            {
                constexpr int points = 24;
                std::vector<double> sum (points, 0.0), count (points, 0.0);
                for (int i = 0; i < n; ++i)
                {
                    const auto phase = std::fmod (i / lag, 1.0);
                    const auto p = juce::jmin (points - 1, (int) (phase * points));
                    sum[(size_t) p] += v[(size_t) i];
                    count[(size_t) p] += 1.0;
                }
                double low = 1.0e9, high = -1.0e9;
                for (int p = 0; p < points; ++p)
                    if (count[(size_t) p] > 0.0)
                    {
                        low = juce::jmin (low, sum[(size_t) p] / count[(size_t) p]);
                        high = juce::jmax (high, sum[(size_t) p] / count[(size_t) p]);
                    }
                return (float) juce::jmax (0.0, high - low);
            };
            found.octaves = swing (bright);
            found.db = swing (level);
            // Too small to matter (a hair of brightness, under a dB): not a movement.
            if (found.octaves < 0.1f && found.db < 1.0f)
                found = {};
            return found;
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

        // A SLOW MOVEMENT (1.8.0), in a held sound: if its tone goes round and round, its level's ups and downs are
        // part of that (they go in the frames), not a fall to a lower sustain. So the loudness is fitted again: the
        // attack's peak looked for in the first second only, the held level as the steady part's average power; a
        // peak no more than 2 dB over that is no peak (SUSTAIN 1, the AMP straight to its level).
        if (a.sustained)
        {
            const auto m = findMovement (x, sampleRate, a.start + 0.3, a.releaseStart);
            if (m.seconds > 0.0)
            {
                a.movementSeconds = m.seconds;
                a.movementStrength = m.strength;
                a.movementOctaves = m.octaves;
                a.movementDb = m.db;
                // The level smoothed over 100 ms (power): unison's shimmer makes single 10 ms readings jump a few dB
                // above the sound's real level.
                const auto smoothed = [&] (int h)
                {
                    double sum = 0.0;
                    int n = 0;
                    for (int k = juce::jmax (0, h - 5); k <= juce::jmin (hops - 1, h + 5); ++k, ++n)
                        sum += std::pow (10.0, rel[(size_t) k] / 10.0);
                    return (float) (10.0 * std::log10 (sum / juce::jmax (1, n) + 1.0e-14));
                };
                const auto firstHop = juce::jmax (startHop, (int) (a.start / Analysis::hopSeconds));
                int attackPeak = firstHop;
                for (int h = firstHop; h <= juce::jmin (releaseHop, firstHop + 100); ++h)
                    if (smoothed (h) > smoothed (attackPeak))
                        attackPeak = h;
                double power = 0.0;
                int counted = 0;
                for (int h = juce::jmin (releaseHop, firstHop + 50); h <= releaseHop; ++h, ++counted)
                    power += std::pow (10.0, rel[(size_t) h] / 10.0);
                const auto heldDb = (float) (10.0 * std::log10 (power / juce::jmax (1, counted) + 1.0e-14));
                // The attack stands out only above the movement's own loudest (half its swing over the average).
                const auto roundTop = heldDb + 0.5f * m.db;
                int settled = attackPeak;
                if (smoothed (attackPeak) > roundTop + 1.5f)
                {
                    while (settled < releaseHop && smoothed (settled) > roundTop)
                        ++settled;
                    a.sustain = std::pow (10.0f, (heldDb - smoothed (attackPeak)) / 20.0f);
                    a.decayMs = juce::jlimit (10.0f, 20000.0f, (float) ((settled - attackPeak) * Analysis::hopSeconds / 0.23 * 1000.0));
                }
                else
                {
                    a.sustain = 1.0f;
                    a.decayMs = 50.0f;
                }
                a.settle = juce::jmax (a.start + 0.05, (settled + 0.5) * Analysis::hopSeconds);
            }
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
        // One frame from a few cycles at each of `times` (seconds), every cycle turned so its fundamental starts at
        // the same point; each harmonic keeps its full level (the cycles' power) at the averaged phase. `rms` is its
        // level as heard (before it's scaled).
        const auto frameAt = [&] (const std::vector<double>& times, double& rms)
        {
            const auto count = [&] (double t) { return juce::jmax (1, juce::jmin (frameSize / 2 - 1, (int) (sampleRate / hzAt (t) / 2.0 * 0.95))); };
            const auto most = count (times.front());
            std::vector<Complex> sum ((size_t) most + 1);
            std::vector<float> power ((size_t) most + 1, 0.0f);
            int used = 0;
            for (auto t : times)
            {
                const auto localHz = hzAt (t);
                const auto period = sampleRate / localHz;
                const auto cycles = juce::jlimit (2, 6, juce::roundToInt (0.03 * localHz));
                t = juce::jmin (t, (total - 2 - cycles * period) / sampleRate);
                const auto upTo = juce::jmin (most, count (t));
                for (int c = 0; c < cycles; ++c, ++used)
                {
                    auto h = cycleHarmonics (x, juce::jmax (0.0, t * sampleRate) + c * period, period, upTo, fft, fftScratch);
                    const auto phase = std::arg (h[1]);
                    for (int k = 1; k <= upTo; ++k)
                    {
                        h[(size_t) k] *= std::polar (1.0f, -phase * (float) k);
                        sum[(size_t) k] += h[(size_t) k];
                        power[(size_t) k] += std::norm (h[(size_t) k]);
                    }
                }
            }
            std::vector<Complex> h ((size_t) most + 1);
            for (int k = 1; k <= most; ++k)
            {
                const auto level = std::sqrt (power[(size_t) k] / (float) juce::jmax (1, used));
                const auto direction = std::abs (sum[(size_t) k]) > 1.0e-12f ? sum[(size_t) k] / std::abs (sum[(size_t) k]) : Complex (1.0f, 0.0f);
                h[(size_t) k] = level * direction;
            }
            auto cycle = cycleFrom (h, fft, fftScratch);
            rms = 0.0;
            for (const auto v : cycle)
                rms += (double) v * v;
            rms = std::sqrt (rms / frameSize);
            return cycle;
        };
        auto samples = std::make_shared<std::vector<float>>();
        const auto addFrame = [&samples] (const std::vector<float>& cycle, double scale)
        {
            for (const auto v : cycle)
                samples->push_back ((float) (v * scale));
        };
        if (a.movementSeconds > 0.0)
        {
            // The attack's frames, then one round of the movement: each of its frames the average of every round heard
            // at that point (what's typical of the round, not one round's chance), at its level against the round's
            // average (the movement's ups and downs in level), and the round's first frame again at the end.
            const auto attackSeconds = a.settle - a.start;
            a.attackFrames = juce::jlimit (0, 16, juce::roundToInt (attackSeconds / frameSpacing));
            const auto roundFrames = juce::jlimit (8, maxFrames - a.attackFrames - 1, juce::roundToInt (a.movementSeconds / 0.06));
            samples->reserve ((size_t) (a.attackFrames + roundFrames + 1) * frameSize);
            for (int f = 0; f < a.attackFrames; ++f)
            {
                double rms = 0.0;
                const auto cycle = frameAt ({ a.start + attackSeconds * f / a.attackFrames }, rms);
                addFrame (cycle, rms > 1.0e-9 ? 0.25 / rms : 0.0);
            }
            std::vector<std::vector<float>> round;
            std::vector<double> levels;
            for (int k = 0; k < roundFrames; ++k)
            {
                std::vector<double> times;
                for (auto t = a.settle + a.movementSeconds * k / roundFrames; t + 0.05 < a.releaseStart; t += a.movementSeconds)
                    times.push_back (t);
                if (times.empty())
                    times.push_back (a.settle);
                double rms = 0.0;
                round.push_back (frameAt (times, rms));
                levels.push_back (rms);
            }
            double meanPower = 0.0;
            for (const auto l : levels)
                meanPower += l * l / roundFrames;
            const auto mean = std::sqrt (meanPower);
            for (int k = 0; k <= roundFrames; ++k)
            {
                const auto i = (size_t) (k % roundFrames);
                const auto against = mean > 1.0e-9 ? juce::jlimit (0.35, 2.8, levels[i] / mean) : 1.0;
                addFrame (round[i], levels[i] > 1.0e-9 ? 0.25 / levels[i] * against : 0.0);
            }
            a.travelSeconds = attackSeconds;
        }
        else
        {
            a.travelSeconds = juce::jmax (0.0, (toneEndHop + 0.5) * Analysis::hopSeconds - a.start);
            const auto frames = juce::jlimit (1, maxFrames, juce::roundToInt (a.travelSeconds / frameSpacing));
            samples->reserve ((size_t) frames * frameSize);
            for (int f = 0; f < frames; ++f)
            {
                const auto t = frames == 1 ? a.start + juce::jmin (0.1, 0.5 * (steadyFrom + steadyTo) - a.start)
                                           : a.start + a.travelSeconds * f / (frames - 1);
                double rms = 0.0;
                const auto cycle = frameAt ({ t }, rms);
                addFrame (cycle, rms > 1.0e-9 ? 0.25 / rms : 0.0);
            }
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

        a.audio = std::make_shared<const std::vector<float>> (std::move (x));
        a.valid = true;
        return a;
    }

    //==================================================================================================================
    Fingerprint fingerprint (const std::vector<float>& audio, double sampleRate, double start, double seconds)
    {
        Fingerprint f;
        const auto first = (size_t) juce::jmax (0.0, start * sampleRate);
        const auto last = seconds > 0.0 ? juce::jmin (audio.size(), first + (size_t) (seconds * sampleRate)) : audio.size();
        if (first >= last)
            return f;
        // 40 ms windows (Hann), every 10 ms, an FFT a power of two long enough.
        const auto window = juce::jmax (64, (int) (0.04 * sampleRate));
        auto order = 6;
        while ((1 << order) < window)
            ++order;
        const auto size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> buffer ((size_t) size * 2);
        // The bands' edges, log-spaced 60 Hz to 16 kHz, as FFT bins.
        std::array<int, Fingerprint::numBands + 1> edges {};
        for (int b = 0; b <= Fingerprint::numBands; ++b)
        {
            const auto hz = 60.0 * std::pow (16000.0 / 60.0, (double) b / Fingerprint::numBands);
            edges[(size_t) b] = juce::jlimit (1, size / 2 - 1, (int) std::round (hz / sampleRate * size));
        }
        const auto hop = juce::jmax (1, (int) (Analysis::hopSeconds * sampleRate));
        for (auto at = first; at + (size_t) hop <= last; at += (size_t) hop)
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            double power = 0.0;
            for (int i = 0; i < window; ++i)
            {
                const auto index = at + (size_t) i - (size_t) window / 2 + (size_t) hop / 2;
                const auto s = index < last && index >= first ? audio[index] : 0.0f;
                power += (double) s * s;
                buffer[(size_t) i] = s * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) window));
            }
            f.levels.push_back ((float) juce::jmax (-120.0, 10.0 * std::log10 (power / window + 1.0e-14) + 3.01));
            fft.performFrequencyOnlyForwardTransform (buffer.data());
            std::array<float, Fingerprint::numBands> bands {};
            for (int b = 0; b < Fingerprint::numBands; ++b)
            {
                double sum = 0.0;
                for (int k = edges[(size_t) b]; k <= juce::jmax (edges[(size_t) b], edges[(size_t) b + 1] - 1); ++k)
                    sum += (double) buffer[(size_t) k] * buffer[(size_t) k];
                bands[(size_t) b] = (float) juce::jmax (-140.0, 10.0 * std::log10 (sum + 1.0e-14));
            }
            f.bands.push_back (bands);
        }
        return f;
    }

    Score compare (const Fingerprint& recording, const Fingerprint& candidate)
    {
        Score s;
        const auto frames = juce::jmin (recording.levels.size(), candidate.levels.size());
        if (frames == 0)
            return s;
        const auto peak = *std::max_element (recording.levels.begin(), recording.levels.begin() + (std::ptrdiff_t) frames);
        // The volume: the difference in level, weighted by how loud the recording is there.
        double weighted = 0.0, weights = 0.0;
        for (size_t i = 0; i < frames; ++i)
            if (recording.levels[i] > peak - 40.0f && candidate.levels[i] > -110.0f)
            {
                const auto w = std::pow (10.0, (recording.levels[i] - peak) / 20.0);
                weighted += w * (recording.levels[i] - candidate.levels[i]);
                weights += w;
            }
        s.gainDb = weights > 0.0 ? weighted / weights : 0.0;
        // The loudness, moment by moment (each side no lower than 60 dB under the loudest: below that is silence).
        double loudness = 0.0, tone = 0.0;
        int loudFrames = 0, toneFrames = 0;
        s.levels.resize (frames);
        for (size_t i = 0; i < frames; ++i)
        {
            const auto r = juce::jmax (recording.levels[i], peak - 60.0f);
            const auto c = juce::jmax ((float) (candidate.levels[i] + s.gainDb), peak - 60.0f);
            s.levels[i] = c - peak;
            if (recording.levels[i] > peak - 60.0f || candidate.levels[i] + s.gainDb > peak - 60.0f)
            {
                loudness += std::abs (r - c);
                ++loudFrames;
            }
            // The tone where the recording is clearly sounding: each spectrum as a shape (its own level out), bands
            // within 40 dB of that moment's loudest band in either.
            if (recording.levels[i] > peak - 30.0f)
            {
                const auto& rb = recording.bands[i];
                const auto& cb = candidate.bands[i];
                const auto rTop = *std::max_element (rb.begin(), rb.end()), cTop = *std::max_element (cb.begin(), cb.end());
                double sum = 0.0;
                int used = 0;
                for (int b = 0; b < Fingerprint::numBands; ++b)
                {
                    const auto rv = juce::jmax (rb[(size_t) b] - rTop, -40.0f), cv = juce::jmax (cb[(size_t) b] - cTop, -40.0f);
                    if (rv > -40.0f || cv > -40.0f)
                    {
                        sum += std::abs (rv - cv);
                        ++used;
                    }
                }
                if (used > 0)
                {
                    tone += sum / used;
                    ++toneFrames;
                }
            }
        }
        s.loudnessDb = loudFrames > 0 ? loudness / loudFrames : 0.0;
        s.toneDb = toneFrames > 0 ? tone / toneFrames : 0.0;
        s.match = 100.0 * std::exp (-0.5 * (s.loudnessDb + s.toneDb) / 10.0);
        return s;
    }

    //==================================================================================================================
    // PICK A NOTE (1.5.0).
    Phrase findNotes (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name)
    {
        Phrase p;
        p.name = name;
        p.sampleRate = sampleRate;
        const auto total = juce::jmin (audio.getNumSamples(), (int) (maxPhraseSeconds * sampleRate));
        const auto channels = audio.getNumChannels();
        p.seconds = total / sampleRate;
        {
            auto copy = std::make_shared<juce::AudioBuffer<float>> (juce::jmax (1, channels), juce::jmax (1, total));
            copy->clear();
            for (int c = 0; c < channels; ++c)
                copy->copyFrom (c, 0, audio, c, 0, total);
            p.audio = copy;
        }
        if (channels == 0 || total < (int) (0.05 * sampleRate))
        {
            p.problem = "The recording is too short (it needs at least a twentieth of a second).";
            return p;
        }
        std::vector<float> x ((size_t) total);
        for (int i = 0; i < total; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c)
                sum += audio.getSample (c, i);
            x[(size_t) i] = sum / (float) channels;
        }

        // The level every 10 ms (20 ms windows), dB from the loudest.
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
            db[(size_t) h] = (float) juce::jmax (-120.0, 10.0 * std::log10 (power + 1.0e-14) + 3.01);
        }
        const auto peakDb = hops > 0 ? *std::max_element (db.begin(), db.end()) : -120.0f;
        if (peakDb < -60.0f)
        {
            p.problem = "The recording is too quiet (its loudest moment is under -60 dB).";
            return p;
        }
        p.levels.resize ((size_t) hops);
        for (int h = 0; h < hops; ++h)
            p.levels[(size_t) h] = db[(size_t) h] - peakDb;
        const auto& rel = p.levels;

        // The pitch every 10 ms (YIN at a reduced rate, 25 Hz to 2 kHz), as a fractional note; NaN where unclear.
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
        const auto window = juce::jmax (minLag * 2, maxLag / 2);   // 20 ms: short enough to follow fast notes
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        p.pitches.assign ((size_t) hops, nan);
        std::vector<float> clarity ((size_t) hops, 0.0f), scratch;
        for (int h = 0; h < hops; ++h)
        {
            if (rel[(size_t) h] < -45.0f)
                continue;
            const auto begin = juce::jmax (0, h * hop / decimation - window / 2);
            if (begin + window + maxLag + 2 >= (int) reduced.size())
                continue;
            float c = 0.0f;
            const auto lag = yinLag (reduced.data() + begin, window, minLag, maxLag, c, scratch);
            clarity[(size_t) h] = c;
            if (c >= 0.7f && lag > 0.0)
                p.pitches[(size_t) h] = (float) (69.0 + 12.0 * std::log2 (reducedRate / lag / 440.0));
        }
        // Single readings an octave off their neighbours (YIN's usual slip) take their neighbours' pitch.
        for (int h = 1; h + 1 < hops; ++h)
        {
            const auto a = p.pitches[(size_t) h - 1], b = p.pitches[(size_t) h], c = p.pitches[(size_t) h + 1];
            if (! std::isnan (a) && ! std::isnan (b) && ! std::isnan (c) && std::abs (a - c) < 1.0f && std::abs (b - 0.5f * (a + c)) > 9.0f)
                p.pitches[(size_t) h] = 0.5f * (a + c);
        }

        // Cutting it into notes.
        const auto voiced = [&] (int h) { return h >= 0 && h < hops && ! std::isnan (p.pitches[(size_t) h]) && rel[(size_t) h] > -40.0f; };
        std::vector<int> starts;   // hop where each note starts
        {
            int h = 0;
            auto inNote = false;
            float noteMiddle = 0.0f;
            std::vector<float> notePitches;
            int away = 0, silent = 0;
            for (; h < hops; ++h)
            {
                if (! voiced (h))
                {
                    if (inNote && ++silent >= 3)
                        inNote = false;
                    continue;
                }
                silent = 0;
                const auto pitch = p.pitches[(size_t) h];
                if (! inNote)
                {
                    // The note starts at its attack: back up while the level is still rising out of the silence.
                    auto s = h;
                    while (s > 0 && rel[(size_t) s - 1] < rel[(size_t) s] && rel[(size_t) s - 1] > -60.0f && h - s < 8
                           && (starts.empty() || s - 1 > starts.back()))
                        --s;
                    starts.push_back (s);
                    inNote = true;
                    notePitches.assign (1, pitch);
                    noteMiddle = pitch;
                    away = 0;
                    continue;
                }
                // A fresh attack: the level jumps 6 dB over the lowest of the last 30 ms (at least 60 ms into the note).
                const auto lowest = juce::jmin (rel[(size_t) juce::jmax (0, h - 1)], rel[(size_t) juce::jmax (0, h - 2)], rel[(size_t) juce::jmax (0, h - 3)]);
                if (h - starts.back() >= 6 && rel[(size_t) h] - lowest > 6.0f && rel[(size_t) h] > -30.0f)
                {
                    starts.push_back (h - 1);
                    notePitches.assign (1, pitch);
                    noteMiddle = pitch;
                    away = 0;
                    continue;
                }
                // Another note: the pitch jumps (0.4 of a semitone within 30 ms) to 0.6 of a semitone or more from this
                // note's middle and stays there for 40 ms. Moving there smoothly is a slide inside the note (its middle
                // follows it, so a jump from where it slid to is judged from there).
                if (std::abs (pitch - noteMiddle) > 0.6f)
                {
                    if (away == 0)
                    {
                        const auto before = p.pitches[(size_t) juce::jmax (starts.back(), h - 3)];
                        if (before > 0.0f && std::abs (pitch - before) < 0.4f)
                        {
                            notePitches.push_back (pitch);
                            noteMiddle = pitch;
                            continue;
                        }
                    }
                    if (++away >= 4)
                    {
                        starts.push_back (h - 3);
                        notePitches.assign (1, pitch);
                        noteMiddle = pitch;
                        away = 0;
                    }
                    continue;
                }
                away = 0;
                notePitches.push_back (pitch);
                if (notePitches.size() <= 30)   // the middle from its first 300 ms (a slide later doesn't move it)
                    noteMiddle = median (notePitches);
            }
        }

        for (size_t n = 0; n < starts.size(); ++n)
        {
            FoundNote f;
            const auto s = starts[n];
            const auto next = n + 1 < starts.size() ? starts[n + 1] : hops;
            // Its own pitches and loudest.
            std::vector<float> pitches, clear;
            float loudest = -120.0f;
            int lastVoiced = s;
            for (int h = s; h < next; ++h)
            {
                loudest = juce::jmax (loudest, rel[(size_t) h]);
                if (voiced (h))
                {
                    pitches.push_back (p.pitches[(size_t) h]);
                    clear.push_back (clarity[(size_t) h]);
                    lastVoiced = h;
                }
            }
            if (pitches.size() < 8)   // under 80 ms of pitch: a click, a grace note
                continue;
            // Where it has died away (40 dB under its loudest), or the next note.
            int endHop = lastVoiced;
            while (endHop + 1 < next && rel[(size_t) endHop + 1] > loudest - 40.0f)
                ++endHop;
            f.ringsOut = endHop + 1 < next || next == hops;
            const auto middle = median (pitches);
            f.note = juce::jlimit (0, 127, juce::roundToInt (middle));
            f.cents = (middle - (float) f.note) * 100.0f;
            f.start = s * Analysis::hopSeconds;
            f.voicedEnd = (lastVoiced + 1) * Analysis::hopSeconds;
            f.end = juce::jmin (p.seconds, (endHop + 1) * Analysis::hopSeconds);
            f.levelDb = loudest;
            f.clarity = median (clear);
            std::vector<float> strays;
            for (const auto pitch : pitches)
                strays.push_back (std::abs (pitch - middle) * 100.0f);
            f.wobbleCents = median (strays);
            // The score: length (to 2 s), clarity, steadiness, loudness, a clean release.
            const auto length = juce::jlimit (0.0, 1.0, (f.voicedEnd - f.start) / 2.0);
            const auto clean = juce::jlimit (0.0f, 1.0f, (f.clarity - 0.7f) / 0.25f);
            const auto steady = 1.0f / (1.0f + f.wobbleCents / 40.0f);
            const auto loud = juce::jlimit (0.0f, 1.0f, (f.levelDb + 30.0f) / 24.0f);
            f.score = (float) (0.15 + 0.85 * length) * (0.3f + 0.7f * clean) * (0.5f + 0.5f * steady) * (0.4f + 0.6f * loud)
                    * (f.ringsOut ? 1.0f : 0.75f);
            p.notes.push_back (f);
        }
        if (p.notes.empty())
        {
            p.problem = "No notes found: MIMIC needs pitched notes (a drum loop, a noise or a dense chord has none to follow).";
            return p;
        }
        for (size_t n = 0; n < p.notes.size(); ++n)
            if (p.best < 0 || p.notes[n].score > p.notes[(size_t) p.best].score)
                p.best = (int) n;
        p.valid = true;
        return p;
    }

    Phrase findNotesInFile (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->sampleRate <= 0.0)
        {
            Phrase p;
            p.name = file.getFileNameWithoutExtension();
            p.problem = "That file couldn't be read as audio (WAV, AIFF, FLAC, OGG or MP3).";
            return p;
        }
        const auto length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (maxPhraseSeconds * reader->sampleRate));
        juce::AudioBuffer<float> buffer ((int) juce::jlimit (1u, 2u, reader->numChannels), juce::jmax (1, length));
        reader->read (&buffer, 0, length, 0, true, buffer.getNumChannels() > 1);
        return findNotes (buffer, reader->sampleRate, file.getFileNameWithoutExtension());
    }

    juce::AudioBuffer<float> cutNote (const Phrase& p, const FoundNote& f)
    {
        if (p.audio == nullptr)
            return {};
        const auto& a = *p.audio;
        const auto from = juce::jmax (0, (int) ((f.start - 0.005) * p.sampleRate));
        const auto to = juce::jmin (a.getNumSamples(), (int) (f.end * p.sampleRate));
        juce::AudioBuffer<float> out (a.getNumChannels(), juce::jmax (1, to - from));
        out.clear();
        for (int c = 0; c < a.getNumChannels(); ++c)
            if (to > from)
                out.copyFrom (c, 0, a, c, from, to - from);
        // A 5 ms fade at the cut's end, so a note cut short by the next doesn't end in a click.
        const auto fade = juce::jmin (out.getNumSamples(), (int) (0.005 * p.sampleRate));
        if (! f.ringsOut && fade > 1)
            out.applyGainRamp (out.getNumSamples() - fade, fade, 1.0f, 0.0f);
        return out;
    }

    Analysis analyseNote (const Phrase& p, int index)
    {
        if (index < 0 || index >= (int) p.notes.size())
            return {};
        const auto& f = p.notes[(size_t) index];
        int same = 0;   // which of its pitch it is, for the name
        for (int k = 0; k <= index; ++k)
            if (p.notes[(size_t) k].note == f.note)
                ++same;
        auto a = analyse (cutNote (p, f), p.sampleRate, p.name + " - " + noteName (f.note) + (same > 1 ? " (" + juce::String (same) + ")" : juce::String()));
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
