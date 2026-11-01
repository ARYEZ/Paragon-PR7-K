#pragma once

#include "rigs/MimicRig.h"

#include <memory>

class ParagonX1AudioProcessor;

// MIMIC part 2 (0.45.0): plays a made sound offline, scores it against the recording (synth::mimic::compare), and
// refines it. A private copy of the synth does the playing, so the one the player hears is never touched: each
// candidate is the note the recording was at, held as long as it was, from silence, its random choices seeded (the
// same candidate always scores the same).
//
// REFINE nudges, one at a time, what MIMIC estimated rather than measured exactly: the AMP's attack, decay, sustain and
// release and the shapes of its falls, the noise's level, unison's detune, vibrato's depth and TRAVEL's speed, each up and down; a nudge is kept if
// the match improves. Three passes, the nudges smaller each pass (about 45 plays). Then the volume is matched (LEVEL).
// Message thread: one play per step(), so the page stays alive.
class MimicRefiner
{
public:
    MimicRefiner();
    ~MimicRefiner();

    // The match of `rig` against the analysis's recording now (one play), with its LEVEL set to match the volume.
    synth::mimic::Score score (Rig& rig, const synth::mimic::Analysis&);

    // Refining, a play at a time.
    void begin (const Rig& rig, const synth::mimic::Analysis&, const mimic::Choices&);
    bool step();   // false once done
    bool isRefining() const noexcept { return refining; }
    int getDone() const noexcept { return done; }
    int getPlanned() const noexcept { return planned; }
    const Rig& getBest() const noexcept { return best; }
    const synth::mimic::Score& getBestScore() const noexcept { return bestScore; }
    const synth::mimic::Score& getFirstScore() const noexcept { return firstScore; }

    // The candidate played (left and right mixed), for tests.
    std::vector<float> play (const Rig&, const synth::mimic::Analysis&);
    static constexpr double renderRate = 48000.0;
    static constexpr double maxSeconds = 6.0;

private:
    struct Knob
    {
        juce::String id;
        bool multiply;        // nudged by a factor (times), or by an amount (plus: `scale` x the pass's amount)
        float low, high;
        float scale = 1.0f;
    };
    double evaluate (Rig&);

    std::unique_ptr<ParagonX1AudioProcessor> renderer;
    synth::mimic::Fingerprint target;
    synth::mimic::Analysis analysis;
    std::vector<Knob> knobs;
    Rig best;
    synth::mimic::Score bestScore, firstScore;
    bool refining = false;
    int pass = 0, knob = 0, direction = 0, done = 0, planned = 0;
    float factor = 1.6f, amount = 0.12f;
};
