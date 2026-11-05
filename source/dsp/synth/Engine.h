#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Noise.h"
#include "SynthSettings.h"
#include "OscModulation.h"
#include "WaveBend.h"
#include "VoiceFilter.h"
#include "Sfz.h"

#include <atomic>

#include <array>
#include <vector>

// PR7-K's synth engine (stage 1): MIDI in, stereo out. Our own design,
// laid out like Avenger's: up to 8 oscillators per note, each through the
// FILTER (or not) and up to 4 SHAPERs (each before or after the filter), then the AMP.
//
// Where the CPU goes, and how little of it this uses:
//   - Every oscillator reads band-limited tables (WaveTables.h), so nothing
//     is oversampled.
//   - An oscillator that's switched off costs nothing; so does a voice with no
//     note, and a sub, noise or drive turned to zero.
//   - A note ends the moment it can't be heard: when its release has fallen
//     below -90 dB (Avenger's manual warns that its barely audible tails
//     "still use cpu"), when it has decayed to a SUSTAIN of zero, or when
//     every oscillator in it has been taken by a newer note.
//   - Slow things (the filter's envelope and cutoff, glide, pitch bend, the
//     tables' choice) are worked out once per 16 samples (a third of a
//     millisecond at 48 kHz) and glided between, rather than every sample.
//
// Audio thread only, apart from prepare().
namespace synth
{
    class Engine
    {
    public:
        Engine();

        void prepare (double sampleRate);

        // Its random choices (start phases, RANDOM pitch, unison copies'
        // vibrato speeds...) from a fixed start, so a test hears the same
        // every run. The plugin leaves them seeded from the clock.
        void seedRandom (juce::int64 seed) noexcept { random.setSeed (seed); glowRandom = (std::uint32_t) seed * 2654435761u + 12345u; }
        void reset() noexcept;   // silence at once, every note gone

        // SFZ (0.71.0): the disk streamer for samples longer than their heads (the processor's; null: heads only),
        // and whether to wait for the disk (bouncing) rather than play silence if it's late.
        void setStreamer (sfz::Streamer* s, bool wait) noexcept { streamer = s; waitForDisk = wait; }

        // Writes `numSamples` of sound into `left` and `right`, playing the MIDI
        // in `midi` whose positions fall in [midiStart, midiStart + numSamples),
        // each at its own sample.
        void process (float* left, float* right, int numSamples,
                      const juce::MidiBuffer& midi, int midiStart, const Settings&) noexcept;

        // Stage 7b: where each send's mix goes (DELAY's, REVERB's), as long as `process`'s output; nullptr: not wanted.
        // Set before each `process`; it fills them (from silence) for the sends that are on.
        void setSendOutputs (const std::array<float*, numSends>& left, const std::array<float*, numSends>& right) noexcept
        {
            sendBaseLeft = left;
            sendBaseRight = right;
        }

        // Stage 7d: where each insert bus's mix goes (A to D), as long as `process`'s output; nullptr: no bus (its
        // oscillators play straight into the mix). Set before each `process`; it fills them from silence.
        void setInsertOutputs (const std::array<float*, numInsertBuses>& left, const std::array<float*, numInsertBuses>& right) noexcept
        {
            insertBaseLeft = left;
            insertBaseRight = right;
        }

        // A source as the whole synth has it now (at the end of the last `process`), for what follows the whole
        // synth rather than a note (the effects' amounts): the macros and buttons, the wheels, channel aftertouch,
        // breath, expression, the sustain pedal, LFOs and envelopes not set to VOICE. NaN for a source each note has
        // its own of (velocity, keytrack, random, a VOICE LFO or envelope, the pitch envelope, the step sequencer).
        float wholeSynthValue (mod::Source, const Settings&) const noexcept;

        // Notes sounding now (including releasing ones), for the display and tests.
        int getNumActiveVoices() const noexcept;
        // 0.57.0, for DOWNPOUR: the pitches (MIDI notes, glides and all) of the notes sounding now, releasing ones too,
        // up to `capacity`; how many.
        int getSoundingNotes (float* pitches, int capacity, bool* isAnswer = nullptr) const noexcept;   // 0.58.0: and which are SIROCCO's answers

        // 0.64.0, for PROXIMITY: the touches of playing, on the sample they happened (`clock`, on the samples rendered
        // since the start: compare with getSampleClock()): a note starting (keys and arps; SIROCCO's answers aren't
        // touched by anyone), a note let go, the sustain pedal down or up. Kept until taken, at most maxTouches.
        struct Touch
        {
            enum class Kind : std::uint8_t { press, release, pedalDown, pedalUp };
            std::int64_t clock = 0;
            float note = 60.0f, velocity = 1.0f;
            Kind kind = Kind::press;
        };
        static constexpr int maxTouches = 32;
        int takeTouches (Touch* out, int capacity) noexcept;
        std::int64_t getSampleClock() const noexcept { return sampleClock; }

        // Oscillators running now, every unison and chord copy counted (as Avenger's
        // info screen counts them): what the CPU is spent on.
        int getNumActiveOscillators (const Settings&) const noexcept;

        // A unison copy's place among the copies, -1 to 1 (0: the note's own pitch,
        // the middle): sets its detune and pan. For the voices display and tests.
        static float unisonPosition (int copy, int voices) noexcept;

        // For tests: play a note now / let it go.
        void noteOn (int note, float velocity, const Settings&) noexcept;
        void noteOff (int note, const Settings&) noexcept;

        static constexpr int controlInterval = 16;   // samples between updates of the slow things

        // A voice ends once its release is below this (-90 dB).
        static constexpr float silence = 3.1623e-5f;

    private:
        // One copy of an oscillator in a note: the note itself, a chord note,
        // or one of their unison copies (stage 2).
        struct Copy
        {
            float phase = 0.0f;
            float vibratoPhase = 0.0f, vibratoRate = 1.0f;   // INDEPENDENT vibrato: its own phase, and speed (a factor)
            float vibratoHeld = 0.0f;                        // ...and the RANDOM wave's value this cycle
            float crushPrevious = 0.0f;                      // CRUSH BIT's last input
            float modPhase = 0.0f;                           // FM / AM's modulator, at RATE times this copy's pitch
            float lastX = 0.0f;                              // where in the wave the last sample read (FM moves it)
        };
        // Copies are numbered note x maxUnison + unison copy: note 0 is the key
        // itself, 1 to 4 the chord notes; unison copy 0 is the note's own pitch.
        static constexpr int maxCopies = (1 + numChordVoices) * maxUnison;

        // SFZ (0.71.0): one region of a sampled instrument playing in a note.
        struct SfzPlayer
        {
            const sfz::Region* region = nullptr;   // null: free
            const sfz::Sample* sample = nullptr;
            double position = 0.0;                 // frames into the file
            float gain = 0.0f;                     // the region's level, velocity and RT_DECAY, panned
            float gainLeft = 0.0f, gainRight = 0.0f;
            float fade = 1.0f, fadeStep = 0.0f;    // a quick fade when a newer note takes its place
            float decay = 1.0f;                    // x each sample: AMPEG_RELEASE's fall, once its key has let go
            int stream = -1;                       // the disk stream reading the rest (-1: the head only)
            bool looping = false;                  // (0.73.0) going round its region's loop (loop_sustain: until released)
            float wheel = -1.0f;                   // (0.75.0) the mod wheel's part in its level as of the last block (-1: not yet)
            float filterA[2] {}, filterB[2] {};    // (0.75.0) its FILTER's state, each side
        };
        static constexpr int maxSfzPlayers = 2 * sfz::maxPlayers;   // a note's regions, and the last note's fading out

        struct OscState
        {
            bool active = false;        // sounding in this voice (an oscillator can be taken by a newer note)
            float phase = 0.0f, subPhase = 0.0f;   // the key's own copy (copies[0] isn't used), and the sub
            float randomOffset = 0.0f;  // semitones, chosen at the note's start (RANDOM)
            float fade = 1.0f, fadeStep = 0.0f;   // FADE IN, or the quick fade when a newer note takes it
            bool fadingOut = false;
            Noise noiseLeft, noiseRight;
            float vibratoPhase = 0.0f, vibratoHeld = 0.0f;   // GLOBAL vibrato
            int vibratoAge = 0;                              // samples since the note started, for the vibrato's FADE IN
            float tableIndex = -1.0f;                        // WAVETABLE's INDEX as of the last interval (-1: the note just started)
            float crushPrevious = 0.0f;                      // CRUSH BIT's last input (the key's own copy)
            float modPhase = 0.0f, lastX = 0.0f;             // ...and its modulator and last read, as Copy's
            int syncAge = 0;                                 // samples since the note started, for SYNC's envelope
            float pulsePhase = 0.0f;                         // FFT VSPEED's swing between the two edited copies
            double travelTime = 0.0;                         // TRAVEL (stage 4e): seconds since the note started (VOICE)...
            float travelIndex = -1.0f, travelGain = 1.0f;    // ...where SMOOTH has it (-1: not yet), and SILENCE's fade
            WarpTable warp;                                  // SKEW / PINCH moved by the matrix: this note's own curve (stage 4f)
            std::array<Copy, maxCopies> copies;
            // SFZ (0.71.0): the samples this note plays, and the instrument they come from.
            std::array<SfzPlayer, maxSfzPlayers> players {};
            const sfz::Instrument* instrument = nullptr;
            int sfzKey = 60;                                 // the key the regions were chosen for
            double heldSamples = 0.0;                        // since the note started (RT_DECAY)
        };

        // A route through the note: the FILTER (or not) and which SHAPERs, with their state.
        struct Path
        {
            int key = -1;               // bit 0: the FILTER; bits 1 to 4: SHAPERs 1 to 4 (-1: not in use);
                                        // bits 5 on (7b): each send's amount, 7 bits each (0: 100%, n: n - 1 %)
                                        // bits 19 to 21 (7d): the insert bus (0: none, 1 to 4: A to D)
            FilterChannel filterLeft, filterRight;
            Saturator driveLeft, driveRight;
            FilterChannel::Coefficients coefficients;   // the filter's now (glided to each update)
            bool coefficientsFresh = true;
            std::array<shaper::Channel, numShapers> shapeLeft, shapeRight;
            std::array<float, numShapers> shapeGain {}, shapeMakeUp {};   // as of the last update, glided from (0: not yet)
            vocal::Channel vocalLeft, vocalRight;          // the VOICE filter (0.39.0)
            vocal::Coefficients vocalCoefficients;         // ...as of the last update, glided from
            bool vocalFresh = true;
            float vocalLevel = 0.0f;                       // LEVELLING's gain as of the last update (0: not yet)

            void reset() noexcept
            {
                filterLeft.reset();
                filterRight.reset();
                driveLeft.reset();
                driveRight.reset();
                coefficientsFresh = true;
                vocalLeft.reset();
                vocalRight.reset();
                vocalFresh = true;
                vocalLevel = 0.0f;
                for (int k = 0; k < numShapers; ++k)
                {
                    shapeLeft[(size_t) k].reset();
                    shapeRight[(size_t) k].reset();
                    shapeGain[(size_t) k] = shapeMakeUp[(size_t) k] = 0.0f;
                }
            }
        };

        struct Voice
        {
            bool playing = false;       // has a note (sounding or releasing)
            int note = -1;
            float velocity = 1.0f;
            bool held = false, sustained = false;
            std::uint64_t age = 0;      // when it started: older notes are taken first
            int source = 0;             // stage 5a: 0 a key; 1 to 8 ARP n
            std::uint8_t oscMask = 0xff;   // ...and the oscillators it plays (bit per oscillator)
            bool killing = false;       // stolen: fading out fast
            float killGain = 1.0f;

            Envelope amp, filter;
            // One per route its oscillators take (stage 3e): oscillators going
            // through the same FILTER and SHAPERs share one; one set to its own
            // route gets its own filter and shapers. With no shapers on, every
            // oscillator through the filter shares one path, as before.
            std::array<Path, numOscillators> paths;

            float pitch = 60.0f, targetPitch = 60.0f, glideRate = 0.0f;   // semitones; per sample
            float glideFrom = 60.0f, glideProgress = 1.0f, glideStep = 0.0f; // GLIDE CURVE's path: from where, how far (0 to 1), per sample
            mod::EnvelopeState pitchEnvelope {};                             // the PITCH module's envelope (stage 4c)
            float pitchEnvelopeValue = 0.0f, pitchSemitones = 0.0f;          // ...-1 to 1, and as semitones on the oscillators
            double seqPosition = 0.0, seqAge = 0.0;                          // the STEP SEQUENCER (stage 4d): steps since the note started (VOICE), and seconds
            float seqValue = 0.0f;                                           // ...as the matrix source, this interval
            float panLeft = 1.0f, panRight = 1.0f;      // the AMP's PAN, KEYTRACK and SPREAD for this note
            float notePan = 0.0f;                       // ...as a position, -1 to 1 (modulation moves it from here)

            // MODULATION (stage 4a): this note's LFOs (those set to VOICE),
            // its RANDOM value, its poly aftertouch, and each route's LAG.
            std::array<mod::LfoState, mod::numLfos> lfos {};
            std::array<mod::EnvelopeState, mod::numEnvelopes> envelopes {};   // those set to VOICE (0.10.0)
            float randomValue = 0.0f, pressure = 0.0f;
            std::uint64_t answers = 0;   // 0.58.0, SIROCCO: an answer's note's age (its source is answerSource)
            int layer = 0;               // ...and which answer it is (0 the first)
            std::array<float, mod::maxRoutes> lagged {};
            bool lagFresh = true;
            float spikePosition = 1.0f;                 // 0 to 1 through the spike (1: done)
            // The VOICE filter's LEVELLING for this note (0.39.0): its gain, and what it was worked out for.
            float vocalLevel = 1.0f, vocalPitch = -1000.0f;
            std::array<float, 3> vocalFor { -1.0f, -1.0f, -1.0f };   // the VOWEL, SIZE and SHARP they're for
            vocal::Coefficients vocalCoefficients;                  // its resonators (the first formant follows a high note)

            std::array<OscState, numOscillators> oscillators;

            // AFTERGLOW (0.60.0): this note's own character, -1 to 1 each (drawn when it starts, from AFTERGLOW's own
            // random numbers, so the synth's other random choices are the same with it or without), and its wander.
            float glowTune = 0.0f, glowCutoff = 0.0f, glowTime = 0.0f;
            float driftTune = 0.0f, driftCutoff = 0.0f, driftTuneTo = 0.0f, driftCutoffTo = 0.0f, driftLeft = 0.0f;
        };

        void handle (const juce::MidiMessage&, const Settings&) noexcept;
        void render (float* left, float* right, int numSamples, const Settings&) noexcept;
        void renderVoice (Voice&, float* left, float* right, int numSamples, const Settings&, int interval) noexcept;
        // This note's own copy of the settings with its modulation applied (`interval`: which of render's intervals it is).
        const Settings& modulate (Voice&, int n, const Settings&, int interval) noexcept;
        void startLfos (Voice&, const Settings&) noexcept;
        void renderOscillators (Voice&, int numSamples, const Settings&, float bendSemitones) noexcept;
        void assignPaths (Voice&, const Settings&) noexcept;   // each oscillator's route, and a path for each
        void processPath (Voice&, Path&, float* left, float* right, int n, const Settings&,
                          const FilterChannel::Coefficients& filterTarget, const vocal::Coefficients& vocalTarget, float vocalLevel) noexcept;
        void processShaper (Path&, int shaper, float* left, float* right, int n, const Settings&, float velocityDown) noexcept;
        float vibratoValue (VibratoWave, float& phase, float& held, float advance) noexcept;   // -1 to 1, then moves on

        void startVoice (Voice&, int note, float velocity, float fromPitch, const Settings&, int source = 0, std::uint8_t mask = 0xff) noexcept;
        void retrigger (Voice&, int note, float velocity, bool restartEnvelopes, bool glide, const Settings&) noexcept;
        void releaseVoice (Voice&) noexcept;
        void kill (Voice&) noexcept;
        void free (Voice&) noexcept;
        Voice* findFreeVoice() noexcept;
        void applyOscillatorLimits (const Voice& newest, const Settings&) noexcept;
        void setGlide (Voice&, float toPitch, bool glide, const Settings&) noexcept;
        void setNotePan (Voice&, const Settings&) noexcept;

        void polyNoteOn (int note, float velocity, const Settings&) noexcept;
        void makeRoom (const Settings&) noexcept;   // over VOICES: a note goes
        void monoNoteOn (int note, float velocity, const Settings&) noexcept;
        void monoNoteOff (int note, const Settings&) noexcept;
        int monoTarget (const Settings&) const noexcept;   // which held note the mono voice plays, or -1
        void allNotesOff() noexcept;
        void followMode (const Settings&) noexcept;   // POLY to MONO or back: every note lets go
        void sustainPedal (bool down) noexcept;

        double sampleRate = 48000.0;
        // Pool: up to maxVoices notes, plus room for stolen ones finishing their fade.
        // On the heap (about 1 MB with every copy's state): an Engine made on the
        // stack, as the tests do, would overflow Windows' 1 MB thread stack.
        // Sized once and never resized, so pointers into it (monoVoice) stay good.
        std::vector<Voice> voices = std::vector<Voice> (maxVoices + 16);
        Voice* monoVoice = nullptr;
        std::uint64_t noteCounter = 0;
        float lastPitch = -1.0f;        // the last note started, for glide (-1: none yet)
        float bend = 0.0f;              // the pitch wheel, -1 to 1
        bool pedalDown = false;
        PlayMode lastMode = PlayMode::poly;

        // Mono: the keys held down, in the order they were pressed.
        std::array<int, 128> heldNotes {};
        std::array<float, 128> heldVelocities {};
        int numHeld = 0;

        // FM from another oscillator: what each oscillator chosen as a modulator
        // played this interval (rendered first, and not heard itself).
        std::array<std::array<float, controlInterval>, numOscillators> fmSignals {};

        // SHAPE's SKEW / PINCH curve for each oscillator, remade when they change (on the heap: 4 KB each).
        std::vector<WarpTable> warpTables = std::vector<WarpTable> (numOscillators);

        std::array<bool, numOscillators> alternate {};   // RANDOM's alternating direction, per oscillator
        juce::Random random;
        std::uint32_t glowRandom = 0x2545f491u;   // AFTERGLOW's own (xorshift32)
        float nextGlow() noexcept   // -1 to 1
        {
            glowRandom ^= glowRandom << 13;
            glowRandom ^= glowRandom >> 17;
            glowRandom ^= glowRandom << 5;
            return (float) (glowRandom >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }
        void driftOn (Voice&, int numSamples) noexcept;   // AFTERGLOW's wander, moved on

        // Scratch for one control interval: each path's sound, per channel; which
        // path each oscillator plays into (-1: none) and which paths are in use.
        std::array<std::array<float, controlInterval>, numOscillators> pathLeft {}, pathRight {};
        std::array<int, numOscillators> oscillatorPath {};
        std::array<bool, numOscillators> pathUsed {};
        std::array<float, controlInterval> mixLeft {}, mixRight {}, scratch {};
        // Stage 7b: the sends' mixes for this voice's interval, and where the block's sends go.
        std::array<std::array<float, controlInterval>, numSends> sendMixLeft {}, sendMixRight {};
        std::array<float*, numSends> sendBaseLeft {}, sendBaseRight {};
        // Stage 7d: the insert buses' mixes for this voice's interval, and where the block's buses go.
        std::array<std::array<float, controlInterval>, numInsertBuses> busMixLeft {}, busMixRight {};
        std::array<float*, numInsertBuses> insertBaseLeft {}, insertBaseRight {};
        static constexpr int busShift = 19;
        static int busOf (int key) noexcept { return (key >> busShift) & 7; }
        static constexpr int voiceBit = 1 << 22;   // 0.39.0: the path goes through the VOICE filter
        float* outputBase = nullptr;   // `process`'s left output (renderVoice finds its place in the sends from it)
        bool sendsWanted (const Settings& s, int send) const noexcept { return s.sendOn[(size_t) send] && sendBaseLeft[(size_t) send] != nullptr; }
        static float sendGainOf (int key, int send) noexcept
        {
            const auto code = (key >> (5 + 7 * send)) & 127;
            return code == 0 ? 1.0f : (float) (code - 1) / 100.0f;
        }

        // The SHAPERs' EQ bands, worked out once per block (they're the same for every note).
        std::array<shaper::Coefficients, numShapers> shaperCoefficients {};
        float dcPole = 0.9993f;   // the shapers' DC blocker: 5 Hz

        float killStep = 1.0f / 144.0f;   // 3 ms at 48 kHz

        // MODULATION: the controllers as last heard (0 to 1), the LFOs running
        // once for the whole synth, their values for each interval of the
        // render under way, and a scratch copy of the settings for a note.
        float modWheel = 0.0f, breath = 0.0f, expression = 0.0f, channelPressure = 0.0f;
        // 0.51.0: whether controller 11 has come since the reset. Until it does, EXPRESSION as a matrix source reads
        // full, as the EXPR wheel shows it and the volume swell takes it (a pedal not yet touched is all the way
        // up), so a WAH routed to it starts at the toe, where the wheel is drawn. (Sent to the mod wheel, it still
        // counts as 0 until it comes, so the wheel isn't pushed up by a pedal that isn't there.)
        bool expressionHeard = false;
        float expressionSource() const noexcept { return expressionHeard ? expression : 1.0f; }
        // SFZ (0.75.0): whether the mod wheel has moved since the reset (until it has, a sampled instrument's
        // SET_CC1 stands for it), and what's sent to the wheel, as of this block.
        bool modWheelHeard = false;
        std::array<bool, 4> sfzToWheel {};
        float sfzWheel127 (const sfz::Instrument* inst) const noexcept;
        std::array<mod::LfoState, mod::numLfos> globalLfos {};
        static constexpr int maxIntervals = 64;   // render() works in pieces of up to 64 intervals (1024 samples)
        std::array<std::array<float, maxIntervals>, mod::numLfos> globalLfoValues {};
        std::array<mod::EnvelopeState, mod::numEnvelopes> globalEnvelopes {};   // FIRST NOTE and LAST NOTE envelopes
        std::array<std::array<float, maxIntervals>, mod::numEnvelopes> globalEnvelopeValues {};
        std::array<float, mod::numLfos> lastGlobalLfo {};             // the newest of each (wholeSynthValue)
        std::array<float, mod::numEnvelopes> lastGlobalEnvelope {};
        void releaseGlobalEnvelopesIfNoneHeld() noexcept;
        // The pitch wheel smoothed by BEND LAG, for each interval of the render under way.
        float smoothedBend = 0.0f;
        std::array<float, maxIntervals> bendValues {};
        bool glides (int note, const Settings& s) const noexcept { return s.pitch.glideSplit <= 0 || note >= s.pitch.glideSplit; }
        double samplesIntoBlock = 0.0;            // for the song position (synced GLOBAL LFOs)

        // The STEP SEQUENCER (stage 4d): its pattern worked out for this block,
        // whether anything needs it, its own clock (in steps) when it isn't
        // following the song or a note, and its values (left, right) for each
        // sample of the render under way, and for one note's interval.
        seq::Prepared seqPrepared;
        bool seqActive = false;
        double seqClock = 0.0, seqStepsPerSample = 0.0;
        std::array<std::array<float, maxIntervals * controlInterval>, 2> seqValues {};
        std::array<float, controlInterval> seqLeft {}, seqRight {};
        float seqDepth (double age, const StepSeqSettings&) const noexcept;   // DELAY and FADE IN, 0 to 1

        // TRAVEL (stage 4e): each oscillator's clock for FIRST NOTE and LAST NOTE (seconds),
        // and how far into the render piece the interval being rendered starts.
        std::array<double, numOscillators> travelClock {};
        double intervalSeconds = 0.0;
        // The settings before this note's modulation (to tell what the matrix moves for it).
        const Settings* baseSettings = nullptr;
        Settings voiceSettings;

        // The ARPEGGIATOR (stage 5a). Each arp: the keys it holds, its clock, its
        // place in the notes; the notes it has scheduled (on and off, at exact
        // samples on `sampleClock`, the samples rendered since the start).
        struct ArpState
        {
            std::array<arp::Held, arp::maxHeld> held {};
            int numHeld = 0, pressCounter = 0;
            std::array<bool, 128> pedalHeld {};   // let go while the sustain pedal is down
            bool running = false;
            std::int64_t startClock = 0;          // when it started (its own clock)
            std::int64_t lastStep = -1;           // the last step it played
            int noteCounter = 0;                  // its place in the notes
            float firstVelocity = 1.0f, lastVelocity = 1.0f;
            std::uint64_t slideAge = 0;           // a voice gliding on into the next block (0: none)...
            int slidePair = 0;                    // ...and the note it started as
        };
        std::array<ArpState, arp::numArps> arps {};
        struct ArpEvent
        {
            std::int64_t time = 0;
            int arp = 0;
            bool on = true;
            int note = 60;
            float velocity = 1.0f;
            int pair = 0;           // the on and off of one note share this
            bool slide = false;     // an on that glides on into the next block
        };
        static constexpr int maxArpEvents = 1024;
        // On the heap (about 48 KB), sized once: the engine stays small enough for a thread's stack.
        std::vector<ArpEvent> arpEvents = std::vector<ArpEvent> (maxArpEvents);
        int numArpEvents = 0, nextPair = 1;

        // SIROCCO (0.58.0): answers waiting for their DELAY, by their note's age.
        static constexpr int answerSource = arp::numArps + 1;   // an answer's voice source (keys 0, arps 1-8)
        // SFZ (0.71.0): a released note's release samples play in a voice of their own, past the note's AMP.
        static constexpr int tailSource = answerSource + 1;
        struct PendingTail
        {
            int note = 60;
            float velocity = 1.0f;
            float heldSeconds = 0.0f;
            std::uint8_t mask = 0;
            std::array<const sfz::Instrument*, numOscillators> instruments {};
            int pedal = -1;   // 0-127: the sustain pedal moved there (its own sounds), not a key let go
            // Samples handed over from the released note: those whose AMPEG_RELEASE outlasts the AMP's (undamped strings).
            std::array<std::array<SfzPlayer, maxSfzPlayers>, numOscillators> handed {};
        };
        // On the heap (each about 3.5 KB with its handed samples): the engine stays small enough for a thread's stack.
        std::vector<PendingTail> pendingTails = std::vector<PendingTail> (32);
        float ampReleaseSeconds = 0.2f;   // the AMP's release as of the last block (which samples ring on past it)
        int numPendingTails = 0;
        const PendingTail* startingTail = nullptr;   // the tail startTails is starting (startSfz reads it)
        std::array<float, controlInterval> sfzRight {};   // a sampled oscillator's right side (scratch is its left)
        void startTails (const Settings&) noexcept;
        void startSfz (Voice&, OscState&, size_t index, const OscSettings&, bool release) noexcept;
        void renderSfz (Voice&, OscState&, const OscSettings&, float semitones, float* left, float* right, int n) noexcept;
        std::array<std::array<std::uint8_t, 128>, numOscillators> sfzTurns {};   // round-robins: each key's turn
        // KEY SWITCHES (0.75.0): each oscillator's last switch key, and the instrument it was for (a new one starts
        // at its SW_DEFAULT); `switchShown` for the oscillator panel.
        std::array<int, numOscillators> sfzSwitch {};
        std::array<const sfz::Instrument*, numOscillators> sfzSwitchFor {};
        struct SharedInt   // an atomic the engine can still be moved with
        {
            std::atomic<int> value { -1 };
            SharedInt() = default;
            SharedInt (const SharedInt& other) noexcept : value (other.value.load()) {}
            SharedInt& operator= (const SharedInt& other) noexcept { value.store (other.value.load()); return *this; }
            int load() const noexcept { return value.load(); }
            void store (int v) noexcept { value.store (v); }
        };
        std::array<SharedInt, numOscillators> switchShown {};
        int switchFor (size_t index, const sfz::Instrument* inst) noexcept;
    public:
        // The oscillator's key switch now (-1: none), for the panel.
        int getSfzSwitch (int oscillator) const noexcept { return switchShown[(size_t) juce::jlimit (0, numOscillators - 1, oscillator)].load(); }
    private:
        sfz::Streamer* streamer = nullptr;
        bool waitForDisk = false;
        std::array<bool, sfz::Streamer::maxStreams> streamsInUse {};
        struct PendingAnswer { std::uint64_t parent = 0; int layer = 0; std::int64_t time = 0; };
        std::array<PendingAnswer, 96> pendingAnswers {};
        int numPendingAnswers = 0;
        void answer (const Voice& parent, const Settings&) noexcept;            // its answers, now or after DELAY
        void startAnswer (std::uint64_t parentAge, int layer, const Settings&) noexcept;
        void reanswer (const Voice& parent, bool restartEnvelopes, bool glide, const Settings&) noexcept;   // the note moved
        void placeAnswer (Voice&, const Settings&) noexcept;                    // DETUNE and SPREAD
        void releaseAnswers (const Voice& parent) noexcept;
        void runAnswers (const Settings&) noexcept;                             // starts the answers due now
        std::vector<std::uint64_t> pairAges = std::vector<std::uint64_t> (maxArpEvents);   // the voice each pair started (by its age)
        std::int64_t sampleClock = 0, blockStartClock = 0;
        std::array<Touch, maxTouches> touches {};   // 0.64.0, PROXIMITY's
        int numTouches = 0;
        void touch (Touch::Kind kind, float note, float velocity) noexcept
        {
            if (numTouches < maxTouches)
                touches[(size_t) numTouches++] = { sampleClock, note, velocity, kind };
        }
        std::uint8_t keyMask = 0xff;                            // the oscillators the keys play themselves
        // AUTODICE (0.16.0): each arp's pattern rolled for this cycle (on the heap), and which cycle.
        std::vector<arp::Pattern> livePatterns = std::vector<arp::Pattern> (arp::numArps);
        std::array<std::int64_t, arp::numArps> liveCycles {};
        // ARPS TOGETHER (0.16.0): the keys down, and when the first went down.
        std::array<bool, 128> keysDown {};
        int numKeysDown = 0;
        std::int64_t sharedStartClock = 0;
        void arpNoteStarted (const Settings&) noexcept;   // ARP NOTES envelopes and LFOs start
        void arpNoteEnded() noexcept;                       // ...and let go when no arp note sounds
        std::uint32_t arpEnvelopeMask = 0;                  // the envelopes the arp notes started (bit each)
        std::array<std::uint8_t, arp::numArps> arpMasks {};      // ...and each arp's

        void updateArpMasks (const Settings&) noexcept;
        bool arpActive (int a) const noexcept { return arpMasks[(size_t) a] != 0; }
        void arpKeyDown (int note, float velocity, const Settings&) noexcept;
        void arpKeyUp (int note) noexcept;
        void removeArpKey (ArpState&, int note) noexcept;
        void stopArpIfEmpty (int a) noexcept;
        std::int64_t arpStepTime (int a, std::int64_t step, const Settings&) const noexcept;   // when a step starts (on sampleClock)
        std::int64_t arpStepAt (int a, std::int64_t time, const Settings&) const noexcept;     // the last step started by then
        bool arpFollowsSong (int a, const Settings&) const noexcept;
        void runArps (const Settings&) noexcept;                                               // plays everything due now
        std::int64_t nextArpTime (const Settings&) const noexcept;                             // when something is next due
        void playArpStep (int a, std::int64_t step, std::int64_t at, const Settings&) noexcept;
        void scheduleArp (const ArpEvent&) noexcept;
        void fireArpEvent (const ArpEvent&, const Settings&) noexcept;
    };
}
