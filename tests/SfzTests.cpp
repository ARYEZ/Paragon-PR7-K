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
