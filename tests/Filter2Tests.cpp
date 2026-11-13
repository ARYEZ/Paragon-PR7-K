// FILTER 2 and the LOW CUT (0.96.0): a second filter with its own envelope for the oscillators set to it, and a
// resonant high-pass in front of each filter, moved with it (a vintage polysynth's two lines: high-pass into
// low-pass, one envelope for both). Off, nothing changes; on, each does what its curve says.
#include "SynthTestUtils.h"

#include "PluginProcessor.h"
#include "ui/SynthDisplays.h"

namespace
{
    double hzOf (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

    // Two sine oscillators, the second two octaves up, each routed as asked.
    Player twoSines (bool secondToFilter2)
    {
        Player p;
        p.osc (0).wave = synth::OscWave::sine;
        p.osc (1).on = true;
        p.osc (1).wave = synth::OscWave::sine;
        p.osc (1).randomPhase = false;
        p.osc (1).transpose = 24.0f;
        p.osc (1).toFilter2 = secondToFilter2;
        return p;
    }
}

TEST_CASE ("FILTER 2 off, unrouted, and LOW CUT at its lowest change nothing at all", "[synth][filter][filter2]")
{
    const auto play = [] (bool fiddle)
    {
        auto p = twoSines (false);
        p.osc (0).wave = p.osc (1).wave = synth::OscWave::saw;
        p.settings.filter.on = true;
        p.settings.filter.cutoffHz = 900.0f;
        p.settings.filter.resonance = 0.4f;
        p.settings.filter.envelopeAmount = 0.4f;
        if (fiddle)
        {
            // FILTER 2 set up but off, then on but with no oscillator going through it.
            p.settings.filter2.cutoffHz = 150.0f;
            p.settings.filter2.lowCutHz = 2000.0f;
            p.settings.filter2.envelopeAmount = -1.0f;
        }
        p.on (57);
        p.on (64);
        auto first = p.render (0.5);
        if (fiddle)
            p.settings.filter2.on = true;
        auto second = p.render (0.5);
        p.off (57);
        p.off (64);
        auto third = p.render (0.5);
        for (auto* part : { &second, &third })
            for (int c = 0; c < 2; ++c)
                first[(size_t) c].insert (first[(size_t) c].end(), (*part)[(size_t) c].begin(), (*part)[(size_t) c].end());
        return first;
    };
    const auto plain = play (false), fiddled = play (true);
    CHECK (plain[0] == fiddled[0]);
    CHECK (plain[1] == fiddled[1]);
}

TEST_CASE ("FILTER 2 filters only the oscillators set to it; the first filter the others", "[synth][filter][filter2]")
{
    // Oscillator 1 at 220 Hz through FILTER 1, oscillator 2 at 880 Hz through FILTER 2: closing one filter cuts only its own.
    const auto levels = [] (float cutoff1, float cutoff2)
    {
        auto p = twoSines (true);
        for (auto* f : { &p.settings.filter, &p.settings.filter2 })
        {
            f->on = true;
            f->type = synth::FilterType::lp24;
        }
        p.settings.filter.cutoffHz = cutoff1;
        p.settings.filter2.cutoffHz = cutoff2;
        p.on (57);
        const auto out = p.render (0.6)[0];
        return std::pair { db (levelAt (out, 220.0, 14400, 14400)), db (levelAt (out, 880.0, 14400, 14400)) };
    };
    const auto open = levels (20000.0f, 20000.0f);
    const auto firstClosed = levels (60.0f, 20000.0f);
    const auto secondClosed = levels (20000.0f, 60.0f);
    INFO ("open " << open.first << " / " << open.second << ", first closed " << firstClosed.first << " / " << firstClosed.second
          << ", second closed " << secondClosed.first << " / " << secondClosed.second);
    CHECK (firstClosed.first < open.first - 40.0);
    CHECK_THAT (firstClosed.second, WithinAbs (open.second, 0.2));
    CHECK (secondClosed.second < open.second - 60.0);
    CHECK_THAT (secondClosed.first, WithinAbs (open.first, 0.2));

    // An oscillator set to FILTER 2 while FILTER 2 is off goes through no filter (not the first one).
    auto p = twoSines (true);
    p.settings.filter.on = true;
    p.settings.filter.cutoffHz = 60.0f;
    p.on (57);
    CHECK_THAT (db (levelAt (p.render (0.6)[0], 880.0, 14400, 14400)), WithinAbs (open.second, 0.2));
}

TEST_CASE ("FILTER 2 has its own envelope: it opens and closes on its own timing", "[synth][filter][filter2]")
{
    auto p = twoSines (true);
    p.osc (0).wave = p.osc (1).wave = synth::OscWave::saw;
    p.osc (1).transpose = 0.0f;
    p.osc (0).on = false;   // only the oscillator on FILTER 2
    p.settings.filter.on = true;
    p.settings.filter.cutoffHz = 20000.0f;     // the first filter open and still: it can't be what moves
    p.settings.filter2.on = true;
    p.settings.filter2.type = synth::FilterType::lp24;
    p.settings.filter2.cutoffHz = 200.0f;
    p.settings.filter2.envelopeAmount = 0.5f;  // +4 octaves at the peak
    p.settings.filter2.envelope = { 0.0f, 0.0f, 300.0f, 0.0f, 100.0f };
    p.on (45);   // a 110 Hz saw
    const auto out = p.render (1.0);
    const auto early = db (levelAt (out[0], 880.0, 0, 2400));
    const auto late = db (levelAt (out[0], 880.0, 36000, 9600));
    INFO (early << " dB early, " << late << " dB late");
    CHECK (early > late + 20.0);
}

TEST_CASE ("LOW CUT: a resonant high-pass in front of each filter, as steep as it, as its curve says", "[synth][filter][filter2]")
{
    for (const auto second : { false, true })
        for (const auto [type, steep] : { std::pair { synth::FilterType::lp12, false }, std::pair { synth::FilterType::lp24, true } })
            for (const auto [lowCut, resonance] : { std::pair { 500.0f, 0.0f }, std::pair { 300.0f, 0.5f } })
                for (const auto note : { 40, 52, 57, 64, 69, 81 })   // 82 Hz to 880 Hz
                {
                    Player p;
                    p.osc().wave = synth::OscWave::sine;
                    p.osc().toFilter2 = second;
                    auto& f = second ? p.settings.filter2 : p.settings.filter;
                    f.on = true;
                    f.type = type;
                    f.cutoffHz = 12000.0f;
                    f.lowCutHz = lowCut;
                    f.lowCutResonance = resonance;
                    p.on (note);
                    const auto hz = hzOf (note);
                    const auto measured = db (levelAt (p.render (0.6)[0], hz, 14400, 14400));
                    const auto expected = FilterDisplay::responseDb (type, 12000.0, 0.0, hz)
                                        + FilterDisplay::responseDb (steep ? synth::FilterType::hp24 : synth::FilterType::hp12,
                                                                     lowCut, resonance, hz);
                    INFO ((second ? "FILTER 2 " : "FILTER ") << synth::filterTypeNames[(int) type] << ", low cut " << lowCut
                          << " Hz res " << resonance << ": " << hz << " Hz measured " << measured << " expected " << expected);
                    if (expected > -60.0)
                        CHECK_THAT (measured, WithinAbs (expected, 0.75));
                    else
                        CHECK (measured < -55.0);
                }
}

TEST_CASE ("LOW CUT moves with its filter: keytrack and the envelope move both", "[synth][filter][filter2]")
{
    // KEYTRACK 100%: each note's own pitch is cut by the LOW CUT the same, an octave apart.
    const auto ownLevel = [] (int note)
    {
        Player p;
        p.osc().wave = synth::OscWave::sine;
        p.settings.filter.on = true;
        p.settings.filter.type = synth::FilterType::lp12;
        p.settings.filter.cutoffHz = 20000.0f;
        p.settings.filter.lowCutHz = 400.0f;
        p.settings.filter.keytrack = 1.0f;
        p.on (note);
        return db (levelAt (p.render (0.6)[0], hzOf (note), 14400, 14400));
    };
    CHECK (ownLevel (60) < -6.0);   // 262 Hz under a 400 Hz low cut
    CHECK_THAT (ownLevel (72), WithinAbs (ownLevel (60), 0.4));

    // The envelope sweeping up moves the LOW CUT up too: a 110 Hz saw's fundamental cut hard at the peak, back once it falls.
    Player p;
    p.settings.filter.on = true;
    p.settings.filter.type = synth::FilterType::lp12;
    p.settings.filter.cutoffHz = 5000.0f;
    p.settings.filter.lowCutHz = 40.0f;
    p.settings.filter.envelopeAmount = 0.5f;
    p.settings.filter.envelope = { 0.0f, 300.0f, 200.0f, 0.0f, 100.0f };
    p.on (45);
    const auto out = p.render (1.0);
    const auto peak = db (levelAt (out[0], 110.0, 2400, 9600));
    const auto settled = db (levelAt (out[0], 110.0, 33600, 9600));
    INFO (peak << " dB at the peak, " << settled << " dB settled");
    CHECK (peak < settled - 20.0);
}

TEST_CASE ("FILTER 2 in the plugin: its parameters, the oscillators' switch, and the matrix", "[filter2][plugin]")
{
    ParagonX1AudioProcessor processor;
    for (const auto& ids : ParamIDs::filterIds)
        for (const auto* id : { ids.on, ids.type, ids.cutoff, ids.res, ids.drive, ids.key, ids.vel, ids.resVel, ids.env, ids.envVel,
                                ids.attack, ids.hold, ids.decay, ids.sustain, ids.release, ids.attackCurve, ids.decayCurve,
                                ids.releaseCurve, ids.lowCut, ids.lowCutRes })
        {
            INFO (id);
            CHECK (processor.getParameters().getParameter (id) != nullptr);
        }
    for (int i = 0; i < ParamIDs::numOscillators; ++i)
        CHECK (processor.getParameters().getParameter (ParamIDs::osc (i, "Filter2")) != nullptr);
    // A new sound: no FILTER 2, no LOW CUT, every oscillator on the first filter.
    const auto s = processor.getSynthSettings();
    CHECK_FALSE (s.filter2.on);
    CHECK_FALSE (s.filter.hasLowCut());
    CHECK_FALSE (s.filter2.hasLowCut());
    for (const auto& o : s.oscillators)
        CHECK_FALSE (o.toFilter2);
}
