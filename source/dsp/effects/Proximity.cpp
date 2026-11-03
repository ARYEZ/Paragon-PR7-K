#include "Proximity.h"

#include <cmath>

namespace effects
{
    void Proximity::prepare (double rate, int)
    {
        sampleRate = rate;
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * rate)));      // 30 ms
        followUp = (float) (1.0 - std::exp (-1.0 / (0.01 * rate)));       // 10 ms
        followDown = (float) (1.0 - std::exp (-1.0 / (0.6 * rate)));      // the room fades within a few seconds
        reset();
    }

    void Proximity::reset() noexcept
    {
        for (auto& h : hits)
            h = {};
        numPending = 0;
        noiseNow = roomNow = closeNow = 0.0f;
        follow = 0.0f;
        roomState = {};
        humPhase = 0.0;
        x1 = x2 = y1 = y2 = {};
        shelfRunning = false;
        noiseShown.store (-100.0f, std::memory_order_relaxed);
        roomShown.store (-100.0f, std::memory_order_relaxed);
        closeShown.store (0.0f, std::memory_order_relaxed);
    }

    bool Proximity::isResting() const noexcept
    {
        if (noiseNow > 0.0f || roomNow > 0.0f || closeNow > 0.0f || shelfRunning)
            return false;
        for (const auto& h : hits)
            if (h.active)
                return false;
        return true;
    }

    void Proximity::addTouch (int sample, float note, float velocity, Kind kind) noexcept
    {
        if (numPending < (int) pending.size())
            pending[(size_t) numPending++] = { juce::jmax (0, sample), note, juce::jlimit (0.0f, 1.0f, velocity), kind };
        const auto n = touchCount.load (std::memory_order_relaxed);
        seen[(size_t) (n % 16)].store ((juce::jlimit (0, 127, juce::roundToInt (note)) << 16) | (juce::jlimit (0, 127, juce::roundToInt (velocity * 127.0f)) << 8)
                                           | (int) kind,
                                       std::memory_order_relaxed);
        touchCount.store (n + 1, std::memory_order_relaxed);
    }

    Proximity::Seen Proximity::getTouch (int ago) const noexcept
    {
        const auto n = touchCount.load (std::memory_order_relaxed);
        if (ago < 0 || ago >= juce::jmin (16, n))
            return { 60.0f, 0.0f, Kind::press };
        const auto code = seen[(size_t) ((n - 1 - ago) % 16)].load (std::memory_order_relaxed);
        return { (float) ((code >> 16) & 127), (float) ((code >> 8) & 127) / 127.0f, (Kind) (code & 3) };
    }

    Proximity::Hit& Proximity::freeHit() noexcept
    {
        Hit* oldest = &hits[0];
        for (auto& h : hits)
        {
            if (! h.active)
                return h;
            if (h.age > oldest->age)
                oldest = &h;
        }
        return *oldest;   // all busy (a fast run): the oldest, nearly faded, gives way
    }

    void Proximity::setBand (Hit& h, float hz, float k) const noexcept
    {
        h.g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, 0.45f * (float) sampleRate, hz) / (float) sampleRate);
        h.k = k;
        h.ic1 = h.ic2 = 0.0f;
    }

    void Proximity::spawn (const Pending& p, const Settings& s) noexcept
    {
        const auto perMs = [this] (float ms) { return (float) std::exp (-1.0 / (0.001 * ms * sampleRate)); };
        const auto tone = toneHz (s.tone);
        // Each touch takes NOISE's level as it happens (so turning NOISE down never cuts off a thud already ringing).
        const auto loud = std::pow (juce::jmax (0.05f, p.velocity), 1.5f) * juce::Decibels::decibelsToGain (noiseDb (s.noise));
        const auto up = juce::jlimit (0.0f, 1.0f, (p.note - 21.0f) / 87.0f);   // A0 to C8
        // Placed where the note is on the keyboard (low left, high right); the pedal in the middle.
        const auto keyed = p.kind == Kind::press || p.kind == Kind::release;
        const auto pan = keyed ? juce::jlimit (-1.0f, 1.0f, (p.note - 60.0f) / 30.0f) * 0.8f * juce::jlimit (0.0f, 1.0f, s.width) : 0.0f;
        const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const auto make = [&] () -> Hit&
        {
            auto& h = freeHit();
            h = {};
            h.active = true;
            h.delay = p.sample;
            h.gainLeft = std::cos (angle) * juce::MathConstants<float>::sqrt2;
            h.gainRight = std::sin (angle) * juce::MathConstants<float>::sqrt2;
            return h;
        };
        const auto level = juce::Decibels::decibelsToGain (noiseDb (s.noise));
        switch (p.kind)
        {
            case Kind::press:
            {
                // The hammer: a felt thud, falling in pitch, lighter and shorter up the keyboard, and a soft burst.
                auto& h = make();
                const auto base = 55.0f + 125.0f * up;
                h.freq = base * 1.6f;
                h.freqEnd = base;
                h.glide = 1.0f - perMs (6.0f);
                h.toneAmp = 0.5f * loud;
                h.toneEnv = 1.0f;
                h.toneDecay = perMs (18.0f - 10.0f * up);
                h.noiseAmp = 0.35f * loud;
                h.noiseEnv = 1.0f;
                h.noiseDecay = perMs (4.0f);
                h.attack = (int) (0.0003 * sampleRate);
                setBand (h, tone * 0.6f, 1.6f);
                break;
            }
            case Kind::release:
            {
                // The key coming back up: a small knock and a click...
                auto& key = make();
                key.freq = 220.0f;
                key.freqEnd = 180.0f;
                key.glide = 1.0f - perMs (4.0f);
                key.toneAmp = 0.08f * (0.5f + 0.5f * p.velocity) * level;
                key.toneEnv = 1.0f;
                key.toneDecay = perMs (6.0f);
                key.noiseAmp = 0.12f * level;
                key.noiseEnv = 1.0f;
                key.noiseDecay = perMs (2.5f);
                setBand (key, tone * 1.4f, 1.0f);
                // ...and the damper settling on the string a moment later: a short, muffled hush near the note.
                auto& damper = make();
                damper.delay = p.sample + (int) (0.004 * sampleRate);
                damper.noiseAmp = 0.06f * level;
                damper.noiseEnv = 1.0f;
                damper.noiseDecay = perMs (45.0f);
                damper.attack = (int) (0.005 * sampleRate);
                setBand (damper, 350.0f + 900.0f * up, 2.0f);
                break;
            }
            case Kind::pedalDown:
            {
                // The pedal going down: a low wooden thud and the mechanism's clack.
                auto& h = make();
                h.freq = 48.0f;
                h.freqEnd = 40.0f;
                h.glide = 1.0f - perMs (10.0f);
                h.toneAmp = 0.35f * level;
                h.toneEnv = 1.0f;
                h.toneDecay = perMs (30.0f);
                h.noiseAmp = 0.08f * level;
                h.noiseEnv = 1.0f;
                h.noiseDecay = perMs (6.0f);
                setBand (h, 600.0f, 1.5f);
                break;
            }
            case Kind::pedalUp:
            {
                // The pedal coming up: the dampers falling back on the strings, a soft hush, and a light knock.
                auto& h = make();
                h.freq = 60.0f;
                h.freqEnd = 55.0f;
                h.glide = 1.0f - perMs (10.0f);
                h.toneAmp = 0.12f * level;
                h.toneEnv = 1.0f;
                h.toneDecay = perMs (20.0f);
                h.noiseAmp = 0.08f * level;
                h.noiseEnv = 1.0f;
                h.noiseDecay = perMs (70.0f);
                h.attack = (int) (0.015 * sampleRate);
                setBand (h, 900.0f, 2.5f);
                break;
            }
        }
    }

    Proximity::Shelf Proximity::shelfFor (float db, double rate) noexcept
    {
        // RBJ low shelf, 180 Hz, slope 1.
        const auto a = std::pow (10.0, db / 40.0);
        const auto w = juce::MathConstants<double>::twoPi * 180.0 / rate;
        const auto cs = std::cos (w), sn = std::sin (w);
        const auto alpha = sn / 2.0 * std::sqrt (2.0);
        const auto sq = 2.0 * std::sqrt (a) * alpha;
        const auto b0 = a * ((a + 1) - (a - 1) * cs + sq), b1 = 2 * a * ((a - 1) - (a + 1) * cs), b2 = a * ((a + 1) - (a - 1) * cs - sq);
        const auto a0 = (a + 1) + (a - 1) * cs + sq, a1 = -2 * ((a - 1) + (a + 1) * cs), a2 = (a + 1) + (a - 1) * cs - sq;
        return { (float) (b0 / a0), (float) (b1 / a0), (float) (b2 / a0), (float) (a1 / a0), (float) (a2 / a0) };
    }

    void Proximity::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto stereo = block.getNumChannels() > 1;
        auto* left = block.getChannelPointer (0);
        auto* right = stereo ? block.getChannelPointer (1) : left;
        // Under a thousandth counts as 0 (the mod matrix eases a knob down and never lands exactly on 0).
        const auto zeroed = [&s] (float v) { return s.on && v >= 1.0e-3f ? juce::jmin (1.0f, v) : 0.0f; };
        const auto noiseTarget = zeroed (s.noise), roomTarget = zeroed (s.room), closeTarget = zeroed (s.close);

        if (noiseTarget <= 0.0f && roomTarget <= 0.0f && closeTarget <= 0.0f && isResting())
        {
            // Untouched. CLOSE's shelf keeps its last inputs, so it wakes without a step.
            if (n >= 2)
            {
                x2 = { left[n - 2], right[n - 2] };
                x1 = { left[n - 1], right[n - 1] };
            }
            numPending = 0;
            noiseShown.store (-100.0f, std::memory_order_relaxed);
            roomShown.store (-100.0f, std::memory_order_relaxed);
            closeShown.store (0.0f, std::memory_order_relaxed);
            return;
        }

        // The touches: only while NOISE is up (the ones already sounding fade out as they were).
        if (noiseTarget > 0.0f)
            for (int t = 0; t < numPending; ++t)
                spawn (pending[(size_t) t], s);
        numPending = 0;

        const auto hum = juce::jlimit (0, 2, s.hum);
        const auto humStep = hum == 0 ? 0.0 : (hum == 1 ? 50.0 : 60.0) / sampleRate;
        const auto roomCoefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2500.0f / (float) sampleRate);
        auto noisePower = 0.0f, roomPower = 0.0f;
        Shelf shelf;

        for (int i = 0; i < n; ++i)
        {
            noiseNow += (noiseTarget - noiseNow) * smoothing;
            roomNow += (roomTarget - roomNow) * smoothing;
            closeNow += (closeTarget - closeNow) * smoothing;
            if (noiseTarget <= 0.0f && noiseNow < 1.0e-5f) noiseNow = 0.0f;
            if (roomTarget <= 0.0f && roomNow < 1.0e-5f) roomNow = 0.0f;
            if (closeTarget <= 0.0f && closeNow < 1.0e-5f) closeNow = 0.0f;

            // CLOSE: the shelf on the sound itself, worked out every 32 samples as it glides.
            const float in[2] = { left[i], right[i] };
            float dry[2] = { in[0], in[1] };
            if (closeNow > 0.0f || shelfRunning)
            {
                if (! shelfRunning)
                {
                    y1 = x1;   // as if it had always been running at 0 dB: no step on waking
                    y2 = x2;
                    shelfRunning = true;
                }
                if (i % 32 == 0 || i == 0)
                    shelf = shelfFor (closeDb (closeNow), sampleRate);
                for (int side = 0; side < 2; ++side)
                {
                    const auto x = in[side];
                    const auto y = shelf.b0 * x + shelf.b1 * x1[(size_t) side] + shelf.b2 * x2[(size_t) side]
                                   - shelf.a1 * y1[(size_t) side] - shelf.a2 * y2[(size_t) side];
                    x2[(size_t) side] = x1[(size_t) side];
                    x1[(size_t) side] = x;
                    y2[(size_t) side] = y1[(size_t) side];
                    y1[(size_t) side] = y;
                    dry[side] = y;
                }
                if (closeNow <= 0.0f && std::abs (y1[0] - x1[0]) < 1.0e-7f && std::abs (y1[1] - x1[1]) < 1.0e-7f)
                    shelfRunning = false;   // back at 0 dB and settled: untouched from here
            }
            else
            {
                x2 = x1;
                x1 = { in[0], in[1] };
            }

            // The touches.
            float touched[2] = { 0.0f, 0.0f };
            for (auto& h : hits)
            {
                if (! h.active)
                    continue;
                if (h.delay > 0)
                {
                    --h.delay;
                    continue;
                }
                auto v = 0.0f;
                if (h.toneEnv > 1.0e-5f)
                {
                    h.freq += (h.freqEnd - h.freq) * h.glide;
                    h.phase += (double) h.freq / sampleRate;
                    if (h.phase >= 1.0)
                        h.phase -= 1.0;
                    v += h.toneAmp * h.toneEnv * (float) std::sin (juce::MathConstants<double>::twoPi * h.phase);
                    h.toneEnv *= h.toneDecay;
                }
                if (h.noiseAmp > 0.0f && h.noiseEnv > 1.0e-5f)
                {
                    const auto rise = h.attack > 0 ? juce::jmin (1.0f, (float) h.age / (float) h.attack) : 1.0f;
                    const auto white = 2.0f * random01() - 1.0f;
                    // TPT state-variable band-pass.
                    const auto a1 = 1.0f / (1.0f + h.g * (h.g + h.k));
                    const auto v1 = a1 * (h.ic1 + h.g * (white - h.ic2));
                    const auto v2 = h.ic2 + h.g * v1;
                    h.ic1 = 2.0f * v1 - h.ic1;
                    h.ic2 = 2.0f * v2 - h.ic2;
                    v += h.noiseAmp * h.noiseEnv * rise * h.k * v1;
                    if (h.age >= h.attack)
                        h.noiseEnv *= h.noiseDecay;
                }
                ++h.age;
                touched[0] += v * h.gainLeft;
                touched[1] += v * h.gainRight;
                if (h.toneEnv <= 1.0e-5f && (h.noiseAmp <= 0.0f || h.noiseEnv <= 1.0e-5f))
                    h.active = false;
            }

            // The room: it follows the music, and dies away a couple of seconds after it.
            const auto level = juce::jmax (std::abs (in[0]), std::abs (in[1]));
            follow += (level - follow) * (level > follow ? followUp : followDown);
            auto roomGain = 0.0f;
            if (roomNow > 0.0f)
            {
                const auto presence = juce::jmin (1.0f, follow / 0.03f);
                roomGain = presence < 0.02f ? 0.0f : presence * juce::Decibels::decibelsToGain (roomDb (roomNow));   // (then stops: silence stays silent)
            }
            float room[2] = { 0.0f, 0.0f };
            if (roomGain > 0.0f)
            {
                humPhase += humStep;
                if (humPhase >= 1.0)
                    humPhase -= 1.0;
                const auto p = juce::MathConstants<double>::twoPi * humPhase;
                const auto humming = hum == 0 ? 0.0f : 0.6f * (float) (std::sin (p) + 0.35 * std::sin (2.0 * p) + 0.2 * std::sin (3.0 * p));
                for (int side = 0; side < 2; ++side)
                {
                    roomState[(size_t) side] += roomCoefficient * ((2.0f * random01() - 1.0f) - roomState[(size_t) side]);
                    room[side] = roomGain * (2.0f * roomState[(size_t) side] + humming);
                }
            }
            else
                roomState = {};

            const float out[2] = { dry[0] + touched[0] + room[0], dry[1] + touched[1] + room[1] };
            left[i] = out[0];
            if (stereo)
                right[i] = out[1];
            noisePower += 0.5f * (touched[0] * touched[0] + touched[1] * touched[1]);
            roomPower += 0.5f * (room[0] * room[0] + room[1] * room[1]);
        }

        const auto np = noisePower / (float) juce::jmax (1, n), rp = roomPower / (float) juce::jmax (1, n);
        noiseShown.store (np > 1.0e-12f ? 10.0f * std::log10 (np) : -100.0f, std::memory_order_relaxed);
        roomShown.store (rp > 1.0e-12f ? 10.0f * std::log10 (rp) : -100.0f, std::memory_order_relaxed);
        closeShown.store (closeDb (closeNow), std::memory_order_relaxed);
    }
}
