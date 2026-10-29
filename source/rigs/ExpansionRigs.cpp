#include "FactoryRigs.h"
#include "Expansions.h"
#include "RigBuilder.h"

#include "dsp/eq/EqDesign.h"
#include "dsp/effects/Delay.h"
#include "dsp/effects/Drive.h"
#include "dsp/effects/Glitch.h"
#include "dsp/effects/LoFi.h"
#include "dsp/effects/Modulation.h"
#include "dsp/effects/Reverb.h"
#include "dsp/synth/Wavetable.h"

// The built-in expansions (Expansions.h): themed sets of 15 presets, 3 each of leads, pads, basses, keys and arps,
// each set in a bank of its own. Our own sounds, set up from common synth recipes for the sound the theme names,
// not copies of anyone's presets, and not tuned by ear (they can't be listened to here). Every preset names its
// three MACROs: 1 the brightness or swell, 2 SPACE, 3 its character. (The macros reach the synth's
// knobs, and since 0.25.0 the effects' amounts: MACRO 2 is SPACE on every preset.)

using namespace rigbuilder;

namespace
{
    using SP = synth::mod::ShapePoint;
    using synth::FilterType;
    using MT = effects::Modulation::Type;
    using RT = effects::Reverb::Type;

    // Effects choices by name, so a reordered list can't change a preset silently (the tests check they're found).
    float delaySync (const char* name) { return (float) effects::Delay::syncNames.indexOf (name); }
    float division (const char* name) { return (float) synth::seq::divisionNames.indexOf (name); }
    float chord (const char* name) { return (float) synth::arp::chordNames.indexOf (name); }
    float table (const char* name) { return (float) synth::factoryTables::names.indexOf (name); }
    constexpr auto wavetable = (float) synth::OscWave::wavetable, triangle = (float) synth::OscWave::triangle;

    struct Expansion
    {
        int bank;
        std::vector<Rig>& rigs;

        Builder make (const juce::String& name, int number, const juce::StringArray& tags, const juce::String& notes)
        {
            Builder b (name, number, tags, notes);
            b.rig.bank = bank;
            b.rig.tags.add (expansions::list[(size_t) (bank - Rig::numBanks)].name);
            return b;
        }
        void add (Builder& b) { rigs.push_back (b.rig); }
    };

    void route (Builder& b, const char* source, const juce::String& destination, float amount, float lagMs = 0.0f)
    {
        b.rig.modulation.routes.push_back ({ true, source, destination, amount, false, lagMs, 0.0f });
    }

    void macros (Builder& b, const char* one, const char* two, const char* three)
    {
        b.rig.modulation.macroNames[0] = one;
        b.rig.modulation.macroNames[1] = two;
        b.rig.modulation.macroNames[2] = three;
    }

    // MACRO 2, SPACE (0.25.0): more reverb, and more and longer echoes where the preset has DELAY on (the effects'
    // amounts are matrix destinations now). At 0 it changes nothing: the preset sounds as set.
    void space (Builder& b)
    {
        route (b, "macro2", ParamIDs::reverbMix, 0.3f);
        const auto delayOn = b.rig.parameters.find (ParamIDs::delayOn);
        if (delayOn != b.rig.parameters.end() && delayOn->second > 0.5f)
        {
            route (b, "macro2", ParamIDs::delayMix, 0.2f);
            route (b, "macro2", ParamIDs::delayFeedback, 0.15f);
        }
    }

    void eqBand (Builder& b, int band, eq::Shape shape, float hz, float db, float q = 0.7f)
    {
        b.set (ParamIDs::eqOn, 1.0f)
         .set (ParamIDs::eqBand (band, "Used"), 1.0f).set (ParamIDs::eqBand (band, "Shape"), (float) (int) shape)
         .set (ParamIDs::eqBand (band, "Freq"), hz).set (ParamIDs::eqBand (band, "Gain"), db).set (ParamIDs::eqBand (band, "Q"), q);
    }

    void reverb (Builder& b, RT type, float size, float mix, float preDelayMs = 20.0f, float tone = 5.0f)
    {
        b.set (ParamIDs::reverbOn, 1.0f).set (ParamIDs::reverbType, (float) (int) type).set (ParamIDs::reverbSize, size)
         .set (ParamIDs::reverbMix, mix).set (ParamIDs::reverbPreDelay, preDelayMs).set (ParamIDs::reverbTone, tone);
    }

    void delay (Builder& b, const char* sync, float feedback, float mix, bool pingPong, float tone = 5.0f)
    {
        b.set (ParamIDs::delayOn, 1.0f).set (ParamIDs::delaySync, delaySync (sync)).set (ParamIDs::delayFeedback, feedback)
         .set (ParamIDs::delayMix, mix).set (ParamIDs::delayPingPong, pingPong ? 1.0f : 0.0f).set (ParamIDs::delayTone, tone);
    }

    void chorus (Builder& b, const ParamIDs::ModIds& ids, MT type, float rate, float depth, float mix)
    {
        b.set (ids.on, 1.0f).set (ids.type, (float) (int) type).set (ids.rate, rate).set (ids.depth, depth).set (ids.mix, mix);
    }

    // NEON RAIN: inspired by the 1982 neo-noir sci-fi film score (brass from a big polysynth, rain-soaked pads, smoky
    // leads, glassy keys, slow neon pulses). No titles, names or music from the film: our own sounds.
    void neonRain (Expansion x)
    {
        // ---- LEADS -------------------------------------------------------------------------------------------
        auto brass = x.make ("Brass Requiem", 1, { "Lead", "Brass", "Mono", "Aftertouch" },
            "A mono brass lead in the manner of the big late-'70s polysynths: a saw and a pulse a few cents apart, the "
            "filter swelling open over a third of a second, a slow glide between notes. Press harder after the note "
            "(aftertouch) to open it further and bring in vibrato. MACROS: BRIGHT (the filter), SPACE (more "
            "hall and echoes, the saw wider), GROWL (SHAPER 1's tube). Chorus, quarter-note echoes and a hall.");
        brass.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.35f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.3f)
             .osc (0, "Volume", 0.6f).osc (0, "VibRate", 5.5f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.42f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.45f)
             .osc (1, "VibRate", 5.5f)
             .shaper (0, synth::ShaperModel::tube, 0.08f)
             .filter (FilterType::lp24, 650.0f, 0.22f, 0.35f).filterEnvelope (350.0f, 900.0f, 0.55f, 600.0f)
             .set (ParamIDs::fltKey, 0.4f)
             .ampEnvelope (60.0f, 600.0f, 0.9f, 500.0f)
             .mono (120.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always);
        route (brass, "aftertouch", ParamIDs::fltCutoff, 0.22f, 80.0f);
        route (brass, "aftertouch", ParamIDs::osc (0, "VibAmount"), 0.22f, 120.0f);
        route (brass, "aftertouch", ParamIDs::osc (1, "VibAmount"), 0.22f, 120.0f);
        route (brass, "macro1", ParamIDs::fltCutoff, 0.3f);
        route (brass, "macro2", ParamIDs::osc (0, "UniSpread"), 0.7f);   // wider, with the room
        route (brass, "macro3", ParamIDs::shp (0, "Drive"), 0.6f);
        macros (brass, "BRIGHT", "SPACE", "GROWL");
        chorus (brass, ParamIDs::modIds, MT::chorus, 0.4f, 0.3f, 0.35f);
        brass.set (ParamIDs::modStereo, 1.0f);
        delay (brass, "1/4", 0.3f, 0.16f, false, 4.0f);
        reverb (brass, RT::hall, 0.7f, 0.3f, 30.0f);
        space (brass);
        brass.set (ParamIDs::patchLevel, -5.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (brass);

        auto reeds = x.make ("Smoke And Reeds", 2, { "Lead", "Breath", "Mono", "Mod Wheel" },
            "A breathy, sax-like lead: a hollow pulse with FORMANT moving its body, a little filtered noise for the "
            "breath (more with velocity), vibrato that comes in after the note starts. The MOD WHEEL growls it through "
            "SHAPER 1 (DIODE) and deepens the vibrato. Tape echoes in dotted eighths and a plate. MACROS: TONE, "
            "SPACE (more plate and echoes), GROWL (the diode and more breath).");
        reeds.osc (0, "Wave", pulse).osc (0, "Width", 0.32f).osc (0, "Formant", 0.18f).osc (0, "FormantMode", 2.0f)
             .osc (0, "Noise", 0.05f).osc (0, "NoiseColour", 0.15f).osc (0, "NoiseWidth", 2.0f).osc (0, "Volume", 0.7f)
             .osc (0, "VibAmount", 12.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 600.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.3f)
             .osc (1, "VibAmount", 12.0f).osc (1, "VibRate", 5.0f).osc (1, "VibFade", 600.0f)
             .shaper (0, synth::ShaperModel::diode, 0.05f)
             .filter (FilterType::lp12, 1700.0f, 0.2f, 0.25f).filterEnvelope (40.0f, 400.0f, 0.6f, 250.0f)
             .set (ParamIDs::fltVel, 0.4f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (30.0f, 300.0f, 0.85f, 250.0f)
             .mono (50.0f);
        route (reeds, "velocity", ParamIDs::osc (0, "Noise"), 0.08f);
        route (reeds, "modWheel", ParamIDs::shp (0, "Drive"), 0.55f);
        route (reeds, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        route (reeds, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (reeds, "macro3", ParamIDs::osc (0, "Noise"), 0.15f);   // more breath with the growl
        route (reeds, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (reeds, "TONE", "SPACE", "GROWL");
        delay (reeds, "1/8 dotted", 0.35f, 0.22f, false, 4.0f);
        reeds.set (ParamIDs::delayType, (float) (int) effects::Delay::Type::tape);
        reverb (reeds, RT::plate, 0.5f, 0.2f);
        space (reeds);
        reeds.set (ParamIDs::patchLevel, 1.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (reeds);

        auto ribbon = x.make ("Wire Ribbon", 3, { "Lead", "Pitch", "Glide", "Mono" },
            "A pure, singing lead like a ribbon controller's: a triangle with a sine an octave up, every note scooping "
            "up into pitch (the PITCH envelope, from almost two semitones under), a long curved glide between notes, "
            "slow vibrato after the note sets in, a vast cathedral and ping-pong echoes. MACROS: BRIGHT, SPACE (more "
            "cathedral and echoes), SCOOP (the scoop's depth). MOD WHEEL: more vibrato.");
        ribbon.osc (0, "Wave", triangle).osc (0, "Volume", 0.75f).osc (0, "VibAmount", 14.0f).osc (0, "VibRate", 5.2f)
              .osc (0, "VibFade", 800.0f).osc (0, "VibCurve", 0.5f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.22f)
              .osc (1, "VibAmount", 14.0f).osc (1, "VibRate", 5.2f).osc (1, "VibFade", 800.0f).osc (1, "VibCurve", 0.5f)
              .filter (FilterType::lp12, 4000.0f, 0.1f, 0.0f)
              .ampEnvelope (15.0f, 500.0f, 0.9f, 700.0f)
              .mono (180.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.4f)
              .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 3.0f);
        ribbon.rig.modulation.pitchEnvelope = { { SP { 0.0f, 0.2f, -0.5f }, SP { 0.14f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (ribbon, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (ribbon, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.25f);
        route (ribbon, "modWheel", ParamIDs::osc (1, "VibAmount"), 0.25f);
        route (ribbon, "macro3", ParamIDs::pitchEnvRange, 0.15f);
        macros (ribbon, "BRIGHT", "SPACE", "SCOOP");
        delay (ribbon, "1/4", 0.35f, 0.18f, true, 5.0f);
        ribbon.set (ParamIDs::delayToReverb, 0.4f);
        reverb (ribbon, RT::cathedral, 0.45f, 0.32f, 60.0f, 4.0f);
        space (ribbon);
        ribbon.set (ParamIDs::patchLevel, -3.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ribbon);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto choir = x.make ("Rain Cathedral", 4, { "Pad", "Choir", "Wavetable", "Travel" },
            "A wordless choir: the Vowels table in unison, opening from \"oo\" to \"oh\" over the first four seconds "
            "of each note (TRAVEL), with a slow LFO keeping the vowel breathing, a darker saw an octave down under it, "
            "a wide ensemble and a long cathedral. MACROS: BRIGHT, SPACE (more cathedral), VOWEL (towards \"ee\"). MOD "
            "WHEEL: drift (the unison detune).");
        choir.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 1.0f)
             .osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.2f).osc (0, "UniSpread", 0.9f)
             .osc (0, "Volume", 0.55f)
             .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::oneShot).osc (0, "TrRate", 0.25f)
             .osc (0, "TrStrength", -0.25f).osc (0, "TrSmooth", 0.2f)
             .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.18f)
             .osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.6f).osc (1, "UniDetune", 0.18f).osc (1, "UniSpread", 0.7f)
             .filter (FilterType::lp12, 2400.0f, 0.12f, 0.0f).set (ParamIDs::fltKey, 0.3f)
             .ampEnvelope (1200.0f, 2000.0f, 0.9f, 2800.0f)
             .set (ParamIDs::lfo (0, "Rate"), 0.12f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        choir.rig.modulation.travel[0].envelope = { { SP { 0.0f, 0.0f, -0.3f }, SP { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (choir, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.06f);
        route (choir, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (choir, "macro3", ParamIDs::osc (0, "WtIndex"), -0.4f);   // the table runs ah, eh, ee, oh, oo
        route (choir, "modWheel", ParamIDs::osc (0, "UniDetune"), 0.35f);
        macros (choir, "BRIGHT", "SPACE", "VOWEL");
        chorus (choir, ParamIDs::modIds, MT::dimension, 0.3f, 0.5f, 0.4f);
        reverb (choir, RT::cathedral, 0.35f, 0.35f, 40.0f, 4.5f);
        eqBand (choir, 0, eq::Shape::lowCut, 110.0f, 0.0f);
        space (choir);
        choir.set (ParamIDs::patchLevel, 0.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (choir);

        auto mist = x.make ("Ion Mist", 5, { "Pad", "Drone", "Travel", "Inserts", "Noise" },
            "A dark, slowly turning drone: the Comb Sweep table drifting back and forth (TRAVEL, ping pong, about 14 "
            "seconds a sweep) with a sub under it, and rain on top: oscillator 2 plays only filtered noise, through "
            "insert bus A's worn cassette and a slow auto-pan, so the hiss crackles and moves and stops with the note. "
            "A very slow LFO breathes the filter. MACROS: BRIGHT, SPACE (more cathedral), RAIN (the noise). MOD WHEEL: "
            "motion (how far the sweep travels).");
        mist.osc (0, "Wave", wavetable).osc (0, "Table", table ("Comb Sweep")).osc (0, "WtIndex", 0.15f)
            .osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.8f)
            .osc (0, "Sub", 0.35f).osc (0, "Volume", 0.55f)
            .osc (0, "TrOn", 1.0f).osc (0, "TrPatternMode", (float) synth::travel::Mode::pingPong)
            .osc (0, "TrMode", (float) synth::travel::Mode::pingPong).osc (0, "TrRate", 0.07f).osc (0, "TrStrength", 0.6f)
            .osc (0, "TrSmooth", 0.3f).osc (0, "TrTrigger", (float) synth::travel::Trigger::firstNote)
            .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.35f).osc (1, "NoiseColour", 0.25f)
            .osc (1, "Volume", 0.7f).osc (1, "Insert", 1.0f).osc (1, "Filter", 0.0f)
            .filter (FilterType::lp24, 900.0f, 0.3f, 0.0f)
            .ampEnvelope (2500.0f, 3000.0f, 1.0f, 3500.0f)
            .set (ParamIDs::lfo (1, "Rate"), 0.05f).set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global)
            .set (ParamIDs::ins (0, "LofiOn"), 1.0f).set (ParamIDs::ins (0, "LofiType"), (float) (int) effects::LoFi::Type::cassette)
            .set (ParamIDs::ins (0, "LofiAmount"), 6.0f).set (ParamIDs::ins (0, "LofiNoise"), 0.0f)   // no hiss of its own: it'd play on
            .set (ParamIDs::ins (0, "SendReverb"), 0.5f).set (ParamIDs::ins (0, "SendDelay"), 0.0f);
        mist.rig.modulation.travel[0].envelope = { { SP { 0.0f, 0.0f, 0.0f }, SP { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        chorus (mist, ParamIDs::insModIds (0), MT::autoPan, 0.15f, 0.6f, 1.0f);
        route (mist, "lfo2", ParamIDs::fltCutoff, 0.12f);
        route (mist, "macro1", ParamIDs::fltCutoff, 0.3f);
        route (mist, "macro3", ParamIDs::osc (1, "Noise"), 0.4f);
        route (mist, "modWheel", ParamIDs::osc (0, "TrStrength"), 0.2f);
        macros (mist, "BRIGHT", "SPACE", "RAIN");
        reverb (mist, RT::cathedral, 0.3f, 0.3f, 30.0f, 3.5f);
        space (mist);
        mist.set (ParamIDs::patchLevel, -6.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (mist);

        auto strings = x.make ("Memory Strings", 6, { "Pad", "Strings", "Unison", "Aftertouch" },
            "An old string machine's ensemble: two wide unison saws (the upper an octave up), a slow bow of an attack "
            "and the filter following it open, a chorus and a dimension doubler for the shimmer, a hall, and ECHELON "
            "trimming the lows and the very top for a worn, vintage colour. Press after the note (aftertouch) to swell "
            "it louder and brighter. MACROS: BRIGHT, SPACE (more hall), ENSEMBLE (the detune). MOD WHEEL: "
            "the upper saw.");
        strings.osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.85f).osc (0, "UniDetune", 0.32f).osc (0, "UniSpread", 1.0f)
               .osc (0, "Volume", 0.45f)
               .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.2f)
               .osc (1, "UniVoices", 5.0f).osc (1, "UniMix", 0.8f).osc (1, "UniDetune", 0.25f).osc (1, "UniSpread", 1.0f)
               .filter (FilterType::lp12, 2800.0f, 0.05f, 0.15f).filterEnvelope (900.0f, 1500.0f, 0.7f, 1500.0f)
               .set (ParamIDs::fltKey, 0.4f)
               .ampEnvelope (700.0f, 1500.0f, 0.9f, 1800.0f)
               .set (ParamIDs::ampVolume, 0.8f);
        route (strings, "aftertouch", ParamIDs::ampVolume, 0.12f, 150.0f);
        route (strings, "aftertouch", ParamIDs::fltCutoff, 0.12f, 150.0f);
        route (strings, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (strings, "macro3", ParamIDs::osc (0, "UniDetune"), 0.25f);
        route (strings, "modWheel", ParamIDs::osc (1, "Volume"), 0.2f);
        macros (strings, "BRIGHT", "SPACE", "ENSEMBLE");
        chorus (strings, ParamIDs::modIds, MT::chorus, 0.7f, 0.55f, 0.55f);
        strings.set (ParamIDs::modStereo, 1.0f);
        chorus (strings, ParamIDs::mod2Ids, MT::dimension, 0.3f, 0.4f, 0.35f);
        reverb (strings, RT::hall, 0.75f, 0.3f, 25.0f, 4.5f);
        eqBand (strings, 0, eq::Shape::lowCut, 120.0f, 0.0f);
        eqBand (strings, 1, eq::Shape::highShelf, 9000.0f, -2.0f);
        space (strings);
        strings.set (ParamIDs::patchLevel, -7.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (strings);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto pulseBass = x.make ("Undercity Pulse", 7, { "Bass", "Sub", "Modulation", "Rack" },
            "A deep bass that breathes with your song: a clean sine sub an octave down (past the filter, no effects) "
            "under a dark, slightly driven saw whose filter LFO 1 opens and closes in half notes. CONSTRICTOR (OPTO) "
            "keeps it even. MACROS: OPEN (the filter), SPACE (more room, on the saw only), GRIT (the "
            "filter's drive). MOD WHEEL: resonance.");
        pulseBass.osc (0, "Volume", 0.55f).osc (0, "PhaseRand", 0.0f).osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.3f)
                 .osc (0, "UniDetune", 0.08f).osc (0, "SendReverb", 0.2f).osc (0, "SendDelay", 0.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.6f)
                 .osc (1, "PhaseRand", 0.0f).osc (1, "Filter", 0.0f).osc (1, "SendReverb", 0.0f).osc (1, "SendDelay", 0.0f)
                 .filter (FilterType::lp24, 320.0f, 0.35f, 0.2f).filterEnvelope (5.0f, 600.0f, 0.3f, 200.0f)
                 .set (ParamIDs::fltDrive, 0.2f)
                 .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sine)
                 .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
                 .set (ParamIDs::lfo (0, "Division"), division ("1/2"))
                 .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
                 .set (ParamIDs::lfo (0, "Offset"), 1.0f)
                 .ampEnvelope (4.0f, 300.0f, 1.0f, 200.0f)
                 .mono (25.0f)
                 .set (ParamIDs::compOn, 1.0f).set (ParamIDs::compCharacter, 1.0f).set (ParamIDs::compThreshold, -20.0f)
                 .set (ParamIDs::compRatio, 3.0f).set (ParamIDs::compMakeUp, 3.0f);
        route (pulseBass, "lfo1", ParamIDs::fltCutoff, 0.18f);
        route (pulseBass, "macro1", ParamIDs::fltCutoff, 0.3f);
        route (pulseBass, "modWheel", ParamIDs::fltRes, 0.35f);
        route (pulseBass, "macro3", ParamIDs::fltDrive, 0.5f);
        macros (pulseBass, "OPEN", "SPACE", "GRIT");
        reverb (pulseBass, RT::room, 0.3f, 0.2f);
        space (pulseBass);
        pulseBass.set (ParamIDs::patchLevel, 1.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pulseBass);

        auto thunder = x.make ("Distant Thunder", 8, { "Bass", "Hit", "Pitch", "Noise", "Reverb" },
            "A booming low hit: a sine that drops an octave into its note in a quarter of a second (the PITCH "
            "envelope), a triangle an octave up so small speakers hear it, and a dark rumble of noise rolling off "
            "behind, warmed by SHAPER 1's tube, into a long hall (the sub itself stays dry). Let it ring. MACROS: "
            "BRIGHT, SPACE (more hall), DROP (how far the pitch falls). MOD WHEEL: more rumble.");
        thunder.osc (0, "Wave", sine).osc (0, "Volume", 0.9f).osc (0, "PhaseRand", 0.0f).osc (0, "Filter", 0.0f)
               .osc (0, "SendReverb", 0.1f).osc (0, "SendDelay", 0.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f)
               .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.6f).osc (2, "NoiseColour", 0.9f)
               .osc (2, "Volume", 1.0f).osc (2, "PitchEnv", 0.0f)
               .shaper (0, synth::ShaperModel::tube, 0.3f)
               .filter (FilterType::lp24, 380.0f, 0.15f, 0.35f).filterEnvelope (0.0f, 1200.0f, 0.0f, 900.0f)
               .ampEnvelope (1.0f, 2500.0f, 0.0f, 1500.0f).set (ParamIDs::ampDecayCurve, -0.4f)
               .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        thunder.rig.modulation.pitchEnvelope = { { SP { 0.0f, 1.0f, -0.6f }, SP { 0.25f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (thunder, "macro1", ParamIDs::fltCutoff, 0.3f);
        route (thunder, "modWheel", ParamIDs::osc (2, "Noise"), 0.4f);
        route (thunder, "macro3", ParamIDs::pitchEnvRange, 0.3f);
        macros (thunder, "BRIGHT", "SPACE", "DROP");
        reverb (thunder, RT::hall, 0.9f, 0.35f, 15.0f, 3.0f);
        space (thunder);
        thunder.set (ParamIDs::patchLevel, -1.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (thunder);

        auto velvet = x.make ("Velvet Noir", 9, { "Bass", "Warm", "Drive", "Inserts", "Glide" },
            "A round, warm bass for slow, smoky lines: a triangle with a sine sub and a quiet saw a few cents off for "
            "edge, the filter plucked open a little more the harder you play, a gentle glide when notes overlap, "
            "and insert bus A's RABID driven lightly for warmth. ECHELON lifts the lows, CONSTRICTOR (OPTO) evens "
            "it. MACROS: TONE, SPACE (a room, none until you turn it up), FAT (the sub). MOD "
            "WHEEL: bite (the filter envelope).");
        velvet.osc (0, "Wave", triangle).osc (0, "Volume", 0.7f).osc (0, "Sub", 0.4f).osc (0, "PhaseRand", 0.0f)
              .osc (0, "Insert", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Fine", 5.0f).osc (1, "Volume", 0.28f).osc (1, "Insert", 1.0f)
              .filter (FilterType::lp24, 700.0f, 0.15f, 0.25f).filterEnvelope (0.0f, 350.0f, 0.4f, 150.0f)
              .set (ParamIDs::fltVel, 0.3f)
              .ampEnvelope (3.0f, 400.0f, 0.85f, 180.0f)
              .mono (70.0f)
              .set (ParamIDs::ins (0, "DriveOn"), 1.0f).set (ParamIDs::ins (0, "DriveModel"), (float) (int) effects::Drive::Model::rabid)
              .set (ParamIDs::ins (0, "DriveGain"), 2.5f).set (ParamIDs::ins (0, "DriveTone"), 4.0f)
              .set (ParamIDs::ins (0, "DriveLevel"), -4.0f)
              .set (ParamIDs::ins (0, "SendReverb"), 0.5f).set (ParamIDs::ins (0, "SendDelay"), 0.0f)
              .set (ParamIDs::compOn, 1.0f).set (ParamIDs::compCharacter, 1.0f).set (ParamIDs::compThreshold, -18.0f)
              .set (ParamIDs::compRatio, 2.5f).set (ParamIDs::compMakeUp, 2.0f);
        eqBand (velvet, 0, eq::Shape::lowShelf, 80.0f, 1.5f);
        eqBand (velvet, 1, eq::Shape::bell, 300.0f, -1.5f, 1.0f);
        route (velvet, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (velvet, "modWheel", ParamIDs::fltEnv, 0.3f);
        route (velvet, "macro3", ParamIDs::osc (0, "Sub"), 0.25f);
        macros (velvet, "TONE", "SPACE", "FAT");
        reverb (velvet, RT::room, 0.4f, 0.0f);   // silent until SPACE brings it in
        space (velvet);
        velvet.set (ParamIDs::patchLevel, 6.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (velvet);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto piano = x.make ("Electric Rain Piano", 10, { "Keys", "FM", "Electric Piano", "Chorus" },
            "An FM electric piano: a sine modulated by oscillator 2 (a sine, not heard itself), the FM bright at the "
            "strike and settling (ENV 1) and brighter the harder you play, a soft triangle tine an octave up, a "
            "tremolo panning in eighth notes, a chorus and a plate. MACROS: BARK (the FM), SPACE (more plate), "
            "DETUNE (the modulator's tuning: a slow beating). MOD WHEEL: more tine.");
        piano.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.08f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.1f)
             .ampEnvelope (2.0f, 2500.0f, 0.2f, 500.0f).set (ParamIDs::ampVel, 0.6f)
             .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::sustain);
        piano.rig.modulation.envelopes[0] = { { SP { 0.0f, 1.0f, -0.5f }, SP { 0.9f, 0.15f, 0.0f } }, 1, -1, -1 };
        route (piano, "env1", ParamIDs::osc (0, "FmAmount"), 0.28f);
        route (piano, "velocity", ParamIDs::osc (0, "FmAmount"), 0.18f);
        route (piano, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (piano, "modWheel", ParamIDs::osc (2, "Volume"), 0.15f);
        route (piano, "macro3", ParamIDs::osc (1, "Fine"), 0.04f);
        macros (piano, "BARK", "SPACE", "DETUNE");
        chorus (piano, ParamIDs::modIds, MT::autoPan, 1.0f, 0.4f, 1.0f);
        piano.set (ParamIDs::modSync, delaySync ("1/8"));
        chorus (piano, ParamIDs::mod2Ids, MT::chorus, 0.5f, 0.3f, 0.35f);
        reverb (piano, RT::plate, 0.45f, 0.2f);
        space (piano);
        piano.set (ParamIDs::patchLevel, -7.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (piano);

        auto chimes = x.make ("Glass Chimes", 11, { "Keys", "FM", "Bell", "Delay" },
            "Long glass bells: a sine modulated by oscillator 2 at an uneven ratio (21 semitones up, a hair sharp: "
            "the clangy, bell-like overtones), the FM fading slower than the note so the strike shines then mellows, a "
            "high sine shimmering above, ping-pong echoes in dotted eighths running into a hall. MACROS: METAL (the "
            "FM), SPACE (more hall and echoes), DETUNE. MOD WHEEL: more shimmer.");
        chimes.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
              .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.12f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 21.0f).osc (1, "Fine", 14.0f)
              .osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
              .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 24.0f).osc (2, "Fine", 4.0f).osc (2, "Volume", 0.1f)
              .ampEnvelope (1.0f, 4500.0f, 0.0f, 2500.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f)
              .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        chimes.rig.modulation.envelopes[0] = { { SP { 0.0f, 1.0f, -0.6f }, SP { 2.5f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (chimes, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (chimes, "velocity", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (chimes, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (chimes, "modWheel", ParamIDs::osc (2, "Volume"), 0.2f);
        route (chimes, "macro3", ParamIDs::osc (0, "Fine"), 0.04f);
        macros (chimes, "METAL", "SPACE", "DETUNE");
        chorus (chimes, ParamIDs::modIds, MT::chorus, 0.3f, 0.25f, 0.25f);
        delay (chimes, "1/8 dotted", 0.45f, 0.28f, true, 7.0f);
        chimes.set (ParamIDs::delayToReverb, 0.4f);
        reverb (chimes, RT::hall, 0.7f, 0.28f, 30.0f, 6.0f);
        space (chimes);
        chimes.set (ParamIDs::patchLevel, -2.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chimes);

        auto nightKeys = x.make ("Night Shift Keys", 12, { "Keys", "Brass", "Velocity", "Aftertouch" },
            "Soft, brassy poly keys for chords: three detuned saws and a pulse through a filter that blooms open with "
            "a slight delay, more the harder you play, and further when you press after the note (aftertouch). A "
            "dark chorus, a hall and a quiet echo. MACROS: BRIGHT, SPACE (more hall and echoes, wider), BRASS (the filter "
            "envelope).");
        nightKeys.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.4f)
                 .osc (0, "Volume", 0.5f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Fine", -5.0f).osc (1, "Volume", 0.4f)
                 .filter (FilterType::lp24, 700.0f, 0.2f, 0.3f).filterEnvelope (120.0f, 900.0f, 0.35f, 400.0f)
                 .set (ParamIDs::fltVel, 0.5f).set (ParamIDs::fltKey, 0.4f)
                 .ampEnvelope (15.0f, 1500.0f, 0.6f, 600.0f);
        route (nightKeys, "aftertouch", ParamIDs::fltCutoff, 0.18f, 100.0f);
        route (nightKeys, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (nightKeys, "macro2", ParamIDs::osc (0, "UniSpread"), 0.6f);   // wider, with the room
        route (nightKeys, "macro3", ParamIDs::fltEnv, 0.3f);
        macros (nightKeys, "BRIGHT", "SPACE", "BRASS");
        chorus (nightKeys, ParamIDs::modIds, MT::carbonite, 0.5f, 0.35f, 0.5f);
        delay (nightKeys, "1/4", 0.25f, 0.1f, false, 4.0f);
        reverb (nightKeys, RT::hall, 0.55f, 0.22f);
        space (nightKeys);
        nightKeys.set (ParamIDs::patchLevel, -11.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (nightKeys);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto grid = x.make ("Neon Grid", 13, { "Arp", "Pulse", "Sync", "Delay" },
            "Hold a chord: ARP 1 walks it up and down two octaves in slow eighth notes, accents on the beats and an "
            "octave jump each half bar, through a resonant filter that LFO 1 sweeps over each bar in time with your "
            "song; ping-pong echoes in dotted eighths fill the gaps. A saw over a pulse an octave down. MACROS: OPEN, "
            "SPACE (more hall and echoes), OCTAVE (the low pulse). MOD WHEEL: resonance.");
        grid.osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f).osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f)
            .osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.6f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.35f).osc (1, "Transpose", -12.0f)
            .osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
            .filter (FilterType::lp24, 800.0f, 0.4f, 0.35f).filterEnvelope (0.0f, 220.0f, 0.1f, 200.0f)
            .ampEnvelope (2.0f, 300.0f, 0.4f, 250.0f)
            .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle)
            .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
            .set (ParamIDs::lfo (0, "Division"), division ("1/1"))
            .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
            .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.5f);
        {
            auto& p = grid.rig.modulation.arps[0].patterns[0];
            p = {};
            for (int k = 0; k < 16; ++k)
            {
                auto& b = p.blocks[(size_t) k];
                b.on = true;
                b.velocity = k % 4 == 0 ? 1.0f : k % 2 == 0 ? 0.8f : 0.6f;
                b.transpose = k % 8 == 6 ? 12 : 0;
            }
        }
        route (grid, "lfo1", ParamIDs::fltCutoff, 0.15f);
        route (grid, "macro1", ParamIDs::fltCutoff, 0.3f);
        route (grid, "modWheel", ParamIDs::fltRes, 0.35f);
        route (grid, "macro3", ParamIDs::osc (1, "Volume"), 0.2f);
        macros (grid, "OPEN", "SPACE", "OCTAVE");
        delay (grid, "1/8 dotted", 0.45f, 0.3f, true, 5.0f);
        reverb (grid, RT::hall, 0.5f, 0.18f);
        space (grid);
        grid.set (ParamIDs::patchLevel, -1.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (grid);

        auto drops = x.make ("Raindrop Machine", 14, { "Arp", "Bell", "FM", "Randomizer" },
            "Sparse, glassy plucks falling like drops: ARP 1's randomizer picks notes from the minor scale over two "
            "octaves, with plenty of rests, and rolls a new pattern every pass (AUTODICE), so it never quite repeats. "
            "Each drop is a sine with FM from a sine an octave up, its own echo in eighths, and a cathedral. Hold a "
            "chord or one key. MACROS: BRIGHT (the FM), SPACE (more cathedral and echoes), SPARKLE (a sine two "
            "octaves up). MOD WHEEL: detune.");
        drops.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
             .osc (0, "FmAmount", 0.2f).osc (0, "ModGrid", (float) synth::ModGrid::harmonic).osc (0, "ModRate", 0.6f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.08f).osc (1, "Arp", 1.0f)
             .ampEnvelope (1.0f, 600.0f, 0.0f, 600.0f).set (ParamIDs::ampDecayCurve, -0.4f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.3f).set (ParamIDs::arp (0, "Humanize"), 0.15f);
        {
            auto& r = drops.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Minor");
            r.restChance = 0.5f;
            r.lengthChances = { 1.0f, 0.2f, 0.0f, 0.0f };
            r.velocityLow = 0.45f;
            r.velocityHigh = 1.0f;
            r.autoDice = true;
            r.seed = 7;
            auto& p = drops.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        route (drops, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (drops, "macro3", ParamIDs::osc (1, "Volume"), 0.15f);
        route (drops, "modWheel", ParamIDs::osc (0, "Fine"), 0.04f);
        macros (drops, "BRIGHT", "SPACE", "SPARKLE");
        delay (drops, "1/8", 0.5f, 0.3f, true, 6.0f);
        drops.set (ParamIDs::delayToReverb, 0.5f);
        reverb (drops, RT::cathedral, 0.25f, 0.3f, 40.0f, 5.0f);
        space (drops);
        drops.set (ParamIDs::patchLevel, -1.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (drops);

        auto skyline = x.make ("Skyline Sequence", 15, { "Arp", "Chords", "Step Seq", "Delay" },
            "Hold a key or a chord (its root is used: FIXED NOTE): ARP 1 plays a slow minor progression on it, a chord a bar (i9, VImaj7, iv7, "
            "v sus4) with a sub note under each, while the STEP SEQUENCER gates the chords into a sixteenth-note "
            "rhythm that bounces between left and right, in time with your song. Long dotted-quarter echoes and a "
            "hall. MACROS: BRIGHT, SPACE (more hall and echoes, wider), SMOOTH (less gating).");
        skyline.osc (0, "Volume", 0.45f).osc (0, "Arp", 1.0f).osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f)
               .osc (0, "UniDetune", 0.22f).osc (0, "UniSpread", 0.6f)
               .filter (FilterType::lp24, 1800.0f, 0.2f, 0.0f)
               .ampEnvelope (5.0f, 500.0f, 0.9f, 400.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::chords)
               .set (ParamIDs::arp (0, "Division"), division ("1/4")).set (ParamIDs::arp (0, "Gate"), 1.0f)
               .set (ParamIDs::arp (0, "NoteSource"), (float) synth::arp::NoteSource::fixedNote)
               .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqMode, (float) synth::seq::Mode::stereo).set (ParamIDs::seqWidth, 0.5f)
               .set (ParamIDs::seqGate, 0.7f).set (ParamIDs::seqContour, 0.1f).set (ParamIDs::seqVolMix, 0.85f)
               .set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        {
            auto& p = skyline.rig.modulation.arps[0].patterns[0];
            p = {};
            const char* chords[] = { "Min9", "Maj7", "Min7", "Sus4" };
            const int roots[] = { 0, -4, 5, 7 };
            for (int bar = 0; bar < 4; ++bar)
            {
                auto& b = p.blocks[(size_t) (bar * 4)];
                b.on = true;
                b.length = 4;
                b.chord = (int) chord (chords[bar]);
                b.transpose = roots[bar];
                b.sub = true;
                b.velocity = 0.9f;
            }
            const bool rhythm[16] = { 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1 };
            for (int row = 0; row < 2; ++row)
                for (int k = 0; k < synth::seq::maxSteps; ++k)
                {
                    auto& step = skyline.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                    const auto at = k % 16;
                    step.on = rhythm[at] && (at % 4 == 0 || (at % 2 == 0) == (row == 0) || at == 15);
                    step.level = at % 4 == 0 ? 1.0f : 0.7f;
                }
        }
        route (skyline, "stepSeq", ParamIDs::fltCutoff, 0.1f);
        route (skyline, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (skyline, "macro3", ParamIDs::seqVolMix, -0.7f);
        route (skyline, "macro2", ParamIDs::osc (0, "UniSpread"), 0.4f);   // wider, with the room
        macros (skyline, "BRIGHT", "SPACE", "SMOOTH");
        delay (skyline, "1/4 dotted", 0.48f, 0.28f, true, 4.5f);
        reverb (skyline, RT::hall, 0.6f, 0.25f);
        eqBand (skyline, 0, eq::Shape::lowCut, 90.0f, 0.0f);
        space (skyline);
        skyline.set (ParamIDs::patchLevel, -6.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (skyline);
    }

    // Insert bus `bus`'s DRIVE, and its sends (the bus sends at its own amounts, after the drive).
    void drive (Builder& b, int bus, effects::Drive::Model model, float gain, float tone, float levelDb, float toDelay = 0.3f, float toReverb = 0.3f)
    {
        b.set (ParamIDs::ins (bus, "DriveOn"), 1.0f).set (ParamIDs::ins (bus, "DriveModel"), (float) (int) model)
         .set (ParamIDs::ins (bus, "DriveGain"), gain).set (ParamIDs::ins (bus, "DriveTone"), tone).set (ParamIDs::ins (bus, "DriveLevel"), levelDb)
         .set (ParamIDs::ins (bus, "SendDelay"), toDelay).set (ParamIDs::ins (bus, "SendReverb"), toReverb);
    }

    void glitch (Builder& b, int bus, effects::Glitch::Mode mode, const char* length, float density, float chaos, float mix)
    {
        b.set (ParamIDs::ins (bus, "GlitchOn"), 1.0f).set (ParamIDs::ins (bus, "GlitchMode"), (float) (int) mode)
         .set (ParamIDs::ins (bus, "GlitchSync"), 1.0f).set (ParamIDs::ins (bus, "GlitchDivision"), (float) effects::Glitch::syncNames.indexOf (length))
         .set (ParamIDs::ins (bus, "GlitchDensity"), density).set (ParamIDs::ins (bus, "GlitchChaos"), chaos).set (ParamIDs::ins (bus, "GlitchMix"), mix);
    }

    void compressor (Builder& b, int character, float thresholdDb, float ratio, float makeUpDb)
    {
        b.set (ParamIDs::compOn, 1.0f).set (ParamIDs::compCharacter, (float) character).set (ParamIDs::compThreshold, thresholdDb)
         .set (ParamIDs::compRatio, ratio).set (ParamIDs::compMakeUp, makeUpDb);
    }

    // HELLFORGE: inspired by modern industrial-metal game scores (synths played like down-tuned guitars through brutal
    // distortion, sub drops, noise walls, a dark choir, syncopated chugs). No names or music from any game: our own
    // sounds. MACRO 3 is DESTROY on most: more drive.
    void hellforge (Expansion x)
    {
        using DM = effects::Drive::Model;
        using GM = effects::Glitch::Mode;
        constexpr int vca = 0, fet = 2;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto riff = x.make ("Molten Riff", 1, { "Lead", "Drive", "Mono", "Inserts" },
            "A synth played like a lead guitar: five detuned saws over a pulse an octave down, through insert bus A's "
            "RABID turned well up, with a short glide and the pitch wheel set to dive a whole octave (bend down 12) for "
            "whammy dives. Dotted-eighth echoes and a room, taken after the drive. MACROS: BRIGHT, SPACE, DESTROY "
            "(SHAPER 1's tube into the drive).");
        riff.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.18f).osc (0, "UniSpread", 0.6f)
            .osc (0, "Volume", 0.6f).osc (0, "Insert", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
            .osc (1, "Insert", 1.0f)
            .shaper (0, synth::ShaperModel::tube, 0.15f)
            .filter (FilterType::lp24, 5000.0f, 0.1f, 0.2f).filterEnvelope (0.0f, 400.0f, 0.6f, 200.0f)
            .ampEnvelope (2.0f, 400.0f, 0.9f, 200.0f)
            .mono (30.0f).set (ParamIDs::bendDown, 12.0f).set (ParamIDs::bendUp, 2.0f);
        drive (riff, 0, DM::rabid, 7.0f, 6.0f, 0.0f, 0.4f, 0.3f);
        route (riff, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (riff, "macro3", ParamIDs::shp (0, "Drive"), 0.6f);
        macros (riff, "BRIGHT", "SPACE", "DESTROY");
        delay (riff, "1/8 dotted", 0.3f, 0.15f, true, 4.0f);
        reverb (riff, RT::room, 0.5f, 0.15f);
        space (riff);
        riff.set (ParamIDs::patchLevel, 1.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (riff);

        auto siren = x.make ("Warning Siren", 2, { "Lead", "Sync", "Pitch", "Mono" },
            "A screaming lead: a saw hard-synced, its sync sweeping down as each note starts, while the PITCH envelope "
            "pulls every note up from an octave below like an air-raid siren winding up. SHAPER 1 folds it, vibrato comes "
            "in late. The MOD WHEEL pushes the sync further. MACROS: BRIGHT, SPACE, SCREAM (more sync).");
        siren.osc (0, "SyncAmount", 0.25f).osc (0, "SyncEnv", 0.4f).osc (0, "SyncEnvTime", 400.0f).osc (0, "Volume", 0.6f)
             .osc (0, "VibAmount", 20.0f).osc (0, "VibRate", 6.0f).osc (0, "VibFade", 300.0f)
             .shaper (0, synth::ShaperModel::sineFold, 0.25f)
             .filter (FilterType::lp24, 6000.0f, 0.2f, 0.0f)
             .ampEnvelope (5.0f, 400.0f, 0.9f, 300.0f)
             .mono (60.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        siren.rig.modulation.pitchEnvelope = { { SP { 0.0f, 0.0f, 0.5f }, SP { 0.35f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (siren, "modWheel", ParamIDs::osc (0, "SyncAmount"), 0.35f);
        route (siren, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (siren, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.4f);
        macros (siren, "BRIGHT", "SPACE", "SCREAM");
        delay (siren, "1/8", 0.3f, 0.15f, true, 5.0f);
        reverb (siren, RT::hall, 0.6f, 0.2f);
        space (siren);
        siren.set (ParamIDs::patchLevel, -8.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (siren);

        auto scream = x.make ("Signal Scream", 3, { "Lead", "FM", "Glitch", "Inserts" },
            "A howling, broken-signal lead: a sine frequency-modulated by a saw a fifth up, FORMANT and a fast LFO making "
            "it howl, through insert bus A's CHIMERA and GLITCH chopping it into sixteenth-note stutters. The MOD WHEEL "
            "opens the howl. MACROS: BRIGHT, SPACE, DESTROY (more FM).");
        scream.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "Formant", 0.25f).osc (0, "Insert", 1.0f)
              .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.3f)
              .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", 7.0f).osc (1, "Volume", 1.0f)
              .filter (FilterType::lp24, 7000.0f, 0.1f, 0.0f)
              .ampEnvelope (3.0f, 300.0f, 0.9f, 250.0f)
              .mono (40.0f)
              .set (ParamIDs::lfo (0, "Rate"), 5.5f);
        drive (scream, 0, DM::chimera, 5.0f, 6.0f, 0.0f, 0.4f, 0.4f);
        glitch (scream, 0, GM::sequence, "1/16", 4.0f, 3.0f, 0.35f);
        route (scream, "lfo1", ParamIDs::osc (0, "Formant"), 0.06f);
        route (scream, "modWheel", ParamIDs::osc (0, "Formant"), 0.3f);
        route (scream, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (scream, "macro3", ParamIDs::osc (0, "FmAmount"), 0.35f);
        macros (scream, "BRIGHT", "SPACE", "DESTROY");
        delay (scream, "1/8", 0.35f, 0.15f, true, 5.0f);
        reverb (scream, RT::plate, 0.5f, 0.15f);
        space (scream);
        scream.set (ParamIDs::patchLevel, 1.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (scream);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto choir = x.make ("Ash Choir", 4, { "Pad", "Choir", "Wavetable", "Drive" },
            "A dark, distorted choir: the Vowels table singing \"ah\" in a wide unison, an \"oh\" an octave below, "
            "both through SHAPER 1's tube so the voices grind, a slow LFO moving the vowel, a dimension doubler and a "
            "cathedral. MACROS: BRIGHT, SPACE, DESTROY (the tube).");
        choir.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.0f)
             .osc (0, "UniVoices", 6.0f).osc (0, "UniMix", 0.75f).osc (0, "UniDetune", 0.25f).osc (0, "UniSpread", 1.0f)
             .osc (0, "Volume", 0.5f)
             .osc (1, "On", 1.0f).osc (1, "Wave", wavetable).osc (1, "Table", table ("Vowels")).osc (1, "WtIndex", 0.75f)
             .osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.6f)
             .osc (1, "UniDetune", 0.2f).osc (1, "UniSpread", 0.8f)
             .shaper (0, synth::ShaperModel::tube, 0.35f)
             .filter (FilterType::lp12, 1800.0f, 0.1f, 0.0f)
             .ampEnvelope (900.0f, 2000.0f, 0.9f, 2200.0f)
             .set (ParamIDs::lfo (0, "Rate"), 0.1f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (choir, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.05f);
        route (choir, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (choir, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (choir, "BRIGHT", "SPACE", "DESTROY");
        chorus (choir, ParamIDs::modIds, MT::dimension, 0.3f, 0.5f, 0.4f);
        reverb (choir, RT::cathedral, 0.4f, 0.3f, 30.0f, 4.0f);
        eqBand (choir, 0, eq::Shape::lowCut, 80.0f, 0.0f);
        space (choir);
        choir.set (ParamIDs::patchLevel, -4.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (choir);

        auto furnace = x.make ("Furnace Drone", 5, { "Pad", "Noise", "Drive", "Inserts" },
            "A wall of noise and heat: seven saws detuned far apart, with a stream of noise, all through insert bus A's "
            "ABYSS with its lows up, the filter breathing very slowly (LFO 2). Hold one low note or a fifth. MACROS: "
            "BRIGHT, SPACE, HEAT (more noise).");
        furnace.osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.85f).osc (0, "UniDetune", 0.5f).osc (0, "UniSpread", 1.0f)
               .osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
               .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.3f).osc (1, "NoiseColour", 0.5f)
               .osc (1, "Volume", 1.0f).osc (1, "Insert", 1.0f)
               .filter (FilterType::lp24, 1200.0f, 0.25f, 0.0f)
               .ampEnvelope (1800.0f, 2000.0f, 1.0f, 3000.0f)
               .set (ParamIDs::lfo (1, "Rate"), 0.08f).set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        drive (furnace, 0, DM::abyss, 6.0f, 4.0f, 0.0f, 0.0f, 0.4f);
        furnace.set (ParamIDs::ins (0, "DriveLow"), 7.0f).set (ParamIDs::ins (0, "DriveHigh"), 4.0f);
        route (furnace, "lfo2", ParamIDs::fltCutoff, 0.2f);
        route (furnace, "macro1", ParamIDs::fltCutoff, 0.3f);
        route (furnace, "macro3", ParamIDs::osc (1, "Noise"), 0.3f);
        macros (furnace, "BRIGHT", "SPACE", "HEAT");
        reverb (furnace, RT::hall, 0.6f, 0.25f);
        space (furnace);
        furnace.set (ParamIDs::patchLevel, 3.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (furnace);

        auto corrupt = x.make ("Corrupted Signal", 6, { "Pad", "Wavetable", "Travel", "Glitch", "Lo-Fi" },
            "A broken transmission: the Bit Steps table stepped through by TRAVEL in sixteenths (jumps, reverses, "
            "retriggers and dropouts), bit-crushed, through insert bus A's GLITCH (INTERFERE) and LO-FI's bitcrusher on "
            "the whole sound. In time with your song. MACROS: BRIGHT, SPACE, CORRUPT (more crush).");
        corrupt.osc (0, "Wave", wavetable).osc (0, "Table", table ("Bit Steps")).osc (0, "WtIndex", 0.2f)
               .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.8f)
               .osc (0, "Crush", 0.25f).osc (0, "CrushMode", 1.0f).osc (0, "Volume", 0.55f).osc (0, "Insert", 1.0f)
               .osc (0, "TrOn", 1.0f).osc (0, "TrEditors", (float) synth::travel::Editors::envSeq).osc (0, "TrSteps", 8.0f)
               .osc (0, "TrSync", (float) synth::mod::LfoSync::straight).osc (0, "TrDivision", division ("1/16"))
               .osc (0, "TrStrength", 0.7f).osc (0, "TrTrigger", (float) synth::travel::Trigger::firstNote)
               .filter (FilterType::lp24, 3000.0f, 0.2f, 0.0f)
               .ampEnvelope (300.0f, 1000.0f, 1.0f, 1500.0f)
               .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::bitcrusher)
               .set (ParamIDs::lofiAmount, 3.0f).set (ParamIDs::lofiMix, 0.4f).set (ParamIDs::lofiNoise, 0.0f);
        glitch (corrupt, 0, GM::interfere, "1/8", 5.0f, 5.0f, 0.4f);
        corrupt.set (ParamIDs::ins (0, "SendReverb"), 0.5f).set (ParamIDs::ins (0, "SendDelay"), 0.3f);
        {
            auto& t = corrupt.rig.modulation.travel[0];
            t.envelope = { { SP { 0.0f, 0.0f, 0.0f }, SP { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
            using M = synth::travel::StepMode;
            const synth::travel::Step steps[] = { { M::normal, 1.0f, 1 }, { M::trigger, 4.0f, 1 }, { M::reverse, 1.0f, 1 }, { M::silence, 1.0f, 1 },
                                                  { M::high, 1.0f, 1 }, { M::trigger, 2.0f, 1 }, { M::low, 1.0f, 1 }, { M::flip, 1.0f, 1 } };
            for (int k = 0; k < 8; ++k)
                t.pattern.steps[(size_t) k] = steps[k];
        }
        route (corrupt, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (corrupt, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        route (corrupt, "macro3", ParamIDs::lofiAmount, 0.3f);
        macros (corrupt, "BRIGHT", "SPACE", "CORRUPT");
        delay (corrupt, "1/16 dotted", 0.35f, 0.12f, true, 5.0f);
        reverb (corrupt, RT::plate, 0.6f, 0.2f);
        space (corrupt);
        corrupt.set (ParamIDs::patchLevel, -7.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (corrupt);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto chug = x.make ("Gut Chug", 7, { "Bass", "Drive", "Mono", "Inserts" },
            "A down-tuned palm-muted chug: a saw and a pulse an octave up (the pick's edge) through insert bus A's RABID, "
            "plucked short by the filter, every note struck afresh, and a clean sine an octave down under it, straight to "
            "the mix. CONSTRICTOR (FET) slams it, ECHELON scoops the mids. Play it in sixteenths. MACROS: BRIGHT, SPACE, "
            "DESTROY.");
        chug.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.1f).osc (0, "Volume", 0.6f)
            .osc (0, "PhaseRand", 0.0f).osc (0, "Insert", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.35f)
            .osc (1, "Insert", 1.0f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.5f).osc (2, "PhaseRand", 0.0f)
            .osc (2, "Filter", 0.0f).osc (2, "SendDelay", 0.0f).osc (2, "SendReverb", 0.0f)
            .shaper (0, synth::ShaperModel::tube, 0.1f)
            .filter (FilterType::lp24, 900.0f, 0.15f, 0.5f).filterEnvelope (0.0f, 80.0f, 0.1f, 40.0f)
            .set (ParamIDs::fltVel, 0.4f)
            .ampEnvelope (1.0f, 70.0f, 0.0f, 35.0f)   // each hit dies inside its sixteenth, like a palm mute
            .mono (0.0f).set (ParamIDs::legato, 0.0f);
        drive (chug, 0, DM::rabid, 4.0f, 5.0f, -5.0f, 0.0f, 0.2f);
        compressor (chug, fet, -18.0f, 4.0f, 3.0f);
        chug.set (ParamIDs::compAttack, 1.0f);   // catches the first hit too
        eqBand (chug, 0, eq::Shape::lowCut, 30.0f, 0.0f);
        eqBand (chug, 1, eq::Shape::bell, 450.0f, -3.0f, 1.0f);
        eqBand (chug, 2, eq::Shape::highShelf, 3000.0f, 2.0f);
        route (chug, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (chug, "macro3", ParamIDs::shp (0, "Drive"), 0.6f);
        macros (chug, "BRIGHT", "SPACE", "DESTROY");
        reverb (chug, RT::room, 0.3f, 0.1f);
        space (chug);
        chug.set (ParamIDs::patchLevel, 4.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chug);

        auto drop = x.make ("Tectonic Drop", 8, { "Bass", "Hit", "Pitch", "Sub" },
            "A sub drop for the moment before everything hits: a sine falling two octaves into its note over about a "
            "second (the PITCH envelope), a saw an octave up giving it a growl through SHAPER 1's tube, ringing three "
            "seconds into a hall. MACROS: BRIGHT, SPACE, DESTROY.");
        drop.osc (0, "Wave", sine).osc (0, "Volume", 0.9f).osc (0, "PhaseRand", 0.0f).osc (0, "Filter", 0.0f)
            .osc (0, "SendReverb", 0.1f).osc (0, "SendDelay", 0.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.2f)
            .shaper (0, synth::ShaperModel::tube, 0.4f)
            .filter (FilterType::lp24, 800.0f, 0.15f, 0.3f).filterEnvelope (0.0f, 1500.0f, 0.0f, 1000.0f)
            .ampEnvelope (2.0f, 3000.0f, 0.0f, 1500.0f).set (ParamIDs::ampDecayCurve, -0.3f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 24.0f);
        drop.rig.modulation.pitchEnvelope = { { SP { 0.0f, 1.0f, -0.3f }, SP { 1.2f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (drop, "macro1", ParamIDs::fltCutoff, 0.3f);
        route (drop, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (drop, "BRIGHT", "SPACE", "DESTROY");
        reverb (drop, RT::hall, 0.8f, 0.25f, 10.0f, 3.0f);
        space (drop);
        drop.set (ParamIDs::patchLevel, -4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (drop);

        auto grinder = x.make ("Grinder Bass", 9, { "Bass", "Wavetable", "Modulation", "Shaper" },
            "A grinding, chewing bass: the Wavefold table with its sub, LFO 1 (sixteenths, in time with your song) "
            "sweeping the fold and the filter, SHAPER 1's diode biting into it, CONSTRICTOR (FET) holding it. The MOD "
            "WHEEL folds it further. MACROS: BRIGHT, SPACE, DESTROY.");
        grinder.osc (0, "Wave", wavetable).osc (0, "Table", table ("Wavefold")).osc (0, "WtIndex", 0.3f)
               .osc (0, "Sub", 0.5f).osc (0, "PhaseRand", 0.0f).osc (0, "Volume", 0.7f)
               .shaper (0, synth::ShaperModel::diode, 0.4f)
               .filter (FilterType::lp24, 1500.0f, 0.3f, 0.0f)
               .ampEnvelope (2.0f, 300.0f, 1.0f, 120.0f)
               .mono (20.0f)
               .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle)
               .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
               .set (ParamIDs::lfo (0, "Division"), division ("1/16"))
               .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
               .set (ParamIDs::lfo (0, "Offset"), 1.0f);
        compressor (grinder, fet, -18.0f, 4.0f, 2.0f);
        route (grinder, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.35f);
        route (grinder, "lfo1", ParamIDs::fltCutoff, 0.15f);
        route (grinder, "modWheel", ParamIDs::osc (0, "WtIndex"), 0.3f);
        route (grinder, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (grinder, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (grinder, "BRIGHT", "SPACE", "DESTROY");
        reverb (grinder, RT::room, 0.3f, 0.08f);
        space (grinder);
        grinder.set (ParamIDs::patchLevel, 3.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (grinder);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto organ = x.make ("Rust Organ", 10, { "Keys", "Organ", "Drive", "Inserts" },
            "A heavy, overdriven organ: the Organ Stack table through insert bus A's BANSHEE and a rotary speaker, so "
            "chords growl and swirl. MACROS: BRIGHT, SPACE, DESTROY (SHAPER 1's tube into the drive).");
        organ.osc (0, "Wave", wavetable).osc (0, "Table", table ("Organ Stack")).osc (0, "WtIndex", 0.6f)
             .osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.06f).osc (0, "Volume", 0.6f)
             .osc (0, "Insert", 1.0f)
             .shaper (0, synth::ShaperModel::tube, 0.1f)
             .filter (FilterType::lp12, 4000.0f, 0.05f, 0.0f)
             .ampEnvelope (5.0f, 200.0f, 1.0f, 150.0f);
        drive (organ, 0, DM::screamer, 7.0f, 5.0f, 3.0f, 0.0f, 0.3f);
        chorus (organ, ParamIDs::insModIds (0), MT::rotary, 6.0f, 0.6f, 1.0f);
        route (organ, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (organ, "macro3", ParamIDs::shp (0, "Drive"), 0.6f);
        macros (organ, "BRIGHT", "SPACE", "DESTROY");
        reverb (organ, RT::room, 0.5f, 0.15f);
        space (organ);
        organ.set (ParamIDs::patchLevel, 0.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (organ);

        auto bells = x.make ("Ritual Bells", 11, { "Keys", "FM", "Bell", "Delay" },
            "Dark, clanging bells: a sine modulated by oscillator 2 a minor ninth up and pulled sharp (the harsh, "
            "inharmonic ring), the strike fading slower than the note, a low sine for the bell's body, SHAPER 1's tube "
            "after the filter for grime, ping-pong echoes and a cathedral. MACROS: METAL (the FM), SPACE, DESTROY.");
        bells.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.15f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 13.0f).osc (1, "Fine", 30.0f).osc (1, "Volume", 1.0f)
             .osc (1, "PhaseRand", 0.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.3f)
             .shaper (0, synth::ShaperModel::tube, 0.25f, true)
             .ampEnvelope (1.0f, 5000.0f, 0.0f, 3000.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f)
             .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        bells.rig.modulation.envelopes[0] = { { SP { 0.0f, 1.0f, -0.6f }, SP { 3.0f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (bells, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (bells, "velocity", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (bells, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (bells, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (bells, "METAL", "SPACE", "DESTROY");
        delay (bells, "1/4", 0.4f, 0.2f, true, 4.0f);
        reverb (bells, RT::cathedral, 0.35f, 0.3f, 30.0f, 4.0f);
        space (bells);
        bells.set (ParamIDs::patchLevel, -7.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (bells);

        auto riff2 = x.make ("Iron Riff", 12, { "Keys", "Chords", "Drive", "Inserts" },
            "Gatling Pulse played by hand: every key gives a whole power chord (the oscillator's chorder adds the fifth "
            "and the octave) with a clean sine sub an octave under it, through insert bus A's RABID, so one finger plays "
            "a down-tuned rhythm-guitar chord. Short notes snap like a palm mute; held ones ring. CONSTRICTOR (FET) "
            "slams it, ECHELON scoops the mids. MACROS: BRIGHT, SPACE, DESTROY.");
        riff2.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.5f)
             .osc (0, "Volume", 0.5f).osc (0, "PhaseRand", 0.0f).osc (0, "Insert", 1.0f)
             .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 7.0f).osc (0, "Ch2On", 1.0f).osc (0, "Ch2Coarse", 12.0f)
             .osc (0, "Ch2Vol", 0.7f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.5f)
             .osc (1, "PhaseRand", 0.0f).osc (1, "Filter", 0.0f).osc (1, "SendDelay", 0.0f).osc (1, "SendReverb", 0.0f)
             .shaper (0, synth::ShaperModel::tube, 0.1f)
             .filter (FilterType::lp24, 1500.0f, 0.15f, 0.4f).filterEnvelope (0.0f, 150.0f, 0.35f, 120.0f)
             .set (ParamIDs::fltVel, 0.4f)
             .ampEnvelope (1.0f, 300.0f, 0.6f, 150.0f);
        drive (riff2, 0, DM::rabid, 6.0f, 5.0f, 0.0f, 0.1f, 0.2f);
        compressor (riff2, fet, -18.0f, 4.0f, 2.0f);
        riff2.set (ParamIDs::compAttack, 1.0f);
        eqBand (riff2, 0, eq::Shape::lowCut, 30.0f, 0.0f);
        eqBand (riff2, 1, eq::Shape::bell, 450.0f, -2.5f, 1.0f);
        route (riff2, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (riff2, "macro3", ParamIDs::shp (0, "Drive"), 0.6f);
        macros (riff2, "BRIGHT", "SPACE", "DESTROY");
        delay (riff2, "1/8 dotted", 0.2f, 0.08f, true, 4.0f);
        reverb (riff2, RT::room, 0.3f, 0.1f);
        space (riff2);
        riff2.set (ParamIDs::patchLevel, 5.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (riff2);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto gatling = x.make ("Gatling Pulse", 13, { "Arp", "Chords", "Drive", "Chug" },
            "Hold one low key (or a chord: its root is used): ARP 1 fires power chords with a sub under them in a "
            "syncopated sixteenth-note chug, with a dissonant half-step stab near the end of the bar, through insert bus "
            "A's RABID, in time with your song. CONSTRICTOR (FET) slams it. MACROS: BRIGHT, SPACE, DESTROY.");
        gatling.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.5f)
               .osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
               .shaper (0, synth::ShaperModel::tube, 0.1f)
               .filter (FilterType::lp24, 1500.0f, 0.15f, 0.4f).filterEnvelope (0.0f, 150.0f, 0.2f, 80.0f)
               .ampEnvelope (1.0f, 200.0f, 0.5f, 80.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::chords)
               .set (ParamIDs::arp (0, "NoteSource"), (float) synth::arp::NoteSource::fixedNote)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Gate"), 0.5f);
        {
            auto& p = gatling.rig.modulation.arps[0].patterns[0];
            p = {};
            const bool hit[16] = { 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1 };
            for (int k = 0; k < 16; ++k)
            {
                auto& b = p.blocks[(size_t) k];
                b.on = hit[k];
                b.chord = (int) chord ("Power");
                b.sub = k % 4 == 0;
                b.transpose = k == 14 ? 1 : k == 15 ? 3 : 0;
                b.velocity = k % 4 == 0 ? 1.0f : 0.8f;
            }
        }
        drive (gatling, 0, DM::rabid, 6.0f, 5.0f, 0.0f, 0.1f, 0.2f);
        compressor (gatling, fet, -18.0f, 4.0f, 2.0f);
        route (gatling, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (gatling, "macro3", ParamIDs::shp (0, "Drive"), 0.6f);
        macros (gatling, "BRIGHT", "SPACE", "DESTROY");
        delay (gatling, "1/8 dotted", 0.2f, 0.08f, true, 4.0f);
        reverb (gatling, RT::room, 0.3f, 0.1f);
        eqBand (gatling, 0, eq::Shape::bell, 450.0f, -2.5f, 1.0f);
        space (gatling);
        gatling.set (ParamIDs::patchLevel, 6.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (gatling);

        auto shrapnel = x.make ("Shrapnel Grid", 14, { "Arp", "Randomizer", "Glitch", "Wavetable" },
            "Sharp metal fragments: ARP 1's randomizer picks notes from the Phrygian scale over three octaves, with "
            "rests and ratchet bursts, and rolls a new pattern every pass. Each note a short bit-crushed pluck from the "
            "Bit Steps table, through insert bus A's GLITCH and ping-pong echoes. Hold a key or a chord. MACROS: BRIGHT, "
            "SPACE, CORRUPT (more crush).");
        shrapnel.osc (0, "Wave", wavetable).osc (0, "Table", table ("Bit Steps")).osc (0, "WtIndex", 0.5f)
                .osc (0, "Crush", 0.3f).osc (0, "CrushMode", 2.0f).osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f)
                .osc (0, "Insert", 1.0f)
                .filter (FilterType::lp24, 4000.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 120.0f, 0.0f, 100.0f)
                .ampEnvelope (0.5f, 150.0f, 0.0f, 100.0f)
                .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
                .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 3.0f)
                .set (ParamIDs::arp (0, "Gate"), 0.4f);
        {
            auto& r = shrapnel.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Phrygian");
            r.restChance = 0.3f;
            r.ratchetChance = 0.3f;
            r.lengthChances = { 1.0f, 0.1f, 0.0f, 0.0f };
            r.velocityLow = 0.6f;
            r.autoDice = true;
            r.seed = 13;
            auto& p = shrapnel.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        glitch (shrapnel, 0, GM::sequence, "1/32", 5.0f, 4.0f, 0.3f);
        shrapnel.set (ParamIDs::ins (0, "SendDelay"), 0.5f).set (ParamIDs::ins (0, "SendReverb"), 0.3f);
        route (shrapnel, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (shrapnel, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        macros (shrapnel, "BRIGHT", "SPACE", "CORRUPT");
        delay (shrapnel, "1/16 dotted", 0.35f, 0.2f, true, 6.0f);
        reverb (shrapnel, RT::plate, 0.5f, 0.15f);
        space (shrapnel);
        shrapnel.set (ParamIDs::patchLevel, 4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (shrapnel);

        auto machine = x.make ("War Machine", 15, { "Arp", "Bass", "Step Seq", "Drive" },
            "A pumping war-march sequence: hold a key and ARP 1 drives a saw an octave down with a square on top through "
            "a sixteenth-note pattern of roots, octaves and fifths, while the STEP SEQUENCER ducks the volume on every "
            "beat like a sidechained mix, all through insert bus A's CHIMERA, in time with your song. MACROS: BRIGHT, "
            "SPACE, DESTROY.");
        machine.osc (0, "Transpose", -12.0f).osc (0, "Volume", 0.55f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
               .osc (0, "Insert", 1.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
               .osc (1, "Insert", 1.0f)
               .shaper (0, synth::ShaperModel::tube, 0.1f)
               .filter (FilterType::lp24, 1800.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 160.0f, 0.2f, 100.0f)
               .ampEnvelope (1.0f, 200.0f, 0.6f, 90.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Gate"), 0.6f)
               .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.6f).set (ParamIDs::seqGate, 1.0f)
               .set (ParamIDs::seqContour, 0.3f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        {
            auto& p = machine.rig.modulation.arps[0].patterns[0];
            p = {};
            const int on[] = { 0, 2, 3, 6, 8, 10, 11, 14 };
            const int transpose[] = { 0, 0, 12, 0, 0, 0, 12, 7 };
            for (int i = 0; i < 8; ++i)
            {
                auto& b = p.blocks[(size_t) on[i]];
                b.on = true;
                b.transpose = transpose[i];
                b.velocity = on[i] % 4 == 0 ? 1.0f : 0.8f;
            }
            const float pump[4] = { 0.3f, 0.65f, 0.9f, 1.0f };   // ducked on the beat, back up by the next
            for (int row = 0; row < 2; ++row)
                for (int k = 0; k < synth::seq::maxSteps; ++k)
                {
                    auto& step = machine.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                    step.on = true;
                    step.level = pump[k % 4];
                    step.tied = true;
                }
        }
        drive (machine, 0, DM::chimera, 5.0f, 5.0f, 0.0f, 0.2f, 0.2f);
        route (machine, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (machine, "macro3", ParamIDs::shp (0, "Drive"), 0.6f);
        macros (machine, "BRIGHT", "SPACE", "DESTROY");
        delay (machine, "1/8 dotted", 0.3f, 0.1f, true, 4.0f);
        reverb (machine, RT::room, 0.4f, 0.12f);
        space (machine);
        machine.set (ParamIDs::patchLevel, 5.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (machine);
    }
}

std::vector<Rig> makeExpansionRigs()
{
    std::vector<Rig> rigs;
    neonRain ({ Rig::numBanks + 0, rigs });
    hellforge ({ Rig::numBanks + 1, rigs });
    return rigs;
}
