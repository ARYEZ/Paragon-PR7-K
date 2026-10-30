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
            "A wordless choir: the Vowels table in unison, opening quickly from \"oo\" to \"oh\" as each chord "
            "starts (TRAVEL, over a quarter note of your song), with a slow LFO keeping the vowel breathing, a darker saw an octave down under it, "
            "a wide ensemble and a long cathedral. MACROS: BRIGHT, SPACE (more cathedral), VOWEL (towards \"ee\"). MOD "
            "WHEEL: drift (the unison detune).");
        choir.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 1.0f)
             .osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.2f).osc (0, "UniSpread", 0.9f)
             .osc (0, "Volume", 0.55f)
             .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::oneShot).osc (0, "TrRate", 0.25f)   // RATE unused: TRAVEL follows the song (a quarter note), as the owner approved by ear
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
            "A dark, slowly turning drone: the Comb Sweep table rocking back and forth on every beat (TRAVEL, ping "
            "pong, in time with your song) with a sub under it, and rain on top: oscillator 2 plays only filtered noise, through "
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

    // STARCOURT (the owner's name): inspired by the dark '80s analog-synth score of a supernatural mystery series:
    // rising arpeggios over minor chords, detuned saws and squares, chorused polysynth pads, simple haunting leads,
    // cassette and VHS wobble, big '80s drums. No titles, characters or music from the series: our own sounds.
    void starcourt (Expansion x)
    {
        using SP2 = synth::mod::ShapePoint;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto hero = x.make ("Arcade Hero", 1, { "Lead", "Pulse", "PWM", "Mono" },
            "A bright square lead from an '80s mono synth: LFO 1 slowly sweeping the pulse width (PWM) so it shimmers, "
            "a quiet square an octave down, a short glide, vibrato coming in late. Dotted-eighth echoes and a hall. "
            "MACROS: BRIGHT, SPACE, HOLLOW (a thinner pulse).");
        hero.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.65f)
            .osc (0, "VibAmount", 15.0f).osc (0, "VibRate", 5.5f).osc (0, "VibFade", 400.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.25f)
            .filter (FilterType::lp24, 3500.0f, 0.2f, 0.2f).filterEnvelope (5.0f, 500.0f, 0.6f, 300.0f)
            .ampEnvelope (5.0f, 300.0f, 0.9f, 250.0f)
            .mono (40.0f)
            .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle).set (ParamIDs::lfo (0, "Rate"), 0.4f)
            .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (hero, "lfo1", ParamIDs::osc (0, "Width"), 0.3f);
        route (hero, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (hero, "macro3", ParamIDs::osc (0, "Width"), -0.35f);
        macros (hero, "BRIGHT", "SPACE", "HOLLOW");
        chorus (hero, ParamIDs::modIds, MT::chorus, 0.5f, 0.3f, 0.3f);
        delay (hero, "1/8 dotted", 0.35f, 0.2f, true, 5.0f);
        reverb (hero, RT::hall, 0.5f, 0.2f);
        space (hero);
        hero.set (ParamIDs::patchLevel, -6.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hero);

        auto wail = x.make ("Static Wail", 2, { "Lead", "Glide", "Tape", "Mono" },
            "A lonely, gliding lead: a saw with a sine an octave up, sliding slowly between notes on a curved glide, "
            "into a tape echo and a worn cassette that wobbles it. MACROS: BRIGHT, SPACE, WOBBLE (more cassette).");
        wail.osc (0, "Volume", 0.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.35f)
            .osc (0, "VibAmount", 10.0f).osc (0, "VibRate", 4.5f).osc (0, "VibFade", 600.0f)
            .filter (FilterType::lp12, 2500.0f, 0.15f, 0.1f)
            .ampEnvelope (20.0f, 400.0f, 0.85f, 500.0f)
            .mono (150.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.3f)
            .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
            .set (ParamIDs::lofiAmount, 2.0f).set (ParamIDs::lofiMix, 0.4f).set (ParamIDs::lofiNoise, 0.0f);
        route (wail, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (wail, "macro3", ParamIDs::lofiAmount, 0.4f);
        route (wail, "macro3", ParamIDs::lofiMix, 0.4f);
        macros (wail, "BRIGHT", "SPACE", "WOBBLE");
        delay (wail, "1/4", 0.45f, 0.25f, false, 4.0f);
        wail.set (ParamIDs::delayType, (float) (int) effects::Delay::Type::tape);
        reverb (wail, RT::hall, 0.6f, 0.2f);
        space (wail);
        wail.set (ParamIDs::patchLevel, 2.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (wail);

        auto phantom = x.make ("Phantom Sync", 3, { "Lead", "Sync", "Modulation", "Mono" },
            "A thin, eerie lead: a hard-synced saw whose sync drifts slowly up and down (LFO 2, the whole synth), with "
            "a fast, shallow vibrato, in a cathedral. MACROS: BRIGHT, SPACE, HAUNT (more sync).");
        phantom.osc (0, "SyncAmount", 0.15f).osc (0, "Volume", 0.6f).osc (0, "VibAmount", 8.0f).osc (0, "VibRate", 7.0f)
               .filter (FilterType::lp24, 5000.0f, 0.1f, 0.0f)
               .ampEnvelope (10.0f, 400.0f, 0.9f, 400.0f)
               .mono (60.0f)
               .set (ParamIDs::lfo (1, "Rate"), 0.2f).set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (phantom, "lfo2", ParamIDs::osc (0, "SyncAmount"), 0.15f);
        route (phantom, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (phantom, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.4f);
        macros (phantom, "BRIGHT", "SPACE", "HAUNT");
        delay (phantom, "1/4 dotted", 0.35f, 0.15f, true, 5.0f);
        reverb (phantom, RT::cathedral, 0.3f, 0.3f, 40.0f, 4.5f);
        space (phantom);
        phantom.set (ParamIDs::patchLevel, -0.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (phantom);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto glow = x.make ("Basement Glow", 4, { "Pad", "Chorus", "Unison", "Polysynth" },
            "A warm '80s polysynth pad: detuned saws over a square an octave down, the filter easing open as the chord "
            "swells in, through a lush, wide chorus. MACROS: BRIGHT, SPACE, CHORUS (deeper).");
        glow.osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.2f).osc (0, "UniSpread", 0.8f)
            .osc (0, "Volume", 0.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.25f)
            .filter (FilterType::lp12, 1800.0f, 0.1f, 0.15f).filterEnvelope (600.0f, 1500.0f, 0.7f, 1500.0f)
            .set (ParamIDs::fltKey, 0.4f)
            .ampEnvelope (500.0f, 1500.0f, 0.9f, 1500.0f);
        route (glow, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (glow, "macro3", ParamIDs::modDepth, 0.4f);
        macros (glow, "BRIGHT", "SPACE", "CHORUS");
        chorus (glow, ParamIDs::modIds, MT::chorus, 0.5f, 0.55f, 0.6f);
        glow.set (ParamIDs::modStereo, 1.0f);
        reverb (glow, RT::hall, 0.6f, 0.25f);
        space (glow);
        glow.set (ParamIDs::patchLevel, -10.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glow);

        auto other = x.make ("Other Side", 5, { "Pad", "Dark", "Envelope", "Reverb" },
            "A dark pad that won't stay in tune: wide detuned saws, and a second layer that slowly sinks flat over six "
            "seconds of each chord (ENV 1), so the chord curdles; a resonant low filter; a reverb that swells in "
            "backwards. MACROS: BRIGHT, SPACE, DRIFT (sinks further).");
        other.osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.8f).osc (0, "UniDetune", 0.3f).osc (0, "UniSpread", 1.0f)
             .osc (0, "Volume", 0.45f)
             .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Volume", 0.35f).osc (1, "UniVoices", 3.0f)
             .osc (1, "UniMix", 0.6f).osc (1, "UniDetune", 0.15f).osc (1, "UniSpread", 0.7f)
             .filter (FilterType::lp24, 1200.0f, 0.25f, 0.0f)
             .ampEnvelope (1500.0f, 2000.0f, 1.0f, 2500.0f)
             .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::sustain);
        other.rig.modulation.envelopes[0] = { { SP2 { 0.0f, 0.0f, 0.3f }, SP2 { 6.0f, 1.0f, 0.0f } }, 1, -1, -1 };
        route (other, "env1", ParamIDs::osc (1, "Fine"), -0.15f);
        route (other, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (other, "macro3", ParamIDs::osc (1, "Fine"), -0.15f);
        macros (other, "BRIGHT", "SPACE", "DRIFT");
        reverb (other, RT::reverse, 0.7f, 0.3f);
        space (other);
        other.set (ParamIDs::patchLevel, -10.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (other);

        auto cassette = x.make ("Cassette Choir", 6, { "Pad", "Choir", "Lo-Fi", "Wavetable" },
            "A choir on a worn cassette: the Vowels table singing \"oh\" in a soft unison, with the tape's slow pitch "
            "wobble (MOD's VIBRATO) and its dull, saturated sound (LO-FI's cassette). MACROS: BRIGHT, SPACE, WOBBLE.");
        cassette.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.78f)
                .osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.18f).osc (0, "UniSpread", 0.9f)
                .osc (0, "Volume", 0.55f)
                .filter (FilterType::lp12, 2600.0f, 0.1f, 0.0f)
                .ampEnvelope (700.0f, 1500.0f, 0.9f, 1800.0f)
                .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
                .set (ParamIDs::lofiAmount, 4.0f).set (ParamIDs::lofiMix, 0.6f).set (ParamIDs::lofiNoise, 0.0f);
        chorus (cassette, ParamIDs::modIds, MT::vibrato, 0.6f, 0.25f, 1.0f);
        route (cassette, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (cassette, "macro3", ParamIDs::modDepth, 0.4f);
        route (cassette, "macro3", ParamIDs::lofiAmount, 0.3f);
        macros (cassette, "BRIGHT", "SPACE", "WOBBLE");
        reverb (cassette, RT::hall, 0.6f, 0.25f);
        space (cassette);
        cassette.set (ParamIDs::patchLevel, 0.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (cassette);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto driveSub = x.make ("Night Drive Sub", 7, { "Bass", "Sub", "Mono", "Analog" },
            "A round analog-style bass: a saw darkened by a resonant low-pass with its sub, a little pluck at the start "
            "of each note, a short glide. CONSTRICTOR (OPTO) holds it steady. MACROS: BRIGHT, SPACE, GROWL (the filter's "
            "drive).");
        driveSub.osc (0, "Volume", 0.6f).osc (0, "Sub", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "SendReverb", 0.2f)
                .osc (0, "SendDelay", 0.0f)
                .filter (FilterType::lp24, 400.0f, 0.3f, 0.25f).filterEnvelope (0.0f, 300.0f, 0.3f, 150.0f)
                .ampEnvelope (2.0f, 400.0f, 0.8f, 150.0f)
                .mono (30.0f);
        compressor (driveSub, 1, -20.0f, 3.0f, 2.0f);
        route (driveSub, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (driveSub, "macro3", ParamIDs::fltDrive, 0.5f);
        macros (driveSub, "BRIGHT", "SPACE", "GROWL");
        reverb (driveSub, RT::room, 0.3f, 0.08f);
        space (driveSub);
        driveSub.set (ParamIDs::patchLevel, 5.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (driveSub);

        auto runner = x.make ("Pulse Runner", 8, { "Bass", "Pluck", "Sequence" },
            "A plucky bass for driving eighth-note lines: a saw and a square an octave down, the filter snapping shut "
            "after each note, harder with velocity. MACROS: BRIGHT, SPACE, SNAP (a deeper pluck).");
        runner.osc (0, "Volume", 0.55f).osc (0, "PhaseRand", 0.0f).osc (0, "SendDelay", 0.3f).osc (0, "SendReverb", 0.3f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f)
              .osc (1, "Volume", 0.3f).osc (1, "SendDelay", 0.0f).osc (1, "SendReverb", 0.0f)
              .filter (FilterType::lp24, 600.0f, 0.35f, 0.5f).filterEnvelope (0.0f, 180.0f, 0.1f, 100.0f)
              .set (ParamIDs::fltVel, 0.4f)
              .ampEnvelope (1.0f, 250.0f, 0.3f, 80.0f);
        route (runner, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (runner, "macro3", ParamIDs::fltEnv, 0.3f);
        macros (runner, "BRIGHT", "SPACE", "SNAP");
        delay (runner, "1/8 dotted", 0.2f, 0.1f, false, 4.0f);
        reverb (runner, RT::room, 0.3f, 0.1f);
        space (runner);
        runner.set (ParamIDs::patchLevel, 1.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (runner);

        auto octave = x.make ("Gated Octave", 9, { "Bass", "Step Seq", "Octave" },
            "Hold a note: a bass and its octave (the chorder's +12) chopped into sixteenths by the STEP SEQUENCER, the "
            "low note on the beats and the octave bouncing between them, in time with your song. MACROS: BRIGHT, SPACE, "
            "SMOOTH (less chopping).");
        octave.osc (0, "Volume", 0.55f).osc (0, "PhaseRand", 0.0f).osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 12.0f)
              .osc (0, "Ch1Vol", 0.6f).osc (0, "SendReverb", 0.3f).osc (0, "SendDelay", 0.0f)
              .filter (FilterType::lp24, 900.0f, 0.25f, 0.0f)
              .ampEnvelope (2.0f, 300.0f, 1.0f, 150.0f)
              .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.9f).set (ParamIDs::seqGate, 0.6f)
              .set (ParamIDs::seqContour, 0.1f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = octave.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = k % 4 != 3;
                step.level = k % 4 == 0 ? 1.0f : 0.7f;
            }
        route (octave, "stepSeq", ParamIDs::fltCutoff, 0.1f);
        route (octave, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (octave, "macro3", ParamIDs::seqVolMix, -0.7f);
        macros (octave, "BRIGHT", "SPACE", "SMOOTH");
        reverb (octave, RT::room, 0.4f, 0.1f);
        space (octave);
        octave.set (ParamIDs::patchLevel, 1.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (octave);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto vhs = x.make ("VHS Piano", 10, { "Keys", "FM", "Electric Piano", "Lo-Fi" },
            "An FM electric piano played back from a worn video tape: the strike bright and settling (ENV 1), brighter "
            "with velocity, the tape's slow pitch wobble (MOD's VIBRATO) and its dull warmth (LO-FI's cassette). MACROS: "
            "BARK (the FM), SPACE, WOBBLE.");
        vhs.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
           .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.08f)
           .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
           .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.1f)
           .ampEnvelope (2.0f, 2500.0f, 0.2f, 500.0f).set (ParamIDs::ampVel, 0.6f)
           .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::sustain)
           .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
           .set (ParamIDs::lofiAmount, 3.0f).set (ParamIDs::lofiMix, 0.5f).set (ParamIDs::lofiNoise, 0.0f);
        vhs.rig.modulation.envelopes[0] = { { SP2 { 0.0f, 1.0f, -0.5f }, SP2 { 0.9f, 0.15f, 0.0f } }, 1, -1, -1 };
        chorus (vhs, ParamIDs::modIds, MT::vibrato, 0.5f, 0.2f, 1.0f);
        route (vhs, "env1", ParamIDs::osc (0, "FmAmount"), 0.28f);
        route (vhs, "velocity", ParamIDs::osc (0, "FmAmount"), 0.18f);
        route (vhs, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (vhs, "macro3", ParamIDs::modDepth, 0.4f);
        route (vhs, "macro3", ParamIDs::lofiAmount, 0.3f);
        macros (vhs, "BARK", "SPACE", "WOBBLE");
        reverb (vhs, RT::plate, 0.45f, 0.2f);
        space (vhs);
        vhs.set (ParamIDs::patchLevel, -1.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (vhs);

        auto chimes = x.make ("Lab Chimes", 11, { "Keys", "FM", "Bell", "Delay" },
            "Cold, glassy chimes from a lab at night: a sine frequency-modulated by a sine four times its pitch (a pure, "
            "bell-like ring), the strike fading slower than the note, ping-pong echoes in dotted quarters into a hall. "
            "MACROS: METAL (the FM), SPACE, SHIMMER (a sine two octaves up).");
        chimes.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
              .osc (0, "FmAmount", 0.15f).osc (0, "ModGrid", (float) synth::ModGrid::harmonic).osc (0, "ModRate", 0.75f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.05f)
              .ampEnvelope (1.0f, 3500.0f, 0.0f, 2500.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f)
              .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        chimes.rig.modulation.envelopes[0] = { { SP2 { 0.0f, 1.0f, -0.6f }, SP2 { 2.0f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (chimes, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (chimes, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (chimes, "macro3", ParamIDs::osc (1, "Volume"), 0.15f);
        macros (chimes, "METAL", "SPACE", "SHIMMER");
        delay (chimes, "1/4 dotted", 0.4f, 0.25f, true, 6.0f);
        chimes.set (ParamIDs::delayToReverb, 0.4f);
        reverb (chimes, RT::hall, 0.7f, 0.25f, 30.0f, 6.0f);
        space (chimes);
        chimes.set (ParamIDs::patchLevel, -1.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chimes);

        auto brass = x.make ("Brass '84", 12, { "Keys", "Brass", "Polysynth" },
            "A punchy '80s polysynth brass for chord stabs: three detuned saws and a pulse, the filter jumping open on "
            "each chord and settling, more with velocity, through a chorus. MACROS: BRIGHT, SPACE, BITE (a bigger "
            "filter jump).");
        brass.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.45f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.5f)
             .osc (0, "Volume", 0.5f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.35f)
             .filter (FilterType::lp24, 900.0f, 0.15f, 0.45f).filterEnvelope (20.0f, 300.0f, 0.4f, 300.0f)
             .set (ParamIDs::fltVel, 0.4f).set (ParamIDs::fltKey, 0.4f)
             .ampEnvelope (5.0f, 500.0f, 0.6f, 200.0f);
        route (brass, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (brass, "macro3", ParamIDs::fltEnv, 0.3f);
        macros (brass, "BRIGHT", "SPACE", "BITE");
        chorus (brass, ParamIDs::modIds, MT::chorus, 0.6f, 0.4f, 0.4f);
        brass.set (ParamIDs::modStereo, 1.0f);
        reverb (brass, RT::hall, 0.5f, 0.2f);
        space (brass);
        brass.set (ParamIDs::patchLevel, -6.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (brass);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto ostinato = x.make ("Dark Ostinato", 13, { "Arp", "Sequence", "Filter", "Sync" },
            "Hold a minor chord: ARP 1 climbs it over two octaves in steady eighth notes on a saw and a thin square an "
            "octave up, through a resonant filter that LFO 1 opens slowly over four bars and closes again, in time with "
            "your song. Dotted-eighth echoes. MACROS: BRIGHT, SPACE, RESONANCE.");
        ostinato.osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f).osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.4f)
                .osc (0, "UniDetune", 0.08f)
                .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Transpose", 12.0f)
                .osc (1, "Volume", 0.18f).osc (1, "Arp", 1.0f)
                .filter (FilterType::lp24, 450.0f, 0.45f, 0.25f).filterEnvelope (0.0f, 200.0f, 0.2f, 150.0f)
                .ampEnvelope (2.0f, 300.0f, 0.6f, 200.0f)
                .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle)
                .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
                .set (ParamIDs::lfo (0, "Division"), division ("4/1"))
                .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
                .set (ParamIDs::lfo (0, "Phase"), 0.75f)   // starts at its lowest, as the song does
                .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
                .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
                .set (ParamIDs::arp (0, "Gate"), 0.7f);
        route (ostinato, "lfo1", ParamIDs::fltCutoff, 0.25f);
        route (ostinato, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (ostinato, "macro3", ParamIDs::fltRes, 0.35f);
        macros (ostinato, "BRIGHT", "SPACE", "RESONANCE");
        delay (ostinato, "1/8 dotted", 0.35f, 0.2f, true, 5.0f);
        reverb (ostinato, RT::hall, 0.5f, 0.2f);
        space (ostinato);
        ostinato.set (ParamIDs::patchLevel, -1.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ostinato);

        auto gated = x.make ("Gated Sequence", 14, { "Arp", "Chords", "Step Seq", "Reverb" },
            "Hold a chord: ARP 1 plays all of it at once (POLY) in a sixteenth-note rhythm, and the STEP SEQUENCER cuts "
            "each hit off sharply, so even the plate's tail stops short like a gated '80s reverb (our imitation: the "
            "synth's sound is gated, the reverb itself isn't). MACROS: BRIGHT, SPACE, SMOOTH.");
        gated.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.7f)
             .osc (0, "Volume", 0.45f).osc (0, "Arp", 1.0f)
             .filter (FilterType::lp24, 2200.0f, 0.2f, 0.25f).filterEnvelope (0.0f, 150.0f, 0.3f, 100.0f)
             .ampEnvelope (1.0f, 200.0f, 0.6f, 100.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::poly)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Gate"), 0.8f)
             .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.85f).set (ParamIDs::seqGate, 0.5f)
             .set (ParamIDs::seqContour, 0.05f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        {
            auto& p = gated.rig.modulation.arps[0].patterns[0];
            p = {};
            const bool hit[16] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0 };
            for (int k = 0; k < 16; ++k)
            {
                p.blocks[(size_t) k].on = hit[k];
                p.blocks[(size_t) k].velocity = k % 4 == 0 ? 1.0f : 0.75f;
            }
            for (int row = 0; row < 2; ++row)
                for (int k = 0; k < synth::seq::maxSteps; ++k)
                {
                    auto& step = gated.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                    step.on = hit[k % 16];
                    step.level = k % 4 == 0 ? 1.0f : 0.8f;
                }
        }
        route (gated, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (gated, "macro3", ParamIDs::seqVolMix, -0.7f);
        macros (gated, "BRIGHT", "SPACE", "SMOOTH");
        reverb (gated, RT::plate, 0.4f, 0.25f);
        space (gated);
        gated.set (ParamIDs::patchLevel, 0.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (gated);

        auto run = x.make ("Midnight Run", 15, { "Arp", "Bass", "Drums", "Sequencer" },
            "Hold a chord above the kit's keys: ARP 1 drives it as a sixteenth-note bass line an octave down (up and "
            "down), while the drum sequencer plays an '80s beat in time with your song: kick on the beats, a big snare "
            "drenched in hall reverb on 2 and 4, eighth-note hats. The kit is on C2 to B2 (DRUMS). MACROS: BRIGHT, "
            "SPACE, DRIVE (the filter's).");
        run.osc (0, "Transpose", -12.0f).osc (0, "Volume", 0.5f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
           .osc (0, "SendReverb", 0.0f).osc (0, "SendDelay", 0.0f)
           .filter (FilterType::lp24, 700.0f, 0.35f, 0.4f).filterEnvelope (0.0f, 150.0f, 0.2f, 100.0f)
           .ampEnvelope (1.0f, 200.0f, 0.5f, 80.0f)
           .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
           .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Gate"), 0.6f)
           .set (ParamIDs::drumsOn, 1.0f).set (ParamIDs::dseqOn, 1.0f)
           .set (ParamIDs::drum (0, "SendReverb"), 0.0f).set (ParamIDs::drum (0, "Decay"), 0.45f)
           .set (ParamIDs::drum (2, "SendReverb"), 0.9f).set (ParamIDs::drum (2, "Tone"), 0.55f).set (ParamIDs::drum (2, "Decay"), 0.6f)
           .set (ParamIDs::drum (6, "SendReverb"), 0.15f).set (ParamIDs::drum (6, "Pan"), 0.2f).set (ParamIDs::drum (6, "Level"), -4.0f);
        {
            auto& a = run.rig.modulation.drumPatterns[0];
            const auto note = [&a] (int row, double start, float velocity)
            {
                synth::drumseq::Note n;
                n.row = row;
                n.start = start;
                n.velocity = velocity;
                a.notes.push_back (n);
            };
            for (const auto k : { 0.0, 4.0, 8.0, 12.0 })
                note (0, k, 1.0f);
            note (2, 4.0, 1.0f);
            note (2, 12.0, 1.0f);
            for (int k = 0; k < 16; k += 2)
                note (6, k, k % 4 == 0 ? 0.8f : 0.55f);
        }
        route (run, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (run, "macro3", ParamIDs::fltDrive, 0.5f);
        macros (run, "BRIGHT", "SPACE", "DRIVE");
        reverb (run, RT::hall, 0.55f, 0.3f);
        space (run);
        run.set (ParamIDs::patchLevel, -0.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (run);
    }

    // SANDSEA (the owner's name): inspired by the score of an epic desert science-fiction film: vast low drones,
    // throat-singing and chant, keening voices with wide ornamented vibrato, enormous horn blasts, metallic ritual
    // percussion, wind and sand, melodies in the Phrygian dominant and double-harmonic scales. No title, characters,
    // places or music from the film: our own sounds.
    void sandsea (Expansion x)
    {
        using SP3 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;
        constexpr int fet = 2;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto voice = x.make ("Desert Voice", 1, { "Lead", "Vocal", "Wavetable", "Mono" },
            "A keening, distant voice: the Vowels table singing an open \"eh\", sliding between notes on a curved glide, "
            "each note scooping up from just under its pitch, a wide vibrato coming in late. The MOD WHEEL widens the "
            "vibrato into an ornament. Ping-pong echoes and a cathedral. MACROS: BRIGHT, SPACE, VOWEL (towards \"ee\").");
        voice.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.25f).osc (0, "Volume", 0.7f)
             .osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.3f).osc (0, "UniDetune", 0.08f)
             .osc (0, "VibAmount", 30.0f).osc (0, "VibRate", 5.2f).osc (0, "VibFade", 350.0f).osc (0, "VibCurve", 0.4f)
             .filter (FilterType::lp12, 4000.0f, 0.1f, 0.0f)
             .ampEnvelope (40.0f, 400.0f, 0.9f, 600.0f)
             .mono (90.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.3f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        voice.rig.modulation.pitchEnvelope = { { SP3 { 0.0f, 0.3f, -0.4f }, SP3 { 0.12f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (voice, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.35f);
        route (voice, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (voice, "macro3", ParamIDs::osc (0, "WtIndex"), 0.25f);
        macros (voice, "BRIGHT", "SPACE", "VOWEL");
        delay (voice, "1/4", 0.3f, 0.15f, true, 4.0f);
        reverb (voice, RT::cathedral, 0.4f, 0.3f, 50.0f, 4.0f);
        space (voice);
        voice.set (ParamIDs::patchLevel, 6.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (voice);

        auto horn = x.make ("Sand Horn", 2, { "Lead", "Brass", "Pitch", "Shaper" },
            "An enormous horn blast: five detuned saws over a square an octave down, through SHAPER 1's diode for grit, "
            "the filter swelling open as each note scoops up three semitones into pitch, ringing into a big hall. "
            "MACROS: BRIGHT, SPACE, GRIT.");
        horn.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.7f)
            .osc (0, "Volume", 0.55f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.45f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
            .shaper (0, synth::ShaperModel::diode, 0.3f)
            .filter (FilterType::lp24, 400.0f, 0.2f, 0.55f).filterEnvelope (150.0f, 800.0f, 0.5f, 500.0f)
            .ampEnvelope (40.0f, 600.0f, 0.9f, 600.0f)
            .mono (60.0f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 4.0f);
        horn.rig.modulation.pitchEnvelope = { { SP3 { 0.0f, 0.12f, -0.4f }, SP3 { 0.25f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (horn, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (horn, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (horn, "BRIGHT", "SPACE", "GRIT");
        reverb (horn, RT::hall, 0.75f, 0.25f, 30.0f, 4.0f);
        space (horn);
        horn.set (ParamIDs::patchLevel, -5.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (horn);

        auto flute = x.make ("Bone Flute", 3, { "Lead", "Breath", "Pitch", "Mono" },
            "A breathy desert flute: a triangle with a little bright breath noise, each note falling into pitch from a "
            "tone above like a grace note, a gentle vibrato, tape echoes in dotted eighths. MACROS: BRIGHT, SPACE, BREATH.");
        flute.osc (0, "Wave", triangle).osc (0, "Volume", 0.75f).osc (0, "Noise", 0.05f).osc (0, "NoiseColour", 0.1f)
             .osc (0, "NoiseWidth", 2.0f).osc (0, "VibAmount", 18.0f).osc (0, "VibRate", 5.5f).osc (0, "VibFade", 250.0f)
             .filter (FilterType::lp12, 2600.0f, 0.15f, 0.15f).filterEnvelope (30.0f, 300.0f, 0.7f, 300.0f)
             .ampEnvelope (30.0f, 300.0f, 0.85f, 300.0f)
             .mono (40.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        flute.rig.modulation.pitchEnvelope = { { SP3 { 0.0f, 1.0f, -0.6f }, SP3 { 0.07f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (flute, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (flute, "macro3", ParamIDs::osc (0, "Noise"), 0.15f);
        macros (flute, "BRIGHT", "SPACE", "BREATH");
        delay (flute, "1/8 dotted", 0.35f, 0.2f, false, 4.0f);
        flute.set (ParamIDs::delayType, (float) (int) effects::Delay::Type::tape);
        reverb (flute, RT::hall, 0.6f, 0.25f);
        space (flute);
        flute.set (ParamIDs::patchLevel, -3.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (flute);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto desert = x.make ("Deep Desert", 4, { "Pad", "Drone", "Noise", "Modulation" },
            "A vast, low drone you feel as much as hear: detuned saws an octave down with their sub, and a wind of "
            "noise blowing through the same resonant filter, LFO 2 gusting it slowly (the whole synth). Hold a low note "
            "or a fifth. MACROS: BRIGHT, SPACE, WIND (more noise).");
        desert.osc (0, "Transpose", -12.0f).osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.15f)
              .osc (0, "UniSpread", 0.9f).osc (0, "Sub", 0.5f).osc (0, "Volume", 0.5f)
              .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.3f).osc (1, "NoiseColour", 0.4f).osc (1, "Volume", 1.0f)
              .filter (FilterType::lp24, 500.0f, 0.3f, 0.0f)
              .ampEnvelope (2000.0f, 2000.0f, 1.0f, 3000.0f)
              .set (ParamIDs::lfo (1, "Rate"), 0.07f).set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::smoothRandom)
              .set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (desert, "lfo2", ParamIDs::fltCutoff, 0.2f);
        route (desert, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (desert, "macro3", ParamIDs::osc (1, "Noise"), 0.3f);
        macros (desert, "BRIGHT", "SPACE", "WIND");
        reverb (desert, RT::cathedral, 0.35f, 0.3f, 30.0f, 3.5f);
        space (desert);
        desert.set (ParamIDs::patchLevel, -10.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (desert);

        auto chant = x.make ("Throat Chant", 5, { "Pad", "Choir", "Wavetable", "Travel" },
            "A low chant like throat singing: the Vowels table an octave down, TRAVEL sweeping its vowel slowly back "
            "and forth (\"oo\" to \"ah\" and back, about eight seconds a sweep) so the overtones sing above the drone, "
            "through SHAPER 1's tube for grit. MACROS: BRIGHT, SPACE, GROWL.");
        chant.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 1.0f).osc (0, "Transpose", -12.0f)
             .osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.06f).osc (0, "UniSpread", 0.6f)
             .osc (0, "Volume", 0.6f)
             .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::pingPong).osc (0, "TrRate", 0.12f)
             .osc (0, "TrSync", (float) synth::mod::LfoSync::free)   // RATE in Hz, not the song's quarter notes
             .osc (0, "TrStrength", -1.0f).osc (0, "TrSmooth", 0.3f)
             .shaper (0, synth::ShaperModel::tube, 0.25f)
             .filter (FilterType::lp12, 3000.0f, 0.1f, 0.0f)
             .ampEnvelope (800.0f, 1500.0f, 0.95f, 2000.0f);
        chant.rig.modulation.travel[0].envelope = { { SP3 { 0.0f, 0.0f, 0.0f }, SP3 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (chant, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (chant, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (chant, "BRIGHT", "SPACE", "GROWL");
        chorus (chant, ParamIDs::modIds, MT::dimension, 0.2f, 0.4f, 0.35f);
        reverb (chant, RT::cathedral, 0.35f, 0.3f, 40.0f, 3.5f);
        space (chant);
        chant.set (ParamIDs::patchLevel, -4.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chant);

        auto mirage = x.make ("Mirage", 6, { "Pad", "Shimmer", "AM", "Reverb" },
            "A shimmering heat-haze pad: triangles an octave up in a wide unison, trembling under a fast AM, swelling in "
            "slowly through a reverb that plays its tail backwards, with a chorus. MACROS: BRIGHT, SPACE, SHIMMER (more AM).");
        mirage.osc (0, "Wave", triangle).osc (0, "Transpose", 12.0f).osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f)
              .osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 1.0f).osc (0, "Volume", 0.6f)
              .osc (0, "AmAmount", 0.2f).osc (0, "AmShape", 0.0f).osc (0, "ModRate", 0.55f)
              .filter (FilterType::lp12, 6000.0f, 0.05f, 0.0f)
              .ampEnvelope (1200.0f, 1500.0f, 0.9f, 2000.0f);
        route (mirage, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (mirage, "macro3", ParamIDs::osc (0, "AmAmount"), 0.3f);
        macros (mirage, "BRIGHT", "SPACE", "SHIMMER");
        chorus (mirage, ParamIDs::modIds, MT::chorus, 0.4f, 0.4f, 0.4f);
        mirage.set (ParamIDs::modStereo, 1.0f);
        reverb (mirage, RT::reverse, 0.8f, 0.35f);
        space (mirage);
        mirage.set (ParamIDs::patchLevel, -10.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (mirage);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto worm = x.make ("Worm Sign", 7, { "Bass", "Sub", "Pitch", "Mono" },
            "Something huge moving under the sand: a clean sine sub, and a gritty saw (SHAPER 1's tube) rising out of "
            "five semitones below over a second and a half, groaning slowly (LFO 1), with a long glide between notes. "
            "MACROS: BRIGHT, SPACE, GROAN (a deeper rise).");
        worm.osc (0, "Volume", 0.45f).osc (0, "PhaseRand", 0.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.7f)
            .osc (1, "PhaseRand", 0.0f).osc (1, "Filter", 0.0f).osc (1, "PitchEnv", 0.0f).osc (1, "SendReverb", 0.0f)
            .osc (1, "Shp1", 0.0f)
            .shaper (0, synth::ShaperModel::tube, 0.3f)
            .filter (FilterType::lp24, 350.0f, 0.3f, 0.0f)
            .ampEnvelope (30.0f, 600.0f, 1.0f, 600.0f)
            .mono (200.0f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 8.0f)
            .set (ParamIDs::lfo (0, "Rate"), 0.3f);
        worm.rig.modulation.pitchEnvelope = { { SP3 { 0.0f, 0.19f, 0.3f }, SP3 { 1.5f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (worm, "lfo1", ParamIDs::osc (0, "Fine"), 0.05f);
        route (worm, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (worm, "macro3", ParamIDs::pitchEnvRange, 0.2f);
        macros (worm, "BRIGHT", "SPACE", "GROAN");
        reverb (worm, RT::hall, 0.6f, 0.15f);
        space (worm);
        worm.set (ParamIDs::patchLevel, -6.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (worm);

        auto quake = x.make ("Quake Pulse", 8, { "Bass", "Hit", "Pitch", "Noise" },
            "A deep pulse of a hit: a sine dropping an octave into its note in 80 ms (the PITCH envelope), a burst of "
            "dark rumbling noise behind it, SHAPER 1's tube thickening both, ringing into a long hall (the sub dry). "
            "MACROS: BRIGHT, SPACE, DESTROY.");
        quake.osc (0, "Wave", sine).osc (0, "Volume", 0.9f).osc (0, "PhaseRand", 0.0f).osc (0, "Filter", 0.0f)
             .osc (0, "SendReverb", 0.1f).osc (0, "SendDelay", 0.0f)
             .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.9f).osc (1, "Volume", 1.0f)
             .osc (1, "PitchEnv", 0.0f)
             .shaper (0, synth::ShaperModel::tube, 0.4f)
             .filter (FilterType::lp24, 400.0f, 0.15f, 0.3f).filterEnvelope (0.0f, 900.0f, 0.0f, 600.0f)
             .ampEnvelope (1.0f, 2500.0f, 0.0f, 1200.0f).set (ParamIDs::ampDecayCurve, -0.4f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        quake.rig.modulation.pitchEnvelope = { { SP3 { 0.0f, 1.0f, -0.6f }, SP3 { 0.08f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (quake, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (quake, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (quake, "BRIGHT", "SPACE", "DESTROY");
        reverb (quake, RT::hall, 0.8f, 0.25f, 10.0f, 3.0f);
        space (quake);
        quake.set (ParamIDs::patchLevel, -1.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (quake);

        auto iron = x.make ("Iron Ritual", 9, { "Bass", "Wavetable", "Drive", "Inserts" },
            "A distorted, metallic ostinato bass: the Wavefold table with its sub, plucked by the filter, through insert "
            "bus A's ABYSS, CONSTRICTOR (FET) holding it. For driving eighth-note figures. MACROS: BRIGHT, SPACE, DESTROY.");
        iron.osc (0, "Wave", wavetable).osc (0, "Table", table ("Wavefold")).osc (0, "WtIndex", 0.4f).osc (0, "Sub", 0.4f)
            .osc (0, "PhaseRand", 0.0f).osc (0, "Volume", 0.6f).osc (0, "Insert", 1.0f)
            .shaper (0, synth::ShaperModel::tube, 0.1f)
            .filter (FilterType::lp24, 900.0f, 0.2f, 0.35f).filterEnvelope (0.0f, 200.0f, 0.3f, 100.0f)
            .ampEnvelope (1.0f, 300.0f, 0.7f, 120.0f)
            .mono (20.0f);
        drive (iron, 0, DM::abyss, 5.0f, 3.0f, 0.0f, 0.0f, 0.15f);
        eqBand (iron, 0, eq::Shape::highCut, 5000.0f, 0.0f);   // the drive's fizz off the top
        compressor (iron, fet, -18.0f, 4.0f, 2.0f);
        route (iron, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (iron, "macro3", ParamIDs::shp (0, "Drive"), 0.6f);
        macros (iron, "BRIGHT", "SPACE", "DESTROY");
        reverb (iron, RT::room, 0.4f, 0.1f);
        space (iron);
        iron.set (ParamIDs::patchLevel, 8.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (iron);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto copper = x.make ("Copper Bells", 10, { "Keys", "FM", "Bell", "Delay" },
            "Dark metallic bells: a sine modulated by oscillator 2 a minor tenth up and pulled sharp (a harsh, inharmonic "
            "ring), the strike fading slower than the note, a low sine for the body, ping-pong echoes into a cathedral. "
            "MACROS: METAL (the FM), SPACE, GRIT (SHAPER 1's tube after the filter).");
        copper.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
              .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.15f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 15.0f).osc (1, "Fine", 40.0f).osc (1, "Volume", 1.0f)
              .osc (1, "PhaseRand", 0.0f)
              .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.2f)
              .shaper (0, synth::ShaperModel::tube, 0.1f, true)
              .ampEnvelope (1.0f, 4500.0f, 0.0f, 2500.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f)
              .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        copper.rig.modulation.envelopes[0] = { { SP3 { 0.0f, 1.0f, -0.6f }, SP3 { 2.5f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (copper, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (copper, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (copper, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (copper, "METAL", "SPACE", "GRIT");
        delay (copper, "1/4", 0.4f, 0.2f, true, 4.0f);
        reverb (copper, RT::cathedral, 0.35f, 0.3f, 30.0f, 4.0f);
        space (copper);
        copper.set (ParamIDs::patchLevel, -4.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (copper);

        auto harp = x.make ("Stone Harp", 11, { "Keys", "Pluck", "String" },
            "A plucked, oud-like string: a saw with a sine an octave up, snapped by a resonant filter that closes as the "
            "string dies, brighter the higher and harder you play. For melodies in the desert scales. MACROS: BRIGHT, "
            "SPACE, TWANG (more resonance).");
        harp.osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.2f)
            .filter (FilterType::lp24, 1100.0f, 0.3f, 0.55f).filterEnvelope (0.0f, 250.0f, 0.0f, 200.0f)
            .set (ParamIDs::fltKey, 0.5f).set (ParamIDs::fltVel, 0.4f)
            .ampEnvelope (1.0f, 900.0f, 0.0f, 400.0f).set (ParamIDs::ampDecayCurve, -0.4f);
        route (harp, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (harp, "macro3", ParamIDs::fltRes, 0.3f);
        macros (harp, "BRIGHT", "SPACE", "TWANG");
        delay (harp, "1/8", 0.25f, 0.12f, true, 5.0f);
        reverb (harp, RT::hall, 0.5f, 0.2f);
        space (harp);
        harp.set (ParamIDs::patchLevel, 3.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (harp);

        auto gong = x.make ("Ritual Gong", 12, { "Keys", "FM", "Gong", "Hit" },
            "A deep, clanging gong: a sine modulated by a sine just over a tritone up (the gong's crowded, unsettled "
            "overtones), the clang fading over four seconds while the body rings on, the modulator wavering slowly so it "
            "shimmers, into a cathedral. Play low. MACROS: CLANG (the FM), SPACE, GRIT.");
        gong.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
            .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.15f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 6.0f).osc (1, "Fine", 25.0f).osc (1, "Volume", 1.0f)
            .osc (1, "PhaseRand", 0.0f)
            .shaper (0, synth::ShaperModel::tube, 0.15f, true)
            .ampEnvelope (5.0f, 6000.0f, 0.0f, 3000.0f).set (ParamIDs::ampDecayCurve, -0.5f)
            .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
            .set (ParamIDs::lfo (0, "Rate"), 0.8f);
        gong.rig.modulation.envelopes[0] = { { SP3 { 0.0f, 1.0f, -0.5f }, SP3 { 4.0f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (gong, "env1", ParamIDs::osc (0, "FmAmount"), 0.3f);
        route (gong, "lfo1", ParamIDs::osc (1, "Fine"), 0.03f);
        route (gong, "macro1", ParamIDs::osc (0, "FmAmount"), 0.3f);
        route (gong, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (gong, "CLANG", "SPACE", "GRIT");
        reverb (gong, RT::cathedral, 0.4f, 0.35f, 30.0f, 4.0f);
        space (gong);
        gong.set (ParamIDs::patchLevel, -1.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (gong);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto war = x.make ("War Drums", 13, { "Arp", "Drums", "Sequencer", "Pulse" },
            "Ritual war drums: the drum sequencer plays a tribal pattern of kick and three toms in sixteenths, in time "
            "with your song, while ARP 1 pulses a low sine in eighths on the notes you hold (above the kit's keys, C2 to "
            "B2). The toms tuned low and sent to a hall. MACROS: BRIGHT, SPACE, PULSE (louder pulse).");
        war.osc (0, "Wave", sine).osc (0, "Transpose", -12.0f).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
           .osc (0, "Arp", 1.0f).osc (0, "SendReverb", 0.2f)
           .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.15f).osc (1, "Arp", 1.0f)
           .filter (FilterType::lp24, 1200.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 150.0f, 0.2f, 100.0f)
           .ampEnvelope (1.0f, 250.0f, 0.4f, 100.0f)
           .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
           .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Gate"), 0.4f)
           .set (ParamIDs::drumsOn, 1.0f).set (ParamIDs::dseqOn, 1.0f)
           .set (ParamIDs::drum (0, "SendReverb"), 0.1f).set (ParamIDs::drum (0, "Decay"), 0.5f)
           .set (ParamIDs::drum (5, "Pitch"), -5.0f).set (ParamIDs::drum (5, "SendReverb"), 0.5f).set (ParamIDs::drum (5, "Pan"), -0.3f)
           .set (ParamIDs::drum (7, "Pitch"), -5.0f).set (ParamIDs::drum (7, "SendReverb"), 0.5f)
           .set (ParamIDs::drum (9, "Pitch"), -5.0f).set (ParamIDs::drum (9, "SendReverb"), 0.5f).set (ParamIDs::drum (9, "Pan"), 0.3f);
        {
            auto& a = war.rig.modulation.drumPatterns[0];
            const auto note = [&a] (int row, double start, float velocity)
            {
                synth::drumseq::Note n;
                n.row = row;
                n.start = start;
                n.velocity = velocity;
                a.notes.push_back (n);
            };
            constexpr int kick = 0, low = 5, mid = 7, high = 9;
            for (const auto k : { 0.0, 6.0, 10.0 })
                note (kick, k, 1.0f);
            for (const auto k : { 0.0, 3.0, 8.0, 11.0, 14.0 })
                note (low, k, k == 0.0 || k == 8.0 ? 1.0f : 0.75f);
            note (mid, 4.0, 0.9f);
            note (mid, 12.0, 0.9f);
            note (high, 7.0, 0.7f);
            note (high, 15.0, 0.8f);
            note (mid, 15.5, 0.6f);
        }
        route (war, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (war, "macro3", ParamIDs::osc (0, "Volume"), 0.2f);
        macros (war, "BRIGHT", "SPACE", "PULSE");
        reverb (war, RT::hall, 0.6f, 0.25f);
        space (war);
        war.set (ParamIDs::patchLevel, -2.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (war);

        auto ripple = x.make ("Sand Ripple", 14, { "Arp", "Randomizer", "Pluck", "Scale" },
            "Fast runs like wind over sand: ARP 1's randomizer picks notes from the Phrygian dominant scale (flat second, "
            "major third) over two octaves in sixteenths, a new pattern every pass, each a short bright pluck, with "
            "ping-pong echoes. Hold one key: its scale's root. MACROS: BRIGHT, SPACE, SPARKLE (a sine two octaves up).");
        ripple.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.05f).osc (1, "Arp", 1.0f)
              .filter (FilterType::lp24, 2500.0f, 0.25f, 0.4f).filterEnvelope (0.0f, 150.0f, 0.0f, 100.0f)
              .ampEnvelope (0.5f, 250.0f, 0.0f, 150.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.5f);
        {
            auto& r = ripple.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Phrygian Dominant");
            r.restChance = 0.15f;
            r.lengthChances = { 1.0f, 0.15f, 0.0f, 0.0f };
            r.velocityLow = 0.55f;
            r.autoDice = true;
            r.seed = 21;
            auto& p = ripple.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        route (ripple, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (ripple, "macro3", ParamIDs::osc (1, "Volume"), 0.15f);
        macros (ripple, "BRIGHT", "SPACE", "SPARKLE");
        delay (ripple, "1/8 dotted", 0.35f, 0.2f, true, 6.0f);
        reverb (ripple, RT::hall, 0.5f, 0.2f);
        space (ripple);
        ripple.set (ParamIDs::patchLevel, 1.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ripple);

        auto caravan = x.make ("Caravan", 15, { "Arp", "Chords", "Modulation", "Triplet" },
            "A slow, swaying caravan: hold a chord and ARP 1 walks it up and down in eighth-note triplets on a warm saw "
            "with a triangle an octave down, while LFO 1 sways the whole sound left and right every half bar, in time "
            "with your song. MACROS: BRIGHT, SPACE, WARMTH (the low triangle).");
        caravan.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.12f).osc (0, "Volume", 0.5f)
               .osc (0, "Arp", 1.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
               .filter (FilterType::lp24, 1400.0f, 0.2f, 0.25f).filterEnvelope (0.0f, 300.0f, 0.3f, 200.0f)
               .ampEnvelope (5.0f, 400.0f, 0.6f, 300.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
               .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Timing"), 1.0f)
               .set (ParamIDs::arp (0, "Octaves"), 2.0f).set (ParamIDs::arp (0, "Gate"), 0.75f)
               .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
               .set (ParamIDs::lfo (0, "Division"), division ("1/2"))
               .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (caravan, "lfo1", ParamIDs::ampPan, 0.3f);
        route (caravan, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (caravan, "macro3", ParamIDs::osc (1, "Volume"), 0.2f);
        macros (caravan, "BRIGHT", "SPACE", "WARMTH");
        delay (caravan, "1/4", 0.3f, 0.15f, true, 4.0f);
        reverb (caravan, RT::hall, 0.55f, 0.22f);
        space (caravan);
        caravan.set (ParamIDs::patchLevel, -1.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (caravan);
    }

    // DATASTREAM (the owner's name): inspired by the scores of a digital-world science-fiction film and its sequel:
    // precise glowing synth pulses locked to the beat, huge throbbing analog basses, orchestral-electronic swells,
    // clean leads with long glides, a robotic vocal texture, crisp digital bleeps. Cold and exact: no tape, no wobble.
    // No title, characters, places or music from the films: our own sounds.
    void datastream (Expansion x)
    {
        using SP4 = synth::mod::ShapePoint;
        using GM = effects::Glitch::Mode;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto trail = x.make ("Light Trail", 1, { "Lead", "Sync", "Glide", "Mono" },
            "A bright, clean lead that streaks between notes: three saws, lightly hard-synced with the sync sweeping down "
            "as each note starts, on a long curved glide, with ping-pong echoes and a hall. MACROS: BRIGHT, SPACE, GLOW "
            "(more sync).");
        trail.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.5f)
             .osc (0, "Volume", 0.6f).osc (0, "SyncAmount", 0.2f).osc (0, "SyncEnv", 0.3f).osc (0, "SyncEnvTime", 250.0f)
             .filter (FilterType::lp24, 6000.0f, 0.15f, 0.0f)
             .ampEnvelope (5.0f, 400.0f, 0.9f, 400.0f)
             .mono (180.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.4f);
        route (trail, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (trail, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.3f);
        macros (trail, "BRIGHT", "SPACE", "GLOW");
        delay (trail, "1/8 dotted", 0.35f, 0.2f, true, 6.0f);
        reverb (trail, RT::hall, 0.6f, 0.2f);
        space (trail);
        trail.set (ParamIDs::patchLevel, -0.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (trail);

        auto vector = x.make ("Vector Voice", 2, { "Lead", "Vocal", "Wavetable", "Travel" },
            "A robotic, talk-box-like voice: the Vowels table saying \"ee\" and opening towards \"ah\" on every note "
            "(TRAVEL, free-running, about 0.4 s), a little bit-crushed so it sounds machine-made. MACROS: BRIGHT, SPACE, "
            "ROBOT (more crush).");
        vector.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.5f).osc (0, "Volume", 0.7f)
              .osc (0, "Crush", 0.12f).osc (0, "CrushMode", 0.0f)
              .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::oneShot)
              .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrRate", 2.5f).osc (0, "TrStrength", -0.5f)
              .osc (0, "TrSmooth", 0.1f)
              .filter (FilterType::lp12, 5000.0f, 0.1f, 0.0f)
              .ampEnvelope (5.0f, 300.0f, 0.9f, 250.0f)
              .mono (50.0f);
        vector.rig.modulation.travel[0].envelope = { { SP4 { 0.0f, 0.0f, -0.3f }, SP4 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (vector, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (vector, "macro3", ParamIDs::osc (0, "Crush"), 0.3f);
        macros (vector, "BRIGHT", "SPACE", "ROBOT");
        delay (vector, "1/8", 0.3f, 0.15f, true, 5.0f);
        reverb (vector, RT::plate, 0.5f, 0.15f);
        space (vector);
        vector.set (ParamIDs::patchLevel, 9.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (vector);

        auto burst = x.make ("Data Burst", 3, { "Lead", "Pulse", "Pitch", "Mono" },
            "A crisp square lead that starts each note with a quick blip down from an octave above (the PITCH envelope, "
            "30 ms), like data arriving. Dotted-eighth echoes. MACROS: BRIGHT, SPACE, BLIP (a bigger blip).");
        burst.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.6f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.25f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
             .filter (FilterType::lp24, 5000.0f, 0.15f, 0.0f)
             .ampEnvelope (1.0f, 300.0f, 0.8f, 150.0f)
             .mono (20.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        burst.rig.modulation.pitchEnvelope = { { SP4 { 0.0f, 1.0f, -0.7f }, SP4 { 0.03f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (burst, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (burst, "macro3", ParamIDs::pitchEnvRange, 0.3f);
        macros (burst, "BRIGHT", "SPACE", "BLIP");
        delay (burst, "1/8 dotted", 0.3f, 0.15f, true, 6.0f);
        reverb (burst, RT::room, 0.5f, 0.12f);
        space (burst);
        burst.set (ParamIDs::patchLevel, -4.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (burst);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto horizon = x.make ("Grid Horizon", 4, { "Pad", "Unison", "Orchestral", "Swell" },
            "A vast, cold swell where synth meets orchestra: seven wide saws over a darker unison an octave down, the "
            "filter opening slowly as the chord swells in like strings, into a big hall. MACROS: BRIGHT, SPACE, SWELL "
            "(a bigger opening).");
        horizon.osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.8f).osc (0, "UniDetune", 0.25f).osc (0, "UniSpread", 1.0f)
               .osc (0, "Volume", 0.45f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.25f)
               .osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.6f).osc (1, "UniDetune", 0.15f).osc (1, "UniSpread", 0.8f)
               .filter (FilterType::lp12, 1500.0f, 0.1f, 0.2f).filterEnvelope (1500.0f, 3000.0f, 0.8f, 2000.0f)
               .set (ParamIDs::fltKey, 0.4f)
               .ampEnvelope (1500.0f, 2000.0f, 0.9f, 2500.0f);
        route (horizon, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (horizon, "macro3", ParamIDs::fltEnv, 0.3f);
        macros (horizon, "BRIGHT", "SPACE", "SWELL");
        reverb (horizon, RT::hall, 0.8f, 0.3f, 30.0f, 4.5f);
        eqBand (horizon, 0, eq::Shape::lowCut, 100.0f, 0.0f);
        space (horizon);
        horizon.set (ParamIDs::patchLevel, -8.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (horizon);

        auto cathedral = x.make ("Neon Cathedral", 5, { "Pad", "Organ", "Wavetable", "Travel" },
            "A glowing, organ-like pad: the Organ Stack table slowly opening and closing its upper harmonics (TRAVEL, "
            "free-running, about twelve seconds a cycle), a slight unison and chorus, in a cathedral. MACROS: BRIGHT, "
            "SPACE, HARMONICS.");
        cathedral.osc (0, "Wave", wavetable).osc (0, "Table", table ("Organ Stack")).osc (0, "WtIndex", 0.3f)
                 .osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.6f)
                 .osc (0, "Volume", 0.55f)
                 .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::pingPong)
                 .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrRate", 0.08f).osc (0, "TrStrength", 0.5f)
                 .osc (0, "TrSmooth", 0.3f)
                 .filter (FilterType::lp12, 5000.0f, 0.05f, 0.0f)
                 .ampEnvelope (800.0f, 1500.0f, 0.95f, 2000.0f);
        cathedral.rig.modulation.travel[0].envelope = { { SP4 { 0.0f, 0.0f, 0.0f }, SP4 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (cathedral, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (cathedral, "macro3", ParamIDs::osc (0, "WtIndex"), 0.3f);
        macros (cathedral, "BRIGHT", "SPACE", "HARMONICS");
        chorus (cathedral, ParamIDs::modIds, MT::chorus, 0.3f, 0.3f, 0.3f);
        reverb (cathedral, RT::cathedral, 0.4f, 0.35f, 40.0f, 4.5f);
        space (cathedral);
        cathedral.set (ParamIDs::patchLevel, -3.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (cathedral);

        auto sky = x.make ("Digital Sky", 6, { "Pad", "Step Seq", "Unison", "Stereo" },
            "A shimmering pad cut into exact sixteenths by the STEP SEQUENCER, the off-beats bouncing between left and "
            "right, in time with your song: bright triangles an octave up over a soft saw. MACROS: BRIGHT, SPACE, SMOOTH "
            "(less gating).");
        sky.osc (0, "Wave", triangle).osc (0, "Transpose", 12.0f).osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f)
           .osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 1.0f).osc (0, "Volume", 0.5f)
           .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Volume", 0.25f)
           .filter (FilterType::lp24, 4000.0f, 0.15f, 0.0f)
           .ampEnvelope (300.0f, 1000.0f, 0.9f, 1200.0f)
           .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqMode, (float) synth::seq::Mode::stereo).set (ParamIDs::seqWidth, 0.8f)
           .set (ParamIDs::seqVolMix, 0.8f).set (ParamIDs::seqGate, 0.55f).set (ParamIDs::seqContour, 0.08f)
           .set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = sky.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                const auto at = k % 4;
                step.on = at == 0 || (at % 2 == 1) == (row == (k / 4) % 2);
                step.level = at == 0 ? 1.0f : 0.7f;
            }
        route (sky, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (sky, "macro3", ParamIDs::seqVolMix, -0.7f);
        macros (sky, "BRIGHT", "SPACE", "SMOOTH");
        reverb (sky, RT::plate, 0.6f, 0.25f);
        space (sky);
        sky.set (ParamIDs::patchLevel, -6.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (sky);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto engine = x.make ("Pulse Engine", 7, { "Bass", "Modulation", "Sub", "Mono" },
            "A huge bass that throbs in sixteenths with your song: a saw with its sub through a resonant low-pass that "
            "LFO 1 snaps open on every sixteenth, CONSTRICTOR (FET) keeping it solid. Hold a note. MACROS: BRIGHT, SPACE, "
            "GROWL (the filter's drive).");
        engine.osc (0, "Volume", 0.6f).osc (0, "Sub", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "SendReverb", 0.15f)
              .osc (0, "SendDelay", 0.0f)
              .filter (FilterType::lp24, 300.0f, 0.3f, 0.0f)
              .ampEnvelope (2.0f, 300.0f, 1.0f, 150.0f)
              .mono (30.0f)
              .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sawDown)
              .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
              .set (ParamIDs::lfo (0, "Division"), division ("1/16"))
              .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
              .set (ParamIDs::lfo (0, "Offset"), 1.0f);
        compressor (engine, 2, -18.0f, 4.0f, 2.0f);
        route (engine, "lfo1", ParamIDs::fltCutoff, 0.25f);
        route (engine, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (engine, "macro3", ParamIDs::fltDrive, 0.5f);
        macros (engine, "BRIGHT", "SPACE", "GROWL");
        reverb (engine, RT::room, 0.3f, 0.08f);
        space (engine);
        engine.set (ParamIDs::patchLevel, 6.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (engine);

        auto bitfall = x.make ("Bitfall Bass", 8, { "Bass", "Crush", "Mod Wheel", "Lo-Fi" },
            "A gritty digital bass that breaks apart as you push it: a pulse with its sub, plucked by the filter; the MOD "
            "WHEEL bit-crushes it, MACRO 3 brings in LO-FI's bitcrusher on top. MACROS: BRIGHT, SPACE, CRUSH.");
        bitfall.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Sub", 0.5f).osc (0, "PhaseRand", 0.0f).osc (0, "Volume", 0.6f)
               .osc (0, "CrushMode", 2.0f).osc (0, "SendReverb", 0.1f)
               .filter (FilterType::lp24, 800.0f, 0.25f, 0.3f).filterEnvelope (0.0f, 250.0f, 0.4f, 120.0f)
               .ampEnvelope (2.0f, 400.0f, 0.85f, 150.0f)
               .mono (25.0f)
               .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::bitcrusher)
               .set (ParamIDs::lofiAmount, 6.0f).set (ParamIDs::lofiMix, 0.0f).set (ParamIDs::lofiNoise, 0.0f);
        route (bitfall, "modWheel", ParamIDs::osc (0, "Crush"), 0.6f);
        route (bitfall, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (bitfall, "macro3", ParamIDs::lofiMix, 0.6f);
        macros (bitfall, "BRIGHT", "SPACE", "CRUSH");
        reverb (bitfall, RT::room, 0.3f, 0.08f);
        space (bitfall);
        bitfall.set (ParamIDs::patchLevel, -3.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (bitfall);

        auto lattice = x.make ("Deep Lattice", 9, { "Bass", "Octave", "Sub", "Mono" },
            "A deep, round bass for slow, heavy lines: a triangle with its octave above (the chorder's +12) and a sine "
            "sub, through a gentle low-pass, with a short glide. MACROS: BRIGHT, SPACE, WEIGHT (more sub).");
        lattice.osc (0, "Wave", triangle).osc (0, "Volume", 0.65f).osc (0, "Sub", 0.5f).osc (0, "PhaseRand", 0.0f)
               .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 12.0f).osc (0, "Ch1Vol", 0.4f).osc (0, "SendReverb", 0.1f)
               .filter (FilterType::lp24, 600.0f, 0.15f, 0.15f).filterEnvelope (0.0f, 400.0f, 0.5f, 200.0f)
               .ampEnvelope (5.0f, 500.0f, 0.9f, 250.0f)
               .mono (60.0f);
        route (lattice, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (lattice, "macro3", ParamIDs::osc (0, "Sub"), 0.3f);
        macros (lattice, "BRIGHT", "SPACE", "WEIGHT");
        reverb (lattice, RT::room, 0.3f, 0.08f);
        space (lattice);
        lattice.set (ParamIDs::patchLevel, -2.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (lattice);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto console = x.make ("Glass Console", 10, { "Keys", "FM", "Glass" },
            "Cold, glassy keys: a sine modulated by oscillator 2 an octave up (a clean, bright ring), the strike settling "
            "fast, brighter with velocity, a sine two octaves up for sparkle, ping-pong echoes and a plate. MACROS: "
            "BRIGHT (the FM), SPACE, SHIMMER.");
        console.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
               .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.1f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
               .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 24.0f).osc (2, "Volume", 0.05f)
               .ampEnvelope (1.0f, 2000.0f, 0.15f, 600.0f).set (ParamIDs::ampVel, 0.5f)
               .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::sustain);
        console.rig.modulation.envelopes[0] = { { SP4 { 0.0f, 1.0f, -0.5f }, SP4 { 0.5f, 0.1f, 0.0f } }, 1, -1, -1 };
        route (console, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (console, "velocity", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (console, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (console, "macro3", ParamIDs::osc (2, "Volume"), 0.15f);
        macros (console, "BRIGHT", "SPACE", "SHIMMER");
        delay (console, "1/8 dotted", 0.35f, 0.18f, true, 6.0f);
        reverb (console, RT::plate, 0.5f, 0.2f);
        space (console);
        console.set (ParamIDs::patchLevel, -9.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (console);

        auto protocol = x.make ("Brass Protocol", 11, { "Keys", "Brass", "Orchestral", "Shaper" },
            "A big orchestral-synth brass stab: five wide saws over a pulse an octave down, the filter punching open on "
            "each chord and settling, SHAPER 1 rounding it off, in a large hall. MACROS: BRIGHT, SPACE, BITE (a bigger "
            "punch).");
        protocol.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.8f)
                .osc (0, "Volume", 0.5f)
                .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.45f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
                .shaper (0, synth::ShaperModel::soft, 0.15f)
                .filter (FilterType::lp24, 700.0f, 0.15f, 0.5f).filterEnvelope (30.0f, 600.0f, 0.45f, 400.0f)
                .set (ParamIDs::fltVel, 0.4f).set (ParamIDs::fltKey, 0.4f)
                .ampEnvelope (10.0f, 800.0f, 0.7f, 400.0f);
        route (protocol, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (protocol, "macro3", ParamIDs::fltEnv, 0.3f);
        macros (protocol, "BRIGHT", "SPACE", "BITE");
        reverb (protocol, RT::hall, 0.7f, 0.25f);
        space (protocol);
        protocol.set (ParamIDs::patchLevel, -11.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (protocol);

        auto chimes = x.make ("Bit Chimes", 12, { "Keys", "FM", "Crush", "Bell" },
            "Crisp digital bleeps: a sine with FM from a sine three times its pitch, bit-crushed (CRUSH's BIT) so it "
            "sounds made of numbers, a short ring and ping-pong echoes in dotted sixteenths. MACROS: BRIGHT (the FM), "
            "SPACE, CRUSH.");
        chimes.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
              .osc (0, "FmAmount", 0.2f).osc (0, "ModGrid", (float) synth::ModGrid::harmonic).osc (0, "ModRate", 0.7f)
              .osc (0, "Crush", 0.3f).osc (0, "CrushMode", 1.0f)
              .ampEnvelope (1.0f, 800.0f, 0.0f, 400.0f).set (ParamIDs::ampDecayCurve, -0.4f).set (ParamIDs::ampVel, 0.4f);
        route (chimes, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (chimes, "macro3", ParamIDs::osc (0, "Crush"), 0.3f);
        macros (chimes, "BRIGHT", "SPACE", "CRUSH");
        delay (chimes, "1/16 dotted", 0.4f, 0.2f, true, 7.0f);
        reverb (chimes, RT::plate, 0.4f, 0.15f);
        space (chimes);
        chimes.set (ParamIDs::patchLevel, 2.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chimes);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto overclock = x.make ("Overclock", 13, { "Arp", "Sequence", "Filter", "Sync" },
            "Hold a chord: ARP 1 drives it up two octaves in tight sixteenths on a saw and a thin square an octave up, "
            "through a resonant filter that LFO 1 opens over four bars and closes again, in time with your song. "
            "Dotted-eighth echoes. MACROS: BRIGHT, SPACE, RESONANCE.");
        overclock.osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.25f).osc (1, "Transpose", 12.0f)
                 .osc (1, "Volume", 0.15f).osc (1, "Arp", 1.0f)
                 .filter (FilterType::lp24, 600.0f, 0.4f, 0.25f).filterEnvelope (0.0f, 120.0f, 0.2f, 100.0f)
                 .ampEnvelope (1.0f, 200.0f, 0.5f, 120.0f)
                 .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle)
                 .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
                 .set (ParamIDs::lfo (0, "Division"), division ("4/1"))
                 .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
                 .set (ParamIDs::lfo (0, "Phase"), 0.75f)
                 .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
                 .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
                 .set (ParamIDs::arp (0, "Gate"), 0.6f);
        route (overclock, "lfo1", ParamIDs::fltCutoff, 0.3f);
        route (overclock, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (overclock, "macro3", ParamIDs::fltRes, 0.35f);
        macros (overclock, "BRIGHT", "SPACE", "RESONANCE");
        delay (overclock, "1/8 dotted", 0.35f, 0.18f, true, 5.0f);
        reverb (overclock, RT::hall, 0.5f, 0.18f);
        space (overclock);
        overclock.set (ParamIDs::patchLevel, -0.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (overclock);

        auto system = x.make ("System Pulse", 14, { "Arp", "Chords", "Step Seq", "Stereo" },
            "Hold a chord: ARP 1 plays it whole (POLY) in an eighth-note rhythm, and the STEP SEQUENCER chops each chord "
            "into sixteenths that bounce between left and right, in time with your song. MACROS: BRIGHT, SPACE, SMOOTH "
            "(less chopping).");
        system.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.6f)
              .osc (0, "Volume", 0.45f).osc (0, "Arp", 1.0f)
              .filter (FilterType::lp24, 2500.0f, 0.2f, 0.2f).filterEnvelope (0.0f, 200.0f, 0.4f, 150.0f)
              .ampEnvelope (2.0f, 300.0f, 0.7f, 150.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::poly)
              .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Gate"), 0.9f)
              .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqMode, (float) synth::seq::Mode::stereo).set (ParamIDs::seqWidth, 0.9f)
              .set (ParamIDs::seqVolMix, 0.85f).set (ParamIDs::seqGate, 0.5f).set (ParamIDs::seqContour, 0.05f)
              .set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        {
            auto& p = system.rig.modulation.arps[0].patterns[0];
            p = {};
            const bool hit[16] = { 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1 };
            for (int k = 0; k < 16; ++k)
            {
                p.blocks[(size_t) k].on = hit[k];
                p.blocks[(size_t) k].velocity = k % 4 == 0 ? 1.0f : 0.75f;
            }
            for (int row = 0; row < 2; ++row)
                for (int k = 0; k < synth::seq::maxSteps; ++k)
                {
                    auto& step = system.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                    step.on = (k % 2 == 0) == (row == 0) || k % 4 == 0;
                    step.level = k % 4 == 0 ? 1.0f : 0.75f;
                }
        }
        route (system, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (system, "macro3", ParamIDs::seqVolMix, -0.7f);
        macros (system, "BRIGHT", "SPACE", "SMOOTH");
        delay (system, "1/4 dotted", 0.35f, 0.15f, true, 5.0f);
        reverb (system, RT::plate, 0.5f, 0.2f);
        space (system);
        system.set (ParamIDs::patchLevel, 0.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (system);

        auto circuit = x.make ("Circuit Run", 15, { "Arp", "Randomizer", "Glitch", "Pluck" },
            "A fast digital run: ARP 1's randomizer picks notes from the minor scale over two octaves in sixteenths, "
            "with rests and ratchet bursts, a new pattern every pass, each a short square pluck through insert bus A's "
            "GLITCH stutters. Hold one key. MACROS: BRIGHT, SPACE, CRUSH.");
        circuit.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
               .osc (0, "CrushMode", 1.0f)
               .filter (FilterType::lp24, 3500.0f, 0.2f, 0.35f).filterEnvelope (0.0f, 120.0f, 0.0f, 100.0f)
               .ampEnvelope (0.5f, 180.0f, 0.0f, 100.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
               .set (ParamIDs::arp (0, "Gate"), 0.5f);
        {
            auto& r = circuit.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Minor");
            r.restChance = 0.2f;
            r.ratchetChance = 0.25f;
            r.lengthChances = { 1.0f, 0.1f, 0.0f, 0.0f };
            r.velocityLow = 0.6f;
            r.autoDice = true;
            r.seed = 31;
            auto& p = circuit.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        glitch (circuit, 0, GM::sequence, "1/32", 4.0f, 3.0f, 0.25f);
        circuit.set (ParamIDs::ins (0, "SendDelay"), 0.4f).set (ParamIDs::ins (0, "SendReverb"), 0.3f);
        route (circuit, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (circuit, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        macros (circuit, "BRIGHT", "SPACE", "CRUSH");
        delay (circuit, "1/8 dotted", 0.3f, 0.15f, true, 6.0f);
        reverb (circuit, RT::plate, 0.4f, 0.15f);
        space (circuit);
        circuit.set (ParamIDs::patchLevel, 1.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (circuit);
    }

    // The felt piano (LAST LIGHT): synthesis, not samples. A sine's FM from a sine at the same pitch gives a full,
    // piano-like set of harmonics at the strike, fading over 0.4 s as felt does (ENV 1); a triangle an octave up a few
    // cents sharp and a little right (strings never quite in tune; no unison: its copies start at random phases, so
    // notes came out up to 7 dB apart); a short thump of noise for the felt hammer (ENV 2 on its volume,
    // 50 ms). The hammer is the FILTER: its envelope opens it at the strike, more the harder you play (ENV VEL), and
    // VELOCITY keeps soft notes darker all through; it follows the keys. A low shelf keeps the bottom octaves from
    // booming. A long fall with a damper on release. The MOD WHEEL adds the hammer's noise (the mechanics); FELT
    // (MACRO 3) clears the felt away.
    void feltPiano (Builder& b)
    {
        using SP5 = synth::mod::ShapePoint;
        b.osc (0, "Wave", sine).osc (0, "Volume", 0.8f).osc (0, "PhaseRand", 0.0f)
         .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.06f)
         .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
         .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Transpose", 12.0f).osc (2, "Fine", 3.0f).osc (2, "Volume", 0.1f)
         .osc (2, "Pan", 0.2f)
         .osc (3, "On", 1.0f).osc (3, "Gain", 0.0f).osc (3, "Noise", 0.3f).osc (3, "NoiseColour", 0.55f).osc (3, "Volume", 0.0f)
         .osc (3, "NoiseWidth", 2.0f)
         .filter (FilterType::lp12, 900.0f, 0.05f, 0.35f).filterEnvelope (0.0f, 600.0f, 0.25f, 300.0f)
         .set (ParamIDs::fltKey, 0.8f).set (ParamIDs::fltVel, 0.6f).set (ParamIDs::fltEnvVel, 0.8f)
         .ampEnvelope (2.0f, 6000.0f, 0.0f, 450.0f).set (ParamIDs::ampDecayCurve, -0.6f).set (ParamIDs::ampVel, 0.7f)
         .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
         .set (ParamIDs::env (1, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        b.rig.modulation.envelopes[0] = { { SP5 { 0.0f, 1.0f, -0.6f }, SP5 { 0.4f, 0.0f, 0.0f } }, -1, -1, -1 };
        b.rig.modulation.envelopes[1] = { { SP5 { 0.0f, 1.0f, -0.5f }, SP5 { 0.05f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (b, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (b, "velocity", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (b, "env2", ParamIDs::osc (3, "Volume"), 0.25f);
        route (b, "modWheel", ParamIDs::osc (3, "Noise"), 0.3f);
        route (b, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (b, "macro3", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (b, "macro3", ParamIDs::fltEnv, 0.25f);
        eqBand (b, 0, eq::Shape::lowShelf, 120.0f, -3.0f);
    }

    // LAST LIGHT (the owner's name): inspired by the score of a film about space, time and leaving the people you love:
    // an enormous church organ, quiet repeating piano, a ticking clock, slow swells, vast cold spaces. Sad, longing,
    // melancholy. No title, characters, places or music from the film: our own sounds.
    void lastLight (Expansion x)
    {
        using SP5 = synth::mod::ShapePoint;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto signal = x.make ("Lonely Signal", 1, { "Lead", "Sine", "Glide", "Mono" },
            "A lonely, pure lead calling across a great distance: a sine with a whisper of a triangle an octave up, a "
            "slow curved glide between notes, a slow vibrato that comes in late, echoes and a cathedral. MACROS: BRIGHT, "
            "SPACE, VIBRATO.");
        signal.osc (0, "Wave", sine).osc (0, "Volume", 0.75f).osc (0, "VibAmount", 12.0f).osc (0, "VibRate", 4.5f)
              .osc (0, "VibFade", 900.0f).osc (0, "VibCurve", 0.5f)
              .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.12f)
              .osc (1, "VibAmount", 12.0f).osc (1, "VibRate", 4.5f).osc (1, "VibFade", 900.0f)
              .filter (FilterType::lp12, 3000.0f, 0.05f, 0.0f)
              .ampEnvelope (120.0f, 600.0f, 0.9f, 900.0f)
              .mono (120.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.4f);
        route (signal, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (signal, "macro3", ParamIDs::osc (0, "VibAmount"), 0.25f);
        route (signal, "macro3", ParamIDs::osc (1, "VibAmount"), 0.25f);
        macros (signal, "BRIGHT", "SPACE", "VIBRATO");
        delay (signal, "1/4", 0.3f, 0.15f, true, 4.0f);
        signal.set (ParamIDs::delayToReverb, 0.4f);
        reverb (signal, RT::cathedral, 0.5f, 0.32f, 60.0f, 4.0f);
        space (signal);
        signal.set (ParamIDs::patchLevel, -4.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (signal);

        auto cello = x.make ("Cello Drift", 2, { "Lead", "Bowed", "Aftertouch", "Mono" },
            "A slow, bowed, cello-like lead: two saws a hair apart through a warm low-pass, a little bow noise, a slow "
            "attack and vibrato that comes in as the note settles. Press after the note (aftertouch) to bow harder: "
            "brighter and louder. MACROS: BRIGHT, SPACE, BOW (more bow noise).");
        cello.osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "Volume", 0.65f)
             .osc (0, "Noise", 0.01f).osc (0, "NoiseColour", 0.6f).osc (0, "NoiseWidth", 2.0f)
             .osc (0, "VibAmount", 20.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 500.0f)
             .shaper (0, synth::ShaperModel::soft, 0.1f)
             .filter (FilterType::lp24, 1200.0f, 0.15f, 0.1f).filterEnvelope (200.0f, 800.0f, 0.8f, 600.0f)
             .set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (250.0f, 600.0f, 0.9f, 700.0f)
             .set (ParamIDs::ampVolume, 0.85f)
             .mono (80.0f);
        route (cello, "aftertouch", ParamIDs::fltCutoff, 0.15f, 120.0f);
        route (cello, "aftertouch", ParamIDs::ampVolume, 0.08f, 120.0f);
        route (cello, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (cello, "macro3", ParamIDs::osc (0, "Noise"), 0.04f);
        macros (cello, "BRIGHT", "SPACE", "BOW");
        reverb (cello, RT::hall, 0.7f, 0.25f, 30.0f, 4.0f);
        space (cello);
        cello.set (ParamIDs::patchLevel, -3.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (cello);

        auto elegy = x.make ("Wordless Elegy", 3, { "Lead", "Choir", "Wavetable", "Mono" },
            "A soft, wordless voice: the Vowels table singing \"oo\" in a small unison, a slow attack, gentle vibrato, a "
            "glide between notes, in a cathedral. MACROS: BRIGHT, SPACE, VOWEL (opening to \"oh\").");
        elegy.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.95f).osc (0, "Volume", 0.7f)
             .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.6f)
             .osc (0, "VibAmount", 15.0f).osc (0, "VibRate", 4.8f).osc (0, "VibFade", 600.0f)
             .filter (FilterType::lp12, 3500.0f, 0.05f, 0.0f)
             .ampEnvelope (300.0f, 800.0f, 0.9f, 1200.0f)
             .mono (100.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always);
        route (elegy, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (elegy, "macro3", ParamIDs::osc (0, "WtIndex"), -0.25f);
        macros (elegy, "BRIGHT", "SPACE", "VOWEL");
        reverb (elegy, RT::cathedral, 0.5f, 0.35f, 50.0f, 4.0f);
        space (elegy);
        elegy.set (ParamIDs::patchLevel, -0.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (elegy);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto organ = x.make ("Cathedral Organ", 4, { "Pad", "Organ", "Wavetable", "Mod Wheel" },
            "An enormous pipe organ in a stone cathedral: the Organ Stack table with its sub for the pedal ranks, a "
            "slow speaking attack and a long ring. The MOD WHEEL is the swell: louder and brighter, from a whisper to "
            "the full organ. MACROS: BRIGHT, SPACE, STOPS (more upper ranks).");
        organ.osc (0, "Wave", wavetable).osc (0, "Table", table ("Organ Stack")).osc (0, "WtIndex", 0.5f)
             .osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.05f).osc (0, "UniSpread", 0.6f)
             .osc (0, "Sub", 0.3f).osc (0, "Volume", 0.55f)
             .filter (FilterType::lp12, 2500.0f, 0.05f, 0.0f).set (ParamIDs::fltKey, 0.3f)
             .ampEnvelope (400.0f, 1000.0f, 1.0f, 2500.0f)
             .set (ParamIDs::ampVolume, 0.7f);
        route (organ, "modWheel", ParamIDs::ampVolume, 0.2f, 200.0f);
        route (organ, "modWheel", ParamIDs::fltCutoff, 0.25f, 200.0f);
        route (organ, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (organ, "macro3", ParamIDs::osc (0, "WtIndex"), 0.3f);
        macros (organ, "BRIGHT", "SPACE", "STOPS");
        reverb (organ, RT::cathedral, 0.5f, 0.4f, 40.0f, 4.0f);
        space (organ);
        organ.set (ParamIDs::patchLevel, -5.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (organ);

        auto strings = x.make ("Horizon Strings", 5, { "Pad", "Strings", "Unison", "Aftertouch" },
            "Slow, aching string swells: seven wide saws over a darker unison an octave down, the bow drawing in over "
            "almost two seconds and the filter opening with it, a chorus and a big hall. Press after the note "
            "(aftertouch) to swell louder. MACROS: BRIGHT, SPACE, SWELL (a bigger opening).");
        strings.osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.85f).osc (0, "UniDetune", 0.28f).osc (0, "UniSpread", 1.0f)
               .osc (0, "Volume", 0.45f)
               .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.2f)
               .osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.6f).osc (1, "UniDetune", 0.2f).osc (1, "UniSpread", 0.8f)
               .filter (FilterType::lp12, 1300.0f, 0.05f, 0.2f).filterEnvelope (2000.0f, 3000.0f, 0.8f, 2500.0f)
               .set (ParamIDs::fltKey, 0.4f)
               .ampEnvelope (1800.0f, 2000.0f, 1.0f, 3000.0f)
               .set (ParamIDs::ampVolume, 0.85f);
        route (strings, "aftertouch", ParamIDs::ampVolume, 0.1f, 200.0f);
        route (strings, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (strings, "macro3", ParamIDs::fltEnv, 0.3f);
        macros (strings, "BRIGHT", "SPACE", "SWELL");
        chorus (strings, ParamIDs::modIds, MT::chorus, 0.4f, 0.4f, 0.35f);
        strings.set (ParamIDs::modStereo, 1.0f);
        reverb (strings, RT::hall, 0.85f, 0.3f, 40.0f, 4.0f);
        space (strings);
        strings.set (ParamIDs::patchLevel, -10.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (strings);

        auto fading = x.make ("Fading Light", 6, { "Pad", "Dark", "Modulation", "Drone" },
            "A dark, breathing pad like light leaving a room: soft triangles in a wide unison over a sine an octave "
            "down, the filter drifting slowly and unpredictably (LFO 2, the whole synth), a very slow swell in and a long "
            "fade, in a cathedral. MACROS: BRIGHT, SPACE, DRIFT (a wider unison).");
        fading.osc (0, "Wave", triangle).osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.15f)
              .osc (0, "UniSpread", 1.0f).osc (0, "Volume", 0.55f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
              .filter (FilterType::lp24, 900.0f, 0.15f, 0.0f)
              .ampEnvelope (2500.0f, 2000.0f, 1.0f, 4000.0f)
              .set (ParamIDs::lfo (1, "Rate"), 0.05f).set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::smoothRandom)
              .set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (fading, "lfo2", ParamIDs::fltCutoff, 0.15f);
        route (fading, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (fading, "macro3", ParamIDs::osc (0, "UniDetune"), 0.3f);
        macros (fading, "BRIGHT", "SPACE", "DRIFT");
        reverb (fading, RT::cathedral, 0.4f, 0.35f, 40.0f, 3.5f);
        space (fading);
        fading.set (ParamIDs::patchLevel, -10.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (fading);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto pedal = x.make ("Organ Pedal", 7, { "Bass", "Organ", "Wavetable", "Sub" },
            "The deep pedal notes of a pipe organ, felt more than heard: the Organ Stack table an octave down with its "
            "sub, a slow speaking attack, ringing into the cathedral. MACROS: BRIGHT, SPACE, RANKS (more upper ranks).");
        pedal.osc (0, "Wave", wavetable).osc (0, "Table", table ("Organ Stack")).osc (0, "WtIndex", 0.2f)
             .osc (0, "Transpose", -12.0f).osc (0, "Sub", 0.5f).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "SendReverb", 0.4f)
             .filter (FilterType::lp12, 800.0f, 0.05f, 0.0f)
             .ampEnvelope (150.0f, 1000.0f, 1.0f, 1500.0f);
        route (pedal, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (pedal, "macro3", ParamIDs::osc (0, "WtIndex"), 0.3f);
        macros (pedal, "BRIGHT", "SPACE", "RANKS");
        reverb (pedal, RT::cathedral, 0.3f, 0.25f, 40.0f, 3.0f);
        space (pedal);
        pedal.set (ParamIDs::patchLevel, 1.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pedal);

        auto well = x.make ("Gravity Well", 8, { "Bass", "Sub", "Swell", "Shaper" },
            "A deep sub that rises slowly out of nothing and sinks back: a sine with a faint triangle an octave up so it "
            "can be heard, warmed by SHAPER 1's tube, swelling in over a second and a half. MACROS: BRIGHT, SPACE, "
            "WARMTH.");
        well.osc (0, "Wave", sine).osc (0, "Volume", 0.85f).osc (0, "PhaseRand", 0.0f).osc (0, "SendReverb", 0.1f)
            .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
            .shaper (0, synth::ShaperModel::tube, 0.15f)
            .filter (FilterType::lp24, 600.0f, 0.05f, 0.0f)
            .ampEnvelope (1500.0f, 1000.0f, 1.0f, 2500.0f);
        route (well, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (well, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (well, "BRIGHT", "SPACE", "WARMTH");
        reverb (well, RT::hall, 0.7f, 0.2f);
        space (well);
        well.set (ParamIDs::patchLevel, -4.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (well);

        auto heart = x.make ("Heartbeat", 9, { "Bass", "Pulse", "Pitch", "Hit" },
            "A soft, low pulse like a heartbeat: a sine dropping a few semitones into each note in 60 ms, dying away in "
            "under half a second, through a dark low-pass, with a little hall. Play two notes close together for the "
            "beat. MACROS: BRIGHT, SPACE, THUMP (a bigger drop).");
        heart.osc (0, "Wave", sine).osc (0, "Volume", 0.9f).osc (0, "PhaseRand", 0.0f).osc (0, "SendReverb", 0.2f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.15f)
             .filter (FilterType::lp24, 300.0f, 0.05f, 0.2f).filterEnvelope (0.0f, 150.0f, 0.0f, 100.0f)
             .ampEnvelope (1.0f, 400.0f, 0.0f, 300.0f).set (ParamIDs::ampVel, 0.6f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 5.0f);
        heart.rig.modulation.pitchEnvelope = { { SP5 { 0.0f, 1.0f, -0.6f }, SP5 { 0.06f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (heart, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (heart, "macro3", ParamIDs::pitchEnvRange, 0.2f);
        macros (heart, "BRIGHT", "SPACE", "THUMP");
        reverb (heart, RT::hall, 0.5f, 0.15f);
        space (heart);
        heart.set (ParamIDs::patchLevel, -0.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (heart);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto memory = x.make ("Felt Memory", 10, { "Keys", "Piano", "Felt", "Cinematic" },
            "A soft, intimate felt piano for quiet, sad piano parts: a dark, rounded tone with the bright strike of "
            "the hammer dying away at once, the soft thump of felt on each note, strings a few cents apart, brighter "
            "and louder the harder you play, ringing on with the sustain pedal, in a small room. Made by synthesis, not "
            "recorded: an impression of a felt piano. The MOD WHEEL adds the keys' mechanical noise. MACROS: BRIGHT, "
            "SPACE, FELT (less felt: a clearer strike).");
        feltPiano (memory);
        macros (memory, "BRIGHT", "SPACE", "FELT");
        reverb (memory, RT::room, 0.6f, 0.2f, 15.0f, 4.0f);
        space (memory);
        memory.set (ParamIDs::patchLevel, -3.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (memory);

        auto distant = x.make ("Distant Felt", 11, { "Keys", "Piano", "Felt", "Lo-Fi" },
            "The felt piano far away and long ago: the same soft piano, its top worn down by an old cassette, heard "
            "through a vast hall with a long pre-delay and soft echoes. MACROS: BRIGHT, SPACE, WEAR (more cassette).");
        feltPiano (distant);
        distant.set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
               .set (ParamIDs::lofiAmount, 3.0f).set (ParamIDs::lofiMix, 0.45f).set (ParamIDs::lofiNoise, 0.0f);
        distant.rig.modulation.routes.erase (std::remove_if (distant.rig.modulation.routes.begin(), distant.rig.modulation.routes.end(),
                                                              [] (const ModRouteData& r) { return r.source == "macro3"; }),
                                              distant.rig.modulation.routes.end());
        route (distant, "macro3", ParamIDs::lofiAmount, 0.4f);
        macros (distant, "BRIGHT", "SPACE", "WEAR");
        delay (distant, "1/4 dotted", 0.3f, 0.1f, true, 3.5f);
        distant.set (ParamIDs::delayToReverb, 0.5f);
        reverb (distant, RT::hall, 0.85f, 0.4f, 80.0f, 3.5f);
        eqBand (distant, 0, eq::Shape::highCut, 6000.0f, 0.0f);
        space (distant);
        distant.set (ParamIDs::patchLevel, -1.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (distant);

        auto musicBox = x.make ("Music Box Elegy", 12, { "Keys", "FM", "Bell", "Delicate" },
            "A fragile music box: a sine with a quick FM chime from a sine three and a half times its pitch, a high "
            "glassy tine, a short ring that fades into a hall. Play it high. MACROS: BRIGHT (the chime), SPACE, TINE (the "
            "high tine).");
        musicBox.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
                .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.08f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 21.0f).osc (1, "Fine", 50.0f).osc (1, "Volume", 1.0f)
                .osc (1, "PhaseRand", 0.0f)
                .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 24.0f).osc (2, "Volume", 0.08f)
                .ampEnvelope (1.0f, 2500.0f, 0.0f, 1500.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f)
                .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        musicBox.rig.modulation.envelopes[0] = { { SP5 { 0.0f, 1.0f, -0.6f }, SP5 { 0.4f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (musicBox, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (musicBox, "macro1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (musicBox, "macro3", ParamIDs::osc (2, "Volume"), 0.12f);
        macros (musicBox, "BRIGHT", "SPACE", "TINE");
        delay (musicBox, "1/4", 0.3f, 0.12f, true, 5.0f);
        reverb (musicBox, RT::hall, 0.7f, 0.3f, 30.0f, 5.0f);
        space (musicBox);
        musicBox.set (ParamIDs::patchLevel, -2.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (musicBox);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto clock = x.make ("Ticking Clock", 13, { "Arp", "Piano", "Drums", "Sequencer" },
            "Time running out: the drum sequencer ticks a clock in eighth notes (the rim, tuned up and short, the tock "
            "softer), in time with your song, while ARP 1 plays the chord you hold as a quiet, repeating felt-piano "
            "figure. Hold a chord above C3 (the kit is on C2 to B2). MACROS: BRIGHT, SPACE, FELT.");
        feltPiano (clock);
        for (const auto i : { 0, 1, 2, 3 })
            clock.osc (i, "Arp", 1.0f);
        clock.set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
             .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.9f)
             .set (ParamIDs::drumsOn, 1.0f).set (ParamIDs::dseqOn, 1.0f)
             .set (ParamIDs::drum (1, "Pitch"), 7.0f).set (ParamIDs::drum (1, "Decay"), 0.15f).set (ParamIDs::drum (1, "Level"), -8.0f)
             .set (ParamIDs::drum (1, "SendReverb"), 0.3f);
        {
            auto& a = clock.rig.modulation.drumPatterns[0];
            for (int k = 0; k < 16; k += 2)
            {
                synth::drumseq::Note n;
                n.row = 1;
                n.start = k;
                n.velocity = k % 4 == 0 ? 0.8f : 0.5f;   // tick, tock
                n.pitch = k % 4 == 0 ? 0.0f : -3.0f;
                a.notes.push_back (n);
            }
        }
        macros (clock, "BRIGHT", "SPACE", "FELT");
        reverb (clock, RT::hall, 0.6f, 0.22f);
        space (clock);
        clock.set (ParamIDs::patchLevel, -3.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (clock);

        auto docking = x.make ("Docking", 14, { "Arp", "Organ", "Sequence", "Build" },
            "A minimalist organ figure that builds: hold a chord and ARP 1 climbs it in sixteenths over two octaves on "
            "the Organ Stack table, while LFO 1 slowly opens the filter over four bars and closes it again, in time "
            "with your song, into a cathedral. MACROS: BRIGHT, SPACE, RANKS.");
        docking.osc (0, "Wave", wavetable).osc (0, "Table", table ("Organ Stack")).osc (0, "WtIndex", 0.4f)
               .osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f)
               .filter (FilterType::lp12, 900.0f, 0.1f, 0.0f)
               .ampEnvelope (5.0f, 300.0f, 0.8f, 200.0f)
               .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle)
               .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
               .set (ParamIDs::lfo (0, "Division"), division ("4/1"))
               .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
               .set (ParamIDs::lfo (0, "Phase"), 0.75f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
               .set (ParamIDs::arp (0, "Gate"), 0.85f);
        route (docking, "lfo1", ParamIDs::fltCutoff, 0.3f);
        route (docking, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (docking, "macro3", ParamIDs::osc (0, "WtIndex"), 0.3f);
        macros (docking, "BRIGHT", "SPACE", "RANKS");
        reverb (docking, RT::cathedral, 0.35f, 0.3f, 30.0f, 4.0f);
        space (docking);
        docking.set (ParamIDs::patchLevel, 5.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (docking);

        auto tesseract = x.make ("Tesseract", 15, { "Arp", "Piano", "Felt", "Reverb" },
            "A slow felt-piano arpeggio folding in on itself: ARP 1 walks the chord you hold up and down two octaves in "
            "eighths, every note swelling back in through a reverb that plays its tail backwards, with soft dotted "
            "echoes. MACROS: BRIGHT, SPACE, FELT.");
        feltPiano (tesseract);
        for (const auto i : { 0, 1, 2, 3 })
            tesseract.osc (i, "Arp", 1.0f);
        tesseract.set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                 .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
                 .set (ParamIDs::arp (0, "Gate"), 1.0f);
        macros (tesseract, "BRIGHT", "SPACE", "FELT");
        delay (tesseract, "1/4 dotted", 0.3f, 0.12f, true, 4.0f);
        reverb (tesseract, RT::reverse, 0.7f, 0.35f);
        space (tesseract);
        tesseract.set (ParamIDs::patchLevel, -3.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (tesseract);
    }

    // SIMULACRUM (the owner's name): inspired by the score and soundtrack of a 1999 science-fiction film about a
    // simulated world: a dark, atonal orchestra (brass in clashing keys, stabbing strings), late-'90s industrial and
    // big-beat (distorted basses, chopped breakbeats), fast trance arpeggios, falling digital rain, time slowing down.
    // No title, characters, places, phrases or music from the film: our own sounds.
    void simulacrum (Expansion x)
    {
        using SP6 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;
        using GM = effects::Glitch::Mode;
        constexpr int fet = 2;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto chase = x.make ("Rooftop Chase", 1, { "Lead", "Sync", "Drive", "Mono" },
            "A screaming chase lead: a saw hard-synced, its sync sweeping down as each note starts, with a square an "
            "octave down, through insert bus A's BANSHEE drive, on a short glide. The MOD WHEEL pushes the sync further. "
            "MACROS: BRIGHT, SPACE, SCREAM (more sync).");
        chase.osc (0, "SyncAmount", 0.3f).osc (0, "SyncEnv", 0.35f).osc (0, "SyncEnvTime", 300.0f).osc (0, "Volume", 0.6f)
             .osc (0, "Insert", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
             .osc (1, "Insert", 1.0f)
             .filter (FilterType::lp24, 3500.0f, 0.15f, 0.0f)
             .ampEnvelope (2.0f, 300.0f, 0.9f, 200.0f)
             .mono (40.0f);
        drive (chase, 0, DM::screamer, 3.5f, 3.0f, 0.0f, 0.35f, 0.25f);
        eqBand (chase, 0, eq::Shape::highShelf, 5000.0f, -4.0f);
        route (chase, "modWheel", ParamIDs::osc (0, "SyncAmount"), 0.3f);
        route (chase, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (chase, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.35f);
        macros (chase, "BRIGHT", "SPACE", "SCREAM");
        delay (chase, "1/8 dotted", 0.3f, 0.15f, true, 5.0f);
        reverb (chase, RT::room, 0.5f, 0.12f);
        space (chase);
        chase.set (ParamIDs::patchLevel, 4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chase);

        auto phantom = x.make ("Phantom Line", 2, { "Lead", "Pulse", "Delay", "Mono" },
            "A cold, hollow lead: a square with a thin pulse a fifth up, a slow vibrato that comes in late, and long "
            "ping-pong echoes ringing off into a plate. MACROS: BRIGHT, SPACE, ECHO (longer echoes).");
        phantom.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.6f)
               .osc (0, "VibAmount", 10.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 700.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.15f).osc (1, "Transpose", 7.0f).osc (1, "Volume", 0.12f)
               .filter (FilterType::lp12, 2500.0f, 0.1f, 0.0f)
               .ampEnvelope (20.0f, 500.0f, 0.85f, 500.0f)
               .mono (0.0f);
        route (phantom, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (phantom, "macro3", ParamIDs::delayFeedback, 0.25f);
        macros (phantom, "BRIGHT", "SPACE", "ECHO");
        delay (phantom, "1/8 dotted", 0.45f, 0.25f, true, 4.0f);
        reverb (phantom, RT::plate, 0.6f, 0.18f);
        space (phantom);
        phantom.set (ParamIDs::patchLevel, -5.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (phantom);

        auto slow = x.make ("Slow Time", 3, { "Lead", "Pitch", "Reverse", "Mono" },
            "Time slowing down: every note sags about a semitone over a second and a half (the PITCH envelope), drawn "
            "out by a slow attack and a long release, smeared through a reverb that plays backwards. Two triangles a "
            "hair apart. The MOD WHEEL makes it sag further. MACROS: BRIGHT, SPACE, SAG.");
        slow.osc (0, "Wave", triangle).osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.12f)
            .osc (0, "UniSpread", 0.6f).osc (0, "Volume", 0.7f)
            .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Volume", 0.12f)
            .filter (FilterType::lp12, 2200.0f, 0.05f, 0.0f)
            .ampEnvelope (150.0f, 800.0f, 0.9f, 1500.0f)
            .mono (200.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        slow.rig.modulation.pitchEnvelope = { { SP6 { 0.0f, 0.5f, 0.3f }, SP6 { 1.5f, 0.42f, 0.0f } }, -1, -1, -1 };
        route (slow, "modWheel", ParamIDs::pitchEnvRange, 0.15f);
        route (slow, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (slow, "macro3", ParamIDs::pitchEnvRange, 0.15f);
        macros (slow, "BRIGHT", "SPACE", "SAG");
        reverb (slow, RT::reverse, 0.6f, 0.3f);
        space (slow);
        slow.set (ParamIDs::patchLevel, -3.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (slow);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto rain = x.make ("Green Rain", 4, { "Pad", "Wavetable", "Travel", "Sparkle" },
            "Digital rain: the Harmonic Rise table cascading downward over and over (TRAVEL, free-running, a fall every "
            "two seconds) in a wide unison, with glittering high sines flickering at random through it (LFO 1, S&H, the "
            "whole synth) into sixteenth-note echoes. MACROS: BRIGHT, SPACE, GLITTER (more sparkle).");
        rain.osc (0, "Wave", wavetable).osc (0, "Table", table ("Harmonic Rise")).osc (0, "WtIndex", 0.0f)
            .osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.9f)
            .osc (0, "Volume", 0.5f)
            .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::loop)
            .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrRate", 0.5f).osc (0, "TrStrength", 0.8f)
            .osc (0, "TrSmooth", 0.15f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.0f)
            .osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.7f).osc (1, "UniDetune", 0.2f).osc (1, "UniSpread", 1.0f)
            .filter (FilterType::lp12, 5000.0f, 0.05f, 0.0f)
            .ampEnvelope (700.0f, 1500.0f, 0.95f, 2000.0f)
            .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sampleHold).set (ParamIDs::lfo (0, "Rate"), 9.0f)
            .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        rain.rig.modulation.travel[0].envelope = { { SP6 { 0.0f, 1.0f, 0.3f }, SP6 { 1.0f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (rain, "lfo1", ParamIDs::osc (1, "Volume"), 0.1f);
        route (rain, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (rain, "macro3", ParamIDs::osc (1, "Volume"), 0.12f);
        macros (rain, "BRIGHT", "SPACE", "GLITTER");
        delay (rain, "1/16", 0.4f, 0.2f, true, 6.0f);
        reverb (rain, RT::hall, 0.7f, 0.25f);
        space (rain);
        rain.set (ParamIDs::patchLevel, -6.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (rain);

        auto cluster = x.make ("Brass Cluster", 5, { "Pad", "Brass", "Unison", "Swell" },
            "A dark orchestral brass swell in two keys at once: five saws swelling in through a warm filter, with "
            "softer saws a tritone (OSC 2) and a minor ninth (OSC 3) above each note you play, so every chord "
            "clashes. Slightly gritty (SHAPER 1). MACROS: BRIGHT, SPACE, CLASH (louder clashing notes).");
        cluster.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.55f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.7f)
               .osc (0, "Volume", 0.5f)
               .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", 6.0f).osc (1, "Volume", 0.18f)
               .osc (1, "UniVoices", 2.0f).osc (1, "UniMix", 0.5f).osc (1, "UniDetune", 0.1f).osc (1, "UniSpread", 0.7f)
               .osc (2, "On", 1.0f).osc (2, "Wave", saw).osc (2, "Transpose", 13.0f).osc (2, "Volume", 0.14f)
               .osc (2, "UniVoices", 2.0f).osc (2, "UniMix", 0.5f).osc (2, "UniDetune", 0.1f).osc (2, "UniSpread", 0.7f)
               .shaper (0, synth::ShaperModel::soft, 0.15f)
               .filter (FilterType::lp24, 600.0f, 0.15f, 0.3f).filterEnvelope (900.0f, 1500.0f, 0.6f, 900.0f)
               .set (ParamIDs::fltKey, 0.4f).set (ParamIDs::fltVel, 0.4f)
               .ampEnvelope (600.0f, 1000.0f, 0.9f, 1200.0f);
        route (cluster, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (cluster, "macro3", ParamIDs::osc (1, "Volume"), 0.15f);
        route (cluster, "macro3", ParamIDs::osc (2, "Volume"), 0.15f);
        macros (cluster, "BRIGHT", "SPACE", "CLASH");
        reverb (cluster, RT::hall, 0.8f, 0.3f, 30.0f, 4.0f);
        space (cluster);
        cluster.set (ParamIDs::patchLevel, -9.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (cluster);

        auto field = x.make ("Endless Field", 6, { "Pad", "Drone", "Step Seq", "Wavetable" },
            "A cold, endless dark drone: the Comb Sweep table drifting slowly (TRAVEL, free-running, ping-pong) over a "
            "sine an octave down and a breath of dark noise, pumping slowly like machinery (the STEP SEQUENCER, half a "
            "bar in and out, in time with your song), in a cathedral. MACROS: BRIGHT, SPACE, PUMP (deeper pumping).");
        field.osc (0, "Wave", wavetable).osc (0, "Table", table ("Comb Sweep")).osc (0, "WtIndex", 0.2f)
             .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.9f)
             .osc (0, "Volume", 0.5f).osc (0, "Noise", 0.05f).osc (0, "NoiseColour", 0.8f).osc (0, "NoiseWidth", 2.0f)
             .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::pingPong)
             .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrRate", 0.07f).osc (0, "TrStrength", 0.5f)
             .osc (0, "TrSmooth", 0.3f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
             .filter (FilterType::lp24, 1200.0f, 0.15f, 0.0f)
             .ampEnvelope (1500.0f, 2000.0f, 1.0f, 3000.0f)
             .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqMode, (float) synth::seq::Mode::mono)
             .set (ParamIDs::seqVolMix, 0.45f).set (ParamIDs::seqGate, 0.5f).set (ParamIDs::seqContour, 0.5f)
             .set (ParamIDs::seqDivision, division ("1/4")).set (ParamIDs::seqLength, 4.0f);
        field.rig.modulation.travel[0].envelope = { { SP6 { 0.0f, 0.0f, 0.0f }, SP6 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = field.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = k % 4 < 2;
                step.level = 1.0f;
            }
        route (field, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (field, "macro3", ParamIDs::seqVolMix, 0.4f);
        macros (field, "BRIGHT", "SPACE", "PUMP");
        reverb (field, RT::cathedral, 0.5f, 0.35f, 40.0f, 3.5f);
        space (field);
        field.set (ParamIDs::patchLevel, -4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (field);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto pulseBass = x.make ("Industrial Pulse", 7, { "Bass", "Arp", "Drive", "Inserts" },
            "A distorted industrial bass chugging in sixteenths with your song: hold a note and ARP 1 repeats it, every "
            "fourth one accented and every note snapping shut (the FILTER envelope), a saw and a square an octave down "
            "through insert bus A's RABID and a FET compressor. MACROS: BRIGHT, SPACE, DESTROY (SHAPER 1's tube into the drive).");
        pulseBass.osc (0, "Volume", 0.55f).osc (0, "Insert", 1.0f).osc (0, "Arp", 1.0f).osc (0, "SendReverb", 0.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f)
                 .osc (1, "Volume", 0.45f).osc (1, "Insert", 1.0f).osc (1, "Arp", 1.0f).osc (1, "SendReverb", 0.0f)
                 .shaper (0, synth::ShaperModel::tube, 0.05f)
                 .filter (FilterType::lp24, 350.0f, 0.25f, 0.4f).filterEnvelope (0.0f, 110.0f, 0.0f, 80.0f)
                 .set (ParamIDs::fltVel, 0.5f)
                 .ampEnvelope (1.0f, 70.0f, 0.0f, 30.0f)   // each hit dies inside its sixteenth: the drive after it would flatten a longer one
                 .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
                 .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
                 .set (ParamIDs::arp (0, "Gate"), 0.6f);
        {
            auto& p = pulseBass.rig.modulation.arps[0].patterns[0];
            for (int k = 0; k < 16; ++k)
                p.blocks[(size_t) k].velocity = k % 4 == 0 ? 1.0f : 0.6f;
        }
        drive (pulseBass, 0, DM::rabid, 5.0f, 5.0f, 0.0f, 0.0f, 0.1f);
        compressor (pulseBass, fet, -18.0f, 4.0f, 2.0f);
        route (pulseBass, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (pulseBass, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (pulseBass, "BRIGHT", "SPACE", "DESTROY");
        reverb (pulseBass, RT::room, 0.4f, 0.08f);
        space (pulseBass);
        pulseBass.set (ParamIDs::patchLevel, 7.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pulseBass);

        auto breakBass = x.make ("Breakbeat Bass", 8, { "Bass", "Unison", "LFO", "Mod Wheel" },
            "A growling big-beat bass: two saws in a slowly beating unison over a sine sub, through a dark filter that "
            "LFO 1 wobbles in eighths, in time with your song. Push the MOD WHEEL to open the filter into the wobble. "
            "MACROS: BRIGHT, SPACE, GROWL (SHAPER 1's grit).");
        breakBass.osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.18f).osc (0, "UniSpread", 0.3f)
                 .osc (0, "Volume", 0.55f).osc (0, "SendReverb", 0.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.5f)
                 .osc (1, "Filter", 0.0f).osc (1, "SendReverb", 0.0f)
                 .shaper (0, synth::ShaperModel::tube, 0.2f)
                 .filter (FilterType::lp24, 280.0f, 0.3f, 0.2f)
                 .ampEnvelope (2.0f, 400.0f, 0.9f, 120.0f)
                 .mono (15.0f)
                 .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sine)
                 .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
                 .set (ParamIDs::lfo (0, "Division"), division ("1/8"))
                 .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (breakBass, "lfo1", ParamIDs::fltCutoff, 0.1f);
        route (breakBass, "modWheel", ParamIDs::fltCutoff, 0.25f, 100.0f);
        route (breakBass, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (breakBass, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (breakBass, "BRIGHT", "SPACE", "GROWL");
        reverb (breakBass, RT::room, 0.4f, 0.08f);
        space (breakBass);
        breakBass.set (ParamIDs::patchLevel, -5.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (breakBass);

        auto mainframe = x.make ("Mainframe Sub", 9, { "Bass", "Sub", "Glitch", "Inserts" },
            "A deep, clean sub that stutters like a failing machine: a sine with a quiet triangle so it's heard on small "
            "speakers, through insert bus A's GLITCH chopping and repeating sixteenths in time with your song. MACROS: "
            "BRIGHT, SPACE, MALFUNCTION (the LO-FI bitcrusher brought in).");
        mainframe.osc (0, "Wave", sine).osc (0, "Volume", 0.85f).osc (0, "PhaseRand", 0.0f).osc (0, "Insert", 1.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.2f).osc (1, "Insert", 1.0f)
                 .filter (FilterType::lp24, 900.0f, 0.05f, 0.0f)
                 .ampEnvelope (3.0f, 600.0f, 0.9f, 150.0f)
                 .mono (0.0f);
        glitch (mainframe, 0, GM::sequence, "1/16", 3.0f, 3.0f, 0.4f);
        mainframe.set (ParamIDs::ins (0, "SendDelay"), 0.0f).set (ParamIDs::ins (0, "SendReverb"), 0.05f);
        route (mainframe, "macro1", ParamIDs::fltCutoff, 0.25f);
        mainframe.set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::bitcrusher)
                 .set (ParamIDs::lofiAmount, 5.0f).set (ParamIDs::lofiMix, 0.0f).set (ParamIDs::lofiNoise, 0.0f);
        route (mainframe, "macro3", ParamIDs::lofiMix, 0.6f);
        macros (mainframe, "BRIGHT", "SPACE", "MALFUNCTION");
        reverb (mainframe, RT::room, 0.3f, 0.05f);
        space (mainframe);
        mainframe.set (ParamIDs::patchLevel, -3.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (mainframe);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto terminal = x.make ("Terminal", 10, { "Keys", "FM", "Bell", "Digital" },
            "Cold digital chimes: a sine frequency-modulated by a shape at a harmonic ratio, the modulation dying away "
            "in a third of a second (ENV 1), so each note strikes bright and settles to a glassy tone, with a quick "
            "decay and ping-pong echoes. MACROS: BRIGHT (more FM), SPACE, RATIO (a stranger ratio).");
        terminal.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
                .osc (0, "FmAmount", 0.12f).osc (0, "ModGrid", (float) synth::ModGrid::harmonic).osc (0, "ModRate", 0.7f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
                .ampEnvelope (1.0f, 1200.0f, 0.0f, 600.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f)
                .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        terminal.rig.modulation.envelopes[0] = { { SP6 { 0.0f, 1.0f, -0.6f }, SP6 { 0.33f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (terminal, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (terminal, "macro1", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (terminal, "macro3", ParamIDs::osc (0, "ModRate"), 0.2f);
        macros (terminal, "BRIGHT", "SPACE", "RATIO");
        delay (terminal, "1/8", 0.3f, 0.15f, true, 6.0f);
        reverb (terminal, RT::plate, 0.5f, 0.15f);
        space (terminal);
        terminal.set (ParamIDs::patchLevel, -4.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (terminal);

        auto glass = x.make ("Glass Code", 11, { "Keys", "Crush", "Lo-Fi", "Pluck" },
            "Brittle glitch keys: a triangle and a pulse an octave up, bit-crushed (CRUSH) and run through the LO-FI "
            "bitcrusher, in short plucks that ring into a small room. MACROS: BRIGHT, SPACE, CRUSH (more crush).");
        glass.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "Crush", 0.25f).osc (0, "CrushMode", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
             .filter (FilterType::lp24, 3000.0f, 0.15f, 0.3f).filterEnvelope (0.0f, 300.0f, 0.2f, 300.0f)
             .set (ParamIDs::fltVel, 0.4f)
             .ampEnvelope (1.0f, 700.0f, 0.15f, 400.0f)
             .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::bitcrusher)
             .set (ParamIDs::lofiAmount, 3.0f).set (ParamIDs::lofiMix, 0.4f).set (ParamIDs::lofiNoise, 0.0f);
        route (glass, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (glass, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        macros (glass, "BRIGHT", "SPACE", "CRUSH");
        delay (glass, "1/16 dotted", 0.25f, 0.12f, true, 6.0f);
        reverb (glass, RT::room, 0.5f, 0.15f);
        space (glass);
        glass.set (ParamIDs::patchLevel, -4.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glass);

        auto warped = x.make ("Warped Keys", 12, { "Keys", "Shape", "Mod Wheel", "Electric" },
            "Electric-piano-like keys that bend out of shape: a sine with a little FM bark over a soft triangle, and the "
            "MOD WHEEL bends the wave itself (SKEW and PINCH) and drags the pitch down a little, as if the world were "
            "melting. A slow chorus. MACROS: BRIGHT, SPACE, WARP (bent without the wheel).");
        warped.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
              .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.08f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
              .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Volume", 0.3f)
              .filter (FilterType::lp12, 3000.0f, 0.05f, 0.25f).filterEnvelope (0.0f, 500.0f, 0.2f, 300.0f)
              .set (ParamIDs::fltVel, 0.5f)
              .ampEnvelope (2.0f, 2500.0f, 0.3f, 500.0f).set (ParamIDs::ampVel, 0.6f);
        route (warped, "modWheel", ParamIDs::osc (0, "Skew"), 0.5f, 80.0f);
        route (warped, "modWheel", ParamIDs::osc (2, "Pinch"), 0.5f, 80.0f);
        route (warped, "modWheel", ParamIDs::osc (0, "Fine"), -0.15f, 80.0f);
        route (warped, "modWheel", ParamIDs::osc (2, "Fine"), -0.15f, 80.0f);
        route (warped, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (warped, "macro3", ParamIDs::osc (0, "Skew"), 0.4f);
        route (warped, "macro3", ParamIDs::osc (2, "Pinch"), 0.4f);
        macros (warped, "BRIGHT", "SPACE", "WARP");
        chorus (warped, ParamIDs::modIds, MT::chorus, 0.3f, 0.4f, 0.4f);
        reverb (warped, RT::room, 0.6f, 0.15f);
        space (warped);
        warped.set (ParamIDs::patchLevel, -12.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (warped);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto breach = x.make ("System Breach", 13, { "Arp", "Drums", "Drive", "Sequencer" },
            "A big-beat breakbeat with a driven bass riff: the drum sequencer plays a chopped break (kick, snare with "
            "ghost notes, hats, an open hat) in time with your song, while ARP 1 plays the notes you hold as a riff in "
            "sixteenths through insert bus A's RABID. Hold notes above C3 (the kit is on C2 to B2). MACROS: BRIGHT, "
            "SPACE, DESTROY (SHAPER 1's tube into the drive).");
        breach.osc (0, "Volume", 0.55f).osc (0, "Insert", 1.0f).osc (0, "Arp", 1.0f).osc (0, "Transpose", -12.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -24.0f)
              .osc (1, "Volume", 0.35f).osc (1, "Insert", 1.0f).osc (1, "Arp", 1.0f)
              .shaper (0, synth::ShaperModel::tube, 0.05f)
              .filter (FilterType::lp24, 500.0f, 0.25f, 0.4f).filterEnvelope (0.0f, 140.0f, 0.1f, 80.0f)
              .ampEnvelope (1.0f, 80.0f, 0.0f, 30.0f)   // each hit dies inside its sixteenth (the drive comes after)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.55f)
              .set (ParamIDs::drumsOn, 1.0f).set (ParamIDs::dseqOn, 1.0f)
              .set (ParamIDs::drum (0, "Decay"), 0.45f).set (ParamIDs::drum (0, "SendReverb"), 0.0f).set (ParamIDs::drum (0, "Level"), -3.0f)
              .set (ParamIDs::drum (2, "Tone"), 0.6f).set (ParamIDs::drum (2, "SendReverb"), 0.25f)
              .set (ParamIDs::drum (6, "Level"), -5.0f).set (ParamIDs::drum (6, "Pan"), 0.15f)
              .set (ParamIDs::drum (10, "Level"), -6.0f).set (ParamIDs::drum (10, "Pan"), 0.15f);
        {
            auto& a = breach.rig.modulation.drumPatterns[0];
            const auto note = [&a] (int row, double start, float velocity)
            {
                synth::drumseq::Note n;
                n.row = row;
                n.start = start;
                n.velocity = velocity;
                a.notes.push_back (n);
            };
            constexpr int kick = 0, snare = 2, hat = 6, open = 10;
            for (const auto k : { 0.0, 3.0, 10.0, 11.0 })
                note (kick, k, k == 0.0 || k == 10.0 ? 1.0f : 0.8f);
            note (snare, 4.0, 1.0f);
            note (snare, 12.0, 1.0f);
            note (snare, 7.0, 0.35f);   // ghost notes
            note (snare, 9.0, 0.4f);
            note (snare, 15.0, 0.35f);
            for (int k = 0; k < 16; k += 2)
                if (k != 14)
                    note (hat, k, k % 4 == 0 ? 0.8f : 0.55f);
            note (open, 14.0, 0.7f);
        }
        drive (breach, 0, DM::rabid, 5.0f, 5.0f, 0.0f, 0.0f, 0.1f);
        compressor (breach, fet, -18.0f, 4.0f, 2.0f);
        route (breach, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (breach, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (breach, "BRIGHT", "SPACE", "DESTROY");
        reverb (breach, RT::room, 0.5f, 0.1f);
        space (breach);
        breach.set (ParamIDs::patchLevel, 4.0f);   // LEVEL: set by its peaks (the drums), -3 dBFS (measured, not by ear)
        x.add (breach);

        auto cascade = x.make ("Rain Cascade", 14, { "Arp", "Randomizer", "Phrygian", "Glitch" },
            "Fast falling runs: ARP 1 runs down three octaves in sixteenths, its randomizer picking notes from the "
            "Phrygian scale (the dark flat second) with rests and ratchet bursts, a new pattern every pass. Short "
            "pulse plucks through insert bus A's GLITCH and dotted echoes. Hold one key: its scale's root. MACROS: "
            "BRIGHT, SPACE, CRUSH (bit-crushed plucks).");
        cascade.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f)
               .osc (1, "Arp", 1.0f).osc (1, "Insert", 1.0f)
               .filter (FilterType::lp24, 3500.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 120.0f, 0.0f, 100.0f)
               .ampEnvelope (0.5f, 160.0f, 0.0f, 100.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::down)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 3.0f)
               .set (ParamIDs::arp (0, "Gate"), 0.45f);
        {
            auto& r = cascade.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Phrygian");
            r.restChance = 0.2f;
            r.ratchetChance = 0.2f;
            r.lengthChances = { 1.0f, 0.1f, 0.0f, 0.0f };
            r.velocityLow = 0.6f;
            r.autoDice = true;
            r.seed = 23;
            auto& p = cascade.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        glitch (cascade, 0, GM::sequence, "1/32", 3.0f, 3.0f, 0.2f);
        cascade.set (ParamIDs::ins (0, "SendDelay"), 0.5f).set (ParamIDs::ins (0, "SendReverb"), 0.3f);
        route (cascade, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (cascade, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        macros (cascade, "BRIGHT", "SPACE", "CRUSH");
        delay (cascade, "1/16 dotted", 0.35f, 0.2f, true, 6.0f);
        reverb (cascade, RT::hall, 0.5f, 0.15f);
        space (cascade);
        cascade.set (ParamIDs::patchLevel, 1.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (cascade);

        auto firefight = x.make ("Firefight", 15, { "Arp", "Trance", "LFO", "Build" },
            "A relentless trance arpeggio: ARP 1 runs up and down the chord you hold over two octaves in sixteenths, "
            "on three detuned saws with a snappy filter, while LFO 1 opens the filter over four bars and closes it "
            "again, in time with your song. A phaser-like sweep (MOD: AEON) and ping-pong echoes. MACROS: BRIGHT, "
            "SPACE, BITE (more resonance).");
        firefight.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.6f)
                 .osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
                 .filter (FilterType::lp24, 700.0f, 0.35f, 0.3f).filterEnvelope (0.0f, 130.0f, 0.1f, 100.0f)
                 .ampEnvelope (1.0f, 200.0f, 0.5f, 100.0f)
                 .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle)
                 .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
                 .set (ParamIDs::lfo (0, "Division"), division ("4/1"))
                 .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
                 .set (ParamIDs::lfo (0, "Phase"), 0.75f)
                 .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                 .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
                 .set (ParamIDs::arp (0, "Gate"), 0.6f);
        route (firefight, "lfo1", ParamIDs::fltCutoff, 0.3f);
        route (firefight, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (firefight, "macro3", ParamIDs::fltRes, 0.3f);
        macros (firefight, "BRIGHT", "SPACE", "BITE");
        chorus (firefight, ParamIDs::modIds, MT::flanger, 0.3f, 0.4f, 0.3f);
        delay (firefight, "1/8 dotted", 0.3f, 0.15f, true, 5.0f);
        reverb (firefight, RT::hall, 0.5f, 0.12f);
        space (firefight);
        firefight.set (ParamIDs::patchLevel, -1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (firefight);
    }

    // DREAMSTATE (the owner's name): inspired by the score of a 2010 film about dreams within dreams: a huge, low
    // brass blast, time slowed to a crawl and running at different speeds in each layer, relentless string ostinatos
    // over slow, heavy minor chords, a twangy reverb-soaked guitar, sparse piano, deep swells and ticking clocks. No
    // title, characters, places, terms or music from the film (nor the song its score slows down): our own sounds.
    void dreamstate (Expansion x)
    {
        using SP7 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto twang = x.make ("Twang Guitar", 1, { "Lead", "Pluck", "Tremolo", "Reverb" },
            "A twangy baritone-guitar-like pluck: a saw and a narrow pulse snapping open and dying away (the FILTER "
            "envelope), a slow tremolo (MOD: tremolo), dotted echoes and a spring reverb. Harder playing twangs more. "
            "MACROS: BRIGHT, SPACE, TWANG (a sharper snap).");
        twang.osc (0, "Volume", 0.55f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.2f).osc (1, "Fine", 4.0f).osc (1, "Volume", 0.3f)
             .filter (FilterType::lp24, 700.0f, 0.25f, 0.5f).filterEnvelope (0.0f, 350.0f, 0.15f, 300.0f)
             .set (ParamIDs::fltKey, 0.5f).set (ParamIDs::fltVel, 0.5f).set (ParamIDs::fltEnvVel, 0.6f)
             .ampEnvelope (1.0f, 1800.0f, 0.15f, 600.0f).set (ParamIDs::ampDecayCurve, -0.4f).set (ParamIDs::ampVel, 0.5f);
        route (twang, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (twang, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (twang, "BRIGHT", "SPACE", "TWANG");
        chorus (twang, ParamIDs::modIds, MT::tremolo, 0.25f, 0.4f, 1.0f);
        delay (twang, "1/8 dotted", 0.3f, 0.15f, false, 4.0f);
        reverb (twang, RT::spring, 0.6f, 0.3f);
        space (twang);
        twang.set (ParamIDs::patchLevel, 1.7f);   // LEVEL: set by its peaks, -3 dBFS (measured, not by ear)
        x.add (twang);

        auto horn = x.make ("Fading Horn", 2, { "Lead", "Brass", "Aftertouch", "Mono" },
            "A soft, noble horn: a saw and a pulse through a warm filter that swells open over a third of a second, "
            "rounded by SHAPER 1, vibrato coming in late, in a big hall. Press into the keys after playing "
            "(aftertouch) for more brass. MACROS: BRIGHT, SPACE, BRASS (a brassier swell).");
        horn.osc (0, "Volume", 0.5f).osc (0, "VibAmount", 12.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 600.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Fine", -5.0f).osc (1, "Volume", 0.35f)
            .osc (1, "VibAmount", 12.0f).osc (1, "VibRate", 5.0f).osc (1, "VibFade", 600.0f)
            .shaper (0, synth::ShaperModel::soft, 0.1f)
            .filter (FilterType::lp24, 600.0f, 0.1f, 0.3f).filterEnvelope (300.0f, 600.0f, 0.6f, 500.0f)
            .set (ParamIDs::fltKey, 0.4f)
            .ampEnvelope (120.0f, 600.0f, 0.9f, 600.0f)
            .mono (60.0f);
        route (horn, "aftertouch", ParamIDs::fltCutoff, 0.15f, 120.0f);
        route (horn, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (horn, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (horn, "BRIGHT", "SPACE", "BRASS");
        reverb (horn, RT::hall, 0.8f, 0.3f, 30.0f, 4.0f);
        space (horn);
        horn.set (ParamIDs::patchLevel, -5.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (horn);

        auto weightless = x.make ("Weightless", 3, { "Lead", "Formant", "Glide", "Breath" },
            "A floating, breathy lead: a pulse pushed through FORMANT so it sings like a hollow voice, a breath of "
            "noise, gliding slowly between notes as if weightless, into a cathedral. MACROS: BRIGHT, SPACE, VOICE (the "
            "formant moved).");
        weightless.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Formant", 0.3f).osc (0, "FormantMode", 2.0f)
                  .osc (0, "Volume", 0.6f).osc (0, "Noise", 0.012f).osc (0, "NoiseColour", 0.15f).osc (0, "NoiseWidth", 2.0f)
                  .osc (0, "VibAmount", 8.0f).osc (0, "VibRate", 4.5f).osc (0, "VibFade", 800.0f)
                  .filter (FilterType::lp12, 2500.0f, 0.1f, 0.0f)
                  .ampEnvelope (150.0f, 600.0f, 0.9f, 1200.0f)
                  .mono (350.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.5f);
        route (weightless, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (weightless, "macro3", ParamIDs::osc (0, "Formant"), -0.25f);
        macros (weightless, "BRIGHT", "SPACE", "VOICE");
        delay (weightless, "1/4", 0.3f, 0.12f, true, 4.0f);
        reverb (weightless, RT::cathedral, 0.5f, 0.3f, 50.0f, 4.0f);
        eqBand (weightless, 0, eq::Shape::lowCut, 180.0f, 0.0f);   // the breath's low end: rumble, not breath
        space (weightless);
        weightless.set (ParamIDs::patchLevel, 8.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (weightless);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto folding = x.make ("Folding City", 4, { "Pad", "Wavetable", "Mod Wheel", "Pitch" },
            "A pad that folds in on itself: the Wavefold table in a wide unison over a sine an octave down. Push the "
            "MOD WHEEL and the world bends: the fold deepens and the upper layer slowly drops an octave, as if a street "
            "were curling overhead. MACROS: BRIGHT, SPACE, FOLD (folded without the wheel).");
        folding.osc (0, "Wave", wavetable).osc (0, "Table", table ("Wavefold")).osc (0, "WtIndex", 0.1f)
               .osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.9f)
               .osc (0, "Volume", 0.5f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
               .filter (FilterType::lp12, 3000.0f, 0.05f, 0.0f)
               .ampEnvelope (900.0f, 1500.0f, 0.95f, 2000.0f);
        route (folding, "modWheel", ParamIDs::osc (0, "WtIndex"), 0.5f, 300.0f);
        route (folding, "modWheel", ParamIDs::osc (0, "Transpose"), -0.125f, 1500.0f);
        route (folding, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (folding, "macro3", ParamIDs::osc (0, "WtIndex"), 0.4f);
        macros (folding, "BRIGHT", "SPACE", "FOLD");
        reverb (folding, RT::hall, 0.8f, 0.3f, 40.0f, 4.0f);
        space (folding);
        folding.set (ParamIDs::patchLevel, -11.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (folding);

        auto dilation = x.make ("Time Dilation", 5, { "Pad", "Pitch", "Reverse", "Slow" },
            "Time slowed to a crawl: a soft unison of saws and triangles swelling in over three seconds, sinking a "
            "little in pitch as it holds (the PITCH envelope, four seconds), a slow, wide vibrato, and a reverb that "
            "plays backwards, so every chord breathes in before it arrives. MACROS: BRIGHT, SPACE, DRAG (sinks further).");
        dilation.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 1.0f)
                .osc (0, "Volume", 0.35f).osc (0, "VibAmount", 10.0f).osc (0, "VibRate", 0.6f)
                .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.3f)
                .osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.6f).osc (1, "UniDetune", 0.1f).osc (1, "UniSpread", 0.8f)
                .filter (FilterType::lp24, 1400.0f, 0.1f, 0.0f)
                .ampEnvelope (3000.0f, 2000.0f, 1.0f, 3500.0f)
                .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        dilation.rig.modulation.pitchEnvelope = { { SP7 { 0.0f, 0.5f, 0.2f }, SP7 { 4.0f, 0.45f, 0.0f } }, -1, -1, -1 };
        route (dilation, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (dilation, "macro3", ParamIDs::pitchEnvRange, 0.15f);
        macros (dilation, "BRIGHT", "SPACE", "DRAG");
        reverb (dilation, RT::reverse, 0.7f, 0.35f);
        space (dilation);
        dilation.set (ParamIDs::patchLevel, -6.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (dilation);

        auto heavy = x.make ("Heavy Chords", 6, { "Pad", "Brass", "Strings", "Unison" },
            "Dark, massive brass and strings for slow minor chords: five wide saws and a pulse swelling through a "
            "warm filter, a saw an octave down for weight, a string-ensemble chorus and a big hall. MACROS: BRIGHT, "
            "SPACE, WEIGHT (more of the low octave).");
        heavy.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.9f)
             .osc (0, "Volume", 0.45f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Volume", 0.2f)
             .osc (2, "On", 1.0f).osc (2, "Wave", saw).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.25f)
             .filter (FilterType::lp24, 700.0f, 0.15f, 0.2f).filterEnvelope (800.0f, 2000.0f, 0.6f, 1200.0f)
             .set (ParamIDs::fltKey, 0.4f).set (ParamIDs::fltVel, 0.4f)
             .ampEnvelope (400.0f, 1500.0f, 0.9f, 1500.0f);
        route (heavy, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (heavy, "macro3", ParamIDs::osc (2, "Volume"), 0.2f);
        macros (heavy, "BRIGHT", "SPACE", "WEIGHT");
        chorus (heavy, ParamIDs::modIds, MT::chorus, 0.5f, 0.4f, 0.4f);
        reverb (heavy, RT::hall, 0.85f, 0.3f, 30.0f, 4.0f);
        space (heavy);
        heavy.set (ParamIDs::patchLevel, -9.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (heavy);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto colossus = x.make ("Colossus Blast", 7, { "Bass", "Brass", "Drive", "Hit" },
            "The huge, low brass blast that shakes the room: seven detuned saws and a pulse an octave down, a clean "
            "sine sub beneath, the filter tearing open as it hits, through insert bus A's ABYSS drive, the pitch sagging "
            "as it rings, into a hall. Play one low note (C1 to C2) and let it ring. The MOD WHEEL adds grit (SHAPER 1). "
            "MACROS: BRIGHT, SPACE, SAG (sinks further).");
        colossus.osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.7f)
                .osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f)
                .osc (1, "Volume", 0.35f).osc (1, "Insert", 1.0f)
                .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.5f)
                .osc (2, "Filter", 0.0f).osc (2, "PhaseRand", 0.0f).osc (2, "SendReverb", 0.1f)
                .shaper (0, synth::ShaperModel::tube, 0.05f)
                .filter (FilterType::lp24, 400.0f, 0.2f, 0.2f).filterEnvelope (60.0f, 1200.0f, 0.4f, 1500.0f)
                .set (ParamIDs::fltVel, 0.4f)
                .ampEnvelope (40.0f, 2500.0f, 0.7f, 1800.0f)
                .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        colossus.rig.modulation.pitchEnvelope = { { SP7 { 0.0f, 0.5f, 0.3f }, SP7 { 2.5f, 0.46f, 0.0f } }, -1, -1, -1 };
        drive (colossus, 0, DM::abyss, 4.0f, 4.0f, 0.0f, 0.0f, 0.35f);
        route (colossus, "modWheel", ParamIDs::shp (0, "Drive"), 0.5f, 100.0f);
        route (colossus, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (colossus, "macro3", ParamIDs::pitchEnvRange, 0.15f);
        macros (colossus, "BRIGHT", "SPACE", "SAG");
        reverb (colossus, RT::hall, 0.8f, 0.2f, 20.0f, 3.5f);
        space (colossus);
        colossus.set (ParamIDs::patchLevel, -5.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (colossus);

        auto undertow = x.make ("Undertow", 8, { "Bass", "Sub", "Swell", "Filter" },
            "A deep sub swelling up from nothing: a sine with a soft saw an octave up, the filter opening slowly as it "
            "swells in over two seconds and closing as it sinks away. Felt as much as heard. MACROS: BRIGHT, SPACE, "
            "SURGE (a bigger opening).");
        undertow.osc (0, "Wave", sine).osc (0, "Volume", 0.85f).osc (0, "PhaseRand", 0.0f).osc (0, "SendReverb", 0.1f)
                .osc (0, "Filter", 0.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.2f)
                .filter (FilterType::lp24, 150.0f, 0.15f, 0.0f).filterEnvelope (2000.0f, 2000.0f, 0.5f, 2000.0f)
                .ampEnvelope (2000.0f, 1500.0f, 1.0f, 2500.0f);
        route (undertow, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (undertow, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (undertow, "BRIGHT", "SPACE", "SURGE");
        reverb (undertow, RT::hall, 0.7f, 0.2f);
        space (undertow);
        undertow.set (ParamIDs::patchLevel, -5.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (undertow);

        auto ostBass = x.make ("Ostinato Bass", 9, { "Bass", "Arp", "Strings", "Driving" },
            "A driving low string bass in sixteenths, in time with your song: hold a note and ARP 1 repeats it, the "
            "first of every four accented, three saws a hair apart with a short bowed snap (the FILTER envelope). The "
            "MOD WHEEL digs in harder. MACROS: BRIGHT, SPACE, BITE (more snap).");
        ostBass.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.4f)
               .osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f).osc (0, "SendReverb", 0.2f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
               .osc (1, "Arp", 1.0f).osc (1, "Filter", 0.0f).osc (1, "SendReverb", 0.0f)
               .filter (FilterType::lp24, 450.0f, 0.2f, 0.4f).filterEnvelope (0.0f, 120.0f, 0.1f, 80.0f)
               .set (ParamIDs::fltVel, 0.5f)
               .ampEnvelope (3.0f, 140.0f, 0.4f, 70.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
               .set (ParamIDs::arp (0, "Gate"), 0.7f);
        {
            auto& p = ostBass.rig.modulation.arps[0].patterns[0];
            for (int k = 0; k < 16; ++k)
                p.blocks[(size_t) k].velocity = k % 4 == 0 ? 1.0f : 0.65f;
        }
        route (ostBass, "modWheel", ParamIDs::fltCutoff, 0.15f, 100.0f);
        route (ostBass, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (ostBass, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (ostBass, "BRIGHT", "SPACE", "BITE");
        reverb (ostBass, RT::hall, 0.6f, 0.1f);
        space (ostBass);
        ostBass.set (ParamIDs::patchLevel, 1.5f);   // LEVEL: set by its peaks, -3 dBFS (measured, not by ear)
        x.add (ostBass);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto echoPiano = x.make ("Echo Piano", 10, { "Keys", "Piano", "Felt", "Delay" },
            "A sparse, sad piano with long echoes: LAST LIGHT's felt piano (synthesis, not samples), darker still, "
            "each note answered by dotted-eighth echoes fading into a big hall. Brighter and louder the harder you "
            "play; the sustain pedal works. MACROS: BRIGHT, SPACE, FELT (a clearer strike).");
        feltPiano (echoPiano);
        echoPiano.set (ParamIDs::fltCutoff, 700.0f);
        delay (echoPiano, "1/8 dotted", 0.4f, 0.18f, true, 3.5f);
        echoPiano.set (ParamIDs::delayToReverb, 0.4f);
        reverb (echoPiano, RT::hall, 0.8f, 0.25f, 30.0f, 4.0f);
        macros (echoPiano, "BRIGHT", "SPACE", "FELT");
        space (echoPiano);
        echoPiano.set (ParamIDs::patchLevel, 0.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (echoPiano);

        auto chorale = x.make ("Slowed Chorale", 11, { "Keys", "Lo-Fi", "Pitch", "Tape" },
            "Keys dragged like a tape slowing down: a soft triangle and saw, each note sagging a little after it "
            "starts (the PITCH envelope), warbling (MOD: vibrato) and worn (LO-FI: cassette), in a hall. Slow chords "
            "work best. MACROS: BRIGHT, SPACE, WARBLE (more wobble).");
        chorale.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f)
               .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Volume", 0.2f)
               .filter (FilterType::lp12, 1300.0f, 0.05f, 0.2f).filterEnvelope (0.0f, 1200.0f, 0.5f, 800.0f)
               .ampEnvelope (20.0f, 3000.0f, 0.4f, 1200.0f)
               .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f)
               .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
               .set (ParamIDs::lofiAmount, 2.0f).set (ParamIDs::lofiMix, 0.35f).set (ParamIDs::lofiNoise, 0.0f);
        chorale.rig.modulation.pitchEnvelope = { { SP7 { 0.0f, 0.5f, 0.0f }, SP7 { 0.8f, 0.48f, 0.0f } }, -1, -1, -1 };
        route (chorale, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (chorale, "macro3", ParamIDs::modDepth, 0.3f);
        macros (chorale, "BRIGHT", "SPACE", "WARBLE");
        chorus (chorale, ParamIDs::modIds, MT::vibrato, 0.5f, 0.25f, 1.0f);
        reverb (chorale, RT::hall, 0.7f, 0.25f);
        space (chorale);
        chorale.set (ParamIDs::patchLevel, -1.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chorale);

        auto glassMem = x.make ("Glass Memory", 12, { "Keys", "FM", "Bell", "Glass" },
            "Cold, glassy bells: a sine frequency-modulated by another a little over two octaves up, the modulation "
            "fading over half a second (ENV 1) so each note rings out clear like struck glass, a quiet octave above, "
            "a long ring into a plate. MACROS: BRIGHT (more FM), SPACE, SHIMMER (the octave).");
        glassMem.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
                .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.05f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 28.0f).osc (1, "Volume", 1.0f)
                .osc (1, "PhaseRand", 0.0f)
                .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.1f)
                .ampEnvelope (1.0f, 3500.0f, 0.0f, 2000.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f)
                .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        glassMem.rig.modulation.envelopes[0] = { { SP7 { 0.0f, 1.0f, -0.5f }, SP7 { 0.5f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (glassMem, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (glassMem, "macro1", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (glassMem, "macro3", ParamIDs::osc (2, "Volume"), 0.2f);
        macros (glassMem, "BRIGHT", "SPACE", "SHIMMER");
        reverb (glassMem, RT::plate, 0.7f, 0.25f, 30.0f, 5.0f);
        space (glassMem);
        glassMem.set (ParamIDs::patchLevel, -0.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glassMem);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto ostStrings = x.make ("Ostinato Strings", 13, { "Arp", "Strings", "Mod Wheel", "Build" },
            "Relentless string sixteenths: hold a chord and ARP 1 runs up and down it, three saws a hair apart with a "
            "bowed snap on each note, a string-ensemble chorus and a hall, in time with your song. Push the MOD WHEEL to "
            "build: brighter and louder. MACROS: BRIGHT, SPACE, BITE (more snap).");
        ostStrings.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.7f)
                  .osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
                  .filter (FilterType::lp24, 1100.0f, 0.15f, 0.3f).filterEnvelope (0.0f, 150.0f, 0.3f, 100.0f)
                  .set (ParamIDs::fltKey, 0.4f)
                  .ampEnvelope (4.0f, 200.0f, 0.6f, 120.0f)
                  .set (ParamIDs::ampVolume, 0.8f)
                  .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                  .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
                  .set (ParamIDs::arp (0, "Gate"), 0.7f);
        route (ostStrings, "modWheel", ParamIDs::fltCutoff, 0.2f, 200.0f);
        route (ostStrings, "modWheel", ParamIDs::ampVolume, 0.12f, 200.0f);
        route (ostStrings, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (ostStrings, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (ostStrings, "BRIGHT", "SPACE", "BITE");
        chorus (ostStrings, ParamIDs::modIds, MT::chorus, 0.6f, 0.4f, 0.4f);
        reverb (ostStrings, RT::hall, 0.7f, 0.2f);
        space (ostStrings);
        ostStrings.set (ParamIDs::patchLevel, -0.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ostStrings);

        auto layers = x.make ("Layers Deep", 14, { "Arp", "Arps Together", "Stereo", "Time" },
            "Three speeds of time at once: hold a chord and three arpeggiators play it together, ARP 1 in sixteenths "
            "high up (glassy plucks, left), ARP 2 in eighths (soft triangles, right) and ARP 3 in slow quarter notes "
            "deep down (a warm pulse, centre), all on the same clock. MACROS: BRIGHT, SPACE, DEPTH (louder slow layer).");
        layers.osc (0, "Wave", sine).osc (0, "Transpose", 12.0f).osc (0, "Volume", 0.35f).osc (0, "Pan", -0.5f)
              .osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.4f).osc (1, "Pan", 0.5f).osc (1, "Arp", 2.0f)
              .osc (2, "On", 1.0f).osc (2, "Wave", pulse).osc (2, "Width", 0.4f).osc (2, "Transpose", -12.0f)
              .osc (2, "Volume", 0.35f).osc (2, "Arp", 3.0f)
              .filter (FilterType::lp24, 2200.0f, 0.1f, 0.2f).filterEnvelope (0.0f, 250.0f, 0.3f, 200.0f)
              .ampEnvelope (2.0f, 500.0f, 0.3f, 300.0f)
              .set (ParamIDs::arpsTogether, 1.0f);
        for (const auto [k, div, mode, gate] : { std::tuple { 0, "1/16", synth::arp::Mode::up, 0.4f },
                                                 std::tuple { 1, "1/8", synth::arp::Mode::down, 0.6f },
                                                 std::tuple { 2, "1/4", synth::arp::Mode::asPlayed, 0.9f } })
            layers.set (ParamIDs::arp (k, "On"), 1.0f).set (ParamIDs::arp (k, "Mode"), (float) mode)
                  .set (ParamIDs::arp (k, "Division"), division (div)).set (ParamIDs::arp (k, "Octaves"), k == 0 ? 2.0f : 1.0f)
                  .set (ParamIDs::arp (k, "Gate"), gate);
        route (layers, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (layers, "macro3", ParamIDs::osc (2, "Volume"), 0.2f);
        macros (layers, "BRIGHT", "SPACE", "DEPTH");
        delay (layers, "1/8 dotted", 0.3f, 0.12f, true, 5.0f);
        reverb (layers, RT::hall, 0.7f, 0.25f);
        space (layers);
        layers.set (ParamIDs::patchLevel, -0.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (layers);

        auto countdown = x.make ("Countdown", 15, { "Arp", "Drums", "Pulse", "Tension" },
            "Time running out: the drum sequencer ticks in sixteenths (a tuned rim, every fourth louder) over a soft "
            "kick on each beat like a heartbeat, in time with your song, while ARP 1 pulses the chord you hold in "
            "eighths on a muted pulse wave. Hold notes above C3 (the kit is on C2 to B2). MACROS: BRIGHT, SPACE, "
            "TENSION (the filter opens).");
        countdown.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
                 .filter (FilterType::lp24, 800.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 180.0f, 0.2f, 120.0f)
                 .ampEnvelope (2.0f, 250.0f, 0.3f, 150.0f)
                 .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                 .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
                 .set (ParamIDs::arp (0, "Gate"), 0.5f)
                 .set (ParamIDs::drumsOn, 1.0f).set (ParamIDs::dseqOn, 1.0f)
                 .set (ParamIDs::drum (0, "Decay"), 0.35f).set (ParamIDs::drum (0, "Level"), -6.0f).set (ParamIDs::drum (0, "SendReverb"), 0.2f)
                 .set (ParamIDs::drum (1, "Pitch"), 9.0f).set (ParamIDs::drum (1, "Decay"), 0.1f).set (ParamIDs::drum (1, "Level"), -10.0f)
                 .set (ParamIDs::drum (1, "SendReverb"), 0.25f);
        {
            auto& a = countdown.rig.modulation.drumPatterns[0];
            for (int k = 0; k < 16; ++k)
            {
                synth::drumseq::Note n;
                n.row = 1;
                n.start = k;
                n.velocity = k % 4 == 0 ? 0.85f : 0.45f;
                a.notes.push_back (n);
                if (k % 4 == 0)
                {
                    synth::drumseq::Note kick;
                    kick.row = 0;
                    kick.start = k;
                    kick.velocity = 0.7f;
                    a.notes.push_back (kick);
                }
            }
        }
        route (countdown, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (countdown, "macro3", ParamIDs::fltCutoff, 0.25f);
        macros (countdown, "BRIGHT", "SPACE", "TENSION");
        reverb (countdown, RT::hall, 0.6f, 0.2f);
        space (countdown);
        countdown.set (ParamIDs::patchLevel, -0.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (countdown);
    }

    // The soft piano (NEGATIVE SPACE): additive, unlike LAST LIGHT's FM felt piano. A real string's harmonics fade at
    // their own rates, the high ones first, and sit a little sharp (stretched); a heavily felted upright recorded close
    // is mostly the first few. So: the fundamental on two strings 0.8 cents apart (fixed phases, so every note comes out
    // the same; the second at half level, so their slow beating swings the level about 5 dB, not 12), then sines at the
    // 2nd to 5th harmonics a few cents sharp, each on its own envelope (1.6, 0.9, 0.5 and 0.3 s), a faint saw through
    // the filter for the low notes' richer top (louder for 0.4 s), and the felt's thump (noise, 50 ms). Velocity is
    // the hammer: the FILTER's strike opens more on hard notes (ENV VEL) and soft notes stay darker (VELOCITY), so a hard
    // note's brightness still dies away (velocity on the harmonics' volumes kept them bright for the whole note). Our
    // own numbers, from what's generally known about pianos, not measured from any instrument. The MOD WHEEL adds the
    // keys' mechanical noise.
    void softPiano (Builder& b)
    {
        using SP8 = synth::mod::ShapePoint;
        b.osc (0, "Wave", sine).osc (0, "Volume", 0.8f).osc (0, "PhaseRand", 0.0f)
         .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Fine", 0.8f).osc (1, "Volume", 0.25f).osc (1, "PhaseRand", 0.0f)
         .osc (1, "Pan", 0.15f);
        constexpr std::array<float, 4> transpose { 12.0f, 19.0f, 24.0f, 28.0f };   // harmonics 2 to 5
        constexpr std::array<float, 4> fine { 1.0f, 4.0f, 5.0f, -6.0f };           // equal-tempered to harmonic, plus stretch
        constexpr std::array<float, 4> base { 0.05f, 0.02f, 0.0f, 0.0f };
        constexpr std::array<float, 4> strike { 0.12f, 0.08f, 0.05f, 0.03f };      // envelope routes (x2 = volume)
        constexpr std::array<float, 4> velocity { 0.015f, 0.01f, 0.005f, 0.0f };   // a touch: velocity's job is the hammer (the FILTER)
        constexpr std::array<float, 4> seconds { 1.6f, 0.9f, 0.5f, 0.3f };
        for (int k = 0; k < 4; ++k)
        {
            const auto o = k + 2;
            b.osc (o, "On", 1.0f).osc (o, "Wave", sine).osc (o, "Transpose", transpose[(size_t) k]).osc (o, "Fine", fine[(size_t) k])
             .osc (o, "Volume", base[(size_t) k]).osc (o, "PhaseRand", 0.0f).osc (o, "Pan", k % 2 == 0 ? -0.1f : 0.1f)
             .set (ParamIDs::env (k, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) k] = { { SP8 { 0.0f, 1.0f, -0.5f }, SP8 { seconds[(size_t) k], 0.0f, 0.0f } }, -1, -1, -1 };
            route (b, ("env" + juce::String (k + 1)).toRawUTF8(), ParamIDs::osc (o, "Volume"), strike[(size_t) k]);
            if (velocity[(size_t) k] > 0.0f)
                route (b, "velocity", ParamIDs::osc (o, "Volume"), velocity[(size_t) k]);
        }
        b.osc (6, "On", 1.0f).osc (6, "Wave", saw).osc (6, "Volume", 0.04f)
         .osc (7, "On", 1.0f).osc (7, "Gain", 0.0f).osc (7, "Noise", 0.3f).osc (7, "NoiseColour", 0.55f).osc (7, "Volume", 0.0f)
         .osc (7, "NoiseWidth", 2.0f)
         .set (ParamIDs::env (4, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
         .set (ParamIDs::env (5, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
         .filter (FilterType::lp12, 900.0f, 0.05f, 0.3f).filterEnvelope (0.0f, 400.0f, 0.2f, 300.0f)
         .set (ParamIDs::fltKey, 0.8f).set (ParamIDs::fltVel, 0.5f).set (ParamIDs::fltEnvVel, 0.8f)
         .ampEnvelope (4.0f, 6500.0f, 0.0f, 380.0f).set (ParamIDs::ampDecayCurve, -0.6f).set (ParamIDs::ampVel, 0.75f);
        b.rig.modulation.envelopes[4] = { { SP8 { 0.0f, 1.0f, -0.5f }, SP8 { 0.4f, 0.0f, 0.0f } }, -1, -1, -1 };
        b.rig.modulation.envelopes[5] = { { SP8 { 0.0f, 1.0f, -0.5f }, SP8 { 0.05f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (b, "env5", ParamIDs::osc (6, "Volume"), 0.04f);
        route (b, "env6", ParamIDs::osc (7, "Volume"), 0.2f);
        route (b, "modWheel", ParamIDs::osc (7, "Noise"), 0.3f);
        route (b, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (b, "macro3", ParamIDs::fltCutoff, 0.15f);
        route (b, "macro3", ParamIDs::osc (6, "Volume"), 0.03f);
        eqBand (b, 0, eq::Shape::lowShelf, 150.0f, -4.0f);
    }

    // NEGATIVE SPACE (the owner's name): inspired by the modern dark-ambient film and TV scores of the 2010s: a
    // fragile, lonely piano close-up over cold, rumbling electronics, hypnotic sequenced analog pulses, glitchy,
    // broken textures and buzzing grit. Cold, tense, quietly melancholy. No titles, composers or music: our own sounds.
    void negativeSpace (Expansion x)
    {
        using SP8 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;
        using GM = effects::Glitch::Mode;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto staticLead = x.make ("Static Lead", 1, { "Lead", "Drive", "Pulse", "Mono" },
            "A buzzing, lonely lead: a narrow pulse and a saw a hair apart through insert bus A's SKYLINE, a gentle "
            "drive that buzzes rather than screams, a slow vibrato and a small dark room. MACROS: BRIGHT, SPACE, BUZZ "
            "(more SHAPER 1 into the drive).");
        staticLead.osc (0, "Wave", pulse).osc (0, "Width", 0.25f).osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
                  .osc (0, "VibAmount", 8.0f).osc (0, "VibRate", 4.0f).osc (0, "VibFade", 600.0f)
                  .osc (1, "On", 1.0f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.3f).osc (1, "Insert", 1.0f)
                  .shaper (0, synth::ShaperModel::soft, 0.05f)
                  .filter (FilterType::lp24, 1800.0f, 0.15f, 0.0f)
                  .ampEnvelope (15.0f, 500.0f, 0.85f, 400.0f)
                  .mono (30.0f);
        drive (staticLead, 0, DM::skyline, 5.0f, 4.0f, 0.0f, 0.2f, 0.3f);
        route (staticLead, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (staticLead, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (staticLead, "BRIGHT", "SPACE", "BUZZ");
        reverb (staticLead, RT::room, 0.4f, 0.15f);
        space (staticLead);
        staticLead.set (ParamIDs::patchLevel, 1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (staticLead);

        auto hollow = x.make ("Hollow Signal", 2, { "Lead", "Filter", "Resonance", "Mono" },
            "A thin, filtered, lonely line: a saw through a resonant band-pass so only a hollow core is left, a slow "
            "glide, quarter-note echoes. The MOD WHEEL sweeps the band up. MACROS: BRIGHT, SPACE, RESONANCE.");
        hollow.osc (0, "Volume", 1.6f)
              .filter (FilterType::bp12, 900.0f, 0.55f, 0.0f).set (ParamIDs::fltKey, 0.8f)
              .ampEnvelope (30.0f, 500.0f, 0.85f, 600.0f)
              .mono (120.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always);
        route (hollow, "modWheel", ParamIDs::fltCutoff, 0.2f, 150.0f);
        route (hollow, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (hollow, "macro3", ParamIDs::fltRes, 0.3f);
        macros (hollow, "BRIGHT", "SPACE", "RESONANCE");
        delay (hollow, "1/4", 0.35f, 0.18f, true, 4.0f);
        reverb (hollow, RT::plate, 0.5f, 0.15f);
        space (hollow);
        hollow.set (ParamIDs::patchLevel, 8.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hollow);

        auto glassTone = x.make ("Glass Tone", 3, { "Lead", "Sine", "Clean", "Mono" },
            "A pure, cold tone: a sine with a faint fifth above, no vibrato, a soft attack and a long, quiet tail into "
            "a plate. Clinical and still. MACROS: BRIGHT, SPACE, FIFTH (more of the fifth).");
        glassTone.osc (0, "Wave", sine).osc (0, "Volume", 0.8f).osc (0, "PhaseRand", 0.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 19.0f).osc (1, "Volume", 0.06f)
                 .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.05f)
                 .ampEnvelope (60.0f, 500.0f, 0.9f, 900.0f)
                 .mono (60.0f);
        route (glassTone, "macro1", ParamIDs::osc (2, "Volume"), 0.08f);
        route (glassTone, "macro3", ParamIDs::osc (1, "Volume"), 0.08f);
        macros (glassTone, "BRIGHT", "SPACE", "FIFTH");
        delay (glassTone, "1/4 dotted", 0.3f, 0.12f, true, 5.0f);
        reverb (glassTone, RT::plate, 0.7f, 0.25f, 30.0f, 4.5f);
        space (glassTone);
        glassTone.set (ParamIDs::patchLevel, -3.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glassTone);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto hum = x.make ("Low Hum", 4, { "Pad", "Drone", "Drive", "Inserts" },
            "A distorted drone rumbling under everything: two saws an octave apart and a sine sub, through a dark "
            "filter and insert bus A's RABID, breathing slowly (LFO 1 on the filter, the whole synth). Hold one or two "
            "low notes. MACROS: BRIGHT, SPACE, GRIT (more SHAPER 1 into the drive).");
        hum.osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.7f)
           .osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
           .osc (1, "On", 1.0f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f).osc (1, "Insert", 1.0f)
           .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.4f).osc (2, "Filter", 0.0f)
           .shaper (0, synth::ShaperModel::tube, 0.05f)
           .filter (FilterType::lp24, 350.0f, 0.15f, 0.0f)
           .ampEnvelope (1500.0f, 1500.0f, 1.0f, 2500.0f)
           .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.15f)
           .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        drive (hum, 0, DM::rabid, 4.0f, 2.0f, -4.0f, 0.0f, 0.3f);
        eqBand (hum, 0, eq::Shape::highCut, 3000.0f, 0.0f);   // a rumble under the piano, not fizz
        route (hum, "lfo1", ParamIDs::fltCutoff, 0.1f);
        route (hum, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (hum, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (hum, "BRIGHT", "SPACE", "GRIT");
        reverb (hum, RT::hall, 0.6f, 0.2f);
        space (hum);
        hum.set (ParamIDs::patchLevel, -4.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hum);

        auto coldRoom = x.make ("Cold Room", 5, { "Pad", "Airy", "Noise", "Sterile" },
            "A sterile, airy ambience: soft sines and triangles in a wide unison with a thin breath of air through a "
            "high-pass, swelling in slowly, the air drifting across the stereo field, in a large, cold hall. MACROS: "
            "BRIGHT, SPACE, AIR (more breath).");
        coldRoom.osc (0, "Wave", triangle).osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.08f)
                .osc (0, "UniSpread", 1.0f).osc (0, "Volume", 0.45f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f)
                .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.03f).osc (2, "NoiseColour", 0.1f)
                .osc (2, "NoiseWidth", 2.0f).osc (2, "Volume", 0.3f)
                .filter (FilterType::lp12, 4000.0f, 0.05f, 0.0f)
                .ampEnvelope (2000.0f, 2000.0f, 1.0f, 3000.0f);
        route (coldRoom, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (coldRoom, "macro3", ParamIDs::osc (2, "Noise"), 0.05f);
        macros (coldRoom, "BRIGHT", "SPACE", "AIR");
        chorus (coldRoom, ParamIDs::modIds, MT::autoPan, 0.1f, 0.4f, 0.6f);
        reverb (coldRoom, RT::hall, 0.9f, 0.3f, 40.0f, 3.5f);
        eqBand (coldRoom, 0, eq::Shape::lowCut, 150.0f, 0.0f);
        space (coldRoom);
        coldRoom.set (ParamIDs::patchLevel, -3.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (coldRoom);

        auto broken = x.make ("Broken Signal", 6, { "Pad", "Glitch", "Step Seq", "Unison" },
            "A pad that keeps breaking up: a soft saw unison cut out at irregular moments by the STEP SEQUENCER and "
            "chopped by insert bus A's GLITCH, in time with your song, like a signal dropping out. MACROS: BRIGHT, "
            "SPACE, DROPOUT (deeper cuts).");
        broken.osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.8f)
              .osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f).osc (1, "Insert", 1.0f)
              .filter (FilterType::lp24, 1500.0f, 0.1f, 0.0f)
              .ampEnvelope (500.0f, 1000.0f, 0.95f, 1500.0f)
              .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqMode, (float) synth::seq::Mode::mono)
              .set (ParamIDs::seqVolMix, 0.7f).set (ParamIDs::seqGate, 1.0f).set (ParamIDs::seqContour, 0.03f)
              .set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        {
            constexpr std::array<bool, 16> on { true, true, true, false, true, true, true, true, false, true, true, true, true, true, false, true };
            for (int row = 0; row < 2; ++row)
                for (int k = 0; k < synth::seq::maxSteps; ++k)
                {
                    auto& step = broken.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                    step.on = on[(size_t) (k % 16)];
                    step.level = 1.0f;
                }
        }
        glitch (broken, 0, GM::sequence, "1/16", 2.0f, 2.0f, 0.25f);
        broken.set (ParamIDs::ins (0, "SendDelay"), 0.3f).set (ParamIDs::ins (0, "SendReverb"), 0.4f);
        route (broken, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (broken, "macro3", ParamIDs::seqVolMix, 0.3f);
        macros (broken, "BRIGHT", "SPACE", "DROPOUT");
        delay (broken, "1/8 dotted", 0.3f, 0.12f, true, 4.0f);
        reverb (broken, RT::hall, 0.7f, 0.25f);
        space (broken);
        broken.set (ParamIDs::patchLevel, -6.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (broken);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto grit = x.make ("Grit Rumble", 7, { "Bass", "Drive", "Sub", "Inserts" },
            "A distorted, rumbling low end: a saw and a square through insert bus A's ABYSS, a clean sine sub under "
            "it, a dark filter that opens a little as each note starts. MACROS: BRIGHT, SPACE, GRIT (more SHAPER 1 into "
            "the drive).");
        grit.osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f)
            .osc (1, "Volume", 0.35f).osc (1, "Insert", 1.0f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.5f)
            .osc (2, "Filter", 0.0f).osc (2, "PhaseRand", 0.0f).osc (2, "SendReverb", 0.0f)
            .shaper (0, synth::ShaperModel::tube, 0.05f)
            .filter (FilterType::lp24, 300.0f, 0.2f, 0.2f).filterEnvelope (0.0f, 600.0f, 0.4f, 400.0f)
            .ampEnvelope (5.0f, 800.0f, 0.85f, 300.0f)
            .mono (20.0f);
        drive (grit, 0, DM::abyss, 4.0f, 3.5f, 0.0f, 0.0f, 0.15f);
        route (grit, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (grit, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (grit, "BRIGHT", "SPACE", "GRIT");
        reverb (grit, RT::room, 0.4f, 0.08f);
        space (grit);
        grit.set (ParamIDs::patchLevel, -0.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (grit);

        auto pulseBass = x.make ("Pulse Bass", 8, { "Bass", "Arp", "Analog", "Sequence" },
            "A hypnotic sequenced analog bass in time with your song: hold a note and ARP 1 plays it in sixteenths, "
            "jumping up an octave on the off-beats, a saw with a short snapping filter and a touch of resonance. "
            "MACROS: BRIGHT, SPACE, SNAP (more filter snap).");
        pulseBass.osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f).osc (0, "SendReverb", 0.1f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
                 .osc (1, "Arp", 1.0f).osc (1, "Filter", 0.0f).osc (1, "SendReverb", 0.0f)
                 .filter (FilterType::lp24, 400.0f, 0.35f, 0.3f).filterEnvelope (0.0f, 110.0f, 0.0f, 80.0f)
                 .set (ParamIDs::fltVel, 0.4f)
                 .ampEnvelope (2.0f, 160.0f, 0.5f, 60.0f)
                 .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
                 .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
                 .set (ParamIDs::arp (0, "Gate"), 0.6f);
        {
            auto& p = pulseBass.rig.modulation.arps[0].patterns[0];
            for (int k = 0; k < 16; ++k)
            {
                p.blocks[(size_t) k].transpose = k % 4 == 2 ? 12 : 0;
                p.blocks[(size_t) k].velocity = k % 4 == 0 ? 1.0f : 0.7f;
            }
        }
        route (pulseBass, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (pulseBass, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (pulseBass, "BRIGHT", "SPACE", "SNAP");
        delay (pulseBass, "1/8 dotted", 0.2f, 0.08f, false, 3.0f);
        reverb (pulseBass, RT::room, 0.4f, 0.08f);
        space (pulseBass);
        pulseBass.set (ParamIDs::patchLevel, 1.8f);   // LEVEL: set by its peaks, -3 dBFS (measured, not by ear)
        x.add (pulseBass);

        auto throb = x.make ("Sub Throb", 9, { "Bass", "Sub", "LFO", "Tremolo" },
            "A deep sub that throbs slowly with your song: a sine with a soft triangle above, its level pulsing in "
            "half notes (LFO 1 on the volume, the whole synth), a warm filter. MACROS: BRIGHT, SPACE, BODY (more of "
            "the triangle, heard on small speakers).");
        throb.osc (0, "Wave", sine).osc (0, "Volume", 0.85f).osc (0, "PhaseRand", 0.0f).osc (0, "SendReverb", 0.05f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
             .filter (FilterType::lp24, 500.0f, 0.05f, 0.0f)
             .ampEnvelope (200.0f, 800.0f, 1.0f, 800.0f)
             .set (ParamIDs::ampVolume, 0.8f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sine)
             .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
             .set (ParamIDs::lfo (0, "Division"), division ("1/2"))
             .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (throb, "lfo1", ParamIDs::ampVolume, 0.08f);
        route (throb, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (throb, "macro3", ParamIDs::osc (1, "Volume"), 0.1f);
        macros (throb, "BRIGHT", "SPACE", "BODY");
        reverb (throb, RT::room, 0.4f, 0.1f);
        space (throb);
        throb.set (ParamIDs::patchLevel, -4.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (throb);

        // ---- KEYS: the three pianos ----------------------------------------------------------------------------
        auto soft = x.make ("Soft Felt Piano", 10, { "Keys", "Piano", "Felt", "Intimate" },
            "The centrepiece: a soft, intimate felt piano heard up close, for quiet, cinematic piano parts. Each note "
            "built harmonic by harmonic, the higher ones fading first as a real string's do and tuned slightly sharp, "
            "two strings a hair apart, the soft thump of felt, brighter and louder the harder you play, ringing on with "
            "the sustain pedal, in a small close room. Made by synthesis, not recorded: an impression of a felted "
            "upright, not a copy of any instrument. The MOD WHEEL adds the keys' mechanical noise. MACROS: BRIGHT, "
            "SPACE, FELT (less felt: a clearer strike).");
        softPiano (soft);
        macros (soft, "BRIGHT", "SPACE", "FELT");
        reverb (soft, RT::room, 0.45f, 0.15f, 5.0f, 4.0f);
        space (soft);
        soft.set (ParamIDs::patchLevel, -4.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (soft);

        auto upright = x.make ("Broken Upright", 11, { "Keys", "Piano", "Detuned", "Drive" },
            "The soft piano worn and out of tune: its two strings pulled seven cents apart, warbling like old tape "
            "(MOD: vibrato, LO-FI: cassette), with a distorted drone rising underneath each note (a saw an octave down "
            "through insert bus A's RABID, swelling in over three seconds). MACROS: BRIGHT, SPACE, DRONE (more drone).");
        softPiano (upright);
        upright.osc (1, "Fine", 7.0f)
               .osc (6, "Transpose", -12.0f).osc (6, "Insert", 1.0f).osc (6, "Filter", 0.0f)
               .set (ParamIDs::env (6, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
               .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
               .set (ParamIDs::lofiAmount, 2.5f).set (ParamIDs::lofiMix, 0.4f).set (ParamIDs::lofiNoise, 0.0f);
        upright.rig.modulation.envelopes[6] = { { SP8 { 0.0f, 0.0f, 0.3f }, SP8 { 3.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (upright, "env7", ParamIDs::osc (6, "Volume"), 0.035f);
        route (upright, "macro3", ParamIDs::osc (6, "Volume"), 0.06f);
        drive (upright, 0, DM::rabid, 5.0f, 2.0f, -6.0f, 0.0f, 0.3f);
        macros (upright, "BRIGHT", "SPACE", "DRONE");
        chorus (upright, ParamIDs::modIds, MT::vibrato, 0.5f, 0.2f, 1.0f);
        reverb (upright, RT::room, 0.5f, 0.18f, 10.0f, 3.5f);
        space (upright);
        upright.set (ParamIDs::patchLevel, -0.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (upright);

        auto ghost = x.make ("Ghost Piano", 12, { "Keys", "Piano", "Reverse", "Cinematic" },
            "The soft piano dissolving into the air: each note followed by its own ghost swelling back in through a "
            "reverb that plays backwards, a quiet shimmer two octaves up rising after the strike, and long ping-pong "
            "echoes. The most cinematic of the three. MACROS: BRIGHT, SPACE, SHIMMER.");
        softPiano (ghost);
        ghost.osc (6, "Wave", sine).osc (6, "Transpose", 24.0f).osc (6, "Fine", 3.0f).osc (6, "Filter", 0.0f)
             .set (ParamIDs::env (6, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        ghost.rig.modulation.envelopes[6] = { { SP8 { 0.0f, 0.0f, 0.4f }, SP8 { 1.2f, 1.0f, -0.4f }, SP8 { 5.0f, 0.0f, 0.0f } }, -1, -1, -1 };
        ghost.rig.modulation.routes.erase (std::remove_if (ghost.rig.modulation.routes.begin(), ghost.rig.modulation.routes.end(),
                                                            [] (const ModRouteData& r) { return r.source == "env5" || (r.source == "macro3" && r.destination != ParamIDs::fltCutoff); }),
                                            ghost.rig.modulation.routes.end());
        route (ghost, "env7", ParamIDs::osc (6, "Volume"), 0.04f);
        route (ghost, "macro3", ParamIDs::osc (6, "Volume"), 0.05f);
        macros (ghost, "BRIGHT", "SPACE", "SHIMMER");
        delay (ghost, "1/4 dotted", 0.4f, 0.15f, true, 4.0f);
        ghost.set (ParamIDs::delayToReverb, 0.5f);
        reverb (ghost, RT::reverse, 0.7f, 0.35f);
        space (ghost);
        ghost.set (ParamIDs::patchLevel, -4.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ghost);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto motion = x.make ("Hypnotic Motion", 13, { "Arp", "Analog", "Pulse", "Sequence" },
            "A driving, hypnotic analog pulse: hold a chord and ARP 1 runs it up in sixteenths over two octaves on a "
            "pulse wave with a snappy filter, the filter slowly opening and closing over four bars (LFO 1, in time "
            "with your song), with dotted echoes. MACROS: BRIGHT, SPACE, SNAP.");
        motion.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Fine", 7.0f).osc (1, "Volume", 0.25f).osc (1, "Arp", 1.0f)
              .filter (FilterType::lp24, 700.0f, 0.3f, 0.35f).filterEnvelope (0.0f, 120.0f, 0.05f, 80.0f)
              .ampEnvelope (1.0f, 180.0f, 0.5f, 90.0f)
              .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sine)
              .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight)
              .set (ParamIDs::lfo (0, "Division"), division ("4/1"))
              .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
              .set (ParamIDs::lfo (0, "Phase"), 0.75f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.5f);
        route (motion, "lfo1", ParamIDs::fltCutoff, 0.2f);
        route (motion, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (motion, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (motion, "BRIGHT", "SPACE", "SNAP");
        delay (motion, "1/8 dotted", 0.3f, 0.15f, true, 4.0f);
        reverb (motion, RT::room, 0.5f, 0.12f);
        space (motion);
        motion.set (ParamIDs::patchLevel, -2.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (motion);

        auto glitchPulse = x.make ("Glitch Pulse", 14, { "Arp", "Glitch", "Crush", "Inserts" },
            "A chopped, stuttering pattern: hold a chord and ARP 1 plays it in sixteenths as short, slightly "
            "bit-crushed plucks, while insert bus A's GLITCH repeats and reverses slices of it in time with your song. "
            "MACROS: BRIGHT, SPACE, CRUSH.");
        glitchPulse.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "Crush", 0.15f).osc (0, "CrushMode", 1.0f)
                   .osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
                   .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.2f).osc (1, "Transpose", 12.0f)
                   .osc (1, "Volume", 0.15f).osc (1, "Arp", 1.0f).osc (1, "Insert", 1.0f)
                   .filter (FilterType::lp24, 3000.0f, 0.15f, 0.3f).filterEnvelope (0.0f, 100.0f, 0.0f, 80.0f)
                   .ampEnvelope (0.5f, 140.0f, 0.0f, 80.0f)
                   .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
                   .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
                   .set (ParamIDs::arp (0, "Gate"), 0.5f);
        glitch (glitchPulse, 0, GM::sequence, "1/16", 4.0f, 3.0f, 0.35f);
        glitchPulse.set (ParamIDs::ins (0, "SendDelay"), 0.4f).set (ParamIDs::ins (0, "SendReverb"), 0.3f);
        route (glitchPulse, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (glitchPulse, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        macros (glitchPulse, "BRIGHT", "SPACE", "CRUSH");
        delay (glitchPulse, "1/16 dotted", 0.3f, 0.15f, true, 5.0f);
        reverb (glitchPulse, RT::plate, 0.5f, 0.15f);
        space (glitchPulse);
        glitchPulse.set (ParamIDs::patchLevel, 5.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glitchPulse);

        auto rows = x.make ("Steady Rows", 15, { "Arp", "Rhythm", "Drums", "Sequencer" },
            "A relentless rowing rhythm: the drum sequencer plays a muted, metronomic pulse (soft kick on the beat, "
            "closed hat in eighths, a rim on the off-beats) in time with your song, while ARP 1 rows through the chord "
            "you hold in eighths, up and down, on a dark saw. Hold notes above C3 (the kit is on C2 to B2). MACROS: "
            "BRIGHT, SPACE, DRIVE (pushes harder).");
        rows.osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
            .filter (FilterType::lp24, 700.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 200.0f, 0.2f, 150.0f)
            .ampEnvelope (3.0f, 300.0f, 0.4f, 150.0f)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
            .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.6f)
            .set (ParamIDs::drumsOn, 1.0f).set (ParamIDs::dseqOn, 1.0f)
            .set (ParamIDs::drum (0, "Decay"), 0.35f).set (ParamIDs::drum (0, "Level"), -5.0f)
            .set (ParamIDs::drum (1, "Pitch"), 5.0f).set (ParamIDs::drum (1, "Level"), -10.0f).set (ParamIDs::drum (1, "SendReverb"), 0.2f)
            .set (ParamIDs::drum (6, "Level"), -9.0f).set (ParamIDs::drum (6, "Decay"), 0.3f);
        {
            auto& a = rows.rig.modulation.drumPatterns[0];
            const auto note = [&a] (int row, double start, float velocity)
            {
                synth::drumseq::Note n;
                n.row = row;
                n.start = start;
                n.velocity = velocity;
                a.notes.push_back (n);
            };
            for (int k = 0; k < 16; k += 4)
                note (0, k, 0.8f);
            for (int k = 0; k < 16; k += 2)
                note (6, k, k % 4 == 0 ? 0.6f : 0.45f);
            for (int k = 2; k < 16; k += 4)
                note (1, k, 0.55f);
        }
        route (rows, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (rows, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (rows, "BRIGHT", "SPACE", "DRIVE");
        reverb (rows, RT::room, 0.5f, 0.12f);
        space (rows);
        rows.set (ParamIDs::patchLevel, 2.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (rows);
    }
}

std::vector<Rig> makeExpansionRigs()
{
    std::vector<Rig> rigs;
    neonRain ({ Rig::numBanks + 0, rigs });
    hellforge ({ Rig::numBanks + 1, rigs });
    starcourt ({ Rig::numBanks + 2, rigs });
    sandsea ({ Rig::numBanks + 3, rigs });
    datastream ({ Rig::numBanks + 4, rigs });
    lastLight ({ Rig::numBanks + 5, rigs });
    simulacrum ({ Rig::numBanks + 6, rigs });
    dreamstate ({ Rig::numBanks + 7, rigs });
    negativeSpace ({ Rig::numBanks + 8, rigs });
    return rigs;
}
