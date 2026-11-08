#pragma once

#include "dsp/synth/SfzStreamer.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// THE SFZ OSCILLATOR (0.71.0, the owner's ask: a sampled piano such as the Salamander Grand, played through the
// whole synth). An SFZ file (an open, plain-text format many multi-sampled instruments come in) is a list of REGIONS:
// a sample, the keys and velocities it plays, the key it was recorded at, its tuning and level. Ours reads it as
// PR7-D 0.19.0 does (its reader ported: comments, #define, #include, <control> <global> <master> <group> <region>
// inheritance) and adds what a melodic instrument needs and drums didn't:
//
//   - PITCH: a region plays every key in its range, at the speed that puts its PITCH_KEYCENTER on the key played
//     (PITCH_KEYTRACK cents a key, 100 by default), plus TUNE and TRANSPOSE. The oscillator's own pitch (TRANSPOSE,
//     FINE, the pitch wheel, glide, vibrato, the PITCH envelope) moves it the same way.
//   - RELEASE SAMPLES (TRIGGER=RELEASE): played when the note lets go (after the pedal), quieter the longer the
//     note was held (RT_DECAY, dB a second).
//   - VELOCITY: LOVEL / HIVEL choose the layer; AMP_VELTRACK (100% by default) makes softer notes quieter inside it.
//   - LOCC64 / HICC64: regions that play only with the sustain pedal down (or up), as some pianos' are.
//   - ROUND-ROBINS: SEQ_LENGTH / SEQ_POSITION take turns; LORAND / HIRAND by chance.
//   - VOLUME (dB), AMPLITUDE (%), PAN, OFFSET (samples into the file); (0.75.0) GROUP_VOLUME, MASTER_VOLUME and
//     GLOBAL_VOLUME add to VOLUME, GROUP_TUNE to TUNE.
//   - AMPEG_RELEASE: only to let a sample ring on past the AMP's release (undamped strings); the AMP shapes the rest.
//   - ON_LOCC64 / ON_HICC64: the sustain pedal's own sounds, played as it goes down or up.
//   - LOOPS (0.73.0): LOOP_MODE loop_continuous / loop_sustain (looping until the key lets go) between LOOP_START and
//     LOOP_END (inclusive), else the file's own loop (a WAV's "smpl" chunk, which loops unless LOOP_MODE says no);
//     no_loop and one_shot play straight through. A looped sample is kept whole in memory (looped orchestral
//     sustains are a few seconds), not streamed.
//   - VELOCITY CROSSFADES (0.73.0): XFIN_LOVEL / XFIN_HIVEL fade a region in across those velocities, XFOUT_LOVEL /
//     XFOUT_HIVEL out; XF_VELCURVE power (equal power, the default) or gain (straight).
//   - THE MOD WHEEL (0.75.0, for orchestral libraries' "performance" patches): XFIN_LOCC1 / XFIN_HICC1 /
//     XFOUT_LOCC1 / XFOUT_HICC1 fade a region in and out across the wheel (XF_CCCURVE power, the default, or gain),
//     live while the note holds: every layer of a note starts together and the wheel changes their mix; GAIN_CC1
//     adds that many dB at the top (in proportion below); SET_CC1 is where the wheel stands until it's first moved.
//   - FILTERS (0.75.0): FIL_TYPE lpf_1p / lpf_2p (the default) / hpf_1p / hpf_2p at CUTOFF (Hz), moved by
//     FIL_KEYTRACK (cents a key from FIL_KEYCENTER), FIL_VELTRACK (cents at full velocity), CUTOFF_CC1 (cents with
//     the wheel at the top) and RESONANCE (dB): soft notes darker, as the library's makers set them.
//   - KEY SWITCHES (0.75.0): SW_LOKEY..SW_HIKEY are switch keys, not notes; a region with SW_LAST plays only while
//     its key was the last switch pressed (SW_DEFAULT before any); SW_LABEL names it.
// Not read: the other envelope stages, other controllers (crossfades, gains and triggers but the wheel's and the
// pedal's), OFF_BY / GROUP.
//
// Every sample keeps its first quarter of a second in memory (a looped one all of it); the rest is streamed from disk
// (SfzStreamer.h).
namespace synth::sfz
{
    struct Sample
    {
        juce::File file;
        double sampleRate = 48000.0;
        juce::int64 length = 0;                 // frames in the file
        std::vector<float> head[2];             // the first `headFrames` (mono files: both the same)
        int headFrames = 0;                     // the whole file when a region loops it
        juce::int64 fileLoopStart = -1, fileLoopEnd = -1;   // the file's own loop (a WAV's smpl chunk), -1: none
        std::unique_ptr<StreamSource> stream;   // the rest, from disk (null: the head is the whole file)
    };

    struct Region
    {
        int sample = -1;                        // into Instrument::samples
        int loKey = 0, hiKey = 127, loVel = 1, hiVel = 127;
        float keyCenter = 60.0f;                // the key it was recorded at
        float keytrack = 1.0f;                  // semitones of speed a key (PITCH_KEYTRACK / 100)
        float tuneSemitones = 0.0f;             // TUNE + TRANSPOSE
        float gain = 1.0f;                      // VOLUME and AMPLITUDE
        float pan = 0.0f;                       // -1 to 1
        int offset = 0;                         // frames into the file
        bool release = false;                   // TRIGGER=RELEASE
        float rtDecayDb = 0.0f;                 // dB a second of the note held, off a release sample
        float velTrack = 1.0f;                  // AMP_VELTRACK / 100
        // AMP_VELCURVE_N (0.81.0, Sam's Sonor's layers): the level at velocity N (0-127), straight lines between the
        // points, (0, 0) and (127, 1) where not given; when there are any, they replace AMP_VELTRACK. Kept as a
        // vector (a 128-point table inline would grow every region).
        std::vector<std::pair<int, float>> velCurve;
        int seqLength = 1, seqPosition = 1;
        float loRand = 0.0f, hiRand = 1.0f;
        int loPedal = 0, hiPedal = 127;         // LOCC64 / HICC64
        float releaseSeconds = -1.0f;           // AMPEG_RELEASE (-1: not given): longer than the AMP's, the sample
                                                // rings on after the key lets go (a piano's undamped top strings)
        bool pedalTrigger = false;              // ON_LOCC64 / ON_HICC64: played when the sustain pedal moves into
        int onLo = 0, onHi = -1;                // ...this range (the pedal's own sound), not by a key
        enum class Loop : std::uint8_t { none, continuous, sustain };
        Loop loop = Loop::none;                 // (0.73.0) loop_sustain stops looping when the key lets go
        juce::int64 loopStart = 0, loopEnd = 0; // frames, LOOP_END inclusive
        int xfinLo = 0, xfinHi = 0, xfoutLo = 127, xfoutHi = 127;   // velocity crossfades (0.73.0)
        bool xfPower = true;                    // XF_VELCURVE: equal power, else straight

        // THE MOD WHEEL (0.75.0): its crossfades and GAIN_CC1.
        int ccInLo = 0, ccInHi = 0, ccOutLo = 127, ccOutHi = 127;
        bool ccPower = true;                    // XF_CCCURVE: equal power, else straight
        float gainCcDb = 0.0f;                  // GAIN_CC1: dB added with the wheel at the top
        // FILTER (0.75.0).
        enum class Filter { none, lowpass1, lowpass2, highpass1, highpass2 };
        Filter filter = Filter::none;
        float cutoff = 0.0f;                    // Hz
        float cutoffCcCents = 0.0f, filterKeytrack = 0.0f, filterKeyCenter = 60.0f, filterVeltrack = 0.0f;
        float filterQ = 0.70710678f;            // from RESONANCE (dB)
        int switchKey = -1;                     // SW_LAST (-1: plays whatever switch is down)
        int switchKeyHi = -1;                   // (0.84.0) SW_HILAST: with SW_LOLAST a range of switches (switchKey is its bottom)

        bool loops() const noexcept { return loop != Loop::none && loopEnd > loopStart; }
        // A crossfade's level for a value: 1 outside both fades, 0 below the fade in or above the fade out.
        static float fadeLevel (float value, float inLo, float inHi, float outLo, float outHi, bool power) noexcept
        {
            auto in = 1.0f, out = 1.0f;
            if (inHi > inLo)
                in = juce::jlimit (0.0f, 1.0f, (value - inLo) / (inHi - inLo));
            if (outHi > outLo)
                out = juce::jlimit (0.0f, 1.0f, (outHi - value) / (outHi - outLo));
            return power ? std::sqrt (in * out) : in * out;
        }
        // The wheel's part in the level (0.75.0), `cc127` 0-127: its crossfades x GAIN_CC1. 1 for a region with neither.
        float wheelLevel (float cc127) const noexcept
        {
            auto level = fadeLevel (cc127, (float) ccInLo, (float) ccInHi, (float) ccOutLo, (float) ccOutHi, ccPower);
            if (gainCcDb != 0.0f)
                level *= std::pow (10.0f, gainCcDb * cc127 / 127.0f / 20.0f);
            return level;
        }
        bool fadesWithWheel() const noexcept { return ccInHi > ccInLo || ccOutHi > ccOutLo || gainCcDb != 0.0f; }
        // The filter's cutoff for a note (Hz), before it's kept under the output's Nyquist.
        float cutoffFor (int key, float velocity, float cc127) const noexcept
        {
            const auto cents = filterKeytrack * ((float) key - filterKeyCenter) + filterVeltrack * velocity + cutoffCcCents * cc127 / 127.0f;
            return cutoff * std::exp2 (cents / 1200.0f);
        }
        // The crossfades' level for a velocity (1-127): 1 outside both fades; 0 below XFIN_LOVEL or above XFOUT_HIVEL.
        float crossfade (int velocity127) const noexcept
        {
            auto in = 1.0f, out = 1.0f;
            if (velocity127 <= xfinLo && xfinHi > xfinLo)
                in = 0.0f;
            else if (velocity127 < xfinHi)
                in = (float) (velocity127 - xfinLo) / (float) (xfinHi - xfinLo);
            if (velocity127 >= xfoutHi && xfoutHi > xfoutLo)
                out = 0.0f;
            else if (velocity127 > xfoutLo)
                out = (float) (xfoutHi - velocity127) / (float) (xfoutHi - xfoutLo);
            return xfPower ? std::sqrt (in * out) : in * out;
        }

        bool plays (int key, int velocity) const noexcept { return key >= loKey && key <= hiKey && velocity >= loVel && velocity <= hiVel; }
        // A note's level from its velocity (0-1) as AMP_VELTRACK says: at 100%, (velocity)^2 (SFZ's curve).
        float velocityGain (float velocity) const noexcept
        {
            if (velCurve.empty())
                return 1.0f + velTrack * (velocity * velocity - 1.0f);
            const auto v = juce::jlimit (0.0f, 127.0f, velocity * 127.0f);
            const auto between = [v] (float x0, float y0, float x1, float y1)
            {
                return x1 <= x0 ? y1 : y0 + (y1 - y0) * (v - x0) / (x1 - x0);
            };
            auto x0 = 0.0f, y0 = 0.0f;   // (0, 0) unless the curve gives velocity 0
            for (const auto& [x, y] : velCurve)
            {
                if (v <= (float) x)
                    return between (x0, y0, (float) x, y);
                x0 = (float) x;
                y0 = y;
            }
            return between (x0, y0, 127.0f, x0 >= 127.0f ? y0 : 1.0f);   // to (127, 1) unless given
        }
    };

    struct Instrument
    {
        juce::String name;
        juce::File file;
        std::vector<Region> regions;
        std::vector<std::unique_ptr<Sample>> samples;
        juce::StringArray problems;             // files that didn't load, opcodes left out...
        bool streamed = false;                  // some sample plays past its head (needs a Streamer)
        bool hasRelease = false;                // some region is TRIGGER=RELEASE
        bool hasPedalSounds = false;            // some region plays when the pedal moves
        double longestSeconds = 0.0;            // the longest sample (how long a note can read it)
        // (0.82.1) The engine's last check that found a note reading this instrument (Engine::markSfzInUse): a retired
        // instrument is freed only once a later check has found none (a looped note can read it for ever).
        mutable std::atomic<std::uint64_t> lastUsedCheck { 0 };
        int setCc1 = -1;                        // SET_CC1 (0.75.0): the wheel until it's moved (-1: none given)
        bool wheelDynamics = false;             // some region fades or swells with the wheel, or opens a filter with it
        // KEY SWITCHES (0.75.0): the switch keys (none: -1), the one down before any is pressed, and each one's name.
        int switchLo = -1, switchHi = -1, switchDefault = -1;
        std::vector<std::pair<int, juce::String>> switchLabels;
        bool isSwitch (int key) const noexcept { return switchLo >= 0 && key >= switchLo && key <= switchHi && (key < loKey || key > hiKey); }
        juce::String switchLabel (int key) const
        {
            for (const auto& [k, label] : switchLabels)
                if (k == key)
                    return label;
            return {};
        }
        int loKey = 127, hiKey = 0;             // the keys it plays

        // The regions a note plays: attack regions for a note-on, release regions for its release. `seqCount` is
        // the oscillator's turn counter for this key (round-robins), `chance` 0-1 (LORAND / HIRAND), `pedal` CC 64.
        // `switchKey` (0.75.0) the last key switch pressed: regions with another SW_LAST don't play.
        int select (int key, int velocity127, bool release, int seqCount, float chance, int pedal,
                    const Region** out, int capacity, int switchKey = -1) const noexcept;
        // The regions the sustain pedal plays moving to `pedal` (0-127).
        int selectPedal (int pedal, float chance, const Region** out, int capacity) const noexcept;
    };

    // Reads an SFZ file and every sample's head (on a background thread: a big piano takes seconds). `progress`
    // (0-1) and `cancel` are optional. Null if the file can't be read or plays nothing.
    std::shared_ptr<const Instrument> load (const juce::File& sfzFile, std::function<void (float)> progress = {},
                                            const std::atomic<bool>* cancel = nullptr);

    inline constexpr double headSeconds = 0.25;
    inline constexpr int maxPlayers = 4;        // regions a note plays at once on one oscillator
}
