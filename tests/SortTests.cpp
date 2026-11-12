// SORT (0.93.0): the phrase re-filed. The orders themselves; off and AMOUNT 0 untouched bit for bit; the first window
// plays live (nothing heard yet to re-file); then each slice of a window is the window before's slice in ORDER, filed by
// the note set when it played (tones at known pitches, checked by which tone each place holds); silent slices to the
// end or in place; the sides kept apart; silence stays silent; STRIKE's hit and fall; turned down it rests, untouched.
#include "SynthTestUtils.h"

#include "dsp/effects/Sort.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <numeric>

namespace
{
    using Sort = effects::Sort;
    using Settings = Sort::Settings;

    constexpr double bpm = 120.0;
    constexpr int perSlice = 12000;   // an eighth at 120 BPM, 48 kHz
    constexpr int perWindow = 8;      // a bar of eighths

    double hzOf (float note) { return 440.0 * std::pow (2.0, ((double) note - 69.0) / 12.0); }

    // A phrase: each slice a steady sine at its note (or silence for a note under 0), across `windows` bars.
    std::vector<float> phrase (const std::vector<float>& notes, int windows, float level = 0.3f)
    {
        std::vector<float> x ((size_t) (windows * perWindow * perSlice), 0.0f);
        for (size_t i = 0; i < x.size(); ++i)
        {
            const auto note = notes[(i / perSlice) % notes.size()];
            if (note >= 0.0f)
                x[i] = level * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hzOf (note) * (double) i / sampleRate);
        }
        return x;
    }

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        bool resting = false;
        int filed = 0;
    };

    // `notes` gives each slice's note to the effect (as the synth would) at the block's start.
    Run runSort (std::vector<float> left, std::vector<float> right, const std::vector<float>& notes, Settings s,
                 int changeAt = -1, Settings changed = {})
    {
        Sort fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        const auto beatsPerSample = bpm / 60.0 / sampleRate;
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            const auto note = notes[(start / perSlice) % notes.size()];
            if (note >= 0.0f)
                fx.setNotes (&note, 1);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            fx.process (block, changeAt >= 0 && (int) start >= changeAt ? changed : s, bpm, beatsPerSample * (double) start);
        }
        return { { left, right }, fx.isResting(), fx.getWindowsFiled() };
    }

    // How strongly a stretch holds a tone (Goertzel, normalised).
    double toneIn (const std::vector<float>& x, size_t from, size_t to, double hz)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / sampleRate;
        double s1 = 0.0, s2 = 0.0;
        for (auto i = from; i < to; ++i)
        {
            const auto s0 = (double) x[i] + 2.0 * std::cos (w) * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const auto power = s1 * s1 + s2 * s2 - 2.0 * std::cos (w) * s1 * s2;
        return std::sqrt (power) / (double) (to - from);
    }

    // The note (of `candidates`) a place of window `window` holds, or -1 if it's silent.
    float noteAt (const std::vector<float>& x, int window, int place, const std::vector<float>& candidates)
    {
        const auto from = (size_t) ((window * perWindow + place) * perSlice + 2000), to = from + (size_t) (perSlice - 4000);
        double rms = 0.0;
        for (auto i = from; i < to; ++i)
            rms += (double) x[i] * x[i];
        if (std::sqrt (rms / (double) (to - from)) < 1.0e-3)
            return -1.0f;
        auto best = -1.0f;
        auto bestLevel = 0.0;
        for (auto c : candidates)
            if (c >= 0.0f)
                if (const auto level = toneIn (x, from, to, hzOf (c)); level > bestLevel)
                {
                    bestLevel = level;
                    best = c;
                }
        return best;
    }

    Settings full (int order = Sort::up)
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.order = order;
        s.strike = 0.0f;
        s.edge = 0.0f;
        return s;
    }

    const std::vector<float> melody { 67.0f, 60.0f, 72.0f, 64.0f, 69.0f, 62.0f, 71.0f, 65.0f };
}

TEST_CASE ("SORT: the orders", "[sort]")
{
    const std::vector<float> notes { 64.0f, 60.0f, 67.0f, -1.0f, 62.0f, 65.0f };
    const std::vector<float> loud { 0.2f, 0.5f, 0.1f, 0.0f, 0.3f, 0.4f };
    std::uint32_t random = 1;
    std::vector<int> r (notes.size());
    const auto run = [&] (int order, bool gaps)
    {
        Sort::orderFor (notes.data(), loud.data(), (int) notes.size(), order, gaps, random, r.data());
        return r;
    };
    CHECK (run (Sort::up, false) == std::vector<int> { 1, 4, 0, 5, 2, 3 });        // 60 62 64 65 67, the rest last
    CHECK (run (Sort::down, false) == std::vector<int> { 2, 5, 0, 4, 1, 3 });
    CHECK (run (Sort::up, true) == std::vector<int> { 1, 4, 0, 3, 5, 2 });         // the rest stays third from the end
    CHECK (run (Sort::loudest, false) == std::vector<int> { 1, 5, 4, 0, 2, 3 });
    // PEAK: rising to the highest and back: 60 64 67 65 62.
    CHECK (run (Sort::peak, false) == std::vector<int> { 1, 0, 2, 5, 4, 3 });
    // SHUFFLE: every slice once, silent ones last, a new order each time.
    const auto a = run (Sort::shuffle, false), b = run (Sort::shuffle, false);
    auto sorted = a;
    std::sort (sorted.begin(), sorted.end());
    CHECK (sorted == std::vector<int> { 0, 1, 2, 3, 4, 5 });
    CHECK (a.back() == 3);
    CHECK (a != b);
}

TEST_CASE ("SORT: off and AMOUNT 0 leave the sound untouched, bit for bit", "[sort]")
{
    const auto input = phrase (melody, 3);
    auto off = full();
    off.on = false;
    const auto a = runSort (input, input, melody, off);
    CHECK (a.out[0] == input);
    CHECK (a.out[1] == input);
    auto zero = full();
    zero.amount = 0.0f;
    const auto b = runSort (input, input, melody, zero);
    CHECK (b.out[0] == input);
    CHECK (b.resting);
    CHECK (b.filed >= 2);   // it keeps filing at rest, so it wakes with the last window to hand
}

TEST_CASE ("SORT: the first window plays live, then each window is the one before, re-filed", "[sort]")
{
    const auto input = phrase (melody, 3);
    auto sortedUp = melody;
    std::sort (sortedUp.begin(), sortedUp.end());
    auto sortedDown = sortedUp;
    std::reverse (sortedDown.begin(), sortedDown.end());

    const auto up = runSort (input, input, melody, full (Sort::up));
    // The first bar: nothing heard yet to re-file, so the live sound, exactly.
    CHECK (std::equal (up.out[0].begin(), up.out[0].begin() + perWindow * perSlice, input.begin()));
    for (int window = 1; window < 3; ++window)
        for (int place = 0; place < perWindow; ++place)
        {
            INFO ("window " << window << " place " << place);
            CHECK (noteAt (up.out[0], window, place, melody) == sortedUp[(size_t) place]);
        }

    const auto down = runSort (input, input, melody, full (Sort::down));
    for (int place = 0; place < perWindow; ++place)
        CHECK (noteAt (down.out[1], 2, place, melody) == sortedDown[(size_t) place]);

    // Already in order: played back whole, a bar late, without a cut (the same samples, one bar on).
    const auto rising = phrase (sortedUp, 3);
    const auto again = runSort (rising, rising, sortedUp, full (Sort::up));
    const auto bar = (size_t) (perWindow * perSlice);
    double worst = 0.0;
    for (auto i = bar + 100; i < 2 * bar; ++i)
        worst = std::max (worst, (double) std::abs (again.out[0][i] - rising[i - bar]));
    CHECK (worst < 1.0e-6);
}

TEST_CASE ("SORT: silent slices go to the end, or stay in place", "[sort]")
{
    const std::vector<float> gappy { 67.0f, 60.0f, -1.0f, 64.0f, 69.0f, -1.0f, 62.0f, 65.0f };
    const auto input = phrase (gappy, 2);
    const auto atEnd = runSort (input, input, gappy, full());
    const std::vector<float> expectEnd { 60.0f, 62.0f, 64.0f, 65.0f, 67.0f, 69.0f, -1.0f, -1.0f };
    for (int place = 0; place < perWindow; ++place)
        CHECK (noteAt (atEnd.out[0], 1, place, gappy) == expectEnd[(size_t) place]);

    auto keep = full();
    keep.gaps = 1;
    const auto inPlace = runSort (input, input, gappy, keep);
    const std::vector<float> expectPlace { 60.0f, 62.0f, -1.0f, 64.0f, 65.0f, -1.0f, 67.0f, 69.0f };
    for (int place = 0; place < perWindow; ++place)
        CHECK (noteAt (inPlace.out[0], 1, place, gappy) == expectPlace[(size_t) place]);
}

TEST_CASE ("SORT: the sides are kept apart, and silence stays silent", "[sort]")
{
    const auto input = phrase (melody, 3);
    const std::vector<float> nothing (input.size(), 0.0f);
    const auto oneSide = runSort (input, nothing, melody, full());
    CHECK (oneSide.out[1] == nothing);
    CHECK (oneSide.out[0] != input);

    auto strike = full();
    strike.strike = 1.0f;
    strike.order = Sort::shuffle;
    const auto quiet = runSort (nothing, nothing, melody, strike);
    CHECK (quiet.out[0] == nothing);
    CHECK (quiet.out[1] == nothing);
}

TEST_CASE ("SORT: STRIKE hits each piece's start and lets it fall", "[sort]")
{
    // Every slice the same note, re-filed by loudness (a stable order: each piece a fresh start).
    const std::vector<float> held { 69.0f, 64.0f, 69.0f, 64.0f, 69.0f, 64.0f, 69.0f, 64.0f };
    const auto input = phrase (held, 2);
    auto plain = full (Sort::down);
    auto struck = plain;
    struck.strike = 1.0f;
    const auto a = runSort (input, input, held, plain);
    const auto b = runSort (input, input, held, struck);
    const auto rms = [] (const std::vector<float>& x, size_t from, size_t to)
    {
        double sum = 0.0;
        for (auto i = from; i < to; ++i)
            sum += (double) x[i] * x[i];
        return std::sqrt (sum / (double) (to - from));
    };
    // A piece starting in the second window: its first 10 ms against its last 50 ms.
    const auto start = (size_t) (perWindow * perSlice + 3 * perSlice);
    const auto hitA = rms (a.out[0], start + 100, start + 480), tailA = rms (a.out[0], start + perSlice - 2400, start + perSlice - 100);
    const auto hitB = rms (b.out[0], start + 100, start + 480), tailB = rms (b.out[0], start + perSlice - 2400, start + perSlice - 100);
    CHECK (hitA / tailA < 1.1);                               // plain: as steady as the tone
    CHECK (20.0 * std::log10 (hitB / tailB) > 8.0);           // struck: a hit, then a fall
    CHECK (20.0 * std::log10 (hitB / hitA) > 2.0);
}

TEST_CASE ("SORT: turned down, it rests and is untouched again", "[sort]")
{
    const auto input = phrase (melody, 4);
    auto zero = full();
    zero.amount = 0.0f;
    const auto changeAt = (int) (2.2 * perWindow * perSlice);
    const auto run = runSort (input, input, melody, full(), changeAt, zero);
    CHECK (run.resting);
    const auto late = (size_t) (2.6 * perWindow * perSlice);
    CHECK (std::equal (run.out[0].begin() + (long) late, run.out[0].end(), input.begin() + (long) late));
    // And while it was on, something moved.
    CHECK (! std::equal (run.out[0].begin() + perWindow * perSlice, run.out[0].begin() + 2 * perWindow * perSlice,
                         input.begin() + perWindow * perSlice));
}

TEST_CASE ("SORT: the CPU it takes", "[sort][cpu]")
{
    const auto input = phrase (melody, 6);
    auto s = full (Sort::shuffle);
    s.strike = 0.5f;
    s.edge = 0.5f;
    const auto begin = std::chrono::steady_clock::now();
    const auto run = runSort (input, input, melody, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - begin).count();
    const auto share = seconds / ((double) input.size() / sampleRate);
    UNSCOPED_INFO ("SORT: " << 100.0 * share << "% of a core");
    CHECK (share < 0.05);
    CHECK (run.out[0].size() == input.size());
}
