#include "Remnant.h"

#include <cmath>

namespace effects
{
    void Remnant::prepare (double rate, int)
    {
        sampleRate = rate;
        // A head falls (1 - speed) behind each sample for LENGTH: at most half of 4 s; 3 s of line is plenty.
        lineSize = (int) std::ceil (3.0 * rate) + 8;
        for (auto& layer : layers)
            for (auto& l : layer.line)
                l.assign ((size_t) lineSize, 0.0f);
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));   // 30 ms
        reset();
    }

    void Remnant::clearLayers() noexcept
    {
        for (auto& layer : layers)
        {
            for (auto& l : layer.line)
                std::fill (l.begin(), l.end(), 0.0f);
            layer.toneState = {};
            layer.power = 0.0f;
        }
        headLength = 0;   // the heads start afresh (from the present) when the layers wake
    }

    void Remnant::reset() noexcept
    {
        clearLayers();
        writeIndex = 0;
        mixNow = 0.0f;
        for (int k = 0; k < maxLayers; ++k)
        {
            layerShown[(size_t) k].store (-100.0f, std::memory_order_relaxed);
            phaseShown[(size_t) k].store (0.0f, std::memory_order_relaxed);
        }
        totalShown.store (-100.0f, std::memory_order_relaxed);
    }

    // `back` samples before the one just written, by four-point Hermite; points not written yet repeat the newest.
    float Remnant::read (const std::vector<float>& line, double back) const noexcept
    {
        const auto whole = (int) std::ceil (back);
        const auto f = (float) ((double) whole - back);
        const auto at = [&] (int k) { return line[(size_t) ((writeIndex - juce::jmax (0, k) + lineSize) % lineSize)]; };
        const auto xm1 = at (whole + 1), x0 = at (whole), x1 = at (whole - 1), x2 = at (whole - 2);
        if (f == 0.0f)
            return x0;
        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    // Where a fresh head should start: somewhere in the last 20 ms (and far enough back that the stretch compared is
    // all written), the place whose next few milliseconds line up best with what the other head is about to play.
    double Remnant::alignedStart (const Layer& layer, double otherLag, double speed) const noexcept
    {
        constexpr int compared = 256, every = 4;
        const auto earliest = speed * compared + 2.0;
        const auto span = 0.02 * sampleRate;
        const auto mono = [&] (double back) { return read (layer.line[0], back) + read (layer.line[1], back); };
        std::array<float, compared / every> other {};
        for (int j = 0; j < compared / every; ++j)
            other[(size_t) j] = mono (juce::jmax (0.0, otherLag - speed * j * every));
        auto best = earliest, bestScore = -1.0e30;
        for (auto d = earliest; d < earliest + span; d += 3.0)
        {
            double dot = 0.0, energy = 1.0e-12;
            for (int j = 0; j < compared / every; ++j)
            {
                const auto v = (double) mono (d - speed * j * every);
                dot += v * other[(size_t) j];
                energy += v * v;
            }
            const auto score = dot / std::sqrt (energy);
            if (score > bestScore)
            {
                bestScore = score;
                best = d;
            }
        }
        return best;
    }

    void Remnant::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        // MIX under a thousandth counts as 0 (the mod matrix eases a knob down and never lands exactly on 0).
        const auto mixTarget = s.on && s.mix >= 1.0e-3f ? juce::jmin (1.0f, s.mix) : 0.0f;

        if (mixTarget <= 0.0f && isResting())
        {
            // Untouched. Layer 1 still records, so the layers wake on what was just played.
            for (int i = 0; i < n; ++i)
            {
                layers[0].line[0][(size_t) writeIndex] = left[i];
                layers[0].line[1][(size_t) writeIndex] = right[i];
                writeIndex = writeIndex + 1 == lineSize ? 0 : writeIndex + 1;
            }
            for (int k = 0; k < maxLayers; ++k)
                layerShown[(size_t) k].store (-100.0f, std::memory_order_relaxed);
            totalShown.store (-100.0f, std::memory_order_relaxed);
            return;
        }

        const auto active = juce::jlimit (1, maxLayers, s.layers + 1);
        const auto speed = (double) speedFor (s.step);
        const auto length = juce::jmax (64, (int) (lengthSeconds (s.length) * sampleRate));
        if (headLength == 0)
        {
            // Waking: the two heads of every layer start at the present, half a LENGTH apart.
            for (auto& layer : layers)
            {
                layer.heads[0] = { speed * 256.0 + 2.0, 0 };
                layer.heads[1] = { speed * 256.0 + 2.0 + (1.0 - speed) * (length / 2), length / 2 };
            }
        }
        headLength = length;
        const auto taper = juce::Decibels::decibelsToGain (taperDb (s.taper));
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        std::array<float, maxLayers> coefficient {}, gain {}, gainLeft {}, gainRight {};
        for (int k = 0; k < maxLayers; ++k)
        {
            const auto hz = toneHz (s.tone) * std::pow (0.7f, (float) k);
            coefficient[(size_t) k] = hz >= 15000.0f ? 1.0f : 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
            gain[(size_t) k] = std::pow (taper, (float) k);
            // WIDTH: the layers lean to alternate sides (1 left, 2 right, 3 left), each side kept its own.
            const auto lean = (k % 2 == 0 ? -0.6f : 0.6f) * width;
            gainLeft[(size_t) k] = std::sqrt (1.0f - lean);
            gainRight[(size_t) k] = std::sqrt (1.0f + lean);
        }
        auto total = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            mixNow += (mixTarget - mixNow) * smoothing;
            if (mixTarget <= 0.0f && mixNow < 1.0e-5f)
                mixNow = 0.0f;

            // Each layer hears the one above it (layer 1 your sound), then plays it back slower.
            std::array<float, 2> heard { left[i], right[i] }, sum {};
            for (int k = 0; k < active; ++k)
            {
                auto& layer = layers[(size_t) k];
                layer.line[0][(size_t) writeIndex] = heard[0];
                layer.line[1][(size_t) writeIndex] = heard[1];
                std::array<float, 2> out {};
                for (int h = 0; h < 2; ++h)
                {
                    auto& head = layer.heads[(size_t) h];
                    const auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) head.age / (float) length);
                    out[0] += w * read (layer.line[0], head.lag);
                    out[1] += w * read (layer.line[1], head.lag);
                    head.lag += 1.0 - speed;
                    if (++head.age >= length)   // a fresh stretch from the present, in step with the other head
                        head = { alignedStart (layer, layer.heads[(size_t) (1 - h)].lag, speed), 0 };
                }
                for (int side = 0; side < 2; ++side)
                {
                    auto& state = layer.toneState[(size_t) side];
                    state += coefficient[(size_t) k] * (out[(size_t) side] - state);
                    heard[(size_t) side] = state;   // the next layer dreams this one
                }
                const auto l = layer.toneState[0] * gain[(size_t) k] * gainLeft[(size_t) k];
                const auto r = layer.toneState[1] * gain[(size_t) k] * gainRight[(size_t) k];
                sum[0] += l;
                sum[1] += r;
                layer.power += 0.5f * (l * l + r * r);
            }
            // The layers below the ones in use are silent: their lines empty.
            for (int k = active; k < maxLayers; ++k)
            {
                layers[(size_t) k].line[0][(size_t) writeIndex] = 0.0f;
                layers[(size_t) k].line[1][(size_t) writeIndex] = 0.0f;
            }
            writeIndex = writeIndex + 1 == lineSize ? 0 : writeIndex + 1;

            left[i] += mixNow * sum[0];
            if (stereo)
                right[i] += mixNow * sum[1];
            total += mixNow * mixNow * 0.5f * (sum[0] * sum[0] + sum[1] * sum[1]);
        }

        for (int k = 0; k < maxLayers; ++k)
        {
            auto& layer = layers[(size_t) k];
            const auto p = k < active ? layer.power / (float) juce::jmax (1, n) : 0.0f;
            layerShown[(size_t) k].store (p > 1.0e-12f ? 10.0f * std::log10 (p) : -100.0f, std::memory_order_relaxed);
            phaseShown[(size_t) k].store ((float) layer.heads[0].age / (float) length, std::memory_order_relaxed);
            layer.power = 0.0f;
        }
        const auto t = total / (float) juce::jmax (1, n);
        totalShown.store (t > 1.0e-12f ? 10.0f * std::log10 (t) : -100.0f, std::memory_order_relaxed);
        if (mixTarget <= 0.0f && mixNow <= 0.0f)
            clearLayers();   // turned down: nothing kept but layer 1's recording
    }
}
