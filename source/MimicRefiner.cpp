#include "MimicRefiner.h"

#include "Parameters.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int block = 512;
    constexpr int passes = 3;
}

MimicRefiner::MimicRefiner() = default;
MimicRefiner::~MimicRefiner() = default;

std::vector<float> MimicRefiner::play (const Rig& rig, const synth::mimic::Analysis& a)
{
    if (renderer == nullptr)
    {
        renderer = std::make_unique<ParagonX1AudioProcessor>();
        renderer->setPlayConfigDetails (0, 2, renderRate, block);
    }
    renderer->applyMimic (rig);
    renderer->prepareToPlay (renderRate, block);   // from silence: no tail from the last candidate
    renderer->finishTableBuilds();
    renderer->seedRandomForTest (1);

    // The note it was recorded at, held as long as it was, then its tail as far as the recording goes.
    const auto held = juce::jlimit (0.05, maxSeconds, a.releaseStart - a.start);
    const auto seconds = juce::jlimit (held, maxSeconds, a.seconds - a.start);
    const auto blocks = (int) std::ceil (seconds * renderRate / block);
    const auto offSample = (int) (held * renderRate);
    std::vector<float> out;
    out.reserve ((size_t) blocks * block);
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, a.rootNote, 0.8f), 0);
        if (offSample >= b * block && offSample < (b + 1) * block)
            midi.addEvent (juce::MidiMessage::noteOff (1, a.rootNote), offSample - b * block);
        buffer.clear();
        renderer->processBlock (buffer, midi);
        for (int i = 0; i < block; ++i)
            out.push_back (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
    }
    return out;
}

synth::mimic::Score MimicRefiner::score (Rig& rig, const synth::mimic::Analysis& a)
{
    if (! a.valid || a.audio == nullptr)
        return {};
    if (&a != &analysis || target.levels.empty())
        target = synth::mimic::fingerprint (*a.audio, a.sampleRate, a.start, maxSeconds);
    const auto candidate = synth::mimic::fingerprint (play (rig, a), renderRate);
    auto s = synth::mimic::compare (target, candidate);
    // The volume matched: the LEVEL the recording's loudness needs (OUTPUT plays both the same).
    const auto found = rig.parameters.find (ParamIDs::patchLevel);
    const auto level = found != rig.parameters.end() ? found->second : 0.0f;
    rig.parameters[ParamIDs::patchLevel] = juce::jlimit (ParamRanges::gainMinDb, ParamRanges::gainMaxDb, level + (float) s.gainDb);
    return s;
}

void MimicRefiner::begin (const Rig& rig, const synth::mimic::Analysis& a, const mimic::Choices& choices)
{
    analysis = a;
    target = synth::mimic::fingerprint (*a.audio, a.sampleRate, a.start, maxSeconds);
    best = rig;
    bestScore = firstScore = score (best, analysis);

    // What to nudge: only what MIMIC estimated and the player chose to take.
    knobs.clear();
    const auto has = [&] (const juce::String& id) { return best.parameters.count (id) > 0; };
    if (choices.envelope)
    {
        knobs.push_back ({ ParamIDs::ampAttack, true, 1.0f, 20000.0f });
        knobs.push_back ({ ParamIDs::ampDecay, true, 10.0f, 20000.0f });
        if (a.sustained)
            knobs.push_back ({ ParamIDs::ampSustain, false, 0.0f, 1.0f });
        knobs.push_back ({ ParamIDs::ampRelease, true, 5.0f, 20000.0f });
        // The fall's shapes: MIMIC assumes a natural one (a steady fall in dB, a curve of -1); a sound that falls in a
        // straighter line needs them bent back, or the times and sustain get bent to make up for it.
        knobs.push_back ({ ParamIDs::ampDecayCurve, false, -1.0f, 1.0f, 3.0f });
        knobs.push_back ({ ParamIDs::ampReleaseCurve, false, -1.0f, 1.0f, 3.0f });
    }
    if (choices.noise && a.noise > 0.0f && has (ParamIDs::osc (1, "Volume")))
        knobs.push_back ({ ParamIDs::osc (1, "Volume"), true, 0.01f, 2.0f });
    if (choices.unison && a.detuneCents > 0.0f && has (ParamIDs::osc (0, "UniDetune")))
        knobs.push_back ({ ParamIDs::osc (0, "UniDetune"), false, 0.0f, 1.0f });
    if (choices.vibrato && a.vibratoCents > 0.0f && has (ParamIDs::osc (0, "VibAmount")))
        knobs.push_back ({ ParamIDs::osc (0, "VibAmount"), true, 1.0f, 100.0f });
    if (has (ParamIDs::osc (0, "TrOn")) && best.parameters.at (ParamIDs::osc (0, "TrOn")) > 0.5f)
        knobs.push_back ({ ParamIDs::osc (0, "TrRate"), true, 0.02f, 40.0f });

    pass = knob = direction = 0;
    factor = 1.6f;
    amount = 0.12f;
    done = 1;
    planned = 1 + passes * 2 * (int) knobs.size();
    refining = ! knobs.empty();
}

bool MimicRefiner::step()
{
    if (! refining)
        return false;
    const auto& k = knobs[(size_t) knob];
    const auto found = best.parameters.find (k.id);
    const auto value = found != best.parameters.end() ? found->second : 0.0f;
    const auto up = direction == 0;
    const auto nudged = juce::jlimit (k.low, k.high, k.multiply ? (up ? value * factor : value / factor) : (up ? value + amount * k.scale : value - amount * k.scale));
    bool improved = false;
    if (! juce::approximatelyEqual (nudged, value))
    {
        auto candidate = best;
        candidate.parameters[k.id] = nudged;
        const auto s = score (candidate, analysis);
        ++done;
        if (s.match > bestScore.match + 0.05)
        {
            best = std::move (candidate);
            bestScore = s;
            improved = true;
        }
    }
    else
        planned = juce::jmax (done, planned - 1);   // at its end already: nothing to play

    // Next: down after up (unless up helped), then the next knob; after the last, a finer pass.
    if (up && ! improved)
        direction = 1;
    else
    {
        if (up && improved)
            planned = juce::jmax (done, planned - 1);   // down isn't tried
        direction = 0;
        if (++knob >= (int) knobs.size())
        {
            knob = 0;
            factor = std::sqrt (factor);
            amount *= 0.5f;
            if (++pass >= passes)
                refining = false;
        }
    }
    return refining;
}
