// SIROCCO (0.58.0): the answering harmony. Checked by what it plays: the scale's answers note by note (the table), the
// answer heard at its pitch and level beside the note, off and LEVEL 0 bit-identical, VOICES coming in one at a time,
// DELAY, letting go with the note, a mono lead's answer gliding with it, an arp's notes answered, DETUNE in cents.
#include "SynthTestUtils.h"

#include <catch2/catch_test_macros.hpp>

using namespace synth;

namespace
{
    constexpr int phrygianDominant = 14;   // arp::scaleNames
    constexpr int d = 2;                   // sirocco::keyNames

    int answerOf (int note, int interval, int layer = 0, int key = d, int scale = phrygianDominant)
    {
        std::array<int, 12> notes {};
        const auto count = sirocco::scaleNotes (scale, notes);
        return sirocco::harmonyNote (note, sirocco::stepsFor (interval, layer, count), key, scale);
    }

    Player sinePlayer()
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.amp.envelope.attackMs = 1.0f;
        p.settings.amp.envelope.releaseMs = 20.0f;
        auto& s = p.settings.sirocco;
        s.on = true;
        s.key = d;
        s.scale = phrygianDominant;
        s.interval = 0;   // 3RD ABOVE
        s.voices = 1;
        s.level = 1.0f;
        s.detuneCents = 0.0f;
        s.spread = 0.0f;
        return p;
    }
}

TEST_CASE ("SIROCCO: the answers keep to the scale", "[sirocco]")
{
    CHECK (arp::scaleNames[phrygianDominant] == "Phrygian Dominant");
    // D Phrygian dominant (D Eb F# G A Bb C): each note's 3rd above, a step of the scale apart from the next.
    const int notes[] = { 62, 63, 66, 67, 69, 70, 72, 74 };
    const int thirds[] = { 66, 67, 69, 70, 72, 74, 75, 78 };   // F# G A Bb C D Eb F#
    for (size_t i = 0; i < std::size (notes); ++i)
    {
        INFO ("note " << notes[i]);
        CHECK (answerOf (notes[i], 0) == thirds[i]);
    }
    CHECK (answerOf (62, 2) == 69);    // 5TH ABOVE: A
    CHECK (answerOf (62, 4) == 74);    // OCTAVE ABOVE: D
    CHECK (answerOf (62, 5) == 58);    // 3RD BELOW: Bb
    CHECK (answerOf (62, 7) == 55);    // 5TH BELOW: G
    CHECK (answerOf (62, 9) == 50);    // OCTAVE BELOW
    CHECK (answerOf (62, 0, 1) == 69); // VOICES: 3rd, then 5th, then 7th
    CHECK (answerOf (62, 0, 2) == 72);
    CHECK (answerOf (64, 0) == 68);    // E, outside the scale: Eb's answer (G) a semitone up, in parallel
    CHECK (answerOf (60, 0, 0, 0, 1) == 64);   // C major: C's 3rd is E
    CHECK (answerOf (62, 0, 0, 0, 1) == 65);   // ...D's is F
    CHECK (answerOf (36, 5, 0, d) == 33);      // low notes: C2's 3rd below in D Phrygian dominant is A1
}

TEST_CASE ("SIROCCO: the answer sounds at its note, 3 dB under, and off or LEVEL 0 changes nothing", "[sirocco]")
{
    auto p = sinePlayer();
    p.on (62);
    const auto out = p.render (1.6);
    const Spectrum spectrum (out[0], (size_t) (0.1 * sampleRate));
    const auto note = spectrum.at (noteHz (62)), answer = spectrum.at (noteHz (66)), wrong = spectrum.at (noteHz (65));   // F: not the scale's 3rd
    INFO ("D " << note << " dB, F# " << answer << " dB, F " << wrong << " dB");
    CHECK (std::abs (answer - note + 3.0) < 0.5);
    CHECK (wrong < note - 80.0);
    CHECK (p.engine.getNumActiveVoices() == 2);

    // Off, and on at LEVEL 0: the same samples as no SIROCCO at all, and no extra voice.
    const auto plain = [] (bool on, float level)
    {
        auto q = sinePlayer();
        q.settings.sirocco.on = on;
        q.settings.sirocco.level = level;
        q.on (62);
        auto r = q.render (0.3);
        CHECK (q.engine.getNumActiveVoices() == 1);
        return r[0];
    };
    auto none = sinePlayer();
    none.settings.sirocco = {};
    none.on (62);
    const auto reference = none.render (0.3)[0];
    CHECK (plain (false, 1.0f) == reference);
    CHECK (plain (true, 0.0f) == reference);
}

TEST_CASE ("SIROCCO: VOICES come in one at a time as LEVEL rises", "[sirocco]")
{
    const auto levels = [] (float level)
    {
        auto p = sinePlayer();
        p.settings.sirocco.voices = 3;
        p.settings.sirocco.level = level;
        p.on (62);
        const auto out = p.render (1.6);
        const Spectrum spectrum (out[0], (size_t) (0.1 * sampleRate));
        const auto note = spectrum.at (noteHz (62));
        return std::array<double, 3> { spectrum.at (noteHz (66)) - note, spectrum.at (noteHz (69)) - note, spectrum.at (noteHz (72)) - note };
    };
    const auto third = levels (1.0f / 3.0f);
    INFO ("at a third: F# " << third[0] << ", A " << third[1] << ", C " << third[2]);
    CHECK (std::abs (third[0] + 3.0) < 0.5);
    CHECK (third[1] < -90.0);
    CHECK (third[2] < -90.0);
    const auto full = levels (1.0f);
    INFO ("at full: F# " << full[0] << ", A " << full[1] << ", C " << full[2]);
    CHECK (std::abs (full[0] - full[1]) < 1.0);
    CHECK (std::abs (full[0] - full[2]) < 1.0);
}

TEST_CASE ("SIROCCO: DELAY, and letting go with the note", "[sirocco]")
{
    auto p = sinePlayer();
    p.settings.sirocco.delayMs = 200.0f;
    p.on (62);
    p.render (0.19);
    CHECK (p.engine.getNumActiveVoices() == 1);   // the answer not yet
    p.render (0.02);
    CHECK (p.engine.getNumActiveVoices() == 2);   // 200 ms in: the answer

    p.off (62);
    p.render (0.3);
    CHECK (p.engine.getNumActiveVoices() == 0);

    // Let go before the DELAY is up: no answer at all.
    auto q = sinePlayer();
    q.settings.sirocco.delayMs = 300.0f;
    q.on (62);
    q.render (0.1);
    q.off (62);
    q.render (0.5);
    CHECK (q.engine.getNumActiveVoices() == 0);
}

TEST_CASE ("SIROCCO: a mono lead's answer glides with it; an arp's notes are answered", "[sirocco]")
{
    auto p = sinePlayer();
    p.settings.play.mode = PlayMode::mono;
    p.settings.play.legato = true;
    p.settings.play.glideMs = 30.0f;
    p.on (62);
    p.render (0.3);
    p.on (67);   // G, legato: the answer moves from F# to Bb
    const auto out = p.render (1.6);
    CHECK (p.engine.getNumActiveVoices() == 2);
    const Spectrum spectrum (out[0], (size_t) (0.1 * sampleRate));
    INFO ("G " << spectrum.at (noteHz (67)) << " dB, Bb " << spectrum.at (noteHz (70)) << " dB, F# " << spectrum.at (noteHz (66)) << " dB");
    CHECK (std::abs (spectrum.at (noteHz (70)) - spectrum.at (noteHz (67)) + 3.0) < 1.0);   // (the window reads a glided note a little low)
    CHECK (spectrum.at (noteHz (66)) < -90.0);
    p.off (67);
    p.off (62);
    p.render (0.3);
    CHECK (p.engine.getNumActiveVoices() == 0);

    // ARP 1 playing the held D on oscillator 1: its notes answered too.
    auto a = sinePlayer();
    a.osc().arp = 1;
    a.settings.arps[0].on = true;
    a.on (62);
    const auto arped = a.render (1.0);
    const auto f = levelAt (arped[0], noteHz (66), (size_t) (0.1 * sampleRate), (size_t) (0.8 * sampleRate));
    INFO ("F# under the arp " << db (f) << " dB");
    CHECK (db (f) > -25.0);
}

TEST_CASE ("SIROCCO: DETUNE puts the answer that many cents sharp", "[sirocco]")
{
    auto p = sinePlayer();
    p.settings.sirocco.detuneCents = 20.0f;
    p.settings.sirocco.level = 1.0f;
    p.on (62);
    const auto out = p.render (0.6);
    const auto from = (size_t) (0.2 * sampleRate), length = (size_t) (0.35 * sampleRate);
    const auto sharp = levelAt (out[0], noteHz (66.2), from, length);
    const auto plainPitch = levelAt (out[0], noteHz (66.0), from, length);
    INFO ("F# +20 ct " << db (sharp) << " dB, F# in tune " << db (plainPitch) << " dB");
    CHECK (sharp > plainPitch * 2.0);
}
