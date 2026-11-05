// THE SFZ OSCILLATOR (0.71.0): sampled instruments read from SFZ files, measured on instruments the tests write
// themselves (sine "samples" at known pitches), so every check is against a number.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

#include "dsp/synth/Engine.h"
#include "dsp/synth/Sfz.h"
#include "dsp/synth/SfzStreamer.h"

#include "SynthTestUtils.h"

namespace
{
    // A folder of its own for each test, removed afterwards.
    struct Folder
    {
        Folder() : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("pr7k-sfz-" + juce::String (juce::Random::getSystemRandom().nextInt64())))
        {
            dir.createDirectory();
        }
        ~Folder() { dir.deleteRecursively(); }
        juce::File dir;
    };

    // A sine of `hz` for `seconds` at `amplitude` (32-bit float WAV: what's read is what was written), mono or stereo
    // (the right side at half the level, so the sides can be told apart).
    std::vector<float> writeSine (const juce::File& file, double hz, double seconds, float amplitude = 0.5f,
                                  double rate = 48000.0, int channels = 1)
    {
        const auto frames = (int) (seconds * rate);
        juce::AudioBuffer<float> buffer (channels, frames);
        std::vector<float> left ((size_t) frames);
        for (int i = 0; i < frames; ++i)
        {
            const auto v = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
            left[(size_t) i] = v;
            buffer.setSample (0, i, v);
            if (channels > 1)
                buffer.setSample (1, i, 0.5f * v);
        }
        file.deleteFile();
        file.getParentDirectory().createDirectory();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        auto options = juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (channels).withBitsPerSample (32)
                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, options);
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        return left;
    }

    juce::File writeSfz (const juce::File& dir, const juce::String& text, const juce::String& name = "test.sfz")
    {
        const auto file = dir.getChildFile (name);
        file.replaceWithText (text);
        return file;
    }

    double hzOf (int note) { return 440.0 * std::exp2 ((note - 69) / 12.0); }

    // The frequency in [from, to) by its zero crossings (placed between samples).
    double frequency (const std::vector<float>& x, size_t from, size_t to)
    {
        double first = -1.0, last = -1.0;
        int crossings = 0;
        for (auto i = from + 1; i < to; ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                const auto at = (double) (i - 1) + (double) (-x[i - 1]) / (double) (x[i] - x[i - 1]);
                if (first < 0.0)
                    first = at;
                else
                    ++crossings;
                last = at;
            }
        return crossings > 0 ? sampleRate * crossings / (last - first) : 0.0;
    }

    // The engine set up to play an SFZ instrument plainly: no filter, no AMP velocity, sustain held, a fast attack;
    // streaming from disk as a bounce does (its own streamer unless given one), or with `heads` only its heads.
    struct SfzPlayer : Player
    {
        enum Disk { stream, heads };
        std::unique_ptr<synth::sfz::Streamer> ownStreamer;
        explicit SfzPlayer (const synth::sfz::Instrument* instrument, Disk disk) : SfzPlayer (instrument, nullptr, true, disk) {}
        explicit SfzPlayer (const synth::sfz::Instrument* instrument, synth::sfz::Streamer* streamer = nullptr, bool wait = true,
                            Disk disk = stream)
        {
            if (streamer == nullptr && disk == stream)
            {
                ownStreamer = std::make_unique<synth::sfz::Streamer>();
                streamer = ownStreamer.get();
            }
            osc().on = true;
            osc().wave = synth::OscWave::sfz;
            osc().instrument = instrument;
            osc().toFilter = false;
            settings.amp.velocity = 0.0f;
            settings.amp.envelope.attackMs = 0.0f;
            settings.amp.envelope.sustain = 1.0f;
            settings.amp.envelope.releaseMs = 50.0f;
            engine.setStreamer (streamer, wait);
        }
    };
}

//==============================================================================
TEST_CASE ("SFZ: the reader takes headers, inheritance, #define, #include and note names", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("samples/c4.wav"), hzOf (60), 0.1);
    writeSine (f.dir.getChildFile ("samples/a4.wav"), hzOf (69), 0.1);
    f.dir.getChildFile ("more.sfz").replaceWithText ("<region> sample=a4.wav lokey=f#4 hikey=b4 pitch_keycenter=a4 lovel=64\n");
    const auto file = writeSfz (f.dir, R"(
        // a comment
        #define $LOUD 6
        <control> default_path=samples/
        <global> amp_veltrack=50
        <group> volume=$LOUD /* inside */ pan=-100
        <region> sample=c4.wav lokey=c4 hikey=f4 pitch_keycenter=60 tune=25 transpose=-1
        #include "more.sfz"
        <region> sample=missing.wav key=30
    )");
    const auto inst = synth::sfz::load (file);
    REQUIRE (inst != nullptr);
    REQUIRE (inst->regions.size() == 2);
    const auto& a = inst->regions[0];
    CHECK (a.loKey == 60);
    CHECK (a.hiKey == 65);
    CHECK_THAT (a.keyCenter, WithinAbs (60.0, 1e-6));
    CHECK_THAT (a.tuneSemitones, WithinAbs (-0.75, 1e-6));
    CHECK_THAT (a.gain, WithinAbs (juce::Decibels::decibelsToGain (6.0f), 1e-5));
    CHECK_THAT (a.pan, WithinAbs (-1.0, 1e-6));
    CHECK_THAT (a.velTrack, WithinAbs (0.5, 1e-6));
    const auto& b = inst->regions[1];
    CHECK (b.loKey == 66);
    CHECK (b.hiKey == 71);
    CHECK_THAT (b.keyCenter, WithinAbs (69.0, 1e-6));
    CHECK (b.loVel == 64);
    CHECK_THAT (b.gain, WithinAbs (juce::Decibels::decibelsToGain (6.0f), 1e-5));   // the <group> carries into the #include
    CHECK (inst->problems.joinIntoString (";").contains ("missing.wav"));
    CHECK (inst->loKey == 60);
    CHECK (inst->hiKey == 71);
    CHECK (! inst->streamed);   // 0.1 s: all of it in the head
}

TEST_CASE ("SFZ: #include names take the #defines, redefined between them (0.76.0)", "[sfz]")
{
    // As SSO's grand piano: "#define $DYN PP" then "#include "$DIR/$DYN.txt"", again for each layer. Before 0.76.0 the
    // include's name kept "$DIR/$DYN.txt" and nothing was read.
    Folder f;
    writeSine (f.dir.getChildFile ("samples/soft.wav"), hzOf (60), 0.1);
    writeSine (f.dir.getChildFile ("samples/loud.wav"), hzOf (60), 0.1);
    f.dir.getChildFile ("Data/PP.txt").create();
    f.dir.getChildFile ("Data/PP.txt").replaceWithText ("<region> sample=soft.$EXT key=60\n");
    f.dir.getChildFile ("Data/FF.txt").replaceWithText ("<region> sample=loud.$EXT key=60\n");
    const auto file = writeSfz (f.dir, R"(
        #define $DIR Data
        #define $EXT wav
        <control> default_path=samples/
        <group> lovel=1 hivel=64
        #define $DYN PP
        #include "$DIR/$DYN.txt"
        <group> lovel=65 hivel=127
        #define $DYN FF
        #include "$DIR/$DYN.txt"
    )");
    const auto inst = synth::sfz::load (file);
    REQUIRE (inst != nullptr);
    REQUIRE (inst->regions.size() == 2);
    CHECK (inst->samples.size() == 2);
    CHECK (inst->regions[0].hiVel == 64);
    CHECK (inst->regions[1].loVel == 65);
    CHECK (inst->samples[(size_t) inst->regions[0].sample]->file.getFileName() == "soft.wav");
    CHECK (inst->samples[(size_t) inst->regions[1].sample]->file.getFileName() == "loud.wav");
}

TEST_CASE ("SFZ: each key plays its region at the right pitch, with TUNE, PITCH_KEYTRACK and the oscillator's own pitch", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("c4.wav"), hzOf (60), 1.0);
    writeSine (f.dir.getChildFile ("c5.wav"), hzOf (72), 1.0);
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<region> sample=c4.wav lokey=48 hikey=66 pitch_keycenter=60\n"
        "<region> sample=c5.wav lokey=67 hikey=84 pitch_keycenter=72\n"
        "<region> sample=c4.wav key=40 pitch_keycenter=40 tune=50\n"
        "<region> sample=c4.wav lokey=20 hikey=30 pitch_keycenter=25 pitch_keytrack=0\n"));
    REQUIRE (inst != nullptr);
    for (const auto note : { 48, 55, 60, 66, 67, 72, 79, 84 })
    {
        SfzPlayer p (inst.get());
        p.on (note);
        const auto out = p.render (0.2);
        INFO ("note " << note);
        CHECK_THAT (frequency (out[0], 960, out[0].size()), WithinAbs (hzOf (note), hzOf (note) * 0.0005));
    }
    {
        SfzPlayer p (inst.get());   // TUNE 50 cents: key 40 sounds C4 a quarter tone up
        p.on (40);
        const auto out = p.render (0.2);
        CHECK_THAT (frequency (out[0], 960, out[0].size()), WithinAbs (hzOf (60) * std::exp2 (0.5 / 12.0), 0.2));
    }
    {
        SfzPlayer p (inst.get());   // PITCH_KEYTRACK 0: every key of 20-30 plays the sample as recorded (C4)
        p.on (22);
        const auto out = p.render (0.2);
        CHECK_THAT (frequency (out[0], 960, out[0].size()), WithinAbs (hzOf (60), 0.2));
    }
    {
        SfzPlayer p (inst.get());   // the oscillator's TRANSPOSE and FINE move it as any wave
        p.osc().transpose = 7.0f;
        p.osc().fineCents = -30.0f;
        p.on (60);
        const auto out = p.render (0.2);
        CHECK_THAT (frequency (out[0], 960, out[0].size()), WithinAbs (hzOf (60) * std::exp2 (6.7 / 12.0), 0.2));
    }
    {
        SfzPlayer p (inst.get());   // a key no region plays: silence
        p.on (100);
        const auto out = p.render (0.1);
        CHECK (rms (out[0], 0, out[0].size()) == 0.0);
    }
}

TEST_CASE ("SFZ: samples at another rate play at their pitch; stereo samples keep their sides; PAN", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("44k.wav"), hzOf (60), 0.5, 0.5f, 44100.0, 2);
    const auto inst = synth::sfz::load (writeSfz (f.dir, "<region> sample=44k.wav lokey=0 hikey=80 pitch_keycenter=60\n"
                                                         "<region> sample=44k.wav key=90 pitch_keycenter=90 pan=100\n"));
    REQUIRE (inst != nullptr);
    SfzPlayer p (inst.get());
    p.on (60);
    const auto out = p.render (0.3);
    CHECK_THAT (frequency (out[0], 960, out[0].size()), WithinAbs (hzOf (60), 0.1));
    CHECK_THAT (rms (out[1], 960, out[1].size()) / rms (out[0], 960, out[0].size()), WithinAbs (0.5, 0.01));

    SfzPlayer right (inst.get());
    right.on (90);
    const auto panned = right.render (0.3);
    CHECK (rms (panned[0], 960, panned[0].size()) < 1e-6);
    CHECK (rms (panned[1], 960, panned[1].size()) > 0.05);
}

TEST_CASE ("SFZ: velocity layers, AMP_VELTRACK, VOLUME and AMPLITUDE", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("soft.wav"), hzOf (60), 0.5);
    writeSine (f.dir.getChildFile ("hard.wav"), hzOf (67), 0.5);
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<region> sample=soft.wav lokey=0 hikey=70 lovel=1 hivel=80 pitch_keycenter=60 amp_veltrack=0\n"
        "<region> sample=hard.wav lokey=0 hikey=70 lovel=81 hivel=127 pitch_keycenter=60 amp_veltrack=0 volume=-6 amplitude=50\n"
        "<region> sample=soft.wav key=80 pitch_keycenter=80\n"));
    REQUIRE (inst != nullptr);
    const auto play = [&] (int note, float velocity)
    {
        SfzPlayer p (inst.get());
        p.on (note, velocity);
        return p.render (0.3);
    };
    const auto soft = play (60, 0.5f), hard = play (60, 1.0f);
    CHECK_THAT (frequency (soft[0], 960, soft[0].size()), WithinAbs (hzOf (60), 0.2));   // the soft layer's sample
    CHECK_THAT (frequency (hard[0], 960, hard[0].size()), WithinAbs (hzOf (67), 0.3));   // the hard one's (recorded a fifth up)
    // -6 dB and 50%: about -12 dB under the soft layer (amp_veltrack 0: velocity doesn't change the level).
    CHECK_THAT (db (rms (hard[0], 960, hard[0].size()) / rms (soft[0], 960, soft[0].size())), WithinAbs (-12.0, 0.1));

    // AMP_VELTRACK 100% (the default): velocity squared.
    const auto full = play (80, 1.0f), half = play (80, 64.0f / 127.0f);
    CHECK_THAT (rms (half[0], 960, half[0].size()) / rms (full[0], 960, full[0].size()),
                WithinAbs (std::pow (64.0 / 127.0, 2.0), 0.005));
}

TEST_CASE ("SFZ: loops hold a note for as long as it's held; LOOP_SUSTAIN lets go at the release; a WAV's own loop", "[sfz]")
{
    // 480 Hz at 48 kHz: 100 samples a cycle, so a loop of whole cycles is seamless. 0.5 s long; the loop 0.1-0.3 s.
    Folder f;
    writeSine (f.dir.getChildFile ("tone.wav"), 480.0, 0.5);
    const auto loopStart = 4800, loopEnd = 4800 + 9600 - 1;
    const auto make = [&] (const juce::String& extra)
    {
        auto inst = synth::sfz::load (writeSfz (f.dir, "<region> sample=tone.wav key=60 amp_veltrack=0 " + extra + "\n",
                                                "t" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".sfz"));
        REQUIRE (inst != nullptr);
        return inst;
    };
    const auto sustained = [&] (const synth::sfz::Instrument& inst, double holdSeconds, double totalSeconds, float releaseMs)
    {
        SfzPlayer p (&inst);
        p.settings.amp.envelope.releaseMs = releaseMs;
        p.on (60, 1.0f);
        auto held = p.render (holdSeconds);
        p.off (60);
        auto rest = p.render (totalSeconds - holdSeconds);
        held[0].insert (held[0].end(), rest[0].begin(), rest[0].end());
        return held[0];
    };
    const auto at = [] (double seconds) { return (size_t) (seconds * sampleRate); };

    // No loop: the note ends with its recording (0.5 s), however long it's held.
    {
        const auto plain = sustained (*make (""), 2.0, 2.2, 50.0f);
        CHECK (rms (plain, at (1.0), at (1.5)) < 1.0e-6);
    }
    // LOOP_CONTINUOUS: still the tone at 1.5 s, at its level and pitch, without a click at the turns (the loop's
    // whole cycles meet: every sample within the sine's own range and no jump bigger than a sine's step).
    {
        const auto inst = make ("loop_mode=loop_continuous loop_start=" + juce::String (loopStart) + " loop_end=" + juce::String (loopEnd));
        REQUIRE (inst->regions[0].loops());
        CHECK_FALSE (inst->streamed);   // kept whole in memory
        const auto looped = sustained (*inst, 2.0, 2.2, 50.0f);
        CHECK_THAT (rms (looped, at (1.0), at (1.5)), WithinAbs (0.5 / std::sqrt (2.0), 0.002));
        CHECK_THAT (levelAt (looped, 480.0, at (1.0), at (0.5)), WithinAbs (0.5, 0.005));
        auto biggestStep = 0.0f;
        for (auto i = at (0.05); i < at (1.9); ++i)
            biggestStep = juce::jmax (biggestStep, std::abs (looped[i] - looped[i - 1]));
        CHECK (biggestStep < 0.5f * (float) (juce::MathConstants<double>::twoPi * 480.0 / sampleRate) * 1.05f);
        // ...and it keeps going through a long AMP release.
        const auto longRelease = sustained (*inst, 1.0, 2.0, 3000.0f);
        CHECK (rms (longRelease, at (1.6), at (1.9)) > 0.05);
    }
    // A loop with no LOOP_START / LOOP_END runs over the whole recording, to its last frame.
    {
        const auto inst = make ("loop_mode=loop_continuous");
        REQUIRE (inst->regions[0].loops());
        CHECK (inst->regions[0].loopStart == 0);
        CHECK (inst->regions[0].loopEnd == 24000 - 1);
        const auto whole = sustained (*inst, 2.0, 2.2, 50.0f);
        CHECK_THAT (levelAt (whole, 480.0, at (1.0), at (0.5)), WithinAbs (0.5, 0.005));
    }
    // LOOP_SUSTAIN: loops while held; let go, it plays on past the loop to the recording's end (0.2 s on), then stops,
    // even under a long AMP release.
    {
        const auto inst = make ("loop_mode=loop_sustain loop_start=" + juce::String (loopStart) + " loop_end=" + juce::String (loopEnd));
        const auto out = sustained (*inst, 1.0, 2.0, 3000.0f);
        CHECK (rms (out, at (0.8), at (0.95)) > 0.3);
        CHECK (rms (out, at (1.5), at (1.9)) < 1.0e-6);
    }
    // The file's own loop (a WAV "smpl" chunk) loops without LOOP_MODE; LOOP_MODE=no_loop turns it off.
    {
        const auto file = f.dir.getChildFile ("smpl.wav");
        const auto frames = 24000;
        juce::AudioBuffer<float> buffer (1, frames);
        for (int i = 0; i < frames; ++i)
            buffer.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 480.0 * i / sampleRate));
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        const std::unordered_map<juce::String, juce::String> meta { { "NumSampleLoops", "1" }, { "Loop0Start", juce::String (loopStart) },
                                                                   { "Loop0End", juce::String (loopEnd) }, { "Loop0Type", "0" } };
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sampleRate).withNumChannels (1)
                                                       .withBitsPerSample (32).withMetadataValues (meta)
                                                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        writer.reset();
        const auto own = synth::sfz::load (writeSfz (f.dir, "<region> sample=smpl.wav key=60 amp_veltrack=0\n", "own.sfz"));
        REQUIRE (own != nullptr);
        CHECK (own->regions[0].loops());
        CHECK (own->regions[0].loopStart == loopStart);
        CHECK (own->regions[0].loopEnd == loopEnd);
        CHECK (rms (sustained (*own, 2.0, 2.2, 50.0f), at (1.0), at (1.5)) > 0.3);
        const auto off = synth::sfz::load (writeSfz (f.dir, "<region> sample=smpl.wav key=60 loop_mode=no_loop\n", "off.sfz"));
        REQUIRE (off != nullptr);
        CHECK_FALSE (off->regions[0].loops());
    }
}

TEST_CASE ("SFZ: velocity crossfades (XFIN / XFOUT_LOVEL / HIVEL) blend layers, equal power or straight", "[sfz]")
{
    // A soft layer at 480 Hz fading out from velocity 40 to 100, a loud one at 720 Hz fading in across the same.
    Folder f;
    writeSine (f.dir.getChildFile ("soft.wav"), 480.0, 0.5);
    writeSine (f.dir.getChildFile ("loud.wav"), 720.0, 0.5);
    const auto levels = [&] (const juce::String& curve, int velocity127)
    {
        const auto inst = synth::sfz::load (writeSfz (f.dir,
            "<group> key=60 amp_veltrack=0 " + curve + "\n"
            "<region> sample=soft.wav xfout_lovel=40 xfout_hivel=100\n"
            "<region> sample=loud.wav xfin_lovel=40 xfin_hivel=100\n", curve.isEmpty() ? "p.sfz" : "g.sfz"));
        REQUIRE (inst != nullptr);
        SfzPlayer p (inst.get());
        p.on (60, (float) velocity127 / 127.0f);
        const auto out = p.render (0.3);
        return std::make_pair (levelAt (out[0], 480.0, 960, 9600) / 0.5, levelAt (out[0], 720.0, 960, 9600) / 0.5);
    };
    // Below the fades: the soft layer alone; above: the loud one alone.
    auto [soft, loud] = levels ("", 30);
    CHECK_THAT (soft, WithinAbs (1.0, 0.01));
    CHECK (loud < 0.001);
    std::tie (soft, loud) = levels ("", 110);
    CHECK (soft < 0.001);
    CHECK_THAT (loud, WithinAbs (1.0, 0.01));
    // Halfway (velocity 70): equal power, each sqrt (1/2) (their powers add to one); straight (XF_VELCURVE=gain), each 1/2.
    std::tie (soft, loud) = levels ("", 70);
    CHECK_THAT (soft, WithinAbs (std::sqrt (0.5), 0.01));
    CHECK_THAT (loud, WithinAbs (std::sqrt (0.5), 0.01));
    std::tie (soft, loud) = levels ("xf_velcurve=gain", 70);
    CHECK_THAT (soft, WithinAbs (0.5, 0.01));
    CHECK_THAT (loud, WithinAbs (0.5, 0.01));
}

TEST_CASE ("SFZ: round-robins take turns, LORAND / HIRAND by chance, LOCC64 / HICC64 by the pedal", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("a.wav"), hzOf (60), 0.3);
    writeSine (f.dir.getChildFile ("b.wav"), hzOf (64), 0.3);
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<group> pitch_keycenter=60 lokey=50 hikey=70 seq_length=2\n"
        "<region> sample=a.wav seq_position=1\n<region> sample=b.wav seq_position=2\n"
        "<group> pitch_keycenter=80 lokey=80 hikey=80\n"
        "<region> sample=a.wav lorand=0 hirand=0.5\n<region> sample=b.wav lorand=0.5 hirand=1\n"
        "<group> pitch_keycenter=90 key=90\n"
        "<region> sample=a.wav hicc64=63\n<region> sample=b.wav locc64=64\n"));
    REQUIRE (inst != nullptr);
    SfzPlayer p (inst.get());
    std::vector<double> heard;
    for (int k = 0; k < 4; ++k)
    {
        p.on (60);
        const auto out = p.render (0.2);
        heard.push_back (frequency (out[0], 960, out[0].size()));
        p.off (60);
        p.render (0.2);
    }
    CHECK_THAT (heard[0], WithinAbs (hzOf (60), 0.3));
    CHECK_THAT (heard[1], WithinAbs (hzOf (64), 0.3));
    CHECK_THAT (heard[2], WithinAbs (hzOf (60), 0.3));
    CHECK_THAT (heard[3], WithinAbs (hzOf (64), 0.3));

    // LORAND / HIRAND: over many notes, both halves of the chance play, one at a time.
    int low = 0, high = 0;
    SfzPlayer r (inst.get());
    for (int k = 0; k < 40; ++k)
    {
        r.on (80);
        const auto out = r.render (0.06);
        const auto hz = frequency (out[0], 480, out[0].size());
        low += std::abs (hz - hzOf (60)) < 2.0 ? 1 : 0;    // (each region's key is its own: the sample's pitch)
        high += std::abs (hz - hzOf (64)) < 2.0 ? 1 : 0;
        r.off (80);
        r.render (0.1);
    }
    CHECK (low + high == 40);
    CHECK (low > 8);
    CHECK (high > 8);

    // The sustain pedal chooses between the two regions of key 90.
    SfzPlayer pedal (inst.get());
    pedal.on (90);
    auto out = pedal.render (0.2);
    CHECK_THAT (frequency (out[0], 960, out[0].size()), WithinAbs (hzOf (60), 0.5));
    pedal.off (90);
    pedal.render (0.2);
    pedal.midi (juce::MidiMessage::controllerEvent (1, 64, 127));
    pedal.render (0.01);
    pedal.on (90);
    out = pedal.render (0.2);
    CHECK_THAT (frequency (out[0], 960, out[0].size()), WithinAbs (hzOf (64), 0.5));
}

TEST_CASE ("SFZ: release samples play when the note lets go (after the pedal), quieter by RT_DECAY, past the AMP", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("note.wav"), hzOf (60), 2.0);
    writeSine (f.dir.getChildFile ("rel.wav"), hzOf (72), 0.4);
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<region> sample=note.wav pitch_keycenter=60\n"
        "<region> sample=rel.wav pitch_keycenter=60 trigger=release rt_decay=10\n"));
    REQUIRE (inst != nullptr);
    CHECK (inst->hasRelease);

    const auto releaseLevel = [&] (double heldSeconds, bool pedal)
    {
        SfzPlayer p (inst.get());
        p.settings.amp.envelope.releaseMs = 5.0f;   // the note itself gone at once...
        if (pedal)
            p.midi (juce::MidiMessage::controllerEvent (1, 64, 127));
        p.on (60);
        auto held = p.render (heldSeconds);
        // While held: only the note (C4), no release sample (C5).
        CHECK (levelAt (held[0], hzOf (72), held[0].size() / 2, held[0].size() / 2) < 1e-4);
        p.off (60);
        auto after = p.render (0.2);
        if (pedal)
        {
            // The pedal holds the note: still no release sample.
            CHECK (levelAt (after[0], hzOf (72), 4800, 4800) < 1e-4);
            p.midi (juce::MidiMessage::controllerEvent (1, 64, 0));
            after = p.render (0.2);
        }
        // ...but its release sample plays its 0.4 s through (past the AMP's 5 ms release), at C5.
        return levelAt (after[0], hzOf (72), 2400, 4800);
    };
    const auto shortHeld = releaseLevel (0.5, false), longHeld = releaseLevel (1.5, false);
    CHECK (shortHeld > 0.1);
    // RT_DECAY 10 dB a second: held a second longer, 10 dB quieter.
    CHECK_THAT (db (longHeld / shortHeld), WithinAbs (-10.0, 0.3));
    CHECK (releaseLevel (0.5, true) > 0.05);
}

TEST_CASE ("SFZ: AMPEG_RELEASE longer than the AMP's lets a sample ring on after its key (undamped strings)", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("ring.wav"), hzOf (60), 3.0);
    writeSine (f.dir.getChildFile ("damped.wav"), hzOf (72), 3.0);
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<region> sample=ring.wav key=60 ampeg_release=3\n"
        "<region> sample=damped.wav key=72 ampeg_release=0.01\n"));
    REQUIRE (inst != nullptr);
    CHECK_THAT (inst->regions[0].releaseSeconds, WithinAbs (3.0, 1e-6));
    const auto play = [&] (int key)
    {
        SfzPlayer p (inst.get());
        p.settings.amp.envelope.releaseMs = 50.0f;
        p.on (key);
        const auto held = p.render (0.5);
        p.off (key);
        const auto after = p.render (0.6);
        return std::pair { rms (held[0], 12000, 24000), rms (after[0], 21600, 26400) };   // 0.45-0.55 s after the release
    };
    const auto [ringHeld, ringAfter] = play (60);
    // Falling 60 dB in 3 s: half a second on, about 10 dB down (the AMP's 50 ms release didn't cut it).
    CHECK_THAT (db (ringAfter / ringHeld), WithinAbs (-10.0, 1.0));
    const auto [dampedHeld, dampedAfter] = play (72);
    CHECK (dampedHeld > 0.1);
    CHECK (dampedAfter < 1.0e-6);   // shorter than the AMP's: the AMP ends it
}

TEST_CASE ("SFZ: the sustain pedal plays its own sounds (ON_LOCC64 / ON_HICC64), keys don't", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("note.wav"), hzOf (60), 1.0);
    writeSine (f.dir.getChildFile ("down.wav"), 1500.0, 0.3);
    writeSine (f.dir.getChildFile ("up.wav"), 3200.0, 0.3);
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<region> sample=note.wav pitch_keycenter=60\n"
        "<group> lokey=-1 hikey=-1 on_locc64=126 on_hicc64=127\n<region> sample=down.wav\n"
        "<group> lokey=-1 hikey=-1 on_locc64=0 on_hicc64=1\n<region> sample=up.wav\n"));
    REQUIRE (inst != nullptr);
    CHECK (inst->hasPedalSounds);
    CHECK (inst->regions.size() == 3);
    SfzPlayer p (inst.get());
    p.midi (juce::MidiMessage::controllerEvent (1, 64, 127));
    const auto down = p.render (0.2);
    CHECK (levelAt (down[0], 1500.0, 480, 4800) > 0.1);   // no note held: the pedal still sounds
    CHECK (levelAt (down[0], 3200.0, 480, 4800) < 0.01);
    p.midi (juce::MidiMessage::controllerEvent (1, 64, 0));
    const auto up = p.render (0.2);
    CHECK (levelAt (up[0], 3200.0, 480, 4800) > 0.1);
    p.render (0.5);
    p.on (60);
    const auto note = p.render (0.2);
    CHECK (levelAt (note[0], 1500.0, 480, 4800) < 0.01);   // a key plays only its note
    CHECK (levelAt (note[0], 3200.0, 480, 4800) < 0.01);
    CHECK (levelAt (note[0], hzOf (60), 480, 4800) > 0.1);
}

TEST_CASE ("SFZ: a long sample streams from disk and plays exactly as written; without a stream, its head and a fade", "[sfz][streaming]")
{
    Folder f;
    const auto written = writeSine (f.dir.getChildFile ("long.wav"), 333.0, 3.0, 0.5f);
    const auto inst = synth::sfz::load (writeSfz (f.dir, "<region> sample=long.wav pitch_keycenter=60\n"));
    REQUIRE (inst != nullptr);
    CHECK (inst->streamed);
    CHECK (inst->samples[0]->headFrames == 12000);

    // Bouncing (waits for the disk): every sample as in the file, the head and the streamed rest alike.
    {
        synth::sfz::Streamer streamer;
        SfzPlayer p (inst.get(), &streamer, true);
        p.on (60);
        const auto out = p.render (3.2);
        const auto k = out[0][1000] / written[1000];
        auto worst = 0.0f;
        for (size_t i = 100; i < written.size(); ++i)
            worst = juce::jmax (worst, std::abs (out[0][i] - k * written[i]));
        CHECK (worst < 1e-5f);
        CHECK (rms (out[0], written.size() + 10, out[0].size()) == 0.0);   // over at the sample's end
        CHECK (streamer.getUnderruns() == 0);
    }
    // Live (never waits): the disk thread keeps ahead; nothing late.
    {
        synth::sfz::Streamer streamer;
        SfzPlayer p (inst.get(), &streamer, false);
        p.on (60);
        std::vector<float> out;
        for (int block = 0; block < (int) (3.0 * sampleRate / blockSize); ++block)
        {
            streamer.waitUntilFilled (500);
            const auto part = p.render ((double) blockSize / sampleRate);
            out.insert (out.end(), part[0].begin(), part[0].end());
        }
        CHECK (streamer.getUnderruns() == 0);
        const auto k = out[1000] / written[1000];
        auto worst = 0.0f;
        for (size_t i = 100; i < out.size(); ++i)
            worst = juce::jmax (worst, std::abs (out[i] - k * written[i]));
        CHECK (worst < 1e-5f);
        p.off (60);
        p.render (0.2);
        CHECK (streamer.getActiveStreams() == 0);   // freed when the note ended
    }
    // No streamer: the head (0.25 s), fading out over its last 5 ms, then silence.
    {
        SfzPlayer p (inst.get(), SfzPlayer::heads);
        p.on (60);
        const auto out = p.render (0.5);
        CHECK (rms (out[0], 6000, 11000) > 0.2);
        CHECK (rms (out[0], 12001, out[0].size()) == 0.0);
    }
}

TEST_CASE ("SFZ: through the synth: the FILTER, and a mono lead's new note", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("c4.wav"), hzOf (60), 1.0);
    writeSine (f.dir.getChildFile ("hi.wav"), 3000.0, 1.0);
    const auto inst = synth::sfz::load (writeSfz (f.dir, "<region> sample=c4.wav lokey=0 hikey=70 pitch_keycenter=60\n"
                                                         "<region> sample=hi.wav key=100 pitch_keycenter=100\n"));
    REQUIRE (inst != nullptr);
    // The FILTER works on it as on any wave: a 3 kHz tone through a 300 Hz low-pass.
    SfzPlayer open (inst.get()), closed (inst.get());
    closed.osc().toFilter = true;
    closed.settings.filter.on = true;
    closed.settings.filter.cutoffHz = 300.0f;
    closed.settings.filter.keytrack = 0.0f;
    open.on (100);
    closed.on (100);
    const auto a = open.render (0.3), b = closed.render (0.3);
    CHECK (db (rms (b[0], 4800, b[0].size()) / rms (a[0], 4800, a[0].size())) < -30.0);

    // Mono: the new note's pitch (legato or restarted).
    SfzPlayer mono (inst.get());
    mono.settings.play.mode = synth::PlayMode::mono;
    mono.on (60);
    mono.render (0.2);
    mono.on (67);
    const auto out = mono.render (0.2);
    CHECK_THAT (frequency (out[0], 960, out[0].size()), WithinAbs (hzOf (67), 0.3));
}

TEST_CASE ("SFZ: CPU, 16 held notes streaming", "[sfz][cpu]")
{
    Folder f;
    for (int k = 0; k < 4; ++k)
        writeSine (f.dir.getChildFile ("n" + juce::String (k) + ".wav"), hzOf (48 + 12 * k), 6.0, 0.1f, 48000.0, 2);
    juce::String text;
    for (int k = 0; k < 4; ++k)
        text << "<region> sample=n" << k << ".wav lokey=" << (42 + 12 * k) << " hikey=" << (53 + 12 * k)
             << " pitch_keycenter=" << (48 + 12 * k) << "\n";
    const auto inst = synth::sfz::load (writeSfz (f.dir, text));
    REQUIRE (inst != nullptr);
    synth::sfz::Streamer streamer;
    SfzPlayer p (inst.get(), &streamer, true);
    p.settings.oscillators[0].toFilter = true;   // as a preset would play it, through the filter
    for (int k = 0; k < 16; ++k)
        p.on (45 + k * 3);
    p.render (0.5);
    const auto seconds = 4.0;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    p.render (seconds);
    const auto share = (juce::Time::getMillisecondCounterHiRes() - start) / (seconds * 1000.0);
    WARN ("SFZ, 16 notes streaming: " << juce::String (share * 100.0, 1) << "% of a core");
    CHECK (share < 0.25);
    CHECK (streamer.getUnderruns() == 0);
}


//==============================================================================
// In the plugin: loading, presets, projects, undo, the library moving.
#include "Parameters.h"
#include "PluginProcessor.h"

namespace
{
    std::unique_ptr<ParagonX1AudioProcessor> makeSfzProcessor (const juce::File& presets)
    {
        auto processor = std::make_unique<ParagonX1AudioProcessor>();
        processor->useRigFolder (presets);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
        processor->setNonRealtime (true);   // waits for the disk, as a bounce: the same sound every run
        return processor;
    }

    std::vector<float> playNote (ParagonX1AudioProcessor& processor, int note, double seconds)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int start = 0; start < (int) (seconds * sampleRate); start += blockSize)
        {
            juce::MidiBuffer midi;
            if (start == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        buffer.clear();
        processor.processBlock (buffer, off);
        for (int k = 0; k < 40; ++k)
        {
            juce::MidiBuffer none;
            buffer.clear();
            processor.processBlock (buffer, none);
        }
        return out;
    }

    int waveOf (ParagonX1AudioProcessor& processor, int osc)
    {
        return juce::roundToInt (processor.getParameters().getRawParameterValue (ParamIDs::osc (osc, "Wave"))->load());
    }
}

TEST_CASE ("SFZ in the plugin: an oscillator loads it in the background and plays it", "[sfz][plugin]")
{
    Folder f;
    const auto presets = f.dir.getChildFile ("presets");
    presets.createDirectory();
    const auto library = f.dir.getChildFile ("My Piano");
    writeSine (library.getChildFile ("c4.wav"), hzOf (60), 2.0);
    const auto file = writeSfz (library, "<region> sample=c4.wav lokey=36 hikey=84 pitch_keycenter=60\n", "piano.sfz");

    auto processor = makeSfzProcessor (presets);
    processor->loadSfz (0, file);
    CHECK (waveOf (*processor, 0) == (int) synth::OscWave::sfz);
    REQUIRE (processor->waitForSfz());
    CHECK (processor->getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);
    CHECK (processor->getSfzName (0) == "piano");
    const auto out = playNote (*processor, 67, 1.0);
    CHECK_THAT (frequency (out, 9600, out.size()), WithinAbs (hzOf (67), 0.3));
    CHECK (processor->getSfzUnderruns() == 0);
    CHECK (processor->getSfzStreams() == 0);   // its stream freed once the note ended

    // The preset names the file, never its samples; another instance loads it from the preset.
    const auto rig = processor->captureRig();
    REQUIRE (rig.sfz.count (0) == 1);
    const auto xml = rig.toXml();
    CHECK (! xml->toString().contains ("RIFF"));
    const auto parsed = Rig::fromXml (*xml);
    REQUIRE (parsed.has_value());
    const auto reread = *parsed;
    CHECK (reread.sfz == rig.sfz);
    auto other = makeSfzProcessor (presets);
    RigLibrary::Entry entry;
    entry.rig = reread;
    other->loadRig (entry);
    REQUIRE (other->waitForSfz());
    CHECK (other->getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);
    const auto again = playNote (*other, 67, 0.6);
    CHECK_THAT (frequency (again, 9600, again.size()), WithinAbs (hzOf (67), 0.3));

    // The project too.
    juce::MemoryBlock project;
    processor->getStateInformation (project);
    auto third = makeSfzProcessor (presets);
    third->setStateInformation (project.getData(), (int) project.getSize());
    REQUIRE (third->waitForSfz());
    CHECK (third->getSfzRef (0) == processor->getSfzRef (0));
    CHECK (third->getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);

    // Undo takes the load back; REMOVE leaves nothing.
    processor->flushUndoStep();
    processor->undo();
    CHECK (processor->getSfzRef (0).isEmpty());
    CHECK (processor->getSfzInstrument (0) == nullptr);
    processor->redo();
    REQUIRE (processor->waitForSfz());
    CHECK (processor->getSfzState (0) == ParagonX1AudioProcessor::SfzState::loaded);
    processor->clearSfz (0);
    CHECK (processor->getSfzState (0) == ParagonX1AudioProcessor::SfzState::none);
    CHECK (processor->getSfzInstrument (0) == nullptr);
    CHECK (processor->captureRig().sfz.empty());
}

TEST_CASE ("SFZ in the plugin: a moved library is found by its folder's name; a missing one says so", "[sfz][plugin]")
{
    Folder f;
    const auto presets = f.dir.getChildFile ("presets");
    presets.createDirectory();
    const auto first = f.dir.getChildFile ("drive1/Grand");
    writeSine (first.getChildFile ("c4.wav"), hzOf (60), 0.2);
    const auto file = writeSfz (first, "<region> sample=c4.wav pitch_keycenter=60\n", "grand.sfz");

    auto processor = makeSfzProcessor (presets);
    processor->loadSfz (0, file);
    REQUIRE (processor->waitForSfz());
    const auto ref = processor->getSfzRef (0);
    CHECK (ref.startsWith ("{sfz:Grand}/grand.sfz"));

    // Moved to another drive and loaded from there once (any instrument in it): the old preset finds it.
    const auto moved = f.dir.getChildFile ("drive2/Grand");
    moved.getParentDirectory().createDirectory();
    REQUIRE (first.moveFileTo (moved));
    CHECK (processor->resolveSfz (ref) == juce::File());   // not yet: that folder isn't known
    processor->loadSfz (1, moved.getChildFile ("grand.sfz"));
    REQUIRE (processor->waitForSfz());
    CHECK (processor->resolveSfz (ref) == moved.getChildFile ("grand.sfz"));

    // A preset naming a library this computer doesn't have: NOT LOADED, with what's missing, and silence.
    Rig rig;
    rig.parameters[ParamIDs::osc (0, "Wave")] = (float) synth::OscWave::sfz;
    rig.sfz[0] = "{sfz:Nowhere}/missing.sfz|/no/such/missing.sfz";
    auto lost = makeSfzProcessor (presets);
    RigLibrary::Entry entry;
    entry.rig = rig;
    lost->loadRig (entry);
    lost->waitForSfz();
    CHECK (lost->getSfzState (0) == ParagonX1AudioProcessor::SfzState::failed);
    CHECK (lost->getSfzProblem (0).contains ("missing"));
    const auto out = playNote (*lost, 60, 0.2);
    CHECK (std::all_of (out.begin(), out.end(), [] (float v) { return v == 0.0f; }));
}

//==============================================================================
// Clips of a real SFZ library for the owner (not run by default): PARAGON_SFZ_FILE the .sfz, PARAGON_RENDER the folder.
// Plays it plainly (no filter, the AMP as a piano's: instant attack, sustain held, a 1 s release), the last clip through
// the synth's FILTER and REVERB. Reports the load time, the memory the heads take, the CPU and late disk reads.
TEST_CASE ("Render SFZ clips", "[.sfzrender]")
{
    const juce::File sfzFile (juce::SystemStats::getEnvironmentVariable ("PARAGON_SFZ_FILE", {}));
    REQUIRE (sfzFile.existsAsFile());
    const auto folder = juce::File (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER", "/tmp/paragon-render"));
    folder.createDirectory();
    Folder presets;

    auto processor = makeSfzProcessor (presets.dir);
    const auto setParam = [&] (const juce::String& id, float value)
    {
        auto* p = processor->getParameters().getParameter (id);
        REQUIRE (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    const auto loadStart = juce::Time::getMillisecondCounterHiRes();
    processor->loadSfz (0, sfzFile);
    REQUIRE (processor->waitForSfz (120000));
    const auto loadSeconds = (juce::Time::getMillisecondCounterHiRes() - loadStart) / 1000.0;
    const auto instrument = processor->getSfzInstrument (0);
    INFO (processor->getSfzProblem (0));
    REQUIRE (instrument != nullptr);
    size_t headBytes = 0;
    for (const auto& s : instrument->samples)
        headBytes += (s->head[0].size() + s->head[1].size()) * sizeof (float);
    WARN (instrument->name << ": " << instrument->regions.size() << " regions, " << instrument->samples.size() << " samples, loaded in "
          << juce::String (loadSeconds, 1) << " s, heads " << juce::String ((double) headBytes / 1.0e6, 0) << " MB; "
          << instrument->problems.joinIntoString ("; "));

    setParam (ParamIDs::osc (0, "Filter"), 0.0f);
    setParam (ParamIDs::ampVel, 0.0f);
    setParam ("amp1Attack", 0.0f);
    setParam ("amp1Sustain", 1.0f);
    setParam ("amp1Release", 1000.0f);
    setParam (ParamIDs::ampReleaseCurve, -1.0f);
    setParam ("voices", 64.0f);
    processor->seedRandomForTest (1);

    struct Event { double beat; int type; int note; float velocity; };   // type 0 on, 1 off, 2 pedal (velocity 0 / 1)
    const auto render = [&] (const juce::String& name, std::vector<Event> events, double bpm, double tailSeconds, bool paced)
    {
        std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.beat < b.beat; });
        const auto secondsPerBeat = 60.0 / bpm;
        const auto total = (int) ((events.back().beat * secondsPerBeat + tailSeconds) * sampleRate);
        processor->setNonRealtime (! paced);
        const auto underrunsBefore = processor->getSfzUnderruns();
        juce::AudioBuffer<float> out (2, total + blockSize), block (2, blockSize);
        size_t next = 0;
        double busy = 0.0;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int at = 0; at < total; at += blockSize)
        {
            juce::MidiBuffer midi;
            while (next < events.size() && (int) (events[next].beat * secondsPerBeat * sampleRate) < at + blockSize)
            {
                const auto& e = events[next++];
                const auto offset = juce::jlimit (0, blockSize - 1, (int) (e.beat * secondsPerBeat * sampleRate) - at);
                midi.addEvent (e.type == 0 ? juce::MidiMessage::noteOn (1, e.note, e.velocity)
                             : e.type == 1 ? juce::MidiMessage::noteOff (1, e.note)
                                           : juce::MidiMessage::controllerEvent (1, 64, e.velocity > 0.5f ? 127 : 0), offset);
            }
            block.clear();
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            processor->processBlock (block, midi);
            busy += juce::Time::getMillisecondCounterHiRes() - t0;
            for (int c = 0; c < 2; ++c)
                out.copyFrom (c, at, block, c, 0, blockSize);
            if (paced)   // as a DAW plays live: a block every 10.7 ms
                while (juce::Time::getMillisecondCounterHiRes() - start < 1000.0 * (at + blockSize) / sampleRate)
                    juce::Thread::sleep (1);
        }
        out.setSize (2, total, true);
        auto peak = 0.0f;
        for (int c = 0; c < 2; ++c)
            peak = juce::jmax (peak, out.getMagnitude (c, 0, total));
        WARN (name << ": peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS, CPU "
              << juce::String (100.0 * busy / (1000.0 * total / sampleRate), 1) << "% of a core"
              << (paced ? ", live: " + juce::String (processor->getSfzUnderruns() - underrunsBefore) + " late samples" : juce::String()));
        const auto file = folder.getChildFile (name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (out, 0, total);
    };
    const auto note = [] (std::vector<Event>& e, double beat, int key, double length, float velocity)
    {
        e.push_back ({ beat, 0, key, velocity });
        e.push_back ({ beat + length, 1, key, 0.0f });
    };
    const auto pedal = [] (std::vector<Event>& e, double down, double up)
    {
        e.push_back ({ down, 2, 64, 1.0f });
        e.push_back ({ up, 2, 64, 0.0f });
    };

    // 1. Velocity: middle C soft to hard, then A from the bottom to the top at a medium touch.
    {
        std::vector<Event> e;
        auto beat = 0.0;
        for (const auto v : { 0.15f, 0.35f, 0.55f, 0.75f, 0.95f })
        {
            note (e, beat, 60, 1.6, v);
            beat += 2.0;
        }
        for (int key = 21; key <= 105; key += 12)
        {
            note (e, beat, key, 1.6, 0.6f);
            beat += 2.0;
        }
        render ("01 Salamander - soft to hard, then low to high", e, 80.0, 3.0, false);
    }
    // 2. Chords with the pedal, changed at each chord (Am F C G), in A minor at 70 BPM.
    const std::vector<std::vector<int>> chords { { 45, 57, 60, 64, 69 }, { 41, 53, 57, 60, 65 }, { 48, 55, 60, 64, 67 }, { 43, 55, 59, 62, 67 } };
    {
        std::vector<Event> e;
        for (int bar = 0; bar < 8; ++bar)
        {
            const auto& chord = chords[(size_t) bar % 4];
            const auto at = bar * 4.0;
            pedal (e, at + 0.1, at + 3.95);
            for (size_t k = 0; k < chord.size(); ++k)
                note (e, at + 0.03 * (double) k, chord[k], k == 0 ? 3.8 : 1.0, k == 0 ? 0.62f : 0.48f);
            note (e, at + 2.0, chord[2] + 12, 1.5, 0.4f);
        }
        render ("02 Salamander - chords with the pedal", e, 70.0, 4.0, false);
    }
    // 3. A melody over a left hand, played live (paced like a DAW, the disk read as it plays: late reads are counted).
    {
        std::vector<Event> e;
        const int melody[] = { 76, 74, 72, 71, 72, 74, 76, 79, 77, 76, 74, 72, 74, 71, 69, 69 };
        const float touch[] = { 0.6f, 0.45f, 0.5f, 0.4f, 0.5f, 0.55f, 0.65f, 0.8f, 0.6f, 0.5f, 0.45f, 0.5f, 0.55f, 0.45f, 0.5f, 0.35f };
        for (int k = 0; k < 16; ++k)
            note (e, k * 1.0, melody[k], k % 4 == 3 ? 1.9 : 0.9, touch[k]);
        for (int bar = 0; bar < 4; ++bar)
        {
            const auto& chord = chords[(size_t) bar];
            pedal (e, bar * 4.0 + 0.05, bar * 4.0 + 3.9);
            for (int k = 0; k < 4; ++k)
                note (e, bar * 4.0 + k, k % 2 == 0 ? chord[0] : chord[1], 0.9, 0.42f);
        }
        render ("03 Salamander - melody, played live", e, 76.0, 4.0, true);
    }
    // 5 (before the filter goes on). A chord held 20 s: a looped sustain holds it; one that isn't ends with its recording.
    {
        std::vector<Event> e;
        for (const auto key : { 48, 55, 60, 64 })
            note (e, 0.0, key, 23.0, 0.7f);   // 23 beats at 70 BPM: about 20 s
        render ("05 a chord held 20 seconds", e, 70.0, 3.0, false);
    }
    // 4. The same chords through the synth: a low-pass opening over each chord (the FILTER's envelope) and the REVERB.
    {
        setParam (ParamIDs::osc (0, "Filter"), 1.0f);
        setParam ("flt1On", 1.0f);
        setParam ("flt1Cutoff", 500.0f);
        setParam ("flt1Env", 0.6f);
        setParam ("flt1Attack", 1200.0f);
        setParam ("flt1Decay", 2500.0f);
        setParam ("flt1Sustain", 0.3f);
        setParam ("reverbOn", 1.0f);
        setParam ("reverbMix", 0.35f);
        std::vector<Event> e;
        for (int bar = 0; bar < 4; ++bar)
        {
            const auto& chord = chords[(size_t) bar];
            pedal (e, bar * 4.0 + 0.1, bar * 4.0 + 3.95);
            for (size_t k = 0; k < chord.size(); ++k)
                note (e, bar * 4.0 + 0.03 * (double) k, chord[k], 3.8, 0.55f);
        }
        render ("04 Salamander - through the filter and reverb", e, 70.0, 5.0, false);
    }
}

//==============================================================================
// 0.75.0: THE MOD WHEEL, FILTERS and KEY SWITCHES, as orchestral libraries' "performance" patches use them.
namespace
{
    juce::MidiMessage wheelAt (int value) { return juce::MidiMessage::controllerEvent (1, 1, value); }
}

TEST_CASE ("SFZ: the mod wheel crossfades layers (XFIN / XFOUT_LOCC1) live while the note holds, without a click", "[sfz]")
{
    // A soft layer at 480 Hz fading out from the wheel at 0 to 64, a loud one at 720 Hz fading in across the same
    // (XF_CCCURVE=gain, as the orchestra library sets it).
    Folder f;
    writeSine (f.dir.getChildFile ("soft.wav"), 480.0, 2.0);
    writeSine (f.dir.getChildFile ("loud.wav"), 720.0, 2.0);
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<group> key=60 amp_veltrack=0 xf_cccurve=gain\n"
        "<region> sample=soft.wav xfout_locc1=0 xfout_hicc1=64\n"
        "<region> sample=loud.wav xfin_locc1=0 xfin_hicc1=64\n"));
    REQUIRE (inst != nullptr);
    CHECK (inst->wheelDynamics);
    CHECK (! inst->problems.joinIntoString (";").contains ("xfin_locc1"));
    const auto levels = [&] (int wheel)
    {
        SfzPlayer p (inst.get());
        p.midi (wheelAt (wheel));
        p.render (0.01);
        p.on (60);
        const auto out = p.render (0.3);
        return std::make_pair (levelAt (out[0], 480.0, 960, 9600) / 0.5, levelAt (out[0], 720.0, 960, 9600) / 0.5);
    };
    auto [soft, loud] = levels (0);
    CHECK_THAT (soft, WithinAbs (1.0, 0.01));
    CHECK (loud < 0.001);
    std::tie (soft, loud) = levels (127);
    CHECK (soft < 0.001);
    CHECK_THAT (loud, WithinAbs (1.0, 0.01));
    std::tie (soft, loud) = levels (32);   // halfway through the fade: each at half (straight, XF_CCCURVE=gain)
    CHECK_THAT (soft, WithinAbs (0.5, 0.01));
    CHECK_THAT (loud, WithinAbs (0.5, 0.01));

    // Live: the note starts soft; moving the wheel up while it holds brings the loud layer in (both were started).
    SfzPlayer p (inst.get());
    p.midi (wheelAt (0));
    p.render (0.01);
    p.on (60);
    const auto before = p.render (0.3);
    p.midi (wheelAt (127));
    const auto after = p.render (0.3);
    CHECK (levelAt (before[0], 720.0, 960, 9600) < 0.001);
    CHECK_THAT (levelAt (after[0], 720.0, 2400, 9600) / 0.5, WithinAbs (1.0, 0.01));
    CHECK (levelAt (after[0], 480.0, 2400, 9600) < 0.001);
    // No step: the change glides across a block, so no sample jumps further than the two sines can move.
    auto biggest = 0.0f;
    for (size_t i = 1; i < 2048; ++i)
        biggest = juce::jmax (biggest, std::abs (after[0][i] - after[0][i - 1]));
    const auto sineStep = 0.5f * (float) (juce::MathConstants<double>::twoPi * 720.0 / sampleRate);
    CHECK (biggest < 1.2f * sineStep);
}

TEST_CASE ("SFZ: GAIN_CC1 raises the level with the wheel; SET_CC1 stands for the wheel until it's moved", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("tone.wav"), 480.0, 1.0);
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<control> set_cc1=127\n"
        "<region> sample=tone.wav key=60 amp_veltrack=0 volume=-12 gain_cc1=12\n"));
    REQUIRE (inst != nullptr);
    CHECK (inst->setCc1 == 127);
    const auto levelWith = [&] (int wheel)
    {
        SfzPlayer p (inst.get());
        if (wheel >= 0)
        {
            p.midi (wheelAt (wheel));
            p.render (0.01);
        }
        p.on (60);
        const auto out = p.render (0.3);
        return db (levelAt (out[0], 480.0, 960, 9600) / 0.5);
    };
    CHECK_THAT (levelWith (0), WithinAbs (-12.0, 0.05));
    CHECK_THAT (levelWith (127), WithinAbs (0.0, 0.05));
    CHECK_THAT (levelWith (64), WithinAbs (-12.0 + 12.0 * 64.0 / 127.0, 0.05));   // dB in proportion
    CHECK_THAT (levelWith (-1), WithinAbs (0.0, 0.05));   // untouched: SET_CC1 (127)
}

TEST_CASE ("SFZ: FIL_TYPE low- and high-pass at CUTOFF, moved by FIL_VELTRACK, FIL_KEYTRACK and CUTOFF_CC1", "[sfz]")
{
    Folder f;
    writeSine (f.dir.getChildFile ("hi.wav"), 4000.0, 1.0);
    writeSine (f.dir.getChildFile ("lo.wav"), 250.0, 1.0);
    const auto cut = [&] (const juce::String& sample, double hz, const juce::String& filter, float velocity = 1.0f, int wheel = 0, int key = 60)
    {
        const auto inst = synth::sfz::load (writeSfz (f.dir,
            "<region> sample=" + sample + " lokey=0 hikey=127 pitch_keycenter=" + juce::String (key) + " amp_veltrack=0 " + filter + "\n",
            "f" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".sfz"));
        REQUIRE (inst != nullptr);
        SfzPlayer p (inst.get());
        p.midi (wheelAt (wheel));
        p.render (0.01);
        p.on (key, velocity);
        const auto out = p.render (0.4);
        return db (levelAt (out[0], hz, 4800, 9600) / 0.5);
    };
    // Two octaves above a 1 kHz cutoff: a one-pole falls about 12 dB, a two-pole (Butterworth) about 24.
    CHECK_THAT (cut ("hi.wav", 4000.0, "fil_type=lpf_1p cutoff=1000"), WithinAbs (-12.3, 0.6));
    CHECK_THAT (cut ("hi.wav", 4000.0, "fil_type=lpf_2p cutoff=1000"), WithinAbs (-24.1, 0.6));
    CHECK_THAT (cut ("hi.wav", 4000.0, "cutoff=1000"), WithinAbs (-24.1, 0.6));   // lpf_2p is the default
    // Two octaves below: the high-passes.
    CHECK_THAT (cut ("lo.wav", 250.0, "fil_type=hpf_1p cutoff=1000"), WithinAbs (-12.3, 0.6));
    CHECK_THAT (cut ("lo.wav", 250.0, "fil_type=hpf_2p cutoff=1000"), WithinAbs (-24.1, 0.6));
    // At the cutoff a Butterworth two-pole is 3 dB down; RESONANCE 6 dB lifts it to +3.
    CHECK_THAT (cut ("hi.wav", 4000.0, "cutoff=4000"), WithinAbs (-3.0, 0.2));
    CHECK_THAT (cut ("hi.wav", 4000.0, "cutoff=4000 resonance=6"), WithinAbs (3.0, 0.2));
    // FIL_VELTRACK 2400 cents (two octaves) from 1 kHz: full velocity opens it to 4 kHz (-3 dB), half to 2 kHz.
    CHECK_THAT (cut ("hi.wav", 4000.0, "cutoff=1000 fil_veltrack=2400"), WithinAbs (-3.0, 0.2));
    CHECK (cut ("hi.wav", 4000.0, "cutoff=1000 fil_veltrack=2400", 0.5f) < -10.0);
    // CUTOFF_CC1: the wheel at the top opens it two octaves; at the bottom it's as set.
    CHECK_THAT (cut ("hi.wav", 4000.0, "cutoff=1000 cutoff_cc1=2400", 1.0f, 127), WithinAbs (-3.0, 0.2));
    CHECK_THAT (cut ("hi.wav", 4000.0, "cutoff=1000 cutoff_cc1=2400", 1.0f, 0), WithinAbs (-24.1, 0.6));
    // FIL_KEYTRACK 100: a key two octaves above FIL_KEYCENTER moves the cutoff up two octaves with it.
    CHECK_THAT (cut ("hi.wav", 4000.0, "cutoff=1000 fil_keytrack=100 fil_keycenter=36", 1.0f, 0, 60), WithinAbs (-3.0, 0.2));
    // No CUTOFF: no filter at all.
    CHECK_THAT (cut ("hi.wav", 4000.0, "fil_type=lpf_2p"), WithinAbs (0.0, 0.05));
}

TEST_CASE ("SFZ: key switches (SW_LOKEY / SW_HIKEY / SW_LAST / SW_DEFAULT / SW_LABEL) pick the regions, and play nothing", "[sfz]")
{
    // Two playing styles of one key: SUSTAIN (480 Hz) on C1, STACCATO (720 Hz) on D1, as the orchestra's KS patches
    // put them (a <global> per style round an #include).
    Folder f;
    writeSine (f.dir.getChildFile ("sus.wav"), 480.0, 1.0);
    writeSine (f.dir.getChildFile ("stac.wav"), 720.0, 1.0);
    f.dir.getChildFile ("sus.sfz").replaceWithText ("<control> set_cc1=96\n<group> amp_veltrack=0\n<region> sample=sus.wav lokey=50 hikey=70 pitch_keycenter=60\n");
    f.dir.getChildFile ("stac.sfz").replaceWithText ("<group> amp_veltrack=0\n<region> sample=stac.wav lokey=50 hikey=70 pitch_keycenter=60\n");
    const auto inst = synth::sfz::load (writeSfz (f.dir,
        "<global> sw_default=c1 sw_lokey=c1 sw_hikey=d1 sw_last=c1 sw_label=Sustain\n#include \"sus.sfz\"\n"
        "<global> sw_default=c1 sw_lokey=c1 sw_hikey=d1 sw_last=d1 sw_label=Staccato\n#include \"stac.sfz\"\n", "ks.sfz"));
    REQUIRE (inst != nullptr);
    CHECK (inst->switchLo == 24);
    CHECK (inst->switchHi == 26);
    CHECK (inst->switchDefault == 24);
    CHECK (inst->switchLabel (24) == "Sustain");
    CHECK (inst->switchLabel (26) == "Staccato");
    CHECK (inst->isSwitch (26));
    CHECK (! inst->isSwitch (60));
    CHECK (inst->setCc1 == 96);   // <control> from the included file

    SfzPlayer p (inst.get());
    // A plain saw on oscillator 2 too: a switch key must not play it either.
    p.osc (1).on = true;
    p.osc (1).wave = synth::OscWave::saw;
    p.osc (1).volume = 0.0f;   // (silent here; switched on below)
    const auto toneOf = [&] (int note)
    {
        p.on (note);
        const auto out = p.render (0.3);
        p.off (note);
        p.render (0.3);
        return std::make_pair (levelAt (out[0], 480.0, 960, 9600) / 0.5, levelAt (out[0], 720.0, 960, 9600) / 0.5);
    };
    auto [sus, stac] = toneOf (60);   // SW_DEFAULT: Sustain
    CHECK_THAT (sus, WithinAbs (1.0, 0.02));
    CHECK (stac < 0.001);
    CHECK (p.engine.getSfzSwitch (0) == 24);
    p.osc (1).volume = 1.0f;
    p.on (26);   // D1: the switch to Staccato. Nothing sounds, the saw neither.
    const auto pressed = p.render (0.2);
    p.off (26);
    p.render (0.1);
    CHECK (rms (pressed[0], 0, pressed[0].size()) < 1.0e-6);
    CHECK (p.engine.getSfzSwitch (0) == 26);
    p.osc (1).volume = 0.0f;
    std::tie (sus, stac) = toneOf (60);
    CHECK (sus < 0.001);
    CHECK_THAT (stac, WithinAbs (1.0, 0.02));
}

// The real orchestra (0.75.0): PARAGON_SSO=<the library's "Sonatina Symphonic Orchestra" folder>. Its performance
// patches with the wheel low, middling and high: the level and the brightness (the share of power above 2 kHz).
TEST_CASE ("SFZ: the orchestra's performance patches follow the wheel (report)", "[.ssocheck]")
{
    const juce::File folder (juce::SystemStats::getEnvironmentVariable ("PARAGON_SSO", {}));
    REQUIRE (folder.isDirectory());
    for (const auto* name : { "Strings - Performance/1st Violins Sustain.sfz", "Brass - Performance/Trumpets Sustain.sfz",
                              "Brass - Performance/Horns KS.sfz", "Woodwinds - Performance/Alto Flute Solo Sustain (looped).sfz",
                              "Strings - Performance/All Strings KS.sfz", "Brass - Notation/Trumpets Sustain (looped).sfz",
                              "Concert Harp.sfz" })
    {
        const auto inst = synth::sfz::load (folder.getChildFile (name));
        REQUIRE (inst != nullptr);
        juce::String line = juce::String (name) + ": " + juce::String ((int) inst->regions.size()) + " regions, problems: "
                          + inst->problems.joinIntoString ("; ");
        if (! inst->switchLabels.empty())
        {
            line << "\n   switches:";
            for (const auto& [k, label] : inst->switchLabels)
                line << " " << juce::MidiMessage::getMidiNoteName (k, true, true, 4) << "=" << label;
        }
        for (const auto wheel : { 20, 64, 127 })
        {
            SfzPlayer p (inst.get());
            p.midi (wheelAt (wheel));
            p.render (0.01);
            p.on (inst->isSwitch (60) ? 72 : 60, 0.8f);
            auto out = p.render (1.5);
            const auto level = db (rms (out[0], 24000, out[0].size()));
            // Brightness: the power left after a 2 kHz high-pass (two one-poles), against the whole.
            double hp = 0.0, total = 0.0, z1 = 0.0, z2 = 0.0, x1 = 0.0, y1 = 0.0;
            const auto a = std::exp (-juce::MathConstants<double>::twoPi * 2000.0 / sampleRate);
            for (size_t i = 24000; i < out[0].size(); ++i)
            {
                const auto x = (double) out[0][i];
                const auto lp1 = (1.0 - a) * x + a * z1; z1 = lp1;
                const auto h1 = x - lp1;
                const auto lp2 = (1.0 - a) * h1 + a * z2; z2 = lp2;
                const auto h2 = h1 - lp2;
                hp += h2 * h2;
                total += x * x;
                juce::ignoreUnused (x1, y1);
            }
            line << "\n   wheel " << wheel << ": " << juce::String (level, 1) << " dB, above 2 kHz " << juce::String (100.0 * hp / juce::jmax (1e-12, total), 1) << "%";
        }
        WARN (line);
    }
}

// Demo clips (0.75.0): PARAGON_SSO=<the library's folder>, PARAGON_RENDER=<where to write>. Dry (no reverb), as the
// engine plays them: the strings swelling and falling with the wheel, the horns through their key-switched styles,
// a trumpet soft then loud.
TEST_CASE ("SFZ: dynamics and key switch demo clips (render)", "[.dynamicsdemo]")
{
    const juce::File folder (juce::SystemStats::getEnvironmentVariable ("PARAGON_SSO", {}));
    const juce::File out (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER", {}));
    REQUIRE (folder.isDirectory());
    out.createDirectory();
    const auto write = [&] (const juce::String& name, const std::vector<float>& l, const std::vector<float>& r, float gain = 1.0f)
    {
        juce::AudioBuffer<float> buffer (2, (int) l.size());
        buffer.copyFrom (0, 0, l.data(), (int) l.size(), gain);
        buffer.copyFrom (1, 0, r.data(), (int) r.size(), gain);
        const auto file = out.getChildFile (name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    };
    struct Clip { std::vector<float> l, r; void add (const std::array<std::vector<float>, 2>& a) { l.insert (l.end(), a[0].begin(), a[0].end()); r.insert (r.end(), a[1].begin(), a[1].end()); } };
    const auto setUp = [] (SfzPlayer& p)
    {
        p.settings.amp.envelope.attackMs = 2.0f;
        p.settings.amp.envelope.releaseMs = 700.0f;
        p.settings.play.voices = 32;
    };

    {   // 1. Strings: a held chord, the wheel up over four seconds and back down.
        const auto inst = synth::sfz::load (folder.getChildFile ("Strings - Performance/All Strings KS.sfz"));
        REQUIRE (inst != nullptr);
        SfzPlayer p (inst.get());
        setUp (p);
        Clip c;
        p.midi (wheelAt (8));
        c.add (p.render (0.05));
        for (const auto n : { 50, 53, 57, 62 })
            p.on (n, 0.8f);
        for (int step = 0; step <= 160; ++step)   // 8 s in 50 ms steps: up to the top at 4 s, back down by 8
        {
            const auto t = step / 160.0;
            p.midi (wheelAt (juce::roundToInt (8.0 + 119.0 * std::sin (juce::MathConstants<double>::pi * t))));
            c.add (p.render (0.05));
        }
        for (const auto n : { 50, 53, 57, 62 })
            p.off (n);
        c.add (p.render (2.0));
        write ("1 Strings - the wheel swells and falls", c.l, c.r, 0.22f);   // (four sections at the top: no limiter here)
    }
    {   // 2. Horns: one phrase in each playing style, chosen by its key switch.
        const auto inst = synth::sfz::load (folder.getChildFile ("Brass - Performance/Horns KS.sfz"));
        REQUIRE (inst != nullptr);
        SfzPlayer p (inst.get());
        setUp (p);
        Clip c;
        p.midi (wheelAt (100));
        c.add (p.render (0.05));
        const int phrase[] = { 50, 53, 57, 55, 53, 50 };
        const double lengths[] = { 0.45, 0.45, 0.9, 0.45, 0.45, 1.2 };
        for (const auto style : { 84, 86, 88 })   // C6 SUSTAIN, D6 MARCATO, E6 STACCATO
        {
            p.on (style, 0.8f);
            p.off (style);
            for (size_t k = 0; k < 6; ++k)
            {
                p.on (phrase[k], 0.85f);
                c.add (p.render (lengths[k] * 0.92));
                p.off (phrase[k]);
                c.add (p.render (lengths[k] * 0.08));
            }
            c.add (p.render (0.8));
        }
        c.add (p.render (1.0));
        write ("2 Horns - sustain, marcato, staccato by key switch", c.l, c.r);
    }
    {   // 3. Trumpets: a note soft (wheel low), then loud (wheel high): another recording, brighter, not just louder.
        const auto inst = synth::sfz::load (folder.getChildFile ("Brass - Performance/Trumpets Sustain.sfz"));
        REQUIRE (inst != nullptr);
        SfzPlayer p (inst.get());
        setUp (p);
        Clip c;
        for (const auto wheel : { 25, 70, 120 })
        {
            p.midi (wheelAt (wheel));
            c.add (p.render (0.05));
            p.on (62, 0.8f);
            c.add (p.render (2.2));
            p.off (62);
            c.add (p.render (0.9));
        }
        write ("3 Trumpets - wheel low, middle, high", c.l, c.r, 1.8f);
    }
}
