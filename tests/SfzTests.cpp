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
