#include "Sort.h"

#include <algorithm>
#include <numeric>

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;   // AMOUNT's smoothing
        constexpr float attackSeconds = 0.015f;  // STRIKE: the hit at a piece's start
        constexpr float fallSeconds = 0.08f;     // STRIKE: and the fall after it
        constexpr float strikeDepth = 0.6f;      // at full STRIKE: +4 dB at the start, falling to -8 dB
        constexpr double noteShare = 1.0 / 3.0;  // a slice is filed by the note playing this far into it

        std::int64_t floorDiv (std::int64_t a, std::int64_t b) noexcept
        {
            const auto q = a / b;
            return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
        }
    }

    void Sort::orderFor (const float* note, const float* loudness, int count, int order, bool gapsInPlace,
                         std::uint32_t& random, int* result) noexcept
    {
        count = juce::jlimit (0, maxSlices, count);
        std::array<int, maxSlices> sounding {}, places {};
        int numSounding = 0, numPlaces = 0;
        for (int k = 0; k < count; ++k)
            if (note[k] >= 0.0f)
                sounding[(size_t) numSounding++] = k;
        auto* first = sounding.data();
        auto* last = sounding.data() + numSounding;
        switch (order)
        {
            case down:
                std::stable_sort (first, last, [note] (int a, int b) { return note[a] > note[b]; });
                break;
            case loudest:
                std::stable_sort (first, last, [loudness] (int a, int b) { return loudness[a] > loudness[b]; });
                break;
            case shuffle:
                for (int k = numSounding - 1; k > 0; --k)
                {
                    random ^= random << 13;
                    random ^= random >> 17;
                    random ^= random << 5;
                    std::swap (sounding[(size_t) k], sounding[(size_t) (random % (std::uint32_t) (k + 1))]);
                }
                break;
            case peak:
            {
                std::stable_sort (first, last, [note] (int a, int b) { return note[a] < note[b]; });
                // Rising on the even ones, falling back on the odd: the highest in the middle.
                std::array<int, maxSlices> arranged {};
                int at = 0;
                for (int k = 0; k < numSounding; k += 2)
                    arranged[(size_t) at++] = sounding[(size_t) k];
                for (int k = numSounding - 1 - (numSounding % 2 == 0 ? 0 : 1); k >= 1; k -= 2)
                    arranged[(size_t) at++] = sounding[(size_t) k];
                sounding = arranged;
                break;
            }
            case up:
            default:
                std::stable_sort (first, last, [note] (int a, int b) { return note[a] < note[b]; });
                break;
        }
        if (gapsInPlace)
        {
            // The silent slices keep their places; the sounding ones fill the rest in order.
            for (int k = 0; k < count; ++k)
                if (note[k] >= 0.0f)
                    places[(size_t) numPlaces++] = k;
            for (int k = 0; k < count; ++k)
                result[k] = k;
            for (int k = 0; k < numPlaces; ++k)
                result[places[(size_t) k]] = sounding[(size_t) k];
        }
        else
        {
            int at = 0;
            for (int k = 0; k < numSounding; ++k)
                result[at++] = sounding[(size_t) k];
            for (int k = 0; k < count; ++k)
                if (note[k] < 0.0f)
                    result[at++] = k;
        }
    }

    void Sort::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * (float) sampleRate));
        attackCoef = std::exp (-1.0f / (attackSeconds * (float) sampleRate));
        fallCoef = std::exp (-1.0f / (fallSeconds * (float) sampleRate));
        const auto size = (size_t) std::ceil (historySeconds * sampleRate) + 8;
        for (auto& h : history)
            h.assign (size, 0.0f);
        reset();
    }

    void Sort::reset() noexcept
    {
        amountNow = 0.0f;
        ownBeats = 0.0;
        lastSlice = std::numeric_limits<std::int64_t>::min();
        lastPerWindow = 0;
        for (auto& h : history)
            std::fill (h.begin(), h.end(), 0.0f);
        written = 0;
        windows = {};
        playWindow = -10;
        playCount = 0;
        current = Player {};
        current.active = true;
        current.gain = 1.0f;
        previous = Player {};
        resting = true;
        amountShown.store (0.0f);
        countShown.store (0);
        placeShown.store (-1);
        windowsShown.store (0);
        // The notes are kept: a reset mustn't forget what's held.
    }

    void Sort::setNotes (const float* notes, int count) noexcept
    {
        if (count <= 0)
            return;
        auto sum = 0.0f;
        for (int k = 0; k < count; ++k)
            sum += notes[k];
        label = sum / (float) count;
        haveLabel = true;
    }

    float Sort::readAt (int channel, std::int64_t position) const noexcept
    {
        const auto& h = history[(size_t) channel];
        const auto size = (std::int64_t) h.size();
        return h[(size_t) (((position % size) + size) % size)];
    }

    void Sort::fileWindow (const Window& w, int order, bool gapsInPlace) noexcept
    {
        std::array<float, maxSlices> notes {}, loudness {};
        for (int k = 0; k < w.count; ++k)
        {
            const auto& s = w.slices[(size_t) k];
            const auto rms = s.samples > 0 ? (float) std::sqrt (s.energy / (double) s.samples) : 0.0f;
            loudness[(size_t) k] = rms;
            notes[(size_t) k] = rms < silentRms ? -1.0f : s.note;
        }
        orderFor (notes.data(), loudness.data(), w.count, order, gapsInPlace, random, playOrder.data());
        playWindow = w.index;
        playCount = w.count;
        for (int k = 0; k < maxSlices; ++k)
        {
            notesShown[(size_t) k].store (k < w.count ? notes[(size_t) k] : -1.0f, std::memory_order_relaxed);
            orderShown[(size_t) k].store (k < w.count ? playOrder[(size_t) k] : k, std::memory_order_relaxed);
        }
        countShown.store (w.count, std::memory_order_relaxed);
        windowsShown.fetch_add (1, std::memory_order_relaxed);
    }

    void Sort::beginSlice (std::int64_t now, std::int64_t window, int place, int perWindow, const Settings&) noexcept
    {
        Player p;
        p.active = true;
        if (playWindow == window - 1 && playCount == perWindow && place < playCount)
        {
            const auto& source = windows[1];
            p.dry = false;
            p.start = source.slices[(size_t) playOrder[(size_t) place]].start;
            placeShown.store (place, std::memory_order_relaxed);
        }
        else
            placeShown.store (-1, std::memory_order_relaxed);

        // A piece that carries straight on from the one playing (the same place in the recording) isn't cut.
        if (p.dry == current.dry && (p.dry || std::abs ((double) p.start - ((double) current.start + current.t)) < 2.0))
            return;
        previous = current;
        current = p;
        (void) now;
    }

    void Sort::playerValue (Player& p, std::int64_t now, float inL, float inR, float strike, float& l, float& r) noexcept
    {
        if (p.dry)
        {
            l = inL;
            r = inR;
            return;
        }
        auto position = p.start + (std::int64_t) p.t;
        const auto size = (std::int64_t) history[0].size();
        position = juce::jlimit (now - size + 4, now - 1, position);
        auto gain = 1.0f;
        if (strike > 0.0f)
        {
            gain = (1.0f + strikeDepth * strike * p.attack) * (1.0f - strikeDepth * strike * (1.0f - p.fall));
            p.attack *= attackCoef;
            p.fall *= fallCoef;
        }
        l = readAt (0, position) * gain;
        r = readAt (1, position) * gain;
        p.t += 1.0;
    }

    void Sort::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0 || history[0].empty())
            return;
        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        bpm = juce::jmax (20.0, bpm);
        const auto beatsPerSample = bpm / 60.0 / sampleRate;
        const auto start = ppq >= 0.0 ? ppq : ownBeats;
        ownBeats += beatsPerSample * n;
        const auto first = written;

        // It always records (so it wakes with the last window to hand).
        const auto size = (std::int64_t) history[0].size();
        for (int i = 0; i < n; ++i)
        {
            const auto at = (size_t) ((first + i) % size);
            history[0][at] = left[i];
            history[1][at] = right != nullptr ? right[i] : left[i];
        }
        written += n;

        const auto sliceBeat = sliceBeats[(size_t) juce::jlimit (0, (int) sliceBeats.size() - 1, s.slice)];
        const auto windowBeat = lengthBeats[(size_t) juce::jlimit (0, (int) lengthBeats.size() - 1, s.length)];
        const auto perWindow = juce::jlimit (1, maxSlices, juce::roundToInt (windowBeat / sliceBeat));
        const auto sliceLength = sliceBeat * 60.0 / bpm * sampleRate;
        // Two windows (the one heard and the one played back) must fit in what's kept, or it plays the live sound.
        const auto fits = 2.2 * perWindow * sliceLength < (double) size;
        if (perWindow != lastPerWindow)
        {
            windows = {};
            playWindow = -10;
            lastPerWindow = perWindow;
        }
        sliceLengthNow = sliceLength;

        const auto target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        const auto awake = ! resting || target > 0.0f;
        resting = ! awake;
        fadeStep = 1.0 / (edgeMs (s.edge) * 0.001 * sampleRate);
        const auto strike = juce::jlimit (0.0f, 1.0f, s.strike);

        for (int i = 0; i < n; ++i)
        {
            const auto now = first + i;
            const auto beats = start + beatsPerSample * i;
            const auto slice = (std::int64_t) std::floor (beats / sliceBeat + 1.0e-9);
            const auto window = floorDiv (slice, perWindow);
            const auto place = (int) (slice - window * perWindow);
            if (slice != lastSlice)
            {
                auto& rec = windows[0];
                if (window != rec.index)
                {
                    // A new window: the last one, if heard whole, is filed for playing back now.
                    if (rec.complete && rec.next == rec.count && rec.index == window - 1 && fits)
                        fileWindow (rec, s.order, s.gaps == 1);
                    windows[1] = rec;
                    rec = Window {};
                    rec.index = window;
                    rec.count = perWindow;
                    rec.complete = place == 0;
                    rec.next = place;
                }
                if (place != rec.next)
                    rec.complete = false;
                rec.next = place + 1;
                rec.slices[(size_t) place] = Slice {};
                rec.slices[(size_t) place].start = now;
                if (awake)
                    beginSlice (now, window, place, perWindow, s);
                lastSlice = slice;
            }

            const auto inL = left[i], inR = right != nullptr ? right[i] : inL;
            auto& sl = windows[0].slices[(size_t) place];
            sl.energy += 0.5 * ((double) inL * inL + (double) inR * inR);
            ++sl.samples;
            if (! sl.noteTaken && (double) (now - sl.start) >= noteShare * sliceLength)
            {
                sl.note = haveLabel ? label : 60.0f;
                sl.noteTaken = true;
            }
            if (! awake)
                continue;   // untouched, bit for bit

            amountNow += (target - amountNow) * amountCoef;
            if (std::abs (target - amountNow) < 1.0e-4f)
                amountNow = target;   // in float the glide stalls about 1e-4 short (its steps round away)

            float wetL = 0.0f, wetR = 0.0f;
            playerValue (current, now, inL, inR, strike, wetL, wetR);
            if (current.gain < 1.0f)
            {
                current.gain = (float) juce::jmin (1.0, current.gain + fadeStep);
                wetL *= current.gain;
                wetR *= current.gain;
            }
            if (previous.active)
            {
                float pl = 0.0f, pr = 0.0f;
                playerValue (previous, now, inL, inR, strike, pl, pr);
                previous.gain = (float) juce::jmax (0.0, previous.gain - fadeStep);
                wetL += pl * previous.gain;
                wetR += pr * previous.gain;
                if (previous.gain <= 0.0f)
                    previous.active = false;
            }
            left[i] = inL + (wetL - inL) * amountNow;
            if (right != nullptr)
                right[i] = inR + (wetR - inR) * amountNow;
        }
        amountShown.store (amountNow, std::memory_order_relaxed);
        if (awake && target <= 0.0f && amountNow <= 0.0f && ! previous.active && current.gain >= 1.0f)
        {
            resting = true;
            current = Player {};
            current.active = true;
            current.gain = 1.0f;
            placeShown.store (-1, std::memory_order_relaxed);
        }
    }
}
