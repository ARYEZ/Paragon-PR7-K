// The VOICE filter (0.39.0): vowel formants at fixed frequencies, morphing, size, routing, level; measured.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/synth/VoiceFilter.h"

namespace
{
    // The filter's gain at `hz` (a steady sine through one channel, its level after it settles).
    double gainAt (float vowel, float size, double hz, float sharp = 0.5f)
    {
        const auto c = synth::vocal::Coefficients::make (vowel, size, sharp, sampleRate);
        synth::vocal::Channel channel;
        const auto samples = (int) sampleRate / 2;
        std::vector<float> out ((size_t) samples);
        for (int i = 0; i < samples; ++i)
            out[(size_t) i] = channel.process ((float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sampleRate), c);
        return juce::Decibels::gainToDecibels (rms (out, (size_t) samples / 2, (size_t) samples) * std::sqrt (2.0));
    }

    // A saw on oscillator 1 through the VOICE filter (and whatever `setup` adds), as the engine plays it.
    std::array<std::vector<float>, 2> play (int note, double seconds, const std::function<void (Player&)>& setup)
    {
        Player p;
        p.settings.amp.envelope.sustain = 1.0f;
        p.settings.amp.envelope.attackMs = 0.0f;
        p.settings.voice.on = true;
        setup (p);
        p.on (note);
        return p.render (seconds);
    }

    // The frequency of the loudest harmonic of `hz` between `low` and `high` Hz: where a formant pulls the sound.
    double loudestHarmonic (const std::vector<float>& x, double hz, double low, double high)
    {
        auto best = 0.0, bestHz = 0.0;
        for (auto h = 1; h * hz < high; ++h)
            if (const auto f = h * hz; f >= low)
                if (const auto level = levelAt (x, f, 9600, 9600); level > best)
                {
                    best = level;
                    bestHz = f;
                }
        return bestHz;
    }
}

TEST_CASE ("VOICE: each vowel's resonances sit at its formants (AH's first two near 600 and 1040 Hz for a bass)", "[voice]")
{
    // AH, a bass singer: peaks at the first two formants, a valley between them and the third.
    const auto f1 = gainAt (0.5f, 0.0f, 600.0), f2 = gainAt (0.5f, 0.0f, 1040.0), valley = gainAt (0.5f, 0.0f, 1600.0);
    const auto low = gainAt (0.5f, 0.0f, 120.0);
    WARN ("AH bass: 600 Hz " << f1 << " dB, 1040 Hz " << f2 << " dB, 1600 Hz " << valley << " dB, 120 Hz " << low << " dB");
    CHECK (f1 > valley + 12.0);
    CHECK (f2 > valley + 6.0);
    CHECK (f1 > low + 12.0);

    // EE has its second formant high (a soprano's about 2140 Hz), OO low (about 700): the difference between the
    // vowels. (A bass's EE has that formant 30 dB down in the published data, so sopranos show it plainly.)
    CHECK (gainAt (1.0f, 1.0f, 2140.0) > gainAt (0.0f, 1.0f, 2140.0) + 15.0);
    CHECK (gainAt (0.0f, 1.0f, 2140.0) < gainAt (0.0f, 1.0f, 325.0) - 20.0);   // OO is dark

    // SIZE moves the formants up: a soprano's AH has its first at about 800 Hz, where a bass's has fallen away.
    CHECK (gainAt (0.5f, 1.0f, 800.0) > gainAt (0.5f, 0.0f, 800.0) + 3.0);

    // SHARP narrows them: further from a formant, a sharp one has let go more.
    CHECK (gainAt (0.5f, 0.0f, 800.0, 1.0f) < gainAt (0.5f, 0.0f, 800.0, 0.0f) - 3.0);

    // The blend between vowels is smooth: halfway from AH to EH, F2 sits between the two (on a log scale).
    const auto between = synth::vocal::formantsFor (0.625f, 0.0f)[1].hz;
    CHECK (between > 1040.0f);
    CHECK (between < 1620.0f);
}

TEST_CASE ("VOICE: the formants stay put whatever note is played (the vowel doesn't move with the pitch)", "[voice]")
{
    // A saw through AH (bass): the loudest harmonic between 400 and 900 Hz lands near F1 (600 Hz) for each note,
    // though the notes are an octave and more apart. Without VOICE, a saw's loudest harmonic is its lowest.
    for (const auto note : { 40, 45, 52 })
    {
        const auto hz = 440.0 * std::exp2 ((note - 69) / 12.0);
        const auto out = play (note, 0.5, [] (Player& p) { p.settings.voice.vowel = 0.5f; p.settings.voice.size = 0.0f; })[0];
        const auto peak = loudestHarmonic (out, hz, 400.0, 900.0);
        INFO ("note " << note << " (" << hz << " Hz): loudest harmonic between 400 and 900 Hz at " << peak << " Hz");
        CHECK (std::abs (peak - 600.0) < hz * 0.75);
    }
    const auto plain = play (45, 0.5, [] (Player& p) { p.settings.voice.on = false; })[0];
    CHECK (loudestHarmonic (plain, 110.0, 400.0, 900.0) < 450.0);
}

TEST_CASE ("VOICE: off, or an oscillator not routed to it, is the sound unchanged; MIX 0 is the dry sound", "[voice]")
{
    const auto plain = play (57, 0.4, [] (Player& p) { p.settings.voice.on = false; })[0];
    const auto unrouted = play (57, 0.4, [] (Player& p) { p.osc().toVoice = false; })[0];
    REQUIRE (plain.size() == unrouted.size());
    CHECK (plain == unrouted);

    const auto dry = play (57, 0.4, [] (Player& p) { p.settings.voice.mix = 0.0f; })[0];
    double apart = 0.0;
    for (size_t i = 0; i < plain.size(); ++i)
        apart = std::max (apart, (double) std::abs (plain[i] - dry[i]));
    CHECK (apart < 1.0e-5);

    // And on, it really changes the sound.
    const auto voiced = play (57, 0.4, [] (Player&) {})[0];
    double moved = 0.0;
    for (size_t i = 0; i < plain.size(); ++i)
        moved = std::max (moved, (double) std::abs (plain[i] - voiced[i]));
    CHECK (moved > 0.1);
}

TEST_CASE ("VOICE: LEVELLING keeps every note near the level it went in at (no note jumping out), any vowel or size", "[voice]")
{
    // Without LEVELLING, a harmonic landing right on a narrow formant made some notes 10 dB louder than the saw
    // going in and others 9.5 dB quieter; levelled, they all sit within a few dB under it.
    const auto plain = play (57, 0.5, [] (Player& p) { p.settings.voice.on = false; })[0];
    const auto dryLevel = juce::Decibels::gainToDecibels (rms (plain, 9600, 24000));
    for (const auto vowel : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        for (const auto size : { 0.0f, 0.5f, 1.0f })
            for (int note = 40; note <= 76; note += 3)
            {
                const auto voiced = play (note, 0.5, [=] (Player& p) { p.settings.voice.vowel = vowel; p.settings.voice.size = size; })[0];
                const auto level = juce::Decibels::gainToDecibels (rms (voiced, 9600, 24000));
                INFO ("vowel " << vowel << ", size " << size << ", note " << note << ": " << level - dryLevel << " dB against the dry saw");
                CHECK (level - dryLevel > -7.0);
                CHECK (level - dryLevel < 2.0);
            }
}

TEST_CASE ("VOICE: the matrix moves VOWEL, SIZE, SHARP and MIX (a note's own resonators), and they're in its list", "[voice][mod]")
{
    using synth::mod::Field;
    // A route from MACRO 1 carrying VOWEL from OO to EE sounds as VOWEL set to EE.
    const auto route = [] (Field field, float from, float to, const std::function<void (synth::Settings&, float)>& set)
    {
        Player a, b;
        for (auto* p : { &a, &b })
        {
            p->settings.amp.envelope.sustain = 1.0f;
            p->settings.amp.envelope.attackMs = 0.0f;
            p->settings.voice.on = true;
        }
        set (a.settings, to);
        set (b.settings, from);
        juce::NormalisableRange<float> range { 0.0f, 1.0f };
        synth::mod::Route r;
        r.source = synth::mod::Source::macro1;
        r.target = { field, 0 };
        r.range = &range;
        r.base = from;
        r.amount = to - from;
        b.settings.modulation.routes = &r;
        b.settings.modulation.numRoutes = 1;
        b.settings.modulation.macros[0] = 1.0f;
        b.settings.modulation.usedSources = 1u << (int) synth::mod::Source::macro1;
        a.on (57);
        b.on (57);
        const auto x = a.render (0.4)[0], y = b.render (0.4)[0];
        double difference = 0.0, level = 0.0;
        for (size_t i = 2400; i < x.size(); ++i)
        {
            difference += (double) (x[i] - y[i]) * (x[i] - y[i]);
            level += (double) x[i] * x[i];
        }
        return 10.0 * std::log10 (juce::jmax (1.0e-30, difference) / juce::jmax (1.0e-30, level));
    };
    CHECK (route (Field::voiceVowel, 0.0f, 1.0f, [] (synth::Settings& s, float v) { s.voice.vowel = v; }) < -60.0);
    CHECK (route (Field::voiceSize, 0.0f, 0.8f, [] (synth::Settings& s, float v) { s.voice.size = v; }) < -60.0);
    CHECK (route (Field::voiceSharp, 0.2f, 0.9f, [] (synth::Settings& s, float v) { s.voice.sharp = v; }) < -60.0);
    CHECK (route (Field::voiceMix, 1.0f, 0.4f, [] (synth::Settings& s, float v) { s.voice.mix = v; }) < -60.0);

    ParagonX1AudioProcessor processor;
    for (const auto* id : { ParamIDs::voiceVowel, ParamIDs::voiceSize, ParamIDs::voiceSharp, ParamIDs::voiceMix })
    {
        INFO (id);
        CHECK (ModDestinations::find (id) != nullptr);
        CHECK (processor.getParameters().getParameter (id) != nullptr);
    }
}

TEST_CASE ("VOICE: the plugin's parameters reach the engine (on, vowel, an oscillator's routing)", "[voice]")
{
    ParagonX1AudioProcessor processor;
    auto& apvts = processor.getParameters();
    const auto set = [&apvts] (const juce::String& id, float value)
    {
        auto* p = apvts.getParameter (id);
        REQUIRE (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set (ParamIDs::voiceOn, 1.0f);
    set (ParamIDs::voiceVowel, 0.75f);
    set (ParamIDs::voiceSize, 0.9f);
    set (ParamIDs::osc (1, "Voice"), 0.0f);
    const auto s = processor.getSynthSettings();
    CHECK (s.voice.on);
    CHECK_THAT (s.voice.vowel, Catch::Matchers::WithinAbs (0.75, 1.0e-4));
    CHECK_THAT (s.voice.size, Catch::Matchers::WithinAbs (0.9, 1.0e-4));
    CHECK (s.oscillators[0].toVoice);
    CHECK_FALSE (s.oscillators[1].toVoice);
}

TEST_CASE ("CPU: a four-note chord of three-voice unison saws through the VOICE filter", "[voice][performance]")
{
    const auto cost = [] (bool on)
    {
        Player p;
        p.settings.amp.envelope.sustain = 1.0f;
        p.osc().unisonVoices = 3;
        p.osc().unisonMix = 0.6f;
        p.settings.voice.on = on;
        for (const auto note : { 48, 55, 60, 64 })
            p.on (note);
        p.render (0.1);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        p.render (2.0);
        return (juce::Time::getMillisecondCounterHiRes() - start) / 2000.0 * 100.0;
    };
    const auto plain = cost (false), voiced = cost (true);
    WARN ("CPU, 4 notes: " << plain << "% plain, " << voiced << "% with VOICE (one core, real time)");
    CHECK (voiced < 25.0);
}


