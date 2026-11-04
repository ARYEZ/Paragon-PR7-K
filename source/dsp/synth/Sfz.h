#pragma once

#include "dsp/synth/SfzStreamer.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
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
//   - VOLUME (dB), AMPLITUDE (%), PAN, OFFSET (samples into the file).
//   - AMPEG_RELEASE: only to let a sample ring on past the AMP's release (undamped strings); the AMP shapes the rest.
//   - ON_LOCC64 / ON_HICC64: the sustain pedal's own sounds, played as it goes down or up.
// Not read: filters, the other envelope stages, loops, keyswitches, other controllers' triggers, OFF_BY / GROUP.
//
// Every sample keeps its first quarter of a second in memory; the rest is streamed from disk (SfzStreamer.h).
namespace synth::sfz
{
    struct Sample
    {
        juce::File file;
        double sampleRate = 48000.0;
        juce::int64 length = 0;                 // frames in the file
        std::vector<float> head[2];             // the first `headFrames` (mono files: both the same)
        int headFrames = 0;
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
        int seqLength = 1, seqPosition = 1;
        float loRand = 0.0f, hiRand = 1.0f;
        int loPedal = 0, hiPedal = 127;         // LOCC64 / HICC64
        float releaseSeconds = -1.0f;           // AMPEG_RELEASE (-1: not given): longer than the AMP's, the sample
                                                // rings on after the key lets go (a piano's undamped top strings)
        bool pedalTrigger = false;              // ON_LOCC64 / ON_HICC64: played when the sustain pedal moves into
        int onLo = 0, onHi = -1;                // ...this range (the pedal's own sound), not by a key

        bool plays (int key, int velocity) const noexcept { return key >= loKey && key <= hiKey && velocity >= loVel && velocity <= hiVel; }
        // A note's level from its velocity (0-1) as AMP_VELTRACK says: at 100%, (velocity)^2 (SFZ's curve).
        float velocityGain (float velocity) const noexcept { return 1.0f + velTrack * (velocity * velocity - 1.0f); }
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
        int loKey = 127, hiKey = 0;             // the keys it plays

        // The regions a note plays: attack regions for a note-on, release regions for its release. `seqCount` is
        // the oscillator's turn counter for this key (round-robins), `chance` 0-1 (LORAND / HIRAND), `pedal` CC 64.
        int select (int key, int velocity127, bool release, int seqCount, float chance, int pedal,
                    const Region** out, int capacity) const noexcept;
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
