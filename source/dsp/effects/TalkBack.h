#pragma once

#include "EffectUtils.h"

#include <algorithm>
#include <array>
#include <atomic>

namespace effects
{
    // TALKBACK (1.9.0, the SUNTHRONE expansion's signature, the owner's choice after hearing the sketch): a talking drum
    // answers what you play. Our own design: no drum is modelled from a recording or a product.
    //
    // It hears your phrases from the notes you play (the synth's key presses and releases, arps' notes too), not from
    // the sound. A phrase ends at the first bar line (WAIT: or beat) where no key is held and you've rested a moment
    // (a quarter of a beat on a bar line, half a beat on a beat); then the drum plays it back from there, in the same
    // rhythm, a bar (or the beats) on from where you started it. Each stroke's pitch follows your melody's rise and fall
    // (RANGE: how much of each interval, about two thirds as a player's squeeze allows), gliding from one stroke to the
    // next in 45 ms as the drum's tension changes, and a long note bends up as it's held (BEND), the "speaking" of a
    // talking drum.
    //
    // AMOUNT (MACRO 3) goes from off to a conversation: up to halfway, that share of phrases answered (the same phrase
    // in the same place always the same: a fixed hash); from halfway every phrase is answered, and the drummer adds
    // more of his own: a flam (a quick grace stroke) before long notes and a falling two-stroke tag after the answer,
    // and the bends grow.
    //
    // The drum: a membrane of five inharmonic partials (1, 1.59, 2.14, 2.65, 2.92: a struck membrane's) with their own
    // decays, and the curved stick's crack (a 12 ms noise burst through a band-pass); PITCH its centre (80-260 Hz),
    // TONE how bright and crisp, PAN where it stands, SPACE its share into the REVERB's send, LEVEL how loud.
    //
    // It works in two steps, as DOWNPOUR does: `render` makes this block's drum into its own buffer (before the sends,
    // so `getOutput` can feed the reverb), `addTo` adds it to a block. The sound passes unchanged; while the drum is
    // silent nothing is added (untouched bit for bit).
    class TalkBack
    {
    public:
        static inline const juce::StringArray waitNames { "NEXT BAR", "NEXT BEAT" };   // never reorder

        struct Settings
        {
            bool on = false;
            float amount = 0.6f;     // 0 to 1 (MACRO 3): how much it answers
            float pitchHz = 150.0f;  // 80 to 260: the drum's centre
            float range = 0.6f;      // 0.2 to 1: the share of each interval followed
            float bend = 1.5f;       // 0 to 3 semitones: how far a held note bends up
            float tone = 0.5f;       // 0 dark and soft to 1 bright and crisp
            int wait = 0;            // waitNames
            float space = 0.3f;      // 0 to 1: into the reverb's send
            float pan = 0.2f;        // -1 left to 1 right
            float levelDb = -6.0f;   // -36 to +6 (the processor takes the preset's LEVEL off, so up to -60 to +40 here)
        };

        // How AMOUNT is shared out: the chance a phrase is answered, the chance of each of the drummer's own touches
        // (a flam, a tag), and how much of BEND is used.
        static float answerChance (float amount) noexcept { return juce::jlimit (0.0f, 1.0f, 2.0f * amount); }
        static float ornamentChance (float amount) noexcept { return juce::jlimit (0.0f, 1.0f, 2.0f * amount - 1.0f); }
        static float bendShare (float amount) noexcept { return 0.5f + 0.5f * juce::jlimit (0.0f, 1.0f, amount); }
        // A phrase's place (its first note's unit line) to a number in 0-1, the same every time.
        static float phraseHash (double line, std::uint32_t salt) noexcept;

        static constexpr int maxNotes = 64, maxStrokes = 192, maxEvents = 64;
        static constexpr std::array<float, 5> partials { 1.0f, 1.59f, 2.14f, 2.65f, 2.92f };

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { noise = seed == 0 ? 0x9e3779b9u : seed; }

        // A key pressed or let go `offset` samples into the coming block (called before `render`).
        void addNote (int offset, float note, float velocity, bool press) noexcept;

        // This block's drum: `numSamples` (up to the prepared size) at `bpm`, the song at `ppq` beats at the block's
        // start (-1 while it's stopped: its own count). Then `addTo` / `getOutput`.
        void render (int numSamples, const Settings&, double bpm, double ppq) noexcept;
        void addTo (juce::dsp::AudioBlock<float> block) const noexcept;
        const float* getOutput (int channel) const noexcept { return output.getReadPointer (channel); }
        int getRenderedSamples() const noexcept { return rendered; }
        bool isSilent() const noexcept { return silent; }

        // For the page and the unit: the last phrase heard (beats from its start line, notes, lengths) and the last
        // answer (strokes' beats from its start, semitones from the drum's centre), the strokes so far, the drum's
        // pitch now (semitones) and whether it's answering.
        static constexpr int shownNotes = 32;
        struct Picture
        {
            int notes = 0, strokes = 0;
            std::array<float, shownNotes> noteBeat {}, noteLength {}, noteValue {};
            std::array<float, shownNotes * 2> strokeBeat {}, strokeSemis {};
            float unit = 4.0f;
        };
        Picture getPicture() const noexcept;
        int getStrokeCount() const noexcept { return strokeCount.load (std::memory_order_relaxed); }
        float getPitchNow() const noexcept { return pitchNow.load (std::memory_order_relaxed); }
        bool isAnswering() const noexcept { return answering.load (std::memory_order_relaxed); }
        int getAnswers() const noexcept { return answers.load (std::memory_order_relaxed); }

    private:
        struct Event { int offset = 0; float note = 0.0f, velocity = 0.0f; bool press = true; };
        struct Heard { double on = 0.0, off = -1.0; float note = 0.0f, velocity = 1.0f; };
        struct Stroke { double beat = 0.0; float semis = 0.0f, velocity = 1.0f; double length = 0.0; };

        void closePhrase (double line, const Settings&) noexcept;
        void strike (const Stroke&, const Settings&) noexcept;
        float random01() noexcept;

        double sampleRate = 48000.0, beatsPerSecond = 2.0;
        juce::AudioBuffer<float> output;
        int rendered = 0;
        bool silent = true;

        std::array<Event, maxEvents> events {};
        int numEvents = 0;

        // The phrase being heard.
        std::array<Heard, maxNotes> phrase {};
        int phraseNotes = 0, held = 0;
        double phraseLine = 0.0, lastActivity = -1.0e9;
        double ownBeat = 0.0, lastBeat = -1.0e9;
        bool wasPlaying = false;

        // The answer to play: strokes by beat, in order.
        std::array<Stroke, maxStrokes> strokes {};
        int numStrokes = 0, nextStroke = 0;

        // The drum.
        std::array<float, partials.size()> env {}, decay {};
        std::array<double, partials.size()> phase {};
        float crack = 0.0f, crackDecay = 0.0f, crackLow = 0.0f, crackHigh = 0.0f;
        float fromSemis = 0.0f, toSemis = 0.0f, squeeze = 0.0f, semisNow = 0.0f;
        double strokeAge = 0.0, squeezeSeconds = 0.0;
        std::uint32_t noise = 0x9e3779b9u;

        // For the page (written on the audio thread, read on the message thread).
        std::array<std::atomic<float>, shownNotes> shownBeat {}, shownLength {}, shownNote {};
        std::array<std::atomic<float>, shownNotes * 2> shownStrokeBeat {}, shownStrokeSemis {};
        std::atomic<int> shownNoteCount { 0 }, shownStrokeCount { 0 }, strokeCount { 0 }, answers { 0 };
        std::atomic<float> shownUnit { 4.0f }, pitchNow { 0.0f };
        std::atomic<bool> answering { false };
    };
}
