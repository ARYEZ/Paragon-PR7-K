// JACK-IN (0.85.0): something breaks into the sound. Off and AMOUNT 0 leave it untouched bit for bit; the first change
// comes exactly on a slice of the song's grid; taken slices are the sound's own past (whole beats back at CHAOS 0, so
// the rhythm holds); nearly every slice is taken at full; DROPS brings tape stops; nothing clicks or blows up; turned
// down it rests, untouched again; silence stays silent.
#include "SynthTestUtils.h"

#include "dsp/effects/JackIn.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <set>

namespace
{
    using Settings = effects::JackIn::Settings;
    using Move = effects::JackIn::Move;

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        bool resting = false;
        std::vector<Move> moves;   // what each slice became, in order (the last 64 kept)
        int slices = 0;
        std::array<int, 7> counts {};
    };

    // `ppqStart`: the song position at the start (-1: stopped, its own count).
    Run runJackIn (std::vector<float> left, std::vector<float> right, Settings s, double bpm = 120.0, double ppqStart = 0.0,
                   int changeAt = -1, Settings changed = {}, std::uint32_t seed = 1)
    {
        effects::JackIn fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (seed);
        const auto beatsPerSample = bpm / 60.0 / sampleRate;
        Run run;
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            const auto ppq = ppqStart >= 0.0 ? ppqStart + beatsPerSample * (double) start : -1.0;
            fx.process (block, changeAt >= 0 && (int) start >= changeAt ? changed : s, bpm, ppq);
        }
        run.out = { left, right };
        run.resting = fx.isResting();
        run.slices = fx.getSliceCount();
        for (int k = juce::jmax (0, run.slices - effects::JackIn::maxHistoryKept); k < run.slices; ++k)
            run.moves.push_back (fx.getMove (k));
        for (int m = 0; m < 7; ++m)
            run.counts[(size_t) m] = fx.getMoveCount ((Move) m);
        return run;
    }

    std::vector<float> sine (double seconds, double hz, float level)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = level * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) i / sampleRate);
        return x;
    }

    Settings full()
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        return s;
    }
}

TEST_CASE ("JACK-IN: off and AMOUNT 0 leave the sound untouched, bit for bit", "[jackin]")
{
    const auto input = sine (3.0, 220.0, 0.3f);
    Settings off = full();
    off.on = false;
    const auto a = runJackIn (input, input, off);
    CHECK (a.out[0] == input);
    CHECK (a.out[1] == input);
    Settings zero = full();
    zero.amount = 0.0f;
    const auto b = runJackIn (input, input, zero);
    CHECK (b.out[0] == input);
    CHECK (b.resting);
}

TEST_CASE ("JACK-IN: the first change comes exactly on a slice of the song's grid", "[jackin]")
{
    // The song starts a third of a sixteenth in; 1/16 slices at 120 BPM are 6000 samples: the first boundary is at 4000.
    const auto input = sine (2.0, 220.0, 0.3f);
    auto s = full();
    s.slice = 1;
    s.drops = 0.0f;
    const auto sixteenth = 0.25;
    const auto run = runJackIn (input, input, s, 120.0, sixteenth / 3.0);
    size_t first = input.size();
    for (size_t i = 0; i < input.size(); ++i)
        if (run.out[0][i] != input[i])
        {
            first = i;
            break;
        }
    INFO ("first change at sample " << first);
    CHECK (first >= 4000);
    CHECK (first <= 4001);
}

TEST_CASE ("JACK-IN: taken slices are the sound's own past, whole beats back at CHAOS 0", "[jackin]")
{
    // A staircase: every sixteenth its own level (slice n at 0.001 n), so where a slice came from can be read off.
    const auto slice = (size_t) (0.125 * sampleRate);   // a sixteenth at 120 BPM
    std::vector<float> stairs (64 * slice);
    for (size_t i = 0; i < stairs.size(); ++i)
        stairs[i] = 0.001f * (float) (i / slice);
    auto s = full();
    s.chaos = 0.0f;
    s.crush = 0.0f;
    s.drops = 0.0f;
    s.reach = 2;   // a bar
    const auto run = runJackIn (stairs, stairs, s);
    REQUIRE (run.slices == 63);   // slice 0 is heard as it is; 1 to 63 decided
    int checked = 0, fromThePast = 0;
    for (int k = 0; k < run.slices; ++k)
    {
        const auto move = run.moves[(size_t) k];
        if (move != Move::replay && move != Move::reverse && move != Move::stutter)
            continue;
        const auto songSlice = k + 1;
        const auto mid = (size_t) songSlice * slice + 3 * slice / 8;   // inside a stutter's repeat, past the crossfade
        const auto value = run.out[0][mid];
        // Whole beats (four sixteenths) back, up to a bar.
        bool matches = false;
        for (int beats = 1; beats <= 4; ++beats)
            matches = matches || (songSlice - 4 * beats >= 0 && std::abs (value - 0.001f * (float) (songSlice - 4 * beats)) < 1.0e-6f);
        // Before the recording reaches back that far, silence.
        matches = matches || std::abs (value) < 1.0e-9f;
        ++checked;
        fromThePast += matches ? 1 : 0;
        if (! matches)
            UNSCOPED_INFO ("slice " << songSlice << " (" << effects::JackIn::moveNames[(int) move] << ") read " << value);
    }
    CHECK (checked > 30);
    CHECK (fromThePast == checked);
}

TEST_CASE ("JACK-IN: at full nearly every slice is taken; CHAOS brings reverses and octaves; DROPS tape stops", "[jackin]")
{
    const auto input = sine (20.0, 220.0, 0.3f);
    auto calm = full();
    calm.chaos = 0.0f;
    calm.drops = 0.0f;
    const auto a = runJackIn (input, input, calm);
    const auto takenA = a.slices - a.counts[(size_t) Move::dry];
    INFO ("calm: " << takenA << " of " << a.slices << " taken; reverses " << a.counts[(size_t) Move::reverse]
          << ", octaves " << a.counts[(size_t) Move::up] + a.counts[(size_t) Move::down]);
    CHECK ((double) takenA / a.slices > 0.88);
    CHECK (a.counts[(size_t) Move::stop] == 0);

    auto wild = calm;
    wild.chaos = 1.0f;
    wild.drops = 1.0f;
    const auto b = runJackIn (input, input, wild);
    const auto octavesA = a.counts[(size_t) Move::up] + a.counts[(size_t) Move::down];
    const auto octavesB = b.counts[(size_t) Move::up] + b.counts[(size_t) Move::down];
    INFO ("wild: reverses " << b.counts[(size_t) Move::reverse] << ", octaves " << octavesB << ", stops " << b.counts[(size_t) Move::stop]);
    const auto share = [] (const Run& r, int count) { return (double) count / (double) r.slices; };
    CHECK (share (b, b.counts[(size_t) Move::reverse]) > 2.5 * share (a, a.counts[(size_t) Move::reverse]));
    CHECK (share (b, octavesB) > 3.0 * share (a, octavesA));
    CHECK (b.counts[(size_t) Move::stop] >= 3);

    auto light = calm;
    light.amount = 0.2f;
    const auto c = runJackIn (input, input, light);
    const auto takenC = c.slices - c.counts[(size_t) Move::dry];
    CHECK ((double) takenC / c.slices < 0.5);
    CHECK (takenC > 0);
}

TEST_CASE ("JACK-IN: a tape stop slows to nothing by the end of its beat", "[jackin]")
{
    // A 440 Hz tone with DROPS at full: find a stop and check it falls silent before the next beat.
    const auto input = sine (30.0, 440.0, 0.3f);
    auto s = full();
    s.drops = 1.0f;
    s.crush = 0.0f;
    effects::JackIn fx;
    fx.prepare (sampleRate, blockSize);
    fx.seedRandom (5);
    auto left = input, right = input;
    const auto beat = (size_t) (0.5 * sampleRate);
    const auto slice = beat / 4;
    std::vector<int> stopAt;   // song slices where a stop starts
    for (size_t start = 0; start < left.size(); start += blockSize)
    {
        const auto n = std::min ((size_t) blockSize, left.size() - start);
        float* channels[] = { left.data() + start, right.data() + start };
        juce::dsp::AudioBlock<float> block (channels, 2, n);
        const auto before = fx.getSliceCount();
        fx.process (block, s, 120.0, 2.0 * (double) start / sampleRate);
        for (int k = before; k < fx.getSliceCount(); ++k)
            if (fx.getMove (k) == Move::stop && (k == 0 || fx.getMove (k - 1) != Move::stop))
                stopAt.push_back (k + 1);
    }
    REQUIRE (! stopAt.empty());
    for (auto songSlice : stopAt)
    {
        const auto begin = (size_t) songSlice * slice;
        const auto beatEnd = (begin / beat + 1) * beat;
        if (beatEnd + 10 > left.size())
            continue;
        // On a beat: it runs the whole beat; the last 5% of it is silent.
        CHECK (begin % beat == 0);
        float tail = 0.0f;
        for (auto i = beatEnd - beat / 20; i < beatEnd - 200; ++i)
            tail = juce::jmax (tail, std::abs (left[i]));
        CHECK (tail < 0.01f);
    }
}

TEST_CASE ("JACK-IN: nothing clicks or blows up; both sides taken the same way", "[jackin]")
{
    // A 220 Hz tone moves at most 0.0086 a sample; a click would jump far more.
    const auto input = sine (20.0, 220.0, 0.3f);
    auto s = full();
    s.chaos = 1.0f;
    s.drops = 1.0f;
    s.crush = 0.0f;
    const auto run = runJackIn (input, input, s);
    float biggestStep = 0.0f, peak = 0.0f;
    for (size_t i = 1; i < input.size(); ++i)
    {
        biggestStep = juce::jmax (biggestStep, std::abs (run.out[0][i] - run.out[0][i - 1]));
        peak = juce::jmax (peak, std::abs (run.out[0][i]));
    }
    INFO ("biggest step " << biggestStep << ", peak " << peak);
    CHECK (biggestStep < 0.05f);
    CHECK (peak < 0.35f);
    CHECK (run.out[0] == run.out[1]);

    // Left and right differ going in: each comes from its own side.
    const auto other = sine (20.0, 330.0, 0.3f);
    const auto stereo = runJackIn (input, other, s);
    double leak = 0.0;
    for (size_t i = 0; i < input.size(); ++i)
        leak += std::abs (stereo.out[0][i] - run.out[0][i]);
    CHECK (leak < 1.0e-3);
}

TEST_CASE ("JACK-IN: turned down it rests, untouched again; silence stays silent; stopped it keeps its own count", "[jackin]")
{
    const auto input = sine (6.0, 220.0, 0.3f);
    auto up = full();
    auto down = up;
    down.amount = 0.0f;
    const auto run = runJackIn (input, input, up, 120.0, 0.0, (int) (2.0 * sampleRate), down);
    double moved = 0.0;
    for (auto i = (size_t) (0.5 * sampleRate); i < (size_t) (1.5 * sampleRate); ++i)
        moved += std::abs (run.out[0][i] - input[i]);
    CHECK (moved > 10.0);
    CHECK (run.resting);
    CHECK (std::equal (run.out[0].begin() + (long) (3.0 * sampleRate), run.out[0].end(), input.begin() + (long) (3.0 * sampleRate)));

    const std::vector<float> silence ((size_t) (3.0 * sampleRate), 0.0f);
    const auto quiet = runJackIn (silence, silence, full());
    CHECK (quiet.out[0] == silence);

    // With the song stopped it still slices (its own count).
    const auto stopped = runJackIn (input, input, full(), 120.0, -1.0);
    CHECK (stopped.slices > 40);
    CHECK (stopped.out[0] != input);
}

TEST_CASE ("JACK-IN: CPU", "[jackin][cpu]")
{
    std::vector<float> input ((size_t) (10.0 * sampleRate));
    juce::Random random (7);
    for (auto& x : input)
        x = random.nextFloat() * 0.4f - 0.2f;
    auto s = full();
    s.chaos = 1.0f;
    s.crush = 1.0f;
    s.drops = 1.0f;
    s.slice = 2;
    const auto t0 = std::chrono::steady_clock::now();
    runJackIn (input, input, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    WARN ("JACK-IN at full: " << 100.0 * seconds / 10.0 << "% of a core");
    CHECK (seconds / 10.0 < 0.05);
}
