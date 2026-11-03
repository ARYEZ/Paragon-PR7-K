#include "FactoryRigs.h"
#include "Expansions.h"
#include "RigBuilder.h"

#include "dsp/eq/EqDesign.h"
#include "dsp/effects/Delay.h"
#include "dsp/effects/Drive.h"
#include "dsp/effects/Echoes.h"
#include "dsp/effects/Crackle.h"
#include "dsp/effects/Glitch.h"
#include "dsp/effects/LoFi.h"
#include "dsp/effects/Modulation.h"
#include "dsp/effects/Reverb.h"
#include "dsp/effects/Wah.h"
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
    // NEON RAIN's MACRO 3, STORM (0.57.0, the owner's ask: a centrepiece for the bank, built round DOWNPOUR): rain on
    // the glass, its drops ringing on the notes played. RAIN is 0 at the macro's left (no rain at all: the preset sounds
    // exactly as before) and 0.9 at the right (a downpour), the rain coming closer as it builds. The preset's old MACRO
    // 3 job rides along (its routes stay), named in the notes: `oldText` there becomes `newText`.
    struct RainSpot
    {
        float drops = 0.6f;      // DROPS
        int octave = 1;          // octaveNames: +1, +2, +3
        float distance = 0.45f;  // at the macro's left; 0.3 closer at its right
        float space = 0.5f;      // into the REVERB (when the preset has it on)
        float levelDb = -18.0f;
        float width = 0.8f;
    };

    void storm (Builder& b, const juce::String& oldText, const juce::String& newText, RainSpot r = {})
    {
        const auto reverbOn = b.rig.parameters.find (ParamIDs::reverbOn);
        const auto hasReverb = reverbOn != b.rig.parameters.end() && reverbOn->second > 0.5f;
        b.set (ParamIDs::downpourOn, 1.0f).set (ParamIDs::downpourRain, 0.0f).set (ParamIDs::downpourDrops, r.drops)
         .set (ParamIDs::downpourOctave, (float) r.octave).set (ParamIDs::downpourDistance, r.distance)
         .set (ParamIDs::downpourSpace, hasReverb ? r.space : 0.0f).set (ParamIDs::downpourWidth, r.width)
         .set (ParamIDs::downpourFollow, 1.0f).set (ParamIDs::downpourLevel, r.levelDb);
        route (b, "macro3", ParamIDs::downpourRain, 0.9f);
        route (b, "macro3", ParamIDs::downpourDistance, -0.3f);
        b.rig.modulation.macroNames[2] = "STORM";
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, newText);
    }

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
        storm (brass, "GROWL (SHAPER 1's tube)",
               "STORM (rain on the glass, its drops ringing on your notes; SHAPER 1's tube growls with it)",
               RainSpot { 0.5f, 1, 0.4f, 0.5f, -6.0f });
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
        storm (reeds, "GROWL (the diode and more breath)",
               "STORM (rain on the glass, its drops ringing on your notes; the diode and more breath with it)",
               RainSpot { 0.4f, 1, 0.55f, 0.5f, -13.0f });
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
        storm (ribbon, "SCOOP (the scoop's depth)",
               "STORM (rain on the glass, its drops ringing on your notes; a deeper scoop with it)",
               RainSpot { 0.5f, 1, 0.35f, 0.5f, -11.0f });
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
        storm (choir, "VOWEL (towards \"ee\")",
               "STORM (rain on the glass, its drops ringing on your notes; the choir towards \"ee\" with it)",
               RainSpot { 0.8f, 1, 0.5f, 0.7f, -15.0f });
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
        storm (mist, "RAIN (the noise)",
               "STORM (rain on the glass, its drops ringing on your notes; the noise rising with it)",
               RainSpot { 0.5f, 2, 0.6f, 0.7f, -5.0f });
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
        storm (strings, "ENSEMBLE (the detune)",
               "STORM (rain on the glass, its drops ringing on your notes; more detune with it)",
               RainSpot { 0.7f, 1, 0.45f, 0.6f, -8.0f });
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
        storm (pulseBass, "GRIT (the filter's drive)",
               "STORM (rain on the glass, its drops ringing high over your notes; the filter's drive with it)",
               RainSpot { 0.5f, 2, 0.55f, 0.5f, -6.0f });
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
        storm (thunder, "DROP (how far the pitch falls)",
               "STORM (rain on the glass, its drops ringing high over your notes; the pitch falling further with it)",
               RainSpot { 0.4f, 2, 0.7f, 0.6f, -9.5f });
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
        storm (velvet, "FAT (the sub)",
               "STORM (rain on the glass, its drops ringing high over your notes; more sub with it)",
               RainSpot { 0.4f, 2, 0.5f, 0.5f, -16.5f });
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
        storm (piano, "DETUNE (the modulator's tuning: a slow beating)",
               "STORM (rain on the glass, its drops ringing on your notes; the modulator detuned with it, a slow beating)",
               RainSpot { 0.8f, 1, 0.3f, 0.5f, -5.5f });
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
        storm (chimes, "SPACE (more hall and echoes), DETUNE.",
               "SPACE (more hall and echoes), STORM (rain on the glass, its drops ringing on your notes; detuned with it).",
               RainSpot { 0.7f, 0, 0.3f, 0.5f, -14.0f });
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
        storm (nightKeys, "BRASS (the filter envelope)",
               "STORM (rain on the glass, its drops ringing on your notes; brassier with it)",
               RainSpot { 0.6f, 1, 0.4f, 0.5f, -4.0f });
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
        storm (grid, "OCTAVE (the low pulse)",
               "STORM (rain on the glass, its drops ringing on your notes; the low pulse with it)",
               RainSpot { 0.6f, 1, 0.4f, 0.5f, -11.0f });
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
        storm (drops, "SPARKLE (a sine two octaves up)",
               "STORM (rain on the glass, its drops ringing on your notes; a sine two octaves up sparkling with it)",
               RainSpot { 0.9f, 0, 0.35f, 0.5f, -14.0f });
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
        storm (skyline, "SMOOTH (less gating)",
               "STORM (rain on the glass, its drops ringing on your notes; less gating with it)",
               RainSpot { 0.6f, 1, 0.45f, 0.5f, -6.5f });
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

    // HELLFORGE's centrepiece (0.59.0, the owner's ask: "a premiere rig"): INFERNAL, parallel destruction. `hell`
    // sets it up; MACRO 3 is DESTROY on every preset: INFERNAL's MIX (for the four kept presets, from 0: the sound
    // as approved), DRIVE and NOISE. The kept presets' old DESTROY routes ride along.
    struct Hell
    {
        float drive = 0.5f, tight = 0.4f, fuzz = 0.6f, crush = 0.0f, amp = 0.8f, noise = 0.3f, feedback = 0.3f, cab = 0.5f;
        float mix = 1.0f, levelDb = 0.0f;
        float mixRise = 0.0f, driveRise = 0.3f, noiseRise = 0.25f;   // MACRO 3's routes
        // The four presets kept from 0.26.0: nothing at the macro's left, INFERNAL coming in as it turns.
        static Hell keep() { Hell h; h.mix = 0.0f; h.mixRise = 0.7f; h.driveRise = 0.25f; h.noiseRise = 0.2f; h.tight = 0.5f; return h; }
    };

    void hell (Builder& b, Hell h)
    {
        b.set (ParamIDs::infernalOn, 1.0f).set (ParamIDs::infernalDrive, h.drive).set (ParamIDs::infernalTight, h.tight)
         .set (ParamIDs::infernalFuzz, h.fuzz).set (ParamIDs::infernalCrush, h.crush).set (ParamIDs::infernalAmp, h.amp)
         .set (ParamIDs::infernalNoise, h.noise).set (ParamIDs::infernalFeedback, h.feedback).set (ParamIDs::infernalCab, h.cab)
         .set (ParamIDs::infernalMix, h.mix).set (ParamIDs::infernalLevel, h.levelDb);
        if (h.mixRise > 0.0f)
            route (b, "macro3", ParamIDs::infernalMix, h.mixRise);
        if (h.driveRise > 0.0f)
            route (b, "macro3", ParamIDs::infernalDrive, h.driveRise);
        if (h.noiseRise > 0.0f)
            route (b, "macro3", ParamIDs::infernalNoise, h.noiseRise);
    }

    // HELLFORGE: inspired by modern industrial-metal game scores (the owner's references: synths as powerful and
    // distorted as down-tuned guitars, leads played like a guitar, clean subs under noise that riffs, a screaming
    // choir, a cathedral organ, a chainsaw and a kick). No names or music from any game: our own sounds. 0.59.0 (the
    // owner's ask): rebuilt round INFERNAL; Gut Chug, Tectonic Drop, Iron Riff and Gatling Pulse kept as they were.
    void hellforge (Expansion x)
    {
        using DM = effects::Drive::Model;
        constexpr int fet = 2;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto lead = x.make ("Hellfire Lead", 1, { "Lead", "Guitar", "Infernal", "Mono" },
            "A lead played like a guitar: a saw and a narrow pulse, a little detuned, through INFERNAL's AMP into its "
            "speaker cabinet, held notes blooming with feedback an octave up, vibrato coming in late, a short glide, and "
            "the pitch wheel diving a whole octave (bend down 12) for whammy dives. The MOD WHEEL feeds back harder. "
            "Dotted-eighth echoes and a hall after the amp. MACROS: BRIGHT, SPACE, DESTROY (more drive, a growl "
            "of pitched noise).");
        lead.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.35f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.4f)
            .osc (0, "Volume", 0.6f).osc (0, "VibAmount", 22.0f).osc (0, "VibRate", 5.6f).osc (0, "VibFade", 350.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.4f)
            .osc (1, "VibAmount", 22.0f).osc (1, "VibRate", 5.6f).osc (1, "VibFade", 350.0f)
            .filter (FilterType::lp24, 7000.0f, 0.1f, 0.15f).filterEnvelope (0.0f, 300.0f, 0.7f, 200.0f)
            .ampEnvelope (3.0f, 300.0f, 0.95f, 250.0f)
            .mono (25.0f).set (ParamIDs::bendDown, 12.0f).set (ParamIDs::bendUp, 2.0f);
        { Hell h; h.amp = 1.0f; h.fuzz = 0.3f; h.noise = 0.0f; h.feedback = 0.55f; h.cab = 0.45f; h.drive = 0.55f; h.tight = 0.35f; hell (lead, h); }
        route (lead, "modWheel", ParamIDs::infernalFeedback, 0.45f);
        route (lead, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (lead, "BRIGHT", "SPACE", "DESTROY");
        delay (lead, "1/8 dotted", 0.3f, 0.15f, true, 4.0f);
        reverb (lead, RT::hall, 0.55f, 0.15f);
        space (lead);
        lead.set (ParamIDs::patchLevel, 0.0f);
        x.add (lead);

        auto wraith = x.make ("Whammy Wraith", 2, { "Lead", "Fuzz", "Infernal", "Mono" },
            "A squealing, fuzzed lead: a narrow pulse with a saw an octave up for the harmonics, through INFERNAL's FUZZ "
            "and AMP, feedback swelling fast on held notes, the pitch wheel diving a whole octave and screaming up one. "
            "The MOD WHEEL widens the vibrato into a wail. MACROS: BRIGHT, SPACE, DESTROY.");
        wraith.osc (0, "Wave", pulse).osc (0, "Width", 0.18f).osc (0, "Volume", 0.6f)
              .osc (0, "VibAmount", 15.0f).osc (0, "VibRate", 6.2f).osc (0, "VibFade", 250.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f)
              .filter (FilterType::lp24, 6000.0f, 0.2f, 0.0f)
              .ampEnvelope (2.0f, 300.0f, 0.95f, 300.0f)
              .mono (40.0f).set (ParamIDs::bendDown, 12.0f).set (ParamIDs::bendUp, 12.0f);
        { Hell h; h.fuzz = 1.0f; h.amp = 0.5f; h.noise = 0.0f; h.feedback = 0.8f; h.cab = 0.7f; h.drive = 0.6f; h.tight = 0.4f; hell (wraith, h); }
        route (wraith, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.35f);
        route (wraith, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (wraith, "BRIGHT", "SPACE", "DESTROY");
        delay (wraith, "1/4", 0.35f, 0.15f, true, 4.0f);
        reverb (wraith, RT::hall, 0.6f, 0.18f);
        space (wraith);
        wraith.set (ParamIDs::patchLevel, -1.0f);
        x.add (wraith);

        auto plasma = x.make ("Plasma Scream", 3, { "Lead", "Sync", "Infernal", "Crush" },
            "A digital scream: a saw hard-synced, its sync sweeping down as each note starts, through INFERNAL's AMP with "
            "its CRUSH grinding underneath, like a guitar through broken electronics. The MOD WHEEL pushes the sync. "
            "MACROS: BRIGHT, SPACE, DESTROY.");
        plasma.osc (0, "SyncAmount", 0.3f).osc (0, "SyncEnv", 0.35f).osc (0, "SyncEnvTime", 300.0f).osc (0, "Volume", 0.6f)
              .osc (0, "VibAmount", 18.0f).osc (0, "VibRate", 5.8f).osc (0, "VibFade", 300.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
              .filter (FilterType::lp24, 6500.0f, 0.15f, 0.0f)
              .ampEnvelope (2.0f, 300.0f, 0.9f, 220.0f)
              .mono (30.0f).set (ParamIDs::bendDown, 12.0f);
        { Hell h; h.amp = 0.9f; h.crush = 0.35f; h.fuzz = 0.2f; h.noise = 0.15f; h.feedback = 0.3f; h.cab = 0.6f; h.drive = 0.5f; hell (plasma, h); }
        route (plasma, "modWheel", ParamIDs::osc (0, "SyncAmount"), 0.35f);
        route (plasma, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (plasma, "BRIGHT", "SPACE", "DESTROY");
        delay (plasma, "1/8", 0.3f, 0.12f, true, 5.0f);
        reverb (plasma, RT::plate, 0.5f, 0.15f);
        space (plasma);
        plasma.set (ParamIDs::patchLevel, 0.0f);
        x.add (plasma);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto screamers = x.make ("Screamer Choir", 4, { "Pad", "Voice", "Choir", "Infernal" },
            "A choir of metal screamers: buzzing saws and a breath of noise sung through the VOICE filter, each note "
            "scooping up into pitch as a scream does and the vowel tearing open from \"oo\" to \"ah\", every voice its "
            "own vibrato, through INFERNAL's FUZZ and pitched NOISE so the voices rasp. Hold chords low. MACROS: BRIGHT, "
            "SPACE, DESTROY (rawer throats).");
        screamers.osc (0, "UniVoices", 6.0f).osc (0, "UniMix", 0.75f).osc (0, "UniDetune", 0.22f).osc (0, "UniSpread", 1.0f)
                 .osc (0, "Volume", 0.5f).osc (0, "VibMode", (float) synth::VibratoMode::independent)
                 .osc (0, "VibAmount", 30.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 200.0f)
                 .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.25f).osc (1, "NoiseColour", 0.1f).osc (1, "Volume", 1.0f)
                 .filter (FilterType::lp12, 4500.0f, 0.1f, 0.0f)
                 .ampEnvelope (60.0f, 800.0f, 0.9f, 900.0f)
                 .set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceVowel, 0.0f).set (ParamIDs::voiceSize, 0.3f)
                 .set (ParamIDs::voiceSharp, 0.6f).set (ParamIDs::voiceMix, 1.0f)
                 .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 4.0f)
                 .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        screamers.rig.modulation.pitchEnvelope = { { SP { 0.0f, 0.0f, -0.4f }, SP { 0.25f, 0.5f, 0.0f } }, -1, -1, -1 };
        screamers.rig.modulation.envelopes[0] = { { SP { 0.0f, 0.0f, 0.3f }, SP { 0.35f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (screamers, "env1", ParamIDs::voiceVowel, 0.5f);
        { Hell h; h.fuzz = 0.5f; h.amp = 0.3f; h.noise = 0.35f; h.feedback = 0.0f; h.mix = 0.4f; h.tight = 0.5f; h.mixRise = 0.4f; hell (screamers, h); }
        route (screamers, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (screamers, "BRIGHT", "SPACE", "DESTROY");
        reverb (screamers, RT::cathedral, 0.5f, 0.3f, 30.0f, 4.0f);
        space (screamers);
        screamers.set (ParamIDs::patchLevel, -4.5f);
        x.add (screamers);

        auto drone = x.make ("Hellfire Drone", 5, { "Pad", "Sine", "Noise", "Infernal" },
            "The machine idling: two clean sines an octave apart, the sub kept clean under INFERNAL while the rest goes "
            "through FUZZ, CRUSH and NOISE pulsed at the notes' pitch, so the drone growls in tune; LFO 1 slowly breathes "
            "the drive. Hold one low note or a fifth. MACROS: BRIGHT, SPACE, DESTROY.");
        drone.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.45f)
             .osc (2, "On", 1.0f).osc (2, "Wave", saw).osc (2, "Transpose", 19.0f).osc (2, "Volume", 0.08f)
             .ampEnvelope (900.0f, 2000.0f, 1.0f, 2500.0f)
             .set (ParamIDs::lfo (0, "Rate"), 0.12f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        { Hell h; h.fuzz = 0.8f; h.crush = 0.3f; h.amp = 0.3f; h.noise = 1.0f; h.feedback = 0.0f; h.mix = 0.85f; h.tight = 0.55f; hell (drone, h); }
        route (drone, "lfo1", ParamIDs::infernalDrive, 0.12f);
        route (drone, "macro1", ParamIDs::osc (2, "Volume"), 0.15f);
        macros (drone, "BRIGHT", "SPACE", "DESTROY");
        reverb (drone, RT::hall, 0.7f, 0.25f);
        space (drone);
        drone.set (ParamIDs::patchLevel, -4.0f);
        x.add (drone);

        auto shadow = x.make ("Riff Shadow", 6, { "Pad", "Layer", "Guitar", "Infernal" },
            "A layer to sit under a down-tuned guitar: a wide saw an octave down and a clean sine sub, through INFERNAL's "
            "AMP and its cabinet so it blends with the guitar's own, the lows kept clean and tight. Play it with the "
            "guitar's riff or hold the chord under it. MACROS: BRIGHT, SPACE, DESTROY.");
        shadow.osc (0, "Transpose", -12.0f).osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.12f)
              .osc (0, "UniSpread", 0.9f).osc (0, "Volume", 0.5f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
              .filter (FilterType::lp24, 3000.0f, 0.1f, 0.2f).filterEnvelope (0.0f, 250.0f, 0.6f, 150.0f)
              .ampEnvelope (4.0f, 300.0f, 0.9f, 200.0f);
        { Hell h; h.amp = 0.8f; h.fuzz = 0.3f; h.noise = 0.1f; h.feedback = 0.0f; h.cab = 0.35f; h.tight = 0.6f; h.mix = 0.7f; h.drive = 0.45f; hell (shadow, h); }
        compressor (shadow, fet, -18.0f, 4.0f, 2.0f);
        route (shadow, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (shadow, "BRIGHT", "SPACE", "DESTROY");
        reverb (shadow, RT::room, 0.4f, 0.1f);
        space (shadow);
        shadow.set (ParamIDs::patchLevel, 3.0f);
        x.add (shadow);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto chug = x.make ("Gut Chug", 7, { "Bass", "Drive", "Mono", "Inserts" },
            "A down-tuned palm-muted chug: a saw and a pulse an octave up (the pick's edge) through insert bus A's RABID, "
            "plucked short by the filter, every note struck afresh, and a clean sine an octave down under it, straight to "
            "the mix. CONSTRICTOR (FET) slams it, ECHELON scoops the mids. Play it in sixteenths. MACROS: BRIGHT, SPACE, "
            "DESTROY (INFERNAL comes in under it, the sub kept clean).");
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
        hell (chug, Hell::keep());
        chug.set (ParamIDs::patchLevel, 4.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chug);

        auto drop = x.make ("Tectonic Drop", 8, { "Bass", "Hit", "Pitch", "Sub" },
            "A sub drop for the moment before everything hits: a sine falling two octaves into its note over about a "
            "second (the PITCH envelope), a saw an octave up giving it a growl through SHAPER 1's tube, ringing three "
            "seconds into a hall. MACROS: BRIGHT, SPACE, DESTROY (INFERNAL comes in, the sub kept clean).");
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
        hell (drop, Hell::keep());
        drop.set (ParamIDs::patchLevel, -4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (drop);

        auto engine = x.make ("Sine Engine", 9, { "Bass", "Sine", "Noise", "Infernal" },
            "The engine of the whole bank: two pure sines, the sub kept clean while the one an octave up goes through "
            "INFERNAL's FUZZ and CRUSH, with noise pulsed at the note's pitch growling on top in time with the riff. "
            "CONSTRICTOR (FET) slams it. Play riffs in sixteenths. The MOD WHEEL brings up the growl. MACROS: BRIGHT, "
            "SPACE, DESTROY.");
        engine.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.6f).osc (1, "PhaseRand", 0.0f)
              .ampEnvelope (1.0f, 200.0f, 0.85f, 60.0f)
              .mono (0.0f).set (ParamIDs::legato, 0.0f);
        { Hell h; h.fuzz = 1.0f; h.crush = 0.25f; h.amp = 0.4f; h.noise = 0.7f; h.feedback = 0.0f; h.tight = 0.45f; h.drive = 0.6f; hell (engine, h); }
        compressor (engine, fet, -18.0f, 4.0f, 2.0f);
        route (engine, "modWheel", ParamIDs::infernalNoise, 0.3f);
        route (engine, "macro1", ParamIDs::osc (1, "Volume"), 0.2f);
        macros (engine, "BRIGHT", "SPACE", "DESTROY");
        reverb (engine, RT::room, 0.3f, 0.06f);
        space (engine);
        engine.set (ParamIDs::patchLevel, 3.0f);
        x.add (engine);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto organ = x.make ("Profane Organ", 10, { "Keys", "Organ", "Cathedral", "Infernal" },
            "An enormous church organ: the Organ Stack table with a pipe an octave down and a high fifth above, speaking "
            "slowly and ringing into a vast cathedral. Clean as it comes; DESTROY drags it through INFERNAL into a hellish "
            "organ. MACROS: BRIGHT, SPACE, DESTROY.");
        organ.osc (0, "Wave", wavetable).osc (0, "Table", table ("Organ Stack")).osc (0, "WtIndex", 0.7f)
             .osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.35f).osc (0, "UniDetune", 0.05f).osc (0, "Volume", 0.55f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.4f)
             .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Transpose", 19.0f).osc (2, "Volume", 0.12f)
             .filter (FilterType::lp12, 6000.0f, 0.05f, 0.0f)
             .ampEnvelope (40.0f, 200.0f, 1.0f, 1200.0f);
        { Hell h; h.mix = 0.0f; h.mixRise = 0.8f; h.amp = 0.8f; h.fuzz = 0.5f; h.noise = 0.2f; h.feedback = 0.2f; h.tight = 0.5f; hell (organ, h); }
        route (organ, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (organ, "BRIGHT", "SPACE", "DESTROY");
        reverb (organ, RT::cathedral, 0.9f, 0.4f, 40.0f, 4.0f);
        space (organ);
        organ.set (ParamIDs::patchLevel, -9.0f);
        x.add (organ);

        auto anvil = x.make ("Anvil Stab", 11, { "Keys", "Stab", "Filter", "Infernal" },
            "A filter-driven stab: saws and a pulse snapped open by the filter and shut again, through INFERNAL's FUZZ and "
            "AMP, so every chord hits like a hammer on an anvil. Harder playing snaps brighter. MACROS: BRIGHT, SPACE, "
            "DESTROY.");
        anvil.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.45f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.7f)
             .osc (0, "Volume", 0.55f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.35f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
             .filter (FilterType::lp24, 400.0f, 0.35f, 0.75f).filterEnvelope (0.0f, 180.0f, 0.0f, 120.0f)
             .set (ParamIDs::fltVel, 0.4f).set (ParamIDs::fltEnvVel, 0.5f)
             .ampEnvelope (1.0f, 400.0f, 0.0f, 200.0f);
        { Hell h; h.fuzz = 0.8f; h.amp = 0.6f; h.noise = 0.1f; h.feedback = 0.0f; h.mix = 0.8f; hell (anvil, h); }
        route (anvil, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (anvil, "BRIGHT", "SPACE", "DESTROY");
        delay (anvil, "1/8 dotted", 0.25f, 0.1f, true, 4.0f);
        reverb (anvil, RT::hall, 0.5f, 0.15f);
        space (anvil);
        anvil.set (ParamIDs::patchLevel, -2.5f);
        x.add (anvil);

        auto riff2 = x.make ("Iron Riff", 12, { "Keys", "Chords", "Drive", "Inserts" },
            "Gatling Pulse played by hand: every key gives a whole power chord (the oscillator's chorder adds the fifth "
            "and the octave) with a clean sine sub an octave under it, through insert bus A's RABID, so one finger plays "
            "a down-tuned rhythm-guitar chord. Short notes snap like a palm mute; held ones ring. CONSTRICTOR (FET) "
            "slams it, ECHELON scoops the mids. MACROS: BRIGHT, SPACE, DESTROY (INFERNAL comes in, the sub kept clean).");
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
        hell (riff2, Hell::keep());
        riff2.set (ParamIDs::patchLevel, 5.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (riff2);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto gatling = x.make ("Gatling Pulse", 13, { "Arp", "Chords", "Drive", "Chug" },
            "Hold one low key (or a chord: its root is used): ARP 1 fires power chords with a sub under them in a "
            "syncopated sixteenth-note chug, with a dissonant half-step stab near the end of the bar, through insert bus "
            "A's RABID, in time with your song. CONSTRICTOR (FET) slams it. MACROS: BRIGHT, SPACE, DESTROY (INFERNAL comes in, the sub kept clean).");
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
        hell (gatling, Hell::keep());
        gatling.set (ParamIDs::patchLevel, 6.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (gatling);

        auto chainsaw = x.make ("Chainsaw Pulse", 14, { "Arp", "Noise", "Kick", "Infernal" },
            "A chainsaw and a kick: hold a low key and ARP 1 fires a sine that drops two octaves into its note on every "
            "beat (the kick) while a buzzing chainsaw idles between them in sixteenths, all through INFERNAL's FUZZ and "
            "pitched NOISE, so the engine rattles in time with your song. MACROS: BRIGHT, SPACE, DESTROY.");
        chainsaw.osc (0, "Wave", sine).osc (0, "Volume", 0.8f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
           .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f).osc (1, "PitchEnv", 0.0f)
           .osc (1, "Transpose", 12.0f)
           .ampEnvelope (0.5f, 140.0f, 0.0f, 60.0f)
           .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 24.0f)
           .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
           .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Gate"), 0.7f);
        chainsaw.rig.modulation.pitchEnvelope = { { SP { 0.0f, 1.0f, -0.6f }, SP { 0.06f, 0.5f, 0.0f } }, -1, -1, -1 };
        {
            auto& p = chainsaw.rig.modulation.arps[0].patterns[0];
            p = {};
            for (int k = 0; k < 16; ++k)
            {
                auto& blk = p.blocks[(size_t) k];
                blk.on = true;
                blk.velocity = k % 4 == 0 ? 1.0f : 0.55f;
            }
        }
        { Hell h; h.fuzz = 0.9f; h.noise = 0.6f; h.amp = 0.3f; h.crush = 0.2f; h.feedback = 0.0f; h.tight = 0.5f; h.mix = 0.8f; hell (chainsaw, h); }
        route (chainsaw, "macro1", ParamIDs::osc (1, "Volume"), 0.2f);
        macros (chainsaw, "BRIGHT", "SPACE", "DESTROY");
        reverb (chainsaw, RT::room, 0.4f, 0.1f);
        space (chainsaw);
        chainsaw.set (ParamIDs::patchLevel, 2.0f);
        x.add (chainsaw);

        auto run = x.make ("Berserk Run", 15, { "Arp", "Shred", "Guitar", "Infernal" },
            "Shredding runs: hold a chord and ARP 1 tears up and down it over two octaves in sixteenths, a lead played "
            "like a guitar through INFERNAL's AMP and cabinet, every note picked, in time with your song. The MOD WHEEL "
            "feeds back. MACROS: BRIGHT, SPACE, DESTROY.");
        run.osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f)
           .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.35f).osc (1, "Arp", 1.0f)
           .filter (FilterType::lp24, 5000.0f, 0.15f, 0.3f).filterEnvelope (0.0f, 120.0f, 0.4f, 80.0f)
           .ampEnvelope (1.0f, 160.0f, 0.6f, 60.0f)
           .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
           .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
           .set (ParamIDs::arp (0, "Gate"), 0.75f);
        { Hell h; h.amp = 1.0f; h.fuzz = 0.3f; h.noise = 0.0f; h.feedback = 0.3f; h.cab = 0.5f; h.drive = 0.55f; hell (run, h); }
        route (run, "modWheel", ParamIDs::infernalFeedback, 0.4f);
        route (run, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (run, "BRIGHT", "SPACE", "DESTROY");
        delay (run, "1/8 dotted", 0.3f, 0.12f, true, 4.0f);
        reverb (run, RT::hall, 0.5f, 0.12f);
        space (run);
        run.set (ParamIDs::patchLevel, 0.0f);
        x.add (run);
    }

    // STARCOURT (the owner's name): inspired by the dark '80s analog-synth score of a supernatural mystery series:
    // rising arpeggios over minor chords, detuned saws and squares, chorused polysynth pads, simple haunting leads,
    // cassette and VHS wobble, big '80s drums. No titles, characters or music from the series: our own sounds.
    // STARCOURT's centrepiece (0.60.0, the owner's ask: "exaggerate the 1980s glorious synths"): AFTERGLOW, an old
    // polysynth's glow. `afterglow` sets it up and routes MACRO 3, GLOW, on every preset: AGE, DRIFT, ENSEMBLE and TAPE
    // up together (for the five kept presets, from 0: the sound as approved). Their old MACRO 3 jobs ride along.
    struct Glow
    {
        float age = 0.35f, drift = 0.3f, ensemble = 0.45f, tape = 0.2f;
        float ageRise = 0.6f, driftRise = 0.6f, ensembleRise = 0.5f, tapeRise = 0.5f;   // MACRO 3's routes
        // The five kept from 0.27.0: nothing at the macro's left, the full glow at its right.
        static Glow keep() { return { 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.8f, 0.75f, 0.55f }; }
    };

    void afterglow (Builder& b, Glow g)
    {
        b.set (ParamIDs::afterglowOn, 1.0f).set (ParamIDs::afterglowAge, g.age).set (ParamIDs::afterglowDrift, g.drift)
         .set (ParamIDs::afterglowEnsemble, g.ensemble).set (ParamIDs::afterglowTape, g.tape);
        route (b, "macro3", ParamIDs::afterglowAge, g.ageRise);
        route (b, "macro3", ParamIDs::afterglowDrift, g.driftRise);
        route (b, "macro3", ParamIDs::afterglowEnsemble, g.ensembleRise);
        route (b, "macro3", ParamIDs::afterglowTape, g.tapeRise);
    }

    void starcourt (Expansion x)
    {
        using SP2 = synth::mod::ShapePoint;
        (void) sizeof (SP2);

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto hero = x.make ("Arcade Hero", 1, { "Lead", "Pulse", "PWM", "Mono" },
            "A bright square lead from an '80s mono synth: LFO 1 slowly sweeping the pulse width (PWM) so it shimmers, "
            "a quiet square an octave down, a short glide, vibrato coming in late. Dotted-eighth echoes and a hall. "
            "MACROS: BRIGHT, SPACE, GLOW (AFTERGLOW comes in, and a thinner pulse).");
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
        macros (hero, "BRIGHT", "SPACE", "GLOW");
        chorus (hero, ParamIDs::modIds, MT::chorus, 0.5f, 0.3f, 0.3f);
        delay (hero, "1/8 dotted", 0.35f, 0.2f, true, 5.0f);
        reverb (hero, RT::hall, 0.5f, 0.2f);
        space (hero);
        afterglow (hero, Glow::keep());
        hero.set (ParamIDs::patchLevel, -6.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hero);

        auto mixtape = x.make ("Mixtape Lead", 2, { "Lead", "Saw", "Afterglow", "Mono" },
            "The big soaring lead of an '80s synth score: two saws a hair apart over a square an octave down, the filter "
            "opening as each note speaks, a lazy glide and vibrato that blooms late, every note a little its own "
            "(AFTERGLOW) into a wide ensemble, dotted-eighth echoes and a hall. MACROS: BRIGHT, SPACE, GLOW (older, "
            "wider, wobblier).");
        mixtape.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.5f)
               .osc (0, "Volume", 0.55f).osc (0, "VibAmount", 18.0f).osc (0, "VibRate", 5.2f).osc (0, "VibFade", 500.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
               .filter (FilterType::lp24, 2800.0f, 0.25f, 0.3f).filterEnvelope (30.0f, 600.0f, 0.6f, 400.0f)
               .ampEnvelope (8.0f, 400.0f, 0.9f, 400.0f)
               .mono (70.0f);
        route (mixtape, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (mixtape, "BRIGHT", "SPACE", "GLOW");
        afterglow (mixtape, { 0.35f, 0.3f, 0.45f, 0.25f, 0.6f, 0.6f, 0.5f, 0.5f });
        delay (mixtape, "1/8 dotted", 0.35f, 0.2f, true, 5.0f);
        reverb (mixtape, RT::hall, 0.6f, 0.22f);
        space (mixtape);
        mixtape.set (ParamIDs::patchLevel, -6.0f);   // LEVEL: measured, not by ear
        x.add (mixtape);

        auto ghost = x.make ("Radio Ghost", 3, { "Lead", "Triangle", "Drift", "Mono" },
            "A thin, haunted voice from a radio left on in an empty house: a triangle with a faint sine an octave up, a "
            "slow glide, deep vibrato, and DRIFT pulling every note slightly off as an old synth's tuning wanders, worn "
            "on tape. MACROS: BRIGHT, SPACE, GLOW (stranger, more worn).");
        ghost.osc (0, "Wave", triangle).osc (0, "Volume", 0.8f).osc (0, "VibAmount", 28.0f).osc (0, "VibRate", 4.6f).osc (0, "VibFade", 250.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
             .osc (1, "VibAmount", 28.0f).osc (1, "VibRate", 4.6f).osc (1, "VibFade", 250.0f)
             .ampEnvelope (40.0f, 400.0f, 0.9f, 600.0f)
             .mono (140.0f);
        route (ghost, "macro1", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (ghost, "BRIGHT", "SPACE", "GLOW");
        afterglow (ghost, { 0.4f, 0.6f, 0.3f, 0.4f, 0.6f, 0.4f, 0.5f, 0.45f });
        delay (ghost, "1/4", 0.4f, 0.18f, true, 4.0f);
        reverb (ghost, RT::plate, 0.75f, 0.3f);
        space (ghost);
        ghost.set (ParamIDs::patchLevel, -3.3f);   // LEVEL: measured, not by ear
        x.add (ghost);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto glow = x.make ("Basement Glow", 4, { "Pad", "Chorus", "Unison", "Polysynth" },
            "A warm '80s polysynth pad: detuned saws over a square an octave down, the filter easing open as the chord "
            "swells in, through a lush, wide chorus. MACROS: BRIGHT, SPACE, GLOW (AFTERGLOW comes in, and a deeper chorus).");
        glow.osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.2f).osc (0, "UniSpread", 0.8f)
            .osc (0, "Volume", 0.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.25f)
            .filter (FilterType::lp12, 1800.0f, 0.1f, 0.15f).filterEnvelope (600.0f, 1500.0f, 0.7f, 1500.0f)
            .set (ParamIDs::fltKey, 0.4f)
            .ampEnvelope (500.0f, 1500.0f, 0.9f, 1500.0f);
        route (glow, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (glow, "macro3", ParamIDs::modDepth, 0.4f);
        macros (glow, "BRIGHT", "SPACE", "GLOW");
        chorus (glow, ParamIDs::modIds, MT::chorus, 0.5f, 0.55f, 0.6f);
        glow.set (ParamIDs::modStereo, 1.0f);
        reverb (glow, RT::hall, 0.6f, 0.25f);
        space (glow);
        afterglow (glow, Glow::keep());
        glow.set (ParamIDs::patchLevel, -10.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glow);

        auto strings = x.make ("Dusk Strings", 5, { "Pad", "Strings", "Ensemble", "PWM" },
            "A string machine at dusk: two pulses an octave apart, their widths swept slowly by LFO 1 so they shimmer, "
            "swelling in and out, through AFTERGLOW's ENSEMBLE: the lush three-voice chorus those machines were made "
            "of. MACROS: BRIGHT, SPACE, GLOW (a huger ensemble, older voices).");
        strings.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.5f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.3f)
               .filter (FilterType::lp12, 3200.0f, 0.05f, 0.0f)
               .ampEnvelope (450.0f, 1000.0f, 0.9f, 1300.0f)
               .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle).set (ParamIDs::lfo (0, "Rate"), 0.35f)
               .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (strings, "lfo1", ParamIDs::osc (0, "Width"), 0.3f);
        route (strings, "lfo1", ParamIDs::osc (1, "Width"), -0.3f);
        route (strings, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (strings, "BRIGHT", "SPACE", "GLOW");
        afterglow (strings, { 0.3f, 0.25f, 0.7f, 0.2f, 0.6f, 0.6f, 0.3f, 0.45f });
        reverb (strings, RT::hall, 0.7f, 0.25f);
        space (strings);
        strings.set (ParamIDs::patchLevel, -9.8f);   // LEVEL: measured, not by ear
        x.add (strings);

        auto dream = x.make ("Polysynth Dream", 6, { "Pad", "Saw", "Afterglow", "Swell" },
            "The full-blown '80s polysynth chord: two saws per voice drifting against each other, the filter swelling "
            "open slowly and settling, each voice its own tuning and brightness (AGE) so the chord breathes, into the "
            "ensemble and a vast hall. Hold big chords. MACROS: BRIGHT, SPACE, GLOW.");
        dream.osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.8f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.7f).osc (0, "Volume", 0.45f)
             .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Fine", 7.0f).osc (1, "Volume", 0.35f)
             .filter (FilterType::lp24, 900.0f, 0.2f, 0.45f).filterEnvelope (1200.0f, 2500.0f, 0.55f, 1800.0f)
             .ampEnvelope (700.0f, 1500.0f, 0.95f, 1800.0f);
        route (dream, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (dream, "BRIGHT", "SPACE", "GLOW");
        afterglow (dream, { 0.5f, 0.4f, 0.5f, 0.3f, 0.5f, 0.5f, 0.5f, 0.4f });
        reverb (dream, RT::hall, 0.8f, 0.3f);
        space (dream);
        dream.set (ParamIDs::patchLevel, -7.3f);   // LEVEL: measured, not by ear
        x.add (dream);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto driveSub = x.make ("Night Drive Sub", 7, { "Bass", "Sub", "Mono", "Analog" },
            "A round analog-style bass: a saw darkened by a resonant low-pass with its sub, a little pluck at the start "
            "of each note, a short glide. CONSTRICTOR (OPTO) holds it steady. MACROS: BRIGHT, SPACE, GLOW (AFTERGLOW comes in, with growl: the filter's "
            "drive).");
        driveSub.osc (0, "Volume", 0.6f).osc (0, "Sub", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "SendReverb", 0.2f)
                .osc (0, "SendDelay", 0.0f)
                .filter (FilterType::lp24, 400.0f, 0.3f, 0.25f).filterEnvelope (0.0f, 300.0f, 0.3f, 150.0f)
                .ampEnvelope (2.0f, 400.0f, 0.8f, 150.0f)
                .mono (30.0f);
        compressor (driveSub, 1, -20.0f, 3.0f, 2.0f);
        route (driveSub, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (driveSub, "macro3", ParamIDs::fltDrive, 0.5f);
        macros (driveSub, "BRIGHT", "SPACE", "GLOW");
        reverb (driveSub, RT::room, 0.3f, 0.08f);
        space (driveSub);
        afterglow (driveSub, Glow::keep());
        driveSub.set (ParamIDs::patchLevel, 5.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (driveSub);

        auto eighths = x.make ("Eighth Note Pulse", 8, { "Bass", "Pulse", "Pluck", "Afterglow" },
            "The driving eighth-note bass under every '80s synth score: a narrow pulse over a clean sine sub, plucked "
            "by the filter, each note a touch different in pitch and brightness (AGE) as an analog bass was, on tape. "
            "Play it in eighths. MACROS: BRIGHT, SPACE, GLOW.");
        eighths.osc (0, "Wave", pulse).osc (0, "Width", 0.32f).osc (0, "Volume", 0.5f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.5f).osc (1, "Filter", 0.0f)
               .filter (FilterType::lp24, 500.0f, 0.35f, 0.6f).filterEnvelope (0.0f, 180.0f, 0.1f, 100.0f)
               .ampEnvelope (1.0f, 250.0f, 0.4f, 80.0f)
               .mono (0.0f).set (ParamIDs::legato, 0.0f);
        route (eighths, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (eighths, "BRIGHT", "SPACE", "GLOW");
        afterglow (eighths, { 0.3f, 0.2f, 0.15f, 0.3f, 0.6f, 0.5f, 0.3f, 0.5f });
        reverb (eighths, RT::room, 0.3f, 0.06f);
        space (eighths);
        eighths.set (ParamIDs::patchLevel, -1.9f);   // LEVEL: measured, not by ear
        x.add (eighths);

        auto wall = x.make ("Saw Wall Bass", 9, { "Bass", "Saw", "Unison", "Afterglow" },
            "A wide wall of saws an octave down under a low saw, the filter falling after each note, every note its own "
            "(AGE) and drifting slowly: the big analog bass that holds the chords up. MACROS: BRIGHT, SPACE, GLOW.");
        wall.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.5f).osc (0, "Volume", 0.45f)
            .osc (1, "On", 1.0f).osc (1, "Wave", saw).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.45f)
            .filter (FilterType::lp24, 800.0f, 0.15f, 0.35f).filterEnvelope (0.0f, 500.0f, 0.4f, 200.0f)
            .ampEnvelope (2.0f, 600.0f, 0.75f, 180.0f)
            .mono (25.0f);
        route (wall, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (wall, "BRIGHT", "SPACE", "GLOW");
        afterglow (wall, { 0.35f, 0.3f, 0.25f, 0.35f, 0.6f, 0.5f, 0.4f, 0.45f });
        reverb (wall, RT::room, 0.35f, 0.08f);
        space (wall);
        wall.set (ParamIDs::patchLevel, 0.4f);   // LEVEL: measured, not by ear
        x.add (wall);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto lantern = x.make ("Lantern Keys", 10, { "Keys", "FM", "Electric Piano", "Afterglow" },
            "An '80s FM electric piano, warm and glassy: the strike bright and settling, brighter when played harder, "
            "each note a little its own (AGE), through the ensemble and onto tape. MACROS: BARK (the FM), SPACE, GLOW.");
        lantern.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
               .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.1f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
               .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.12f)
               .ampEnvelope (2.0f, 2200.0f, 0.25f, 450.0f).set (ParamIDs::ampVel, 0.6f)
               .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::sustain);
        lantern.rig.modulation.envelopes[0] = { { SP2 { 0.0f, 1.0f, -0.5f }, SP2 { 0.8f, 0.2f, 0.0f } }, 1, -1, -1 };
        route (lantern, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (lantern, "velocity", ParamIDs::osc (0, "FmAmount"), 0.18f);
        route (lantern, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        macros (lantern, "BARK", "SPACE", "GLOW");
        afterglow (lantern, { 0.3f, 0.2f, 0.45f, 0.3f, 0.6f, 0.5f, 0.5f, 0.45f });
        reverb (lantern, RT::plate, 0.5f, 0.2f);
        space (lantern);
        lantern.set (ParamIDs::patchLevel, -5.2f);   // LEVEL: measured, not by ear
        x.add (lantern);

        auto stabs = x.make ("Neon Stabs", 11, { "Keys", "Stab", "Polysynth", "Afterglow" },
            "Bright polysynth stabs: saws and a pulse snapped open by the filter and shut again, harder playing snapping "
            "brighter, every note its own (AGE) so the chords shimmer, through the ensemble and dotted-eighth echoes. "
            "MACROS: BRIGHT, SPACE, GLOW.");
        stabs.osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.6f).osc (0, "Volume", 0.5f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Volume", 0.3f)
             .filter (FilterType::lp24, 700.0f, 0.3f, 0.6f).filterEnvelope (0.0f, 220.0f, 0.15f, 150.0f)
             .set (ParamIDs::fltEnvVel, 0.5f)
             .ampEnvelope (2.0f, 500.0f, 0.3f, 250.0f);
        route (stabs, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (stabs, "BRIGHT", "SPACE", "GLOW");
        afterglow (stabs, { 0.4f, 0.25f, 0.5f, 0.25f, 0.6f, 0.5f, 0.45f, 0.45f });
        delay (stabs, "1/8 dotted", 0.3f, 0.15f, true, 5.0f);
        reverb (stabs, RT::hall, 0.5f, 0.15f);
        space (stabs);
        stabs.set (ParamIDs::patchLevel, -4.9f);   // LEVEL: measured, not by ear
        x.add (stabs);

        auto brass = x.make ("Brass '84", 12, { "Keys", "Brass", "Polysynth" },
            "A punchy '80s polysynth brass for chord stabs: three detuned saws and a pulse, the filter jumping open on "
            "each chord and settling, more with velocity, through a chorus. MACROS: BRIGHT, SPACE, GLOW (AFTERGLOW comes in, with bite: a bigger "
            "filter jump).");
        brass.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.45f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.5f)
             .osc (0, "Volume", 0.5f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.35f)
             .filter (FilterType::lp24, 900.0f, 0.15f, 0.45f).filterEnvelope (20.0f, 300.0f, 0.4f, 300.0f)
             .set (ParamIDs::fltVel, 0.4f).set (ParamIDs::fltKey, 0.4f)
             .ampEnvelope (5.0f, 500.0f, 0.6f, 200.0f);
        route (brass, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (brass, "macro3", ParamIDs::fltEnv, 0.3f);
        macros (brass, "BRIGHT", "SPACE", "GLOW");
        chorus (brass, ParamIDs::modIds, MT::chorus, 0.6f, 0.4f, 0.4f);
        brass.set (ParamIDs::modStereo, 1.0f);
        reverb (brass, RT::hall, 0.5f, 0.2f);
        space (brass);
        afterglow (brass, Glow::keep());
        brass.set (ParamIDs::patchLevel, -6.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (brass);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto ostinato = x.make ("Dark Ostinato", 13, { "Arp", "Sequence", "Filter", "Sync" },
            "Hold a minor chord: ARP 1 climbs it over two octaves in steady eighth notes on a saw and a thin square an "
            "octave up, through a resonant filter that LFO 1 opens slowly over four bars and closes again, in time with "
            "your song. Dotted-eighth echoes. MACROS: BRIGHT, SPACE, GLOW (AFTERGLOW comes in, and more resonance).");
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
        route (ostinato, "macro3", ParamIDs::fltRes, 0.18f);   // 0.60.0: about halved: GLOW adds level of its own
        macros (ostinato, "BRIGHT", "SPACE", "GLOW");
        delay (ostinato, "1/8 dotted", 0.35f, 0.2f, true, 5.0f);
        reverb (ostinato, RT::hall, 0.5f, 0.2f);
        space (ostinato);
        afterglow (ostinato, Glow::keep());
        ostinato.set (ParamIDs::patchLevel, -1.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ostinato);

        auto rising = x.make ("Rising Arp", 14, { "Arp", "Sixteenths", "Filter", "Afterglow" },
            "The rising arpeggio that opens a thousand '80s synth scores: hold a minor chord and ARP 1 climbs it over two "
            "octaves in sixteenths, the filter slowly opening over the bar, every note its own (AGE), into the ensemble "
            "and echoes, in time with your song. MACROS: BRIGHT, SPACE, GLOW.");
        rising.osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.2f).osc (1, "Arp", 1.0f)
              .filter (FilterType::lp24, 900.0f, 0.3f, 0.35f).filterEnvelope (0.0f, 160.0f, 0.2f, 100.0f)
              .ampEnvelope (1.0f, 220.0f, 0.4f, 120.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.7f)
              .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle).set (ParamIDs::lfo (0, "Rate"), 0.12f)
              .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (rising, "lfo1", ParamIDs::fltCutoff, 0.15f);
        route (rising, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (rising, "BRIGHT", "SPACE", "GLOW");
        afterglow (rising, { 0.35f, 0.25f, 0.45f, 0.2f, 0.6f, 0.5f, 0.5f, 0.45f });
        delay (rising, "1/8 dotted", 0.3f, 0.15f, true, 5.0f);
        reverb (rising, RT::hall, 0.55f, 0.18f);
        space (rising);
        rising.set (ParamIDs::patchLevel, -0.5f);   // LEVEL: measured, not by ear
        x.add (rising);

        auto cassette = x.make ("Cassette Arp", 15, { "Arp", "Tape", "Up-Down", "Afterglow" },
            "A bright arpeggio found on an old cassette: hold a chord and ARP 1 rolls up and down it in sixteenths, "
            "plucked pulses an octave apart, worn and wobbling on TAPE, every note its own. MACROS: BRIGHT, SPACE, GLOW "
            "(more worn).");
        cassette.osc (0, "Wave", pulse).osc (0, "Width", 0.45f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f).osc (1, "Arp", 1.0f)
                .filter (FilterType::lp24, 2200.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 140.0f, 0.2f, 80.0f)
                .ampEnvelope (1.0f, 180.0f, 0.3f, 100.0f)
                .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
                .set (ParamIDs::arp (0, "Gate"), 0.6f);
        route (cassette, "macro1", ParamIDs::fltCutoff, 0.25f);
        macros (cassette, "BRIGHT", "SPACE", "GLOW");
        afterglow (cassette, { 0.3f, 0.3f, 0.3f, 0.5f, 0.6f, 0.5f, 0.5f, 0.45f });
        delay (cassette, "1/4", 0.3f, 0.12f, true, 4.0f);
        reverb (cassette, RT::plate, 0.45f, 0.15f);
        space (cassette);
        cassette.set (ParamIDs::patchLevel, -1.4f);   // LEVEL: measured, not by ear
        x.add (cassette);
    }

    // SANDSEA (the owner's name): inspired by the score of an epic desert science-fiction film: vast low drones,
    // throat-singing and chant, keening voices with wide ornamented vibrato, enormous horn blasts, metallic ritual
    // percussion, wind and sand, melodies in the Phrygian dominant and double-harmonic scales. No title, characters,
    // places or music from the film: our own sounds.
    // SANDSEA's MACRO 3, SIROCCO (0.58.0, the owner's ask and name: the bank's centrepiece): a harmony answering every note
    // in D Phrygian dominant, the bank's scale. LEVEL 0 at the macro's left (no answers at all: the preset sounds exactly
    // as before), the answers coming in one at a time as it rises, spreading wider. The preset's old MACRO 3 job rides
    // along (its routes stay), named in the notes: `oldText` there becomes `newText`.
    struct Answer
    {
        int interval = 0;        // synth::sirocco::intervalNames
        int voices = 1;
        float delayMs = 0.0f;
        float levelTop = 1.0f;   // LEVEL at the macro's right
        float detune = 6.0f;     // cents
    };

    void answers (Builder& b, const juce::String& oldText, const juce::String& newText, Answer a)
    {
        b.set (ParamIDs::siroccoOn, 1.0f).set (ParamIDs::siroccoKey, 2.0f)                  // D
         .set (ParamIDs::siroccoScale, (float) synth::arp::scaleNames.indexOf ("Phrygian Dominant"))
         .set (ParamIDs::siroccoInterval, (float) a.interval).set (ParamIDs::siroccoVoices, (float) a.voices)
         .set (ParamIDs::siroccoDelay, a.delayMs).set (ParamIDs::siroccoLevel, 0.0f)
         .set (ParamIDs::siroccoDetune, a.detune).set (ParamIDs::siroccoSpread, 0.2f);
        route (b, "macro3", ParamIDs::siroccoLevel, a.levelTop);
        route (b, "macro3", ParamIDs::siroccoSpread, 0.4f);
        b.rig.modulation.macroNames[2] = "SIROCCO";
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, newText);
    }

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
        answers (voice, "VOWEL (towards \"ee\")",
                 "SIROCCO (a second voice answering each note a 3rd above, a moment late; with it, the vowel towards \"ee\")",
                 Answer { 0, 1, 120.0f, 1.0f, 8.0f });
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
        answers (horn, "GRIT.",
                 "SIROCCO (horns joining below, a 3rd and then a 5th under each note; with them, grit).",
                 Answer { 5, 2, 0.0f, 1.0f, 6.0f });
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
        answers (flute, "BREATH.",
                 "SIROCCO (a second flute a 6th below, a breath behind; with it, more breath).",
                 Answer { 8, 1, 60.0f, 1.0f, 8.0f });
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
        answers (desert, "WIND (more noise)",
                 "SIROCCO (a 5th and then a 7th rising over the drone; with them, more wind)",
                 Answer { 2, 2, 0.0f, 1.0f, 5.0f });
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
        answers (chant, "GROWL.",
                 "SIROCCO (a second chanter a 5th above; with it, growl).",
                 Answer { 2, 1, 0.0f, 0.35f, 5.0f });
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
        answers (mirage, "SHIMMER (more AM)",
                 "SIROCCO (a choir of three answering in 3rds above; with it, more AM)",
                 Answer { 0, 3, 0.0f, 1.0f, 8.0f });
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
        answers (worm, "GROAN (a deeper rise)",
                 "SIROCCO (a 5th above groaning with it; with it, a deeper rise)",
                 Answer { 2, 1, 0.0f, 1.0f, 4.0f });
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
        answers (quake, "DESTROY.",
                 "SIROCCO (a second quake a 5th above, a beat behind; with it, destruction).",
                 Answer { 2, 1, 150.0f, 0.6f, 4.0f });
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
        answers (iron, "DESTROY.",
                 "SIROCCO (the riff doubled a 4th above; with it, destruction).",
                 Answer { 1, 1, 0.0f, 0.6f, 4.0f });
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
        answers (copper, "GRIT (SHAPER 1's tube after the filter)",
                 "SIROCCO (bells answering a 3rd and a 5th above, a moment late; with them, SHAPER 1's tube after the filter)",
                 Answer { 0, 2, 80.0f, 0.45f, 6.0f });
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
        answers (harp, "TWANG (more resonance)",
                 "SIROCCO (a second string a 3rd below; with it, more resonance)",
                 Answer { 5, 1, 0.0f, 0.8f, 6.0f });
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
        answers (gong, "GRIT.",
                 "SIROCCO (a second gong a 5th above, struck after the first; with it, grit).",
                 Answer { 2, 1, 250.0f, 1.0f, 4.0f });
        gong.set (ParamIDs::patchLevel, -1.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (gong);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto war = x.make ("War Pulse", 13, { "Arp", "Pulse", "Sequencer", "Sine" },
            "A war pulse: ARP 1 pulses a low sine in eighths on the notes you hold, in time with your song. MACROS: "
            "BRIGHT, SPACE, PULSE (louder pulse).");
        war.osc (0, "Wave", sine).osc (0, "Transpose", -12.0f).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
           .osc (0, "Arp", 1.0f).osc (0, "SendReverb", 0.2f)
           .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.15f).osc (1, "Arp", 1.0f)
           .filter (FilterType::lp24, 1200.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 150.0f, 0.2f, 100.0f)
           .ampEnvelope (1.0f, 250.0f, 0.4f, 100.0f)
           .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
           .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Gate"), 0.4f);
        route (war, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (war, "macro3", ParamIDs::osc (0, "Volume"), 0.2f);
        macros (war, "BRIGHT", "SPACE", "PULSE");
        reverb (war, RT::hall, 0.6f, 0.25f);
        space (war);
        answers (war, "PULSE (louder pulse)",
                 "SIROCCO (a pulse a 5th above; with it, a louder pulse)",
                 Answer { 2, 1, 0.0f, 0.6f, 4.0f });
        war.set (ParamIDs::patchLevel, 1.5f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
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
        answers (ripple, "SPARKLE (a sine two octaves up)",
                 "SIROCCO (the runs answered in 3rds; with them, a sine two octaves up)",
                 Answer { 0, 1, 0.0f, 0.7f, 5.0f });
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
        answers (caravan, "WARMTH (the low triangle)",
                 "SIROCCO (each step answered a 6th above; with it, the low triangle)",
                 Answer { 3, 1, 0.0f, 0.6f, 6.0f });
        caravan.set (ParamIDs::patchLevel, -1.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (caravan);
    }

    // DATASTREAM (the owner's name): inspired by the scores of a digital-world science-fiction film and its sequel:
    // precise glowing synth pulses locked to the beat, huge throbbing analog basses, orchestral-electronic swells,
    // clean leads with long glides, a robotic vocal texture, crisp digital bleeps. Cold and exact: no tape, no wobble.
    // No title, characters, places or music from the films: our own sounds.
    // DATASTREAM's centrepiece (0.61.0, the owner's ask): LIGHTTRAIL on every preset, MACRO 3 TRAIL. Each preset gets
    // its own trail (division, rise, length, tone, spread) at MIX 0, so at the macro's left it sounds exactly as approved;
    // the macro brings the trail in and lengthens it. Its old MACRO 3 job rides along (the notes say so).
    struct Trail
    {
        int division = 2;             // effects::LightTrail::divisionNames
        float rise = 0.0f, length = 0.5f, tone = 0.7f, spread = 0.7f;
        float mixTop = 0.5f;          // the macro's MIX at full
    };

    void trails (Builder& b, const juce::String& oldText, const juce::String& newText, Trail t)
    {
        b.set (ParamIDs::trailOn, 1.0f).set (ParamIDs::trailDivision, (float) t.division).set (ParamIDs::trailRise, t.rise)
         .set (ParamIDs::trailLength, t.length).set (ParamIDs::trailTone, t.tone).set (ParamIDs::trailSpread, t.spread)
         .set (ParamIDs::trailMix, 0.0f);
        route (b, "macro3", ParamIDs::trailMix, t.mixTop);
        route (b, "macro3", ParamIDs::trailLength, 0.2f);
        b.rig.modulation.macroNames[2] = "TRAIL";
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, newText);
    }

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
        macros (trail, "BRIGHT", "SPACE", "TRAIL");
        delay (trail, "1/8 dotted", 0.35f, 0.2f, true, 6.0f);
        reverb (trail, RT::hall, 0.6f, 0.2f);
        space (trail);
        trails (trail, "GLOW (more sync)", "TRAIL (each note's trail, plus GLOW (more sync))", { 2, 7.0f, 0.5f, 0.8f, 0.8f, 0.55f });
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
        macros (vector, "BRIGHT", "SPACE", "TRAIL");
        delay (vector, "1/8", 0.3f, 0.15f, true, 5.0f);
        reverb (vector, RT::plate, 0.5f, 0.15f);
        space (vector);
        trails (vector, "ROBOT (more crush)", "TRAIL (each note's trail, plus ROBOT (more crush))", { 3, 12.0f, 0.45f, 0.7f, 0.7f, 0.5f });
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
        macros (burst, "BRIGHT", "SPACE", "TRAIL");
        delay (burst, "1/8 dotted", 0.3f, 0.15f, true, 6.0f);
        reverb (burst, RT::room, 0.5f, 0.12f);
        space (burst);
        trails (burst, "BLIP (a bigger blip)", "TRAIL (each note's trail, plus BLIP (a bigger blip))", { 0, 2.0f, 0.6f, 0.9f, 0.6f, 0.5f });
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
        macros (horizon, "BRIGHT", "SPACE", "TRAIL");
        reverb (horizon, RT::hall, 0.8f, 0.3f, 30.0f, 4.5f);
        eqBand (horizon, 0, eq::Shape::lowCut, 100.0f, 0.0f);
        space (horizon);
        trails (horizon, "SWELL (a bigger opening)", "TRAIL (each note's trail, plus SWELL (a bigger opening))", { 5, 12.0f, 0.45f, 0.6f, 0.7f, 0.45f });
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
        macros (cathedral, "BRIGHT", "SPACE", "TRAIL");
        chorus (cathedral, ParamIDs::modIds, MT::chorus, 0.3f, 0.3f, 0.3f);
        reverb (cathedral, RT::cathedral, 0.4f, 0.35f, 40.0f, 4.5f);
        space (cathedral);
        trails (cathedral, "HARMONICS", "TRAIL (each note's trail, plus HARMONICS)", { 4, 7.0f, 0.45f, 0.6f, 0.8f, 0.45f });
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
        macros (sky, "BRIGHT", "SPACE", "TRAIL");
        reverb (sky, RT::plate, 0.6f, 0.25f);
        space (sky);
        trails (sky, "SMOOTH (less gating)", "TRAIL (each note's trail, plus SMOOTH (less gating))", { 3, 0.0f, 0.5f, 0.7f, 1.0f, 0.45f });
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
        macros (engine, "BRIGHT", "SPACE", "TRAIL");
        reverb (engine, RT::room, 0.3f, 0.08f);
        space (engine);
        trails (engine, "GROWL (the filter's drive)", "TRAIL (each note's trail, plus GROWL (the filter's drive))", { 2, 12.0f, 0.35f, 0.6f, 0.6f, 0.4f });
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
        macros (bitfall, "BRIGHT", "SPACE", "TRAIL");
        reverb (bitfall, RT::room, 0.3f, 0.08f);
        space (bitfall);
        trails (bitfall, "CRUSH", "TRAIL (each note's trail, plus CRUSH)", { 2, -12.0f, 0.4f, 0.6f, 0.6f, 0.45f });
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
        macros (lattice, "BRIGHT", "SPACE", "TRAIL");
        reverb (lattice, RT::room, 0.3f, 0.08f);
        space (lattice);
        trails (lattice, "WEIGHT (more sub)", "TRAIL (each note's trail, plus WEIGHT (more sub))", { 4, 12.0f, 0.35f, 0.6f, 0.6f, 0.4f });
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
        macros (console, "BRIGHT", "SPACE", "TRAIL");
        delay (console, "1/8 dotted", 0.35f, 0.18f, true, 6.0f);
        reverb (console, RT::plate, 0.5f, 0.2f);
        space (console);
        trails (console, "SHIMMER", "TRAIL (each note's trail, plus SHIMMER)", { 4, 7.0f, 0.5f, 1.0f, 0.8f, 0.5f });
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
        macros (protocol, "BRIGHT", "SPACE", "TRAIL");
        reverb (protocol, RT::hall, 0.7f, 0.25f);
        space (protocol);
        trails (protocol, "BITE (a bigger punch)", "TRAIL (each note's trail, plus BITE (a bigger punch))", { 5, 0.0f, 0.4f, 0.6f, 0.7f, 0.4f });
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
        macros (chimes, "BRIGHT", "SPACE", "TRAIL");
        delay (chimes, "1/16 dotted", 0.4f, 0.2f, true, 7.0f);
        reverb (chimes, RT::plate, 0.4f, 0.15f);
        space (chimes);
        trails (chimes, "CRUSH", "TRAIL (each note's trail, plus CRUSH)", { 1, 5.0f, 0.55f, 0.9f, 0.8f, 0.5f });
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
        macros (overclock, "BRIGHT", "SPACE", "TRAIL");
        delay (overclock, "1/8 dotted", 0.35f, 0.18f, true, 5.0f);
        reverb (overclock, RT::hall, 0.5f, 0.18f);
        space (overclock);
        trails (overclock, "RESONANCE", "TRAIL (each note's trail, plus RESONANCE)", { 2, -5.0f, 0.45f, 0.7f, 0.7f, 0.45f });
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
        macros (system, "BRIGHT", "SPACE", "TRAIL");
        delay (system, "1/4 dotted", 0.35f, 0.15f, true, 5.0f);
        reverb (system, RT::plate, 0.5f, 0.2f);
        space (system);
        trails (system, "SMOOTH (less chopping)", "TRAIL (each note's trail, plus SMOOTH (less chopping))", { 4, 12.0f, 0.45f, 0.7f, 0.8f, 0.45f });
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
        macros (circuit, "BRIGHT", "SPACE", "TRAIL");
        delay (circuit, "1/8 dotted", 0.3f, 0.15f, true, 6.0f);
        reverb (circuit, RT::plate, 0.4f, 0.15f);
        space (circuit);
        trails (circuit, "CRUSH", "TRAIL (each note's trail, plus CRUSH)", { 0, 7.0f, 0.5f, 0.8f, 0.7f, 0.5f });
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
    // LAST LIGHT's centrepiece (0.62.0, the owner's ask): BLACKHOLE on every preset, MACRO 3 GRAVITY. Each preset gets
    // its own haze (hold, size, pull, tone, width) at MIX 0, so at the macro's left it sounds exactly as approved; the
    // macro brings the haze in and lengthens it. Its old MACRO 3 job rides along (the notes say so).
    struct Haze
    {
        float hold = 0.7f, size = 0.5f, pull = 0.15f, tone = 0.5f, width = 0.8f;
        float mixTop = 0.45f;   // the macro's MIX at full
    };

    void gravity (Builder& b, const juce::String& oldText, const juce::String& newText, Haze h)
    {
        b.set (ParamIDs::blackholeOn, 1.0f).set (ParamIDs::blackholeHold, h.hold).set (ParamIDs::blackholeSize, h.size)
         .set (ParamIDs::blackholePull, h.pull).set (ParamIDs::blackholeTone, h.tone).set (ParamIDs::blackholeWidth, h.width)
         .set (ParamIDs::blackholeMix, 0.0f);
        route (b, "macro3", ParamIDs::blackholeMix, h.mixTop);
        route (b, "macro3", ParamIDs::blackholeHold, 0.1f);
        b.rig.modulation.macroNames[2] = "GRAVITY";
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, newText);
    }

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
        macros (signal, "BRIGHT", "SPACE", "GRAVITY");
        delay (signal, "1/4", 0.3f, 0.15f, true, 4.0f);
        signal.set (ParamIDs::delayToReverb, 0.4f);
        reverb (signal, RT::cathedral, 0.5f, 0.32f, 60.0f, 4.0f);
        space (signal);
        gravity (signal, "VIBRATO", "GRAVITY (the haze, plus VIBRATO)", { 0.7f, 0.4f, 0.15f, 0.6f, 0.8f, 0.45f });
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
        macros (cello, "BRIGHT", "SPACE", "GRAVITY");
        reverb (cello, RT::hall, 0.7f, 0.25f, 30.0f, 4.0f);
        space (cello);
        gravity (cello, "BOW (more bow noise)", "GRAVITY (the haze, plus BOW (more bow noise))", { 0.65f, 0.5f, 0.2f, 0.45f, 0.7f, 0.4f });
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
        macros (elegy, "BRIGHT", "SPACE", "GRAVITY");
        reverb (elegy, RT::cathedral, 0.5f, 0.35f, 50.0f, 4.0f);
        space (elegy);
        gravity (elegy, "VOWEL (opening to \"oh\")", "GRAVITY (the haze, plus VOWEL (opening to \"oh\"))", { 0.75f, 0.5f, 0.15f, 0.55f, 0.9f, 0.45f });
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
        macros (organ, "BRIGHT", "SPACE", "GRAVITY");
        reverb (organ, RT::cathedral, 0.5f, 0.4f, 40.0f, 4.0f);
        space (organ);
        gravity (organ, "STOPS (more upper ranks)", "GRAVITY (the haze, plus STOPS (more upper ranks))", { 0.8f, 0.7f, 0.1f, 0.45f, 0.9f, 0.4f });
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
        macros (strings, "BRIGHT", "SPACE", "GRAVITY");
        chorus (strings, ParamIDs::modIds, MT::chorus, 0.4f, 0.4f, 0.35f);
        strings.set (ParamIDs::modStereo, 1.0f);
        reverb (strings, RT::hall, 0.85f, 0.3f, 40.0f, 4.0f);
        space (strings);
        gravity (strings, "SWELL (a bigger opening)", "GRAVITY (the haze, plus SWELL (a bigger opening))", { 0.8f, 0.6f, 0.25f, 0.5f, 1.0f, 0.45f });
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
        macros (fading, "BRIGHT", "SPACE", "GRAVITY");
        reverb (fading, RT::cathedral, 0.4f, 0.35f, 40.0f, 3.5f);
        space (fading);
        gravity (fading, "DRIFT (a wider unison)", "GRAVITY (the haze, plus DRIFT (a wider unison))", { 0.85f, 0.8f, 0.4f, 0.35f, 1.0f, 0.5f });
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
        macros (pedal, "BRIGHT", "SPACE", "GRAVITY");
        reverb (pedal, RT::cathedral, 0.3f, 0.25f, 40.0f, 3.0f);
        space (pedal);
        gravity (pedal, "RANKS (more upper ranks)", "GRAVITY (the haze, plus RANKS (more upper ranks))", { 0.55f, 0.4f, 0.1f, 0.25f, 0.4f, 0.25f });
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
        macros (well, "BRIGHT", "SPACE", "GRAVITY");
        reverb (well, RT::hall, 0.7f, 0.2f);
        space (well);
        gravity (well, "WARMTH", "GRAVITY (the haze, plus WARMTH)", { 0.7f, 0.6f, 0.6f, 0.25f, 0.5f, 0.3f });
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
        macros (heart, "BRIGHT", "SPACE", "GRAVITY");
        reverb (heart, RT::hall, 0.5f, 0.15f);
        space (heart);
        gravity (heart, "THUMP (a bigger drop)", "GRAVITY (the haze, plus THUMP (a bigger drop))", { 0.5f, 0.3f, 0.2f, 0.3f, 0.4f, 0.25f });
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
        macros (memory, "BRIGHT", "SPACE", "GRAVITY");
        reverb (memory, RT::room, 0.6f, 0.2f, 15.0f, 4.0f);
        space (memory);
        gravity (memory, "FELT (less felt: a clearer strike)", "GRAVITY (the haze, plus FELT (less felt: a clearer strike))", { 0.75f, 0.4f, 0.1f, 0.55f, 0.8f, 0.45f });
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
        macros (distant, "BRIGHT", "SPACE", "GRAVITY");
        delay (distant, "1/4 dotted", 0.3f, 0.1f, true, 3.5f);
        distant.set (ParamIDs::delayToReverb, 0.5f);
        reverb (distant, RT::hall, 0.85f, 0.4f, 80.0f, 3.5f);
        eqBand (distant, 0, eq::Shape::highCut, 6000.0f, 0.0f);
        space (distant);
        gravity (distant, "WEAR (more cassette)", "GRAVITY (the haze, plus WEAR (more cassette))", { 0.8f, 0.5f, 0.15f, 0.4f, 0.8f, 0.45f });
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
        macros (musicBox, "BRIGHT", "SPACE", "GRAVITY");
        delay (musicBox, "1/4", 0.3f, 0.12f, true, 5.0f);
        reverb (musicBox, RT::hall, 0.7f, 0.3f, 30.0f, 5.0f);
        space (musicBox);
        gravity (musicBox, "TINE (the high tine)", "GRAVITY (the haze, plus TINE (the high tine))", { 0.8f, 0.4f, 0.15f, 0.7f, 0.9f, 0.45f });
        musicBox.set (ParamIDs::patchLevel, -2.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (musicBox);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto clock = x.make ("Ticking Clock", 13, { "Arp", "Piano", "Felt", "Sequencer" },
            "Time running out: ARP 1 plays the chord you hold as a quiet, repeating felt-piano figure, in time with your "
            "song. MACROS: BRIGHT, SPACE, FELT.");
        feltPiano (clock);
        for (const auto i : { 0, 1, 2, 3 })
            clock.osc (i, "Arp", 1.0f);
        clock.set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
             .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.9f);
        macros (clock, "BRIGHT", "SPACE", "GRAVITY");
        reverb (clock, RT::hall, 0.6f, 0.22f);
        space (clock);
        gravity (clock, "FELT", "GRAVITY (the haze, plus FELT)", { 0.7f, 0.35f, 0.1f, 0.55f, 0.8f, 0.4f });
        clock.set (ParamIDs::patchLevel, -3.7f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
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
        macros (docking, "BRIGHT", "SPACE", "GRAVITY");
        reverb (docking, RT::cathedral, 0.35f, 0.3f, 30.0f, 4.0f);
        space (docking);
        gravity (docking, "RANKS", "GRAVITY (the haze, plus RANKS)", { 0.75f, 0.6f, 0.1f, 0.5f, 0.9f, 0.4f });
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
        macros (tesseract, "BRIGHT", "SPACE", "GRAVITY");
        delay (tesseract, "1/4 dotted", 0.3f, 0.12f, true, 4.0f);
        reverb (tesseract, RT::reverse, 0.7f, 0.35f);
        space (tesseract);
        gravity (tesseract, "FELT", "GRAVITY (the haze, plus FELT)", { 0.85f, 0.7f, 0.2f, 0.5f, 1.0f, 0.5f });
        tesseract.set (ParamIDs::patchLevel, -3.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (tesseract);
    }

    // SIMULACRUM's centrepiece (0.63.0, the owner's ask): DILATION on every preset, MACRO 3 DILATE. Each preset gets
    // its own slowing (how far down, how fast it slides, how it comes back, how far behind it can fall, how dark) at
    // SLOW 0, so at the macro's left it sounds exactly as approved; the macro slows time. Its old MACRO 3 job rides
    // along (the notes say so).
    struct Pace
    {
        float slowTop = 0.5f;   // the macro's SLOW at full (0.5: half speed, an octave down)
        float ramp = 0.5f;
        bool snap = false;      // RETURN: GLIDE or SNAP
        float reach = 2.0f / 3.0f;
        float tone = 0.5f;
    };

    void dilate (Builder& b, const juce::String& oldJob, Pace pace)
    {
        b.set (ParamIDs::dilationOn, 1.0f).set (ParamIDs::dilationSlow, 0.0f).set (ParamIDs::dilationRamp, pace.ramp)
         .set (ParamIDs::dilationReturn, pace.snap ? 1.0f : 0.0f).set (ParamIDs::dilationReach, pace.reach)
         .set (ParamIDs::dilationTone, pace.tone).set (ParamIDs::dilationMix, 1.0f);
        route (b, "macro3", ParamIDs::dilationSlow, pace.slowTop);
        b.rig.modulation.macroNames[2] = "DILATE";
        const auto oldText = "SPACE, " + oldJob;
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, "SPACE, DILATE (time slowing down, plus " + oldJob + ")");
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
        dilate (chase, "SCREAM (more sync)", { 0.5f, 0.45f, false, 2.0f / 3.0f, 0.4f });
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
        dilate (phantom, "ECHO (longer echoes)", { 0.5f, 0.55f, false, 2.0f / 3.0f, 0.5f });
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
        dilate (slow, "SAG", { 0.6f, 0.7f, false, 2.0f / 3.0f, 0.35f });
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
        dilate (rain, "GLITTER (more sparkle)", { 0.45f, 0.6f, false, 2.0f / 3.0f, 0.5f });
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
        dilate (cluster, "CLASH (louder clashing notes)", { 0.55f, 0.65f, false, 2.0f / 3.0f, 0.3f });
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
        dilate (field, "PUMP (deeper pumping)", { 0.5f, 0.7f, false, 1.0f, 0.5f });
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
        dilate (pulseBass, "DESTROY (SHAPER 1's tube into the drive)", { 0.3f, 0.3f, true, 2.0f / 3.0f, 0.4f });
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
        dilate (breakBass, "GROWL (SHAPER 1's grit)", { 0.25f, 0.35f, true, 2.0f / 3.0f, 0.5f });
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
        dilate (mainframe, "MALFUNCTION (the LO-FI bitcrusher brought in)", { 0.25f, 0.4f, false, 2.0f / 3.0f, 0.5f });
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
        dilate (terminal, "RATIO (a stranger ratio)", { 0.5f, 0.5f, false, 2.0f / 3.0f, 0.5f });
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
        dilate (glass, "CRUSH (more crush)", { 0.5f, 0.4f, true, 2.0f / 3.0f, 0.6f });
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
        dilate (warped, "WARP (bent without the wheel)", { 0.45f, 0.6f, false, 2.0f / 3.0f, 0.4f });
        x.add (warped);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto breach = x.make ("System Breach", 13, { "Arp", "Riff", "Drive", "Sequencer" },
            "A driven bass riff: ARP 1 plays the notes you hold as a riff in sixteenths, in time with your song, through "
            "insert bus A's RABID. MACROS: BRIGHT, SPACE, DESTROY (SHAPER 1's tube into the drive).");
        breach.osc (0, "Volume", 0.55f).osc (0, "Insert", 1.0f).osc (0, "Arp", 1.0f).osc (0, "Transpose", -12.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -24.0f)
              .osc (1, "Volume", 0.35f).osc (1, "Insert", 1.0f).osc (1, "Arp", 1.0f)
              .shaper (0, synth::ShaperModel::tube, 0.05f)
              .filter (FilterType::lp24, 500.0f, 0.25f, 0.4f).filterEnvelope (0.0f, 140.0f, 0.1f, 80.0f)
              .ampEnvelope (1.0f, 80.0f, 0.0f, 30.0f)   // each hit dies inside its sixteenth (the drive comes after)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.55f);
        drive (breach, 0, DM::rabid, 5.0f, 5.0f, 0.0f, 0.0f, 0.1f);
        compressor (breach, fet, -18.0f, 4.0f, 2.0f);
        route (breach, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (breach, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (breach, "BRIGHT", "SPACE", "DESTROY");
        reverb (breach, RT::room, 0.5f, 0.1f);
        space (breach);
        breach.set (ParamIDs::patchLevel, 4.0f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        dilate (breach, "DESTROY (SHAPER 1's tube into the drive)", { 0.5f, 0.35f, true, 0.5f, 0.5f });
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
        dilate (cascade, "CRUSH (bit-crushed plucks)", { 0.5f, 0.45f, false, 0.5f, 0.6f });
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
        dilate (firefight, "BITE (more resonance)", { 0.5f, 0.3f, true, 0.5f, 0.5f });
        x.add (firefight);
    }

    // DREAMSTATE's centrepiece (0.64.0, the owner's ask): REMNANT on every preset, MACRO 3 DREAM. Each preset gets its
    // own layers (how many, how far down each, how long a stretch, how much quieter and darker, how wide) at MIX 0, so
    // at the macro's left it sounds exactly as approved; the macro brings the layers in beneath it. Its old MACRO 3 job
    // rides along (the notes say so).
    struct Depths
    {
        int layers = 1;          // Remnant::layerNames index: 2 layers
        int step = 0;            // OCTAVE, FIFTH, FOURTH
        float length = 0.5f, taper = 0.3f, tone = 0.45f, width = 0.7f;
        float mixTop = 0.5f;     // the macro's MIX at full
    };

    void dream (Builder& b, const juce::String& oldJob, Depths d)
    {
        b.set (ParamIDs::remnantOn, 1.0f).set (ParamIDs::remnantLayers, (float) d.layers).set (ParamIDs::remnantStep, (float) d.step)
         .set (ParamIDs::remnantLength, d.length).set (ParamIDs::remnantTaper, d.taper).set (ParamIDs::remnantTone, d.tone)
         .set (ParamIDs::remnantWidth, d.width).set (ParamIDs::remnantMix, 0.0f);
        route (b, "macro3", ParamIDs::remnantMix, d.mixTop);
        b.rig.modulation.macroNames[2] = "DREAM";
        const auto oldText = "SPACE, " + oldJob;
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, "SPACE, DREAM (layers beneath, each deeper one slower, plus " + oldJob + ")");
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
        dream (twang, "TWANG (a sharper snap)", { 1, 0, 0.45f, 0.3f, 0.45f, 0.7f, 0.5f });
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
        dream (horn, "BRASS (a brassier swell)", { 1, 1, 0.55f, 0.3f, 0.4f, 0.6f, 0.55f });
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
        dream (weightless, "VOICE (the formant moved)", { 2, 1, 0.6f, 0.25f, 0.5f, 0.8f, 0.4f });   // (already +5 dBFS at full MACRO 3 from VOICE)
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
        dream (folding, "FOLD (folded without the wheel)", { 2, 0, 0.65f, 0.25f, 0.45f, 0.8f, 0.6f });
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
        dream (dilation, "DRAG (sinks further)", { 2, 0, 0.8f, 0.2f, 0.4f, 0.7f, 0.6f });
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
        dream (heavy, "WEIGHT (more of the low octave)", { 1, 0, 0.6f, 0.4f, 0.35f, 0.6f, 0.45f });
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
        dream (colossus, "SAG (sinks further)", { 0, 2, 0.5f, 0.3f, 0.3f, 0.5f, 0.4f });
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
        dream (undertow, "SURGE (a bigger opening)", { 0, 2, 0.6f, 0.3f, 0.3f, 0.4f, 0.35f });
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
        dream (ostBass, "BITE (more snap)", { 0, 1, 0.35f, 0.3f, 0.35f, 0.5f, 0.35f });
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
        dream (echoPiano, "FELT (a clearer strike)", { 2, 0, 0.55f, 0.3f, 0.45f, 0.7f, 0.55f });
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
        dream (chorale, "WARBLE (more wobble)", { 1, 2, 0.6f, 0.3f, 0.4f, 0.7f, 0.55f });
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
        dream (glassMem, "SHIMMER (the octave)", { 2, 1, 0.5f, 0.25f, 0.55f, 0.8f, 0.5f });
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
        dream (ostStrings, "BITE (more snap)", { 1, 0, 0.4f, 0.35f, 0.45f, 0.7f, 0.5f });
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
        dream (layers, "DEPTH (louder slow layer)", { 2, 0, 0.5f, 0.3f, 0.45f, 0.8f, 0.25f });   // (0.5 peaked +0.6 dBFS, 0.35 0.0)
        x.add (layers);

        auto countdown = x.make ("Countdown", 15, { "Arp", "Pulse", "Tension", "Muted" },
            "Time running out: ARP 1 pulses the chord you hold in eighths on a muted pulse wave, in time with your song. "
            "MACROS: BRIGHT, SPACE, TENSION (the filter opens).");
        countdown.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
                 .filter (FilterType::lp24, 800.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 180.0f, 0.2f, 120.0f)
                 .ampEnvelope (2.0f, 250.0f, 0.3f, 150.0f)
                 .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                 .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
                 .set (ParamIDs::arp (0, "Gate"), 0.5f);
        route (countdown, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (countdown, "macro3", ParamIDs::fltCutoff, 0.25f);
        macros (countdown, "BRIGHT", "SPACE", "TENSION");
        reverb (countdown, RT::hall, 0.6f, 0.2f);
        space (countdown);
        countdown.set (ParamIDs::patchLevel, -0.8f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        dream (countdown, "TENSION (the filter opens)", { 1, 1, 0.35f, 0.35f, 0.4f, 0.6f, 0.45f });
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

    // NEGATIVE SPACE's centrepiece (0.65.0, the owner's ask): PROXIMITY on every preset, MACRO 3 CLOSER. Each preset gets
    // its own closeness (how loud the touches, the room and hum, the close-up low end, how bright the felt) at 0, so at
    // the macro's left it sounds exactly as approved; the macro brings you up close. Its old MACRO 3 job rides along
    // (the notes say so).
    struct Near
    {
        float noiseTop = 0.5f, roomTop = 0.45f, closeTop = 0.35f;   // the macro's NOISE, ROOM and CLOSE at full
        float tone = 0.5f;
        int hum = 1;                                                // OFF, 50 HZ, 60 HZ
        float width = 0.6f;
    };

    void closer (Builder& b, const juce::String& oldJob, Near n)
    {
        b.set (ParamIDs::proximityOn, 1.0f).set (ParamIDs::proximityNoise, 0.0f).set (ParamIDs::proximityRoom, 0.0f)
         .set (ParamIDs::proximityClose, 0.0f).set (ParamIDs::proximityTone, n.tone).set (ParamIDs::proximityHum, (float) n.hum)
         .set (ParamIDs::proximityWidth, n.width);
        route (b, "macro3", ParamIDs::proximityNoise, n.noiseTop);
        route (b, "macro3", ParamIDs::proximityRoom, n.roomTop);
        route (b, "macro3", ParamIDs::proximityClose, n.closeTop);
        b.rig.modulation.macroNames[2] = "CLOSER";
        const auto oldText = "SPACE, " + oldJob;
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, "SPACE, CLOSER (the sound of playing, up close, plus " + oldJob + ")");
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
        closer (staticLead, "BUZZ (more SHAPER 1 into the drive)", { 0.55f, 0.5f, 0.4f, 0.6f, 1 });
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
        closer (hollow, "RESONANCE", { 0.5f, 0.45f, 0.35f, 0.5f, 1 });
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
        closer (glassTone, "FIFTH (more of the fifth)", { 0.45f, 0.4f, 0.3f, 0.55f, 0 });
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
        closer (hum, "GRIT (more SHAPER 1 into the drive)", { 0.35f, 0.55f, 0.1f, 0.4f, 1 });   // (CLOSE 0.4 took the drone to +1.1 dBFS)
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
        closer (coldRoom, "AIR (more breath)", { 0.35f, 0.6f, 0.3f, 0.6f, 0 });
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
        closer (broken, "DROPOUT (deeper cuts)", { 0.5f, 0.5f, 0.35f, 0.55f, 1 });
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
        closer (grit, "GRIT (more SHAPER 1 into the drive)", { 0.4f, 0.45f, 0.25f, 0.45f, 1 });
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
        closer (pulseBass, "SNAP (more filter snap)", { 0.45f, 0.4f, 0.25f, 0.5f, 1 });
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
        closer (throb, "BODY (more of the triangle, heard on small speakers)", { 0.3f, 0.4f, 0.2f, 0.4f, 1 });
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
        closer (soft, "FELT (less felt: a clearer strike)", { 0.75f, 0.45f, 0.6f, 0.3f, 0 });
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
        closer (upright, "DRONE (more drone)", { 0.7f, 0.5f, 0.5f, 0.4f, 1 });
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
        closer (ghost, "SHIMMER", { 0.65f, 0.5f, 0.5f, 0.35f, 0 });
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
        closer (motion, "SNAP", { 0.5f, 0.45f, 0.35f, 0.55f, 1 });
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
        closer (glitchPulse, "CRUSH", { 0.55f, 0.45f, 0.3f, 0.6f, 1 });
        x.add (glitchPulse);

        auto rows = x.make ("Steady Rows", 15, { "Arp", "Rhythm", "Saw", "Sequencer" },
            "A relentless rowing rhythm: ARP 1 rows through the chord you hold in eighths, up and down, on a dark saw, in "
            "time with your song. MACROS: BRIGHT, SPACE, DRIVE (pushes harder).");
        rows.osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
            .filter (FilterType::lp24, 700.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 200.0f, 0.2f, 150.0f)
            .ampEnvelope (3.0f, 300.0f, 0.4f, 150.0f)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
            .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.6f);
        route (rows, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (rows, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (rows, "BRIGHT", "SPACE", "DRIVE");
        reverb (rows, RT::room, 0.5f, 0.12f);
        space (rows);
        rows.set (ParamIDs::patchLevel, 3.1f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        closer (rows, "DRIVE (pushes harder)", { 0.5f, 0.45f, 0.35f, 0.5f, 1 });
        x.add (rows);
    }

    // NIGHTDRIVE's centrepiece (0.66.0, the owner's ask): HAZE on every preset, MACRO 3 HAZE. Each preset gets its own
    // pass (how often, how far it leans and bends, the wind, how dark far off) at DEPTH 0, so at the macro's left it
    // sounds exactly as approved; the macro sends it sweeping past like streetlights. Its old MACRO 3 job rides along
    // (the notes say so).
    struct Lights
    {
        int division = 1;                                           // 1/2 BAR, 1 BAR, 2 BARS, 4 BARS
        float depthTop = 0.6f, dopplerTop = 0.45f, whooshTop = 0.3f;   // the macro's DEPTH, DOPPLER, WHOOSH at full
        float width = 0.8f, tone = 0.45f;
    };

    void passing (Builder& b, const juce::String& oldJob, Lights l)
    {
        b.set (ParamIDs::hazeOn, 1.0f).set (ParamIDs::hazeDivision, (float) l.division).set (ParamIDs::hazeDepth, 0.0f)
         .set (ParamIDs::hazeDoppler, 0.0f).set (ParamIDs::hazeWhoosh, 0.0f).set (ParamIDs::hazeWidth, l.width)
         .set (ParamIDs::hazeTone, l.tone);
        route (b, "macro3", ParamIDs::hazeDepth, l.depthTop);
        route (b, "macro3", ParamIDs::hazeDoppler, l.dopplerTop);
        route (b, "macro3", ParamIDs::hazeWhoosh, l.whooshTop);
        b.rig.modulation.macroNames[2] = "HAZE";
        const auto oldText = "SPACE, " + oldJob;
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, "SPACE, HAZE (streetlights passing, plus " + oldJob + ")");
    }

    // NIGHTDRIVE (the owner's name): inspired by the score and soundtrack of a 2011 neo-noir film about a getaway
    // driver: late-night retro synthwave, turned all the way up (the owner's ask): warm analog polysynths, chorus on
    // everything, big reverbs and echoes, pulsing octave basses, glassy '80s keys, punchy drum-machine beats, dreamy,
    // romantic leads. No title, characters, songs or artists: our own sounds.
    void nightdrive (Expansion x)
    {
        using SP9 = synth::mod::ShapePoint;

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto cruise = x.make ("Night Cruise", 1, { "Lead", "Unison", "Glide", "Chorus" },
            "A smooth, singing synthwave lead: two saws in a soft unison through a warm filter, a slow glide between "
            "notes, vibrato coming in late, a wide chorus, dotted-eighth echoes and a big hall. MACROS: BRIGHT, SPACE, "
            "VIBRATO.");
        cruise.osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.6f)
              .osc (0, "Volume", 0.6f).osc (0, "VibAmount", 15.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 500.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.2f)
              .filter (FilterType::lp24, 2800.0f, 0.15f, 0.2f).filterEnvelope (20.0f, 600.0f, 0.7f, 400.0f)
              .ampEnvelope (10.0f, 500.0f, 0.9f, 450.0f)
              .mono (90.0f).set (ParamIDs::glideCurve, 0.4f);
        route (cruise, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (cruise, "macro3", ParamIDs::osc (0, "VibAmount"), 0.25f);
        macros (cruise, "BRIGHT", "SPACE", "VIBRATO");
        chorus (cruise, ParamIDs::modIds, MT::chorus, 0.5f, 0.5f, 0.5f);
        delay (cruise, "1/8 dotted", 0.4f, 0.22f, true, 4.5f);
        cruise.set (ParamIDs::delayToReverb, 0.3f);
        reverb (cruise, RT::hall, 0.8f, 0.25f, 30.0f, 4.5f);
        space (cruise);
        cruise.set (ParamIDs::patchLevel, -4.9f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (cruise, "VIBRATO", { 1, 0.6f, 0.5f, 0.3f, 0.8f, 0.45f });
        x.add (cruise);

        auto whistle = x.make ("Chrome Whistle", 2, { "Lead", "Square", "Vibrato", "Mono" },
            "A glassy, bright lead that whistles: a square with a quiet square an octave up, a quick vibrato, a "
            "shimmering dimension chorus, quarter-note echoes and a plate. The MOD WHEEL deepens the vibrato. MACROS: "
            "BRIGHT, SPACE, OCTAVE (more of the upper square).");
        whistle.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.55f)
               .osc (0, "VibAmount", 10.0f).osc (0, "VibRate", 6.0f).osc (0, "VibFade", 250.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.12f)
               .osc (1, "VibAmount", 10.0f).osc (1, "VibRate", 6.0f).osc (1, "VibFade", 250.0f)
               .filter (FilterType::lp12, 4000.0f, 0.1f, 0.0f)
               .ampEnvelope (8.0f, 400.0f, 0.85f, 350.0f)
               .mono (40.0f);
        route (whistle, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.25f);
        route (whistle, "modWheel", ParamIDs::osc (1, "VibAmount"), 0.25f);
        route (whistle, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (whistle, "macro3", ParamIDs::osc (1, "Volume"), 0.12f);
        macros (whistle, "BRIGHT", "SPACE", "OCTAVE");
        chorus (whistle, ParamIDs::modIds, MT::dimension, 0.4f, 0.5f, 0.5f);
        delay (whistle, "1/4", 0.35f, 0.2f, true, 5.0f);
        reverb (whistle, RT::plate, 0.6f, 0.22f);
        space (whistle);
        whistle.set (ParamIDs::patchLevel, -1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (whistle, "OCTAVE (more of the upper square)", { 1, 0.55f, 0.45f, 0.25f, 0.8f, 0.5f });
        x.add (whistle);

        auto romance = x.make ("Romance Sync", 3, { "Lead", "Sync", "Chorus", "Mono" },
            "A soaring lead for big hooks: a saw lightly hard-synced, its sync sweeping down as each note starts, over "
            "a soft triangle an octave down, with a wide chorus, ping-pong echoes and a hall. The MOD WHEEL pushes the "
            "sync. MACROS: BRIGHT, SPACE, SOAR (more sync).");
        romance.osc (0, "SyncAmount", 0.2f).osc (0, "SyncEnv", 0.3f).osc (0, "SyncEnvTime", 350.0f).osc (0, "Volume", 0.55f)
               .osc (0, "VibAmount", 12.0f).osc (0, "VibRate", 5.5f).osc (0, "VibFade", 400.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
               .filter (FilterType::lp24, 4500.0f, 0.15f, 0.0f)
               .ampEnvelope (5.0f, 400.0f, 0.9f, 400.0f)
               .mono (60.0f);
        route (romance, "modWheel", ParamIDs::osc (0, "SyncAmount"), 0.3f);
        route (romance, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (romance, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.3f);
        macros (romance, "BRIGHT", "SPACE", "SOAR");
        chorus (romance, ParamIDs::modIds, MT::chorus, 0.4f, 0.45f, 0.45f);
        delay (romance, "1/8 dotted", 0.35f, 0.18f, true, 5.0f);
        reverb (romance, RT::hall, 0.7f, 0.22f);
        space (romance);
        romance.set (ParamIDs::patchLevel, -1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (romance, "SOAR (more sync)", { 1, 0.6f, 0.5f, 0.3f, 0.8f, 0.45f });
        x.add (romance);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto haze = x.make ("Neon Haze", 4, { "Pad", "Polysynth", "Chorus", "Unison" },
            "A lush '80s polysynth pad: five saws and a pulse in a wide unison, the filter opening gently as the chord "
            "swells in, through two choruses (MOD and MOD 2) into a huge hall. MACROS: BRIGHT, SPACE, LUSH (more "
            "detune).");
        haze.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 1.0f)
            .osc (0, "Volume", 0.45f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.35f).osc (1, "Volume", 0.25f)
            .filter (FilterType::lp24, 1500.0f, 0.15f, 0.2f).filterEnvelope (500.0f, 1500.0f, 0.7f, 1200.0f)
            .set (ParamIDs::fltKey, 0.4f)
            .ampEnvelope (300.0f, 1500.0f, 0.9f, 1500.0f);
        route (haze, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (haze, "macro3", ParamIDs::osc (0, "UniDetune"), 0.25f);
        macros (haze, "BRIGHT", "SPACE", "LUSH");
        chorus (haze, ParamIDs::modIds, MT::chorus, 0.5f, 0.55f, 0.55f);
        chorus (haze, ParamIDs::mod2Ids, MT::dimension, 0.3f, 0.4f, 0.35f);
        reverb (haze, RT::hall, 0.9f, 0.3f, 30.0f, 4.5f);
        space (haze);
        haze.set (ParamIDs::patchLevel, -12.0f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (haze, "LUSH (more detune)", { 2, 0.6f, 0.35f, 0.35f, 0.9f, 0.4f });
        x.add (haze);

        auto strings = x.make ("Midnight Strings", 5, { "Pad", "Strings", "Ensemble", "Swell" },
            "Dreamy string-machine strings: saws a hair apart bowing in slowly, a bright but soft top, through a fast, "
            "shimmering ensemble chorus and a slower one on top of it, into a big hall. Press into the keys after "
            "playing (aftertouch) to swell. MACROS: BRIGHT, SPACE, ENSEMBLE (deeper shimmer).");
        strings.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.8f)
               .osc (0, "Volume", 0.5f)
               .osc (1, "On", 1.0f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.2f)
               .filter (FilterType::lp12, 3500.0f, 0.05f, 0.0f)
               .ampEnvelope (500.0f, 1000.0f, 1.0f, 1500.0f)
               .set (ParamIDs::ampVolume, 0.85f);
        route (strings, "aftertouch", ParamIDs::ampVolume, 0.1f, 200.0f);
        route (strings, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (strings, "macro3", ParamIDs::modDepth, 0.25f);
        macros (strings, "BRIGHT", "SPACE", "ENSEMBLE");
        chorus (strings, ParamIDs::modIds, MT::chorus, 0.9f, 0.5f, 0.6f);
        chorus (strings, ParamIDs::mod2Ids, MT::chorus, 0.25f, 0.4f, 0.35f);
        reverb (strings, RT::hall, 0.85f, 0.28f, 30.0f, 4.5f);
        space (strings);
        strings.set (ParamIDs::patchLevel, -11.4f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (strings, "ENSEMBLE (deeper shimmer)", { 2, 0.55f, 0.3f, 0.3f, 0.9f, 0.45f });
        x.add (strings);

        auto afterglow = x.make ("Afterglow", 6, { "Pad", "Step Seq", "Swell", "Shimmer" },
            "A slow, warm swell that shimmers: soft saws and a triangle an octave up rising over two seconds, the "
            "upper shimmer gently gated in sixteenths (the STEP SEQUENCER, in time with your song), a chorus and a "
            "cathedral. MACROS: BRIGHT, SPACE, GATE (deeper gating).");
        afterglow.osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.9f)
                 .osc (0, "Volume", 0.45f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.3f)
                 .filter (FilterType::lp24, 2000.0f, 0.1f, 0.0f)
                 .ampEnvelope (2000.0f, 2000.0f, 1.0f, 3000.0f)
                 .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqMode, (float) synth::seq::Mode::stereo).set (ParamIDs::seqWidth, 0.6f)
                 .set (ParamIDs::seqVolMix, 0.35f).set (ParamIDs::seqGate, 0.6f).set (ParamIDs::seqContour, 0.1f)
                 .set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        route (afterglow, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (afterglow, "macro3", ParamIDs::seqVolMix, 0.4f);
        macros (afterglow, "BRIGHT", "SPACE", "GATE");
        chorus (afterglow, ParamIDs::modIds, MT::chorus, 0.4f, 0.45f, 0.45f);
        reverb (afterglow, RT::cathedral, 0.5f, 0.3f, 40.0f, 4.5f);
        space (afterglow);
        afterglow.set (ParamIDs::patchLevel, -8.6f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (afterglow, "GATE (deeper gating)", { 2, 0.55f, 0.3f, 0.3f, 0.9f, 0.45f });
        x.add (afterglow);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto octave = x.make ("Octave Drive", 7, { "Bass", "Arp", "Octave", "Pulse" },
            "The classic synthwave octave bass: hold a note and ARP 1 bounces it between its root and the octave in "
            "sixteenths, in time with your song, a saw and a pulse with a snappy filter, a sine underneath. MACROS: "
            "BRIGHT, SPACE, SNAP (more filter snap).");
        octave.osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f).osc (0, "SendReverb", 0.1f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.25f)
              .osc (1, "Arp", 1.0f)
              .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.35f).osc (2, "Arp", 1.0f).osc (2, "Filter", 0.0f)
              .osc (2, "SendReverb", 0.0f)
              .filter (FilterType::lp24, 500.0f, 0.25f, 0.35f).filterEnvelope (0.0f, 120.0f, 0.1f, 80.0f)
              .set (ParamIDs::fltVel, 0.3f)
              .ampEnvelope (2.0f, 180.0f, 0.5f, 70.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.6f);
        {
            auto& p = octave.rig.modulation.arps[0].patterns[0];
            for (int k = 0; k < 16; ++k)
            {
                p.blocks[(size_t) k].transpose = k % 2 == 1 ? 12 : 0;
                p.blocks[(size_t) k].velocity = k % 4 == 0 ? 1.0f : 0.75f;
            }
        }
        route (octave, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (octave, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (octave, "BRIGHT", "SPACE", "SNAP");
        chorus (octave, ParamIDs::modIds, MT::chorus, 0.4f, 0.3f, 0.25f);
        reverb (octave, RT::room, 0.5f, 0.1f);
        space (octave);
        octave.set (ParamIDs::patchLevel, -0.2f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (octave, "SNAP (more filter snap)", { 1, 0.35f, 0.2f, 0.15f, 0.3f, 0.55f });
        x.add (octave);

        auto velvet = x.make ("Velvet Bass", 8, { "Bass", "Analog", "Warm", "Mono" },
            "A round, warm analog bass for slow songs: a saw through a dark filter that opens a little on each note, "
            "a sine underneath for weight, a soft glide, a touch of chorus. MACROS: BRIGHT, SPACE, WARMTH (SHAPER 1's "
            "tube).");
        velvet.osc (0, "Volume", 0.5f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.5f).osc (1, "Filter", 0.0f).osc (1, "PhaseRand", 0.0f)
              .osc (1, "SendReverb", 0.0f)
              .shaper (0, synth::ShaperModel::tube, 0.05f)
              .filter (FilterType::lp24, 350.0f, 0.15f, 0.25f).filterEnvelope (0.0f, 400.0f, 0.3f, 300.0f)
              .set (ParamIDs::fltVel, 0.4f)
              .ampEnvelope (5.0f, 800.0f, 0.85f, 250.0f)
              .mono (40.0f);
        route (velvet, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (velvet, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (velvet, "BRIGHT", "SPACE", "WARMTH");
        chorus (velvet, ParamIDs::modIds, MT::chorus, 0.3f, 0.25f, 0.2f);
        reverb (velvet, RT::room, 0.4f, 0.08f);
        space (velvet);
        velvet.set (ParamIDs::patchLevel, -4.4f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (velvet, "WARMTH (SHAPER 1's tube)", { 1, 0.3f, 0.2f, 0.15f, 0.3f, 0.55f });
        x.add (velvet);

        auto rubber = x.make ("Rubber Pulse", 9, { "Bass", "Resonance", "Filter", "Bouncy" },
            "A bouncy, rubbery bass: a square through a resonant filter that snaps open and closes on each note with "
            "a quick pitch blip into the note, harder playing opening it further. MACROS: BRIGHT, SPACE, BOUNCE (a "
            "bigger snap).");
        rubber.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.6f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f).osc (1, "Filter", 0.0f)
              .osc (1, "SendReverb", 0.0f)
              .filter (FilterType::lp24, 300.0f, 0.5f, 0.5f).filterEnvelope (0.0f, 180.0f, 0.1f, 120.0f)
              .set (ParamIDs::fltVel, 0.4f).set (ParamIDs::fltEnvVel, 0.5f)
              .ampEnvelope (2.0f, 400.0f, 0.6f, 120.0f)
              .mono (0.0f)
              .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        rubber.rig.modulation.pitchEnvelope = { { SP9 { 0.0f, 1.0f, -0.6f }, SP9 { 0.04f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (rubber, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (rubber, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (rubber, "BRIGHT", "SPACE", "BOUNCE");
        chorus (rubber, ParamIDs::modIds, MT::chorus, 0.4f, 0.25f, 0.2f);
        delay (rubber, "1/8 dotted", 0.2f, 0.08f, false, 3.5f);
        reverb (rubber, RT::room, 0.4f, 0.08f);
        space (rubber);
        rubber.set (ParamIDs::patchLevel, -4.9f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (rubber, "BOUNCE (a bigger snap)", { 1, 0.35f, 0.2f, 0.15f, 0.3f, 0.55f });
        x.add (rubber);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto tine = x.make ("Glass Tine", 10, { "Keys", "FM", "Electric Piano", "Chorus" },
            "The '80s digital electric piano: a sine struck by a high, fast-fading FM tine (about fourteen times its "
            "pitch) for the glassy attack, a soft 1:1 body underneath, brighter the harder you play, a lush chorus and "
            "a plate. MACROS: BRIGHT (more tine), SPACE, BARK (more body FM).");
        tine.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
            .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.05f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.5f).osc (2, "PhaseRand", 0.0f)
            .osc (2, "FmSource", (float) (synth::fmFirstOscillator + 3)).osc (2, "FmAmount", 0.0f)
            .osc (3, "On", 1.0f).osc (3, "Wave", sine).osc (3, "Transpose", 46.0f).osc (3, "Fine", -31.0f).osc (3, "Volume", 1.0f)
            .osc (3, "PhaseRand", 0.0f)
            .ampEnvelope (1.0f, 3500.0f, 0.15f, 500.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.6f)
            .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
            .set (ParamIDs::env (1, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        tine.rig.modulation.envelopes[0] = { { SP9 { 0.0f, 1.0f, -0.7f }, SP9 { 0.25f, 0.0f, 0.0f } }, -1, -1, -1 };
        tine.rig.modulation.envelopes[1] = { { SP9 { 0.0f, 1.0f, -0.5f }, SP9 { 0.8f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (tine, "env1", ParamIDs::osc (2, "FmAmount"), 0.12f);
        route (tine, "env2", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (tine, "macro1", ParamIDs::osc (2, "FmAmount"), 0.1f);
        route (tine, "macro3", ParamIDs::osc (0, "FmAmount"), 0.12f);
        macros (tine, "BRIGHT", "SPACE", "BARK");
        chorus (tine, ParamIDs::modIds, MT::chorus, 0.6f, 0.5f, 0.5f);
        delay (tine, "1/4 dotted", 0.25f, 0.1f, true, 5.0f);
        reverb (tine, RT::plate, 0.6f, 0.2f);
        space (tine);
        tine.set (ParamIDs::patchLevel, -13.5f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (tine, "BARK (more body FM)", { 1, 0.55f, 0.4f, 0.25f, 0.8f, 0.5f });
        x.add (tine);

        auto stab = x.make ("Retro Stab", 11, { "Keys", "Polysynth", "Stab", "Chorus" },
            "A big, chorused '80s polysynth chord stab: three saws and a pulse snapping open and closing in a third of "
            "a second, a little sustain left behind, into a wide chorus, dotted echoes and a hall. Play chords. MACROS: "
            "BRIGHT, SPACE, PUNCH (a bigger snap).");
        stab.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.14f).osc (0, "UniSpread", 0.8f)
            .osc (0, "Volume", 0.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Volume", 0.25f)
            .filter (FilterType::lp24, 900.0f, 0.2f, 0.5f).filterEnvelope (0.0f, 350.0f, 0.15f, 300.0f)
            .set (ParamIDs::fltVel, 0.4f).set (ParamIDs::fltKey, 0.4f)
            .ampEnvelope (2.0f, 700.0f, 0.3f, 400.0f).set (ParamIDs::ampVel, 0.5f);
        route (stab, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (stab, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (stab, "BRIGHT", "SPACE", "PUNCH");
        chorus (stab, ParamIDs::modIds, MT::chorus, 0.5f, 0.5f, 0.5f);
        delay (stab, "1/8 dotted", 0.3f, 0.15f, true, 4.5f);
        reverb (stab, RT::hall, 0.7f, 0.22f);
        space (stab);
        stab.set (ParamIDs::patchLevel, -9.3f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (stab, "PUNCH (a bigger snap)", { 0, 0.5f, 0.35f, 0.3f, 0.8f, 0.5f });
        x.add (stab);

        auto cassette = x.make ("Cassette Keys", 12, { "Keys", "Lo-Fi", "Tape", "Warm" },
            "Soft keys from an old cassette: a triangle and a narrow pulse with a gentle filter pluck, warbling like "
            "worn tape (MOD: vibrato) and darkened (LO-FI: cassette), with a chorus and a room. MACROS: BRIGHT, SPACE, "
            "WOBBLE (more warble).");
        cassette.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f)
                .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.25f).osc (1, "Volume", 0.2f)
                .filter (FilterType::lp12, 1800.0f, 0.05f, 0.3f).filterEnvelope (0.0f, 600.0f, 0.4f, 500.0f)
                .set (ParamIDs::fltVel, 0.4f)
                .ampEnvelope (3.0f, 2500.0f, 0.4f, 800.0f).set (ParamIDs::ampVel, 0.5f)
                .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
                .set (ParamIDs::lofiAmount, 2.5f).set (ParamIDs::lofiMix, 0.4f).set (ParamIDs::lofiNoise, 0.0f);
        route (cassette, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (cassette, "macro3", ParamIDs::mod2Depth, 0.3f);
        macros (cassette, "BRIGHT", "SPACE", "WOBBLE");
        chorus (cassette, ParamIDs::modIds, MT::chorus, 0.5f, 0.4f, 0.4f);
        chorus (cassette, ParamIDs::mod2Ids, MT::vibrato, 0.5f, 0.2f, 1.0f);
        reverb (cassette, RT::room, 0.5f, 0.18f);
        space (cassette);
        cassette.set (ParamIDs::patchLevel, -7.7f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (cassette, "WOBBLE (more warble)", { 1, 0.55f, 0.45f, 0.25f, 0.8f, 0.4f });
        x.add (cassette);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto lights = x.make ("Highway Lights", 13, { "Arp", "Pluck", "Chorus", "Delay" },
            "Lights streaming past: hold a chord and ARP 1 runs it up three octaves in sixteenths as bright, chorused "
            "pulse plucks, in time with your song, multiplied by dotted-eighth ping-pong echoes into a hall. MACROS: "
            "BRIGHT, SPACE, SPARKLE (a sine an octave up).");
        lights.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.0f).osc (1, "Arp", 1.0f)
              .filter (FilterType::lp24, 2500.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 150.0f, 0.1f, 100.0f)
              .ampEnvelope (1.0f, 250.0f, 0.2f, 150.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 3.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.5f);
        route (lights, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (lights, "macro3", ParamIDs::osc (1, "Volume"), 0.15f);
        macros (lights, "BRIGHT", "SPACE", "SPARKLE");
        chorus (lights, ParamIDs::modIds, MT::chorus, 0.5f, 0.45f, 0.45f);
        delay (lights, "1/8 dotted", 0.45f, 0.25f, true, 5.0f);
        reverb (lights, RT::hall, 0.7f, 0.2f);
        space (lights);
        lights.set (ParamIDs::patchLevel, -3.2f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (lights, "SPARKLE (a sine an octave up)", { 0, 0.6f, 0.45f, 0.3f, 0.9f, 0.5f });
        x.add (lights);

        auto runner = x.make ("Night Runner", 14, { "Arp", "Pluck", "Sequencer", "Driving" },
            "Driving through the night: ARP 1 runs the chord you hold in sixteenths on a chorused saw pluck, in time with "
            "your song. MACROS: BRIGHT, SPACE, SNAP.");
        runner.osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.6f)
              .osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
              .filter (FilterType::lp24, 1200.0f, 0.25f, 0.35f).filterEnvelope (0.0f, 120.0f, 0.05f, 90.0f)
              .ampEnvelope (1.0f, 180.0f, 0.3f, 100.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.5f);
        route (runner, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (runner, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (runner, "BRIGHT", "SPACE", "SNAP");
        chorus (runner, ParamIDs::modIds, MT::chorus, 0.5f, 0.4f, 0.4f);
        delay (runner, "1/8 dotted", 0.3f, 0.12f, true, 4.5f);
        reverb (runner, RT::plate, 0.6f, 0.15f);
        space (runner);
        runner.set (ParamIDs::patchLevel, 1.8f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        passing (runner, "SNAP", { 0, 0.55f, 0.4f, 0.3f, 0.9f, 0.5f });
        x.add (runner);

        auto heartline = x.make ("Heartline", 15, { "Arp", "Emotional", "Delay", "Soft" },
            "An emotional, slow arpeggio: hold a chord and ARP 1 walks up and down it over two octaves in eighths on a "
            "soft triangle and sine, each note answered by quarter-note ping-pong echoes, in a wide chorus and a "
            "cathedral. MACROS: BRIGHT, SPACE, ECHO (longer echoes).");
        heartline.osc (0, "Wave", triangle).osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.2f).osc (1, "Arp", 1.0f)
                 .filter (FilterType::lp12, 2500.0f, 0.05f, 0.2f).filterEnvelope (0.0f, 300.0f, 0.3f, 300.0f)
                 .ampEnvelope (5.0f, 900.0f, 0.3f, 500.0f)
                 .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                 .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
                 .set (ParamIDs::arp (0, "Gate"), 0.8f);
        route (heartline, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (heartline, "macro3", ParamIDs::delayFeedback, 0.25f);
        macros (heartline, "BRIGHT", "SPACE", "ECHO");
        chorus (heartline, ParamIDs::modIds, MT::chorus, 0.4f, 0.5f, 0.5f);
        delay (heartline, "1/4", 0.45f, 0.25f, true, 4.5f);
        heartline.set (ParamIDs::delayToReverb, 0.4f);
        reverb (heartline, RT::cathedral, 0.5f, 0.28f);
        space (heartline);
        heartline.set (ParamIDs::patchLevel, -3.8f);   // LEVEL: balanced against the others (measured, not by ear)
        passing (heartline, "ECHO (longer echoes)", { 1, 0.6f, 0.45f, 0.3f, 0.9f, 0.45f });
        x.add (heartline);
    }
    // UNSPOKEN (the owner's name): inspired by the score of a 2016 film about first contact and a language that
    // changes how you see time: human voices breathing, murmuring and stuttering in eerie layers, vast swelling
    // drones, circular phrases that run forwards and back, cold fog, quiet awe and grief. No title, characters,
    // terms or music from the film: our own sounds.
    void unspoken (Expansion x)
    {
        using SP10 = synth::mod::ShapePoint;
        using GM = effects::Glitch::Mode;
        const auto lightNoise = [] (Builder& b, int o, float noise, float volume)   // breath: light noise, no tone
        {
            b.osc (o, "On", 1.0f).osc (o, "Gain", 0.0f).osc (o, "Noise", noise).osc (o, "NoiseColour", 0.15f)
             .osc (o, "NoiseWidth", 2.0f).osc (o, "Volume", volume);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto voice = x.make ("Glass Voice", 1, { "Lead", "Vowel", "Wavetable", "Mono" },
            "A clear, wordless voice: the Vowels table singing \"eh\", its vowel drifting slowly on its own (LFO 1), "
            "FORMANT nudged up for a smaller, stranger throat, a slow glide between notes, vibrato coming in late, a "
            "touch of breath, quarter-note echoes and a hall. MACROS: BRIGHT, SPACE, VOWEL (towards \"oo\"). MOD WHEEL: "
            "vibrato.");
        voice.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.25f).osc (0, "Volume", 1.5f)
             .osc (0, "Formant", 0.12f).osc (0, "FormantMode", 2.0f)
             .osc (0, "VibAmount", 12.0f).osc (0, "VibRate", 4.8f).osc (0, "VibFade", 700.0f).osc (0, "VibCurve", 0.5f)
             .filter (FilterType::lp12, 3800.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.4f)
             .ampEnvelope (60.0f, 600.0f, 0.9f, 900.0f)
             .mono (140.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.3f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.3f);
        lightNoise (voice, 1, 0.3f, 0.025f);
        route (voice, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.08f);
        route (voice, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.25f);
        route (voice, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (voice, "macro3", ParamIDs::osc (0, "WtIndex"), 0.6f);   // the table runs ah, eh, ee, oh, oo
        macros (voice, "BRIGHT", "SPACE", "VOWEL");
        delay (voice, "1/4", 0.35f, 0.18f, true, 4.0f);
        reverb (voice, RT::hall, 0.75f, 0.28f, 40.0f, 4.0f);
        eqBand (voice, 0, eq::Shape::lowCut, 150.0f, 0.0f);
        space (voice);
        voice.set (ParamIDs::patchLevel, 0.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (voice);

        auto call = x.make ("Distant Call", 2, { "Lead", "Pitch Envelope", "Glide", "Mono" },
            "A long call from far away, like a whale's: a sine and a soft triangle an octave down that bend up into "
            "every note (the PITCH envelope, a slow scoop from below) and slide a long way between them, a faint FM "
            "growl in the throat, long echoes and a cathedral. MACROS: BRIGHT, SPACE, SCOOP (a deeper bend). MOD "
            "WHEEL: growl (more FM).");
        call.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "FmAmount", 0.06f).osc (0, "ModRate", 0.5f)
            .osc (0, "VibAmount", 8.0f).osc (0, "VibRate", 3.5f).osc (0, "VibFade", 1200.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
            .filter (FilterType::lp12, 2500.0f, 0.15f, 0.0f)
            .ampEnvelope (250.0f, 800.0f, 0.9f, 1500.0f)
            .mono (450.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.5f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 5.0f);
        call.rig.modulation.pitchEnvelope = { { SP10 { 0.0f, 0.0f, 0.4f }, SP10 { 0.6f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (call, "modWheel", ParamIDs::osc (0, "FmAmount"), 0.3f, 100.0f);
        route (call, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (call, "macro3", ParamIDs::pitchEnvRange, 0.25f);
        macros (call, "BRIGHT", "SPACE", "SCOOP");
        delay (call, "1/2", 0.45f, 0.2f, true, 3.5f);
        call.set (ParamIDs::delayToReverb, 0.4f);
        reverb (call, RT::cathedral, 0.6f, 0.3f, 60.0f, 3.5f);
        space (call);
        call.set (ParamIDs::patchLevel, -4.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (call);

        auto signal = x.make ("Signal Tone", 3, { "Lead", "Sine", "Tremolo", "Mono" },
            "A pure tone that doesn't quite hold still: a sine with a quiet one an octave and a fifth up, both "
            "wandering a few cents in pitch (a slow random LFO, each its own way), a slow tremolo pulsing through it "
            "(MOD), so it sounds like a signal from somewhere else. A plate and dotted-quarter echoes. MACROS: BRIGHT "
            "(more of the upper tone), SPACE, WOBBLE (a slow vibrato on top). MOD WHEEL: a deeper tremolo.");
        signal.osc (0, "Wave", sine).osc (0, "Volume", 0.75f).osc (0, "VibRate", 0.9f).osc (0, "VibAmount", 0.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 19.0f).osc (1, "Volume", 0.1f).osc (1, "Pan", 0.3f)
              .osc (1, "VibRate", 0.9f).osc (1, "VibAmount", 0.0f)
              .ampEnvelope (40.0f, 500.0f, 0.95f, 1000.0f)
              .mono (60.0f)
              .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.7f);
        route (signal, "lfo1", ParamIDs::osc (0, "Fine"), 0.04f);
        route (signal, "lfo1", ParamIDs::osc (1, "Fine"), -0.06f);
        route (signal, "modWheel", ParamIDs::modIds.depth, 0.4f);
        route (signal, "macro1", ParamIDs::osc (1, "Volume"), 0.08f);
        route (signal, "macro3", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (signal, "macro3", ParamIDs::osc (1, "VibAmount"), 0.2f);
        macros (signal, "BRIGHT", "SPACE", "WOBBLE");
        chorus (signal, ParamIDs::modIds, MT::tremolo, 0.25f, 0.35f, 1.0f);
        delay (signal, "1/4 dotted", 0.4f, 0.18f, true, 4.0f);
        reverb (signal, RT::plate, 0.7f, 0.25f);
        space (signal);
        signal.set (ParamIDs::patchLevel, -4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (signal);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto choir = x.make ("Breathing Choir", 4, { "Pad", "Choir", "Wavetable", "Travel", "Noise" },
            "A slow choir that breathes: the Vowels table in a wide unison, its vowel rocking from \"oo\" to \"oh\" and "
            "back every eight seconds or so (TRAVEL, free running), a darker choir an octave down, and breath through "
            "it all: light noise swelling in and out with LFO 1, as if the singers inhale together. A cathedral. "
            "MACROS: BRIGHT, SPACE, BREATH (more air). MOD WHEEL: drift (the unison detune).");
        choir.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 1.0f)
             .osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.9f)
             .osc (0, "Volume", 0.5f)
             .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::pingPong).osc (0, "TrRate", 0.12f)
             .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrStrength", -0.3f).osc (0, "TrSmooth", 0.3f)
             .osc (1, "On", 1.0f).osc (1, "Wave", wavetable).osc (1, "Table", table ("Vowels")).osc (1, "WtIndex", 0.8f)
             .osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
             .osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.6f).osc (1, "UniDetune", 0.12f).osc (1, "UniSpread", 0.6f)
             .filter (FilterType::lp12, 2800.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.3f)
             .ampEnvelope (1500.0f, 2000.0f, 0.9f, 3000.0f)
             .set (ParamIDs::lfo (0, "Rate"), 0.18f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        lightNoise (choir, 2, 0.3f, 0.04f);
        choir.rig.modulation.travel[0].envelope = { { SP10 { 0.0f, 0.0f, 0.0f }, SP10 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (choir, "lfo1", ParamIDs::osc (2, "Volume"), 0.05f);
        route (choir, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (choir, "macro3", ParamIDs::osc (2, "Volume"), 0.12f);
        route (choir, "modWheel", ParamIDs::osc (0, "UniDetune"), 0.35f);
        macros (choir, "BRIGHT", "SPACE", "BREATH");
        chorus (choir, ParamIDs::modIds, MT::dimension, 0.25f, 0.5f, 0.4f);
        reverb (choir, RT::cathedral, 0.45f, 0.35f, 40.0f, 4.0f);
        eqBand (choir, 0, eq::Shape::lowCut, 120.0f, 0.0f);
        space (choir);
        choir.set (ParamIDs::patchLevel, 0.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (choir);

        auto circle = x.make ("Circle Drone", 5, { "Pad", "Drone", "Wavetable", "Travel" },
            "A drone whose colour goes round and round: the Harmonic Rise table looping slowly through its frames "
            "(TRAVEL, LOOP, about ten seconds a turn), a second copy a fifth up turning the other way round, a sub "
            "under it, and the whole sound circling the room with a slow auto-pan. MACROS: BRIGHT, SPACE, MOTION (a "
            "wider turn). MOD WHEEL: the pinch of the wave (a nasal edge).");
        circle.osc (0, "Wave", wavetable).osc (0, "Table", table ("Harmonic Rise")).osc (0, "WtIndex", 0.1f)
              .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.7f)
              .osc (0, "Sub", 0.3f).osc (0, "Volume", 0.5f)
              .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::loop).osc (0, "TrRate", 0.1f)
              .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrStrength", 0.5f).osc (0, "TrSmooth", 0.4f)
              .osc (1, "On", 1.0f).osc (1, "Wave", wavetable).osc (1, "Table", table ("Harmonic Rise")).osc (1, "WtIndex", 0.6f)
              .osc (1, "Transpose", 7.0f).osc (1, "Volume", 0.25f)
              .osc (1, "TrOn", 1.0f).osc (1, "TrMode", (float) synth::travel::Mode::loop).osc (1, "TrRate", 0.07f)
              .osc (1, "TrSync", (float) synth::mod::LfoSync::free).osc (1, "TrStrength", -0.5f).osc (1, "TrSmooth", 0.4f)
              .filter (FilterType::lp24, 2200.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.3f)
              .ampEnvelope (2500.0f, 2000.0f, 1.0f, 3500.0f);
        circle.rig.modulation.travel[0].envelope = { { SP10 { 0.0f, 0.0f, 0.0f }, SP10 { 0.5f, 1.0f, 0.0f }, SP10 { 1.0f, 0.0f, 0.0f } }, -1, -1, -1 };
        circle.rig.modulation.travel[1].envelope = circle.rig.modulation.travel[0].envelope;
        chorus (circle, ParamIDs::mod2Ids, MT::autoPan, 0.08f, 0.5f, 1.0f);
        route (circle, "modWheel", ParamIDs::osc (0, "Pinch"), 0.4f);
        route (circle, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (circle, "macro3", ParamIDs::osc (0, "TrStrength"), 0.25f);
        macros (circle, "BRIGHT", "SPACE", "MOTION");
        reverb (circle, RT::hall, 0.9f, 0.3f, 30.0f, 4.0f);
        space (circle);
        circle.set (ParamIDs::patchLevel, -6.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (circle);

        auto mist = x.make ("Low Mist", 6, { "Pad", "Reverse", "Noise", "Phaser" },
            "A cold fog: dark saws a hair apart and filtered noise, swelling in very slowly through a reverb that "
            "plays backwards, so every chord seems to arrive before it's played, with a slow phaser drifting through "
            "it. MACROS: BRIGHT, SPACE, FOG (more of the noise). MOD WHEEL: opens the fog (the filter).");
        mist.osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 1.0f)
            .osc (0, "Volume", 0.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
            .filter (FilterType::lp24, 700.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.3f)
            .ampEnvelope (2000.0f, 2000.0f, 1.0f, 3000.0f);
        lightNoise (mist, 2, 0.4f, 0.25f);
        route (mist, "modWheel", ParamIDs::fltCutoff, 0.25f, 200.0f);
        route (mist, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (mist, "macro3", ParamIDs::osc (2, "Volume"), 0.3f);
        macros (mist, "BRIGHT", "SPACE", "FOG");
        chorus (mist, ParamIDs::modIds, MT::phaser, 0.1f, 0.6f, 0.5f);
        reverb (mist, RT::reverse, 0.8f, 0.4f, 20.0f, 3.5f);
        eqBand (mist, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (mist);
        mist.set (ParamIDs::patchLevel, -4.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (mist);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto hull = x.make ("Vast Hull", 7, { "Bass", "Drone", "FM", "Metallic" },
            "The sound of something enormous and metal hanging in the air: a low sine FM'd by a second oscillator "
            "tuned off-harmonic (a tritone and a few cents up), so it rings like a struck hull, a resonant filter "
            "breathing slowly (LFO 1), a clean sub under it and a dark hall. MACROS: BRIGHT, SPACE, METAL (more FM). "
            "MOD WHEEL: opens the resonance.");
        hull.osc (0, "Wave", sine).osc (0, "Volume", 0.8f)
            .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.2f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 18.0f).osc (1, "Fine", 7.0f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.35f).osc (2, "Filter", 0.0f)
            .filter (FilterType::lp24, 600.0f, 0.4f, 0.0f).set (ParamIDs::fltKey, 0.5f)
            .ampEnvelope (400.0f, 2000.0f, 1.0f, 2000.0f)
            .mono (0.0f)
            .set (ParamIDs::lfo (0, "Rate"), 0.15f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (hull, "lfo1", ParamIDs::fltCutoff, 0.12f);
        route (hull, "modWheel", ParamIDs::fltRes, 0.3f);
        route (hull, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (hull, "macro3", ParamIDs::osc (0, "FmAmount"), 0.35f);
        macros (hull, "BRIGHT", "SPACE", "METAL");
        reverb (hull, RT::hall, 0.7f, 0.22f, 20.0f, 3.0f);
        space (hull);
        hull.set (ParamIDs::patchLevel, -7.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hull);

        auto tide = x.make ("Tidal Swell", 8, { "Bass", "Brass", "Swell", "Unison" },
            "A low swell like strings and brass rising and sinking: saws in a slow unison with a quiet pulse an "
            "octave up, the filter opening as each note grows (a slow filter envelope) and then rising and falling "
            "once every two bars with the song (LFO 1), like a tide. Press into the keys (aftertouch) to push it. "
            "MACROS: BRIGHT, SPACE, BRASS (more of the pulse).");
        tide.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.5f)
            .osc (0, "Volume", 0.6f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
            .filter (FilterType::lp24, 350.0f, 0.15f, 0.35f).filterEnvelope (1500.0f, 2500.0f, 0.5f, 1500.0f)
            .set (ParamIDs::fltKey, 0.3f)
            .ampEnvelope (900.0f, 1500.0f, 0.9f, 1500.0f)
            .mono (0.0f)
            .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight).set (ParamIDs::lfo (0, "Division"), division ("2/1"))
            .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (tide, "lfo1", ParamIDs::fltCutoff, 0.1f);
        route (tide, "aftertouch", ParamIDs::fltCutoff, 0.15f, 150.0f);
        route (tide, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (tide, "macro3", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (tide, "BRIGHT", "SPACE", "BRASS");
        reverb (tide, RT::hall, 0.8f, 0.2f, 30.0f, 3.5f);
        space (tide);
        tide.set (ParamIDs::patchLevel, -3.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (tide);

        auto murmur = x.make ("Deep Murmur", 9, { "Bass", "Vowel", "Sub", "Noise" },
            "A dark low voice under everything: a clean sine sub with the Vowels table's \"oo\" an octave up "
            "murmuring on top of it, the vowel opening a little towards \"oh\" with each note (a short modulation "
            "envelope), and a breath of noise through the filter. MACROS: BRIGHT, SPACE, MOUTH (the vowel open "
            "towards \"ah\"). MOD WHEEL: more breath.");
        murmur.osc (0, "Wave", sine).osc (0, "Volume", 0.75f).osc (0, "Filter", 0.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", wavetable).osc (1, "Table", table ("Vowels")).osc (1, "WtIndex", 1.0f)
              .osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.4f)
              .filter (FilterType::lp12, 1200.0f, 0.15f, 0.0f).set (ParamIDs::fltKey, 0.4f)
              .ampEnvelope (30.0f, 800.0f, 0.85f, 500.0f)
              .mono (40.0f)
              .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        lightNoise (murmur, 2, 0.25f, 0.06f);
        murmur.rig.modulation.envelopes[0] = { { SP10 { 0.0f, 0.0f, 0.3f }, SP10 { 0.3f, 1.0f, -0.3f }, SP10 { 1.2f, 0.4f, 0.0f } }, -1, -1, -1 };
        route (murmur, "env1", ParamIDs::osc (1, "WtIndex"), -0.15f);
        route (murmur, "modWheel", ParamIDs::osc (2, "Volume"), 0.15f);
        route (murmur, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (murmur, "macro3", ParamIDs::osc (1, "WtIndex"), -1.0f);
        macros (murmur, "BRIGHT", "SPACE", "MOUTH");
        reverb (murmur, RT::room, 0.5f, 0.15f);
        space (murmur);
        murmur.set (ParamIDs::patchLevel, -3.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (murmur);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto bells = x.make ("Ink Bells", 10, { "Keys", "Bells", "FM", "Swell" },
            "Soft bells that bloom like ink spreading in water: a sine struck with FM from a second sine 3.5 times "
            "its pitch (bright at the strike, fading over a second and a half), and under it a soft triangle that "
            "swells in after the strike (a modulation envelope), into ping-pong echoes and a big plate. MACROS: "
            "BRIGHT (a harder strike), SPACE, BLOOM (more of the swell).");
        bells.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 2)).osc (0, "FmAmount", 0.05f)
             .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 21.0f).osc (2, "Fine", 69.0f)   // 3.5 x: 21.69 semitones
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.05f).osc (1, "Pan", 0.2f)
             .ampEnvelope (2.0f, 4000.0f, 0.0f, 1500.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.6f)
             .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
             .set (ParamIDs::env (1, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        bells.rig.modulation.envelopes[0] = { { SP10 { 0.0f, 1.0f, -0.5f }, SP10 { 1.5f, 0.0f, 0.0f } }, -1, -1, -1 };
        bells.rig.modulation.envelopes[1] = { { SP10 { 0.0f, 0.0f, 0.3f }, SP10 { 1.2f, 1.0f, 0.0f }, SP10 { 4.0f, 0.3f, 0.0f } }, -1, -1, -1 };
        route (bells, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (bells, "velocity", ParamIDs::osc (0, "FmAmount"), 0.08f);
        route (bells, "env2", ParamIDs::osc (1, "Volume"), 0.12f);
        route (bells, "macro1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (bells, "macro3", ParamIDs::osc (1, "Volume"), 0.15f);
        macros (bells, "BRIGHT", "SPACE", "BLOOM");
        delay (bells, "1/4 dotted", 0.4f, 0.2f, true, 4.0f);
        reverb (bells, RT::plate, 0.8f, 0.28f, 30.0f, 4.5f);
        space (bells);
        bells.set (ParamIDs::patchLevel, -4.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (bells);

        auto memory = x.make ("Memory Loop", 11, { "Keys", "Piano", "Reverse", "Delay" },
            "NEGATIVE SPACE's soft felt piano, played as a memory: every note answered by its own reflection coming "
            "back the wrong way round (a reverb played backwards), with soft half-note echoes trailing after. The MOD "
            "WHEEL adds the felt's noise. MACROS: BRIGHT, SPACE, ECHO (more of the echoes).");
        softPiano (memory);
        route (memory, "macro3", ParamIDs::delayMix, 0.25f);
        macros (memory, "BRIGHT", "SPACE", "ECHO");
        delay (memory, "1/2", 0.35f, 0.15f, false, 3.5f);
        reverb (memory, RT::reverse, 0.7f, 0.35f, 10.0f, 4.0f);
        space (memory);
        memory.set (ParamIDs::patchLevel, -7.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (memory);

        auto fog = x.make ("Fog Glass", 12, { "Keys", "Wavetable", "Chorder", "Glass" },
            "Cold, glassy keys in a huge space: the Harmonic Rise table's bright end plucked by the filter, a quiet "
            "copy an octave and a fifth up (the chorder) for the glass, a slow shimmering chorus, and a cathedral. "
            "MACROS: BRIGHT, SPACE, SHINE (further along the table). MOD WHEEL: a slow shimmer in pitch.");
        fog.osc (0, "Wave", wavetable).osc (0, "Table", table ("Harmonic Rise")).osc (0, "WtIndex", 0.6f).osc (0, "Volume", 0.65f)
           .osc (0, "PhaseRand", 0.0f)
           .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 19.0f).osc (0, "Ch1Vol", 0.25f)
           .osc (0, "VibRate", 0.5f).osc (0, "VibAmount", 0.0f)
           .filter (FilterType::lp12, 1500.0f, 0.1f, 0.45f).filterEnvelope (0.0f, 900.0f, 0.2f, 900.0f)
           .set (ParamIDs::fltKey, 0.6f).set (ParamIDs::fltEnvVel, 0.6f)
           .ampEnvelope (3.0f, 3500.0f, 0.15f, 1400.0f).set (ParamIDs::ampVel, 0.6f);
        route (fog, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        route (fog, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (fog, "macro3", ParamIDs::osc (0, "WtIndex"), 0.4f);
        macros (fog, "BRIGHT", "SPACE", "SHINE");
        chorus (fog, ParamIDs::modIds, MT::chorus, 0.2f, 0.45f, 0.4f);
        reverb (fog, RT::cathedral, 0.6f, 0.32f, 30.0f, 4.5f);
        eqBand (fog, 0, eq::Shape::lowCut, 120.0f, 0.0f);
        space (fog);
        fog.set (ParamIDs::patchLevel, -7.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (fog);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto stutter = x.make ("Stutter Choir", 13, { "Arp", "Choir", "Ratchet", "Vowel" },
            "Choir voices that stutter, as if trying to say something: a held chord through ARP 1 in sixteenths, "
            "short vowel syllables (the Vowels table, each note's vowel opening from \"oo\" to \"ah\") broken into "
            "quick bursts of two, three and four (ratchets) with gaps between, a little humanize so it never quite "
            "repeats, and a cathedral. MACROS: BRIGHT, SPACE, VOWEL (the syllables open sooner).");
        stutter.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.9f)
               .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.8f)
               .osc (0, "Volume", 1.3f).osc (0, "Arp", 1.0f)
               .filter (FilterType::lp12, 3000.0f, 0.1f, 0.0f)
               .ampEnvelope (8.0f, 250.0f, 0.6f, 120.0f)
               .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Gate"), 0.7f)
               .set (ParamIDs::arp (0, "Humanize"), 0.2f);
        stutter.rig.modulation.envelopes[0] = { { SP10 { 0.0f, 0.0f, 0.0f }, SP10 { 0.15f, 1.0f, 0.0f } }, -1, -1, -1 };
        {
            auto& p = stutter.rig.modulation.arps[0].patterns[0];
            p = {};
            const int ratchets[16] = { 3, 0, 0, 1, 2, 0, 1, 0, 4, 0, 0, 0, 1, 2, 0, 1 };
            for (int k = 0; k < 16; ++k)
            {
                auto& b = p.blocks[(size_t) k];
                b.on = ratchets[k] > 0;
                b.ratchet = std::max (1, ratchets[k]);
                b.velocity = k % 4 == 0 ? 1.0f : 0.75f;
            }
        }
        route (stutter, "env1", ParamIDs::osc (0, "WtIndex"), -0.9f);
        route (stutter, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (stutter, "macro3", ParamIDs::osc (0, "WtIndex"), -0.4f);
        macros (stutter, "BRIGHT", "SPACE", "VOWEL");
        reverb (stutter, RT::cathedral, 0.4f, 0.3f, 30.0f, 4.0f);
        eqBand (stutter, 0, eq::Shape::lowCut, 120.0f, 0.0f);
        space (stutter);
        stutter.set (ParamIDs::patchLevel, -4.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (stutter);

        auto circular = x.make ("Circular Time", 14, { "Arp", "Pluck", "Palindrome", "Delay" },
            "A phrase that reads the same forwards and backwards: ARP 1 in eighths climbs a held chord's notes with "
            "its pattern's leaps (up a fifth, an octave, a ninth) and comes back down by exactly the same steps, soft "
            "sine-and-triangle plucks with dotted-quarter echoes folding over them and a hall. MACROS: BRIGHT, SPACE, "
            "GLOW (filter resonance).");
        circular.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.3f).osc (1, "PhaseRand", 0.0f)
                .osc (1, "Arp", 1.0f)
                .filter (FilterType::lp12, 1800.0f, 0.15f, 0.35f).filterEnvelope (0.0f, 400.0f, 0.2f, 400.0f)
                .set (ParamIDs::fltKey, 0.5f)
                .ampEnvelope (3.0f, 900.0f, 0.2f, 600.0f)
                .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Gate"), 0.8f);
        {
            auto& p = circular.rig.modulation.arps[0].patterns[0];
            p = {};
            const int leaps[16] = { 0, 0, 7, 0, 12, 0, 14, 0, 14, 0, 12, 0, 7, 0, 0, 0 };   // the same read either way
            for (int k = 0; k < 16; ++k)
            {
                auto& b = p.blocks[(size_t) k];
                b.on = true;
                b.transpose = leaps[k];
                b.velocity = leaps[k] == 14 ? 1.0f : 0.75f;
            }
        }
        route (circular, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (circular, "macro3", ParamIDs::fltRes, 0.35f);
        macros (circular, "BRIGHT", "SPACE", "GLOW");
        delay (circular, "1/4 dotted", 0.45f, 0.25f, true, 4.0f);
        reverb (circular, RT::hall, 0.7f, 0.25f);
        space (circular);
        circular.set (ParamIDs::patchLevel, -1.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (circular);

        auto breath = x.make ("Breath Pulse", 15, { "Arp", "Noise", "Step Sequencer", "Glitch" },
            "A rhythm of breaths and soft voices: a held chord of quiet \"ah\" voices and breathy noise cut into a "
            "pattern of sixteenths by the STEP SEQUENCER (some steps tied into longer breaths, the right side "
            "answering the left), through insert bus A's GLITCH repeating a little of it now and then, and a room. "
            "MACROS: BRIGHT, SPACE, BREATH (more noise against the voices).");
        breath.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.0f).osc (0, "Volume", 1.3f)
              .osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.6f)
              .osc (0, "Insert", 1.0f)
              .filter (FilterType::lp12, 2800.0f, 0.1f, 0.0f)
              .ampEnvelope (10.0f, 500.0f, 1.0f, 400.0f)
              .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqMode, (float) synth::seq::Mode::stereo).set (ParamIDs::seqWidth, 0.6f)
              .set (ParamIDs::seqVolMix, 0.95f).set (ParamIDs::seqGate, 0.8f).set (ParamIDs::seqContour, 0.35f)
              .set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f)
              .set (ParamIDs::ins (0, "SendReverb"), 0.6f).set (ParamIDs::ins (0, "SendDelay"), 0.0f);
        lightNoise (breath, 1, 0.45f, 0.5f);
        breath.osc (1, "Insert", 1.0f);
        glitch (breath, 0, GM::pitchDelay, "1/8", 2.0f, 2.0f, 0.25f);
        {
            const bool left[16]  = { 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0 };
            const bool right[16] = { 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1 };
            for (int row = 0; row < 2; ++row)
                for (int k = 0; k < synth::seq::maxSteps; ++k)
                {
                    auto& step = breath.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                    const auto at = k % 16;
                    step.on = row == 0 ? left[at] : right[at];
                    step.level = at % 4 == 0 ? 1.0f : 0.7f;
                    step.tied = at % 8 == 2;   // a longer breath twice a bar
                }
        }
        route (breath, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (breath, "macro3", ParamIDs::osc (1, "Volume"), 0.3f);
        macros (breath, "BRIGHT", "SPACE", "BREATH");
        reverb (breath, RT::room, 0.6f, 0.22f);
        eqBand (breath, 0, eq::Shape::lowCut, 150.0f, 0.0f);
        space (breath);
        breath.set (ParamIDs::patchLevel, 5.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (breath);
    }

    // CYBERSHRINE's centrepiece (0.67.0, the owner's ask): SPECTRE on every preset, MACRO 3 SPECTRE. Each preset gets
    // its own ghost (how far behind, how long it lingers, how breathy, how far it drifts, its outline moved) at MIX 0,
    // so at the macro's left it sounds exactly as approved; the macro brings the ghost in and lets it linger a little
    // longer. Its old MACRO 3 job rides along (the notes say so).
    struct Ghost
    {
        float mixTop = 0.55f;                                         // the macro's MIX at full
        float trail = 0.3f, linger = 0.45f, air = 0.6f, drift = 0.6f, shift = 0.0f;
        float lingerRise = 0.15f;                                     // the macro's LINGER on top at full
    };

    void haunt (Builder& b, const juce::String& oldJob, Ghost g)
    {
        b.set (ParamIDs::spectreOn, 1.0f).set (ParamIDs::spectreMix, 0.0f).set (ParamIDs::spectreTrail, g.trail)
         .set (ParamIDs::spectreLinger, g.linger).set (ParamIDs::spectreAir, g.air).set (ParamIDs::spectreDrift, g.drift)
         .set (ParamIDs::spectreShift, g.shift);
        route (b, "macro3", ParamIDs::spectreMix, g.mixTop);
        route (b, "macro3", ParamIDs::spectreLinger, g.lingerRise);
        b.rig.modulation.macroNames[2] = "SPECTRE";
        // "ORNAMENT (a wider flick)" reads "plus ORNAMENT: a wider flick" inside SPECTRE's own brackets.
        auto plus = oldJob;
        if (plus.endsWith (")") && plus.contains (" ("))
            plus = plus.upToFirstOccurrenceOf (" (", false, false) + ": " + plus.fromFirstOccurrenceOf (" (", false, false).dropLastCharacters (1);
        const auto oldText = "SPACE, " + oldJob;
        jassert (b.rig.notes.contains (oldText));
        b.rig.notes = b.rig.notes.replace (oldText, "SPACE, SPECTRE (a whispered ghost following it, plus " + plus + ")");
    }

    // CYBERSHRINE (the owner's name): inspired by the score of a 1995 animated cyberpunk film about a mind that may
    // be a ghost in a machine: an ancient chorus of women's voices in close, clashing harmony, ornamented like a
    // folk ritual, huge slow drums and deep temple bells, and a cold, rainy, reverberant machine world. No title,
    // characters, places, terms or music from the film: our own sounds.
    void cybershrine (Expansion x)
    {
        using SP11 = synth::mod::ShapePoint;
        const auto rain = [] (Builder& b, int o, float volume)   // light noise past the filter: rain or air
        {
            b.osc (o, "On", 1.0f).osc (o, "Gain", 0.0f).osc (o, "Noise", 0.4f).osc (o, "NoiseColour", 0.1f)
             .osc (o, "NoiseWidth", 2.0f).osc (o, "Volume", volume).osc (o, "Filter", 0.0f);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto folk = x.make ("Folk Voice", 1, { "Lead", "Vowel", "Pitch Envelope", "Mono" },
            "A bright, keening voice singing like a folk ritual: the Vowels table's \"ah\" with FORMANT up for a "
            "narrower, more nasal throat, every note flicking up into its pitch from just below (the PITCH envelope, "
            "80 ms), a quick narrow vibrato, a short glide and a big hall. MACROS: BRIGHT, SPACE, ORNAMENT (a wider "
            "flick). MOD WHEEL: vibrato.");
        folk.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.05f).osc (0, "Volume", 1.4f)
            .osc (0, "Formant", 0.2f).osc (0, "FormantMode", 2.0f)
            .osc (0, "VibAmount", 14.0f).osc (0, "VibRate", 6.0f).osc (0, "VibFade", 300.0f)
            .filter (FilterType::lp12, 4500.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.4f)
            .ampEnvelope (25.0f, 500.0f, 0.9f, 700.0f)
            .mono (50.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        folk.rig.modulation.pitchEnvelope = { { SP11 { 0.0f, 0.0f, -0.4f }, SP11 { 0.08f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (folk, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.25f);
        route (folk, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (folk, "macro3", ParamIDs::pitchEnvRange, 0.15f);
        macros (folk, "BRIGHT", "SPACE", "ORNAMENT");
        delay (folk, "1/4", 0.3f, 0.15f, true, 4.0f);
        reverb (folk, RT::hall, 0.8f, 0.3f, 40.0f, 4.5f);
        eqBand (folk, 0, eq::Shape::lowCut, 180.0f, 0.0f);
        space (folk);
        folk.set (ParamIDs::patchLevel, 4.0f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (folk, "ORNAMENT (a wider flick)", { 0.6f, 0.3f, 0.45f, 0.55f, 0.6f, 0.0f });
        x.add (folk);

        auto wire = x.make ("Wire Song", 2, { "Lead", "Sync", "Digital", "Mono" },
            "A glassy digital lead: a saw hard-synced high, its sync falling as each note starts, with a sine an "
            "octave down for body, a shimmering flanger, dotted-eighth ping-pong echoes and a plate. MACROS: BRIGHT, "
            "SPACE, SYNC (more of it). MOD WHEEL: vibrato.");
        wire.osc (0, "SyncAmount", 0.35f).osc (0, "SyncEnv", 0.25f).osc (0, "SyncEnvTime", 250.0f).osc (0, "Volume", 0.5f)
            .osc (0, "VibAmount", 0.0f).osc (0, "VibRate", 5.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.4f)
            .filter (FilterType::lp24, 5000.0f, 0.15f, 0.0f)
            .ampEnvelope (5.0f, 400.0f, 0.85f, 500.0f)
            .mono (35.0f);
        route (wire, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (wire, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (wire, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.3f);
        macros (wire, "BRIGHT", "SPACE", "SYNC");
        chorus (wire, ParamIDs::modIds, MT::flanger, 0.2f, 0.4f, 0.3f);
        delay (wire, "1/8 dotted", 0.4f, 0.2f, true, 5.0f);
        reverb (wire, RT::plate, 0.6f, 0.2f);
        space (wire);
        wire.set (ParamIDs::patchLevel, -1.9f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (wire, "SYNC (more of it)", { 0.55f, 0.25f, 0.4f, 0.4f, 0.7f, 0.0f });
        x.add (wire);

        auto half = x.make ("Half There", 3, { "Lead", "Shimmer", "Random", "Mono" },
            "A lead that seems to fade in and out of view: a triangle and a soft pulse an octave up, its level "
            "drifting at random (LFO 1, a smooth random) so the line comes and goes, a slow phaser, long echoes and "
            "a cathedral. MACROS: BRIGHT, SPACE, SHIMMER (more phaser). MOD WHEEL: steadier (less drift).");
        half.osc (0, "Wave", triangle).osc (0, "Volume", 0.7f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.12f)
            .filter (FilterType::lp12, 3000.0f, 0.1f, 0.0f)
            .ampEnvelope (80.0f, 600.0f, 0.9f, 1200.0f).set (ParamIDs::ampVolume, 0.8f)
            .mono (90.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always)
            .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 1.2f);
        route (half, "lfo1", ParamIDs::ampVolume, 0.2f);
        route (half, "modWheel", ParamIDs::ampVolume, 0.1f);
        route (half, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (half, "macro3", ParamIDs::modIds.mix, 0.4f);
        macros (half, "BRIGHT", "SPACE", "SHIMMER");
        chorus (half, ParamIDs::modIds, MT::phaser, 0.15f, 0.6f, 0.35f);
        delay (half, "1/2", 0.45f, 0.2f, true, 4.0f);
        half.set (ParamIDs::delayToReverb, 0.4f);
        reverb (half, RT::cathedral, 0.5f, 0.3f);
        space (half);
        half.set (ParamIDs::patchLevel, 1.4f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (half, "SHIMMER (more phaser)", { 0.6f, 0.4f, 0.55f, 0.7f, 0.8f, 0.0f });
        x.add (half);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto chorus1 = x.make ("Ancient Chorus", 4, { "Pad", "Choir", "Chorder", "Wavetable" },
            "The centrepiece: a chorus of women's voices in close, clashing harmony. The Vowels table singing \"ah\" "
            "in a gentle unison with a fifth above each note (the chorder), and a second choir a whole step above "
            "rubbing against it (oscillator 2), the way folk choirs sing seconds. Their vowels drift slowly (TRAVEL), "
            "a narrow vibrato, a cathedral. MACROS: BRIGHT, SPACE, CLASH (more of the second). MOD WHEEL: vibrato.");
        chorus1.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.0f).osc (0, "Volume", 1.0f)
               .osc (0, "Formant", 0.15f).osc (0, "FormantMode", 2.0f)
               .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.8f)
               .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 7.0f).osc (0, "Ch1Vol", 0.6f).osc (0, "Ch1Pan", -0.3f)
               .osc (0, "VibAmount", 10.0f).osc (0, "VibRate", 5.2f).osc (0, "VibFade", 900.0f)
               .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::pingPong).osc (0, "TrRate", 0.08f)
               .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrStrength", 0.25f).osc (0, "TrSmooth", 0.3f)
               .osc (1, "On", 1.0f).osc (1, "Wave", wavetable).osc (1, "Table", table ("Vowels")).osc (1, "WtIndex", 0.2f)
               .osc (1, "Transpose", 2.0f).osc (1, "Volume", 0.45f).osc (1, "Pan", 0.35f)
               .osc (1, "Formant", 0.15f).osc (1, "FormantMode", 2.0f)
               .osc (1, "VibAmount", 10.0f).osc (1, "VibRate", 4.7f).osc (1, "VibFade", 900.0f)
               .filter (FilterType::lp12, 4000.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.3f)
               .ampEnvelope (700.0f, 2000.0f, 0.9f, 2500.0f);
        chorus1.rig.modulation.travel[0].envelope = { { SP11 { 0.0f, 0.0f, 0.0f }, SP11 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (chorus1, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (chorus1, "modWheel", ParamIDs::osc (1, "VibAmount"), 0.2f);
        route (chorus1, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (chorus1, "macro3", ParamIDs::osc (1, "Volume"), 0.3f);
        macros (chorus1, "BRIGHT", "SPACE", "CLASH");
        reverb (chorus1, RT::cathedral, 0.55f, 0.35f, 40.0f, 4.5f);
        eqBand (chorus1, 0, eq::Shape::lowCut, 150.0f, 0.0f);
        space (chorus1);
        chorus1.set (ParamIDs::patchLevel, -1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (chorus1, "CLASH (more of the second)", { 0.55f, 0.5f, 0.6f, 0.65f, 0.8f, -0.25f });
        x.add (chorus1);

        auto city = x.make ("Rain City", 5, { "Pad", "Noise", "Dark", "Auto-Pan" },
            "A dark, wet city at night: saws a hair apart through a low, slowly breathing filter (LFO 1), a soft "
            "pulse an octave up, and rain falling through it all (light noise past the filter, wandering across the "
            "speakers with MOD 2's auto-pan), into a plate. MACROS: BRIGHT, SPACE, RAIN. MOD WHEEL: opens the filter.");
        city.osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.9f)
            .osc (0, "Volume", 0.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.25f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
            .filter (FilterType::lp24, 800.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.3f)
            .ampEnvelope (1200.0f, 2000.0f, 1.0f, 2500.0f)
            .set (ParamIDs::lfo (0, "Rate"), 0.08f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        rain (city, 2, 0.025f);
        route (city, "lfo1", ParamIDs::fltCutoff, 0.1f);
        route (city, "modWheel", ParamIDs::fltCutoff, 0.25f, 200.0f);
        route (city, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (city, "macro3", ParamIDs::osc (2, "Volume"), 0.15f);
        macros (city, "BRIGHT", "SPACE", "RAIN");
        chorus (city, ParamIDs::mod2Ids, MT::autoPan, 0.12f, 0.5f, 1.0f);
        reverb (city, RT::plate, 0.8f, 0.3f, 30.0f, 4.0f);
        eqBand (city, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (city);
        city.set (ParamIDs::patchLevel, -8.0f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (city, "RAIN", { 0.5f, 0.45f, 0.6f, 0.8f, 0.9f, 0.0f });
        x.add (city);

        auto dive = x.make ("Deep Dive", 6, { "Pad", "Wavetable", "Travel", "Phaser" },
            "Sinking under water: the Comb Sweep table travelling slowly from its bright end to its dark end over "
            "ten seconds of each chord (TRAVEL, one shot, free running), so the sound seems to go down and down, a sub "
            "under it, a slow phaser and a deep chorus swirling like water, into a hall. MACROS: BRIGHT, SPACE, DEPTH "
            "(a deeper swirl). MOD WHEEL: brings it back up (the table's bright end).");
        dive.osc (0, "Wave", wavetable).osc (0, "Table", table ("Comb Sweep")).osc (0, "WtIndex", 1.0f)
            .osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.9f)
            .osc (0, "Sub", 0.3f).osc (0, "Volume", 0.5f)
            .osc (0, "TrOn", 1.0f).osc (0, "TrMode", (float) synth::travel::Mode::oneShot).osc (0, "TrRate", 0.1f)
            .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrStrength", -0.9f).osc (0, "TrSmooth", 0.4f)
            .filter (FilterType::lp24, 2500.0f, 0.15f, 0.0f).set (ParamIDs::fltKey, 0.3f)
            .ampEnvelope (1500.0f, 2000.0f, 1.0f, 3000.0f);
        dive.rig.modulation.travel[0].envelope = { { SP11 { 0.0f, 0.0f, 0.2f }, SP11 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (dive, "modWheel", ParamIDs::osc (0, "TrStrength"), 0.4f);
        route (dive, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (dive, "macro3", ParamIDs::modIds.depth, 0.3f);
        macros (dive, "BRIGHT", "SPACE", "DEPTH");
        chorus (dive, ParamIDs::modIds, MT::phaser, 0.1f, 0.6f, 0.45f);
        chorus (dive, ParamIDs::mod2Ids, MT::chorus, 0.3f, 0.6f, 0.4f);
        reverb (dive, RT::hall, 0.9f, 0.3f, 30.0f, 3.5f);
        space (dive);
        dive.set (ParamIDs::patchLevel, -4.1f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (dive, "DEPTH (a deeper swirl)", { 0.55f, 0.55f, 0.65f, 0.5f, 0.8f, -0.25f });
        x.add (dive);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto drum = x.make ("Temple Drum", 7, { "Bass", "Drum", "Pitch Envelope", "Percussive" },
            "A huge drum you can play as a bass: a sine that drops an octave into its note as it's struck (the PITCH "
            "envelope), a thump of noise for the skin (a 60 ms modulation envelope), a gentle tube for weight, ringing "
            "out for a second and a half into a big hall. Velocity hits harder. MACROS: BRIGHT (more skin), SPACE, "
            "BOOM (a deeper drop).");
        drum.osc (0, "Wave", sine).osc (0, "Volume", 0.9f).osc (0, "PhaseRand", 0.0f)
            .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.6f).osc (1, "Volume", 0.0f)
            .shaper (0, synth::ShaperModel::tube, 0.2f)
            .filter (FilterType::lp12, 1500.0f, 0.1f, 0.0f).set (ParamIDs::fltVel, 0.4f)
            .ampEnvelope (1.0f, 1500.0f, 0.0f, 1200.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.6f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f)
            .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        drum.rig.modulation.pitchEnvelope = { { SP11 { 0.0f, 1.0f, -0.6f }, SP11 { 0.2f, 0.5f, 0.0f } }, -1, -1, -1 };
        drum.rig.modulation.envelopes[0] = { { SP11 { 0.0f, 1.0f, -0.5f }, SP11 { 0.06f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (drum, "env1", ParamIDs::osc (1, "Volume"), 0.25f);
        route (drum, "macro1", ParamIDs::osc (1, "Volume"), 0.1f);
        route (drum, "macro3", ParamIDs::pitchEnvRange, 0.25f);
        macros (drum, "BRIGHT", "SPACE", "BOOM");
        reverb (drum, RT::hall, 0.8f, 0.2f, 20.0f, 3.5f);
        space (drum);
        drum.set (ParamIDs::patchLevel, -1.3f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (drum, "BOOM (a deeper drop)", { 0.6f, 0.2f, 0.45f, 0.75f, 0.6f, 0.75f });
        x.add (drum);

        auto current = x.make ("Cold Current", 8, { "Bass", "Step Sequencer", "Resonant", "Mono" },
            "A dark bass that pulses with your song: a saw and a square a hair apart through a resonant low-pass, cut "
            "into a pattern of sixteenths by the STEP SEQUENCER with some steps tied into longer swells, the filter "
            "following it. Hold a note. MACROS: BRIGHT, SPACE, PULSE (deeper cuts). MOD WHEEL: resonance.");
        current.osc (0, "Volume", 0.6f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Fine", 8.0f).osc (1, "Volume", 0.35f)
               .filter (FilterType::lp24, 450.0f, 0.35f, 0.0f).set (ParamIDs::fltKey, 0.4f)
               .ampEnvelope (5.0f, 500.0f, 1.0f, 300.0f)
               .mono (0.0f)
               .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.8f).set (ParamIDs::seqGate, 0.7f)
               .set (ParamIDs::seqContour, 0.25f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        {
            const bool on[16] = { 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1 };
            for (int row = 0; row < 2; ++row)
                for (int k = 0; k < synth::seq::maxSteps; ++k)
                {
                    auto& step = current.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                    const auto at = k % 16;
                    step.on = on[at];
                    step.level = at % 4 == 0 ? 1.0f : 0.75f;
                    step.tied = at == 5 || at == 12;
                }
        }
        route (current, "stepSeq", ParamIDs::fltCutoff, 0.12f);
        route (current, "modWheel", ParamIDs::fltRes, 0.3f);
        route (current, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (current, "macro3", ParamIDs::seqVolMix, 0.2f);
        macros (current, "BRIGHT", "SPACE", "PULSE");
        delay (current, "1/8 dotted", 0.3f, 0.1f, true, 3.5f);
        reverb (current, RT::room, 0.5f, 0.12f);
        space (current);
        current.set (ParamIDs::patchLevel, -1.1f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (current, "PULSE (deeper cuts)", { 0.55f, 0.15f, 0.3f, 0.8f, 0.5f, 1.0f });
        x.add (current);

        auto heart = x.make ("Machine Heart", 9, { "Bass", "Sub", "Step Sequencer", "Shaper" },
            "A slow, low heartbeat: a sine sub and a soft triangle an octave up beating twice every half note in time "
            "with your song (the STEP SEQUENCER: a strong beat then a softer one), through SHAPER 1's tube for warmth. "
            "Hold a note. MACROS: BRIGHT, SPACE, GROWL (more drive).");
        heart.osc (0, "Wave", sine).osc (0, "Volume", 0.9f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f)
             .shaper (0, synth::ShaperModel::tube, 0.25f)
             .filter (FilterType::lp12, 900.0f, 0.1f, 0.0f)
             .ampEnvelope (5.0f, 500.0f, 1.0f, 400.0f)
             .mono (0.0f)
             .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 1.0f).set (ParamIDs::seqGate, 1.0f)
             .set (ParamIDs::seqContour, 0.3f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 8.0f)
             .set (ParamIDs::seqDecay, 350.0f);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = heart.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                const auto at = k % 8;
                step.on = at == 0 || at == 2;
                step.level = at == 0 ? 1.0f : 0.7f;
            }
        route (heart, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (heart, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (heart, "BRIGHT", "SPACE", "GROWL");
        reverb (heart, RT::room, 0.5f, 0.12f);
        space (heart);
        heart.set (ParamIDs::patchLevel, 10.0f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (heart, "GROWL (more drive)", { 0.55f, 0.2f, 0.35f, 0.75f, 0.5f, 1.0f });
        x.add (heart);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto bell = x.make ("Temple Bell", 10, { "Keys", "Bells", "FM", "Long" },
            "Deep temple bells: a sine struck with FM from a second sine at 2.76 times its pitch (a bell's "
            "off-harmonic ring), bright at the strike and mellowing over two seconds, and a twin a few cents apart so "
            "each bell beats and shimmers as it rings out for eight seconds, into a cathedral. MACROS: BRIGHT (a "
            "harder strike), SPACE, BEAT (faster beating).");
        bell.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
            .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 2)).osc (0, "FmAmount", 0.08f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 17.0f).osc (2, "Fine", 58.0f)   // 2.76 x: 17.58 semitones
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Fine", 3.0f).osc (1, "Volume", 0.4f).osc (1, "Pan", 0.25f)
            .osc (1, "PhaseRand", 0.0f)
            .ampEnvelope (1.0f, 8000.0f, 0.0f, 4000.0f).set (ParamIDs::ampDecayCurve, -0.6f).set (ParamIDs::ampVel, 0.6f)
            .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        bell.rig.modulation.envelopes[0] = { { SP11 { 0.0f, 1.0f, -0.5f }, SP11 { 2.0f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (bell, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (bell, "velocity", ParamIDs::osc (0, "FmAmount"), 0.08f);
        route (bell, "macro1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (bell, "macro3", ParamIDs::osc (1, "Fine"), 0.03f);
        macros (bell, "BRIGHT", "SPACE", "BEAT");
        reverb (bell, RT::cathedral, 0.6f, 0.3f, 30.0f, 4.5f);
        space (bell);
        bell.set (ParamIDs::patchLevel, -8.8f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (bell, "BEAT (faster beating)", { 0.5f, 0.35f, 0.7f, 0.3f, 0.7f, 0.0f });
        x.add (bell);

        auto silk = x.make ("Silk Strings", 11, { "Keys", "Pluck", "Pitch Envelope", "Zither" },
            "Plucked zither-like strings: a saw and a triangle plucked by the filter, each note pressed up a semitone "
            "a moment after it's plucked (the PITCH envelope, as a player presses the string behind the bridge), "
            "quarter-note echoes and a hall. MACROS: BRIGHT, SPACE, BEND (a wider press).");
        silk.osc (0, "Volume", 0.45f).osc (0, "PhaseRand", 0.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.4f).osc (1, "PhaseRand", 0.0f)
            .filter (FilterType::lp12, 900.0f, 0.15f, 0.5f).filterEnvelope (0.0f, 350.0f, 0.1f, 400.0f)
            .set (ParamIDs::fltKey, 0.6f).set (ParamIDs::fltEnvVel, 0.6f)
            .ampEnvelope (1.0f, 1800.0f, 0.0f, 900.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        silk.rig.modulation.pitchEnvelope = { { SP11 { 0.0f, 0.5f, 0.0f }, SP11 { 0.3f, 0.5f, 0.3f }, SP11 { 0.55f, 0.75f, 0.0f } }, -1, -1, -1 };
        route (silk, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (silk, "macro3", ParamIDs::pitchEnvRange, 0.05f);
        macros (silk, "BRIGHT", "SPACE", "BEND");
        delay (silk, "1/4", 0.3f, 0.15f, true, 4.0f);
        reverb (silk, RT::hall, 0.6f, 0.22f);
        space (silk);
        silk.set (ParamIDs::patchLevel, 0.0f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (silk, "BEND (a wider press)", { 0.5f, 0.3f, 0.6f, 0.45f, 0.7f, 0.25f });
        x.add (silk);

        auto net = x.make ("Glass Net", 12, { "Keys", "FM", "Digital", "Chorus" },
            "Cold digital keys: a sine struck with FM from another four times its pitch, bright and glassy at the "
            "strike and fading to a pure tone, a quiet sine an octave up, a slow chorus and a plate. MACROS: BRIGHT "
            "(a harder strike), SPACE, BITS (a touch of bit crush).");
        net.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
           .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 2)).osc (0, "FmAmount", 0.05f)
           .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 24.0f)
           .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
           .ampEnvelope (2.0f, 2500.0f, 0.2f, 800.0f).set (ParamIDs::ampVel, 0.6f)
           .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        net.rig.modulation.envelopes[0] = { { SP11 { 0.0f, 1.0f, -0.5f }, SP11 { 0.6f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (net, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (net, "velocity", ParamIDs::osc (0, "FmAmount"), 0.08f);
        route (net, "macro1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (net, "macro3", ParamIDs::osc (0, "Crush"), 0.3f);
        macros (net, "BRIGHT", "SPACE", "BITS");
        chorus (net, ParamIDs::modIds, MT::chorus, 0.3f, 0.4f, 0.4f);
        reverb (net, RT::plate, 0.6f, 0.22f);
        space (net);
        net.set (ParamIDs::patchLevel, -8.3f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (net, "BITS (a touch of bit crush)", { 0.5f, 0.35f, 0.6f, 0.3f, 0.8f, 0.0f });
        x.add (net);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto ritual = x.make ("Ritual Choir", 13, { "Arp", "Choir", "Ritual", "Cascade" },
            "A slow ritual: ARP 1 sings the chord you hold one voice at a time, a note a beat, up and down over two "
            "octaves, each sung note swelling in from \"oo\" to \"ah\" (an envelope through the Vowels table) and "
            "left to ring over the next, so the choir builds as it climbs. MACROS: BRIGHT, SPACE, VOWEL (the choir "
            "towards \"oo\").");
        ritual.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.0f).osc (0, "Volume", 1.3f)
              .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.8f)
              .osc (0, "Arp", 1.0f)
              .filter (FilterType::lp12, 3500.0f, 0.1f, 0.0f)
              .ampEnvelope (220.0f, 1500.0f, 0.8f, 1800.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
              .set (ParamIDs::arp (0, "Division"), division ("1/4")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
              .set (ParamIDs::arp (0, "Gate"), 1.0f)
              .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        ritual.rig.modulation.envelopes[0] = { { SP11 { 0.0f, 0.0f, 0.3f }, SP11 { 0.6f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (ritual, "env1", ParamIDs::osc (0, "WtIndex"), 0.35f);
        route (ritual, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (ritual, "macro3", ParamIDs::osc (0, "WtIndex"), 0.8f);
        macros (ritual, "BRIGHT", "SPACE", "VOWEL");
        reverb (ritual, RT::hall, 0.8f, 0.25f, 30.0f, 4.0f);
        space (ritual);
        ritual.set (ParamIDs::patchLevel, -1.1f);   // LEVEL: re-measured as an arp (0.55.0; measured, not by ear)
        haunt (ritual, "VOWEL (the choir towards \"oo\")", { 0.5f, 0.3f, 0.55f, 0.6f, 0.9f, -0.25f });
        x.add (ritual);

        auto ghost = x.make ("Signal Ghost", 14, { "Arp", "Randomizer", "FM", "Delay" },
            "Stray signals in the dark: ARP 1's randomizer scatters short glassy FM blips from the minor scale over "
            "three octaves, with plenty of rests, rolling a new pattern every pass, into long ping-pong echoes and a "
            "cathedral. Hold a key or a chord. MACROS: BRIGHT, SPACE, ECHO (more echoes).");
        ghost.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
             .osc (0, "FmAmount", 0.15f).osc (0, "ModRate", 0.75f).osc (0, "ModGrid", 1.0f)
             .ampEnvelope (1.0f, 300.0f, 0.0f, 200.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 3.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.4f);
        {
            auto& r = ghost.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Minor");
            r.restChance = 0.55f;
            r.lengthChances = { 1.0f, 0.2f, 0.0f, 0.0f };
            r.velocityLow = 0.5f;
            r.autoDice = true;
            r.seed = 21;
            auto& p = ghost.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        route (ghost, "macro1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (ghost, "macro3", ParamIDs::delayFeedback, 0.25f);
        macros (ghost, "BRIGHT", "SPACE", "ECHO");
        delay (ghost, "1/8 dotted", 0.5f, 0.3f, true, 4.5f);
        ghost.set (ParamIDs::delayToReverb, 0.4f);
        reverb (ghost, RT::cathedral, 0.5f, 0.28f);
        space (ghost);
        ghost.set (ParamIDs::patchLevel, 3.1f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (ghost, "ECHO (more echoes)", { 0.55f, 0.25f, 0.5f, 0.5f, 0.9f, 0.0f });
        x.add (ghost);

        auto sequence = x.make ("Dive Sequence", 15, { "Arp", "Rising", "Phaser", "Delay" },
            "A watery arpeggio that keeps rising: ARP 1 climbs the held chord over three octaves in eighths, soft "
            "triangles and a sine an octave up, the filter opening slowly over each bar (LFO 1, with your song), a "
            "deep phaser like light through water, dotted-quarter echoes and a hall. MACROS: BRIGHT, SPACE, DEPTH (a "
            "deeper phaser).");
        sequence.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f).osc (1, "Arp", 1.0f)
                .filter (FilterType::lp12, 1500.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 400.0f, 0.3f, 400.0f)
                .set (ParamIDs::fltKey, 0.5f)
                .ampEnvelope (5.0f, 700.0f, 0.3f, 500.0f)
                .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sawUp)
                .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight).set (ParamIDs::lfo (0, "Division"), division ("1/1"))
                .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
                .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
                .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 3.0f)
                .set (ParamIDs::arp (0, "Gate"), 0.8f);
        route (sequence, "lfo1", ParamIDs::fltCutoff, 0.12f);
        route (sequence, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (sequence, "macro3", ParamIDs::modIds.depth, 0.3f);
        macros (sequence, "BRIGHT", "SPACE", "DEPTH");
        chorus (sequence, ParamIDs::modIds, MT::phaser, 0.2f, 0.6f, 0.4f);
        delay (sequence, "1/4 dotted", 0.45f, 0.25f, true, 4.0f);
        reverb (sequence, RT::hall, 0.7f, 0.25f);
        space (sequence);
        sequence.set (ParamIDs::patchLevel, 1.7f);   // LEVEL: balanced against the others (measured, not by ear)
        haunt (sequence, "DEPTH (a deeper phaser)", { 0.5f, 0.25f, 0.45f, 0.6f, 0.9f, 0.0f });
        x.add (sequence);
    }
    // STOWAWAY (the owner's name): inspired by the score and sound of a 1979 science-fiction horror film about
    // something hiding aboard a ship in deep space: a sparse, slow score of echoing woodwinds, primitive horns,
    // trembling string harmonics and low brass clusters, the ship's own hum, hiss, chatter and alarms, and dread
    // that never resolves. At the owner's ask, the leads and pads here avoid the recipes the earlier banks lean on
    // (mono glide-and-vibrato leads, unison or vowel pads): each is built round a tool those banks never used (a
    // drawn wave, the FFT editor, drawn LFO shapes, band-pass, notch and high-pass filters, ring-style AM). No
    // title, characters, ship, creature, company or music from the film: our own sounds.
    void stowaway (Expansion x)
    {
        using SP12 = synth::mod::ShapePoint;
        const auto freeRate = [] (float ratio) { return 0.5f + std::log2 (ratio) / 8.0f; };   // ModRate for a FREE ratio
        const auto air = [] (Builder& b, int o, float noise, float volume)   // light noise: breath, vents, steam
        {
            b.osc (o, "On", 1.0f).osc (o, "Gain", 0.0f).osc (o, "Noise", noise).osc (o, "NoiseColour", 0.05f)
             .osc (o, "NoiseWidth", 2.0f).osc (o, "Volume", volume);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto flute = x.make ("Echo Flute", 1, { "Lead", "Freeform", "Tape Echo", "Breath" },
            "A breathy flute that echoes back at you: a hand-drawn wave (DRAW: a rounded cycle with a little of the "
            "third harmonic, so it's hollow, not a sine) with breath on every note, played straight, no vibrato, "
            "into long dark echoes that keep repeating and fading, as the score's woodwinds do. MACROS: BRIGHT, SPACE, "
            "TENSION (a ghost of the note a half step up). MOD WHEEL: more breath.");
        {
            synth::FreeformDrawing d;
            for (int i = 0; i < synth::FreeformDrawing::points; ++i)
            {
                const auto t = juce::MathConstants<float>::twoPi * (float) i / (float) synth::FreeformDrawing::points;
                d.values[(size_t) i] = 0.8f * std::sin (t) + 0.22f * std::sin (3.0f * t) + 0.06f * std::sin (5.0f * t);
            }
            d.smoothA = 0.08f;
            flute.rig.drawings[0] = d;
        }
        flute.osc (0, "Wave", (float) synth::OscWave::freeform).osc (0, "Volume", 0.8f).osc (0, "PhaseRand", 0.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 1.0f).osc (2, "Volume", 0.0f)
             .filter (FilterType::lp12, 3500.0f, 0.05f, 0.15f).filterEnvelope (40.0f, 300.0f, 0.7f, 400.0f)
             .set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (70.0f, 400.0f, 0.85f, 600.0f);
        air (flute, 1, 0.3f, 0.05f);
        route (flute, "modWheel", ParamIDs::osc (1, "Volume"), 0.08f);
        route (flute, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (flute, "macro3", ParamIDs::osc (2, "Volume"), 0.2f);
        macros (flute, "BRIGHT", "SPACE", "TENSION");
        delay (flute, "1/4 dotted", 0.5f, 0.32f, false, 2.5f);
        flute.set (ParamIDs::delayToReverb, 0.3f);
        reverb (flute, RT::hall, 0.7f, 0.2f, 30.0f, 3.5f);
        eqBand (flute, 0, eq::Shape::lowCut, 200.0f, 0.0f);
        space (flute);
        flute.set (ParamIDs::patchLevel, -5.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (flute);

        auto horn = x.make ("Bone Horn", 2, { "Lead", "FFT", "AM", "Horn" },
            "A primitive horn, part conch, part didgeridoo: a saw edited in the FFT editor down to its odd harmonics "
            "with a nasal bump around the 5th to 9th and nothing above the 20th, rough with ring-style AM at an "
            "off-harmonic ratio, each note pushed up into pitch from just under (the PITCH envelope). MACROS: BRIGHT, "
            "SPACE, TENSION (a rougher growl). MOD WHEEL: opens the filter.");
        for (int band = 1; band < synth::SpectrumEdit::bands; band += 2)
            horn.rig.spectra[0].gainsDb[(size_t) band] = synth::SpectrumEdit::minDb;   // harmonics 2, 4, 6...
        for (int band = 4; band <= 8; band += 2)
            horn.rig.spectra[0].gainsDb[(size_t) band] = 8.0f;                        // harmonics 5, 7, 9
        horn.osc (0, "FftLp", 20.0f).osc (0, "FftSlope", 0.5f).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
            .osc (0, "AmAmount", 0.25f).osc (0, "AmShape", 0.0f).osc (0, "ModRate", freeRate (1.41f))
            .filter (FilterType::lp12, 1600.0f, 0.15f, 0.0f).set (ParamIDs::fltKey, 0.5f)
            .ampEnvelope (90.0f, 600.0f, 0.85f, 700.0f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 1.5f);
        horn.rig.modulation.pitchEnvelope = { { SP12 { 0.0f, 0.0f, -0.3f }, SP12 { 0.25f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (horn, "modWheel", ParamIDs::fltCutoff, 0.2f, 150.0f);
        route (horn, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (horn, "macro3", ParamIDs::osc (0, "AmAmount"), 0.4f);
        macros (horn, "BRIGHT", "SPACE", "TENSION");
        delay (horn, "1/2", 0.4f, 0.15f, false, 3.0f);
        reverb (horn, RT::cathedral, 0.5f, 0.25f, 40.0f, 3.5f);
        space (horn);
        horn.set (ParamIDs::patchLevel, 1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (horn);

        auto whistle = x.make ("Cold Whistle", 3, { "Lead", "Band-Pass", "Pitch Envelope", "Thin" },
            "A thin, cold whistle: a triangle and a breath of noise through a narrow, ringing band-pass that follows "
            "the note, sagging a quarter-tone flat over three seconds as you hold it (the PITCH envelope), so nothing "
            "quite stays in tune. MACROS: BRIGHT, SPACE, TENSION (a deeper sag). MOD WHEEL: more air.");
        whistle.osc (0, "Wave", triangle).osc (0, "Volume", 2.0f)
               .filter (FilterType::bp12, 300.0f, 0.75f, 0.0f).set (ParamIDs::fltKey, 1.0f)
               .ampEnvelope (120.0f, 500.0f, 0.9f, 900.0f)
               .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 1.0f);
        air (whistle, 1, 0.5f, 0.15f);
        whistle.rig.modulation.pitchEnvelope = { { SP12 { 0.0f, 0.5f, 0.3f }, SP12 { 3.0f, 0.25f, 0.0f } }, -1, -1, -1 };
        route (whistle, "modWheel", ParamIDs::osc (1, "Volume"), 0.25f);
        route (whistle, "macro1", ParamIDs::fltRes, -0.3f);
        route (whistle, "macro3", ParamIDs::pitchEnvRange, 0.06f);
        macros (whistle, "BRIGHT", "SPACE", "TENSION");
        delay (whistle, "1/2", 0.5f, 0.2f, true, 3.0f);
        reverb (whistle, RT::hall, 0.8f, 0.25f, 40.0f, 3.5f);
        space (whistle);
        whistle.set (ParamIDs::patchLevel, 1.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (whistle);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto hum = x.make ("Hull Hum", 4, { "Pad", "Notch", "Drawn LFO", "Drone" },
            "The room tone of a ship drifting: a low saw and a buzzing narrow pulse a few cents apart, a notch filter "
            "wandering through them on an uneven drawn shape (LFO 1, CUSTOM 1, about fourteen seconds a turn) so the "
            "hum keeps changing colour, and the vents breathing above (light noise rising and falling at random). "
            "MACROS: BRIGHT, SPACE, TENSION (a low tritone growing under it). MOD WHEEL: a sharper notch.");
        hum.osc (0, "Transpose", -12.0f).osc (0, "Volume", 0.6f)
           .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.12f).osc (1, "Fine", 4.0f).osc (1, "Volume", 0.25f)
           .osc (3, "On", 1.0f).osc (3, "Wave", sine).osc (3, "Transpose", -6.0f).osc (3, "Volume", 0.0f)
           .filter (FilterType::bs24, 500.0f, 0.4f, 0.0f)
           .ampEnvelope (2000.0f, 2000.0f, 1.0f, 3000.0f)
           .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::custom1).set (ParamIDs::lfo (0, "Rate"), 0.07f)
           .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
           .set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (1, "Rate"), 0.3f)
           .set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        air (hum, 2, 0.4f, 0.008f);
        hum.osc (2, "Filter", 0.0f);
        hum.rig.modulation.lfoShapes[0] = { { SP12 { 0.0f, 0.2f, 0.3f }, SP12 { 0.3f, 0.9f, -0.3f }, SP12 { 0.42f, 0.45f, 0.2f },
                                              SP12 { 0.7f, 1.0f, -0.4f }, SP12 { 1.0f, 0.2f, 0.0f } }, -1, -1, -1 };
        route (hum, "lfo1", ParamIDs::fltCutoff, 0.25f);
        route (hum, "lfo2", ParamIDs::osc (2, "Volume"), 0.008f);
        route (hum, "modWheel", ParamIDs::fltRes, 0.35f);
        route (hum, "macro1", ParamIDs::osc (1, "Volume"), 0.25f);
        route (hum, "macro3", ParamIDs::osc (3, "Volume"), 0.35f);
        macros (hum, "BRIGHT", "SPACE", "TENSION");
        reverb (hum, RT::room, 0.7f, 0.25f, 20.0f, 3.0f);
        eqBand (hum, 0, eq::Shape::highShelf, 3000.0f, -6.0f);
        space (hum);
        hum.set (ParamIDs::patchLevel, -6.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hum);

        auto strings = x.make ("Cluster Strings", 5, { "Pad", "Strings", "Cluster", "Drawn Envelope" },
            "Strings bowing a chord with a half step inside it: two pinched, nasal saws a few cents apart and a quiet "
            "third a half step above, swelling unevenly as a nervous player would (a drawn modulation envelope: a "
            "push, a falter, then the full swell) with a slow, uneasy tremble. MACROS: BRIGHT, SPACE, TENSION (more "
            "of the clashing half step). MOD WHEEL: a tritone above it all.");
        strings.osc (0, "Pinch", 0.3f).osc (0, "Volume", 0.45f)
               .osc (1, "On", 1.0f).osc (1, "Pinch", 0.3f).osc (1, "Fine", 7.0f).osc (1, "Volume", 0.4f).osc (1, "Pan", 0.3f)
               .osc (2, "On", 1.0f).osc (2, "Pinch", 0.3f).osc (2, "Transpose", 6.0f).osc (2, "Volume", 0.0f).osc (2, "Pan", -0.3f)
               .osc (3, "On", 1.0f).osc (3, "Pinch", 0.3f).osc (3, "Transpose", 1.0f).osc (3, "Volume", 0.12f).osc (3, "Pan", -0.2f)
               .filter (FilterType::lp12, 1200.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.4f)
               .ampEnvelope (300.0f, 2000.0f, 1.0f, 2000.0f).set (ParamIDs::ampVolume, 0.7f)
               .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::sustain)
               .set (ParamIDs::lfo (0, "Rate"), 5.5f);
        for (int o = 0; o < 4; ++o)
            strings.osc (o, "VibAmount", 6.0f).osc (o, "VibRate", 5.5f).osc (o, "VibFade", 1500.0f);
        strings.rig.modulation.envelopes[0] = { { SP12 { 0.0f, 0.0f, 0.3f }, SP12 { 0.8f, 0.55f, -0.2f }, SP12 { 1.4f, 0.3f, 0.4f },
                                                  SP12 { 3.5f, 1.0f, 0.0f } }, 3, -1, -1 };
        route (strings, "env1", ParamIDs::fltCutoff, 0.25f);
        route (strings, "env1", ParamIDs::ampVolume, 0.3f);
        route (strings, "modWheel", ParamIDs::osc (2, "Volume"), 0.3f);
        route (strings, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (strings, "macro3", ParamIDs::osc (3, "Volume"), 0.3f);
        macros (strings, "BRIGHT", "SPACE", "TENSION");
        reverb (strings, RT::hall, 0.75f, 0.3f, 30.0f, 4.0f);
        space (strings);
        strings.set (ParamIDs::patchLevel, -9.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (strings);

        auto shiver = x.make ("Harmonic Shiver", 6, { "Pad", "FFT", "Drawn LFO", "Glassy" },
            "Glassy string harmonics trembling high up: a saw with its lowest seven harmonics taken out in the FFT "
            "editor, so only the thin, high partials are left, shivering on a drawn LFO shape (sharp catches, about "
            "eight a second) and drifting a few cents at random (LFO 2). MACROS: BRIGHT (more partials), SPACE, "
            "TENSION (a detuned twin: the shiver beats). MOD WHEEL: a slow, wide drift.");
        for (int o = 0; o < 2; ++o)
        {
            auto& s = shiver.rig.spectra[o];
            for (int band = 0; band < 7; ++band)
                s.gainsDb[(size_t) band] = synth::SpectrumEdit::minDb;   // harmonics 1 to 7 gone
        }
        shiver.osc (0, "FftLp", 28.0f).osc (0, "Volume", 1.2f).osc (0, "VibRate", 0.3f).osc (0, "VibAmount", 0.0f)
              .osc (1, "On", 1.0f).osc (1, "FftLp", 28.0f).osc (1, "Fine", 14.0f).osc (1, "Volume", 0.0f).osc (1, "Pan", 0.4f)
              .ampEnvelope (1500.0f, 2000.0f, 1.0f, 2500.0f).set (ParamIDs::ampVolume, 0.85f)
              .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::custom1).set (ParamIDs::lfo (0, "Rate"), 8.0f)
              .set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (1, "Rate"), 0.2f)
              .set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        shiver.rig.modulation.lfoShapes[0] = { { SP12 { 0.0f, 1.0f, -0.8f }, SP12 { 0.15f, 0.2f, 0.4f }, SP12 { 0.6f, 0.8f, -0.5f },
                                                 SP12 { 0.7f, 0.4f, 0.3f }, SP12 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (shiver, "lfo1", ParamIDs::ampVolume, 0.15f, 6.0f);   // a short lag: sharp catches without clicks
        route (shiver, "lfo2", ParamIDs::osc (0, "Fine"), 0.03f);
        route (shiver, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.3f);
        route (shiver, "macro1", ParamIDs::osc (0, "FftLp"), 0.15f);
        route (shiver, "macro3", ParamIDs::osc (1, "Volume"), 0.5f);
        macros (shiver, "BRIGHT", "SPACE", "TENSION");
        reverb (shiver, RT::cathedral, 0.6f, 0.35f, 40.0f, 4.0f);
        space (shiver);
        shiver.set (ParamIDs::patchLevel, -0.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (shiver);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto brass = x.make ("Dread Brass", 7, { "Bass", "Brass", "Cluster", "Drawn Envelope" },
            "Low brass notes a half step apart blasting, falling back, then swelling into a second, bigger blast that "
            "dies away (a drawn modulation envelope on the filter and the level), a sub under them and a tube for "
            "rasp. MACROS: BRIGHT, SPACE, TENSION (louder clash). MOD WHEEL: more rasp.");
        brass.osc (0, "Volume", 0.5f)
             .osc (1, "On", 1.0f).osc (1, "Transpose", 1.0f).osc (1, "Volume", 0.3f).osc (1, "Pan", 0.2f)
             .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.4f).osc (2, "Filter", 0.0f)
             .shaper (0, synth::ShaperModel::tube, 0.25f)
             .filter (FilterType::lp24, 250.0f, 0.15f, 0.0f).set (ParamIDs::fltKey, 0.3f)
             .ampEnvelope (10.0f, 3000.0f, 1.0f, 1500.0f).set (ParamIDs::ampVolume, 0.6f)
             .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        brass.rig.modulation.envelopes[0] = { { SP12 { 0.0f, 0.0f, -0.5f }, SP12 { 0.05f, 0.8f, 0.3f }, SP12 { 0.5f, 0.2f, 0.4f },
                                                SP12 { 2.5f, 1.0f, -0.3f }, SP12 { 6.0f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (brass, "env1", ParamIDs::fltCutoff, 0.35f);
        route (brass, "env1", ParamIDs::ampVolume, 0.4f);
        route (brass, "modWheel", ParamIDs::shp (0, "Drive"), 0.4f);
        route (brass, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (brass, "macro3", ParamIDs::osc (1, "Volume"), 0.3f);
        macros (brass, "BRIGHT", "SPACE", "TENSION");
        reverb (brass, RT::hall, 0.8f, 0.2f, 30.0f, 3.0f);
        space (brass);
        brass.set (ParamIDs::patchLevel, -6.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (brass);

        auto pressure = x.make ("Pressure Drone", 8, { "Bass", "Drone", "Pitch Envelope", "Creeping" },
            "A sub drone that won't stay put: a sine and a dark resonant saw that creep up a semitone over twenty "
            "seconds as you hold them (the PITCH envelope), never arriving anywhere, with a faint off-harmonic ring "
            "(AM) like metal under strain. MACROS: BRIGHT, SPACE, TENSION (a higher creep). MOD WHEEL: opens the "
            "filter.");
        pressure.osc (0, "Wave", sine).osc (0, "Volume", 0.8f).osc (0, "Filter", 0.0f)
                .osc (1, "On", 1.0f).osc (1, "Volume", 0.4f).osc (1, "AmAmount", 0.2f).osc (1, "ModRate", freeRate (2.83f))
                .filter (FilterType::lp24, 220.0f, 0.45f, 0.0f).set (ParamIDs::fltKey, 0.3f)
                .ampEnvelope (1500.0f, 2000.0f, 1.0f, 2500.0f)
                .mono (0.0f)
                .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 1.0f);
        pressure.rig.modulation.pitchEnvelope = { { SP12 { 0.0f, 0.5f, 0.2f }, SP12 { 20.0f, 1.0f, 0.0f } }, -1, -1, -1 };
        route (pressure, "modWheel", ParamIDs::fltCutoff, 0.25f, 200.0f);
        route (pressure, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (pressure, "macro3", ParamIDs::pitchEnvRange, 0.08f);
        macros (pressure, "BRIGHT", "SPACE", "TENSION");
        reverb (pressure, RT::hall, 0.7f, 0.15f, 20.0f, 3.0f);
        space (pressure);
        pressure.set (ParamIDs::patchLevel, -6.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pressure);

        auto steam = x.make ("Steam Bass", 9, { "Bass", "Skew", "Noise", "Mono" },
            "A gritty, bent bass with a hiss of steam on every note: a square bent lopsided (SKEW and PINCH) that "
            "drops into pitch from two semitones up, a resonant low-pass plucked by its envelope, and a burst of "
            "light noise past the filter as the note starts. MACROS: BRIGHT, SPACE, TENSION (more bend). MOD WHEEL: "
            "more steam.");
        steam.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Skew", 0.4f).osc (0, "Pinch", 0.2f).osc (0, "Volume", 0.7f)
             .osc (0, "Sub", 0.4f).osc (0, "PhaseRand", 0.0f)
             .filter (FilterType::lp24, 500.0f, 0.4f, 0.4f).filterEnvelope (0.0f, 300.0f, 0.2f, 200.0f)
             .ampEnvelope (2.0f, 600.0f, 0.7f, 250.0f)
             .mono (30.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f)
             .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        air (steam, 1, 0.5f, 0.0f);
        steam.osc (1, "Filter", 0.0f);
        steam.rig.modulation.pitchEnvelope = { { SP12 { 0.0f, 1.0f, -0.5f }, SP12 { 0.12f, 0.5f, 0.0f } }, -1, -1, -1 };
        steam.rig.modulation.envelopes[0] = { { SP12 { 0.0f, 1.0f, -0.4f }, SP12 { 0.3f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (steam, "env1", ParamIDs::osc (1, "Volume"), 0.08f);
        route (steam, "modWheel", ParamIDs::osc (1, "Volume"), 0.08f);
        route (steam, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (steam, "macro3", ParamIDs::osc (0, "Skew"), 0.4f);
        macros (steam, "BRIGHT", "SPACE", "TENSION");
        reverb (steam, RT::room, 0.5f, 0.12f);
        eqBand (steam, 1, eq::Shape::highShelf, 6000.0f, -3.0f);
        space (steam);
        steam.set (ParamIDs::patchLevel, -5.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (steam);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto metal = x.make ("Bowed Metal", 10, { "Keys", "FM", "Swell", "Inharmonic" },
            "A metal plate struck and then bowed: a sine FM'd by a second at 2.41 times its pitch (an off-harmonic, "
            "metallic ratio), its brightness flaring at the strike, falling back and then swelling again as if a bow "
            "had caught it (a drawn modulation envelope), ringing into a cathedral. MACROS: BRIGHT, SPACE, TENSION "
            "(a harsher ring). MOD WHEEL: a slow, wide waver.");
        metal.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.06f)
             .osc (0, "VibRate", 0.7f).osc (0, "VibAmount", 0.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 15.0f).osc (1, "Fine", 23.0f)   // 2.41 x: 15.23 semitones
             .ampEnvelope (1.0f, 500.0f, 0.6f, 3000.0f).set (ParamIDs::ampVel, 0.5f)
             .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::sustain);
        metal.rig.modulation.envelopes[0] = { { SP12 { 0.0f, 1.0f, -0.5f }, SP12 { 0.4f, 0.15f, 0.3f }, SP12 { 2.5f, 0.8f, 0.0f } }, 2, -1, -1 };
        route (metal, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (metal, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (metal, "macro1", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (metal, "macro3", ParamIDs::osc (1, "Fine"), 0.1f);
        macros (metal, "BRIGHT", "SPACE", "TENSION");
        reverb (metal, RT::cathedral, 0.55f, 0.3f, 30.0f, 4.0f);
        space (metal);
        metal.set (ParamIDs::patchLevel, -4.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (metal);

        auto chatter = x.make ("Data Chatter", 11, { "Keys", "Crush", "Random", "Computer" },
            "An old computer reading out: short, bit-crushed square blips, each jumping somewhere up to an octave "
            "above the key you play (the RANDOM source on TRANSPOSE), so every press chatters differently, with "
            "quick sixteenth-note echoes. MACROS: BRIGHT, SPACE, TENSION (coarser bits). MOD WHEEL: more echoes.");
        chatter.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 1.0f).osc (0, "PhaseRand", 0.0f)
               .osc (0, "Crush", 0.35f).osc (0, "CrushMode", 2.0f)
               .ampEnvelope (1.0f, 90.0f, 0.0f, 60.0f);
        route (chatter, "random", ParamIDs::osc (0, "Transpose"), 0.125f);
        route (chatter, "modWheel", ParamIDs::delayFeedback, 0.3f);
        route (chatter, "macro1", ParamIDs::osc (0, "Width"), -0.3f);
        route (chatter, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        macros (chatter, "BRIGHT", "SPACE", "TENSION");
        delay (chatter, "1/16", 0.3f, 0.2f, true, 6.0f);
        reverb (chatter, RT::room, 0.4f, 0.12f);
        space (chatter);
        chatter.set (ParamIDs::patchLevel, -1.1f);   // LEVEL: set by its peaks when blips pile up (measured, not by ear)
        x.add (chatter);

        auto piano = x.make ("Dread Piano", 12, { "Keys", "Piano", "Prepared", "AM" },
            "Low, muffled piano with something wrong inside it: two detuned string tones (a saw and a triangle nine "
            "cents apart) struck by the filter, and a buzzing rattle on each note as if screws were laid on the "
            "strings (ring-style AM at an off-harmonic ratio, dying away quickly). MACROS: BRIGHT, SPACE, TENSION "
            "(more rattle).");
        piano.osc (0, "Volume", 0.35f).osc (0, "PhaseRand", 0.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Fine", 9.0f).osc (1, "Volume", 0.6f).osc (1, "PhaseRand", 0.0f)
             .osc (1, "AmAmount", 0.0f).osc (1, "AmShape", 3.0f).osc (1, "ModRate", freeRate (7.13f))
             .filter (FilterType::lp12, 450.0f, 0.1f, 0.45f).filterEnvelope (0.0f, 500.0f, 0.1f, 400.0f)
             .set (ParamIDs::fltKey, 0.7f).set (ParamIDs::fltEnvVel, 0.7f)
             .ampEnvelope (2.0f, 3500.0f, 0.0f, 600.0f).set (ParamIDs::ampDecayCurve, -0.6f).set (ParamIDs::ampVel, 0.6f)
             .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
        piano.rig.modulation.envelopes[0] = { { SP12 { 0.0f, 1.0f, -0.6f }, SP12 { 0.5f, 0.0f, 0.0f } }, -1, -1, -1 };
        route (piano, "env1", ParamIDs::osc (1, "AmAmount"), 0.35f);
        route (piano, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (piano, "macro3", ParamIDs::osc (1, "AmAmount"), 0.3f);
        macros (piano, "BRIGHT", "SPACE", "TENSION");
        reverb (piano, RT::room, 0.6f, 0.2f, 15.0f, 3.5f);
        space (piano);
        piano.set (ParamIDs::patchLevel, -0.4f);   // LEVEL: set by its strike peaks (measured, not by ear)
        x.add (piano);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto ping = x.make ("Proximity Ping", 13, { "Arp", "Ratchet", "Ping", "Delay" },
            "A ping that keeps coming closer: hold one note and ARP 1 pings it slowly, then more and more often, the "
            "last pings breaking into quick bursts (ratchets), over and over every two bars. A pure sine with a "
            "bright FM tick at the start, into echoes. MACROS: BRIGHT, SPACE, TENSION (longer echoes).");
        ping.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
            .osc (0, "FmAmount", 0.0f).osc (0, "ModRate", freeRate (4.0f))
            .ampEnvelope (1.0f, 400.0f, 0.0f, 300.0f)
            .set (ParamIDs::env (0, "Mode"), (float) synth::mod::EnvelopeMode::oneShot)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
            .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Gate"), 0.9f);
        ping.rig.modulation.envelopes[0] = { { SP12 { 0.0f, 1.0f, -0.6f }, SP12 { 0.04f, 0.0f, 0.0f } }, -1, -1, -1 };
        {
            auto& p = ping.rig.modulation.arps[0].patterns[0];
            p = {};
            const int ratchets[16] = { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 2, 3 };
            for (int k = 0; k < 16; ++k)
            {
                auto& b = p.blocks[(size_t) k];
                b.on = ratchets[k] > 0;
                b.ratchet = std::max (1, ratchets[k]);
                b.velocity = 0.6f + 0.4f * (float) k / 15.0f;
            }
        }
        route (ping, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (ping, "macro1", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (ping, "macro3", ParamIDs::delayFeedback, 0.25f);
        macros (ping, "BRIGHT", "SPACE", "TENSION");
        delay (ping, "1/4", 0.45f, 0.25f, true, 4.0f);
        reverb (ping, RT::hall, 0.7f, 0.2f);
        space (ping);
        ping.set (ParamIDs::patchLevel, 1.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ping);

        auto crawl = x.make ("Vent Crawl", 14, { "Arp", "Randomizer", "Band-Pass", "Scratchy" },
            "Something moving in the walls: ARP 1's randomizer picks chromatic notes in sixteenths with many rests "
            "and sudden bursts, a new pattern every pass, each one a short scratch of noise and a thin pulse through a "
            "ringing band-pass that follows the note. Hold a key or a chord. MACROS: BRIGHT, SPACE, TENSION (a "
            "sharper ring).");
        crawl.osc (0, "Wave", pulse).osc (0, "Width", 0.15f).osc (0, "Volume", 1.8f).osc (0, "Arp", 1.0f)
             .filter (FilterType::bp12, 1200.0f, 0.35f, 0.3f).filterEnvelope (0.0f, 80.0f, 0.0f, 60.0f)
             .set (ParamIDs::fltKey, 0.8f)
             .ampEnvelope (0.5f, 70.0f, 0.0f, 50.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.4f);
        air (crawl, 1, 0.6f, 1.0f);
        crawl.osc (1, "NoiseColour", 0.3f);
        crawl.osc (1, "Arp", 1.0f);
        {
            auto& r = crawl.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Chromatic");
            r.restChance = 0.55f;
            r.ratchetChance = 0.3f;
            r.lengthChances = { 1.0f, 0.0f, 0.0f, 0.0f };
            r.velocityLow = 0.4f;
            r.autoDice = true;
            r.seed = 31;
            auto& p = crawl.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        route (crawl, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (crawl, "macro3", ParamIDs::fltRes, 0.3f);
        macros (crawl, "BRIGHT", "SPACE", "TENSION");
        delay (crawl, "1/8 dotted", 0.3f, 0.15f, true, 5.0f);
        reverb (crawl, RT::room, 0.5f, 0.18f);
        space (crawl);
        crawl.set (ParamIDs::patchLevel, 6.3f);   // LEVEL: set by its peaks, sparse scratches (measured, not by ear)
        x.add (crawl);

        auto alarm = x.make ("Alarm Pulse", 15, { "Arp", "Step Sequencer", "Tritone", "Alarm" },
            "An alarm going off somewhere in the ship: two narrow pulses a tritone apart, whooping up two semitones "
            "every half note (LFO 1) and pulsing on and off in eighths (the STEP SEQUENCER) with your song. Hold a note "
            "or a chord. MACROS: BRIGHT, SPACE, TENSION (a harsher alarm).");
        alarm.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.35f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Transpose", 6.0f).osc (1, "Volume", 0.25f)
             .filter (FilterType::lp12, 2500.0f, 0.2f, 0.0f)
             .ampEnvelope (5.0f, 500.0f, 1.0f, 200.0f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sawUp)
             .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight).set (ParamIDs::lfo (0, "Division"), division ("1/2"))
             .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
             .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.9f).set (ParamIDs::seqGate, 0.6f)
             .set (ParamIDs::seqContour, 0.15f).set (ParamIDs::seqDivision, division ("1/8")).set (ParamIDs::seqLength, 8.0f);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = alarm.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = k % 2 == 0;
                step.level = 1.0f;
            }
        route (alarm, "lfo1", ParamIDs::osc (0, "Transpose"), 0.02f);
        route (alarm, "lfo1", ParamIDs::osc (1, "Transpose"), 0.02f);
        route (alarm, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (alarm, "macro3", ParamIDs::osc (0, "Width"), -0.2f);
        route (alarm, "macro3", ParamIDs::osc (1, "Width"), -0.2f);
        macros (alarm, "BRIGHT", "SPACE", "TENSION");
        reverb (alarm, RT::hall, 0.6f, 0.2f);
        space (alarm);
        alarm.set (ParamIDs::patchLevel, -4.7f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        x.add (alarm);
    }
    // CHROMEFURY (the owner's name): inspired by the score of a 2015 post-apocalyptic chase film: relentless war
    // drums, sawing string ostinatos, brass blasts, guitar-like screaming leads over growling low synths, engines,
    // speed, dust and fire. At the owner's ask the bank is lead-heavy (6 leads, 3 pads, 3 basses, 1 keys, 2 arps)
    // and its leads hit harder than HELLFORGE's: distortion stacked in series (a gritty wave, a shaper before the
    // filter and one after it, an insert drive) and in parallel (two layers through two drive models on two buses,
    // blended like two amps), a power chord inside each note, and a clean sub kept off the drive so the low end holds.
    // Each lead pairs one heavy drive model with a light one (BANSHEE or SKYLINE) to keep the CPU sane. No title,
    // characters, vehicles, places, phrases or music from the film: our own sounds.
    void chromefury (Expansion x)
    {
        using SP13 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;
        using GM = effects::Glitch::Mode;
        const auto cleanSub = [] (Builder& b, int o, float volume)   // a sine an octave down, off the filter, shapers and drives
        {
            b.osc (o, "On", 1.0f).osc (o, "Wave", sine).osc (o, "Transpose", -12.0f).osc (o, "Volume", volume)
             .osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "PhaseRand", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };

        // ---- LEADS (6) ---------------------------------------------------------------------------------------
        auto scream = x.make ("Highway Scream", 1, { "Lead", "Drive", "Parallel", "Power Chord" },
            "A screaming, guitar-like solo lead: three saws with a fifth and an octave inside every note (the "
            "chorder) through RABID on insert bus A, blended with a pulse a few cents off through BANSHEE on bus B "
            "(two amps on one guitar), a tube shaper in front, the mids pushed so it cuts, a clean sub under it all. "
            "Bend down for a whammy dive (an octave). MACROS: BRIGHT, SPACE, FURY (more gain into the shaper). MOD "
            "WHEEL: vibrato.");
        scream.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.5f)
              .osc (0, "Volume", 0.7f).osc (0, "Insert", 1.0f)
              .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 7.0f).osc (0, "Ch1Vol", 0.6f).osc (0, "Ch2On", 1.0f).osc (0, "Ch2Coarse", 12.0f).osc (0, "Ch2Vol", 0.4f)
              .osc (0, "VibAmount", 0.0f).osc (0, "VibRate", 5.8f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Fine", 8.0f).osc (1, "Volume", 0.5f).osc (1, "Insert", 2.0f)
              .osc (1, "VibAmount", 0.0f).osc (1, "VibRate", 5.8f)
              .shaper (0, synth::ShaperModel::tube, 0.35f)
              .filter (FilterType::lp24, 5000.0f, 0.15f, 0.0f)
              .ampEnvelope (3.0f, 500.0f, 1.0f, 300.0f)
              .mono (35.0f).set (ParamIDs::bendDown, 12.0f).set (ParamIDs::bendUp, 2.0f);
        cleanSub (scream, 2, 0.35f);
        drive (scream, 0, DM::rabid, 7.5f, 6.0f, -2.0f, 0.4f, 0.3f);
        drive (scream, 1, DM::screamer, 8.0f, 6.0f, -2.0f, 0.4f, 0.3f);
        eqBand (scream, 0, eq::Shape::bell, 1500.0f, 3.0f, 0.8f);
        eqBand (scream, 1, eq::Shape::lowCut, 60.0f, 0.0f);
        route (scream, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (scream, "modWheel", ParamIDs::osc (1, "VibAmount"), 0.2f);
        route (scream, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (scream, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (scream, "BRIGHT", "SPACE", "FURY");
        delay (scream, "1/8 dotted", 0.3f, 0.15f, true, 4.5f);
        reverb (scream, RT::hall, 0.6f, 0.15f);
        space (scream);
        scream.set (ParamIDs::patchLevel, -3.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (scream);

        auto howl = x.make ("Chrome Howl", 2, { "Lead", "Sync", "Shaper", "Fuzz" },
            "A hard-synced lead that howls as its sync sweeps down on every note, through two shapers in series (a "
            "HARD clip before the filter, a TUBE after it) into COSMONAUT's fuzz on insert bus A, with a square an "
            "octave down for weight and a clean sub. MACROS: BRIGHT, SPACE, FURY (the after-filter tube harder). MOD "
            "WHEEL: more sync (a higher howl).");
        howl.osc (0, "SyncAmount", 0.35f).osc (0, "SyncEnv", 0.45f).osc (0, "SyncEnvTime", 450.0f).osc (0, "Volume", 0.65f)
            .osc (0, "Insert", 1.0f).osc (0, "VibAmount", 10.0f).osc (0, "VibRate", 5.5f).osc (0, "VibFade", 400.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.4f)
            .osc (1, "Insert", 1.0f)
            .shaper (0, synth::ShaperModel::hard, 0.4f)
            .shaper (1, synth::ShaperModel::tube, 0.35f, true)
            .filter (FilterType::lp24, 4000.0f, 0.2f, 0.0f)
            .ampEnvelope (3.0f, 500.0f, 1.0f, 300.0f)
            .mono (45.0f).set (ParamIDs::bendDown, 12.0f);
        cleanSub (howl, 2, 0.3f);
        drive (howl, 0, DM::muff, 6.5f, 5.5f, -2.0f, 0.4f, 0.3f);
        eqBand (howl, 0, eq::Shape::bell, 1200.0f, 2.5f, 0.8f);
        route (howl, "modWheel", ParamIDs::osc (0, "SyncAmount"), 0.3f);
        route (howl, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (howl, "macro3", ParamIDs::shp (1, "Drive"), 0.5f);
        macros (howl, "BRIGHT", "SPACE", "FURY");
        delay (howl, "1/4", 0.3f, 0.15f, true, 4.5f);
        reverb (howl, RT::plate, 0.5f, 0.15f);
        space (howl);
        howl.set (ParamIDs::patchLevel, -0.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (howl);

        auto wall = x.make ("Fury Saw", 3, { "Lead", "Unison", "Drive", "Wall" },
            "The thickest lead here, a wall: seven wide saws and five more an octave down, through a SOFT shaper and "
            "CHIMERA on insert bus A, the lower layer also through SKYLINE on bus B, a clean sub under it. Plays "
            "chords too. MACROS: BRIGHT, SPACE, FURY (more shaper). MOD WHEEL: wider detune.");
        wall.osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.75f).osc (0, "UniDetune", 0.28f).osc (0, "UniSpread", 1.0f)
            .osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Transpose", -12.0f).osc (1, "UniVoices", 5.0f).osc (1, "UniMix", 0.7f)
            .osc (1, "UniDetune", 0.22f).osc (1, "UniSpread", 0.8f).osc (1, "Volume", 0.4f).osc (1, "Insert", 2.0f)
            .shaper (0, synth::ShaperModel::soft, 0.45f)
            .filter (FilterType::lp24, 6000.0f, 0.1f, 0.0f)
            .ampEnvelope (5.0f, 500.0f, 1.0f, 350.0f)
            .mono (25.0f);
        cleanSub (wall, 2, 0.3f);
        drive (wall, 0, DM::chimera, 6.0f, 5.5f, -3.0f, 0.3f, 0.3f);
        drive (wall, 1, DM::skyline, 7.0f, 5.0f, -3.0f, 0.3f, 0.3f);
        eqBand (wall, 0, eq::Shape::bell, 900.0f, 2.0f, 0.7f);
        route (wall, "modWheel", ParamIDs::osc (0, "UniDetune"), 0.3f);
        route (wall, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (wall, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (wall, "BRIGHT", "SPACE", "FURY");
        delay (wall, "1/8", 0.25f, 0.12f, true, 4.5f);
        reverb (wall, RT::hall, 0.6f, 0.15f);
        space (wall);
        wall.set (ParamIDs::patchLevel, -6.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (wall);

        auto wail = x.make ("Nitro Wail", 4, { "Lead", "FM", "Fold", "Drive" },
            "A wailing FM lead: a sine driven hard by another at twice its pitch, folded (SINE FOLD) and pushed "
            "through RABID on insert bus A, each note scooping up into pitch, a wide vibrato coming in as you hold. "
            "Bend down a whole octave. MACROS: BRIGHT, SPACE, FURY (more fold). MOD WHEEL: more FM (a wilder wail).");
        wail.osc (0, "Wave", sine).osc (0, "Volume", 0.8f).osc (0, "Insert", 1.0f).osc (0, "PhaseRand", 0.0f)
            .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.35f)
            .osc (0, "VibAmount", 30.0f).osc (0, "VibRate", 6.0f).osc (0, "VibFade", 600.0f).osc (0, "VibCurve", 0.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f)
            .shaper (0, synth::ShaperModel::sineFold, 0.35f)
            .filter (FilterType::lp12, 5000.0f, 0.1f, 0.0f)
            .ampEnvelope (5.0f, 500.0f, 1.0f, 350.0f)
            .mono (60.0f).set (ParamIDs::bendDown, 12.0f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        wail.rig.modulation.pitchEnvelope = { { SP13 { 0.0f, 0.0f, -0.4f }, SP13 { 0.12f, 0.5f, 0.0f } }, -1, -1, -1 };
        cleanSub (wail, 2, 0.3f);
        drive (wail, 0, DM::rabid, 8.0f, 6.5f, -3.0f, 0.4f, 0.3f);
        route (wail, "modWheel", ParamIDs::osc (0, "FmAmount"), 0.3f, 80.0f);
        route (wail, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (wail, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (wail, "BRIGHT", "SPACE", "FURY");
        delay (wail, "1/8 dotted", 0.35f, 0.15f, true, 4.5f);
        reverb (wail, RT::hall, 0.6f, 0.15f);
        space (wail);
        wail.set (ParamIDs::patchLevel, 0.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (wail);

        auto throat = x.make ("Iron Throat", 5, { "Lead", "Vowel", "Talk", "Drive" },
            "A distorted lead that talks: the Vowels table in a three-voice unison and a saw an octave down, through a "
            "DIODE shaper and BANSHEE on insert bus A, then a HARD clip after the filter. The MOD WHEEL moves the "
            "vowel from \"oo\" to \"ah\", like a talk box. MACROS: BRIGHT, SPACE, FURY (harder clip).");
        throat.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 1.0f).osc (0, "Volume", 1.4f)
              .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.5f)
              .osc (0, "Insert", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f).osc (1, "Insert", 1.0f)
              .shaper (0, synth::ShaperModel::diode, 0.4f)
              .shaper (1, synth::ShaperModel::hard, 0.25f, true)
              .filter (FilterType::lp24, 4500.0f, 0.15f, 0.0f)
              .ampEnvelope (5.0f, 500.0f, 1.0f, 300.0f)
              .mono (40.0f).set (ParamIDs::bendDown, 12.0f);
        cleanSub (throat, 2, 0.3f);
        drive (throat, 0, DM::screamer, 8.5f, 6.0f, -2.0f, 0.3f, 0.3f);
        route (throat, "modWheel", ParamIDs::osc (0, "WtIndex"), -1.0f, 60.0f);
        route (throat, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (throat, "macro3", ParamIDs::shp (1, "Drive"), 0.5f);
        macros (throat, "BRIGHT", "SPACE", "FURY");
        delay (throat, "1/8 dotted", 0.3f, 0.12f, true, 4.5f);
        reverb (throat, RT::plate, 0.5f, 0.15f);
        space (throat);
        throat.set (ParamIDs::patchLevel, -1.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (throat);

        auto octave = x.make ("Thunder Octave", 6, { "Lead", "Octave Fuzz", "Rectify", "Fuzz" },
            "An octave fuzz lead: a pulse with copies an octave up and an octave down inside every note, rectified "
            "(RECTIFY: the octave-up snarl of a fuzz pedal) and slammed into COSMONAUT on insert bus A, a clean sub "
            "underneath, a slow glide. MACROS: BRIGHT, SPACE, FURY (more rectify). MOD WHEEL: vibrato.");
        octave.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Volume", 0.6f).osc (0, "Insert", 1.0f)
              .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 12.0f).osc (0, "Ch1Vol", 0.5f).osc (0, "Ch2On", 1.0f).osc (0, "Ch2Coarse", -12.0f).osc (0, "Ch2Vol", 0.6f)
              .osc (0, "VibAmount", 0.0f).osc (0, "VibRate", 5.5f)
              .shaper (0, synth::ShaperModel::rectify, 0.3f)
              .filter (FilterType::lp24, 3500.0f, 0.2f, 0.0f)
              .ampEnvelope (3.0f, 500.0f, 1.0f, 300.0f)
              .mono (80.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::bendDown, 12.0f);
        cleanSub (octave, 2, 0.3f);
        drive (octave, 0, DM::muff, 7.5f, 5.0f, -3.0f, 0.3f, 0.3f);
        eqBand (octave, 0, eq::Shape::bell, 1000.0f, 2.5f, 0.8f);
        route (octave, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (octave, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (octave, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (octave, "BRIGHT", "SPACE", "FURY");
        delay (octave, "1/4", 0.3f, 0.12f, true, 4.5f);
        reverb (octave, RT::hall, 0.5f, 0.15f);
        space (octave);
        octave.set (ParamIDs::patchLevel, -0.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (octave);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto storm = x.make ("Sand Storm", 7, { "Pad", "Noise", "Drive", "Auto-Pan" },
            "A distorted, wind-blown pad: detuned saws and a band of noise through SKYLINE on insert bus A, a "
            "resonant filter blown back and forth by a slow random LFO, the whole storm swirling round the room "
            "(MOD 2's auto-pan). MACROS: BRIGHT, SPACE, FURY (more wind). MOD WHEEL: opens the filter.");
        storm.osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.18f).osc (0, "UniSpread", 0.9f)
             .osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.3f).osc (1, "NoiseWidth", 2.0f)
             .osc (1, "Volume", 0.3f).osc (1, "Insert", 1.0f)
             .filter (FilterType::lp24, 1200.0f, 0.4f, 0.0f).set (ParamIDs::fltKey, 0.3f)
             .ampEnvelope (900.0f, 1500.0f, 1.0f, 1500.0f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.4f)
             .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        drive (storm, 0, DM::skyline, 6.0f, 5.0f, -4.0f, 0.2f, 0.4f);
        chorus (storm, ParamIDs::mod2Ids, MT::autoPan, 0.2f, 0.6f, 1.0f);
        route (storm, "lfo1", ParamIDs::fltCutoff, 0.15f);
        route (storm, "modWheel", ParamIDs::fltCutoff, 0.25f, 200.0f);
        route (storm, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (storm, "macro3", ParamIDs::osc (1, "Volume"), 0.4f);
        macros (storm, "BRIGHT", "SPACE", "FURY");
        reverb (storm, RT::hall, 0.8f, 0.25f);
        eqBand (storm, 0, eq::Shape::lowCut, 80.0f, 0.0f);
        space (storm);
        storm.set (ParamIDs::patchLevel, 3.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (storm);

        auto horizon = x.make ("Burning Horizon", 8, { "Pad", "Rising", "Shaper", "Brass" },
            "A heavy pad that rises like heat off the road: saws and a pulse swelling in while the filter climbs for "
            "four seconds (a slow filter envelope), through a TUBE shaper for grit, a fifth inside every note. MACROS: "
            "BRIGHT, SPACE, FURY (more tube). MOD WHEEL: pushes it brighter still.");
        horizon.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.8f)
               .osc (0, "Volume", 0.5f).osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 7.0f).osc (0, "Ch1Vol", 0.4f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
               .shaper (0, synth::ShaperModel::tube, 0.4f)
               .filter (FilterType::lp24, 300.0f, 0.2f, 0.5f).filterEnvelope (4000.0f, 2000.0f, 0.8f, 2000.0f)
               .ampEnvelope (1500.0f, 2000.0f, 1.0f, 2000.0f);
        route (horizon, "modWheel", ParamIDs::fltCutoff, 0.2f, 200.0f);
        route (horizon, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (horizon, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (horizon, "BRIGHT", "SPACE", "FURY");
        reverb (horizon, RT::hall, 0.8f, 0.25f);
        space (horizon);
        horizon.set (ParamIDs::patchLevel, -12.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (horizon);

        auto choir = x.make ("Dust Choir", 9, { "Pad", "Choir", "Wavetable", "Drive" },
            "A gritty choir in a storm: the Vowels table's \"ah\" in a wide unison through BANSHEE on insert bus A, "
            "a saw an octave down for body. MACROS: BRIGHT, SPACE, FURY (more grit, through SHAPER 1). MOD WHEEL: "
            "the vowel towards \"oh\".");
        choir.osc (0, "Wave", wavetable).osc (0, "Table", table ("Vowels")).osc (0, "WtIndex", 0.0f).osc (0, "Volume", 1.2f)
             .osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 1.0f)
             .osc (0, "Insert", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.25f)
             .shaper (0, synth::ShaperModel::soft, 0.1f)
             .filter (FilterType::lp12, 3500.0f, 0.1f, 0.0f)
             .ampEnvelope (900.0f, 1500.0f, 1.0f, 2000.0f);
        drive (choir, 0, DM::screamer, 6.0f, 5.0f, -4.0f, 0.2f, 0.4f);
        route (choir, "modWheel", ParamIDs::osc (0, "WtIndex"), 0.75f, 100.0f);
        route (choir, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (choir, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (choir, "BRIGHT", "SPACE", "FURY");
        reverb (choir, RT::cathedral, 0.4f, 0.3f);
        eqBand (choir, 0, eq::Shape::lowCut, 120.0f, 0.0f);
        space (choir);
        choir.set (ParamIDs::patchLevel, -2.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (choir);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto engine = x.make ("Engine Growl", 10, { "Bass", "FM", "Drive", "Mono" },
            "A snarling engine of a bass: a saw FM'd by a square an octave down and clipped HARD, through RABID on "
            "insert bus A, a clean sub underneath. The MOD WHEEL revs it (more FM and a brighter filter). MACROS: "
            "BRIGHT, SPACE, FURY (harder clip).");
        engine.osc (0, "Volume", 0.6f).osc (0, "Insert", 1.0f)
              .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.12f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f)
              .shaper (0, synth::ShaperModel::hard, 0.35f)
              .filter (FilterType::lp24, 700.0f, 0.3f, 0.3f).filterEnvelope (0.0f, 250.0f, 0.4f, 150.0f)
              .ampEnvelope (2.0f, 400.0f, 1.0f, 150.0f)
              .mono (20.0f);
        cleanSub (engine, 2, 0.5f);
        drive (engine, 0, DM::rabid, 6.0f, 5.0f, -4.0f, 0.0f, 0.1f);
        route (engine, "modWheel", ParamIDs::osc (0, "FmAmount"), 0.3f, 100.0f);
        route (engine, "modWheel", ParamIDs::fltCutoff, 0.15f, 100.0f);
        route (engine, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (engine, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (engine, "BRIGHT", "SPACE", "FURY");
        reverb (engine, RT::room, 0.4f, 0.08f);
        space (engine);
        engine.set (ParamIDs::patchLevel, -1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (engine);

        auto diesel = x.make ("Diesel Sub", 11, { "Bass", "Sub", "Parallel", "Drive" },
            "A clean, deep sub with a distorted growl laid on top: the sine stays clean while a saw an octave up goes "
            "alone through CHIMERA on insert bus A, so the low end stays solid however hard the top snarls. MACROS: "
            "BRIGHT, SPACE, FURY (more of the growl). MOD WHEEL: opens the growl's filter.");
        diesel.osc (0, "Volume", 0.35f).osc (0, "Insert", 1.0f)
              .filter (FilterType::lp24, 900.0f, 0.25f, 0.2f).filterEnvelope (0.0f, 300.0f, 0.5f, 150.0f)
              .ampEnvelope (2.0f, 400.0f, 1.0f, 150.0f)
              .mono (20.0f);
        cleanSub (diesel, 1, 0.8f);
        diesel.osc (1, "Transpose", 0.0f);
        drive (diesel, 0, DM::chimera, 7.0f, 5.0f, -4.0f, 0.0f, 0.1f);
        route (diesel, "modWheel", ParamIDs::fltCutoff, 0.2f, 100.0f);
        route (diesel, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (diesel, "macro3", ParamIDs::osc (0, "Volume"), 0.3f);
        macros (diesel, "BRIGHT", "SPACE", "FURY");
        reverb (diesel, RT::room, 0.4f, 0.08f);
        space (diesel);
        diesel.set (ParamIDs::patchLevel, -5.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (diesel);

        auto piston = x.make ("Piston Drive", 12, { "Bass", "Step Sequencer", "Drive", "Pumping" },
            "A driving bass pumping in eighths with your song: a saw and a square cut by the STEP SEQUENCER (the "
            "offbeats ducked), through SKYLINE on insert bus A, a clean sub under it. Hold a note. MACROS: BRIGHT, "
            "SPACE, FURY (more drive, through SHAPER 1). MOD WHEEL: opens the filter.");
        piston.osc (0, "Volume", 0.55f).osc (0, "Insert", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.35f).osc (1, "Insert", 1.0f)
              .shaper (0, synth::ShaperModel::soft, 0.15f)
              .filter (FilterType::lp24, 600.0f, 0.3f, 0.0f)
              .ampEnvelope (2.0f, 400.0f, 1.0f, 150.0f)
              .mono (0.0f)
              .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.85f).set (ParamIDs::seqGate, 0.75f)
              .set (ParamIDs::seqContour, 0.2f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 8.0f);
        cleanSub (piston, 2, 0.45f);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = piston.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = k % 8 != 3 && k % 8 != 7;
                step.level = k % 2 == 0 ? 1.0f : 0.7f;
            }
        drive (piston, 0, DM::skyline, 7.0f, 5.0f, -4.0f, 0.0f, 0.1f);
        route (piston, "stepSeq", ParamIDs::fltCutoff, 0.1f);
        route (piston, "modWheel", ParamIDs::fltCutoff, 0.2f, 100.0f);
        route (piston, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (piston, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (piston, "BRIGHT", "SPACE", "FURY");
        reverb (piston, RT::room, 0.4f, 0.08f);
        space (piston);
        piston.set (ParamIDs::patchLevel, 2.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (piston);

        // ---- KEYS (1) ----------------------------------------------------------------------------------------
        auto stabs = x.make ("Rust Stabs", 13, { "Keys", "Stab", "Drive", "Unison" },
            "Big distorted chord stabs: saws in a wide unison, short and punchy, through RABID on insert bus A (each "
            "hit dies inside its own beat, so the drive stays punchy instead of a roar), a TUBE shaper in front. "
            "MACROS: BRIGHT, SPACE, FURY (more tube). MOD WHEEL: brighter stabs.");
        stabs.osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.2f).osc (0, "UniSpread", 1.0f)
             .osc (0, "Volume", 0.45f).osc (0, "Insert", 1.0f)
             .shaper (0, synth::ShaperModel::tube, 0.3f)
             .filter (FilterType::lp24, 1200.0f, 0.2f, 0.45f).filterEnvelope (0.0f, 180.0f, 0.1f, 120.0f)
             .ampEnvelope (6.0f, 220.0f, 0.0f, 120.0f);   // 6 ms: a 1 ms edge made RABID spike 5 dB over the hit
        drive (stabs, 0, DM::rabid, 6.5f, 5.5f, 4.0f, 0.3f, 0.4f);
        route (stabs, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (stabs, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (stabs, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (stabs, "BRIGHT", "SPACE", "FURY");
        delay (stabs, "1/8 dotted", 0.3f, 0.15f, true, 4.5f);
        reverb (stabs, RT::hall, 0.6f, 0.2f);
        space (stabs);
        stabs.set (ParamIDs::patchLevel, -1.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (stabs);

        // ---- ARPS (2) ----------------------------------------------------------------------------------------
        auto drums = x.make ("Battle Cry", 14, { "Arp", "Ratchet", "Drive", "Saw" },
            "A battle cry that never lets up: ARP 1 hammers the chord you hold in sixteenths over two octaves, up and "
            "down, on driven saws (BANSHEE on insert bus A), accents on the beat and bursts of ratchets (2 and 3 hits in "
            "a step) before each bar turns. MACROS: BRIGHT, SPACE, FURY (more grit, through SHAPER 1).");
        drums.osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.9f)
             .osc (0, "Volume", 0.4f).osc (0, "Insert", 1.0f).osc (0, "Arp", 1.0f)
             .shaper (0, synth::ShaperModel::soft, 0.1f)
             .filter (FilterType::lp24, 2500.0f, 0.15f, 0.35f).filterEnvelope (0.0f, 120.0f, 0.2f, 80.0f)
             .ampEnvelope (2.0f, 160.0f, 0.5f, 90.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.55f);
        {
            auto& p = drums.rig.modulation.arps[0].patterns[0];
            p = {};
            for (int k = 0; k < 16; ++k)
            {
                auto& block = p.blocks[(size_t) k];
                block.on = true;
                block.velocity = k % 4 == 0 ? 1.0f : 0.62f;
                block.ratchet = k == 7 ? 2 : k == 14 ? 3 : k == 15 ? 2 : 1;
            }
        }
        drive (drums, 0, DM::screamer, 6.0f, 5.0f, -4.0f, 0.2f, 0.3f);
        route (drums, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (drums, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (drums, "BRIGHT", "SPACE", "FURY");
        reverb (drums, RT::hall, 0.7f, 0.2f);
        space (drums);
        drums.set (ParamIDs::patchLevel, 6.9f);   // LEVEL: re-measured as an arp (0.55.0; measured, not by ear)
        x.add (drums);

        auto pursuit = x.make ("Pursuit", 15, { "Arp", "Ostinato", "Strings", "Shaper" },
            "A relentless string-like ostinato in sixteenths: ARP 1 saws away at the chord you hold with the pattern "
            "stabbing up to the flat second and back (a Phrygian bite), sharp bowed attacks, a TUBE shaper for "
            "rasp, with your song. MACROS: BRIGHT, SPACE, FURY (more rasp). MOD WHEEL: more accent on the bow.");
        pursuit.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.8f)
               .osc (0, "Pinch", 0.25f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
               .osc (1, "On", 1.0f).osc (1, "Pinch", 0.25f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
               .shaper (0, synth::ShaperModel::tube, 0.3f)
               .filter (FilterType::lp24, 1800.0f, 0.15f, 0.35f).filterEnvelope (0.0f, 120.0f, 0.3f, 100.0f)
               .ampEnvelope (2.0f, 200.0f, 0.5f, 90.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Gate"), 0.7f);
        {
            auto& p = pursuit.rig.modulation.arps[0].patterns[0];
            p = {};
            const int transpose[16] = { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 3, 1, 0, -2 };
            for (int k = 0; k < 16; ++k)
            {
                auto& b = p.blocks[(size_t) k];
                b.on = true;
                b.transpose = transpose[k];
                b.velocity = k % 4 == 0 ? 1.0f : 0.7f;
            }
        }
        route (pursuit, "modWheel", ParamIDs::fltEnv, 0.2f);
        route (pursuit, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (pursuit, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (pursuit, "BRIGHT", "SPACE", "FURY");
        delay (pursuit, "1/8 dotted", 0.25f, 0.1f, true, 4.5f);
        reverb (pursuit, RT::hall, 0.6f, 0.18f);
        space (pursuit);
        pursuit.set (ParamIDs::patchLevel, 0.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pursuit);
    }
    // ASCENSION (the owner's name): inspired by the score of a 2001 science-fiction game about an ancient ring-shaped
    // world: a monks' chant growing into a vast choir, strings and brass for heroic action, big drums, mystery and awe.
    // Built on the VOICE filter (0.39.0), made for it at the owner's ask for a choir that sounds human and epic: the
    // choirs are buzzy sources (saws, narrow pulses, a breath of noise) shaped into sung vowels whose formants stay
    // put whatever note is sung, each voice with its own vibrato, the vowel opening from a hummed "mm" as a singer's
    // does. The grand choir's SIZE follows the keys (basses low, sopranos high). No title, characters, factions,
    // places, terms or music from the game: our own sounds.
    void ascension (Expansion x)
    {
        using SP14 = synth::mod::ShapePoint;
        const auto voice = [] (Builder& b, float vowel, float size, float sharp)
        {
            b.set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceVowel, vowel).set (ParamIDs::voiceSize, size).set (ParamIDs::voiceSharp, sharp);
        };
        const auto breath = [] (Builder& b, int o, float volume)   // light noise, through the VOICE filter too: a breathy vowel
        {
            b.osc (o, "On", 1.0f).osc (o, "Gain", 0.0f).osc (o, "Noise", 0.5f).osc (o, "NoiseColour", 0.08f)
             .osc (o, "NoiseWidth", 2.0f).osc (o, "Volume", volume);
        };
        const auto ensemble = [] (Builder& b, int o, int voices, float detune, float cents, float rate)   // a section: each its own vibrato
        {
            b.osc (o, "UniVoices", (float) voices).osc (o, "UniMix", 0.85f).osc (o, "UniDetune", detune).osc (o, "UniSpread", 0.9f)
             .osc (o, "VibMode", (float) synth::VibratoMode::independent).osc (o, "VibAmount", cents).osc (o, "VibRate", rate)
             .osc (o, "VibFade", 700.0f).osc (o, "VibCurve", 0.4f);
        };
        // The hummed start: the vowel opens from OO to the one set over `ms` (a one-shot envelope on VOWEL).
        const auto hum = [] (Builder& b, int env, float openTo, float ms)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { { SP14 { 0.0f, 0.0f, 0.3f }, SP14 { ms / 1000.0f, 1.0f, 0.0f } }, -1, -1, -1 };
            b.set (ParamIDs::voiceVowel, 0.0f);
            route (b, ("env" + juce::String (env + 1)).toRawUTF8(), ParamIDs::voiceVowel, openTo);
        };
        // SIZE following the keys: about a bass at C2, an alto at middle C, a soprano at C6 (keytrack is note / 127,
        // so two routes bent to its square: 2 x (note / 127)^2).
        const auto sizeByKey = [] (Builder& b)
        {
            b.set (ParamIDs::voiceSize, 0.0f);
            for (int k = 0; k < 2; ++k)
                b.rig.modulation.routes.push_back ({ true, "keytrack", ParamIDs::voiceSize, 1.0f, false, 0.0f, 0.5f });
        };
        const auto offVoice = [] (Builder& b, int o) { b.osc (o, "Voice", 0.0f); };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto monk = x.make ("Monk Solo", 1, { "Lead", "Voice", "Chant", "Mono" },
            "A single low voice chanting: a saw shaped by the VOICE filter into a bass singer's \"oh\", opening from a hum "
            "as each phrase starts, a slow, steady vibrato, a breath, a long cathedral. MACROS: BRIGHT, SPACE, VOWEL "
            "(towards \"ah\"). MOD WHEEL: opens the vowel too.");
        monk.osc (0, "Volume", 0.8f).osc (0, "VibAmount", 9.0f).osc (0, "VibRate", 4.6f).osc (0, "VibFade", 900.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.2f).osc (1, "Fine", 4.0f).osc (1, "Volume", 0.3f)
            .ampEnvelope (180.0f, 800.0f, 0.95f, 1200.0f)
            .mono (90.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always);
        breath (monk, 2, 0.05f);
        voice (monk, 0.25f, 0.05f, 0.45f);
        hum (monk, 0, 0.25f, 260.0f);
        route (monk, "modWheel", ParamIDs::voiceVowel, 0.25f, 80.0f);
        route (monk, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (monk, "macro3", ParamIDs::voiceVowel, 0.25f);
        macros (monk, "BRIGHT", "SPACE", "VOWEL");
        reverb (monk, RT::cathedral, 0.75f, 0.32f, 40.0f, 4.0f);
        space (monk);
        monk.set (ParamIDs::patchLevel, -4.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (monk);

        auto soprano = x.make ("Soprano Call", 2, { "Lead", "Voice", "Soprano", "Vibrato" },
            "A solo soprano soaring over everything: a narrow pulse and a quiet saw sung as \"ah\" by the VOICE filter at a "
            "soprano's size, a wide vibrato blooming as she holds a note, a gentle glide, echoes and a cathedral. MACROS: "
            "BRIGHT, SPACE, VOWEL (towards \"ee\"). MOD WHEEL: a wider vibrato.");
        soprano.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.7f)
               .osc (0, "VibAmount", 22.0f).osc (0, "VibRate", 5.6f).osc (0, "VibFade", 450.0f).osc (0, "VibCurve", 0.5f)
               .osc (1, "On", 1.0f).osc (1, "Volume", 0.3f).osc (1, "Fine", -3.0f)
               .osc (1, "VibAmount", 22.0f).osc (1, "VibRate", 5.6f).osc (1, "VibFade", 450.0f).osc (1, "VibCurve", 0.5f)
               .ampEnvelope (120.0f, 800.0f, 0.95f, 900.0f)
               .mono (70.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::legato);
        breath (soprano, 2, 0.04f);
        voice (soprano, 0.5f, 0.95f, 0.5f);
        route (soprano, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (soprano, "modWheel", ParamIDs::osc (1, "VibAmount"), 0.2f);
        route (soprano, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (soprano, "macro3", ParamIDs::voiceVowel, 0.4f);
        macros (soprano, "BRIGHT", "SPACE", "VOWEL");
        delay (soprano, "1/4", 0.35f, 0.15f, true, 4.0f);
        reverb (soprano, RT::cathedral, 0.6f, 0.3f, 40.0f, 4.5f);
        space (soprano);
        soprano.set (ParamIDs::patchLevel, -6.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (soprano);

        auto hero = x.make ("Hero Theme", 3, { "Lead", "Brass", "Strings", "Heroic" },
            "A heroic lead of brass and strings together: saws in a small unison that swell brighter as each note "
            "speaks (a brassy filter envelope), with a horn's body from the VOICE filter (a broad \"oh\", half mixed in), "
            "a string-like vibrato, and a big hall. MACROS: BRIGHT, SPACE, POWER (more brass bite). MOD WHEEL: vibrato.");
        hero.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.5f)
            .osc (0, "Volume", 0.6f).osc (0, "VibAmount", 6.0f).osc (0, "VibRate", 5.2f).osc (0, "VibFade", 500.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
            .filter (FilterType::lp24, 900.0f, 0.15f, 0.45f).filterEnvelope (120.0f, 600.0f, 0.6f, 500.0f)
            .set (ParamIDs::fltKey, 0.5f).set (ParamIDs::fltEnvVel, 0.5f)
            .ampEnvelope (40.0f, 600.0f, 0.9f, 600.0f)
            .mono (50.0f);
        voice (hero, 0.3f, 0.3f, 0.2f);
        hero.set (ParamIDs::voiceMix, 0.5f);
        route (hero, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        route (hero, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (hero, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (hero, "BRIGHT", "SPACE", "POWER");
        delay (hero, "1/8 dotted", 0.3f, 0.12f, true, 4.5f);
        reverb (hero, RT::hall, 0.8f, 0.22f);
        space (hero);
        hero.set (ParamIDs::patchLevel, -0.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hero);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto chant = x.make ("Monk Chant", 4, { "Pad", "Choir", "Voice", "Chant" },
            "Low male voices in slow unison, the sound of the bank: a section of five saw voices, each with its own slow "
            "vibrato, and a section an octave down, sung by the VOICE filter as basses' \"oh\", opening from a hum at "
            "each chord, a breath through it, a vast cathedral. MACROS: BRIGHT, SPACE, VOWEL (towards \"ah\"). MOD WHEEL: "
            "more breath.");
        chant.osc (0, "Volume", 0.55f)
             .osc (1, "On", 1.0f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
             .ampEnvelope (500.0f, 1500.0f, 0.95f, 2200.0f);
        ensemble (chant, 0, 5, 0.08f, 8.0f, 4.4f);
        ensemble (chant, 1, 3, 0.06f, 7.0f, 4.0f);
        breath (chant, 2, 0.05f);
        voice (chant, 0.25f, 0.05f, 0.45f);
        hum (chant, 0, 0.25f, 500.0f);
        route (chant, "modWheel", ParamIDs::osc (2, "Volume"), 0.1f);
        route (chant, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (chant, "macro3", ParamIDs::voiceVowel, 0.25f);
        macros (chant, "BRIGHT", "SPACE", "VOWEL");
        reverb (chant, RT::cathedral, 0.9f, 0.35f, 40.0f, 4.0f);
        eqBand (chant, 0, eq::Shape::lowCut, 60.0f, 0.0f);
        space (chant);
        chant.set (ParamIDs::patchLevel, -11.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (chant);

        auto grand = x.make ("Grand Choir", 5, { "Pad", "Choir", "Voice", "Epic" },
            "The full, epic choir in all four sections: the low notes you play sung by basses, the middle by tenors and "
            "altos, the high by sopranos (the VOICE filter's SIZE follows the keys), a section of saws and a section of "
            "narrow pulses each with its own vibrato, every chord opening from a hum into \"ah\", a breath through it, a "
            "huge hall. MACROS: BRIGHT, SPACE, VOWEL (towards \"oh\"). MOD WHEEL: the vowel towards \"ee\".");
        grand.osc (0, "Volume", 0.5f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.25f).osc (1, "Volume", 0.35f)
             .ampEnvelope (450.0f, 1500.0f, 0.95f, 2000.0f);
        ensemble (grand, 0, 5, 0.1f, 10.0f, 5.0f);
        ensemble (grand, 1, 4, 0.08f, 12.0f, 5.4f);
        breath (grand, 2, 0.04f);
        voice (grand, 0.5f, 0.4f, 0.45f);
        sizeByKey (grand);
        hum (grand, 0, 0.5f, 380.0f);
        route (grand, "modWheel", ParamIDs::voiceVowel, 0.4f, 100.0f);
        route (grand, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (grand, "macro3", ParamIDs::voiceVowel, -0.25f);
        macros (grand, "BRIGHT", "SPACE", "VOWEL");
        reverb (grand, RT::hall, 0.95f, 0.32f, 40.0f, 4.5f);
        eqBand (grand, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (grand);
        grand.set (ParamIDs::patchLevel, -15.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (grand);

        auto angel = x.make ("Angel Choir", 6, { "Pad", "Choir", "Voice", "Airy" },
            "Airy high voices: sopranos singing a soft \"oh\" opening towards \"ah\" (the VOICE filter, SHARP low for a "
            "breathy, unfocused sound), a quiet copy an octave up floating above, more breath, slow echoes and a cathedral. "
            "MACROS: BRIGHT, SPACE, AIR (more breath). MOD WHEEL: the vowel towards \"ah\".");
        angel.osc (0, "Wave", triangle).osc (0, "Volume", 0.8f)
             .osc (1, "On", 1.0f).osc (1, "Volume", 0.35f)
             .osc (1, "Ch1On", 1.0f).osc (1, "Ch1Coarse", 12.0f).osc (1, "Ch1Vol", 0.35f)
             .ampEnvelope (700.0f, 1500.0f, 0.95f, 2500.0f);
        ensemble (angel, 0, 4, 0.08f, 12.0f, 5.5f);
        ensemble (angel, 1, 3, 0.1f, 12.0f, 5.8f);
        breath (angel, 2, 0.08f);
        voice (angel, 0.3f, 0.95f, 0.2f);
        route (angel, "modWheel", ParamIDs::voiceVowel, 0.25f, 100.0f);
        route (angel, "macro1", ParamIDs::voiceSharp, 0.4f);
        route (angel, "macro3", ParamIDs::osc (2, "Volume"), 0.12f);
        macros (angel, "BRIGHT", "SPACE", "AIR");
        delay (angel, "1/2", 0.4f, 0.15f, true, 4.0f);
        reverb (angel, RT::cathedral, 0.7f, 0.35f, 50.0f, 5.0f);
        eqBand (angel, 0, eq::Shape::lowCut, 150.0f, 0.0f);
        space (angel);
        angel.set (ParamIDs::patchLevel, -11.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (angel);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto low = x.make ("Low Chant", 7, { "Bass", "Voice", "Drone", "Sub" },
            "The deepest voices as a bass: a saw sung as a bass singer's dark \"oo\" by the VOICE filter, a clean sine "
            "under it kept off the filter, a slow vibrato. MACROS: BRIGHT, SPACE, VOWEL (towards \"oh\"). MOD WHEEL: opens "
            "the vowel.");
        low.osc (0, "Volume", 0.8f).osc (0, "VibAmount", 6.0f).osc (0, "VibRate", 4.0f).osc (0, "VibFade", 900.0f)
           .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.5f).osc (1, "PhaseRand", 0.0f)
           .ampEnvelope (250.0f, 1000.0f, 0.95f, 900.0f)
           .mono (40.0f);
        offVoice (low, 1);
        voice (low, 0.1f, 0.0f, 0.5f);
        route (low, "modWheel", ParamIDs::voiceVowel, 0.25f, 80.0f);
        route (low, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (low, "macro3", ParamIDs::voiceVowel, 0.2f);
        macros (low, "BRIGHT", "SPACE", "VOWEL");
        reverb (low, RT::hall, 0.6f, 0.15f);
        space (low);
        low.set (ParamIDs::patchLevel, -5.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (low);

        auto strings = x.make ("Battle Strings", 8, { "Bass", "Strings", "Ostinato", "Step Sequencer" },
            "Low strings driving an ostinato in sixteenths with your song: pinched, bowed saws a few cents apart, each "
            "step bitten by the filter, the STEP SEQUENCER accenting the pattern. Hold a note. MACROS: BRIGHT, SPACE, "
            "BITE (more filter snap). MOD WHEEL: opens the filter.");
        strings.osc (0, "Pinch", 0.3f).osc (0, "Volume", 0.55f)
               .osc (1, "On", 1.0f).osc (1, "Pinch", 0.3f).osc (1, "Fine", 7.0f).osc (1, "Volume", 0.45f)
               .filter (FilterType::lp24, 700.0f, 0.15f, 0.3f).filterEnvelope (0.0f, 150.0f, 0.3f, 120.0f)
               .ampEnvelope (3.0f, 500.0f, 1.0f, 200.0f)
               .mono (0.0f)
               .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.85f).set (ParamIDs::seqGate, 0.7f)
               .set (ParamIDs::seqContour, 0.15f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = strings.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                const auto at = k % 16;
                step.on = true;
                step.level = at % 4 == 0 ? 1.0f : at % 2 == 0 ? 0.7f : 0.5f;
            }
        route (strings, "stepSeq", ParamIDs::fltCutoff, 0.1f);
        route (strings, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (strings, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (strings, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (strings, "BRIGHT", "SPACE", "BITE");
        reverb (strings, RT::hall, 0.6f, 0.15f);
        space (strings);
        strings.set (ParamIDs::patchLevel, 1.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (strings);

        auto brass = x.make ("Deep Brass", 9, { "Bass", "Brass", "Swell", "Voice" },
            "Low brass swelling up: a saw and a pulse an octave down, the filter opening slowly as the note grows (a brass "
            "swell), a horn's throat from the VOICE filter (a bass's \"oh\", half mixed in), a hall. Press into the keys "
            "to push it. MACROS: BRIGHT, SPACE, BLARE (a harder swell).");
        brass.osc (0, "Volume", 0.6f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.4f)
             .filter (FilterType::lp24, 300.0f, 0.15f, 0.45f).filterEnvelope (700.0f, 1200.0f, 0.7f, 800.0f)
             .ampEnvelope (300.0f, 1000.0f, 0.95f, 900.0f)
             .mono (0.0f);
        voice (brass, 0.25f, 0.0f, 0.2f);
        brass.set (ParamIDs::voiceMix, 0.5f);
        route (brass, "aftertouch", ParamIDs::fltCutoff, 0.15f, 150.0f);
        route (brass, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (brass, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (brass, "BRIGHT", "SPACE", "BLARE");
        reverb (brass, RT::hall, 0.8f, 0.18f);
        space (brass);
        brass.set (ParamIDs::patchLevel, -1.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (brass);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto stabs = x.make ("Choir Stabs", 10, { "Keys", "Choir", "Voice", "Stab" },
            "Short shouts of \"ah!\" from the whole choir: a section of saws whose vowel snaps open from \"oo\" in a few "
            "milliseconds, sung by basses low and sopranos high (SIZE follows the keys), short, into a huge hall. MACROS: "
            "BRIGHT, SPACE, VOWEL (towards \"oh\"). MOD WHEEL: the vowel towards \"ee\".");
        stabs.osc (0, "Volume", 0.6f)
             .ampEnvelope (6.0f, 380.0f, 0.0f, 300.0f).set (ParamIDs::ampDecayCurve, -0.4f);
        ensemble (stabs, 0, 5, 0.1f, 6.0f, 5.0f);
        stabs.osc (0, "VibFade", 0.0f);
        breath (stabs, 1, 0.05f);
        voice (stabs, 0.5f, 0.4f, 0.45f);
        sizeByKey (stabs);
        hum (stabs, 0, 0.5f, 60.0f);
        route (stabs, "modWheel", ParamIDs::voiceVowel, 0.4f, 60.0f);
        route (stabs, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (stabs, "macro3", ParamIDs::voiceVowel, -0.25f);
        macros (stabs, "BRIGHT", "SPACE", "VOWEL");
        reverb (stabs, RT::hall, 0.9f, 0.3f, 30.0f, 4.5f);
        eqBand (stabs, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (stabs);
        stabs.set (ParamIDs::patchLevel, -7.9f);   // LEVEL: set by its peaks: big chords struck hard (measured, not by ear)
        x.add (stabs);

        auto bells = x.make ("Ancient Bells", 11, { "Keys", "Bells", "Additive", "Church" },
            "Deep, ringing bells built as a real bell's partials are: a hum an octave down, the strike note, a minor "
            "third, a fifth and an octave above (five sines), each fading at its own pace (the higher, the sooner), a "
            "cathedral. MACROS: BRIGHT (more of the upper partials), SPACE, HUM (more of the low hum).");
        constexpr std::array<float, 5> partials { -12.0f, 0.0f, 3.0f, 7.0f, 12.0f };
        constexpr std::array<float, 5> fines { 0.0f, 0.0f, 16.0f, 2.0f, 0.0f };     // the tierce a little sharp, as on a real bell
        constexpr std::array<float, 5> levels { 0.35f, 0.5f, 0.3f, 0.2f, 0.25f };
        constexpr std::array<float, 5> seconds { 7.0f, 5.0f, 3.5f, 2.5f, 2.0f };
        for (int k = 0; k < 5; ++k)
        {
            bells.osc (k, "On", 1.0f).osc (k, "Wave", sine).osc (k, "Transpose", partials[(size_t) k]).osc (k, "Fine", fines[(size_t) k])
                 .osc (k, "Volume", 0.0f).osc (k, "PhaseRand", 0.0f)
                 .set (ParamIDs::env (k, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            bells.rig.modulation.envelopes[(size_t) k] = { { SP14 { 0.0f, 1.0f, -0.6f }, SP14 { seconds[(size_t) k], 0.0f, 0.0f } }, -1, -1, -1 };
            route (bells, ("env" + juce::String (k + 1)).toRawUTF8(), ParamIDs::osc (k, "Volume"), levels[(size_t) k] * 0.5f);
        }
        bells.ampEnvelope (1.0f, 7000.0f, 0.0f, 3000.0f).set (ParamIDs::ampVel, 0.5f);
        route (bells, "macro1", ParamIDs::osc (3, "Volume"), 0.1f);
        route (bells, "macro1", ParamIDs::osc (4, "Volume"), 0.1f);
        route (bells, "macro3", ParamIDs::osc (0, "Volume"), 0.15f);
        macros (bells, "BRIGHT", "SPACE", "HUM");
        reverb (bells, RT::cathedral, 0.6f, 0.3f, 30.0f, 4.5f);
        space (bells);
        bells.set (ParamIDs::patchLevel, -4.3f);   // LEVEL: set by its peaks when a chord is struck (measured, not by ear)
        x.add (bells);

        auto lament = x.make ("Lament Piano", 12, { "Keys", "Piano", "Sad", "Spacious" },
            "A sad, spacious piano: NEGATIVE SPACE's soft felt piano, here far back in a hall, its echoes slow and dark. "
            "The MOD WHEEL adds the felt's noise. MACROS: BRIGHT, SPACE, ECHO (more of the echoes).");
        softPiano (lament);
        route (lament, "macro3", ParamIDs::delayMix, 0.25f);
        macros (lament, "BRIGHT", "SPACE", "ECHO");
        delay (lament, "1/2", 0.35f, 0.12f, true, 3.0f);
        reverb (lament, RT::hall, 0.85f, 0.32f, 30.0f, 4.0f);
        space (lament);
        lament.set (ParamIDs::patchLevel, -4.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (lament);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto charge = x.make ("String Charge", 13, { "Arp", "Strings", "Ostinato", "Heroic" },
            "A racing string ostinato: ARP 1 runs up and down the chord you hold over two octaves in sixteenths with "
            "your song, pinched bowed saws with a sharp attack, the first of every four accented, a hall. MACROS: BRIGHT, "
            "SPACE, BITE (more filter snap).");
        charge.osc (0, "Pinch", 0.3f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Pinch", 0.3f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.4f).osc (1, "Arp", 1.0f)
              .filter (FilterType::lp24, 1500.0f, 0.15f, 0.35f).filterEnvelope (0.0f, 110.0f, 0.25f, 90.0f)
              .ampEnvelope (2.0f, 220.0f, 0.5f, 100.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
              .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.7f);
        {
            auto& p = charge.rig.modulation.arps[0].patterns[0];
            p = {};
            for (int k = 0; k < 16; ++k)
            {
                p.blocks[(size_t) k].on = true;
                p.blocks[(size_t) k].velocity = k % 4 == 0 ? 1.0f : 0.7f;
            }
        }
        route (charge, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (charge, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (charge, "BRIGHT", "SPACE", "BITE");
        reverb (charge, RT::hall, 0.7f, 0.2f);
        space (charge);
        charge.set (ParamIDs::patchLevel, 3.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (charge);

        auto sacred = x.make ("Sacred Chant", 14, { "Arp", "Choir", "Voice", "Chant" },
            "A monks' chant: ARP 1 walks up the notes you hold in eighths, its pattern leaping a fifth and an octave "
            "in a repeating figure, every syllable sung through the VOICE filter, opening from a hum to \"ah\" (so each "
            "note is a new word), in a cathedral. Hold one note for a plain chant. MACROS: BRIGHT, SPACE, VOWEL.");
        sacred.osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f).osc (1, "Arp", 1.0f)
              .ampEnvelope (15.0f, 220.0f, 0.3f, 180.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
              .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.7f).set (ParamIDs::arp (0, "Length"), 8.0f);
        {
            auto& p = sacred.rig.modulation.arps[0].patterns[0];
            p = {};
            constexpr int figure[8] = { 0, 0, 7, 0, 12, 7, 0, 7 };
            for (int k = 0; k < 8; ++k)
            {
                auto& block = p.blocks[(size_t) k];
                block.on = true;
                block.transpose = figure[k];
                block.velocity = k % 4 == 0 ? 1.0f : 0.7f;
            }
        }
        ensemble (sacred, 0, 5, 0.08f, 9.0f, 4.6f);
        breath (sacred, 1, 0.05f);
        voice (sacred, 0.25f, 0.2f, 0.45f);
        hum (sacred, 0, 0.35f, 200.0f);   // each syllable open inside an eighth at 80 BPM
        route (sacred, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (sacred, "macro3", ParamIDs::voiceVowel, 0.25f);
        macros (sacred, "BRIGHT", "SPACE", "VOWEL");
        reverb (sacred, RT::cathedral, 0.8f, 0.3f);
        space (sacred);
        sacred.set (ParamIDs::patchLevel, 2.0f);   // LEVEL: re-measured as an arp (0.55.0; measured, not by ear)
        x.add (sacred);

        auto pulse8 = x.make ("Signal Pulse", 15, { "Arp", "Electronic", "Pulse", "Delay" },
            "A pulsing electronic arp for the action: ARP 1 plays the chord you hold in eighths, a narrow pulse plucked "
            "by a resonant filter, dotted-eighth echoes bouncing across, a hall. MACROS: BRIGHT, SPACE, GLOW (resonance).");
        pulse8.osc (0, "Wave", pulse).osc (0, "Width", 0.25f).osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
              .filter (FilterType::lp24, 1200.0f, 0.4f, 0.35f).filterEnvelope (0.0f, 200.0f, 0.15f, 150.0f)
              .ampEnvelope (1.0f, 300.0f, 0.2f, 200.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
              .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.6f);
        route (pulse8, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (pulse8, "macro3", ParamIDs::fltRes, 0.3f);
        macros (pulse8, "BRIGHT", "SPACE", "GLOW");
        delay (pulse8, "1/8 dotted", 0.45f, 0.25f, true, 4.5f);
        reverb (pulse8, RT::hall, 0.6f, 0.18f);
        space (pulse8);
        pulse8.set (ParamIDs::patchLevel, -1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pulse8);
    }

    // REFRACTION: inspired by the score of a 2018 science-fiction film about an expedition into a zone where
    // everything is refracted and copied (a quiet folk guitar, light bending into colours, plants and bodies
    // mutating, a vast wailing alien swell). No title, characters, places, terms or music from the film: our own
    // sounds. The owner's ask: otherworldly. MACRO 3 is MUTATE on most: it pushes each sound from lovely to wrong.
    void refraction (Expansion x)
    {
        using SP15 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;
        const auto freeRate = [] (float ratio) { return 0.5f + std::log2 (ratio) / 8.0f; };   // ModRate for a FREE ratio
        const auto oneShot = [] (Builder& b, int env, std::vector<SP15> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto voice = [] (Builder& b, float vowel, float size, float sharp)
        {
            b.set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceVowel, vowel).set (ParamIDs::voiceSize, size).set (ParamIDs::voiceSharp, sharp);
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers, buses and VOICE
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto prism = x.make ("Refraction", 1, { "Lead", "Split", "Stereo", "Drift" },
            "One note that splits like light through a prism as you hold it: a pulse in the middle, then a fifth "
            "fading in on the left and an octave on the right, each sliding into tune from a little off (the fifth "
            "from under, the octave from over), a faint twelfth above them, a shimmering chorus, echoes and a "
            "digital space. MACROS: BRIGHT, SPACE, MUTATE (the split copies pulled out of tune: microtones). "
            "MOD WHEEL: vibrato.");
        prism.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Volume", 0.55f)
             .osc (0, "VibAmount", 5.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 600.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 7.0f).osc (1, "Fine", -25.0f)
             .osc (1, "Pan", -0.6f).osc (1, "Volume", 0.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 12.0f).osc (2, "Fine", 20.0f)
             .osc (2, "Pan", 0.6f).osc (2, "Volume", 0.0f)
             .osc (3, "On", 1.0f).osc (3, "Wave", sine).osc (3, "Transpose", 19.0f).osc (3, "Volume", 0.0f)
             .filter (FilterType::lp12, 5000.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (20.0f, 600.0f, 0.85f, 900.0f);
        oneShot (prism, 0, { SP15 { 0.0f, 0.0f, 0.4f }, SP15 { 1.5f, 1.0f, 0.0f } });    // the split opening
        oneShot (prism, 1, { SP15 { 0.0f, 0.0f, 0.0f }, SP15 { 1.0f, 0.0f, 0.4f }, SP15 { 3.0f, 1.0f, 0.0f } });   // the twelfth, later
        route (prism, "env1", ParamIDs::osc (1, "Volume"), 0.2f);
        route (prism, "env1", ParamIDs::osc (2, "Volume"), 0.1f);
        route (prism, "env2", ParamIDs::osc (3, "Volume"), 0.03f);
        route (prism, "env1", ParamIDs::osc (1, "Fine"), 0.125f);    // -25 cents into tune
        route (prism, "env1", ParamIDs::osc (2, "Fine"), -0.1f);     // +20 cents into tune
        route (prism, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (prism, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (prism, "macro3", ParamIDs::osc (1, "Fine"), 0.2f);
        route (prism, "macro3", ParamIDs::osc (2, "Fine"), -0.2f);
        macros (prism, "BRIGHT", "SPACE", "MUTATE");
        chorus (prism, ParamIDs::modIds, MT::dimension, 0.4f, 0.5f, 0.4f);
        delay (prism, "1/4 dotted", 0.4f, 0.2f, true, 5.0f);
        reverb (prism, RT::digital, 0.7f, 0.25f, 30.0f, 5.0f);
        space (prism);
        prism.set (ParamIDs::patchLevel, -2.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (prism);

        auto mirror = x.make ("Mirror Voice", 2, { "Lead", "Voice", "Pitch", "Stereo" },
            "A lead with a double that copies you a moment late and slightly wrong: a saw on the left, and on the right a "
            "second saw that arrives after it, sliding up into the note from a half step under (the PITCH envelope, for "
            "the double only), sung as an inhuman \"ee\" by the VOICE filter, with an unsteady, random vibrato. "
            "MACROS: BRIGHT, SPACE, MUTATE (the double's formant pushed, its voice stranger). MOD WHEEL: more of the double.");
        mirror.osc (0, "Volume", 0.55f).osc (0, "Pan", -0.35f).osc (0, "PitchEnv", 0.0f).osc (0, "Voice", 0.0f)
              .osc (1, "On", 1.0f).osc (1, "Volume", 0.0f).osc (1, "Pan", 0.35f)
              .osc (1, "VibWave", (float) synth::VibratoWave::random).osc (1, "VibAmount", 15.0f).osc (1, "VibRate", 6.0f)
              .filter (FilterType::lp12, 3000.0f, 0.1f, 0.15f).filterEnvelope (20.0f, 400.0f, 0.7f, 500.0f)
              .set (ParamIDs::fltKey, 0.5f)
              .ampEnvelope (15.0f, 500.0f, 0.9f, 700.0f)
              .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 1.0f);
        mirror.rig.modulation.pitchEnvelope = { { SP15 { 0.0f, 0.0f, 0.0f }, SP15 { 0.15f, 0.0f, -0.4f }, SP15 { 0.5f, 0.5f, 0.0f } }, -1, -1, -1 };
        oneShot (mirror, 0, { SP15 { 0.0f, 0.0f, 0.0f }, SP15 { 0.15f, 0.0f, 0.3f }, SP15 { 0.5f, 1.0f, 0.0f } });   // the double arriving
        route (mirror, "env1", ParamIDs::osc (1, "Volume"), 0.3f);
        voice (mirror, 1.0f, 0.7f, 0.6f);
        route (mirror, "modWheel", ParamIDs::osc (1, "Volume"), 0.15f);
        route (mirror, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (mirror, "macro3", ParamIDs::osc (1, "Formant"), 0.2f);
        route (mirror, "macro3", ParamIDs::voiceSize, -0.4f);
        macros (mirror, "BRIGHT", "SPACE", "MUTATE");
        delay (mirror, "1/8", 0.25f, 0.18f, false, 4.0f);
        reverb (mirror, RT::hall, 0.7f, 0.25f, 30.0f, 4.5f);
        space (mirror);
        mirror.set (ParamIDs::patchLevel, -3.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (mirror);

        auto wail = x.make ("Hollow Wail", 3, { "Lead", "Sync", "Glide", "Drive" },
            "The vast alien swell: a saw whose SYNC opens as the note grows, giving it a voice that wanders (a slow LFO "
            "on the sync), a pulse an octave down, a tube shaper and insert bus A's SKYLINE, every note scooping up from "
            "under, sliding slowly between notes, a wide vibrato that blooms late, a cathedral. Play it slowly. MACROS: "
            "BRIGHT, SPACE, MUTATE (more sync: a harsher, stranger voice). MOD WHEEL: wider vibrato.");
        wail.osc (0, "SyncAmount", 0.2f).osc (0, "SyncEnv", 0.35f).osc (0, "SyncEnvTime", 900.0f).osc (0, "Volume", 0.55f)
            .osc (0, "VibAmount", 18.0f).osc (0, "VibRate", 4.5f).osc (0, "VibFade", 900.0f).osc (0, "VibCurve", 0.6f)
            .osc (0, "Insert", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.45f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
            .osc (1, "Insert", 1.0f)
            .shaper (0, synth::ShaperModel::tube, 0.25f)
            .filter (FilterType::lp24, 1000.0f, 0.35f, 0.3f).filterEnvelope (300.0f, 1500.0f, 0.6f, 800.0f)
            .set (ParamIDs::fltKey, 0.5f)
            .ampEnvelope (120.0f, 800.0f, 0.9f, 1200.0f)
            .mono (220.0f).set (ParamIDs::glideMode, (float) synth::GlideMode::always).set (ParamIDs::glideCurve, 0.4f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 5.0f)
            .set (ParamIDs::lfo (0, "Rate"), 0.35f);
        wail.rig.modulation.pitchEnvelope = { { SP15 { 0.0f, 0.25f, -0.4f }, SP15 { 0.45f, 0.5f, 0.0f } }, -1, -1, -1 };
        drive (wail, 0, DM::skyline, 2.5f, 3.0f, 4.0f, 0.3f, 0.4f);
        route (wail, "lfo1", ParamIDs::osc (0, "SyncAmount"), 0.08f);
        route (wail, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (wail, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (wail, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.35f);
        macros (wail, "BRIGHT", "SPACE", "MUTATE");
        delay (wail, "1/4", 0.35f, 0.15f, true, 4.0f);
        reverb (wail, RT::cathedral, 0.8f, 0.3f, 40.0f, 4.0f);
        space (wail);
        wail.set (ParamIDs::patchLevel, -1.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (wail);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto veil = x.make ("Prism Veil", 4, { "Pad", "FFT", "Unison", "Glass" },
            "A glassy veil: a saw edited in the FFT editor (its lowest overtones taken out, a bright band from the 4th to "
            "the 12th lifted), swinging slowly between two versions of itself so its harmonics rise and fall (VSPEED), "
            "in a wide unison, a sine an octave up, a shimmering chorus and a huge digital space. MACROS: BRIGHT, SPACE, "
            "MUTATE (SKEW: bends the wave, bringing a hollow, wrong low end back). MOD WHEEL: faster shimmer.");
        veil.rig.spectra[0].gainsDb[1] = synth::SpectrumEdit::minDb;   // harmonics 2 and 3 out
        veil.rig.spectra[0].gainsDb[2] = synth::SpectrumEdit::minDb;
        for (int band = 3; band <= 11; ++band)
            veil.rig.spectra[0].gainsDb[(size_t) band] = 6.0f;          // harmonics 4 to 12
        veil.osc (0, "Volume", 0.5f).osc (0, "FftVSpeed", 0.3f).osc (0, "FftVStrength", 0.6f).osc (0, "FftLp", 64.0f)
            .osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.75f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.9f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
            .ampEnvelope (900.0f, 1500.0f, 0.95f, 2500.0f)
            .set (ParamIDs::lfo (1, "Rate"), 0.07f).set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (veil, "lfo2", ParamIDs::osc (0, "FftLp"), 0.1f);
        route (veil, "modWheel", ParamIDs::osc (0, "FftVSpeed"), 0.2f);
        route (veil, "macro1", ParamIDs::osc (0, "FftLp"), 0.15f);
        route (veil, "macro3", ParamIDs::osc (0, "Skew"), 0.3f);
        macros (veil, "BRIGHT", "SPACE", "MUTATE");
        chorus (veil, ParamIDs::modIds, MT::dimension, 0.3f, 0.6f, 0.5f);
        reverb (veil, RT::digital, 0.9f, 0.3f, 40.0f, 5.0f);
        eqBand (veil, 0, eq::Shape::lowCut, 120.0f, 0.0f);
        space (veil);
        veil.set (ParamIDs::patchLevel, -8.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (veil);

        auto growth = x.make ("Overgrowth", 5, { "Pad", "Wavetable", "Travel", "Evolving" },
            "A pad that keeps growing into something else while you hold it: the Wavefold table walked by TRAVEL's "
            "envelope and an eight-step pattern (forwards, back, flipped, halfway and back) over about twelve seconds, "
            "each note starting somewhere different (a slow random LFO on INDEX, per note), so no two chords are quite "
            "the same, a warm triangle under it. MACROS: BRIGHT, SPACE, MUTATE (TRAVEL goes further). MOD WHEEL: FORMANT.");
        growth.osc (0, "Wave", wavetable).osc (0, "Table", table ("Wavefold")).osc (0, "WtIndex", 0.2f).osc (0, "Volume", 0.5f)
              .osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.85f)
              .osc (0, "TrOn", 1.0f).osc (0, "TrEditors", (float) synth::travel::Editors::envSeq).osc (0, "TrSteps", 8.0f)
              .osc (0, "TrPatternMode", (float) synth::travel::Mode::pingPong)
              .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrRate", 0.08f)
              .osc (0, "TrStrength", 0.5f).osc (0, "TrSmooth", 0.5f)
              .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.25f)
              .filter (FilterType::lp12, 2500.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.3f)
              .ampEnvelope (1500.0f, 2000.0f, 0.95f, 3000.0f)
              .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.15f);
        {
            auto& t = growth.rig.modulation.travel[0];
            t.envelope = { { SP15 { 0.0f, 0.0f, 0.3f }, SP15 { 0.6f, 1.0f, -0.3f }, SP15 { 1.0f, 0.4f, 0.0f } }, -1, -1, -1 };
            using M = synth::travel::StepMode;
            const synth::travel::Step steps[] = { { M::normal, 1.0f, 2 }, { M::reverse, 1.0f, 1 }, { M::pingPongHalf, 1.0f, 1 }, { M::flip, 1.0f, 1 },
                                                  { M::normal, 2.0f, 1 }, { M::mid, 1.0f, 1 }, { M::reverse, 1.0f, 2 }, { M::pingPongHalf, 2.0f, 1 } };
            for (int k = 0; k < 8; ++k)
                t.pattern.steps[(size_t) k] = steps[k];
        }
        route (growth, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.1f);
        route (growth, "modWheel", ParamIDs::osc (0, "Formant"), 0.15f);
        route (growth, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (growth, "macro3", ParamIDs::osc (0, "TrStrength"), 0.2f);
        macros (growth, "BRIGHT", "SPACE", "MUTATE");
        chorus (growth, ParamIDs::modIds, MT::uniVibe, 0.2f, 0.4f, 0.3f);
        reverb (growth, RT::hall, 0.9f, 0.3f, 40.0f, 4.5f);
        eqBand (growth, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (growth);
        growth.set (ParamIDs::patchLevel, -8.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (growth);

        auto spores = x.make ("Spore Drift", 6, { "Pad", "Noise", "Voice", "Sparkle" },
            "Air full of drifting spores: soft noise whispered through the VOICE filter, its vowel wandering by itself "
            "(a slow random LFO), a quiet triangle chord inside it, and two high sines (two octaves up, and two octaves "
            "and a fifth) that twinkle on and off at random, one left, one right, in a vast digital space. MACROS: BRIGHT, "
            "SPACE, MUTATE (the whisper narrows into a strange, ringing tone). MOD WHEEL: more twinkles.");
        spores.osc (0, "Gain", 0.0f).osc (0, "Noise", 0.6f).osc (0, "NoiseColour", 0.1f).osc (0, "NoiseWidth", 2.0f).osc (0, "Volume", 0.8f)
              .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.3f).osc (1, "Voice", 0.0f)
              .osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.6f).osc (1, "UniDetune", 0.1f).osc (1, "UniSpread", 0.8f)
              .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 24.0f).osc (2, "Volume", 0.0f).osc (2, "Pan", -0.6f).osc (2, "Voice", 0.0f)
              .osc (3, "On", 1.0f).osc (3, "Wave", sine).osc (3, "Transpose", 31.0f).osc (3, "Volume", 0.0f).osc (3, "Pan", 0.6f).osc (3, "Voice", 0.0f)
              .ampEnvelope (1500.0f, 2000.0f, 1.0f, 3000.0f)
              .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.2f)
              .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
              .set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::sampleHold).set (ParamIDs::lfo (1, "Rate"), 6.0f)
              .set (ParamIDs::lfo (2, "Wave"), (float) synth::mod::LfoWave::sampleHold).set (ParamIDs::lfo (2, "Rate"), 4.3f);
        voice (spores, 0.5f, 0.5f, 0.55f);
        route (spores, "lfo1", ParamIDs::voiceVowel, 0.3f);
        route (spores, "lfo2", ParamIDs::osc (2, "Volume"), 0.08f, 15.0f);
        route (spores, "lfo3", ParamIDs::osc (3, "Volume"), 0.06f, 15.0f);
        route (spores, "modWheel", ParamIDs::osc (2, "Volume"), 0.05f);
        route (spores, "modWheel", ParamIDs::osc (3, "Volume"), 0.04f);
        route (spores, "macro1", ParamIDs::osc (0, "NoiseColour"), -0.1f);
        route (spores, "macro3", ParamIDs::voiceSharp, 0.4f);
        route (spores, "macro3", ParamIDs::voiceSize, -0.4f);
        macros (spores, "BRIGHT", "SPACE", "MUTATE");
        delay (spores, "1/8 dotted", 0.4f, 0.15f, true, 5.5f);
        reverb (spores, RT::digital, 0.9f, 0.35f, 40.0f, 5.0f);
        eqBand (spores, 0, eq::Shape::lowCut, 200.0f, 0.0f);
        space (spores);
        spores.set (ParamIDs::patchLevel, -6.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (spores);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto mutation = x.make ("Mutation", 7, { "Bass", "Shape", "Formant", "Mono" },
            "A bass that changes as it holds: each note starts as a plain filtered saw and, over three seconds, turns "
            "strange (an envelope moving FORMANT, SKEW and a little CRUSH), over a clean sine kept off all of it. "
            "MACROS: BRIGHT, SPACE, MUTATE (it goes further). MOD WHEEL: opens the filter.");
        mutation.osc (0, "Volume", 0.6f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
                .filter (FilterType::lp24, 900.0f, 0.25f, 0.3f).filterEnvelope (0.0f, 400.0f, 0.4f, 300.0f)
                .ampEnvelope (5.0f, 800.0f, 0.9f, 400.0f)
                .mono (0.0f);
        clean (mutation, 1);
        oneShot (mutation, 0, { SP15 { 0.0f, 0.0f, 0.4f }, SP15 { 3.0f, 1.0f, 0.0f } });
        route (mutation, "env1", ParamIDs::osc (0, "Formant"), 0.2f);
        route (mutation, "env1", ParamIDs::osc (0, "Skew"), 0.25f);
        route (mutation, "env1", ParamIDs::osc (0, "Crush"), 0.15f);
        route (mutation, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (mutation, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (mutation, "macro3", ParamIDs::osc (0, "Formant"), 0.2f);
        route (mutation, "macro3", ParamIDs::osc (0, "Skew"), 0.2f);
        macros (mutation, "BRIGHT", "SPACE", "MUTATE");
        reverb (mutation, RT::room, 0.5f, 0.1f);
        space (mutation);
        mutation.set (ParamIDs::patchLevel, -4.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (mutation);

        auto rot = x.make ("Root Rot", 8, { "Bass", "Drone", "FM", "LFO Shape" },
            "A dark sub drone with something rotting on top: a clean sine, and a narrow pulse with a little FM at an "
            "uneven ratio (a damp, off-pitch growl), a diode shaper, a low filter breathing in a drawn, irregular "
            "shape (LFO 1's own curve, never quite a rhythm). MACROS: BRIGHT, SPACE, MUTATE (more rot: the shaper and "
            "FM). MOD WHEEL: opens the filter.");
        rot.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
           .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.12f).osc (1, "Volume", 0.45f)
           .osc (1, "FmAmount", 0.08f).osc (1, "ModRate", freeRate (0.53f))
           .shaper (0, synth::ShaperModel::diode, 0.35f)
           .filter (FilterType::lp24, 350.0f, 0.3f, 0.0f)
           .ampEnvelope (120.0f, 1000.0f, 1.0f, 800.0f)
           .mono (30.0f)
           .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::custom1).set (ParamIDs::lfo (0, "Rate"), 0.3f);
        clean (rot, 0);
        rot.rig.modulation.lfoShapes[0] = { { SP15 { 0.0f, 0.3f, 0.4f }, SP15 { 0.18f, 0.85f, -0.3f }, SP15 { 0.31f, 0.5f, 0.2f },
                                              SP15 { 0.47f, 0.95f, -0.6f }, SP15 { 0.7f, 0.15f, 0.3f }, SP15 { 0.86f, 0.55f, 0.0f } }, -1, -1, -1 };
        route (rot, "lfo1", ParamIDs::fltCutoff, 0.08f);
        route (rot, "modWheel", ParamIDs::fltCutoff, 0.12f);
        route (rot, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (rot, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        route (rot, "macro3", ParamIDs::osc (1, "FmAmount"), 0.1f);
        macros (rot, "BRIGHT", "SPACE", "MUTATE");
        reverb (rot, RT::hall, 0.6f, 0.12f);
        space (rot);
        rot.set (ParamIDs::patchLevel, -3.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (rot);

        auto cells = x.make ("Cell Division", 9, { "Bass", "Step Seq", "LFO", "Pulse" },
            "A pulsing bass that keeps dividing: a pulse wave beating in eighths with your song (the STEP SEQUENCER), and "
            "an octave copy of it, a few cents off, that splits away for the first half of every beat (a square LFO in "
            "time), over a clean sine. Hold a note. MACROS: BRIGHT, SPACE, MUTATE (the copy further out of tune and "
            "bent). MOD WHEEL: opens the filter.");
        cells.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.5f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", 12.0f).osc (1, "Fine", 8.0f)
             .osc (1, "Volume", 0.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.35f).osc (2, "PhaseRand", 0.0f)
             .filter (FilterType::lp24, 600.0f, 0.3f, 0.35f).filterEnvelope (0.0f, 180.0f, 0.3f, 150.0f)
             .ampEnvelope (3.0f, 500.0f, 1.0f, 200.0f)
             .mono (0.0f)
             .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.75f).set (ParamIDs::seqGate, 0.6f)
             .set (ParamIDs::seqContour, 0.15f).set (ParamIDs::seqDivision, division ("1/8")).set (ParamIDs::seqLength, 8.0f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::square)
             .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight).set (ParamIDs::lfo (0, "Division"), 5.0f)   // 1/4
             .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        clean (cells, 2);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = cells.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = true;
                step.level = k % 2 == 0 ? 1.0f : 0.6f;
            }
        route (cells, "lfo1", ParamIDs::osc (1, "Volume"), 0.15f, 5.0f);
        route (cells, "stepSeq", ParamIDs::fltCutoff, 0.08f);
        route (cells, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (cells, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (cells, "macro3", ParamIDs::osc (1, "Fine"), 0.1f);
        route (cells, "macro3", ParamIDs::osc (1, "Formant"), 0.15f);
        macros (cells, "BRIGHT", "SPACE", "MUTATE");
        reverb (cells, RT::hall, 0.5f, 0.1f);
        space (cells);
        cells.set (ParamIDs::patchLevel, -4.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (cells);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto folk = x.make ("Folk Pluck", 10, { "Keys", "Pluck", "Guitar-like", "Intimate" },
            "A quiet, fingerpicked string, close up: a saw and a pulse plucked by the filter (bright at the pick, "
            "brighter the harder you play, darker as it rings), a tiny noise click for the fingertip, a wooden body "
            "(a bump at 220 Hz) and a small room. Synthesis, not a sampled guitar: guitar-like, not a guitar. MACROS: "
            "BRIGHT, SPACE, MUTATE (the second string drifts out of tune, as if the instrument is changing). MOD WHEEL: "
            "brighter.");
        folk.osc (0, "Volume", 0.5f).osc (0, "PhaseRand", 0.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.3f)
            .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.4f).osc (2, "NoiseColour", 0.2f).osc (2, "Volume", 0.0f)
            .filter (FilterType::lp12, 1800.0f, 0.1f, 0.55f).filterEnvelope (0.0f, 350.0f, 0.05f, 300.0f)
            .set (ParamIDs::fltKey, 0.8f).set (ParamIDs::fltEnvVel, 0.6f)
            .ampEnvelope (1.0f, 1800.0f, 0.0f, 600.0f).set (ParamIDs::ampDecayCurve, -0.4f).set (ParamIDs::ampVel, 0.6f);
        oneShot (folk, 0, { SP15 { 0.0f, 1.0f, 0.0f }, SP15 { 0.02f, 0.0f, 0.0f } });
        route (folk, "env1", ParamIDs::osc (2, "Volume"), 0.15f);
        route (folk, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (folk, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (folk, "macro3", ParamIDs::osc (1, "Fine"), 0.15f);
        macros (folk, "BRIGHT", "SPACE", "MUTATE");
        eqBand (folk, 0, eq::Shape::bell, 220.0f, 3.0f, 1.0f);
        eqBand (folk, 1, eq::Shape::lowCut, 70.0f, 0.0f);
        reverb (folk, RT::room, 0.45f, 0.15f);
        space (folk);
        folk.set (ParamIDs::patchLevel, -0.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (folk);

        auto bloom = x.make ("Bloom Keys", 11, { "Keys", "FM", "Reverse Reverb", "Bell" },
            "Bell-like keys that blossom after you strike them: a sine with FM from oscillator 2 (a sine 19 semitones "
            "up, a hair sharp: a soft bell), the FM opening after the strike instead of before (ENV 1 rises over 0.6 s, "
            "then settles), a triangle an octave up, and a reverse reverb that swells up behind each note. MACROS: "
            "BRIGHT, SPACE, MUTATE (the bell's ratio pulled off: clangy, alien). MOD WHEEL: more FM.");
        bloom.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.03f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 19.0f).osc (1, "Fine", 12.0f)
             .osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.12f)
             .ampEnvelope (2.0f, 3000.0f, 0.1f, 1500.0f).set (ParamIDs::ampVel, 0.5f);
        oneShot (bloom, 0, { SP15 { 0.0f, 0.0f, 0.5f }, SP15 { 0.6f, 1.0f, -0.3f }, SP15 { 2.0f, 0.4f, 0.0f } });
        route (bloom, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (bloom, "modWheel", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (bloom, "macro1", ParamIDs::osc (2, "Volume"), 0.1f);
        route (bloom, "macro3", ParamIDs::osc (1, "Fine"), 0.2f);
        macros (bloom, "BRIGHT", "SPACE", "MUTATE");
        chorus (bloom, ParamIDs::modIds, MT::uniVibe, 0.3f, 0.3f, 0.3f);
        delay (bloom, "1/4", 0.3f, 0.12f, true, 4.5f);
        reverb (bloom, RT::reverse, 0.6f, 0.3f, 20.0f, 5.0f);
        space (bloom);
        bloom.set (ParamIDs::patchLevel, -3.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (bloom);

        auto crystal = x.make ("Crystal Trees", 12, { "Keys", "Chorder", "Crush", "Glass" },
            "Brittle crystal chimes: a triangle with two chorder notes far above it (an octave and a fifth a few cents "
            "sharp, and an octave and a flat seventh tuned down near the 7th harmonic, so they shimmer against each other), a touch of CRUSH for "
            "a glassy grain, every note a little out of tune at random, short and bright, through fast sixteenth echoes "
            "and a digital space. MACROS: BRIGHT, SPACE, MUTATE (more crush and a pushed formant: crystal cracking). MOD "
            "WHEEL: longer echoes.");
        crystal.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "Crush", 0.2f).osc (0, "CrushMode", 0.0f)
               .osc (0, "Random", 0.15f).osc (0, "PhaseRand", 0.0f)
               .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 19.0f).osc (0, "Ch1Fine", 13.0f).osc (0, "Ch1Vol", 0.35f).osc (0, "Ch1Pan", -0.4f)
               .osc (0, "Ch2On", 1.0f).osc (0, "Ch2Coarse", 22.0f).osc (0, "Ch2Fine", -27.0f).osc (0, "Ch2Vol", 0.2f).osc (0, "Ch2Pan", 0.4f)
               .filter (FilterType::hp12, 400.0f, 0.1f, 0.0f)
               .ampEnvelope (0.0f, 900.0f, 0.0f, 800.0f).set (ParamIDs::ampDecayCurve, -0.5f).set (ParamIDs::ampVel, 0.5f);
        route (crystal, "modWheel", ParamIDs::delayFeedback, 0.2f);
        route (crystal, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (crystal, "macro3", ParamIDs::osc (0, "Crush"), 0.3f);
        route (crystal, "macro3", ParamIDs::osc (0, "Formant"), 0.15f);
        macros (crystal, "BRIGHT", "SPACE", "MUTATE");
        delay (crystal, "1/16", 0.45f, 0.25f, true, 7.0f);
        reverb (crystal, RT::digital, 0.7f, 0.25f, 30.0f, 6.0f);
        space (crystal);
        crystal.set (ParamIDs::patchLevel, 7.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (crystal);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto doubling = x.make ("Doubling", 13, { "Arp", "Arps Together", "Stereo", "Mirror" },
            "Two arps mirroring each other: hold a chord, and ARP 1 climbs it in sixteenths on a pulse on the left while "
            "ARP 2 falls through it on a triangle an octave up on the right, a few cents off, both on the same clock, a slow flanger sweeping through them. "
            "MACROS: BRIGHT, SPACE, MUTATE (the mirror image drifts further from the original). ");
        doubling.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Pan", -0.6f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Fine", 7.0f)
                .osc (1, "Pan", 0.6f).osc (1, "Volume", 0.55f).osc (1, "Arp", 2.0f)
                .filter (FilterType::lp24, 2500.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 200.0f, 0.2f, 150.0f)
                .ampEnvelope (1.0f, 350.0f, 0.2f, 200.0f)
                .set (ParamIDs::arpsTogether, 1.0f);
        for (const auto [k, mode] : { std::pair { 0, synth::arp::Mode::up }, std::pair { 1, synth::arp::Mode::down } })
            doubling.set (ParamIDs::arp (k, "On"), 1.0f).set (ParamIDs::arp (k, "Mode"), (float) mode)
                    .set (ParamIDs::arp (k, "Division"), division ("1/16")).set (ParamIDs::arp (k, "Octaves"), 2.0f)
                    .set (ParamIDs::arp (k, "Gate"), 0.5f);
        route (doubling, "macro1", ParamIDs::fltCutoff, 0.25f);
        route (doubling, "macro3", ParamIDs::osc (1, "Fine"), 0.15f);
        route (doubling, "macro3", ParamIDs::osc (1, "Formant"), 0.2f);
        macros (doubling, "BRIGHT", "SPACE", "MUTATE");
        chorus (doubling, ParamIDs::modIds, MT::flanger, 0.08f, 0.6f, 0.4f);   // a slow AEON sweep through both
        delay (doubling, "1/8 dotted", 0.3f, 0.15f, true, 5.0f);
        reverb (doubling, RT::hall, 0.6f, 0.2f);
        space (doubling);
        doubling.set (ParamIDs::patchLevel, -2.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (doubling);

        auto glow = x.make ("Bioluminescence", 14, { "Arp", "Random", "Pitch", "Whole Tone" },
            "Little lights blinking on in the dark: ARP 1 plays random notes from the chord you hold over three octaves, "
            "in a dreamlike whole-tone scale, with plenty of rests, a new pattern every pass (AUTODICE); every note is a "
            "glassy blip that chirps down into its pitch from an octave up in a few milliseconds (the PITCH envelope), "
            "a quiet sine two octaves over it, echoes and a digital space. MACROS: BRIGHT, SPACE, MUTATE (FORMANT and a "
            "deeper chirp). MOD WHEEL: more of the high sine.");
        glow.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.1f).osc (1, "Arp", 1.0f)
            .ampEnvelope (1.0f, 500.0f, 0.0f, 500.0f).set (ParamIDs::ampDecayCurve, -0.4f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
            .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 3.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.3f).set (ParamIDs::arp (0, "Humanize"), 0.1f);
        glow.rig.modulation.pitchEnvelope = { { SP15 { 0.0f, 1.0f, -0.5f }, SP15 { 0.03f, 0.5f, 0.0f } }, -1, -1, -1 };
        {
            auto& r = glow.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Whole Tone");
            r.restChance = 0.45f;
            r.lengthChances = { 1.0f, 0.3f, 0.0f, 0.0f };
            r.velocityLow = 0.4f;
            r.velocityHigh = 1.0f;
            r.autoDice = true;
            r.seed = 11;
            auto& p = glow.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        route (glow, "modWheel", ParamIDs::osc (1, "Volume"), 0.08f);
        route (glow, "macro1", ParamIDs::osc (1, "Volume"), 0.05f);
        route (glow, "macro3", ParamIDs::osc (0, "Formant"), 0.25f);
        route (glow, "macro3", ParamIDs::pitchEnvRange, 0.25f);
        macros (glow, "BRIGHT", "SPACE", "MUTATE");
        delay (glow, "1/16 dotted", 0.45f, 0.3f, true, 6.0f);
        glow.set (ParamIDs::delayToReverb, 0.4f);
        reverb (glow, RT::digital, 0.8f, 0.3f, 40.0f, 5.5f);
        space (glow);
        glow.set (ParamIDs::patchLevel, 3.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glow);

        auto garden = x.make ("Pulse Garden", 15, { "Arp", "Voice", "Ratchet", "Organic" },
            "A slow, organic pulse, like something breathing in the dark: ARP 1 plays the chord you hold in eighths "
            "with uneven accents, a few rests, a flutter (ratchets) now and then and one slide, each pulse a saw sung by "
            "the VOICE filter opening from \"oo\" and closing again (an envelope restarted by every arp note), a sine "
            "body under it, a hall. MACROS: BRIGHT, SPACE, MUTATE (the voice's size, human to something else).");
        garden.osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "Arp", 1.0f)
              .osc (1, "Voice", 0.0f)
              .ampEnvelope (10.0f, 500.0f, 0.3f, 250.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
              .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
              .set (ParamIDs::arp (0, "Gate"), 0.8f)
              .set (ParamIDs::env (0, "Trigger"), (float) synth::mod::EnvelopeTrigger::arpNotes);
        oneShot (garden, 0, { SP15 { 0.0f, 0.0f, 0.3f }, SP15 { 0.25f, 1.0f, 0.2f }, SP15 { 0.7f, 0.3f, 0.0f } });
        voice (garden, 0.0f, 0.3f, 0.6f);
        route (garden, "env1", ParamIDs::voiceVowel, 0.6f);
        {
            auto& p = garden.rig.modulation.arps[0].patterns[0];
            p = {};
            constexpr float velocities[] = { 1.0f, 0.55f, 0.7f, 0.5f, 0.9f, 0.6f, 0.0f, 0.65f, 1.0f, 0.5f, 0.75f, 0.55f, 0.85f, 0.0f, 0.6f, 0.7f };
            for (int k = 0; k < 16; ++k)
            {
                auto& block = p.blocks[(size_t) k];
                block.on = velocities[k] > 0.0f;
                block.velocity = velocities[k];
            }
            p.blocks[7].ratchet = 3;
            p.blocks[15].ratchet = 2;
            p.blocks[10].slide = true;
        }
        route (garden, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (garden, "macro3", ParamIDs::voiceSize, 0.5f);
        macros (garden, "BRIGHT", "SPACE", "MUTATE");
        delay (garden, "1/4 dotted", 0.3f, 0.15f, true, 4.5f);
        reverb (garden, RT::hall, 0.7f, 0.25f);
        space (garden);
        garden.set (ParamIDs::patchLevel, 1.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (garden);
    }

    // PSI STORM: inspired by the score of a 1988 animated cyberpunk film about a biker gang and a boy whose psychic
    // power breaks loose (Balinese gamelan with its shimmering paired metallophones and deep bamboo, rhythmic group
    // chant and slow Buddhist-style chant, a neon city at night, light trails, power swelling out of control, a body
    // mutating). No title, characters, the city, the band, terms or music from the film: our own sounds. MACRO 3 is
    // POWER on every preset: the energy breaking loose.
    void psiStorm (Expansion x)
    {
        using SP16 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;
        const auto oneShot = [] (Builder& b, int env, std::vector<SP16> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto voice = [] (Builder& b, float vowel, float size, float sharp)
        {
            b.set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceVowel, vowel).set (ParamIDs::voiceSize, size).set (ParamIDs::voiceSharp, sharp);
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers, buses and VOICE
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };
        // A bronze metallophone the gamelan way: two sines a pair, one tuned a little flat and one a little sharp
        // (so they beat: the shimmer the gamelan tunes in on purpose), both with FM from a sine at about 2.76 times
        // the note (a struck bar's first overtone, not a harmonic: the bell-like clang), the FM bright at the strike
        // and falling fast (ENV `env`). Oscillators o and o + 1 heard, o + 2 the modulator. `arp`: their ARP.
        const auto bronze = [&oneShot] (Builder& b, int o, int env, float volume, float cents, float arp = 0.0f)
        {
            for (int k = 0; k < 2; ++k)
                b.osc (o + k, "On", 1.0f).osc (o + k, "Wave", sine).osc (o + k, "Volume", volume).osc (o + k, "PhaseRand", 0.0f)
                 .osc (o + k, "Fine", k == 0 ? -cents : cents).osc (o + k, "Pan", k == 0 ? -0.3f : 0.3f)
                 .osc (o + k, "FmSource", (float) (synth::fmFirstOscillator + o + 2)).osc (o + k, "FmAmount", 0.05f)
                 .osc (o + k, "Arp", arp);
            b.osc (o + 2, "On", 1.0f).osc (o + 2, "Wave", sine).osc (o + 2, "Transpose", 18.0f).osc (o + 2, "Fine", -42.0f)
             .osc (o + 2, "Volume", 1.0f).osc (o + 2, "PhaseRand", 0.0f).osc (o + 2, "Arp", arp);
            oneShot (b, env, { SP16 { 0.0f, 1.0f, -0.6f }, SP16 { 0.5f, 0.15f, 0.0f } });
            const auto source = "env" + juce::String (env + 1);
            route (b, source.toRawUTF8(), ParamIDs::osc (o, "FmAmount"), 0.15f);
            route (b, source.toRawUTF8(), ParamIDs::osc (o + 1, "FmAmount"), 0.15f);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto leader = x.make ("Chant Leader", 1, { "Lead", "Voice", "Chant", "LFO Shape" },
            "A single male voice calling out the chant: a saw and a narrow pulse sung by the VOICE filter at a man's "
            "size, its vowel stepping through syllables in time with your song (a drawn LFO shape on VOWEL, a quarter "
            "note a cycle: \"ah-oh-eh-ah\"), each note pushed up into pitch from a little under, a slow, wide vibrato, a "
            "breath of tube grit, a hall. MACROS: BRIGHT, SPACE, POWER (more grit and a sharper voice). MOD WHEEL: wider "
            "vibrato.");
        leader.osc (0, "Volume", 0.6f).osc (0, "VibAmount", 12.0f).osc (0, "VibRate", 4.0f).osc (0, "VibFade", 700.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.2f).osc (1, "Fine", 4.0f).osc (1, "Volume", 0.3f)
              .shaper (0, synth::ShaperModel::tube, 0.1f, true)
              .ampEnvelope (40.0f, 600.0f, 0.9f, 500.0f)
              .mono (40.0f)
              .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 1.0f)
              .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::custom1)
              .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight).set (ParamIDs::lfo (0, "Division"), 5.0f)   // 1/4
              .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        leader.rig.modulation.pitchEnvelope = { { SP16 { 0.0f, 0.0f, -0.4f }, SP16 { 0.09f, 0.5f, 0.0f } }, -1, -1, -1 };
        leader.rig.modulation.lfoShapes[0] = { { SP16 { 0.0f, 0.5f, -0.6f }, SP16 { 0.25f, 0.0f, -0.6f }, SP16 { 0.5f, 1.0f, -0.6f },
                                                 SP16 { 0.75f, 0.5f, -0.6f } }, -1, -1, -1 };
        voice (leader, 0.45f, 0.1f, 0.5f);
        route (leader, "lfo1", ParamIDs::voiceVowel, 0.2f, 40.0f);
        route (leader, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (leader, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (leader, "macro3", ParamIDs::shp (0, "Drive"), 0.35f);
        route (leader, "macro3", ParamIDs::voiceSharp, 0.2f);
        macros (leader, "BRIGHT", "SPACE", "POWER");
        delay (leader, "1/8 dotted", 0.3f, 0.12f, true, 4.0f);
        reverb (leader, RT::hall, 0.75f, 0.25f, 30.0f, 4.5f);
        space (leader);
        leader.set (ParamIDs::patchLevel, -3.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (leader);

        auto trail = x.make ("Tail Light Streak", 2, { "Lead", "Pitch", "Unison", "Delay" },
            "A bright, fast lead that streaks into every note like a tail light smearing across the night: a three-voice "
            "saw unison that drops into each note from an octave up in a few tens of milliseconds (the PITCH envelope), "
            "a quick glide between notes, fast sixteenth echoes trailing behind, a chorus. MACROS: BRIGHT, SPACE, POWER "
            "(a longer streak and a wider unison). MOD WHEEL: more echo.");
        trail.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.6f)
             .osc (0, "Volume", 0.55f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.25f)
             .osc (1, "PitchEnv", 0.0f)
             .filter (FilterType::lp24, 5000.0f, 0.15f, 0.2f).filterEnvelope (0.0f, 300.0f, 0.6f, 300.0f)
             .set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (3.0f, 400.0f, 0.85f, 350.0f)
             .mono (30.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        trail.rig.modulation.pitchEnvelope = { { SP16 { 0.0f, 1.0f, -0.5f }, SP16 { 0.07f, 0.5f, 0.0f } }, -1, -1, -1 };
        route (trail, "modWheel", ParamIDs::delayMix, 0.2f);
        route (trail, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (trail, "macro3", ParamIDs::pitchEnvRange, 0.25f);
        route (trail, "macro3", ParamIDs::osc (0, "UniDetune"), 0.25f);
        macros (trail, "BRIGHT", "SPACE", "POWER");
        chorus (trail, ParamIDs::modIds, MT::chorus, 0.6f, 0.35f, 0.3f);
        delay (trail, "1/16", 0.5f, 0.22f, true, 6.0f);
        reverb (trail, RT::plate, 0.5f, 0.15f);
        space (trail);
        trail.set (ParamIDs::patchLevel, -3.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (trail);

        auto scream = x.make ("Psychic Scream", 3, { "Lead", "Sync", "Drive", "Envelope" },
            "A lead whose power grows the longer you hold it: a saw that starts clean, then over four seconds its pitch "
            "rises a whole step, SYNC tears into it, a hard shaper and insert bus A's SKYLINE build, and at the top it "
            "cracks (CRUSH breaks in), over a pulse an octave down. Hold the notes. MACROS: BRIGHT, SPACE, POWER (all "
            "of it sooner and harder). MOD WHEEL: vibrato.");
        scream.osc (0, "Volume", 0.55f).osc (0, "Insert", 1.0f).osc (0, "VibRate", 6.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
              .osc (1, "Insert", 1.0f)
              .shaper (0, synth::ShaperModel::hard, 0.05f)
              .filter (FilterType::lp24, 2500.0f, 0.25f, 0.0f).set (ParamIDs::fltKey, 0.5f)
              .ampEnvelope (20.0f, 600.0f, 0.9f, 600.0f)
              .mono (60.0f)
              .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f)
              .set (ParamIDs::pitchEnvMode, (float) synth::mod::EnvelopeMode::sustain);
        scream.rig.modulation.pitchEnvelope = { { SP16 { 0.0f, 0.5f, 0.4f }, SP16 { 4.0f, 1.0f, 0.0f } }, 1, -1, -1 };
        oneShot (scream, 0, { SP16 { 0.0f, 0.0f, 0.5f }, SP16 { 4.0f, 1.0f, 0.0f } });                                    // the build
        oneShot (scream, 1, { SP16 { 0.0f, 0.0f, 0.0f }, SP16 { 3.5f, 0.0f, -0.5f }, SP16 { 4.0f, 1.0f, 0.0f } });          // the crack
        drive (scream, 0, DM::skyline, 3.0f, 4.5f, 2.0f, 0.3f, 0.3f);
        route (scream, "env1", ParamIDs::osc (0, "SyncAmount"), 0.4f);
        route (scream, "env1", ParamIDs::shp (0, "Drive"), 0.4f);
        route (scream, "env1", ParamIDs::fltCutoff, 0.15f);
        route (scream, "env2", ParamIDs::osc (0, "Crush"), 0.3f);
        route (scream, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.25f);
        route (scream, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (scream, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.2f);
        route (scream, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        macros (scream, "BRIGHT", "SPACE", "POWER");
        delay (scream, "1/4", 0.35f, 0.15f, true, 4.0f);
        reverb (scream, RT::hall, 0.8f, 0.25f);
        space (scream);
        scream.set (ParamIDs::patchLevel, -1.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (scream);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto requiem = x.make ("Requiem Chant", 4, { "Pad", "Voice", "Chant", "Fifths" },
            "Low monks chanting in parallel fifths, as old chant does: a saw section with its own fifth above in the "
            "chorder, sung by the VOICE filter at a bass's size, the vowel turning slowly \"oo-oh-ah\" and back as if "
            "drawing out one long syllable (a slow LFO), a throat drone an octave down, a temple of a reverb. MACROS: "
            "BRIGHT, SPACE, POWER (the voices open and harden). MOD WHEEL: the vowel opens.");
        requiem.osc (0, "Volume", 0.5f).osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.8f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.8f)
               .osc (0, "VibMode", (float) synth::VibratoMode::independent).osc (0, "VibAmount", 7.0f).osc (0, "VibRate", 4.2f).osc (0, "VibFade", 800.0f)
               .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 7.0f).osc (0, "Ch1Vol", 0.6f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.15f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
               .ampEnvelope (700.0f, 1500.0f, 0.95f, 2500.0f)
               .set (ParamIDs::lfo (0, "Rate"), 0.08f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        voice (requiem, 0.25f, 0.0f, 0.45f);
        route (requiem, "lfo1", ParamIDs::voiceVowel, 0.15f);
        route (requiem, "modWheel", ParamIDs::voiceVowel, 0.2f, 100.0f);
        route (requiem, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (requiem, "macro3", ParamIDs::voiceVowel, 0.2f);
        route (requiem, "macro3", ParamIDs::voiceSharp, 0.3f);
        macros (requiem, "BRIGHT", "SPACE", "POWER");
        reverb (requiem, RT::cathedral, 0.85f, 0.32f, 40.0f, 4.0f);
        eqBand (requiem, 0, eq::Shape::lowCut, 55.0f, 0.0f);
        space (requiem);
        requiem.set (ParamIDs::patchLevel, -12.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (requiem);

        auto ruins = x.make ("Ruined City", 5, { "Pad", "FM", "Noise", "Lo-Fi" },
            "A broken city after the blast: cold metallic air from sines with slow, deep FM at an uneven ratio (the "
            "metal sings and shifts, a slow LFO moving the FM), wind through it (noise through a band-pass whose centre "
            "drifts), worn like an old tape (LO-FI's cassette, lightly), a long hall. MACROS: BRIGHT, SPACE, POWER (the "
            "metal gets harsher). MOD WHEEL: more wind.");
        ruins.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.08f)
             .osc (0, "UniSpread", 0.9f).osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.12f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 21.0f).osc (1, "Fine", 18.0f).osc (1, "Volume", 1.0f)
             .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.5f).osc (2, "NoiseColour", 0.1f).osc (2, "NoiseWidth", 2.0f)
             .osc (2, "Volume", 0.25f)
             .filter (FilterType::bp12, 1200.0f, 0.3f, 0.0f)
             .ampEnvelope (1500.0f, 2000.0f, 1.0f, 3000.0f)
             .set (ParamIDs::lfo (0, "Rate"), 0.05f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
             .set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (1, "Rate"), 0.15f)
             .set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global)
             .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
             .set (ParamIDs::lofiAmount, 3.0f).set (ParamIDs::lofiMix, 0.5f).set (ParamIDs::lofiNoise, 0.0f);
        ruins.osc (0, "Filter", 0.0f).osc (1, "Filter", 0.0f);   // only the wind goes through the band-pass
        route (ruins, "lfo1", ParamIDs::osc (0, "FmAmount"), 0.08f);
        route (ruins, "lfo2", ParamIDs::fltCutoff, 0.15f);
        route (ruins, "modWheel", ParamIDs::osc (2, "Volume"), 0.2f);
        route (ruins, "macro1", ParamIDs::osc (0, "FmAmount"), 0.08f);
        route (ruins, "macro3", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (ruins, "macro3", ParamIDs::osc (1, "Fine"), 0.1f);
        macros (ruins, "BRIGHT", "SPACE", "POWER");
        reverb (ruins, RT::hall, 0.9f, 0.3f, 50.0f, 4.0f);
        eqBand (ruins, 0, eq::Shape::lowCut, 80.0f, 0.0f);
        space (ruins);
        ruins.set (ParamIDs::patchLevel, -6.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ruins);

        auto surge = x.make ("Power Surge", 6, { "Pad", "Shaper", "Unison", "Swell" },
            "Power building up that can't be held in: a saw unison swelling over six seconds as it holds, the filter "
            "opening, the copies drifting further apart, a SINE FOLD shaper folding it into a bright, unstable roar, and "
            "a nervous random flutter in the pitch (a random vibrato) that grows with it. MACROS: BRIGHT, SPACE, POWER (more fold: it "
            "bursts). MOD WHEEL: more flutter.");
        surge.osc (0, "Volume", 0.5f).osc (0, "VibWave", (float) synth::VibratoWave::random).osc (0, "VibRate", 9.0f)
             .osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.75f).osc (0, "UniDetune", 0.06f).osc (0, "UniSpread", 0.9f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
             .shaper (0, synth::ShaperModel::sineFold, 0.0f)
             .filter (FilterType::lp24, 600.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.3f)
             .ampEnvelope (2500.0f, 2000.0f, 1.0f, 2500.0f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 7.0f);
        oneShot (surge, 0, { SP16 { 0.0f, 0.0f, 0.4f }, SP16 { 6.0f, 1.0f, 0.0f } });
        route (surge, "env1", ParamIDs::fltCutoff, 0.3f);
        route (surge, "env1", ParamIDs::osc (0, "UniDetune"), 0.2f);
        route (surge, "env1", ParamIDs::shp (0, "Drive"), 0.25f);
        route (surge, "env1", ParamIDs::osc (0, "VibAmount"), 0.12f);   // the flutter grows with it
        route (surge, "lfo1", ParamIDs::osc (0, "Fine"), 0.03f);
        route (surge, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        route (surge, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (surge, "macro3", ParamIDs::shp (0, "Drive"), 0.35f);
        macros (surge, "BRIGHT", "SPACE", "POWER");
        reverb (surge, RT::hall, 0.85f, 0.28f);
        eqBand (surge, 0, eq::Shape::lowCut, 60.0f, 0.0f);
        space (surge);
        surge.set (ParamIDs::patchLevel, -13.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (surge);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto bamboo = x.make ("Bamboo Bass", 7, { "Bass", "Struck", "Hollow", "Pairs" },
            "Deep struck bamboo tubes, like the giant bamboo gamelan: a hollow square wave (odd harmonics only: the sound "
            "of a tube) plucked by the filter and a sine a pair, tuned a few cents apart so they beat slowly, a soft "
            "thump of a mallet, ringing for a couple of seconds. MACROS: BRIGHT, SPACE, POWER (a harder strike).");
        bamboo.osc (0, "Wave", sine).osc (0, "Volume", 0.55f).osc (0, "Fine", -5.0f).osc (0, "PhaseRand", 0.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Fine", 5.0f).osc (1, "Volume", 0.4f)
              .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.5f).osc (2, "NoiseColour", 0.5f).osc (2, "Volume", 0.0f)
              .filter (FilterType::lp12, 300.0f, 0.15f, 0.4f).filterEnvelope (0.0f, 250.0f, 0.0f, 300.0f)
              .set (ParamIDs::fltKey, 0.6f).set (ParamIDs::fltEnvVel, 0.5f)
              .ampEnvelope (1.0f, 2200.0f, 0.0f, 600.0f).set (ParamIDs::ampDecayCurve, -0.3f).set (ParamIDs::ampVel, 0.5f);
        clean (bamboo, 0);
        oneShot (bamboo, 0, { SP16 { 0.0f, 1.0f, -0.5f }, SP16 { 0.03f, 0.0f, 0.0f } });
        route (bamboo, "env1", ParamIDs::osc (2, "Volume"), 0.15f);
        route (bamboo, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (bamboo, "macro3", ParamIDs::fltEnv, 0.25f);
        macros (bamboo, "BRIGHT", "SPACE", "POWER");
        reverb (bamboo, RT::room, 0.6f, 0.12f);
        space (bamboo);
        bamboo.set (ParamIDs::patchLevel, -3.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (bamboo);

        auto ride = x.make ("Night Ride", 8, { "Bass", "Arp", "Octaves", "Drive" },
            "A driving bass for the chase through the city at night: hold a note and ARP 1 runs it in sixteenths with "
            "your song, jumping up an octave on the offbeats (the pattern), a saw and a pulse with a snappy filter, a "
            "clean sine under it, SKYLINE on insert bus A for growl. MACROS: BRIGHT, SPACE, POWER (more filter snap and "
            "drive). MOD WHEEL: opens the filter.");
        ride.osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.35f).osc (1, "Fine", 6.0f).osc (1, "Volume", 0.35f)
            .osc (1, "Arp", 1.0f).osc (1, "Insert", 1.0f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.4f).osc (2, "PhaseRand", 0.0f).osc (2, "Arp", 1.0f)
            .filter (FilterType::lp24, 500.0f, 0.3f, 0.35f).filterEnvelope (0.0f, 140.0f, 0.15f, 100.0f)
            .ampEnvelope (2.0f, 200.0f, 0.6f, 80.0f)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
            .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.6f);
        clean (ride, 2);
        ride.osc (2, "Arp", 1.0f);
        {
            auto& p = ride.rig.modulation.arps[0].patterns[0];
            p = {};
            for (int k = 0; k < 16; ++k)
            {
                auto& block = p.blocks[(size_t) k];
                block.on = true;
                block.transpose = k % 4 == 2 ? 12 : 0;
                block.velocity = k % 4 == 0 ? 1.0f : 0.75f;
            }
        }
        drive (ride, 0, DM::skyline, 3.0f, 5.0f, 3.0f, 0.1f, 0.15f);
        route (ride, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (ride, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (ride, "macro3", ParamIDs::fltEnv, 0.25f);
        route (ride, "macro3", ParamIDs::fltRes, 0.2f);
        macros (ride, "BRIGHT", "SPACE", "POWER");
        reverb (ride, RT::room, 0.5f, 0.08f);
        space (ride);
        ride.set (ParamIDs::patchLevel, 0.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ride);

        auto flesh = x.make ("Mutant Flesh", 9, { "Bass", "Wavetable", "Shaper", "Wobble" },
            "A bloated, wobbling growl, like something growing out of control: the FM Sweep table swelling back and forth "
            "in an uneven wobble (a drawn LFO shape, two to a beat with your song), FORMANT bulging with it, a SINE FOLD "
            "shaper, the pitch sagging and swelling slowly, a clean sine under it. MACROS: BRIGHT, SPACE, POWER (more "
            "fold and bulge). MOD WHEEL: more bulge.");
        flesh.osc (0, "Wave", wavetable).osc (0, "Table", table ("FM Sweep")).osc (0, "WtIndex", 0.3f).osc (0, "Volume", 0.6f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
             .shaper (0, synth::ShaperModel::sineFold, 0.15f)
             .filter (FilterType::lp24, 1200.0f, 0.2f, 0.0f)
             .ampEnvelope (10.0f, 800.0f, 1.0f, 300.0f)
             .mono (40.0f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::custom1)
             .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight).set (ParamIDs::lfo (0, "Division"), 6.0f)   // 1/8
             .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
             .set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (1, "Rate"), 0.4f);
        clean (flesh, 1);
        flesh.rig.modulation.lfoShapes[0] = { { SP16 { 0.0f, 0.0f, 0.5f }, SP16 { 0.35f, 1.0f, -0.4f }, SP16 { 0.55f, 0.3f, 0.3f },
                                                SP16 { 0.7f, 0.8f, -0.5f } }, -1, -1, -1 };
        route (flesh, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.25f);
        route (flesh, "lfo1", ParamIDs::osc (0, "Formant"), 0.1f);
        route (flesh, "lfo1", ParamIDs::fltCutoff, 0.1f);
        route (flesh, "lfo2", ParamIDs::osc (0, "Fine"), 0.06f);
        route (flesh, "modWheel", ParamIDs::osc (0, "Formant"), 0.15f);
        route (flesh, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (flesh, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        route (flesh, "macro3", ParamIDs::osc (0, "Formant"), 0.15f);
        macros (flesh, "BRIGHT", "SPACE", "POWER");
        reverb (flesh, RT::room, 0.5f, 0.1f);
        space (flesh);
        flesh.set (ParamIDs::patchLevel, -9.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (flesh);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto gamelan = x.make ("Bronze Gamelan", 10, { "Keys", "FM", "Gamelan", "Pairs" },
            "Bronze metallophones played as the gamelan pairs them: each note on two instruments, one tuned a little flat "
            "and one a little sharp, so they beat and shimmer (left and right), each with a bell-like clang from FM at a "
            "struck bar's overtone, bright at the strike, ringing for a few seconds, a hall. Synthesis, not recorded "
            "bronze. MACROS: BRIGHT, SPACE, POWER (a wider shimmer and a harder clang).");
        bronze (gamelan, 0, 0, 0.5f, 8.0f);
        gamelan.ampEnvelope (1.0f, 3500.0f, 0.0f, 1500.0f).set (ParamIDs::ampDecayCurve, -0.3f).set (ParamIDs::ampVel, 0.5f);
        route (gamelan, "macro1", ParamIDs::osc (0, "FmAmount"), 0.05f);
        route (gamelan, "macro1", ParamIDs::osc (1, "FmAmount"), 0.05f);
        route (gamelan, "macro3", ParamIDs::osc (0, "Fine"), -0.04f);
        route (gamelan, "macro3", ParamIDs::osc (1, "Fine"), 0.04f);
        route (gamelan, "macro3", ParamIDs::osc (0, "FmAmount"), 0.08f);
        route (gamelan, "macro3", ParamIDs::osc (1, "FmAmount"), 0.08f);
        macros (gamelan, "BRIGHT", "SPACE", "POWER");
        reverb (gamelan, RT::hall, 0.6f, 0.22f);
        space (gamelan);
        gamelan.set (ParamIDs::patchLevel, -8.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (gamelan);

        auto gong = x.make ("Temple Gong", 11, { "Keys", "FM", "Gong", "Pitch" },
            "Big temple gongs: a low sine with a sine an octave up a little sharp (the beating hum of a gong's boss), "
            "FM at an uneven ratio that blooms after the strike and dies away (ENV 1), the pitch sagging a little as it "
            "rings (a gong bends down), a soft noise rush at the strike, long ringing, a cathedral. Play low. MACROS: "
            "BRIGHT, SPACE, POWER (a bigger bloom).");
        gong.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
            .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 2)).osc (0, "FmAmount", 0.02f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Fine", 14.0f).osc (1, "Volume", 0.3f)
            .osc (1, "PhaseRand", 0.0f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 6.0f).osc (2, "Fine", 3.0f).osc (2, "Volume", 1.0f)
            .osc (3, "On", 1.0f).osc (3, "Gain", 0.0f).osc (3, "Noise", 0.4f).osc (3, "NoiseColour", 0.4f).osc (3, "Volume", 0.0f)
            .ampEnvelope (2.0f, 7000.0f, 0.0f, 3000.0f).set (ParamIDs::ampDecayCurve, -0.3f).set (ParamIDs::ampVel, 0.5f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 1.0f);
        gong.rig.modulation.pitchEnvelope = { { SP16 { 0.0f, 0.5f, 0.3f }, SP16 { 3.0f, 0.25f, 0.0f } }, -1, -1, -1 };
        oneShot (gong, 0, { SP16 { 0.0f, 0.0f, 0.4f }, SP16 { 0.5f, 1.0f, -0.4f }, SP16 { 4.0f, 0.0f, 0.0f } });
        oneShot (gong, 1, { SP16 { 0.0f, 1.0f, -0.5f }, SP16 { 0.15f, 0.0f, 0.0f } });
        route (gong, "env1", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (gong, "env2", ParamIDs::osc (3, "Volume"), 0.1f);
        route (gong, "macro1", ParamIDs::osc (1, "Volume"), 0.1f);
        route (gong, "macro3", ParamIDs::osc (0, "FmAmount"), 0.12f);
        macros (gong, "BRIGHT", "SPACE", "POWER");
        reverb (gong, RT::cathedral, 0.7f, 0.25f);
        space (gong);
        gong.set (ParamIDs::patchLevel, -3.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (gong);

        auto marimba = x.make ("Bamboo Marimba", 12, { "Keys", "Additive", "Wood", "Mallet" },
            "Woody bamboo keys: a sine with the bar's overtones built in as a tuned bar has them (two octaves up, and a "
            "little over three octaves and a third up, each fading much faster than the note), a mallet's click, short "
            "and bright, a room. MACROS: BRIGHT (more of the overtones), SPACE, POWER (a harder mallet).");
        marimba.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.0f).osc (1, "PhaseRand", 0.0f)
               .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 40.0f).osc (2, "Fine", -14.0f).osc (2, "Volume", 0.0f)
               .osc (2, "PhaseRand", 0.0f)
               .osc (3, "On", 1.0f).osc (3, "Gain", 0.0f).osc (3, "Noise", 0.4f).osc (3, "NoiseColour", 0.2f).osc (3, "Volume", 0.0f)
               .ampEnvelope (1.0f, 900.0f, 0.0f, 400.0f).set (ParamIDs::ampDecayCurve, -0.4f).set (ParamIDs::ampVel, 0.6f);
        oneShot (marimba, 0, { SP16 { 0.0f, 1.0f, -0.5f }, SP16 { 0.25f, 0.0f, 0.0f } });   // the 4th harmonic
        oneShot (marimba, 1, { SP16 { 0.0f, 1.0f, -0.5f }, SP16 { 0.08f, 0.0f, 0.0f } });   // the high overtone
        oneShot (marimba, 2, { SP16 { 0.0f, 1.0f, -0.5f }, SP16 { 0.012f, 0.0f, 0.0f } });  // the click
        route (marimba, "env1", ParamIDs::osc (1, "Volume"), 0.12f);
        route (marimba, "env2", ParamIDs::osc (2, "Volume"), 0.05f);
        route (marimba, "env3", ParamIDs::osc (3, "Volume"), 0.1f);
        route (marimba, "macro1", ParamIDs::osc (1, "Volume"), 0.06f);
        route (marimba, "macro3", ParamIDs::osc (3, "Volume"), 0.06f);
        route (marimba, "macro3", ParamIDs::osc (2, "Volume"), 0.04f);
        macros (marimba, "BRIGHT", "SPACE", "POWER");
        reverb (marimba, RT::room, 0.6f, 0.15f);
        space (marimba);
        marimba.set (ParamIDs::patchLevel, -0.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (marimba);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto interlock = x.make ("Interlock", 13, { "Arp", "Gamelan", "Pelog", "Arps Together" },
            "Two bronze parts locked together as gamelan players do, each filling the other's gaps: hold one key (the "
            "root) and ARP 1 and ARP 2 play together in sixteenths, ARP 1 on the beats and ARP 2 between them, their notes "
            "from the pelog scale (the gamelan's own five notes), each on a shimmering bronze pair, ARP 2 an octave up. "
            "MACROS: BRIGHT, SPACE, POWER (a harder clang).");
        bronze (interlock, 0, 0, 0.45f, 7.0f, 1.0f);
        bronze (interlock, 3, 1, 0.4f, 7.0f, 2.0f);
        interlock.osc (3, "Transpose", 12.0f).osc (4, "Transpose", 12.0f).osc (5, "Transpose", 30.0f)
                 .ampEnvelope (1.0f, 900.0f, 0.0f, 600.0f).set (ParamIDs::ampDecayCurve, -0.3f)
                 .set (ParamIDs::arpsTogether, 1.0f);
        for (int k = 0; k < 2; ++k)
        {
            interlock.set (ParamIDs::arp (k, "On"), 1.0f).set (ParamIDs::arp (k, "Mode"), (float) synth::arp::Mode::up)
                     .set (ParamIDs::arp (k, "Division"), division ("1/16")).set (ParamIDs::arp (k, "Octaves"), 1.0f)
                     .set (ParamIDs::arp (k, "Gate"), 0.8f);
            auto& r = interlock.rig.modulation.arpRandomizers[(size_t) k];
            r.scale = synth::arp::scaleNames.indexOf ("Pelog");
            r.restChance = 0.0f;
            r.lengthChances = { 1.0f, 0.0f, 0.0f, 0.0f };
            r.velocityLow = 0.7f;
            r.velocityHigh = 1.0f;
            r.seed = 21 + k;
            auto& p = interlock.rig.modulation.arps[(size_t) k].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
            for (int step = 0; step < 16; ++step)
                if (step % 2 != k)
                    p.blocks[(size_t) step].on = false;   // ARP 1 on the even sixteenths, ARP 2 on the odd
        }
        route (interlock, "macro1", ParamIDs::osc (0, "FmAmount"), 0.05f);
        route (interlock, "macro1", ParamIDs::osc (3, "FmAmount"), 0.05f);
        route (interlock, "macro3", ParamIDs::osc (1, "FmAmount"), 0.08f);
        route (interlock, "macro3", ParamIDs::osc (4, "FmAmount"), 0.08f);
        macros (interlock, "BRIGHT", "SPACE", "POWER");
        reverb (interlock, RT::hall, 0.6f, 0.2f);
        space (interlock);
        interlock.set (ParamIDs::patchLevel, -3.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (interlock);

        auto chant = x.make ("Chant Rhythm", 14, { "Arp", "Voice", "Chant", "Syncopated" },
            "A crowd barking a hypnotic rhythm, as a group chant does: hold a chord and ARP 1 plays it in sixteenths in a "
            "syncopated three-three-two pattern, each hit a short, sharp \"ah!\" (a saw and noise through the VOICE "
            "filter, the vowel snapping open from \"oo\" on every hit), voices thrown left and right. MACROS: BRIGHT, "
            "SPACE, POWER (harder, brighter shouts).");
        chant.osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f).osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.8f).osc (0, "UniDetune", 0.12f)
             .osc (0, "UniSpread", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.05f).osc (1, "Volume", 0.15f)
             .osc (1, "Arp", 1.0f)
             .ampEnvelope (2.0f, 140.0f, 0.0f, 80.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.5f)
             .set (ParamIDs::env (0, "Trigger"), (float) synth::mod::EnvelopeTrigger::arpNotes);
        oneShot (chant, 0, { SP16 { 0.0f, 0.0f, 0.3f }, SP16 { 0.03f, 1.0f, 0.0f } });
        voice (chant, 0.0f, 0.3f, 0.5f);
        route (chant, "env1", ParamIDs::voiceVowel, 0.5f);
        {
            auto& p = chant.rig.modulation.arps[0].patterns[0];
            p = {};
            constexpr int hits[] = { 0, 3, 6, 8, 11, 14 };   // 3 + 3 + 2, twice
            for (int k = 0; k < 16; ++k)
                p.blocks[(size_t) k].velocity = 0.7f;
            for (const auto k : hits)
            {
                p.blocks[(size_t) k].on = true;
                p.blocks[(size_t) k].velocity = k == 0 || k == 8 ? 1.0f : 0.8f;
            }
        }
        route (chant, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (chant, "macro3", ParamIDs::voiceSharp, 0.25f);
        route (chant, "macro3", ParamIDs::voiceVowel, 0.15f);
        macros (chant, "BRIGHT", "SPACE", "POWER");
        delay (chant, "1/8 dotted", 0.25f, 0.12f, true, 4.5f);
        reverb (chant, RT::hall, 0.6f, 0.18f);
        space (chant);
        chant.set (ParamIDs::patchLevel, -0.5f);   // LEVEL: set by its peaks: short shouts (measured, not by ear)
        x.add (chant);

        auto festival = x.make ("Festival Bronze", 15, { "Arp", "Gamelan", "Bronze", "Festival" },
            "A festival in bronze: ARP 1 rings the chord you hold out as a fast gamelan figure in sixteenths, leaping "
            "an octave up and back (its pattern's transposes), a few strikes left out so the figure breathes, every "
            "strike on a shimmering bronze pair left to ring over the next. MACROS: BRIGHT, SPACE, POWER (a harder clang).");
        bronze (festival, 0, 0, 0.4f, 8.0f, 1.0f);
        festival.ampEnvelope (1.0f, 1400.0f, 0.0f, 900.0f).set (ParamIDs::ampDecayCurve, -0.3f)
                .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
                .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
                .set (ParamIDs::arp (0, "Gate"), 0.9f);
        {
            auto& p = festival.rig.modulation.arps[0].patterns[0];
            p = {};
            constexpr int leaps[16] = { 0, 12, 0, 0, 12, 0, 12, 24, 0, 12, 0, 0, 12, 24, 12, 0 };
            for (int k = 0; k < 16; ++k)
            {
                auto& block = p.blocks[(size_t) k];
                block.on = k != 3 && k != 11;
                block.transpose = leaps[k];
                block.velocity = k % 4 == 0 ? 1.0f : k % 2 == 0 ? 0.75f : 0.55f;
            }
        }
        route (festival, "macro1", ParamIDs::osc (0, "FmAmount"), 0.05f);
        route (festival, "macro1", ParamIDs::osc (1, "FmAmount"), 0.05f);
        route (festival, "macro3", ParamIDs::osc (0, "FmAmount"), 0.08f);
        route (festival, "macro3", ParamIDs::osc (1, "FmAmount"), 0.08f);
        macros (festival, "BRIGHT", "SPACE", "POWER");
        reverb (festival, RT::hall, 0.75f, 0.22f);
        space (festival);
        festival.set (ParamIDs::patchLevel, -1.3f);   // LEVEL: re-measured as an arp (0.55.0; measured, not by ear)
        x.add (festival);
    }

    // BLOODWAVE: inspired by the soundtrack of a 2012 top-down action game set in a neon-soaked 1989 city: the dark,
    // dirty side of the '80s (driving distorted synths at club tempos, basses pumping under a drum machine, hazy,
    // feverish detuned loops, worn tape, bit-crushed grit, ominous lulls). No title, characters, masks, the city, the
    // phone messages, artists or tracks: our own sounds. MACRO 3 is RAGE on every preset: more drive, crush, wobble.
    void bloodwave (Expansion x)
    {
        using SP17 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;
        using GM = effects::Glitch::Mode;
        const auto oneShot = [] (Builder& b, int env, std::vector<SP17> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers, buses and VOICE
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };
        const auto lofi = [] (Builder& b, effects::LoFi::Type type, float amount, float mix)
        {
            b.set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) type).set (ParamIDs::lofiAmount, amount)
             .set (ParamIDs::lofiMix, mix).set (ParamIDs::lofiNoise, 0.0f);   // no hiss of its own: the presets must go silent
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto blade = x.make ("Neon Blade", 1, { "Lead", "Crush", "Square", "Drive" },
            "A sharp lead that cuts through everything: a square wave and a quieter square an octave up, BIT-crushed "
            "into a hard digital edge, through insert bus A's BANSHEE, a quick glide, dotted-eighth echoes. MACROS: "
            "BRIGHT, SPACE, RAGE (more crush and bite). MOD WHEEL: vibrato.");
        blade.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.55f).osc (0, "Crush", 0.3f).osc (0, "CrushMode", 1.0f)
             .osc (0, "Insert", 1.0f).osc (0, "VibRate", 5.5f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", 12.0f).osc (1, "Fine", 5.0f)
             .osc (1, "Volume", 0.2f).osc (1, "Insert", 1.0f)
             .filter (FilterType::lp24, 4000.0f, 0.2f, 0.2f).filterEnvelope (0.0f, 250.0f, 0.6f, 300.0f)
             .set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (6.0f, 400.0f, 0.85f, 300.0f)
             .mono (15.0f);
        drive (blade, 0, DM::screamer, 4.0f, 5.5f, 0.0f, 0.3f, 0.2f);
        route (blade, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (blade, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (blade, "macro3", ParamIDs::osc (0, "Crush"), 0.35f);
        route (blade, "macro3", ParamIDs::fltDrive, 0.4f);
        macros (blade, "BRIGHT", "SPACE", "RAGE");
        delay (blade, "1/8 dotted", 0.35f, 0.18f, true, 5.0f);
        reverb (blade, RT::plate, 0.5f, 0.12f);
        space (blade);
        blade.set (ParamIDs::patchLevel, 0.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (blade);

        auto fever = x.make ("Fever Lead", 2, { "Lead", "Tape", "Wobble", "Lo-Fi" },
            "A sick, dreamy lead, like a melody on a stretched tape: a saw and a triangle a few cents apart, their pitch "
            "wandering at random (a slow random vibrato and a drifting LFO), a tape-speed wobble on the whole sound "
            "(MOD's VIBRATO), worn by LO-FI's cassette, a hall. MACROS: BRIGHT, SPACE, RAGE (the warble goes seasick). "
            "MOD WHEEL: more warble.");
        fever.osc (0, "Volume", 0.5f).osc (0, "VibWave", (float) synth::VibratoWave::random).osc (0, "VibRate", 1.5f).osc (0, "VibAmount", 12.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Fine", 9.0f).osc (1, "Volume", 0.45f)
             .osc (1, "VibWave", (float) synth::VibratoWave::random).osc (1, "VibRate", 1.2f).osc (1, "VibAmount", 12.0f)
             .filter (FilterType::lp12, 2500.0f, 0.1f, 0.1f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (30.0f, 500.0f, 0.9f, 600.0f)
             .mono (60.0f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.6f)
             .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        chorus (fever, ParamIDs::modIds, MT::vibrato, 0.7f, 0.25f, 1.0f);
        lofi (fever, effects::LoFi::Type::cassette, 4.0f, 0.6f);
        route (fever, "lfo1", ParamIDs::osc (0, "Fine"), 0.04f);
        route (fever, "lfo1", ParamIDs::osc (1, "Fine"), -0.04f);
        route (fever, "modWheel", ParamIDs::modDepth, 0.3f);
        route (fever, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (fever, "macro3", ParamIDs::modDepth, 0.4f);
        route (fever, "macro3", ParamIDs::osc (0, "VibAmount"), 0.2f);
        macros (fever, "BRIGHT", "SPACE", "RAGE");
        delay (fever, "1/4", 0.35f, 0.15f, true, 3.5f);
        reverb (fever, RT::hall, 0.7f, 0.2f);
        space (fever);
        fever.set (ParamIDs::patchLevel, -0.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (fever);

        auto siren = x.make ("Siren Scream", 3, { "Lead", "Rabid", "Pitch", "Dive" },
            "A distorted saw that screams and, when you let go, dives: a three-voice saw unison through insert bus A's "
            "RABID, each note falling an octave as it's released (the PITCH envelope's release part), a quick glide, "
            "a hard shaper before the filter, a plate. MACROS: BRIGHT, SPACE, RAGE (more shaper). MOD WHEEL: a wide "
            "vibrato.");
        siren.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.5f)
             .osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f).osc (0, "VibRate", 6.0f)
             .shaper (0, synth::ShaperModel::hard, 0.1f)
             .filter (FilterType::lp24, 3000.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (6.0f, 600.0f, 0.9f, 700.0f)
             .mono (40.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f)
             .set (ParamIDs::pitchEnvMode, (float) synth::mod::EnvelopeMode::sustain);
        siren.rig.modulation.pitchEnvelope = { { SP17 { 0.0f, 0.5f, 0.0f }, SP17 { 0.05f, 0.5f, 0.5f }, SP17 { 0.7f, 0.0f, 0.0f } }, 1, -1, -1 };
        drive (siren, 0, DM::rabid, 5.0f, 5.0f, 2.0f, 0.3f, 0.3f);
        route (siren, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.3f);
        route (siren, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (siren, "macro3", ParamIDs::shp (0, "Drive"), 0.35f);
        macros (siren, "BRIGHT", "SPACE", "RAGE");
        delay (siren, "1/8", 0.3f, 0.12f, true, 4.5f);
        reverb (siren, RT::plate, 0.6f, 0.15f);
        space (siren);
        siren.set (ParamIDs::patchLevel, 0.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (siren);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto haze = x.make ("Pink Haze", 4, { "Pad", "Unison", "Tape", "Dreamy" },
            "A hazy dream you can't wake up from: a wide seven-voice saw unison, very detuned, softened by the filter "
            "breathing slowly, a chorus (PRISM) and a slow tape wobble (MOD 2's VIBRATO), worn by LO-FI's cassette, a "
            "long hall. MACROS: BRIGHT, SPACE, RAGE (more detune and wear). MOD WHEEL: opens the filter.");
        haze.osc (0, "Volume", 0.5f).osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.85f).osc (0, "UniDetune", 0.25f).osc (0, "UniSpread", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
            .filter (FilterType::lp12, 1800.0f, 0.15f, 0.0f).set (ParamIDs::fltKey, 0.3f)
            .ampEnvelope (900.0f, 1500.0f, 0.95f, 2000.0f)
            .set (ParamIDs::lfo (0, "Rate"), 0.1f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        chorus (haze, ParamIDs::modIds, MT::dimension, 0.3f, 0.5f, 0.5f);
        chorus (haze, ParamIDs::mod2Ids, MT::vibrato, 0.35f, 0.2f, 1.0f);
        lofi (haze, effects::LoFi::Type::cassette, 3.0f, 0.5f);
        route (haze, "lfo1", ParamIDs::fltCutoff, 0.08f);
        route (haze, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (haze, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (haze, "macro3", ParamIDs::osc (0, "UniDetune"), 0.2f);
        route (haze, "macro3", ParamIDs::lofiAmount, 0.3f);
        macros (haze, "BRIGHT", "SPACE", "RAGE");
        reverb (haze, RT::hall, 0.85f, 0.3f);
        eqBand (haze, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (haze);
        haze.set (ParamIDs::patchLevel, -6.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (haze);

        auto staticPad = x.make ("Night Static", 5, { "Pad", "Lo-Fi", "Telephone", "Eerie" },
            "An eerie chord heard down a crackling phone line in the middle of the night: soft pulse waves and a "
            "sine an octave up, a slow tremolo (MOD's RAZOR), crackle (filtered noise that flickers at random), all "
            "squeezed through LO-FI's TELEPHONE, a dark room. MACROS: BRIGHT, SPACE, RAGE (more crackle and a harder "
            "line). MOD WHEEL: more crackle.");
        staticPad.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.5f)
                 .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.7f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.25f)
                 .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.5f).osc (2, "NoiseColour", 0.05f).osc (2, "Volume", 0.0f)
                 .ampEnvelope (800.0f, 1500.0f, 0.95f, 1800.0f)
                 .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sampleHold).set (ParamIDs::lfo (0, "Rate"), 11.0f)
                 .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        chorus (staticPad, ParamIDs::modIds, MT::tremolo, 0.25f, 0.4f, 1.0f);
        lofi (staticPad, effects::LoFi::Type::telephone, 5.0f, 0.85f);
        route (staticPad, "lfo1", ParamIDs::osc (2, "Volume"), 0.06f, 3.0f);
        route (staticPad, "modWheel", ParamIDs::osc (2, "Volume"), 0.05f);
        route (staticPad, "macro1", ParamIDs::lofiMix, -0.3f);
        route (staticPad, "macro3", ParamIDs::osc (2, "Volume"), 0.06f);
        route (staticPad, "macro3", ParamIDs::lofiAmount, 0.3f);
        macros (staticPad, "BRIGHT", "SPACE", "RAGE");
        reverb (staticPad, RT::room, 0.6f, 0.2f);
        space (staticPad);
        staticPad.set (ParamIDs::patchLevel, -1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (staticPad);

        auto comedown = x.make ("Comedown", 6, { "Pad", "Drone", "Pitch", "Dark" },
            "The slow, dark drone after the chaos: two saws an octave apart and a sine below them, the filter closing as "
            "it holds and every note sagging a little flat over eight seconds (the PITCH envelope), a diode shaper "
            "underneath, a huge dark hall. MACROS: BRIGHT, SPACE, RAGE (the shaper growls). MOD WHEEL: opens the filter.");
        comedown.osc (0, "Volume", 0.45f).osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.08f)
                .osc (1, "On", 1.0f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.35f)
                .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -24.0f).osc (2, "Volume", 0.3f).osc (2, "PitchEnv", 0.0f)
                .shaper (0, synth::ShaperModel::diode, 0.05f)
                .filter (FilterType::lp24, 1400.0f, 0.15f, 0.0f)
                .ampEnvelope (1200.0f, 2000.0f, 1.0f, 3000.0f)
                .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 1.0f);
        comedown.rig.modulation.pitchEnvelope = { { SP17 { 0.0f, 0.5f, 0.3f }, SP17 { 8.0f, 0.25f, 0.0f } }, -1, -1, -1 };
        oneShot (comedown, 0, { SP17 { 0.0f, 1.0f, 0.4f }, SP17 { 8.0f, 0.0f, 0.0f } });
        route (comedown, "env1", ParamIDs::fltCutoff, 0.2f);
        route (comedown, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (comedown, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (comedown, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (comedown, "BRIGHT", "SPACE", "RAGE");
        reverb (comedown, RT::hall, 0.95f, 0.3f, 50.0f, 3.5f);
        space (comedown);
        comedown.set (ParamIDs::patchLevel, -12.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (comedown);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto pump = x.make ("Pump Bass", 7, { "Bass", "Step Seq", "Pump", "Club" },
            "The club's breathing bass: a saw and a clean sine whose volume ducks on every beat and swells back up (the "
            "STEP SEQUENCER drawing a pump in sixteenths, as a bass ducks under a kick), a tube shaper, the filter "
            "opening as it swells. Play eighths or hold notes. MACROS: BRIGHT, SPACE, RAGE (more tube and filter). MOD "
            "WHEEL: opens the filter.");
        pump.osc (0, "Volume", 0.55f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
            .shaper (0, synth::ShaperModel::tube, 0.15f)
            .filter (FilterType::lp24, 700.0f, 0.2f, 0.2f).filterEnvelope (0.0f, 200.0f, 0.5f, 150.0f)
            .ampEnvelope (3.0f, 400.0f, 1.0f, 150.0f)
            .mono (0.0f)
            .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.85f).set (ParamIDs::seqGate, 1.0f)
            .set (ParamIDs::seqContour, 0.5f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 4.0f);
        clean (pump, 1);
        for (int row = 0; row < 2; ++row)
        {
            constexpr float levels[] = { 0.1f, 0.5f, 0.85f, 1.0f };
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = pump.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = true;
                step.level = levels[k % 4];
            }
        }
        route (pump, "stepSeq", ParamIDs::fltCutoff, 0.1f);
        route (pump, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (pump, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (pump, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        route (pump, "macro3", ParamIDs::fltEnv, 0.2f);
        macros (pump, "BRIGHT", "SPACE", "RAGE");
        reverb (pump, RT::room, 0.4f, 0.08f);
        space (pump);
        pump.set (ParamIDs::patchLevel, -2.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pump);

        auto grinder = x.make ("Darkgrind Bass", 8, { "Bass", "Abyss", "LFO", "Darksynth" },
            "A filthy darksynth bass: a saw and a narrow pulse through insert bus A's ABYSS, the filter chewing in "
            "sixteenths with your song (a synced LFO), a clean sine under it kept off the drive. MACROS: BRIGHT, SPACE, "
            "RAGE (the filter's own drive). MOD WHEEL: a deeper chew.");
        grinder.osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.2f).osc (1, "Fine", 7.0f).osc (1, "Volume", 0.35f).osc (1, "Insert", 1.0f)
               .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.45f).osc (2, "PhaseRand", 0.0f)
               .filter (FilterType::lp24, 600.0f, 0.3f, 0.2f).filterEnvelope (0.0f, 200.0f, 0.4f, 150.0f)
               .ampEnvelope (6.0f, 500.0f, 1.0f, 200.0f)
               .mono (20.0f)
               .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight).set (ParamIDs::lfo (0, "Division"), 7.0f)   // 1/16
               .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sawDown)
               .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        clean (grinder, 2);
        drive (grinder, 0, DM::abyss, 5.0f, 5.0f, -3.0f, 0.05f, 0.1f);
        route (grinder, "lfo1", ParamIDs::fltCutoff, 0.08f);
        route (grinder, "modWheel", ParamIDs::fltCutoff, 0.1f);
        route (grinder, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (grinder, "macro3", ParamIDs::fltDrive, 0.5f);
        macros (grinder, "BRIGHT", "SPACE", "RAGE");
        reverb (grinder, RT::room, 0.4f, 0.06f);
        space (grinder);
        grinder.set (ParamIDs::patchLevel, -0.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (grinder);

        auto thump = x.make ("Sub Thump", 9, { "Bass", "Sub", "Pitch", "808" },
            "A round sub that drops into every hit: a sine that falls an octave into its note in a few tens of "
            "milliseconds (the PITCH envelope: the classic drum-machine bass), a soft shaper for weight, a quick fade. "
            "MACROS: BRIGHT (more shaper), SPACE, RAGE (a longer drop and harder shaper).");
        thump.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
             .shaper (0, synth::ShaperModel::soft, 0.15f)
             .ampEnvelope (1.0f, 900.0f, 0.4f, 250.0f).set (ParamIDs::ampDecayCurve, -0.3f)
             .mono (0.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f);
        thump.rig.modulation.pitchEnvelope = { { SP17 { 0.0f, 1.0f, -0.6f }, SP17 { 0.05f, 0.5f, 0.0f } }, -1, -1, -1 };
        thump.osc (0, "Filter", 0.0f);
        route (thump, "macro1", ParamIDs::shp (0, "Drive"), 0.2f);
        route (thump, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        route (thump, "macro3", ParamIDs::pitchEnvRange, 0.25f);
        macros (thump, "BRIGHT", "SPACE", "RAGE");
        reverb (thump, RT::room, 0.3f, 0.05f);
        space (thump);
        thump.set (ParamIDs::patchLevel, -4.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (thump);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto woozy = x.make ("Woozy Keys", 10, { "Keys", "FM", "Electric Piano", "Wobble" },
            "A druggy electric piano: a sine with FM from a sine at the same pitch (the bark at the strike, louder the "
            "harder you play), a tine an octave up, its pitch swimming at random (a slow random vibrato) and a "
            "chorus. MACROS: BRIGHT, SPACE, RAGE (a deeper swim). MOD WHEEL: more swim.");
        woozy.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.06f)
             .osc (0, "VibWave", (float) synth::VibratoWave::random).osc (0, "VibRate", 1.0f).osc (0, "VibAmount", 8.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f).osc (1, "Fine", 3.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.1f)
             .osc (2, "VibWave", (float) synth::VibratoWave::random).osc (2, "VibRate", 1.0f).osc (2, "VibAmount", 8.0f)
             .ampEnvelope (2.0f, 2500.0f, 0.25f, 500.0f).set (ParamIDs::ampVel, 0.6f);
        oneShot (woozy, 0, { SP17 { 0.0f, 1.0f, -0.5f }, SP17 { 0.8f, 0.15f, 0.0f } });
        route (woozy, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (woozy, "velocity", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (woozy, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        route (woozy, "macro1", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (woozy, "macro3", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (woozy, "macro3", ParamIDs::osc (2, "VibAmount"), 0.2f);
        macros (woozy, "BRIGHT", "SPACE", "RAGE");
        chorus (woozy, ParamIDs::modIds, MT::chorus, 0.5f, 0.4f, 0.4f);
        reverb (woozy, RT::plate, 0.5f, 0.18f);
        space (woozy);
        woozy.set (ParamIDs::patchLevel, -10.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (woozy);

        auto vhs = x.make ("VHS Bells", 11, { "Keys", "FM", "Bell", "Lo-Fi" },
            "Bells off a worn-out tape: a sine with FM from a sine at 3.5 times its pitch (a glassy bell), a touch of "
            "BIT crush, a tape-speed wobble (MOD's VIBRATO) and LO-FI's cassette, ringing into ping-pong echoes. "
            "MACROS: BRIGHT, SPACE, RAGE (more crush and wear). MOD WHEEL: more wobble.");
        vhs.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f).osc (0, "Crush", 0.1f).osc (0, "CrushMode", 1.0f)
           .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.08f)
           .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 22.0f).osc (1, "Fine", -31.0f).osc (1, "Volume", 1.0f)
           .osc (1, "PhaseRand", 0.0f)
           .ampEnvelope (1.0f, 2200.0f, 0.0f, 1200.0f).set (ParamIDs::ampDecayCurve, -0.4f).set (ParamIDs::ampVel, 0.5f);
        oneShot (vhs, 0, { SP17 { 0.0f, 1.0f, -0.5f }, SP17 { 1.2f, 0.2f, 0.0f } });
        chorus (vhs, ParamIDs::modIds, MT::vibrato, 0.6f, 0.15f, 1.0f);
        lofi (vhs, effects::LoFi::Type::cassette, 4.0f, 0.6f);
        route (vhs, "env1", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (vhs, "modWheel", ParamIDs::modDepth, 0.3f);
        route (vhs, "macro1", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (vhs, "macro3", ParamIDs::osc (0, "Crush"), 0.3f);
        route (vhs, "macro3", ParamIDs::lofiAmount, 0.3f);
        macros (vhs, "BRIGHT", "SPACE", "RAGE");
        delay (vhs, "1/8 dotted", 0.4f, 0.2f, true, 4.0f);
        reverb (vhs, RT::hall, 0.6f, 0.18f);
        space (vhs);
        vhs.set (ParamIDs::patchLevel, -2.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (vhs);

        auto stabs = x.make ("Club Stabs", 12, { "Keys", "Chorder", "Stab", "House" },
            "Short club chord stabs: one key plays a whole minor-seventh chord (the chorder: +3, +7, +10), saws and a "
            "pulse plucked short by a snappy filter, a room and a quarter-note echo. MACROS: BRIGHT, SPACE, RAGE (a "
            "harder pluck through a tube shaper). MOD WHEEL: a longer pluck.");
        stabs.osc (0, "Volume", 0.4f).osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.6f)
             .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 3.0f).osc (0, "Ch1Vol", 0.8f)
             .osc (0, "Ch2On", 1.0f).osc (0, "Ch2Coarse", 7.0f).osc (0, "Ch2Vol", 0.8f)
             .osc (0, "Ch3On", 1.0f).osc (0, "Ch3Coarse", 10.0f).osc (0, "Ch3Vol", 0.7f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.4f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
             .shaper (0, synth::ShaperModel::tube, 0.05f)
             .filter (FilterType::lp24, 600.0f, 0.25f, 0.5f).filterEnvelope (0.0f, 180.0f, 0.0f, 150.0f)
             .ampEnvelope (2.0f, 300.0f, 0.0f, 150.0f);
        route (stabs, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (stabs, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (stabs, "macro3", ParamIDs::fltEnv, 0.2f);
        route (stabs, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        macros (stabs, "BRIGHT", "SPACE", "RAGE");
        delay (stabs, "1/4", 0.25f, 0.12f, true, 4.0f);
        reverb (stabs, RT::room, 0.5f, 0.15f);
        space (stabs);
        stabs.set (ParamIDs::patchLevel, -1.5f);   // LEVEL: set so a three-key chord (twelve notes) doesn't clip (measured, not by ear)
        x.add (stabs);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto rampage = x.make ("Rampage Arp", 13, { "Arp", "Drive", "Glitch", "Fast" },
            "A fast, aggressive arp: ARP 1 tears up and down the chord you hold over two octaves in sixteenths with your "
            "song, a saw and a square plucked by a resonant filter through insert bus A's BANSHEE, the odd stutter "
            "from GLITCH. MACROS: BRIGHT, SPACE, RAGE (resonance and filter drive).");
        rampage.osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Fine", -6.0f).osc (1, "Volume", 0.35f)
               .osc (1, "Arp", 1.0f).osc (1, "Insert", 1.0f)
               .filter (FilterType::lp24, 1500.0f, 0.4f, 0.4f).filterEnvelope (0.0f, 150.0f, 0.15f, 100.0f)
               .ampEnvelope (2.0f, 200.0f, 0.4f, 100.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
               .set (ParamIDs::arp (0, "Gate"), 0.6f);
        drive (rampage, 0, DM::screamer, 5.0f, 5.0f, 0.0f, 0.2f, 0.2f);
        glitch (rampage, 0, GM::sequence, "1/16", 2.0f, 3.0f, 0.3f);
        route (rampage, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (rampage, "macro3", ParamIDs::fltRes, 0.25f);
        route (rampage, "macro3", ParamIDs::fltDrive, 0.4f);
        macros (rampage, "BRIGHT", "SPACE", "RAGE");
        delay (rampage, "1/8 dotted", 0.25f, 0.1f, true, 5.0f);
        reverb (rampage, RT::plate, 0.5f, 0.1f);
        space (rampage);
        rampage.set (ParamIDs::patchLevel, 2.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (rampage);

        auto hypno = x.make ("Hypno Loop", 14, { "Arp", "Delay", "Flanger", "Hypnotic" },
            "A hypnotic loop that spins around you: ARP 1 circles the chord you hold in eighths, a few steps fluttering "
            "(ratchets), two triangles pulled apart in pitch (one a little flat, one a little sharp) and a soft square, "
            "long ping-pong echoes in dotted eighths, a slow flanger (AEON) sweeping through it all. MACROS: BRIGHT, "
            "SPACE, RAGE (the pair pulled further out of tune).");
        hypno.osc (0, "Wave", triangle).osc (0, "Fine", -12.0f).osc (0, "Pan", -0.4f).osc (0, "Volume", 0.45f).osc (0, "Arp", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Fine", 12.0f).osc (1, "Pan", 0.4f).osc (1, "Volume", 0.45f).osc (1, "Arp", 1.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", pulse).osc (2, "Width", 0.5f).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.12f)
             .osc (2, "Arp", 1.0f)
             .ampEnvelope (2.0f, 400.0f, 0.3f, 250.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::asPlayed)
             .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.6f);
        {
            auto& p = hypno.rig.modulation.arps[0].patterns[0];
            p = synth::arp::Patterns::makeDefault().patterns[0];
            p.blocks[3].ratchet = 2;
            p.blocks[11].ratchet = 3;
            p.blocks[14].ratchet = 2;
        }
        chorus (hypno, ParamIDs::modIds, MT::flanger, 0.07f, 0.5f, 0.35f);
        route (hypno, "macro1", ParamIDs::osc (2, "Volume"), 0.08f);
        route (hypno, "macro3", ParamIDs::osc (0, "Fine"), -0.1f);
        route (hypno, "macro3", ParamIDs::osc (1, "Fine"), 0.1f);
        macros (hypno, "BRIGHT", "SPACE", "RAGE");
        delay (hypno, "1/8 dotted", 0.55f, 0.28f, true, 4.5f);
        reverb (hypno, RT::hall, 0.6f, 0.18f);
        space (hypno);
        hypno.set (ParamIDs::patchLevel, -2.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hypno);

        auto machine = x.make ("Offbeat Run", 15, { "Arp", "Bass", "Offbeat", "Club" },
            "A club bassline: the chord you hold drives an offbeat bass two octaves down (ARP 1 in eighths, a saw plucked "
            "by the filter), in time with your song. MACROS: BRIGHT, SPACE, RAGE (a harder, drivier bass).");
        machine.osc (0, "Transpose", -24.0f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -24.0f).osc (1, "Volume", 0.4f).osc (1, "Arp", 1.0f)
               .shaper (0, synth::ShaperModel::tube, 0.1f)
               .filter (FilterType::lp24, 500.0f, 0.3f, 0.35f).filterEnvelope (0.0f, 150.0f, 0.1f, 100.0f)
               .ampEnvelope (2.0f, 250.0f, 0.3f, 100.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
               .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
               .set (ParamIDs::arp (0, "Gate"), 0.6f);
        clean (machine, 1);
        machine.osc (1, "Arp", 1.0f);
        {
            auto& p = machine.rig.modulation.arps[0].patterns[0];
            p = {};
            for (int k = 1; k < 16; k += 2)   // the offbeat eighths
                p.blocks[(size_t) k].on = true;
        }
        route (machine, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (machine, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        route (machine, "macro3", ParamIDs::fltEnv, 0.2f);
        macros (machine, "BRIGHT", "SPACE", "RAGE");
        reverb (machine, RT::room, 0.5f, 0.12f);
        space (machine);
        machine.set (ParamIDs::patchLevel, 2.7f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        x.add (machine);
    }

    // DUSTFALL: inspired by the score of a 2017 science-fiction film, a sequel set thirty years on: colder, bigger and
    // more brutal than its forerunner's (NEON RAIN): colossal low brass blasts, crushing sub drones, distorted walls,
    // orange dust and grey snow over empty cities, and in between, fragile, lonely beauty. No title, characters, places,
    // terms or music from the film: our own sounds. MACRO 3 is WEIGHT on every preset: more low end, distortion, size.
    void dustfall (Expansion x)
    {
        using SP18 = synth::mod::ShapePoint;
        using DM = effects::Drive::Model;
        const auto oneShot = [] (Builder& b, int env, std::vector<SP18> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto voice = [] (Builder& b, float vowel, float size, float sharp)
        {
            b.set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceVowel, vowel).set (ParamIDs::voiceSize, size).set (ParamIDs::voiceSharp, sharp);
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers, buses and VOICE
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };
        // WEIGHT's own sub: a sine an octave down, kept clean, silent until MACRO 3 brings it in.
        const auto weightSub = [&clean] (Builder& b, int o, float amount)
        {
            b.osc (o, "On", 1.0f).osc (o, "Wave", sine).osc (o, "Transpose", -12.0f).osc (o, "Volume", 0.0f).osc (o, "PhaseRand", 0.0f);
            clean (b, o);
            route (b, "macro3", ParamIDs::osc (o, "Volume"), amount);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto hologram = x.make ("Hologram Voice", 1, { "Lead", "Voice", "Flicker", "Glass" },
            "A translucent, ghostly lead, as if it might flicker out: a triangle and a narrow pulse sung by the VOICE "
            "filter as a high, glassy \"ee\" opening to \"ah\" as you hold it, its level flickering faintly at random, a "
            "shimmering chorus, echoes and a digital space. MACROS: BRIGHT, SPACE, WEIGHT (a deeper voice and a sub "
            "under it). MOD WHEEL: more flicker.");
        hologram.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "VibAmount", 6.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 500.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.15f).osc (1, "Volume", 0.35f).osc (1, "Fine", 6.0f)
                .ampEnvelope (60.0f, 600.0f, 0.9f, 900.0f)
                .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sampleHold).set (ParamIDs::lfo (0, "Rate"), 14.0f);
        voice (hologram, 1.0f, 0.85f, 0.65f);
        oneShot (hologram, 0, { SP18 { 0.0f, 0.0f, 0.3f }, SP18 { 1.2f, 1.0f, 0.0f } });
        route (hologram, "env1", ParamIDs::voiceVowel, -0.5f);
        route (hologram, "lfo1", ParamIDs::osc (0, "Volume"), 0.04f, 8.0f);
        route (hologram, "lfo1", ParamIDs::osc (1, "Volume"), 0.03f, 8.0f);
        route (hologram, "modWheel", ParamIDs::osc (1, "Volume"), 0.0f);
        route (hologram, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (hologram, "macro3", ParamIDs::voiceSize, -0.5f);
        weightSub (hologram, 2, 0.25f);
        macros (hologram, "BRIGHT", "SPACE", "WEIGHT");
        chorus (hologram, ParamIDs::modIds, MT::dimension, 0.4f, 0.5f, 0.45f);
        delay (hologram, "1/4 dotted", 0.4f, 0.2f, true, 5.5f);
        reverb (hologram, RT::digital, 0.8f, 0.28f, 40.0f, 5.0f);
        space (hologram);
        hologram.set (ParamIDs::patchLevel, -3.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hologram);

        auto blast = x.make ("Warning Blast", 2, { "Lead", "Brass", "Drive", "Pitch" },
            "An enormous distorted brass blare: a three-voice saw unison and a pulse an octave down, lurching up into "
            "each note from five semitones under (the PITCH envelope), the filter flaring open as it speaks, a hard shaper "
            "and insert bus A's RABID, a vast hall. MACROS: BRIGHT, SPACE, WEIGHT (more shaper and a sub). MOD WHEEL: "
            "a slow, wide vibrato.");
        blast.osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.5f)
             .osc (0, "Volume", 0.5f).osc (0, "Insert", 1.0f).osc (0, "VibRate", 4.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.45f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.4f)
             .osc (1, "Insert", 1.0f)
             .shaper (0, synth::ShaperModel::hard, 0.12f)
             .filter (FilterType::lp24, 700.0f, 0.2f, 0.45f).filterEnvelope (80.0f, 900.0f, 0.5f, 600.0f).set (ParamIDs::fltKey, 0.4f)
             .ampEnvelope (6.0f, 800.0f, 0.9f, 900.0f)
             .mono (40.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 5.0f);
        blast.rig.modulation.pitchEnvelope = { { SP18 { 0.0f, 0.0f, -0.5f }, SP18 { 0.12f, 0.5f, 0.0f } }, -1, -1, -1 };
        drive (blast, 0, DM::rabid, 4.0f, 4.5f, 0.0f, 0.3f, 0.4f);
        route (blast, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.25f);
        route (blast, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (blast, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        weightSub (blast, 2, 0.3f);
        macros (blast, "BRIGHT", "SPACE", "WEIGHT");
        delay (blast, "1/4", 0.3f, 0.12f, true, 4.0f);
        reverb (blast, RT::hall, 0.95f, 0.3f, 40.0f, 3.5f);
        space (blast);
        blast.set (ParamIDs::patchLevel, 1.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (blast);

        auto thread = x.make ("Memory Thread", 3, { "Lead", "Fragile", "Wobble", "Echo" },
            "A fragile, thin melody, like something half remembered: a sine and a quiet triangle an octave up, its pitch "
            "wavering slightly at random (as an old recording does), a soft glide, long, lonely echoes and a cathedral. "
            "MACROS: BRIGHT, SPACE, WEIGHT (an octave down underneath). MOD WHEEL: more wavering.");
        thread.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
              .osc (0, "VibWave", (float) synth::VibratoWave::random).osc (0, "VibRate", 0.9f).osc (0, "VibAmount", 9.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
              .osc (1, "VibWave", (float) synth::VibratoWave::random).osc (1, "VibRate", 0.9f).osc (1, "VibAmount", 9.0f)
              .ampEnvelope (40.0f, 600.0f, 0.85f, 1200.0f)
              .mono (70.0f);
        route (thread, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (thread, "modWheel", ParamIDs::osc (1, "VibAmount"), 0.2f);
        route (thread, "macro1", ParamIDs::osc (1, "Volume"), 0.15f);
        weightSub (thread, 2, 0.3f);
        macros (thread, "BRIGHT", "SPACE", "WEIGHT");
        delay (thread, "1/2", 0.42f, 0.3f, true, 3.0f);
        thread.set (ParamIDs::delayToReverb, 0.25f);
        reverb (thread, RT::cathedral, 0.8f, 0.3f, 50.0f, 3.5f);
        space (thread);
        thread.set (ParamIDs::patchLevel, -5.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (thread);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto haze = x.make ("Orange Haze", 4, { "Pad", "Wavetable", "Dust", "Warm" },
            "A warm, dusty haze hanging in the air: the Analog Morph table in a wide six-voice unison, its frame drifting "
            "slowly (a slow LFO on INDEX), a warm filter, worn by LO-FI's cassette like dust in the light, a chorus and a "
            "long hall. MACROS: BRIGHT, SPACE, WEIGHT (the filter's grit and a sub). MOD WHEEL: opens the filter.");
        haze.osc (0, "Wave", wavetable).osc (0, "Table", table ("Analog Morph")).osc (0, "WtIndex", 0.4f).osc (0, "Volume", 0.55f)
            .osc (0, "UniVoices", 6.0f).osc (0, "UniMix", 0.8f).osc (0, "UniDetune", 0.16f).osc (0, "UniSpread", 0.95f)
            .filter (FilterType::lp12, 1500.0f, 0.15f, 0.0f).set (ParamIDs::fltKey, 0.3f)
            .ampEnvelope (1200.0f, 2000.0f, 1.0f, 2500.0f)
            .set (ParamIDs::lfo (0, "Rate"), 0.07f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
            .set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
            .set (ParamIDs::lofiAmount, 2.5f).set (ParamIDs::lofiMix, 0.4f).set (ParamIDs::lofiNoise, 0.0f);
        route (haze, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.15f);
        route (haze, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (haze, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (haze, "macro3", ParamIDs::fltDrive, 0.5f);
        weightSub (haze, 1, 0.25f);
        macros (haze, "BRIGHT", "SPACE", "WEIGHT");
        chorus (haze, ParamIDs::modIds, MT::dimension, 0.25f, 0.5f, 0.45f);
        reverb (haze, RT::hall, 0.9f, 0.3f, 40.0f, 4.0f);
        eqBand (haze, 0, eq::Shape::lowCut, 60.0f, 0.0f);
        space (haze);
        haze.set (ParamIDs::patchLevel, -3.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (haze);

        auto snow = x.make ("Grey Snowfall", 5, { "Pad", "FFT", "Noise", "Cold" },
            "A cold, slow pad with snow falling through it: a saw edited in the FFT editor to its high harmonics only "
            "(the 3rd to the 16th, the fundamental and 2nd kept faint: thin and glassy), in a slow unison, and flakes of "
            "bright noise flickering at random, a vast cathedral, its tone dark. MACROS: BRIGHT, SPACE, WEIGHT (a sub and "
            "a bigger space). MOD WHEEL: more snow.");
        snow.rig.spectra[0].gainsDb[0] = -14.0f;
        snow.rig.spectra[0].gainsDb[1] = -10.0f;
        for (int band = 16; band < synth::SpectrumEdit::bands; ++band)
            snow.rig.spectra[0].gainsDb[(size_t) band] = synth::SpectrumEdit::minDb;
        snow.osc (0, "Volume", 0.6f).osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.9f)
            .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.6f).osc (1, "NoiseColour", 0.0f).osc (1, "NoiseWidth", 2.0f)
            .osc (1, "Volume", 0.0f)
            .ampEnvelope (1500.0f, 2000.0f, 1.0f, 3000.0f)
            .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sampleHold).set (ParamIDs::lfo (0, "Rate"), 9.0f)
            .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (snow, "lfo1", ParamIDs::osc (1, "Volume"), 0.05f, 20.0f);
        route (snow, "modWheel", ParamIDs::osc (1, "Volume"), 0.05f);
        route (snow, "macro1", ParamIDs::osc (0, "FftLp"), 0.15f);
        route (snow, "macro3", ParamIDs::reverbSize, 0.15f);
        weightSub (snow, 2, 0.25f);
        macros (snow, "BRIGHT", "SPACE", "WEIGHT");
        reverb (snow, RT::cathedral, 0.8f, 0.35f, 60.0f, 2.5f);
        eqBand (snow, 0, eq::Shape::lowCut, 150.0f, 0.0f);
        space (snow);
        snow.set (ParamIDs::patchLevel, -4.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (snow);

        auto monolith = x.make ("Monolith", 6, { "Pad", "Drone", "Shaper", "Huge" },
            "A vast, low, distorted drone with the weight of a building: saws an octave and two octaves down and a sine "
            "under them, through a diode shaper, a low filter breathing very slowly, a huge dark hall. Play low. MACROS: "
            "BRIGHT, SPACE, WEIGHT (more distortion). MOD WHEEL: opens the filter.");
        monolith.osc (0, "Transpose", -12.0f).osc (0, "Volume", 0.5f).osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.07f)
                .osc (1, "On", 1.0f).osc (1, "Transpose", -24.0f).osc (1, "Volume", 0.35f)
                .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", -12.0f).osc (2, "Volume", 0.35f).osc (2, "PhaseRand", 0.0f)
                .shaper (0, synth::ShaperModel::diode, 0.2f)
                .filter (FilterType::lp24, 450.0f, 0.2f, 0.0f)
                .ampEnvelope (1500.0f, 2000.0f, 1.0f, 3500.0f)
                .set (ParamIDs::lfo (0, "Rate"), 0.05f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        clean (monolith, 2);
        route (monolith, "lfo1", ParamIDs::fltCutoff, 0.08f);
        route (monolith, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (monolith, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (monolith, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        macros (monolith, "BRIGHT", "SPACE", "WEIGHT");
        reverb (monolith, RT::hall, 0.95f, 0.3f, 30.0f, 3.0f);
        space (monolith);
        monolith.set (ParamIDs::patchLevel, -15.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (monolith);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto collapse = x.make ("Sub Collapse", 7, { "Bass", "Sub", "Pitch", "Shaper" },
            "A crushing sub that caves in on every hit: a sine and a dark saw whose pitch sags two semitones under the "
            "note over the first half second (the PITCH envelope), a soft shaper for weight, a clean sine under it. "
            "MACROS: BRIGHT, SPACE, WEIGHT (a deeper sag and more shaper). MOD WHEEL: opens the filter.");
        collapse.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
                .osc (1, "On", 1.0f).osc (1, "Volume", 0.35f)
                .shaper (0, synth::ShaperModel::soft, 0.25f)
                .filter (FilterType::lp24, 400.0f, 0.15f, 0.25f).filterEnvelope (0.0f, 300.0f, 0.3f, 300.0f)
                .ampEnvelope (3.0f, 1500.0f, 0.6f, 500.0f)
                .mono (0.0f)
                .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 4.0f);
        collapse.rig.modulation.pitchEnvelope = { { SP18 { 0.0f, 0.5f, 0.4f }, SP18 { 0.5f, 0.25f, 0.0f } }, -1, -1, -1 };
        route (collapse, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (collapse, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (collapse, "macro3", ParamIDs::pitchEnvRange, 0.1f);
        route (collapse, "macro3", ParamIDs::shp (0, "Drive"), 0.3f);
        macros (collapse, "BRIGHT", "SPACE", "WEIGHT");
        reverb (collapse, RT::room, 0.4f, 0.06f);
        space (collapse);
        collapse.set (ParamIDs::patchLevel, -6.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (collapse);

        auto throb = x.make ("Engine Throb", 8, { "Bass", "AM", "Machine", "Pulse" },
            "A dark bass that throbs like a huge machine idling: a saw whose level pulses at an eighth of its own pitch "
            "(AM from a sine: low notes throb slowly, higher ones faster, as an engine's revs do), a pulse an octave up, a "
            "low filter, a clean sine under it. MACROS: BRIGHT, SPACE, WEIGHT (a harder throb and the filter's grit). "
            "MOD WHEEL: opens the filter.");
        throb.osc (0, "Volume", 0.55f).osc (0, "AmAmount", 0.55f).osc (0, "AmShape", 0.0f).osc (0, "ModRate", 0.5f + std::log2 (0.125f) / 8.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.15f)
             .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.45f).osc (2, "PhaseRand", 0.0f)
             .filter (FilterType::lp24, 500.0f, 0.25f, 0.0f)
             .ampEnvelope (20.0f, 800.0f, 1.0f, 400.0f)
             .mono (30.0f);
        clean (throb, 2);
        route (throb, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (throb, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (throb, "macro3", ParamIDs::osc (0, "AmAmount"), 0.3f);
        route (throb, "macro3", ParamIDs::fltDrive, 0.4f);
        macros (throb, "BRIGHT", "SPACE", "WEIGHT");
        reverb (throb, RT::room, 0.4f, 0.06f);
        space (throb);
        throb.set (ParamIDs::patchLevel, -3.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (throb);

        auto iron = x.make ("Iron Wall", 9, { "Bass", "Banshee", "Fuzz", "Wall" },
            "A fuzzed, saturated wall of bass: two saws a few cents apart and a pulse through insert bus A's BANSHEE and a "
            "hard shaper, a low filter with its own drive, a clean sine under it all. MACROS: BRIGHT, SPACE, WEIGHT (the "
            "filter driven harder). MOD WHEEL: opens the filter.");
        iron.osc (0, "Volume", 0.45f).osc (0, "Insert", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Fine", 9.0f).osc (1, "Volume", 0.4f).osc (1, "Insert", 1.0f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.45f).osc (2, "PhaseRand", 0.0f)
            .shaper (0, synth::ShaperModel::hard, 0.15f)
            .filter (FilterType::lp24, 700.0f, 0.2f, 0.15f).filterEnvelope (0.0f, 300.0f, 0.6f, 200.0f)
            .set (ParamIDs::fltDrive, 0.3f)
            .ampEnvelope (6.0f, 600.0f, 1.0f, 250.0f)
            .mono (20.0f);
        clean (iron, 2);
        drive (iron, 0, DM::screamer, 5.0f, 4.0f, 0.0f, 0.05f, 0.1f);
        route (iron, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (iron, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (iron, "macro3", ParamIDs::fltDrive, 0.5f);
        macros (iron, "BRIGHT", "SPACE", "WEIGHT");
        reverb (iron, RT::room, 0.4f, 0.05f);
        space (iron);
        iron.set (ParamIDs::patchLevel, -4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (iron);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto ghost = x.make ("Ghost Keys", 10, { "Keys", "FM", "Flicker", "Glass" },
            "A shimmering, glassy electric piano that seems only half there: a sine with FM from a sine at the same pitch "
            "(bright at the strike, louder the harder you play), a high glassy sine two octaves up, the whole sound fading "
            "in and out a little at random, a chorus and a digital space. MACROS: BRIGHT, SPACE, WEIGHT (a sub under it).");
        ghost.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.05f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 1.0f).osc (1, "Fine", 4.0f).osc (1, "PhaseRand", 0.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Transpose", 24.0f).osc (2, "Volume", 0.06f)
             .ampEnvelope (2.0f, 2800.0f, 0.2f, 800.0f).set (ParamIDs::ampVel, 0.6f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 1.5f)
             .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        oneShot (ghost, 0, { SP18 { 0.0f, 1.0f, -0.5f }, SP18 { 0.9f, 0.15f, 0.0f } });
        route (ghost, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (ghost, "velocity", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (ghost, "lfo1", ParamIDs::ampVolume, 0.05f);
        route (ghost, "macro1", ParamIDs::osc (2, "Volume"), 0.08f);
        weightSub (ghost, 3, 0.2f);
        macros (ghost, "BRIGHT", "SPACE", "WEIGHT");
        chorus (ghost, ParamIDs::modIds, MT::chorus, 0.4f, 0.4f, 0.4f);
        reverb (ghost, RT::digital, 0.7f, 0.25f, 30.0f, 5.0f);
        space (ghost);
        ghost.set (ParamIDs::patchLevel, -10.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ghost);

        auto distantPiano = x.make ("Distant Piano", 11, { "Keys", "Piano", "Lonely", "Far" },
            "A worn, lonely piano heard from far away: NEGATIVE SPACE's soft felt piano, its top taken off (as distance "
            "does), slow echoes, a long dark hall. MACROS: BRIGHT, SPACE, WEIGHT (a bigger, darker room). The MOD WHEEL "
            "adds the felt's noise.");
        softPiano (distantPiano);
        route (distantPiano, "macro3", ParamIDs::reverbSize, 0.15f);
        macros (distantPiano, "BRIGHT", "SPACE", "WEIGHT");
        delay (distantPiano, "1/2", 0.4f, 0.15f, true, 2.5f);
        reverb (distantPiano, RT::hall, 0.85f, 0.35f, 60.0f, 3.0f);
        eqBand (distantPiano, 0, eq::Shape::highCut, 3500.0f, 0.0f);
        space (distantPiano);
        distantPiano.set (ParamIDs::patchLevel, -10.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (distantPiano);

        auto bells = x.make ("Cold Bells", 12, { "Keys", "Additive", "Bell", "Icy" },
            "Icy bells ringing out into nothing: built from a struck metal bar's own overtones (1, 2.76, 5.40 and 8.93 "
            "times the note: not harmonics, so it rings, not sings), each on its own envelope, the higher fading sooner, "
            "a digital space. MACROS: BRIGHT (more of the high overtones), SPACE, WEIGHT (a sub under the strike).");
        constexpr std::array<float, 4> semis { 0.0f, 17.0f, 29.0f, 37.0f };
        constexpr std::array<float, 4> fines { 0.0f, 58.0f, 20.0f, 87.0f };      // 2.76, 5.40, 8.93 times
        constexpr std::array<float, 4> levels { 0.5f, 0.3f, 0.18f, 0.1f };
        constexpr std::array<float, 4> seconds { 4.0f, 2.2f, 1.2f, 0.6f };
        for (int k = 0; k < 4; ++k)
        {
            bells.osc (k, "On", 1.0f).osc (k, "Wave", sine).osc (k, "Transpose", semis[(size_t) k]).osc (k, "Fine", fines[(size_t) k])
                 .osc (k, "Volume", 0.0f).osc (k, "PhaseRand", 0.0f);
            oneShot (bells, k, { SP18 { 0.0f, 1.0f, -0.6f }, SP18 { seconds[(size_t) k], 0.0f, 0.0f } });
            route (bells, ("env" + juce::String (k + 1)).toRawUTF8(), ParamIDs::osc (k, "Volume"), levels[(size_t) k] * 0.5f);
        }
        bells.ampEnvelope (1.0f, 4000.0f, 0.0f, 2500.0f).set (ParamIDs::ampVel, 0.5f);
        route (bells, "macro1", ParamIDs::osc (2, "Volume"), 0.06f);
        route (bells, "macro1", ParamIDs::osc (3, "Volume"), 0.05f);
        weightSub (bells, 4, 0.2f);
        macros (bells, "BRIGHT", "SPACE", "WEIGHT");
        reverb (bells, RT::digital, 0.8f, 0.3f, 30.0f, 5.5f);
        space (bells);
        bells.set (ParamIDs::patchLevel, -3.8f);   // LEVEL: set by its peaks when a chord is struck (measured, not by ear)
        x.add (bells);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto data = x.make ("Data Pulse", 13, { "Arp", "Precise", "Pulse", "Cold" },
            "A cold, precise, clinical arp: ARP 1 runs up the chord you hold over two octaves in sixteenths with your "
            "song, short, narrow pulses clipped by a tight filter, crisp dotted echoes, a plate. MACROS: BRIGHT, SPACE, "
            "WEIGHT (an octave down under every note).");
        data.osc (0, "Wave", pulse).osc (0, "Width", 0.18f).osc (0, "Volume", 0.55f).osc (0, "Arp", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.0f).osc (1, "Arp", 1.0f)
            .filter (FilterType::lp24, 2500.0f, 0.3f, 0.3f).filterEnvelope (0.0f, 90.0f, 0.1f, 60.0f)
            .ampEnvelope (1.0f, 120.0f, 0.2f, 60.0f)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
            .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.3f);
        route (data, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (data, "macro3", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (data, "BRIGHT", "SPACE", "WEIGHT");
        delay (data, "1/16 dotted", 0.35f, 0.18f, true, 6.0f);
        reverb (data, RT::plate, 0.5f, 0.15f);
        space (data);
        data.set (ParamIDs::patchLevel, 2.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (data);

        auto searchlight = x.make ("Searchlight", 14, { "Arp", "FFT", "Sweep", "Slow" },
            "A slow arp that sweeps and swells like a light passing over: ARP 1 plays the chord you hold in quarter notes, "
            "each note swelling in, the saw's harmonics opened and closed by a slow sweep of the FFT editor's LP (a slow "
            "LFO, the light crossing), a big hall. MACROS: BRIGHT, SPACE, WEIGHT (a sub under every note).");
        searchlight.osc (0, "Volume", 0.55f).osc (0, "FftLp", 8.0f).osc (0, "FftSlope", 0.6f).osc (0, "Arp", 1.0f)
                   .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.7f)
                   .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.0f).osc (1, "Arp", 1.0f)
                   .ampEnvelope (250.0f, 800.0f, 0.7f, 600.0f)
                   .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
                   .set (ParamIDs::arp (0, "Division"), division ("1/4")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
                   .set (ParamIDs::arp (0, "Gate"), 0.95f)
                   .set (ParamIDs::lfo (0, "Rate"), 0.12f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (searchlight, "lfo1", ParamIDs::osc (0, "FftLp"), 0.3f);
        route (searchlight, "macro1", ParamIDs::osc (0, "FftLp"), 0.2f);
        route (searchlight, "macro3", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (searchlight, "BRIGHT", "SPACE", "WEIGHT");
        delay (searchlight, "1/2", 0.35f, 0.15f, true, 4.0f);
        reverb (searchlight, RT::hall, 0.85f, 0.3f);
        space (searchlight);
        searchlight.set (ParamIDs::patchLevel, -2.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (searchlight);

        auto city = x.make ("Machine City", 15, { "Arp", "Step Sequencer", "Industrial", "Pulse" },
            "A vast machine city at work: the chord you hold pulses two octaves down in eighths with your song (the STEP "
            "SEQUENCER on a saw through a low filter), heavy and dark. MACROS: BRIGHT, SPACE, WEIGHT (the filter's "
            "grit).");
        city.osc (0, "Transpose", -24.0f).osc (0, "Volume", 0.5f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -24.0f).osc (1, "Volume", 0.3f)
            .filter (FilterType::lp24, 400.0f, 0.3f, 0.0f)
            .ampEnvelope (20.0f, 800.0f, 1.0f, 400.0f)
            .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.8f).set (ParamIDs::seqGate, 0.55f)
            .set (ParamIDs::seqContour, 0.25f).set (ParamIDs::seqDivision, division ("1/8")).set (ParamIDs::seqLength, 8.0f);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = city.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = k % 8 != 5;
                step.level = k % 4 == 0 ? 1.0f : 0.6f;
            }
        route (city, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (city, "macro3", ParamIDs::fltDrive, 0.5f);
        macros (city, "BRIGHT", "SPACE", "WEIGHT");
        reverb (city, RT::hall, 0.8f, 0.2f);
        space (city);
        city.set (ParamIDs::patchLevel, -1.7f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        x.add (city);
    }

    // SUPERPOSITION: inspired by the score of a 2024 science-fiction series about a physicist thrown into other versions
    // of his own life, trying to find his way home: cold, clinical and cerebral, a pulse of quiet dread, the same sound in
    // many versions (copies drifting apart, notes that come out different each time, patterns that never repeat), and
    // under it a warm, human thread of home. No title, characters, places, terms or music from the series: our own
    // sounds. MACRO 3 is SPLIT on every preset: the sound pulled into more, and more different, versions of itself.
    void superposition (Expansion x)
    {
        using SP19 = synth::mod::ShapePoint;
        const auto oneShot = [] (Builder& b, int env, std::vector<SP19> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers, buses and VOICE
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };
        const auto ratio = [] (float r) { return 0.5f + std::log2 (r) / 8.0f; };   // FM / AM RATE for a ratio

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto other = x.make ("Other Self", 1, { "Lead", "Double", "Pulse", "Uneasy" },
            "The same line played by two of you: a pulse on the left and a second pulse on the right that arrives a "
            "moment later (an envelope fades it in over 150 ms), nine cents apart, its pitch settling from a little "
            "flat (the PITCH envelope, on the second only), a soft glide, ping-pong echoes. MACROS: BRIGHT, SPACE, SPLIT "
            "(the two pull further apart in tune). MOD WHEEL: vibrato.");
        other.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Volume", 0.45f).osc (0, "Pan", -0.45f).osc (0, "PitchEnv", 0.0f)
             .osc (0, "VibRate", 5.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.35f).osc (1, "Volume", 0.0f).osc (1, "Pan", 0.45f)
             .osc (1, "Fine", 9.0f).osc (1, "VibRate", 4.6f)
             .filter (FilterType::lp24, 2600.0f, 0.2f, 0.2f).filterEnvelope (10.0f, 400.0f, 0.6f, 400.0f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (15.0f, 500.0f, 0.85f, 500.0f)
             .mono (40.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 1.0f);
        other.rig.modulation.pitchEnvelope = { { SP19 { 0.0f, 0.2f, -0.4f }, SP19 { 0.3f, 0.5f, 0.0f } }, -1, -1, -1 };
        oneShot (other, 0, { SP19 { 0.0f, 0.0f, 0.3f }, SP19 { 0.15f, 1.0f, 0.0f } });
        route (other, "env1", ParamIDs::osc (1, "Volume"), 0.22f);
        route (other, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (other, "modWheel", ParamIDs::osc (1, "VibAmount"), 0.2f);
        route (other, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (other, "macro3", ParamIDs::osc (1, "Fine"), 0.12f);
        macros (other, "BRIGHT", "SPACE", "SPLIT");
        delay (other, "1/8 dotted", 0.35f, 0.18f, true, 4.5f);
        reverb (other, RT::plate, 0.6f, 0.15f);
        space (other);
        other.set (ParamIDs::patchLevel, -2.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (other);

        auto collapse = x.make ("Collapse Lead", 2, { "Lead", "Sync", "Random", "Edgy" },
            "A hard-sync lead that never plays a note the same way twice: how hard the saw is synced is rolled at random "
            "on every note (the RANDOM source), so each one lands as a slightly different version of itself; a sine an "
            "octave down keeps it grounded. MACROS: BRIGHT, SPACE, SPLIT (a wider roll: the notes differ more). MOD "
            "WHEEL: vibrato.");
        collapse.osc (0, "Volume", 0.5f).osc (0, "SyncAmount", 0.25f).osc (0, "VibRate", 5.5f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "PhaseRand", 0.0f)
                .filter (FilterType::lp24, 4500.0f, 0.15f, 0.15f).filterEnvelope (0.0f, 300.0f, 0.6f, 300.0f).set (ParamIDs::fltKey, 0.4f)
                .ampEnvelope (6.0f, 400.0f, 0.8f, 350.0f);
        clean (collapse, 1);
        route (collapse, "random", ParamIDs::osc (0, "SyncAmount"), 0.25f);
        route (collapse, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.2f);
        route (collapse, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (collapse, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (collapse, "BRIGHT", "SPACE", "SPLIT");
        delay (collapse, "1/8", 0.3f, 0.15f, true, 5.0f);
        reverb (collapse, RT::digital, 0.6f, 0.15f);
        space (collapse);
        collapse.set (ParamIDs::patchLevel, -0.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (collapse);

        auto lucid = x.make ("Lucid Line", 3, { "Lead", "Freeform", "Clean", "Intimate" },
            "A clean, close, intimate line: a hand-drawn wave (DRAW: a sine leaning to one side, warm with even "
            "harmonics), its tuning drifting very slightly at random, a quiet plate. MACROS: BRIGHT, SPACE, SPLIT (a "
            "second drawn copy fades in a dozen cents away). MOD WHEEL: vibrato.");
        {
            synth::FreeformDrawing d;
            for (int i = 0; i < synth::FreeformDrawing::points; ++i)
            {
                const auto t = juce::MathConstants<float>::twoPi * (float) i / (float) synth::FreeformDrawing::points;
                d.values[(size_t) i] = 0.75f * std::sin (t) + 0.25f * std::sin (2.0f * t) + 0.1f * std::sin (4.0f * t);
            }
            d.smoothA = 0.05f;
            lucid.rig.drawings[0] = d;
            lucid.rig.drawings[1] = d;
        }
        lucid.osc (0, "Wave", (float) synth::OscWave::freeform).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f).osc (0, "VibRate", 5.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", (float) synth::OscWave::freeform).osc (1, "Volume", 0.0f).osc (1, "Fine", 12.0f)
             .osc (1, "Pan", 0.4f)
             .filter (FilterType::lp12, 4000.0f, 0.05f, 0.0f)
             .ampEnvelope (30.0f, 500.0f, 0.9f, 600.0f)
             .mono (50.0f)
             .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.4f);
        route (lucid, "lfo1", ParamIDs::osc (0, "Fine"), 0.02f);
        route (lucid, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (lucid, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (lucid, "macro3", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (lucid, "BRIGHT", "SPACE", "SPLIT");
        delay (lucid, "1/4", 0.3f, 0.12f, false, 4.0f);
        reverb (lucid, RT::plate, 0.6f, 0.18f);
        space (lucid);
        lucid.set (ParamIDs::patchLevel, -5.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (lucid);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto interference = x.make ("Interference", 4, { "Pad", "Wavetable", "Flicker", "Cold" },
            "Two versions of the same chord laid over each other: the Hard Sync table in STEPPY mode, its frame jumping "
            "at random between two places several times a second (a sample-and-hold LFO), so the chord flickers between "
            "two colours, in a slow unison, a cold hall. MACROS: BRIGHT, SPACE, SPLIT (more unison spread). MOD WHEEL: "
            "wider jumps.");
        interference.osc (0, "Wave", wavetable).osc (0, "Table", table ("Hard Sync")).osc (0, "WtIndex", 0.35f)
                    .osc (0, "WtMode", 1.0f).osc (0, "Volume", 0.5f)
                    .osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.85f)
                    .filter (FilterType::lp12, 2200.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.3f)
                    .ampEnvelope (900.0f, 2000.0f, 1.0f, 2500.0f)
                    .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sampleHold).set (ParamIDs::lfo (0, "Rate"), 5.0f)
                    .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (interference, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.12f);
        route (interference, "modWheel", ParamIDs::osc (0, "WtIndex"), 0.15f);
        route (interference, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (interference, "macro3", ParamIDs::osc (0, "UniDetune"), 0.2f);
        macros (interference, "BRIGHT", "SPACE", "SPLIT");
        reverb (interference, RT::hall, 0.85f, 0.3f, 40.0f, 4.0f);
        eqBand (interference, 0, eq::Shape::lowCut, 80.0f, 0.0f);
        space (interference);
        interference.set (ParamIDs::patchLevel, -5.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (interference);

        auto fog = x.make ("Quantum Fog", 5, { "Pad", "Voice", "Noise", "Airless" },
            "A cold, airless haze: breath noise and a faint triangle sung by the VOICE filter, its vowel drifting slowly "
            "at random, high and thin, a dark hall. MACROS: BRIGHT (sharper vowels), SPACE, SPLIT (detuned copies of the "
            "triangle come in). MOD WHEEL: more breath.");
        fog.osc (0, "Gain", 0.0f).osc (0, "Noise", 0.7f).osc (0, "NoiseColour", 0.1f).osc (0, "NoiseWidth", 2.0f).osc (0, "Volume", 0.6f)
           .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.35f)
           .osc (1, "UniVoices", 5.0f).osc (1, "UniMix", 0.0f).osc (1, "UniDetune", 0.12f).osc (1, "UniSpread", 0.9f)
           .ampEnvelope (1500.0f, 2000.0f, 1.0f, 2500.0f)
           .set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceVowel, 0.6f).set (ParamIDs::voiceSize, 0.8f).set (ParamIDs::voiceSharp, 0.6f)
           .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.15f)
           .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (fog, "lfo1", ParamIDs::voiceVowel, 0.3f);
        route (fog, "modWheel", ParamIDs::osc (0, "Volume"), 0.3f);
        route (fog, "macro1", ParamIDs::voiceSharp, 0.3f);
        route (fog, "macro3", ParamIDs::osc (1, "UniMix"), 0.7f);
        macros (fog, "BRIGHT", "SPACE", "SPLIT");
        reverb (fog, RT::hall, 0.9f, 0.3f, 50.0f, 3.0f);
        eqBand (fog, 0, eq::Shape::lowCut, 180.0f, 0.0f);
        space (fog);
        fog.set (ParamIDs::patchLevel, -5.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (fog);

        auto echo = x.make ("Infinite Echo", 6, { "Pad", "FFT", "Echo", "Distant" },
            "A pad that trails off into copies of itself: a saw edited in the FFT editor down to its first eight "
            "harmonics (soft and round), in a slow unison, feeding long ping-pong echoes into a cathedral, each echo "
            "further away. MACROS: BRIGHT, SPACE, SPLIT (a wider unison). MOD WHEEL: opens the FFT editor's LP.");
        for (int band = 8; band < synth::SpectrumEdit::bands; ++band)
            echo.rig.spectra[0].gainsDb[(size_t) band] = synth::SpectrumEdit::minDb;
        echo.osc (0, "Volume", 0.55f).osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.07f).osc (0, "UniSpread", 0.8f)
            .osc (0, "FftLp", 6.0f).osc (0, "FftSlope", 0.5f)
            .ampEnvelope (700.0f, 1500.0f, 0.9f, 2000.0f);
        route (echo, "modWheel", ParamIDs::osc (0, "FftLp"), 0.3f);
        route (echo, "macro1", ParamIDs::osc (0, "FftLp"), 0.3f);
        route (echo, "macro3", ParamIDs::osc (0, "UniDetune"), 0.2f);
        macros (echo, "BRIGHT", "SPACE", "SPLIT");
        delay (echo, "1/4 dotted", 0.45f, 0.28f, true, 3.5f);
        echo.set (ParamIDs::delayToReverb, 0.4f);
        reverb (echo, RT::cathedral, 0.8f, 0.28f, 40.0f, 3.5f);
        space (echo);
        echo.set (ParamIDs::patchLevel, -10.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (echo);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto probability = x.make ("Probability Bass", 7, { "Bass", "Wavetable", "Travel", "Moving" },
            "A bass that changes colour beat by beat: the Wavefold table walked by TRAVEL's pattern in eighths with your "
            "song (forward, jumps high and low, backwards, a retrigger), a low filter, a clean sine underneath. MACROS: "
            "BRIGHT, SPACE, SPLIT (bigger jumps). MOD WHEEL: opens the filter.");
        probability.osc (0, "Wave", wavetable).osc (0, "Table", table ("Wavefold")).osc (0, "WtIndex", 0.15f).osc (0, "Volume", 0.5f)
                   .osc (0, "TrOn", 1.0f).osc (0, "TrEditors", (float) synth::travel::Editors::envSeq).osc (0, "TrSteps", 8.0f)
                   .osc (0, "TrSync", (float) synth::mod::LfoSync::straight).osc (0, "TrDivision", division ("1/8"))
                   .osc (0, "TrStrength", 0.45f).osc (0, "TrTrigger", (float) synth::travel::Trigger::firstNote)
                   .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
                   .filter (FilterType::lp24, 900.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.5f)
                   .ampEnvelope (5.0f, 600.0f, 1.0f, 200.0f)
                   .mono (20.0f);
        clean (probability, 1);
        {
            auto& t = probability.rig.modulation.travel[0];
            t.envelope = { { SP19 { 0.0f, 0.0f, 0.0f }, SP19 { 1.0f, 1.0f, 0.0f } }, -1, -1, -1 };
            using M = synth::travel::StepMode;
            const synth::travel::Step steps[] = { { M::normal, 1.0f, 1 }, { M::high, 1.0f, 1 }, { M::normal, 1.0f, 1 }, { M::reverse, 1.0f, 1 },
                                                  { M::low, 1.0f, 1 }, { M::trigger, 2.0f, 1 }, { M::mid, 1.0f, 1 }, { M::normal, 1.0f, 1 } };
            for (int k = 0; k < 8; ++k)
                t.pattern.steps[(size_t) k] = steps[k];
        }
        route (probability, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (probability, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (probability, "macro3", ParamIDs::osc (0, "TrStrength"), 0.25f);
        macros (probability, "BRIGHT", "SPACE", "SPLIT");
        reverb (probability, RT::room, 0.4f, 0.06f);
        space (probability);
        probability.set (ParamIDs::patchLevel, -4.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (probability);

        auto horizon = x.make ("Event Horizon", 8, { "Bass", "FM", "Pitch", "Deep" },
            "A deep, dark bass with a growl inside it: a sine phase-modulated by a sine an octave below (the growl, "
            "brightest at the strike and stronger the harder you play), and when you let go, the note falls an octave "
            "into silence (the PITCH envelope's release). MACROS: BRIGHT, SPACE, SPLIT (a detuned copy beside it). MOD "
            "WHEEL: more growl.");
        horizon.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
               .osc (0, "FmSource", (float) (synth::fmFirstOscillator + 1)).osc (0, "FmAmount", 0.1f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 1.0f).osc (1, "PhaseRand", 0.0f)
               .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Fine", 14.0f).osc (2, "Volume", 0.0f).osc (2, "Pan", 0.5f)
               .ampEnvelope (4.0f, 800.0f, 0.8f, 600.0f)
               .mono (25.0f)
               .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 12.0f)
               .set (ParamIDs::pitchEnvMode, (float) synth::mod::EnvelopeMode::sustain);
        horizon.rig.modulation.pitchEnvelope = { { SP19 { 0.0f, 0.5f, 0.0f }, SP19 { 0.05f, 0.5f, 0.4f }, SP19 { 0.6f, 0.0f, 0.0f } }, 1, -1, -1 };
        oneShot (horizon, 0, { SP19 { 0.0f, 1.0f, -0.5f }, SP19 { 0.5f, 0.2f, 0.0f } });
        route (horizon, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (horizon, "velocity", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (horizon, "modWheel", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (horizon, "macro1", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (horizon, "macro3", ParamIDs::osc (2, "Volume"), 0.25f);
        macros (horizon, "BRIGHT", "SPACE", "SPLIT");
        reverb (horizon, RT::room, 0.4f, 0.05f);
        space (horizon);
        horizon.set (ParamIDs::patchLevel, -3.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (horizon);

        auto fracture = x.make ("Fracture Bass", 9, { "Bass", "Crush", "Shaper", "Gritty" },
            "A broken, gritty bass: a square, BIT-crushed into steps, phase-modulated by its own pitch for a hollow "
            "bite, through a hard shaper and a low filter, with a clean sine underneath it all. MACROS: BRIGHT, SPACE, "
            "SPLIT (more crush). MOD WHEEL: opens the filter.");
        fracture.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.45f).osc (0, "Crush", 0.2f).osc (0, "CrushMode", 1.0f)
                .osc (0, "FmAmount", 0.12f).osc (0, "ModRate", ratio (2.0f)).osc (0, "ModGrid", 1.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
                .shaper (0, synth::ShaperModel::hard, 0.15f)
                .filter (FilterType::lp24, 800.0f, 0.25f, 0.2f).filterEnvelope (0.0f, 250.0f, 0.4f, 200.0f)
                .ampEnvelope (4.0f, 500.0f, 1.0f, 200.0f)
                .mono (15.0f);
        clean (fracture, 1);
        route (fracture, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (fracture, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (fracture, "macro3", ParamIDs::osc (0, "Crush"), 0.3f);
        macros (fracture, "BRIGHT", "SPACE", "SPLIT");
        reverb (fracture, RT::room, 0.4f, 0.05f);
        space (fracture);
        fracture.set (ParamIDs::patchLevel, -9.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (fracture);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto home = x.make ("Home Piano", 10, { "Keys", "Piano", "Warm", "Close" },
            "The warm thread of home: NEGATIVE SPACE's soft felt piano, close and intimate, in a small warm room. "
            "MACROS: BRIGHT, SPACE, SPLIT (a soft chorus: the memory blurring). The MOD WHEEL adds the felt's noise.");
        softPiano (home);
        chorus (home, ParamIDs::mod2Ids, MT::chorus, 0.5f, 0.5f, 0.0f);
        route (home, "macro3", ParamIDs::mod2Mix, 0.5f);
        macros (home, "BRIGHT", "SPACE", "SPLIT");
        reverb (home, RT::room, 0.5f, 0.2f, 15.0f, 4.0f);
        space (home);
        home.set (ParamIDs::patchLevel, -9.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (home);

        auto worlds = x.make ("Many Worlds Keys", 11, { "Keys", "Chorder", "Detuned", "Wide" },
            "Every version of a note at once: a soft electric-piano tone (a triangle and a quiet sine an octave up) where "
            "each key plays three copies, one in tune in the middle, one fourteen cents sharp on the left and one "
            "fourteen flat on the right (the chorder), so a chord shimmers and spreads. MACROS: BRIGHT, SPACE, SPLIT (a "
            "unison spread on top). MOD WHEEL: tremolo.");
        worlds.osc (0, "Wave", triangle).osc (0, "Volume", 0.45f).osc (0, "PhaseRand", 0.0f)
              .osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 0.0f).osc (0, "Ch1Fine", 14.0f).osc (0, "Ch1Pan", -0.7f).osc (0, "Ch1Vol", 0.7f)
              .osc (0, "Ch2On", 1.0f).osc (0, "Ch2Coarse", 0.0f).osc (0, "Ch2Fine", -14.0f).osc (0, "Ch2Pan", 0.7f).osc (0, "Ch2Vol", 0.7f)
              .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.0f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 0.8f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.12f)
              .filter (FilterType::lp12, 3000.0f, 0.05f, 0.3f).filterEnvelope (0.0f, 600.0f, 0.3f, 400.0f).set (ParamIDs::fltKey, 0.5f)
              .ampEnvelope (2.0f, 2000.0f, 0.3f, 600.0f).set (ParamIDs::ampVel, 0.5f);
        chorus (worlds, ParamIDs::modIds, MT::tremolo, 0.45f, 0.5f, 0.0f);
        route (worlds, "modWheel", ParamIDs::modMix, 0.6f);
        route (worlds, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (worlds, "macro3", ParamIDs::osc (0, "UniMix"), 0.6f);
        macros (worlds, "BRIGHT", "SPACE", "SPLIT");
        reverb (worlds, RT::hall, 0.6f, 0.2f);
        space (worlds);
        worlds.set (ParamIDs::patchLevel, -4.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (worlds);

        auto glass = x.make ("Lab Glass", 12, { "Keys", "FM", "Glass", "Exact" },
            "A cold, exact struck-glass tone: a sine phase-modulated at three and a half times its pitch (not a "
            "harmonic, so it rings like glass), the FM bright at the strike and gone in half a second, louder with "
            "velocity. MACROS: BRIGHT, SPACE, SPLIT (a second glass beside it, a quarter tone off). ");
        glass.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmAmount", 0.05f).osc (0, "ModRate", ratio (3.5f)).osc (0, "ModGrid", 0.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Fine", 50.0f).osc (1, "Volume", 0.0f).osc (1, "Pan", 0.5f)
             .osc (1, "FmAmount", 0.05f).osc (1, "ModRate", ratio (3.5f)).osc (1, "ModGrid", 0.0f)
             .ampEnvelope (1.0f, 2500.0f, 0.0f, 1500.0f).set (ParamIDs::ampVel, 0.5f);
        oneShot (glass, 0, { SP19 { 0.0f, 1.0f, -0.6f }, SP19 { 0.5f, 0.0f, 0.0f } });
        route (glass, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (glass, "env1", ParamIDs::osc (1, "FmAmount"), 0.25f);
        route (glass, "velocity", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (glass, "macro1", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (glass, "macro3", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (glass, "BRIGHT", "SPACE", "SPLIT");
        reverb (glass, RT::digital, 0.7f, 0.25f, 30.0f, 5.5f);
        space (glass);
        glass.set (ParamIDs::patchLevel, -7.5f);   // LEVEL: set by its peaks when a chord is struck (measured, not by ear)
        x.add (glass);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto paths = x.make ("Branching Paths", 13, { "Arp", "Random", "Pluck", "Searching" },
            "A different path through the chord every time: ARP 1 plays what you hold over two octaves in sixteenths, "
            "the randomizer rolling a fresh pattern (C minor, a few rests) on every pass, a soft pluck, dotted echoes. "
            "MACROS: BRIGHT, SPACE, SPLIT (a second voice a fifth up joins every note).");
        paths.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Transpose", 7.0f).osc (1, "Volume", 0.0f)
             .osc (1, "Arp", 1.0f)
             .filter (FilterType::lp24, 2500.0f, 0.2f, 0.35f).filterEnvelope (0.0f, 150.0f, 0.1f, 100.0f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (1.0f, 250.0f, 0.0f, 150.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::random)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.5f);
        {
            auto& r = paths.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Minor");
            r.restChance = 0.25f;
            r.lengthChances = { 1.0f, 0.15f, 0.0f, 0.0f };
            r.velocityLow = 0.6f;
            r.autoDice = true;
            r.seed = 7;
            auto& p = paths.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        route (paths, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (paths, "macro3", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (paths, "BRIGHT", "SPACE", "SPLIT");
        delay (paths, "1/8 dotted", 0.4f, 0.2f, true, 4.5f);
        reverb (paths, RT::hall, 0.6f, 0.18f);
        space (paths);
        paths.set (ParamIDs::patchLevel, 4.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (paths);

        auto countdown = x.make ("Countdown Pulse", 14, { "Arp", "Step Seq", "Ticking", "Tense" },
            "A tight, ticking pulse that won't let up: the chord you hold chopped into sixteenths by the STEP SEQUENCER "
            "(every fourth tick accented, crisp edges, one tick missing each bar), narrow pulses through a tight filter. "
            "MACROS: BRIGHT, SPACE, SPLIT (a detuned copy an octave up). MOD WHEEL: opens the filter.");
        countdown.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Volume", 0.55f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.35f).osc (1, "Transpose", 12.0f).osc (1, "Fine", 10.0f)
                 .osc (1, "Volume", 0.0f)
                 .filter (FilterType::lp24, 2000.0f, 0.12f, 0.0f).set (ParamIDs::fltKey, 0.5f)
                 .ampEnvelope (5.0f, 500.0f, 1.0f, 300.0f)
                 .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 1.0f).set (ParamIDs::seqGate, 0.55f)
                 .set (ParamIDs::seqContour, 0.12f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = countdown.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = k % 16 != 13;
                step.level = k % 4 == 0 ? 1.0f : 0.55f;
            }
        route (countdown, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (countdown, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (countdown, "macro3", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (countdown, "BRIGHT", "SPACE", "SPLIT");
        delay (countdown, "1/8 dotted", 0.25f, 0.1f, true, 5.0f);
        reverb (countdown, RT::plate, 0.5f, 0.12f);
        space (countdown);
        countdown.set (ParamIDs::patchLevel, -6.6f);   // LEVEL: set by its peaks: its ticks peak far above their average (measured, not by ear)
        x.add (countdown);

        auto threshold = x.make ("Threshold", 15, { "Arp", "Pulse", "Throb", "Dark" },
            "Standing at the edge of the unknown: the chord you hold throbs darkly, its filter opening and closing on "
            "every beat. MACROS: BRIGHT, SPACE, SPLIT (a wider unison).");
        threshold.osc (0, "Volume", 0.45f).osc (0, "Transpose", -12.0f)
                 .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.06f).osc (0, "UniSpread", 0.7f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f).osc (1, "PhaseRand", 0.0f)
                 .filter (FilterType::lp24, 500.0f, 0.25f, 0.0f)
                 .ampEnvelope (300.0f, 1000.0f, 1.0f, 800.0f)
                 .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::triangle)
                 .set (ParamIDs::lfo (0, "Sync"), (float) synth::mod::LfoSync::straight).set (ParamIDs::lfo (0, "Division"), 5.0f)
                 .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (threshold, "lfo1", ParamIDs::fltCutoff, 0.12f);
        route (threshold, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (threshold, "macro3", ParamIDs::osc (0, "UniDetune"), 0.2f);
        macros (threshold, "BRIGHT", "SPACE", "SPLIT");
        reverb (threshold, RT::hall, 0.8f, 0.2f);
        space (threshold);
        threshold.set (ParamIDs::patchLevel, -6.1f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        x.add (threshold);
    }

    // SEVERED: inspired by the score of a 2022 series about office workers whose work memories are surgically split
    // from their home lives: a cheerful, sterile corporate surface (muzak, chimes, retro screens, humming lights, long
    // white corridors) over quiet dread and a melancholy piano. No title, company, departments, characters, terms or
    // music from the series: our own sounds. MACRO 3 is DREAD on every preset: the cheerful surface turns uneasy.
    // Each preset is built round a different part of the synth.
    void severed (Expansion x)
    {
        using SP20 = synth::mod::ShapePoint;
        using GM = effects::Glitch::Mode;
        const auto oneShot = [] (Builder& b, int env, std::vector<SP20> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers, buses and VOICE
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };
        const auto ratio = [] (float r) { return 0.5f + std::log2 (r) / 8.0f; };   // FM / AM RATE for a ratio
        // DREAD's clash: a quiet copy of the note a tritone up (or a half step), silent until MACRO 3 brings it in.
        const auto clash = [&clean] (Builder& b, int o, synth::OscWave wave, float semitones, float amount)
        {
            b.osc (o, "On", 1.0f).osc (o, "Wave", (float) wave).osc (o, "Transpose", semitones).osc (o, "Volume", 0.0f);
            clean (b, o);
            route (b, "macro3", ParamIDs::osc (o, "Volume"), amount);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto memo = x.make ("Cheerful Memo", 1, { "Lead", "Freeform", "Bright", "Office" },
            "A chipper office-jingle lead with a smile that doesn't reach the eyes: a hand-drawn wave (DRAW: a bright, "
            "lopsided cycle) evened out by EDGE's SYMMETRY mode, blending it with its own mirror image, so it's rounder "
            "and cleaner), a quick pluck of the filter, a short room. MACROS: BRIGHT, SPACE, DREAD (a tritone creeps in "
            "underneath and the pitch wavers). MOD WHEEL: vibrato.");
        {
            synth::FreeformDrawing d;
            for (int i = 0; i < synth::FreeformDrawing::points; ++i)
            {
                const auto t = (float) i / (float) synth::FreeformDrawing::points;
                d.values[(size_t) i] = t < 0.3f ? t / 0.3f : 1.0f - 1.6f * (t - 0.3f) / 0.7f;   // a fast rise, a long fall
            }
            d.edge = 1.0f;
            d.edgeMode = synth::FreeformEdge::symmetry;
            memo.rig.drawings[0] = d;
        }
        memo.osc (0, "Wave", (float) synth::OscWave::freeform).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "VibRate", 5.5f)
            .filter (FilterType::lp24, 3500.0f, 0.15f, 0.3f).filterEnvelope (0.0f, 250.0f, 0.5f, 200.0f).set (ParamIDs::fltKey, 0.5f)
            .ampEnvelope (5.0f, 400.0f, 0.75f, 250.0f)
            .mono (25.0f);
        clash (memo, 1, synth::OscWave::triangle, -6.0f, 0.3f);
        route (memo, "macro3", ParamIDs::osc (0, "VibAmount"), 0.15f);
        route (memo, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (memo, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (memo, "BRIGHT", "SPACE", "DREAD");
        delay (memo, "1/8", 0.25f, 0.1f, false, 5.0f);
        reverb (memo, RT::room, 0.4f, 0.12f);
        space (memo);
        memo.set (ParamIDs::patchLevel, 1.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (memo);

        auto sorter = x.make ("Number Sorter", 2, { "Lead", "Crush", "Retro", "Digital" },
            "A stepped, retro-screen lead, like an old terminal humming to itself: a triangle cut into steps by SHAPE's "
            "CRUSH in RATE mode (so it buzzes like early computer sound), a square an octave down, a dry little room. "
            "MACROS: BRIGHT (finer steps), SPACE, DREAD (coarser steps and a half-step clash). MOD WHEEL: vibrato.");
        sorter.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "Crush", 0.35f).osc (0, "CrushMode", 0.0f).osc (0, "VibRate", 6.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.2f)
              .ampEnvelope (3.0f, 300.0f, 0.8f, 150.0f)
              .mono (0.0f);
        clash (sorter, 2, synth::OscWave::triangle, 1.0f, 0.2f);
        route (sorter, "macro1", ParamIDs::osc (0, "Crush"), -0.2f);
        route (sorter, "macro3", ParamIDs::osc (0, "Crush"), 0.3f);
        route (sorter, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        macros (sorter, "BRIGHT", "SPACE", "DREAD");
        delay (sorter, "1/16 dotted", 0.3f, 0.12f, true, 5.0f);
        reverb (sorter, RT::room, 0.35f, 0.1f);
        space (sorter);
        sorter.set (ParamIDs::patchLevel, -0.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (sorter);

        auto whistle = x.make ("Hallway Whistle", 3, { "Lead", "Formant", "Whistle", "Echo" },
            "A thin whistle echoing down long white corridors: a sine played faster within each cycle by SHAPE's "
            "FORMANT in X-FADE mode (a hollow, breathy edge), through a band-pass sitting on the note, a breath of noise, "
            "long echoes and a hall. MACROS: BRIGHT (more formant), SPACE, DREAD (the pitch sags flat and wavers). MOD "
            "WHEEL: vibrato.");
        whistle.osc (0, "Wave", sine).osc (0, "Volume", 0.8f).osc (0, "PhaseRand", 0.0f).osc (0, "Formant", 0.35f)
               .osc (0, "FormantMode", 2.0f).osc (0, "VibRate", 5.0f).osc (0, "VibAmount", 4.0f).osc (0, "VibFade", 400.0f)
               .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.1f).osc (1, "Volume", 0.08f)
               .filter (FilterType::bp12, 350.0f, 0.3f, 0.0f).set (ParamIDs::fltKey, 1.0f)
               .ampEnvelope (60.0f, 500.0f, 0.9f, 500.0f)
               .mono (60.0f);
        route (whistle, "macro1", ParamIDs::osc (0, "Formant"), 0.2f);
        route (whistle, "macro3", ParamIDs::osc (0, "Fine"), -0.12f);
        route (whistle, "macro3", ParamIDs::osc (0, "VibAmount"), 0.12f);
        route (whistle, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        macros (whistle, "BRIGHT", "SPACE", "DREAD");
        delay (whistle, "1/4 dotted", 0.4f, 0.2f, true, 3.5f);
        reverb (whistle, RT::hall, 0.8f, 0.25f, 40.0f, 4.0f);
        eqBand (whistle, 0, eq::Shape::lowCut, 200.0f, 0.0f);
        space (whistle);
        whistle.set (ParamIDs::patchLevel, 4.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (whistle);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto hum = x.make ("Fluorescent Hum", 4, { "Pad", "FFT", "Pulsing", "Office" },
            "Office lights buzzing overhead: a saw whose harmonics swell and fade at a fast, uneven flicker (the FFT "
            "editor's VSPEED and VSTRENGTH), a quiet mains-like hum an octave down, and ECHELON's dynamic band pulling "
            "the low-mids back whenever they bloom. MACROS: BRIGHT, SPACE, DREAD (a tritone and the filter's "
            "grit). MOD WHEEL: opens the filter.");
        hum.osc (0, "Volume", 0.5f).osc (0, "FftVSpeed", 0.55f).osc (0, "FftVStrength", 0.5f)
           .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.06f).osc (0, "UniSpread", 0.7f)
           .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.2f)
           .filter (FilterType::lp12, 2500.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.3f)
           .ampEnvelope (700.0f, 1500.0f, 1.0f, 1500.0f);
        clash (hum, 2, synth::OscWave::saw, 6.0f, 0.15f);
        eqBand (hum, 0, eq::Shape::bell, 300.0f, 0.0f, 1.0f);
        hum.set (ParamIDs::eqBand (0, "Dyn"), -6.0f).set (ParamIDs::eqBand (0, "DynAuto"), 1.0f);
        route (hum, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (hum, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (hum, "macro3", ParamIDs::fltDrive, 0.4f);
        macros (hum, "BRIGHT", "SPACE", "DREAD");
        reverb (hum, RT::room, 0.6f, 0.2f);
        space (hum);
        hum.set (ParamIDs::patchLevel, -6.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hum);

        auto wellness = x.make ("Wellness Room", 5, { "Pad", "Warp", "Soothing", "Wrong" },
            "Soothing on the surface, quietly bending out of shape underneath: soft triangles whose cycles are skewed and "
            "pinched back and forth by two slow LFOs (SHAPE's SKEW and PINCH), so the tone keeps leaning and never "
            "settles, a warm filter, a big soft hall. MACROS: BRIGHT, SPACE, DREAD (deeper bending and a tritone). MOD "
            "WHEEL: more bending.");
        wellness.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f)
                .osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.07f).osc (0, "UniSpread", 0.8f)
                .filter (FilterType::lp12, 2000.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.4f)
                .ampEnvelope (1200.0f, 2000.0f, 1.0f, 2500.0f)
                .set (ParamIDs::lfo (0, "Rate"), 0.11f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
                .set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (1, "Rate"), 0.17f)
                .set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        clash (wellness, 1, synth::OscWave::triangle, 6.0f, 0.15f);
        route (wellness, "lfo1", ParamIDs::osc (0, "Skew"), 0.3f);
        route (wellness, "lfo2", ParamIDs::osc (0, "Pinch"), 0.25f);
        route (wellness, "modWheel", ParamIDs::osc (0, "Skew"), 0.3f);
        route (wellness, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (wellness, "macro3", ParamIDs::osc (0, "Pinch"), 0.3f);
        macros (wellness, "BRIGHT", "SPACE", "DREAD");
        reverb (wellness, RT::hall, 0.9f, 0.3f, 40.0f, 3.5f);
        space (wellness);
        wellness.set (ParamIDs::patchLevel, -15.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (wellness);

        auto elevator = x.make ("Elevator Descent", 6, { "Pad", "Organ", "Rotary", "Sinking" },
            "The long ride down between one life and the other: the Organ Stack table through MOD's ROTARY speaker, the "
            "whole chord sinking slowly a semitone over each held chord (the PITCH envelope), a soft hall. MACROS: BRIGHT, "
            "SPACE, DREAD (a deeper sink and a faster rotor). MOD WHEEL: the rotor speeds up.");
        elevator.osc (0, "Wave", wavetable).osc (0, "Table", table ("Organ Stack")).osc (0, "WtIndex", 0.4f).osc (0, "Volume", 0.6f)
                .filter (FilterType::lp12, 3000.0f, 0.1f, 0.0f)
                .ampEnvelope (250.0f, 1000.0f, 1.0f, 1500.0f)
                .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        elevator.rig.modulation.pitchEnvelope = { { SP20 { 0.0f, 0.5f, 0.0f }, SP20 { 6.0f, 0.25f, 0.0f } }, -1, -1, -1 };
        chorus (elevator, ParamIDs::modIds, MT::rotary, 0.3f, 0.6f, 0.8f);
        route (elevator, "modWheel", ParamIDs::modRate, 0.4f);
        route (elevator, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (elevator, "macro3", ParamIDs::pitchEnvRange, 0.08f);
        route (elevator, "macro3", ParamIDs::modRate, 0.3f);
        macros (elevator, "BRIGHT", "SPACE", "DREAD");
        reverb (elevator, RT::hall, 0.7f, 0.22f);
        space (elevator);
        elevator.set (ParamIDs::patchLevel, -0.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (elevator);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto quarterly = x.make ("Quarterly Bass", 7, { "Bass", "Step Seq", "Legato", "Pulse" },
            "A busy, efficient bass that never stops working: a held note chopped by the STEP SEQUENCER in sixteenths "
            "with your song, some steps TIED so they flow into the next without a gap (legato runs among the stabs), a "
            "low filter, a clean sine under it. MACROS: BRIGHT, SPACE, DREAD (the filter's grit and a half-step growl). "
            "MOD WHEEL: opens the filter.");
        quarterly.osc (0, "Wave", pulse).osc (0, "Width", 0.4f).osc (0, "Volume", 0.5f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
                 .filter (FilterType::lp24, 700.0f, 0.25f, 0.0f).set (ParamIDs::fltKey, 0.5f)
                 .ampEnvelope (5.0f, 500.0f, 1.0f, 200.0f)
                 .mono (15.0f)
                 .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 1.0f).set (ParamIDs::seqGate, 0.6f)
                 .set (ParamIDs::seqContour, 0.15f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        clean (quarterly, 1);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = quarterly.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                const auto at = k % 16;
                step.on = at != 3 && at != 11;
                step.level = at % 4 == 0 ? 1.0f : 0.7f;
                step.tied = at == 5 || at == 6 || at == 13;   // runs of linked steps
            }
        clash (quarterly, 2, synth::OscWave::saw, 1.0f, 0.15f);
        route (quarterly, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (quarterly, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (quarterly, "macro3", ParamIDs::fltDrive, 0.4f);
        macros (quarterly, "BRIGHT", "SPACE", "DREAD");
        reverb (quarterly, RT::room, 0.35f, 0.05f);
        space (quarterly);
        quarterly.set (ParamIDs::patchLevel, -3.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (quarterly);

        auto sub = x.make ("Severed Sub", 8, { "Bass", "Glitch", "Sub", "Split" },
            "A bass split in two: a clean, steady sine sub straight to the mix, and over it a buzzy saw an octave up on "
            "insert bus A, cut in and out by GLITCH's KILLSWITCH in sixteenths with your song (two halves of one sound "
            "that never quite meet). MACROS: BRIGHT, SPACE, DREAD (the upper half louder and grittier). MOD WHEEL: opens "
            "the filter.");
        sub.osc (0, "Wave", sine).osc (0, "Volume", 0.65f).osc (0, "PhaseRand", 0.0f)
           .osc (1, "On", 1.0f).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.3f).osc (1, "Insert", 1.0f)
           .filter (FilterType::lp24, 1200.0f, 0.25f, 0.0f).set (ParamIDs::fltKey, 0.5f)
           .ampEnvelope (5.0f, 600.0f, 1.0f, 250.0f)
           .mono (20.0f);
        clean (sub, 0);
        glitch (sub, 0, GM::killswitch, "1/16", 6.0f, 3.0f, 1.0f);
        route (sub, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (sub, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (sub, "macro3", ParamIDs::osc (1, "Volume"), 0.2f);
        route (sub, "macro3", ParamIDs::fltDrive, 0.4f);
        macros (sub, "BRIGHT", "SPACE", "DREAD");
        reverb (sub, RT::room, 0.35f, 0.05f);
        space (sub);
        sub.set (ParamIDs::patchLevel, -3.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (sub);

        auto breakRoom = x.make ("Break Room Bass", 9, { "Bass", "Sync", "Snap", "Hard" },
            "A bass with a hard, unfriendly snap: a saw hard-synced high at the start of each note and falling back over "
            "a quarter second (SYNC's own envelope), so every note barks then settles, a low filter, a clean sine "
            "underneath. MACROS: BRIGHT, SPACE, DREAD (a harsher sync and the filter's grit). MOD WHEEL: opens the "
            "filter.");
        breakRoom.osc (0, "Volume", 0.5f).osc (0, "SyncAmount", 0.1f).osc (0, "SyncEnv", 0.55f).osc (0, "SyncEnvTime", 250.0f)
                 .osc (0, "SyncEnvCurve", -0.4f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
                 .filter (FilterType::lp24, 1500.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.5f)
                 .ampEnvelope (3.0f, 500.0f, 0.9f, 200.0f)
                 .mono (10.0f);
        clean (breakRoom, 1);
        route (breakRoom, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (breakRoom, "macro1", ParamIDs::fltCutoff, 0.2f);
        route (breakRoom, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.2f);
        route (breakRoom, "macro3", ParamIDs::fltDrive, 0.4f);
        macros (breakRoom, "BRIGHT", "SPACE", "DREAD");
        reverb (breakRoom, RT::room, 0.35f, 0.05f);
        space (breakRoom);
        breakRoom.set (ParamIDs::patchLevel, -0.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (breakRoom);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto lobby = x.make ("Lobby Piano", 10, { "Keys", "Piano", "Muzak", "Radio" },
            "The soft felt piano as reception-desk muzak, heard through a small radio on the counter (LO-FI's AM RADIO, "
            "thin and boxed in), a little room. MACROS: BRIGHT, SPACE, DREAD (the radio's tape wobbles and the signal "
            "frays). The MOD WHEEL adds the felt's noise.");
        softPiano (lobby);
        lobby.set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::amRadio)
             .set (ParamIDs::lofiAmount, 3.0f).set (ParamIDs::lofiMix, 0.75f).set (ParamIDs::lofiNoise, 0.0f);
        chorus (lobby, ParamIDs::mod2Ids, MT::vibrato, 0.6f, 0.4f, 0.0f);
        route (lobby, "macro3", ParamIDs::mod2Mix, 0.6f);
        route (lobby, "macro3", ParamIDs::lofiAmount, 0.3f);
        macros (lobby, "BRIGHT", "SPACE", "DREAD");
        reverb (lobby, RT::room, 0.4f, 0.15f);
        space (lobby);
        lobby.set (ParamIDs::patchLevel, -4.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (lobby);

        auto marimba = x.make ("Incentive Marimba", 11, { "Keys", "FM", "Mallet", "Eager" },
            "A bright, eager marimba, like a reward chime for good work: a sine struck with FM at four times its pitch "
            "(the bar's bright overtone), dying fast, harder hits ringing brighter, a bright room. MACROS: BRIGHT, SPACE, "
            "DREAD (a detuned, tritone shadow under every hit). ");
        marimba.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
               .osc (0, "FmAmount", 0.05f).osc (0, "ModRate", ratio (4.0f)).osc (0, "ModGrid", 1.0f)
               .ampEnvelope (1.0f, 700.0f, 0.0f, 400.0f).set (ParamIDs::ampVel, 0.6f);
        oneShot (marimba, 0, { SP20 { 0.0f, 1.0f, -0.7f }, SP20 { 0.15f, 0.0f, 0.0f } });
        route (marimba, "env1", ParamIDs::osc (0, "FmAmount"), 0.25f);
        route (marimba, "velocity", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (marimba, "macro1", ParamIDs::osc (0, "FmAmount"), 0.1f);
        clash (marimba, 1, synth::OscWave::sine, -6.0f, 0.25f);
        macros (marimba, "BRIGHT", "SPACE", "DREAD");
        reverb (marimba, RT::room, 0.5f, 0.15f);
        space (marimba);
        marimba.set (ParamIDs::patchLevel, -3.8f);   // LEVEL: set by its peaks when a chord is struck (measured, not by ear)
        x.add (marimba);

        auto chimes = x.make ("Memo Chimes", 12, { "Keys", "Wavetable", "Travel", "Chime" },
            "Chimes whose colour changes as they ring: the Harmonic Rise table walked by TRAVEL with its +MOD strength "
            "curves, bright at the strike and falling back to a soft fundamental, each note decaying slowly, a digital "
            "space. MACROS: BRIGHT (more travel), SPACE, DREAD (a quarter-tone-flat ghost of each chime). ");
        chimes.osc (0, "Wave", wavetable).osc (0, "Table", table ("Harmonic Rise")).osc (0, "WtIndex", 0.0f).osc (0, "Volume", 0.6f)
              .osc (0, "TrOn", 1.0f).osc (0, "TrEditors", (float) synth::travel::Editors::envSeqMod).osc (0, "TrMode", (float) synth::travel::Mode::oneShot)
              .osc (0, "TrSync", (float) synth::mod::LfoSync::free).osc (0, "TrRate", 0.6f).osc (0, "TrSteps", 1.0f)
              .osc (0, "TrStrength", 0.8f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Fine", -50.0f).osc (1, "Volume", 0.0f)
              .ampEnvelope (1.0f, 2500.0f, 0.0f, 1500.0f).set (ParamIDs::ampVel, 0.5f);
        {
            auto& t = chimes.rig.modulation.travel[0];
            t.envelope = { { SP20 { 0.0f, 1.0f, -0.6f }, SP20 { 1.0f, 0.0f, 0.0f } }, -1, -1, -1 };   // bright, then back down
            t.pattern.strength[0] = { 1.0f, 0.6f, 0.2f, true, -0.4f };                                      // the travel itself fading
        }
        route (chimes, "macro1", ParamIDs::osc (0, "TrStrength"), 0.2f);
        route (chimes, "macro3", ParamIDs::osc (1, "Volume"), 0.25f);
        macros (chimes, "BRIGHT", "SPACE", "DREAD");
        reverb (chimes, RT::digital, 0.6f, 0.22f, 30.0f, 5.0f);
        space (chimes);
        chimes.set (ParamIDs::patchLevel, -5.1f);   // LEVEL: set by its peaks when a chord is struck (measured, not by ear)
        x.add (chimes);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto refine = x.make ("Data Refinement", 13, { "Arp", "Chords", "Interlock", "Precise" },
            "Two arps sorting the same chord between them: ARP 1 plays the chord you hold as stabs on the beat (CHORDS "
            "mode: the chord, then its sus and seventh versions), ARP 2 picks single notes between them in sixteenths, "
            "both on one clock (ARPS TOGETHER), with your song. MACROS: BRIGHT, SPACE, DREAD (a tritone in the stabs).");
        refine.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.4f).osc (0, "Arp", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.45f).osc (1, "Arp", 2.0f)
              .filter (FilterType::lp24, 3000.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 150.0f, 0.2f, 100.0f).set (ParamIDs::fltKey, 0.5f)
              .ampEnvelope (1.0f, 250.0f, 0.1f, 150.0f)
              .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::chords)
              .set (ParamIDs::arp (0, "Division"), division ("1/4")).set (ParamIDs::arp (0, "Gate"), 0.4f).set (ParamIDs::arp (0, "Length"), 4.0f)
              .set (ParamIDs::arp (1, "On"), 1.0f).set (ParamIDs::arp (1, "Mode"), (float) synth::arp::Mode::upDown)
              .set (ParamIDs::arp (1, "Division"), division ("1/16")).set (ParamIDs::arp (1, "Octaves"), 2.0f)
              .set (ParamIDs::arp (1, "Gate"), 0.4f)
              .set (ParamIDs::arpsTogether, 1.0f);
        {
            auto& p = refine.rig.modulation.arps[0].patterns[0];
            p = {};
            const char* chords[] = { "Minor", "Sus4", "Min7", "Sus2" };
            for (int k = 0; k < 4; ++k)
            {
                auto& b = p.blocks[(size_t) k];
                b.on = true;
                b.chord = (int) chord (chords[k]);
                b.velocity = k == 0 ? 1.0f : 0.8f;
            }
            auto& q = refine.rig.modulation.arps[1].patterns[0];
            q = synth::arp::Patterns::makeDefault().patterns[0];
            for (int k = 0; k < 16; ++k)
                q.blocks[(size_t) k].on = k % 4 != 0;   // the steps between the stabs
        }
        clash (refine, 2, synth::OscWave::pulse, 6.0f, 0.15f);
        refine.osc (2, "Arp", 1.0f);
        route (refine, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (refine, "BRIGHT", "SPACE", "DREAD");
        delay (refine, "1/8 dotted", 0.3f, 0.12f, true, 5.0f);
        reverb (refine, RT::plate, 0.5f, 0.12f);
        space (refine);
        refine.set (ParamIDs::patchLevel, -0.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (refine);

        auto sorting = x.make ("Sorting Pulse", 14, { "Arp", "Ratchet", "Slide", "Busy" },
            "A busy little arp that never stops sorting: ARP 1 runs up the chord you hold in sixteenths with your song, "
            "some steps rattled out twice or three times (RATCHET) and some sliding into the next note (SLIDE), a "
            "plucky square, crisp echoes. MACROS: BRIGHT, SPACE, DREAD (a half-step shadow on every note).");
        sorting.osc (0, "Wave", pulse).osc (0, "Width", 0.45f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f)
               .filter (FilterType::lp24, 2800.0f, 0.25f, 0.3f).filterEnvelope (0.0f, 120.0f, 0.1f, 80.0f).set (ParamIDs::fltKey, 0.5f)
               .ampEnvelope (1.0f, 200.0f, 0.2f, 100.0f)
               .set (ParamIDs::glide, 60.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
               .set (ParamIDs::arp (0, "Gate"), 0.5f);
        {
            auto& p = sorting.rig.modulation.arps[0].patterns[0];
            p = synth::arp::Patterns::makeDefault().patterns[0];
            p.blocks[3].ratchet = 2;
            p.blocks[7].slide = true;
            p.blocks[10].ratchet = 3;
            p.blocks[14].slide = true;
            p.blocks[15].ratchet = 2;
        }
        clash (sorting, 1, synth::OscWave::pulse, 1.0f, 0.15f);
        sorting.osc (1, "Arp", 1.0f);
        route (sorting, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (sorting, "BRIGHT", "SPACE", "DREAD");
        delay (sorting, "1/16 dotted", 0.3f, 0.12f, true, 5.5f);
        reverb (sorting, RT::room, 0.45f, 0.1f);
        space (sorting);
        sorting.set (ParamIDs::patchLevel, -1.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (sorting);

        auto corridor = x.make ("Cold Corridor", 15, { "Arp", "Pulse", "Tritone", "Cold" },
            "Walking a cold corridor that never ends: a cold pulse of the chord you hold, in time with your song. MACROS: "
            "BRIGHT, SPACE, DREAD (a tritone in the pulse).");
        corridor.osc (0, "Wave", triangle).osc (0, "Volume", 0.45f)
                .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.4f).osc (0, "UniDetune", 0.06f).osc (0, "UniSpread", 0.7f)
                .filter (FilterType::lp24, 1800.0f, 0.2f, 0.0f)
                .ampEnvelope (200.0f, 800.0f, 1.0f, 600.0f)
                .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.6f).set (ParamIDs::seqGate, 0.5f)
                .set (ParamIDs::seqContour, 0.3f).set (ParamIDs::seqDivision, division ("1/8")).set (ParamIDs::seqLength, 8.0f);   // short, so each swell is one beat long
        clash (corridor, 1, synth::OscWave::triangle, 6.0f, 0.15f);
        route (corridor, "macro1", ParamIDs::fltCutoff, 0.2f);
        macros (corridor, "BRIGHT", "SPACE", "DREAD");
        reverb (corridor, RT::hall, 0.8f, 0.2f);
        space (corridor);
        corridor.set (ParamIDs::patchLevel, -3.1f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        x.add (corridor);
    }

    // SLINGBLADE: inspired by a trilogy of science-fiction novels about a miner who rises against a gilded ruling
    // class, in three acts: the mines below (brutal, industrial), the gilded court at war (opulent), the dawn of the
    // rebellion (soaring). Each category has one sound from each act. Built round the WAH (0.51.0), worked by the EXPR
    // wheel on the leads. No titles, characters, places, terms or slogans from the books: our own sounds. MACRO 3 is
    // RISE on every preset: from held down to defiant, each its own way.
    void slingblade (Expansion x)
    {
        using SP21 = synth::mod::ShapePoint;
        using WM = effects::Wah::Mode;
        using WR = effects::Wah::Range;
        const auto oneShot = [] (Builder& b, int env, std::vector<SP21> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers, buses and VOICE
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };
        const auto ratio = [] (float r) { return 0.5f + std::log2 (r) / 8.0f; };   // FM RATE for a ratio
        // Bus A's WAH.
        const auto wah = [] (Builder& b, WM mode, WR range, float position, float resonance, float depth = 1.0f)
        {
            b.set (ParamIDs::ins (0, "WahOn"), 1.0f).set (ParamIDs::ins (0, "WahMode"), (float) (int) mode)
             .set (ParamIDs::ins (0, "WahRange"), (float) (int) range).set (ParamIDs::ins (0, "WahPosition"), position)
             .set (ParamIDs::ins (0, "WahResonance"), resonance).set (ParamIDs::ins (0, "WahDepth"), depth);
        };
        // The EXPR wheel (or a keyboard's expression pedal) rocking bus A's wah from `heel` to `toe`. Before it's
        // touched it reads full: the toe.
        const auto exprWah = [] (Builder& b, float heel, float toe)
        {
            b.set (ParamIDs::ins (0, "WahPosition"), heel);
            route (b, "expression", ParamIDs::ins (0, "WahPosition"), toe - heel);
        };
        // RISE's extra voice: a copy of the note (an octave up, or a fifth...) at VOLUME 0 until MACRO 3 brings it in.
        const auto rise = [] (Builder& b, int o, float wave, float semitones, float amount)
        {
            b.osc (o, "On", 1.0f).osc (o, "Wave", wave).osc (o, "Transpose", semitones).osc (o, "Volume", 0.0f);
            route (b, "macro3", ParamIDs::osc (o, "Volume"), amount);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto drill = x.make ("Drillbit Scream", 1, { "Lead", "Wah", "Sync", "Mines" },
            "The mines below: a lead that screams like a drill biting rock. A hard-synced saw (its edge snarling as each "
            "note starts) through the WAH into a RAT-style DRIVE, on insert bus A. Rock the EXPR wheel (or your "
            "keyboard's expression pedal) for the wah: heel dark and choked, toe screaming. MACROS: BRIGHT, SPACE, RISE "
            "(harder sync and an octave above). MOD WHEEL: vibrato.");
        drill.osc (0, "SyncAmount", 0.3f).osc (0, "SyncEnv", 0.3f).osc (0, "SyncEnvTime", 300.0f).osc (0, "Volume", 0.6f)
             .osc (0, "VibRate", 5.8f).osc (0, "Insert", 1.0f)
             .filter (FilterType::lp24, 5000.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.3f)
             .ampEnvelope (3.0f, 300.0f, 0.85f, 200.0f)
             .mono (30.0f);
        rise (drill, 1, saw, 12.0f, 0.25f);
        drill.osc (1, "Insert", 1.0f);
        wah (drill, WM::pedal, WR::classic, 0.08f, 6.5f);
        exprWah (drill, 0.08f, 0.95f);
        drive (drill, 0, effects::Drive::Model::rabid, 6.0f, 5.5f, 0.0f, 0.25f, 0.2f);
        route (drill, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (drill, "macro3", ParamIDs::osc (0, "SyncAmount"), 0.3f);
        route (drill, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        macros (drill, "BRIGHT", "SPACE", "RISE");
        delay (drill, "1/8 dotted", 0.3f, 0.12f, true, 4.5f);
        reverb (drill, RT::room, 0.5f, 0.12f);
        space (drill);
        drill.set (ParamIDs::patchLevel, 2.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (drill);

        auto fanfare = x.make ("Gilded Fanfare", 2, { "Lead", "Brass", "Regal", "Court" },
            "The gilded court: a regal brass lead, every note a fanfare. Two saws a few cents apart and a pulse, the "
            "fifth stacked on each note (the CHORDER), the filter's brassy blat at the start of each note, a slow "
            "vibrato, a light chorus, a hall. MACROS: BRIGHT, SPACE, RISE (a bigger blat and an octave above). MOD "
            "WHEEL: vibrato.");
        fanfare.osc (0, "Volume", 0.4f).osc (0, "Fine", -6.0f).osc (0, "Ch1On", 1.0f).osc (0, "Ch1Coarse", 7.0f).osc (0, "Ch1Vol", 0.5f)
               .osc (0, "VibRate", 5.2f).osc (0, "VibAmount", 6.0f).osc (0, "VibFade", 500.0f)
               .osc (1, "On", 1.0f).osc (1, "Volume", 0.4f).osc (1, "Fine", 6.0f).osc (1, "Ch1On", 1.0f).osc (1, "Ch1Coarse", 7.0f).osc (1, "Ch1Vol", 0.5f)
               .osc (1, "VibRate", 5.2f).osc (1, "VibAmount", 6.0f).osc (1, "VibFade", 500.0f)
               .osc (2, "On", 1.0f).osc (2, "Wave", pulse).osc (2, "Width", 0.4f).osc (2, "Volume", 0.2f)
               .filter (FilterType::lp24, 900.0f, 0.2f, 0.55f).filterEnvelope (30.0f, 450.0f, 0.45f, 300.0f).set (ParamIDs::fltKey, 0.4f)
               .ampEnvelope (25.0f, 600.0f, 0.85f, 350.0f)
               .mono (20.0f);
        rise (fanfare, 3, saw, 12.0f, 0.2f);
        route (fanfare, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (fanfare, "macro3", ParamIDs::fltEnv, 0.25f);
        route (fanfare, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        route (fanfare, "modWheel", ParamIDs::osc (1, "VibAmount"), 0.15f);
        macros (fanfare, "BRIGHT", "SPACE", "RISE");
        chorus (fanfare, ParamIDs::modIds, MT::chorus, 0.5f, 0.3f, 0.25f);
        reverb (fanfare, RT::hall, 0.7f, 0.22f);
        space (fanfare);
        fanfare.set (ParamIDs::patchLevel, -3.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (fanfare);

        auto dawn = x.make ("Dawnbreaker", 3, { "Lead", "Wah", "Supersaw", "Anthem" },
            "The dawn: a soaring anthem lead. A seven-voice supersaw that scoops up a semitone into every note (the PITCH "
            "envelope), through the WAH in its WIDE range and a light SKYLINE drive on insert bus A. Rock the EXPR wheel "
            "for vocal swells, heel to toe. MACROS: BRIGHT, SPACE, RISE (a supersaw an octave above joins). MOD WHEEL: "
            "vibrato.");
        dawn.osc (0, "Volume", 0.55f).osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.7f).osc (0, "UniDetune", 0.18f).osc (0, "UniSpread", 0.9f)
            .osc (0, "VibRate", 5.5f).osc (0, "Insert", 1.0f)
            .ampEnvelope (8.0f, 500.0f, 0.9f, 400.0f)
            .mono (40.0f)
            .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        dawn.rig.modulation.pitchEnvelope = { { SP21 { 0.0f, 0.25f, 0.0f }, SP21 { 0.12f, 0.5f, 0.0f } }, -1, -1, -1 };
        rise (dawn, 1, saw, 12.0f, 0.3f);
        dawn.osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.6f).osc (1, "UniDetune", 0.15f).osc (1, "UniSpread", 0.9f).osc (1, "Insert", 1.0f);
        wah (dawn, WM::pedal, WR::wide, 0.15f, 4.5f);
        exprWah (dawn, 0.15f, 0.9f);
        drive (dawn, 0, effects::Drive::Model::skyline, 3.0f, 6.0f, 0.0f, 0.35f, 0.35f);
        route (dawn, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.2f);
        route (dawn, "macro1", ParamIDs::osc (0, "UniDetune"), 0.1f);
        macros (dawn, "BRIGHT", "SPACE", "RISE");
        delay (dawn, "1/4 dotted", 0.35f, 0.18f, true, 4.5f);
        reverb (dawn, RT::hall, 0.8f, 0.22f);
        space (dawn);
        dawn.set (ParamIDs::patchLevel, -3.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (dawn);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto crust = x.make ("Under The Crust", 4, { "Pad", "FM", "Dark", "Mines" },
            "Deep in the tunnels: a dark drone. Saws growling with a slow FM rumble (an octave-down sine pushing their "
            "phase, swelling and fading), the wind blowing through the shafts (filtered noise, gusting with a random "
            "LFO on the filter), a long dark hall. MACROS: BRIGHT, SPACE, RISE (the filter opens and grits). MOD WHEEL: "
            "opens the filter.");
        crust.osc (0, "Volume", 0.5f).osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (0.5f)).osc (0, "FmAmount", 0.05f)
             .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.7f)
             .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.5f).osc (1, "Volume", 0.12f)
             .filter (FilterType::lp24, 340.0f, 0.35f, 0.0f).set (ParamIDs::fltKey, 0.3f)
             .ampEnvelope (1500.0f, 2000.0f, 1.0f, 2500.0f)
             .set (ParamIDs::lfo (0, "Rate"), 0.07f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global)
             .set (ParamIDs::lfo (1, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (1, "Rate"), 0.25f)
             .set (ParamIDs::lfo (1, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (crust, "lfo1", ParamIDs::osc (0, "FmAmount"), 0.06f);
        route (crust, "lfo2", ParamIDs::fltCutoff, 0.07f);
        route (crust, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (crust, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (crust, "macro3", ParamIDs::fltCutoff, 0.2f);
        route (crust, "macro3", ParamIDs::fltDrive, 0.4f);
        macros (crust, "BRIGHT", "SPACE", "RISE");
        reverb (crust, RT::hall, 0.8f, 0.25f, 30.0f, 3.0f);
        eqBand (crust, 0, eq::Shape::lowCut, 40.0f, 0.0f);
        space (crust);
        crust.set (ParamIDs::patchLevel, -8.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (crust);

        auto glass = x.make ("Glass Palace", 5, { "Pad", "Wavetable", "Shimmer", "Court" },
            "The gilded court's glass halls: a lavish, shimmering pad. The FM Sweep table drifting slowly through its "
            "frames (an LFO on INDEX), four voices wide, a quiet sine two octaves up for sparkle, the PRISM chorus, a "
            "bright plate and slow echoes. MACROS: BRIGHT, SPACE, RISE (further through the table and more sparkle). "
            "MOD WHEEL: through the table.");
        glass.osc (0, "Wave", wavetable).osc (0, "Table", table ("FM Sweep")).osc (0, "WtIndex", 0.2f).osc (0, "Volume", 0.5f)
             .osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.9f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.06f)
             .filter (FilterType::lp12, 6000.0f, 0.1f, 0.0f)
             .ampEnvelope (900.0f, 1500.0f, 1.0f, 2000.0f)
             .set (ParamIDs::lfo (0, "Rate"), 0.09f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (glass, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.15f);
        route (glass, "modWheel", ParamIDs::osc (0, "WtIndex"), 0.3f);
        route (glass, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (glass, "macro3", ParamIDs::osc (0, "WtIndex"), 0.2f);
        route (glass, "macro3", ParamIDs::osc (1, "Volume"), 0.1f);
        macros (glass, "BRIGHT", "SPACE", "RISE");
        chorus (glass, ParamIDs::modIds, MT::dimension, 0.4f, 0.5f, 0.4f);
        delay (glass, "1/4", 0.35f, 0.12f, true, 6.0f);
        reverb (glass, RT::plate, 0.8f, 0.28f, 30.0f, 6.0f);
        space (glass);
        glass.set (ParamIDs::patchLevel, -8.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glass);

        auto tide = x.make ("Rising Tide Choir", 6, { "Pad", "Choir", "Voice", "Hope" },
            "The rebellion's voice: a choir that swells from a hum into an open \"ah\" (the VOICE filter, its SIZE "
            "following the keys, so low notes are sung by basses and high ones by sopranos), two sections each with its "
            "own vibrato, a breath through it, a huge hall. MACROS: BRIGHT, SPACE, RISE (a second choir an octave up "
            "joins). MOD WHEEL: the vowel towards \"ee\".");
        tide.osc (0, "Volume", 0.5f).osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.85f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.9f)
            .osc (0, "VibMode", (float) synth::VibratoMode::independent).osc (0, "VibAmount", 10.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 700.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.25f).osc (1, "Volume", 0.35f)
            .osc (1, "UniVoices", 4.0f).osc (1, "UniMix", 0.85f).osc (1, "UniDetune", 0.08f).osc (1, "UniSpread", 0.9f)
            .osc (1, "VibMode", (float) synth::VibratoMode::independent).osc (1, "VibAmount", 12.0f).osc (1, "VibRate", 5.4f).osc (1, "VibFade", 700.0f)
            .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.5f).osc (2, "NoiseColour", 0.08f).osc (2, "NoiseWidth", 2.0f).osc (2, "Volume", 0.04f)
            .ampEnvelope (500.0f, 1500.0f, 0.95f, 2200.0f)
            .set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceSharp, 0.45f);
        rise (tide, 3, saw, 12.0f, 0.3f);
        tide.osc (3, "UniVoices", 4.0f).osc (3, "UniMix", 0.85f).osc (3, "UniDetune", 0.1f).osc (3, "UniSpread", 0.9f)
            .osc (3, "VibMode", (float) synth::VibratoMode::independent).osc (3, "VibAmount", 10.0f).osc (3, "VibRate", 5.2f);
        tide.set (ParamIDs::voiceSize, 0.0f);   // SIZE by the keys: 2 x (note / 127)^2
        for (int k = 0; k < 2; ++k)
            tide.rig.modulation.routes.push_back ({ true, "keytrack", ParamIDs::voiceSize, 1.0f, false, 0.0f, 0.5f });
        oneShot (tide, 0, { SP21 { 0.0f, 0.0f, 0.3f }, SP21 { 0.45f, 1.0f, 0.0f } });   // the hum opening into "ah"
        tide.set (ParamIDs::voiceVowel, 0.0f);
        route (tide, "env1", ParamIDs::voiceVowel, 0.5f);
        route (tide, "modWheel", ParamIDs::voiceVowel, 0.4f, 100.0f);
        route (tide, "macro1", ParamIDs::voiceSharp, 0.3f);
        macros (tide, "BRIGHT", "SPACE", "RISE");
        reverb (tide, RT::hall, 0.95f, 0.32f, 40.0f, 4.5f);
        eqBand (tide, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (tide);
        tide.set (ParamIDs::patchLevel, -16.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (tide);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto hammer = x.make ("Pick And Hammer", 7, { "Bass", "Auto-Wah", "Drive", "Mines" },
            "Machinery at work below: a gritty bass through the WAH in AUTO (its LOW range), so every note you play "
            "opens it with a growl and lets it fall back (play harder for more), then a TS808-style drive, on insert "
            "bus A; a clean sine under it holds the low end. MACROS: BRIGHT, SPACE, RISE (the wah sits further open). "
            "MOD WHEEL: opens the filter.");
        hammer.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Volume", 0.55f).osc (0, "Insert", 1.0f)
              .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
              .filter (FilterType::lp24, 3000.0f, 0.1f, 0.0f).set (ParamIDs::fltKey, 0.4f)
              .ampEnvelope (3.0f, 400.0f, 0.7f, 150.0f)
              .mono (10.0f);
        clean (hammer, 1);
        wah (hammer, WM::autoWah, WR::low, 0.05f, 6.5f, 0.85f);
        hammer.set (ParamIDs::ins (0, "WahSensitivity"), 6.0f).set (ParamIDs::ins (0, "WahSpeed"), 6.0f);
        drive (hammer, 0, effects::Drive::Model::screamer, 5.0f, 6.0f, 0.0f, 0.05f, 0.05f);
        route (hammer, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (hammer, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (hammer, "macro3", ParamIDs::ins (0, "WahPosition"), 0.3f);
        macros (hammer, "BRIGHT", "SPACE", "RISE");
        reverb (hammer, RT::room, 0.35f, 0.05f);
        space (hammer);
        hammer.set (ParamIDs::patchLevel, -2.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hammer);

        auto hull = x.make ("Warship Hull", 8, { "Bass", "Reese", "Detune", "War" },
            "The war in the void: a huge, slow-moving bass. Two saws a little apart, their beating drifting (a slow LFO "
            "on one's tuning), a low filter, a tube shaper after it, a clean sine underneath. MACROS: BRIGHT, SPACE, "
            "RISE (more grit and an opener filter). MOD WHEEL: opens the filter.");
        hull.osc (0, "Volume", 0.45f).osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 1.0f).osc (0, "UniDetune", 0.12f)
            .osc (1, "On", 1.0f).osc (1, "Volume", 0.4f).osc (1, "Fine", -10.0f)
            .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.4f).osc (2, "PhaseRand", 0.0f)
            .filter (FilterType::lp24, 450.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.5f)
            .shaper (0, synth::ShaperModel::tube, 0.2f, true)
            .ampEnvelope (10.0f, 800.0f, 1.0f, 300.0f)
            .mono (25.0f)
            .set (ParamIDs::lfo (0, "Rate"), 0.15f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        clean (hull, 2);
        route (hull, "lfo1", ParamIDs::osc (1, "Fine"), 0.05f);
        route (hull, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (hull, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (hull, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        route (hull, "macro3", ParamIDs::fltCutoff, 0.1f);
        macros (hull, "BRIGHT", "SPACE", "RISE");
        reverb (hull, RT::room, 0.4f, 0.06f);
        space (hull);
        hull.set (ParamIDs::patchLevel, -7.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (hull);

        auto uprising = x.make ("Uprising", 9, { "Bass", "Step Seq", "Pumping", "Rebellion" },
            "The rising: an aggressive, pumping bass. A held note chopped by the STEP SEQUENCER in sixteenths with your "
            "song (accents on the beat, a few steps tied into runs), a snappy filter, a soft shaper's grit, a clean "
            "sine under it. MACROS: BRIGHT, SPACE, RISE (more grit and snap). MOD WHEEL: opens the filter.");
        uprising.osc (0, "Volume", 0.5f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
                .filter (FilterType::lp24, 800.0f, 0.3f, 0.45f).filterEnvelope (0.0f, 160.0f, 0.2f, 100.0f).set (ParamIDs::fltKey, 0.5f)
                .shaper (0, synth::ShaperModel::soft, 0.2f)
                .ampEnvelope (2.0f, 400.0f, 1.0f, 150.0f)
                .mono (10.0f)
                .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 1.0f).set (ParamIDs::seqGate, 0.65f)
                .set (ParamIDs::seqContour, 0.15f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        clean (uprising, 1);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = uprising.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                const auto at = k % 16;
                step.on = at != 2 && at != 10 && at != 14;
                step.level = at % 4 == 0 ? 1.0f : 0.65f;
                step.tied = at == 6 || at == 12;
            }
        route (uprising, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (uprising, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (uprising, "macro3", ParamIDs::shp (0, "Drive"), 0.4f);
        route (uprising, "macro3", ParamIDs::fltEnv, 0.2f);
        macros (uprising, "BRIGHT", "SPACE", "RISE");
        reverb (uprising, RT::room, 0.35f, 0.05f);
        space (uprising);
        uprising.set (ParamIDs::patchLevel, -4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (uprising);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto lullaby = x.make ("Lullaby Below", 10, { "Keys", "Music Box", "FM", "Fragile" },
            "A fragile song in the dark: a little music box. A sine struck with a bright FM tine that fades in half a "
            "second (a one-shot envelope on FM AMOUNT), a quiet sine an octave up, every note dying away, a small room "
            "and soft echoes. MACROS: BRIGHT, SPACE, RISE (a brighter, harder strike). MOD WHEEL: vibrato.");
        lullaby.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "Transpose", 12.0f)
               .osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (4.0f)).osc (0, "FmAmount", 0.02f).osc (0, "VibRate", 5.0f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.15f).osc (1, "PhaseRand", 0.0f)
               .ampEnvelope (1.0f, 1800.0f, 0.0f, 600.0f).set (ParamIDs::ampVel, 0.4f);
        oneShot (lullaby, 0, { SP21 { 0.0f, 1.0f, -0.4f }, SP21 { 0.5f, 0.0f, 0.0f } });
        route (lullaby, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (lullaby, "macro1", ParamIDs::osc (1, "Volume"), 0.1f);
        route (lullaby, "macro3", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (lullaby, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        macros (lullaby, "BRIGHT", "SPACE", "RISE");
        delay (lullaby, "1/4 dotted", 0.3f, 0.12f, true, 4.0f);
        reverb (lullaby, RT::room, 0.5f, 0.18f);
        space (lullaby);
        lullaby.set (ParamIDs::patchLevel, -8.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (lullaby);

        auto harpsichord = x.make ("Ballroom Harpsichord", 11, { "Keys", "Harpsichord", "Pluck", "Court" },
            "The gilded ballroom: a bright, decadent harpsichord. Two narrow pulses a few cents apart (two strings) and "
            "a saw an octave up (the bright stop), plucked by the filter, every note dying away, a gilded hall. MACROS: "
            "BRIGHT, SPACE, RISE (the octave stop louder). MOD WHEEL: opens the filter.");
        harpsichord.osc (0, "Wave", pulse).osc (0, "Width", 0.15f).osc (0, "Volume", 0.4f).osc (0, "Fine", 4.0f)
                   .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Volume", 0.3f).osc (1, "Fine", -4.0f)
                   .osc (2, "On", 1.0f).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.18f)
                   .filter (FilterType::lp12, 4500.0f, 0.1f, 0.25f).filterEnvelope (0.0f, 400.0f, 0.3f, 300.0f).set (ParamIDs::fltKey, 0.5f)
                   .ampEnvelope (1.0f, 1500.0f, 0.0f, 300.0f).set (ParamIDs::ampVel, 0.3f);
        route (harpsichord, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (harpsichord, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (harpsichord, "macro3", ParamIDs::osc (2, "Volume"), 0.2f);
        macros (harpsichord, "BRIGHT", "SPACE", "RISE");
        reverb (harpsichord, RT::hall, 0.55f, 0.18f);
        space (harpsichord);
        harpsichord.set (ParamIDs::patchLevel, -7.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (harpsichord);

        auto victory = x.make ("Victory Bells", 12, { "Keys", "Bells", "Additive", "Triumph" },
            "The dawn of victory: huge ringing bells, built as a bell's partials are (a hum an octave down, the strike, "
            "a major third, a fifth, an octave and a twelfth: six sines), each fading at its own pace, the higher the "
            "sooner, a cathedral. MACROS: BRIGHT (more of the upper partials), SPACE, RISE (the octave and twelfth "
            "ring louder).");
        constexpr std::array<float, 6> partials { -12.0f, 0.0f, 4.0f, 7.0f, 12.0f, 19.0f };
        constexpr std::array<float, 6> fines { 0.0f, 0.0f, -6.0f, 2.0f, 0.0f, 3.0f };
        constexpr std::array<float, 6> levels { 0.35f, 0.5f, 0.25f, 0.2f, 0.25f, 0.15f };
        constexpr std::array<float, 6> seconds { 7.0f, 5.0f, 3.5f, 2.8f, 2.2f, 1.6f };
        for (int k = 0; k < 6; ++k)
        {
            victory.osc (k, "On", 1.0f).osc (k, "Wave", sine).osc (k, "Transpose", partials[(size_t) k]).osc (k, "Fine", fines[(size_t) k])
                   .osc (k, "Volume", 0.0f).osc (k, "PhaseRand", 0.0f);
            oneShot (victory, k, { SP21 { 0.0f, 1.0f, -0.6f }, SP21 { seconds[(size_t) k], 0.0f, 0.0f } });
            route (victory, ("env" + juce::String (k + 1)).toRawUTF8(), ParamIDs::osc (k, "Volume"), levels[(size_t) k] * 0.5f);
        }
        victory.ampEnvelope (1.0f, 7000.0f, 0.0f, 3000.0f).set (ParamIDs::ampVel, 0.5f);
        route (victory, "macro1", ParamIDs::osc (3, "Volume"), 0.1f);
        route (victory, "macro1", ParamIDs::osc (5, "Volume"), 0.08f);
        route (victory, "macro3", ParamIDs::osc (4, "Volume"), 0.12f);
        route (victory, "macro3", ParamIDs::osc (5, "Volume"), 0.1f);
        macros (victory, "BRIGHT", "SPACE", "RISE");
        reverb (victory, RT::cathedral, 0.65f, 0.3f, 30.0f, 5.0f);
        space (victory);
        victory.set (ParamIDs::patchLevel, -8.6f);   // LEVEL: set by its peaks when a chord is struck (measured, not by ear)
        x.add (victory);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto shift = x.make ("Machine Shift", 13, { "Arp", "FM", "Industrial", "Wah" },
            "The rhythm of labour below: the chord you hold is played by ARP 1 in sixteenths as clanging metal (FM at an "
            "off ratio), through the WAH sweeping by itself every two beats (LFO) and a RAT-style drive on insert bus A. "
            "MACROS: BRIGHT, SPACE, RISE (more clang and an opener wah).");
        shift.osc (0, "Wave", sine).osc (0, "Volume", 0.5f).osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (3.5f)).osc (0, "FmAmount", 0.2f)
             .osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Volume", 0.25f).osc (1, "Arp", 1.0f).osc (1, "Insert", 1.0f)
             .ampEnvelope (1.0f, 180.0f, 0.0f, 80.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.5f);
        wah (shift, WM::lfo, WR::classic, 0.1f, 7.0f, 0.8f);
        shift.set (ParamIDs::ins (0, "WahSync"), delaySync ("1/2"));
        drive (shift, 0, effects::Drive::Model::rabid, 4.0f, 5.0f, 0.0f, 0.2f, 0.2f);
        {
            auto& p = shift.rig.modulation.arps[0].patterns[0];
            p = {};
            for (int k = 0; k < 16; ++k)
            {
                p.blocks[(size_t) k].on = k % 8 != 7;
                p.blocks[(size_t) k].velocity = k % 4 == 0 ? 1.0f : 0.65f;
            }
        }
        route (shift, "macro1", ParamIDs::osc (1, "Width"), 0.15f);
        route (shift, "macro3", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (shift, "macro3", ParamIDs::ins (0, "WahPosition"), 0.2f);
        macros (shift, "BRIGHT", "SPACE", "RISE");
        reverb (shift, RT::room, 0.5f, 0.12f);
        space (shift);
        shift.set (ParamIDs::patchLevel, 3.4f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        x.add (shift);

        auto siege = x.make ("Siege Clock", 14, { "Arp", "Randomizer", "Tense", "Strategy" },
            "The war game's ticking clock: a tense, ever-shifting arp. Hold a key and ARP 1 plays a pattern rolled by the "
            "RANDOMIZER from the minor scale in sixteenths with your song, each note a short wooden tick (a triangle "
            "with a click of noise), echoes bouncing left and right. MACROS: BRIGHT, SPACE, RISE (a sharper, brighter "
            "tick). MOD WHEEL: opens the filter.");
        siege.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.2f).osc (1, "Volume", 0.12f).osc (1, "Arp", 1.0f)
             .filter (FilterType::lp12, 2500.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 90.0f, 0.0f, 60.0f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (1.0f, 160.0f, 0.0f, 80.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
             .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.6f);
        {
            auto& r = siege.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Minor");
            r.restChance = 0.2f;
            r.lengthChances = { 1.0f, 0.2f, 0.0f, 0.0f };
            r.octaveChances = { 0.0f, 0.2f, 1.0f, 0.5f, 0.0f };
            r.velocityLow = 0.6f;
            r.velocityHigh = 1.0f;
            r.seed = 47;
            auto& p = siege.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        route (siege, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (siege, "macro3", ParamIDs::fltEnv, 0.25f);
        route (siege, "macro3", ParamIDs::osc (1, "Volume"), 0.08f);
        route (siege, "modWheel", ParamIDs::fltCutoff, 0.2f);
        macros (siege, "BRIGHT", "SPACE", "RISE");
        delay (siege, "1/8 dotted", 0.35f, 0.18f, true, 5.0f);
        reverb (siege, RT::room, 0.5f, 0.12f);
        space (siege);
        siege.set (ParamIDs::patchLevel, 5.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (siege);

        auto fleet = x.make ("Fleet Assault", 15, { "Arp", "Arps Together", "Wah", "Epic" },
            "The final assault: three arps locked together (ARPS TOGETHER) on the chord you hold. ARP 1 races up two "
            "octaves in sixteenths on wide saws through the WAH and a SKYLINE drive (rock the EXPR wheel to sweep it), "
            "ARP 2 drives the root an octave down in eighths, ARP 3 rings a high counter-line. A big hall. MACROS: "
            "BRIGHT, SPACE, RISE (the high line louder and the saws brighter).");
        fleet.osc (0, "Volume", 0.45f).osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 0.8f)
             .osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.45f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.4f).osc (1, "Arp", 2.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", triangle).osc (2, "Transpose", 12.0f).osc (2, "Volume", 0.15f).osc (2, "Arp", 3.0f)
             .filter (FilterType::lp24, 2500.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 180.0f, 0.3f, 100.0f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (2.0f, 250.0f, 0.5f, 120.0f)
             .set (ParamIDs::arpsTogether, 1.0f);
        clean (fleet, 2);
        fleet.osc (2, "Arp", 3.0f);
        const std::array<std::pair<synth::arp::Mode, const char*>, 3> arpModes {{ { synth::arp::Mode::up, "1/16" },
                                                                                  { synth::arp::Mode::up, "1/8" },
                                                                                  { synth::arp::Mode::upDown, "1/16" } }};
        for (int k = 0; k < 3; ++k)
        {
            fleet.set (ParamIDs::arp (k, "On"), 1.0f).set (ParamIDs::arp (k, "Mode"), (float) arpModes[(size_t) k].first)
                 .set (ParamIDs::arp (k, "Division"), division (arpModes[(size_t) k].second))
                 .set (ParamIDs::arp (k, "Octaves"), k == 0 ? 2.0f : 1.0f).set (ParamIDs::arp (k, "Gate"), k == 1 ? 0.8f : 0.6f);
            auto& p = fleet.rig.modulation.arps[(size_t) k].patterns[0];
            p = {};
            const auto steps = k == 1 ? 8 : 16;
            for (int s = 0; s < steps; ++s)
            {
                p.blocks[(size_t) s].on = k == 1 ? true : k == 2 ? (s % 3 == 0) : true;
                p.blocks[(size_t) s].velocity = s % 4 == 0 ? 1.0f : 0.7f;
                if (k == 1)
                    p.blocks[(size_t) s].transpose = 0;   // the root: ARP 2 plays only the lowest key (its MODE runs up from it)
            }
        }
        fleet.set (ParamIDs::arp (1, "Length"), 8.0f);
        wah (fleet, WM::pedal, WR::wide, 0.15f, 5.0f);
        exprWah (fleet, 0.15f, 0.9f);
        drive (fleet, 0, effects::Drive::Model::skyline, 3.5f, 6.0f, 0.0f, 0.3f, 0.35f);
        route (fleet, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (fleet, "macro3", ParamIDs::osc (2, "Volume"), 0.15f);
        route (fleet, "macro3", ParamIDs::fltEnv, 0.15f);
        macros (fleet, "BRIGHT", "SPACE", "RISE");
        reverb (fleet, RT::hall, 0.75f, 0.2f);
        space (fleet);
        fleet.set (ParamIDs::patchLevel, -0.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (fleet);
    }

    // PARADOX (the owner's name): inspired by the score of a German series about four families in a small town where
    // time runs in a loop and the past, present and future are tied together: crushing sub drones, walls of noise,
    // bowed strings, a choir, ticking clocks and rain, rumbling underground passages, melancholy glitchy
    // indie-electronic. No title, characters, places, phrases or music from the series: our own sounds. Built round
    // ECHOES (0.53.0): every preset hears its own past come back (as an echo bars later, backwards, at half speed or
    // caught in a loop). MACRO 3 is ERA on every preset, starting in the middle (the present): left goes back (worn
    // tape, a wobble, the echoes older and darker), right goes forward (cold, digital: crushed and brighter).
    void paradox (Expansion x)
    {
        using SPx = synth::mod::ShapePoint;
        using EM = effects::Echoes::Mode;
        const auto oneShot = [] (Builder& b, int env, std::vector<SPx> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers and buses
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };
        const auto ratio = [] (float r) { return 0.5f + std::log2 (r) / 8.0f; };   // FM RATE for a ratio
        // ECHOES: `distance` by distanceNames (0 a beat ... 2 a bar ... 5 eight bars).
        const auto echoes = [] (Builder& b, EM mode, int distance, float mix, float feedback, float age)
        {
            b.set (ParamIDs::echoOn, 1.0f).set (ParamIDs::echoMode, (float) (int) mode).set (ParamIDs::echoDistance, (float) distance)
             .set (ParamIDs::echoMix, mix).set (ParamIDs::echoFeedback, feedback).set (ParamIDs::echoAge, age);
        };
        // ERA (MACRO 3), loaded at the middle: the present. `slide` sets a knob to `past` with the macro at the left and
        // `present` in the middle (and on in a straight line to the right); `span` is the knob's range (Volume: 2,
        // vibrato: 100 cents). `future` adds `amount` of a knob's travel, coming in only towards the right (curve: the
        // macro to the 4th power, so 6% of it in the middle).
        const auto slide = [] (Builder& b, const juce::String& id, float past, float present, float span = 1.0f)
        {
            b.set (id, past);
            b.rig.modulation.routes.push_back ({ true, "macro3", id, 2.0f * (present - past) / span, false, 0.0f, 0.0f });
        };
        const auto future = [] (Builder& b, const juce::String& id, float amount)
        {
            b.rig.modulation.routes.push_back ({ true, "macro3", id, amount, false, 0.0f, 1.0f });
        };
        // The past's worn tape: the LO-FI's cassette (no hiss: the presets must go silent), all there at the left, gone by
        // the middle; the echoes older; a tape wobble on `wobbly` oscillators.
        const auto era = [slide] (Builder& b, float tape, float agePast, float agePresent, std::initializer_list<int> wobbly, float wobbleCents = 9.0f)
        {
            b.set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
             .set (ParamIDs::lofiNoise, 0.0f).set (ParamIDs::lofiAmount, 6.0f).set (ParamIDs::macros[2], 0.5f);
            slide (b, ParamIDs::lofiMix, tape, 0.0f);
            slide (b, ParamIDs::echoAge, agePast, agePresent);
            for (const auto o : wobbly)
            {
                b.osc (o, "VibRate", 0.7f);
                slide (b, ParamIDs::osc (o, "VibAmount"), wobbleCents, 0.0f, 100.0f);
            }
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto voice = x.make ("Yesterday's Voice", 1, { "Lead", "Echoes", "Reverse", "Melancholy" },
            "A lonely, hollow lead that answers itself from the past: ECHOES in REVERSE, so each bar you play comes back "
            "backwards under the next, swelling into its own beginning. A narrow pulse and a quiet saw an octave down, a "
            "soft filter, a slow vibrato. MACROS: BRIGHT, SPACE, ERA (left: worn tape, a wobble, older echoes; right: "
            "cold and crushed). MOD WHEEL: vibrato.");
        voice.osc (0, "Wave", pulse).osc (0, "Width", 0.3f).osc (0, "Volume", 0.55f)
             .osc (1, "On", 1.0f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.18f)
             .filter (FilterType::lp24, 2200.0f, 0.2f, 0.2f).filterEnvelope (40.0f, 600.0f, 0.5f, 400.0f).set (ParamIDs::fltKey, 0.4f)
             .ampEnvelope (30.0f, 600.0f, 0.85f, 500.0f)
             .mono (40.0f);
        echoes (voice, EM::reverse, 2, 0.5f, 0.15f, 0.3f);
        era (voice, 0.6f, 0.8f, 0.3f, { 1 });   // the wobble on the octave below (the mod wheel's vibrato is on the pulse)
        future (voice, ParamIDs::osc (0, "Crush"), 0.45f);
        future (voice, ParamIDs::fltRes, 0.25f);
        route (voice, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (voice, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        voice.osc (0, "VibRate", 5.2f);
        macros (voice, "BRIGHT", "SPACE", "ERA");
        reverb (voice, RT::hall, 0.7f, 0.18f);
        space (voice);
        voice.set (ParamIDs::patchLevel, -4.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (voice);

        auto loop = x.make ("Caught In The Loop", 2, { "Lead", "Echoes", "Loop", "Wavetable" },
            "A lead that gets caught in time: ECHOES in LOOP grabs the bar you just played and repeats it twice under "
            "you while you play on, then grabs the next. The Analog Morph table, three voices wide, gliding. MACROS: "
            "BRIGHT, SPACE, ERA (left: worn tape and a duller wave; right: further through the table, cold and crushed). "
            "MOD WHEEL: through the table.");
        loop.osc (0, "Wave", wavetable).osc (0, "Table", table ("Analog Morph")).osc (0, "Volume", 0.55f)
            .osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.6f)
            .filter (FilterType::lp24, 3500.0f, 0.15f, 0.0f).set (ParamIDs::fltKey, 0.3f)
            .ampEnvelope (8.0f, 400.0f, 0.8f, 300.0f)
            .mono (60.0f);
        echoes (loop, EM::loop, 2, 0.45f, 0.15f, 0.25f);
        era (loop, 0.5f, 0.7f, 0.25f, { 0 });
        slide (loop, ParamIDs::osc (0, "WtIndex"), 0.0f, 0.3f);
        future (loop, ParamIDs::osc (0, "Crush"), 0.4f);
        route (loop, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (loop, "modWheel", ParamIDs::osc (0, "WtIndex"), 0.3f);
        macros (loop, "BRIGHT", "SPACE", "ERA");
        reverb (loop, RT::plate, 0.6f, 0.15f);
        space (loop);
        loop.set (ParamIDs::patchLevel, -0.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (loop);

        auto ghost = x.make ("Slow Tape Ghost", 3, { "Lead", "Echoes", "Tape", "FM" },
            "A glassy FM lead with a ghost: ECHOES in TAPE plays the start of each phrase back at half speed, an "
            "octave down and slurred, under the next. A sine pushed by a sine an octave up, the push fading after each "
            "note starts. MACROS: BRIGHT, SPACE, ERA (left: worn tape and a wobble; right: a colder, harder FM edge, "
            "crushed). MOD WHEEL: vibrato.");
        ghost.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
             .osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (2.0f)).osc (0, "FmAmount", 0.12f)
             .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Volume", 0.25f).osc (1, "Fine", 6.0f)
             .ampEnvelope (10.0f, 800.0f, 0.75f, 450.0f)
             .mono (35.0f);
        oneShot (ghost, 0, { SPx { 0.0f, 1.0f, -0.4f }, SPx { 0.6f, 0.0f, 0.0f } });
        route (ghost, "env1", ParamIDs::osc (0, "FmAmount"), 0.12f);
        echoes (ghost, EM::tape, 1, 0.45f, 0.1f, 0.35f);
        era (ghost, 0.6f, 0.8f, 0.35f, { 1 });
        future (ghost, ParamIDs::osc (0, "FmAmount"), 0.3f);
        future (ghost, ParamIDs::osc (1, "Crush"), 0.5f);
        route (ghost, "macro1", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (ghost, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        ghost.osc (0, "VibRate", 5.0f);
        macros (ghost, "BRIGHT", "SPACE", "ERA");
        delay (ghost, "1/4 dotted", 0.25f, 0.1f, true, 4.0f);
        reverb (ghost, RT::hall, 0.75f, 0.2f);
        space (ghost);
        ghost.set (ParamIDs::patchLevel, -4.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (ghost);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto wall = x.make ("Wall Of Static", 4, { "Pad", "Echoes", "Noise", "Drone" },
            "A wall of sound: seven wide saws under a roar of filtered noise, slowly breathing (an LFO on the filter), and "
            "ECHOES bringing back what was here a bar ago, worn darker each time round. MACROS: BRIGHT, SPACE, ERA "
            "(left: worn tape and older echoes; right: the saws crushed, the noise harsher). MOD WHEEL: opens the filter.");
        wall.osc (0, "Volume", 0.45f).osc (0, "UniVoices", 7.0f).osc (0, "UniMix", 0.8f).osc (0, "UniDetune", 0.15f).osc (0, "UniSpread", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 1.0f).osc (1, "NoiseColour", 0.3f).osc (1, "NoiseWidth", 2.0f).osc (1, "Volume", 0.3f)
            .filter (FilterType::lp24, 1100.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.2f)
            .ampEnvelope (1800.0f, 2000.0f, 1.0f, 2500.0f)
            .set (ParamIDs::lfo (0, "Rate"), 0.08f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (wall, "lfo1", ParamIDs::fltCutoff, 0.08f);
        echoes (wall, EM::echo, 2, 0.35f, 0.1f, 0.5f);
        era (wall, 0.7f, 0.85f, 0.5f, { 0 }, 6.0f);
        future (wall, ParamIDs::osc (0, "Crush"), 0.35f);
        future (wall, ParamIDs::osc (1, "NoiseColour"), -0.3f);
        route (wall, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (wall, "modWheel", ParamIDs::fltCutoff, 0.25f);
        macros (wall, "BRIGHT", "SPACE", "ERA");
        reverb (wall, RT::hall, 0.9f, 0.25f, 30.0f, 3.5f);
        eqBand (wall, 0, eq::Shape::lowCut, 50.0f, 0.0f);
        space (wall);
        wall.set (ParamIDs::patchLevel, -10.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (wall);

        auto bowed = x.make ("Bowed In Reverse", 5, { "Pad", "Echoes", "Strings", "Reverse" },
            "Bowed strings that breathe backwards: a string section (saws in two sections, each player's vibrato its own, "
            "a slow bow) with ECHOES in REVERSE, so every bar swells back in reverse under the next. MACROS: BRIGHT, "
            "SPACE, ERA (left: worn tape, a warble; right: a cold glassy layer joins, crushed). MOD WHEEL: opens the "
            "filter.");
        bowed.osc (0, "Volume", 0.45f).osc (0, "UniVoices", 4.0f).osc (0, "UniMix", 0.8f).osc (0, "UniDetune", 0.08f).osc (0, "UniSpread", 0.8f)
             .osc (0, "VibMode", (float) synth::VibratoMode::independent).osc (0, "VibAmount", 8.0f).osc (0, "VibRate", 5.2f).osc (0, "VibFade", 600.0f)
             .osc (1, "On", 1.0f).osc (1, "Volume", 0.35f).osc (1, "Transpose", -12.0f).osc (1, "UniVoices", 3.0f).osc (1, "UniMix", 0.8f)
             .osc (1, "UniDetune", 0.07f).osc (1, "UniSpread", 0.6f)
             .osc (1, "VibMode", (float) synth::VibratoMode::independent).osc (1, "VibAmount", 7.0f).osc (1, "VibRate", 4.8f).osc (1, "VibFade", 600.0f)
             .osc (2, "On", 1.0f).osc (2, "Wave", wavetable).osc (2, "Table", table ("Harmonic Rise")).osc (2, "WtIndex", 0.8f).osc (2, "Transpose", 12.0f)
             .osc (2, "Volume", 0.0f)
             .filter (FilterType::lp12, 2600.0f, 0.1f, 0.2f).filterEnvelope (600.0f, 1500.0f, 0.7f, 1200.0f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (700.0f, 1500.0f, 0.9f, 1500.0f);
        echoes (bowed, EM::reverse, 2, 0.5f, 0.05f, 0.35f);
        era (bowed, 0.5f, 0.75f, 0.35f, { 1 }, 12.0f);
        future (bowed, ParamIDs::osc (2, "Volume"), 0.15f);
        future (bowed, ParamIDs::osc (2, "Crush"), 0.5f);
        route (bowed, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (bowed, "modWheel", ParamIDs::fltCutoff, 0.2f);
        macros (bowed, "BRIGHT", "SPACE", "ERA");
        reverb (bowed, RT::hall, 0.85f, 0.25f, 30.0f, 4.0f);
        space (bowed);
        bowed.set (ParamIDs::patchLevel, -9.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (bowed);

        auto choir = x.make ("Choir Of Ages", 6, { "Pad", "Echoes", "Choir", "Voice" },
            "A choir singing across time: two sections through the VOICE filter (an \"oh\" opening towards \"ah\", basses "
            "below middle C and sopranos above), and ECHOES in TAPE bringing the start of each bar back at half speed, an "
            "octave down: a second, older choir under the first. MACROS: BRIGHT, SPACE, ERA (left: worn tape, a softer, older "
            "choir; right: a colder, sharper choir, crushed). MOD WHEEL: the vowel towards \"ee\".");
        choir.osc (0, "Volume", 0.5f).osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.85f).osc (0, "UniDetune", 0.1f).osc (0, "UniSpread", 0.9f)
             .osc (0, "VibMode", (float) synth::VibratoMode::independent).osc (0, "VibAmount", 10.0f).osc (0, "VibRate", 5.0f).osc (0, "VibFade", 700.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.25f).osc (1, "Volume", 0.3f)
             .osc (1, "UniVoices", 4.0f).osc (1, "UniMix", 0.85f).osc (1, "UniDetune", 0.08f).osc (1, "UniSpread", 0.9f)
             .osc (1, "VibMode", (float) synth::VibratoMode::independent).osc (1, "VibAmount", 12.0f).osc (1, "VibRate", 5.4f).osc (1, "VibFade", 700.0f)
             .ampEnvelope (600.0f, 1500.0f, 0.95f, 2000.0f)
             .set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceSize, 0.0f).set (ParamIDs::voiceVowel, 0.25f);
        for (int k = 0; k < 2; ++k)   // SIZE by the keys: 2 x (note / 127)^2
            choir.rig.modulation.routes.push_back ({ true, "keytrack", ParamIDs::voiceSize, 1.0f, false, 0.0f, 0.5f });
        oneShot (choir, 0, { SPx { 0.0f, 0.0f, 0.3f }, SPx { 0.8f, 1.0f, 0.0f } });
        route (choir, "env1", ParamIDs::voiceVowel, 0.25f);
        route (choir, "modWheel", ParamIDs::voiceVowel, 0.4f, 100.0f);
        echoes (choir, EM::tape, 1, 0.4f, 0.05f, 0.4f);
        era (choir, 0.5f, 0.8f, 0.4f, {});
        slide (choir, ParamIDs::voiceSharp, 0.15f, 0.45f);
        future (choir, ParamIDs::osc (1, "Crush"), 0.45f);
        route (choir, "macro1", ParamIDs::voiceSharp, 0.25f);
        macros (choir, "BRIGHT", "SPACE", "ERA");
        reverb (choir, RT::cathedral, 0.85f, 0.3f, 40.0f, 4.5f);
        eqBand (choir, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (choir);
        choir.set (ParamIDs::patchLevel, -12.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (choir);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto pressure = x.make ("Pressure Below", 7, { "Bass", "Echoes", "Sub", "Drone" },
            "A crushing sub drone, the weight of the ground overhead: a clean sine sub under a growling saw through a tube "
            "shaper and a low filter, and ECHOES bringing each note back a beat later, darker, a low rumble under the "
            "next. MACROS: BRIGHT, SPACE, ERA (left: worn tape; right: the growl harder and crushed). MOD WHEEL: opens "
            "the filter.");
        pressure.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
                .osc (1, "On", 1.0f).osc (1, "Volume", 0.4f).osc (1, "UniVoices", 2.0f).osc (1, "UniMix", 1.0f).osc (1, "UniDetune", 0.1f)
                .filter (FilterType::lp24, 320.0f, 0.25f, 0.2f).filterEnvelope (5.0f, 500.0f, 0.4f, 300.0f).set (ParamIDs::fltKey, 0.4f)
                .shaper (0, synth::ShaperModel::tube, 0.3f, true)
                .ampEnvelope (15.0f, 800.0f, 1.0f, 400.0f)
                .mono (30.0f);
        clean (pressure, 0);
        echoes (pressure, EM::echo, 0, 0.2f, 0.15f, 0.6f);
        era (pressure, 0.5f, 0.85f, 0.6f, {});
        future (pressure, ParamIDs::shp (0, "Drive"), 0.4f);
        future (pressure, ParamIDs::osc (1, "Crush"), 0.4f);
        route (pressure, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (pressure, "modWheel", ParamIDs::fltCutoff, 0.2f);
        macros (pressure, "BRIGHT", "SPACE", "ERA");
        reverb (pressure, RT::room, 0.4f, 0.06f);
        space (pressure);
        pressure.set (ParamIDs::patchLevel, -7.1f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pressure);

        auto pulseBass = x.make ("Decay Pulse", 8, { "Bass", "Echoes", "Step Seq", "Pulse" },
            "A nervous, pulsing bass: a held note chopped by the STEP SEQUENCER in sixteenths with your song, a snappy "
            "filter, a clean sine under it, and ECHOES bringing the pulse back two beats later, a shadow out of step. "
            "MACROS: BRIGHT, SPACE, ERA (left: worn tape and a wobble; right: crushed, with a sharper resonance). MOD "
            "WHEEL: opens the filter.");
        pulseBass.osc (0, "Wave", pulse).osc (0, "Width", 0.4f).osc (0, "Volume", 0.5f)
                 .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
                 .filter (FilterType::lp24, 700.0f, 0.3f, 0.45f).filterEnvelope (0.0f, 150.0f, 0.2f, 100.0f).set (ParamIDs::fltKey, 0.5f)
                 .ampEnvelope (2.0f, 400.0f, 1.0f, 150.0f)
                 .mono (10.0f)
                 .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 1.0f).set (ParamIDs::seqGate, 0.55f)
                 .set (ParamIDs::seqContour, 0.15f).set (ParamIDs::seqDivision, division ("1/16")).set (ParamIDs::seqLength, 16.0f);
        clean (pulseBass, 1);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = pulseBass.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                const auto at = k % 16;
                step.on = at != 3 && at != 7 && at != 11 && at != 13;
                step.level = at % 4 == 0 ? 1.0f : 0.6f;
            }
        echoes (pulseBass, EM::echo, 1, 0.25f, 0.15f, 0.4f);
        era (pulseBass, 0.5f, 0.75f, 0.4f, { 0 });
        future (pulseBass, ParamIDs::osc (0, "Crush"), 0.4f);
        future (pulseBass, ParamIDs::fltRes, 0.3f);
        route (pulseBass, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (pulseBass, "modWheel", ParamIDs::fltCutoff, 0.15f);
        macros (pulseBass, "BRIGHT", "SPACE", "ERA");
        reverb (pulseBass, RT::room, 0.35f, 0.05f);
        space (pulseBass);
        pulseBass.set (ParamIDs::patchLevel, -1.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pulseBass);

        auto rumble = x.make ("Underground Rumble", 9, { "Bass", "Echoes", "Reese", "Reverse" },
            "Deep underground: a slow, churning bass (two saws drifting in and out of tune with each other), a clean sine "
            "beneath, and ECHOES in REVERSE, so each half bar rumbles back in reverse under the next. MACROS: BRIGHT, "
            "SPACE, ERA (left: worn tape; right: folded and crushed, cold). MOD WHEEL: opens the filter.");
        rumble.osc (0, "Volume", 0.45f).osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 1.0f).osc (0, "UniDetune", 0.12f)
              .osc (1, "On", 1.0f).osc (1, "Volume", 0.4f).osc (1, "Fine", -9.0f)
              .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.4f).osc (2, "PhaseRand", 0.0f)
              .filter (FilterType::lp24, 400.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.5f)
              .shaper (0, synth::ShaperModel::sineFold, 0.0f, true)
              .ampEnvelope (10.0f, 800.0f, 1.0f, 300.0f)
              .mono (25.0f)
              .set (ParamIDs::lfo (0, "Rate"), 0.12f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        clean (rumble, 2);
        route (rumble, "lfo1", ParamIDs::osc (1, "Fine"), 0.05f);
        echoes (rumble, EM::reverse, 1, 0.3f, 0.1f, 0.5f);
        era (rumble, 0.5f, 0.85f, 0.5f, {});
        future (rumble, ParamIDs::shp (0, "Drive"), 0.35f);
        future (rumble, ParamIDs::osc (0, "Crush"), 0.35f);
        route (rumble, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (rumble, "modWheel", ParamIDs::fltCutoff, 0.15f);
        macros (rumble, "BRIGHT", "SPACE", "ERA");
        reverb (rumble, RT::room, 0.4f, 0.06f);
        space (rumble);
        rumble.set (ParamIDs::patchLevel, -7.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (rumble);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto upright = x.make ("Worn Upright", 10, { "Keys", "Echoes", "Piano", "Felt" },
            "A soft, felted upright in an empty house, and what you played a bar ago coming back to you through ECHOES, "
            "a little more worn each time. MACROS: BRIGHT, SPACE, ERA (left: an old cassette of it, wobbling, the echoes "
            "faded; right: a colder, metallic strike, crushed). MOD WHEEL: more of the felt's noise.");
        feltPiano (upright);
        upright.rig.modulation.routes.erase (std::remove_if (upright.rig.modulation.routes.begin(), upright.rig.modulation.routes.end(),
            [] (const ModRouteData& r) { return r.source == "macro3"; }), upright.rig.modulation.routes.end());   // ERA's instead
        echoes (upright, EM::echo, 2, 0.3f, 0.15f, 0.45f);
        era (upright, 0.6f, 0.9f, 0.45f, { 1, 2 }, 7.0f);
        future (upright, ParamIDs::osc (0, "FmAmount"), 0.2f);
        future (upright, ParamIDs::osc (2, "Crush"), 0.5f);
        macros (upright, "BRIGHT", "SPACE", "ERA");
        reverb (upright, RT::room, 0.6f, 0.18f);
        space (upright);
        upright.set (ParamIDs::patchLevel, -3.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (upright);

        auto musicBox = x.make ("Music Box Memory", 11, { "Keys", "Echoes", "Music Box", "Reverse" },
            "A music box found in an attic: a sine struck with a bright FM tine fading in half a second, a quiet sine two "
            "octaves up, and ECHOES in REVERSE, each bar coming back backwards, as if the tune were rewinding. MACROS: "
            "BRIGHT, SPACE, ERA (left: worn tape and a wobble; right: a colder chime, crushed). MOD WHEEL: vibrato.");
        musicBox.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "Transpose", 12.0f)
                .osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (4.0f)).osc (0, "FmAmount", 0.02f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 24.0f).osc (1, "Volume", 0.12f).osc (1, "PhaseRand", 0.0f)
                .ampEnvelope (1.0f, 1800.0f, 0.0f, 600.0f).set (ParamIDs::ampVel, 0.4f);
        oneShot (musicBox, 0, { SPx { 0.0f, 1.0f, -0.4f }, SPx { 0.5f, 0.0f, 0.0f } });
        route (musicBox, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        echoes (musicBox, EM::reverse, 2, 0.45f, 0.15f, 0.3f);
        era (musicBox, 0.6f, 0.8f, 0.3f, { 1 }, 12.0f);
        future (musicBox, ParamIDs::osc (1, "Crush"), 0.5f);
        future (musicBox, ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (musicBox, "macro1", ParamIDs::osc (1, "Volume"), 0.1f);
        route (musicBox, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.15f);
        musicBox.osc (0, "VibRate", 5.0f);
        macros (musicBox, "BRIGHT", "SPACE", "ERA");
        reverb (musicBox, RT::room, 0.5f, 0.18f);
        space (musicBox);
        musicBox.set (ParamIDs::patchLevel, -6.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (musicBox);

        auto organ = x.make ("Chapel Organ", 12, { "Keys", "Echoes", "Organ", "Tape" },
            "A small chapel organ (four sines: the note, an octave, a twelfth and two octaves up, as drawbars) through a "
            "slow rotary speaker, and ECHOES in TAPE: the start of each phrase comes back at half speed, an octave down, "
            "like a memory of the hymn. MACROS: BRIGHT (more of the upper drawbars), SPACE, ERA (left: worn tape and a "
            "wow; right: cold, crushed). MOD WHEEL: the rotary's fast speed.");
        constexpr std::array<float, 4> bars { 0.0f, 12.0f, 19.0f, 24.0f };
        constexpr std::array<float, 4> barLevels { 0.5f, 0.35f, 0.18f, 0.12f };
        for (int k = 0; k < 4; ++k)
            organ.osc (k, "On", 1.0f).osc (k, "Wave", sine).osc (k, "Transpose", bars[(size_t) k]).osc (k, "Volume", barLevels[(size_t) k])
                 .osc (k, "PhaseRand", 0.0f);
        organ.ampEnvelope (8.0f, 300.0f, 1.0f, 200.0f);
        chorus (organ, ParamIDs::modIds, MT::rotary, 0.2f, 0.6f, 0.6f);
        echoes (organ, EM::tape, 1, 0.35f, 0.05f, 0.4f);
        era (organ, 0.6f, 0.8f, 0.4f, { 0, 1 }, 8.0f);
        future (organ, ParamIDs::osc (0, "Crush"), 0.45f);
        future (organ, ParamIDs::osc (1, "Crush"), 0.45f);
        route (organ, "macro1", ParamIDs::osc (2, "Volume"), 0.1f);
        route (organ, "macro1", ParamIDs::osc (3, "Volume"), 0.08f);
        route (organ, "modWheel", ParamIDs::modRate, 0.4f);
        macros (organ, "BRIGHT", "SPACE", "ERA");
        reverb (organ, RT::cathedral, 0.6f, 0.25f);
        space (organ);
        organ.set (ParamIDs::patchLevel, -5.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (organ);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto tick = x.make ("Second Hand", 13, { "Arp", "Echoes", "Loop", "Clock" },
            "A ticking clock that loses its place: ARP 1 plays the chord you hold in sixteenths as short wooden ticks (a "
            "triangle with a click of noise), and ECHOES in LOOP catches two beats and repeats them twice under the next, "
            "so time stutters. MACROS: BRIGHT, SPACE, ERA (left: worn tape; right: cold FM ticks, crushed). MOD WHEEL: "
            "opens the filter.");
        tick.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f)
            .osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (3.0f)).osc (0, "FmAmount", 0.0f)
            .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.15f).osc (1, "Volume", 0.12f).osc (1, "Arp", 1.0f)
            .filter (FilterType::lp12, 2800.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 80.0f, 0.0f, 60.0f).set (ParamIDs::fltKey, 0.5f)
            .ampEnvelope (1.0f, 140.0f, 0.0f, 70.0f)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
            .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.5f);
        echoes (tick, EM::loop, 1, 0.45f, 0.15f, 0.25f);
        era (tick, 0.6f, 0.75f, 0.25f, {});
        future (tick, ParamIDs::osc (0, "FmAmount"), 0.35f);
        future (tick, ParamIDs::osc (0, "Crush"), 0.4f);
        route (tick, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (tick, "modWheel", ParamIDs::fltCutoff, 0.2f);
        macros (tick, "BRIGHT", "SPACE", "ERA");
        reverb (tick, RT::room, 0.5f, 0.12f);
        space (tick);
        tick.set (ParamIDs::patchLevel, 4.2f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (tick);

        auto rain = x.make ("Rain On The Window", 14, { "Arp", "Echoes", "Randomizer", "Rain" },
            "Rain on a window at night: hold a chord and ARP 1 plays notes the RANDOMIZER rolled from the minor scale, "
            "soft glassy drops (a sine with a quick FM ping) scattered across two octaves and the stereo field, and "
            "ECHOES bringing back what fell two beats ago. MACROS: BRIGHT, SPACE, ERA (left: worn tape; right: colder, "
            "harder drops, crushed). MOD WHEEL: brighter drops.");
        rain.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f).osc (0, "Random", 0.5f)
            .osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (3.5f)).osc (0, "FmAmount", 0.0f)
            .ampEnvelope (1.0f, 400.0f, 0.0f, 250.0f).set (ParamIDs::ampVel, 0.5f)
            .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::sampleHold).set (ParamIDs::lfo (0, "Rate"), 6.0f)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
            .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.6f);
        oneShot (rain, 0, { SPx { 0.0f, 1.0f, -0.5f }, SPx { 0.08f, 0.0f, 0.0f } });
        route (rain, "env1", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (rain, "lfo1", ParamIDs::ampPan, 0.6f);
        {
            auto& r = rain.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Minor");
            r.restChance = 0.35f;
            r.lengthChances = { 1.0f, 0.3f, 0.0f, 0.0f };
            r.octaveChances = { 0.0f, 0.3f, 1.0f, 0.6f, 0.0f };
            r.velocityLow = 0.4f;
            r.velocityHigh = 0.9f;
            r.seed = 33;
            auto& p = rain.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        echoes (rain, EM::echo, 1, 0.3f, 0.2f, 0.4f);
        era (rain, 0.6f, 0.8f, 0.4f, {});
        future (rain, ParamIDs::osc (0, "Crush"), 0.4f);
        future (rain, ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (rain, "macro1", ParamIDs::osc (0, "FmAmount"), 0.08f);
        route (rain, "modWheel", ParamIDs::osc (0, "FmAmount"), 0.12f);
        macros (rain, "BRIGHT", "SPACE", "ERA");
        reverb (rain, RT::hall, 0.7f, 0.25f);
        space (rain);
        rain.set (ParamIDs::patchLevel, 2.4f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (rain);

        auto boot = x.make ("Bootstrap Sequence", 15, { "Arp", "Echoes", "Glitch", "Pluck" },
            "Melancholy, glitchy electronica: ARP 1 plays the chord you hold as a plucked, filtered pulse in eighths, "
            "GLITCH stuttering it now and then on insert bus A, and ECHOES in REVERSE bringing every two beats back "
            "backwards. MACROS: BRIGHT, SPACE, ERA (left: worn tape; right: cold, crushed). MOD WHEEL: opens the "
            "filter.");
        boot.osc (0, "Wave", pulse).osc (0, "Width", 0.35f).osc (0, "Volume", 0.5f).osc (0, "Arp", 1.0f).osc (0, "Insert", 1.0f)
            .osc (1, "On", 1.0f).osc (1, "Wave", triangle).osc (1, "Transpose", 12.0f).osc (1, "Volume", 0.2f).osc (1, "Arp", 1.0f).osc (1, "Insert", 1.0f)
            .filter (FilterType::lp24, 1500.0f, 0.25f, 0.4f).filterEnvelope (0.0f, 200.0f, 0.1f, 120.0f).set (ParamIDs::fltKey, 0.5f)
            .ampEnvelope (1.0f, 300.0f, 0.0f, 150.0f)
            .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
            .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
            .set (ParamIDs::arp (0, "Gate"), 0.6f);
        glitch (boot, 0, effects::Glitch::Mode::sequence, "1/16", 0.25f, 0.3f, 0.6f);
        echoes (boot, EM::reverse, 1, 0.35f, 0.15f, 0.35f);
        era (boot, 0.6f, 0.75f, 0.35f, { 1 });
        future (boot, ParamIDs::osc (0, "Crush"), 0.4f);
        future (boot, ParamIDs::fltRes, 0.25f);
        route (boot, "macro1", ParamIDs::fltCutoff, 0.15f);
        route (boot, "modWheel", ParamIDs::fltCutoff, 0.2f);
        macros (boot, "BRIGHT", "SPACE", "ERA");
        reverb (boot, RT::room, 0.55f, 0.15f);
        space (boot);
        boot.set (ParamIDs::patchLevel, 0.6f);   // LEVEL: re-measured without the drums (0.54.0; measured, not by ear)
        x.add (boot);
    }

    // MELTDOWN (the owner's name): inspired by the score of a 2019 series about a nuclear disaster, made almost
    // entirely from recordings inside a power plant: pressure drones, groaning and ringing metal, pumps breathing,
    // a counter's crackle, a lone voice rising out of the noise. Invisible danger, dread, grief. No title, places,
    // people, terms or music from the series: our own sounds, built from the synth (no recordings). Built round
    // CRACKLE (0.56.0); MACRO 3 is DOSE on every preset: at 0 clean but for a rare faint tick, turned up the clicks
    // thicken into a roar and the sound corrodes, each its own way.
    void meltdown (Expansion x)
    {
        using SPm = synth::mod::ShapePoint;
        using EMm = effects::Echoes::Mode;
        const auto oneShot = [] (Builder& b, int env, std::vector<SPm> points)
        {
            b.set (ParamIDs::env (env, "Mode"), (float) synth::mod::EnvelopeMode::oneShot);
            b.rig.modulation.envelopes[(size_t) env] = { std::move (points), -1, -1, -1 };
        };
        const auto clean = [] (Builder& b, int o)   // off the filter, shapers, buses and VOICE
        {
            b.osc (o, "Filter", 0.0f).osc (o, "Insert", 0.0f).osc (o, "Voice", 0.0f);
            for (const auto* shp : { "Shp1", "Shp2", "Shp3", "Shp4" })
                b.osc (o, shp, 0.0f);
        };
        const auto ratio = [] (float r) { return 0.5f + std::log2 (r) / 8.0f; };   // FM RATE for a ratio
        // DOSE (MACRO 3): CRACKLE following the sound, a rare faint tick at 0; turned up, up to about 330 clicks a
        // second, louder (about -11 dB), more bunched. `toneHz` the clicks' pitch for the preset.
        const auto dose = [] (Builder& b, float toneHz, float baseRate = 1.0f, float baseLevel = -30.0f, float levelRise = 0.35f)
        {
            b.set (ParamIDs::crackleOn, 1.0f).set (ParamIDs::crackleRate, baseRate).set (ParamIDs::crackleCluster, 0.5f)
             .set (ParamIDs::crackleTone, toneHz).set (ParamIDs::crackleWidth, 0.8f).set (ParamIDs::crackleFollow, 1.0f)
             .set (ParamIDs::crackleLevel, baseLevel);
            route (b, "macro3", ParamIDs::crackleRate, 0.7f);
            route (b, "macro3", ParamIDs::crackleLevel, levelRise);
            route (b, "macro3", ParamIDs::crackleCluster, 0.3f);
        };
        // DOSE's wear: LO-FI's cassette coming in (no hiss: the presets must go silent).
        const auto wear = [] (Builder& b, float amount)
        {
            b.set (ParamIDs::lofiOn, 1.0f).set (ParamIDs::lofiType, (float) (int) effects::LoFi::Type::cassette)
             .set (ParamIDs::lofiNoise, 0.0f).set (ParamIDs::lofiAmount, 6.0f).set (ParamIDs::lofiMix, 0.0f);
            route (b, "macro3", ParamIDs::lofiMix, amount);
        };
        // DOSE's sickness: a random drift on an oscillator's pitch (vibrato with a random wave, slow), deeper as DOSE rises.
        const auto drift = [] (Builder& b, int o, float cents)
        {
            b.osc (o, "VibWave", (float) synth::VibratoWave::random).osc (o, "VibRate", 1.3f);
            route (b, "macro3", ParamIDs::osc (o, "VibAmount"), cents / 100.0f);
        };
        const auto echoes = [] (Builder& b, EMm mode, int distance, float mix, float feedback, float age)
        {
            b.set (ParamIDs::echoOn, 1.0f).set (ParamIDs::echoMode, (float) (int) mode).set (ParamIDs::echoDistance, (float) distance)
             .set (ParamIDs::echoMix, mix).set (ParamIDs::echoFeedback, feedback).set (ParamIDs::echoAge, age);
        };

        // ---- LEADS -------------------------------------------------------------------------------------------
        auto lone = x.make ("Lone Voice", 1, { "Lead", "Voice", "Crackle", "Grief" },
            "A lone voice rising out of the noise: a buzzing tone sung through the VOICE filter (an \"oo\" opening to "
            "\"ah\"), a breath of hiss around it, a slow, aching vibrato, a long hall. MACROS: BRIGHT, SPACE, DOSE (the "
            "counter's crackle thickening, the voice wavering and wearing as on an old tape). MOD WHEEL: the vowel "
            "towards \"ee\".");
        lone.osc (0, "Volume", 0.55f).osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.5f).osc (0, "UniDetune", 0.05f)
            .osc (0, "VibRate", 4.6f).osc (0, "VibAmount", 9.0f).osc (0, "VibFade", 900.0f)
            .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.05f).osc (1, "NoiseWidth", 2.0f)
            .osc (1, "Volume", 0.06f)
            .ampEnvelope (250.0f, 1200.0f, 0.85f, 1400.0f)
            .mono (60.0f)
            .set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceSize, 0.75f).set (ParamIDs::voiceSharp, 0.5f).set (ParamIDs::voiceVowel, 0.0f);
        oneShot (lone, 0, { SPm { 0.0f, 0.0f, 0.3f }, SPm { 0.7f, 1.0f, 0.0f } });
        route (lone, "env1", ParamIDs::voiceVowel, 0.25f);
        route (lone, "modWheel", ParamIDs::voiceVowel, 0.4f, 100.0f);
        route (lone, "macro1", ParamIDs::voiceSharp, 0.3f);
        dose (lone, 4500.0f);
        drift (lone, 0, 30.0f);
        wear (lone, 0.6f);
        macros (lone, "BRIGHT", "SPACE", "DOSE");
        reverb (lone, RT::hall, 0.9f, 0.3f, 40.0f, 4.0f);
        eqBand (lone, 0, eq::Shape::lowCut, 90.0f, 0.0f);
        space (lone);
        lone.set (ParamIDs::patchLevel, -1.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (lone);

        auto warning = x.make ("Warning Tone", 2, { "Lead", "Pulse", "Echoes", "Drift" },
            "A thin warning tone that won't hold still: a narrow pulse wandering slowly out of tune (a random LFO on its "
            "pitch), its last two beats coming back from ECHOES, a cold room. MACROS: BRIGHT, SPACE, DOSE (the crackle "
            "thickening, the tone crushed and drifting further). MOD WHEEL: a faster, wider waver.");
        warning.osc (0, "Wave", pulse).osc (0, "Width", 0.12f).osc (0, "Volume", 0.5f)
               .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.35f).osc (1, "PhaseRand", 0.0f)
               .filter (FilterType::lp12, 3200.0f, 0.15f, 0.0f).set (ParamIDs::fltKey, 0.4f)
               .ampEnvelope (20.0f, 600.0f, 0.8f, 600.0f)
               .mono (30.0f)
               .set (ParamIDs::lfo (0, "Wave"), (float) synth::mod::LfoWave::smoothRandom).set (ParamIDs::lfo (0, "Rate"), 0.4f)
               .set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (warning, "lfo1", ParamIDs::osc (0, "Fine"), 0.04f);
        route (warning, "lfo1", ParamIDs::osc (1, "Fine"), 0.03f);
        route (warning, "modWheel", ParamIDs::osc (0, "VibAmount"), 0.25f);
        warning.osc (0, "VibRate", 6.5f);
        route (warning, "macro1", ParamIDs::fltCutoff, 0.15f);
        dose (warning, 3000.0f);
        route (warning, "macro3", ParamIDs::osc (0, "Crush"), 0.45f);
        drift (warning, 1, 25.0f);
        echoes (warning, EMm::echo, 1, 0.35f, 0.2f, 0.5f);
        macros (warning, "BRIGHT", "SPACE", "DOSE");
        reverb (warning, RT::room, 0.6f, 0.15f);
        space (warning);
        warning.set (ParamIDs::patchLevel, -2.3f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (warning);

        auto rim = x.make ("Bowed Rim", 3, { "Lead", "FM", "Metal", "Bowed" },
            "Metal singing under a bow: a sine pushed by another at an inharmonic 2.76 (a struck bar's first overtone), "
            "the push swelling in as each note is held, so it starts pure and turns to ringing metal, a slow vibrato, a "
            "long dark hall. MACROS: BRIGHT, SPACE, DOSE (the crackle thickening, the metal harsher and grinding). MOD "
            "WHEEL: more ring.");
        rim.osc (0, "Wave", sine).osc (0, "Volume", 0.7f).osc (0, "PhaseRand", 0.0f)
           .osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (2.76f)).osc (0, "FmAmount", 0.04f)
           .osc (0, "VibRate", 4.2f).osc (0, "VibAmount", 6.0f).osc (0, "VibFade", 800.0f)
           .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", 12.0f).osc (1, "Fine", 7.0f).osc (1, "Volume", 0.12f)
           .ampEnvelope (400.0f, 1500.0f, 0.85f, 1600.0f)
           .mono (80.0f);
        oneShot (rim, 0, { SPm { 0.0f, 0.0f, 0.4f }, SPm { 1.5f, 1.0f, 0.0f } });
        route (rim, "env1", ParamIDs::osc (0, "FmAmount"), 0.12f);
        route (rim, "modWheel", ParamIDs::osc (0, "FmAmount"), 0.1f);
        route (rim, "macro1", ParamIDs::osc (1, "Volume"), 0.1f);
        dose (rim, 2200.0f);
        route (rim, "macro3", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (rim, "macro3", ParamIDs::osc (0, "Crush"), 0.3f);
        macros (rim, "BRIGHT", "SPACE", "DOSE");
        reverb (rim, RT::hall, 0.85f, 0.28f, 30.0f, 3.5f);
        space (rim);
        rim.set (ParamIDs::patchLevel, -3.9f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (rim);

        // ---- PADS --------------------------------------------------------------------------------------------
        auto pressure = x.make ("Pressure Front", 4, { "Pad", "Drone", "Sub", "Breathing" },
            "A pressure drone you feel more than hear: a sine an octave down and a low, slowly breathing saw (an LFO on "
            "the filter, once every few seconds), swelling in over seconds. Hold a low chord. MACROS: BRIGHT, SPACE, "
            "DOSE (the crackle thickening, the saw driven into a tube and the pressure rising). MOD WHEEL: opens the "
            "filter.");
        pressure.osc (0, "Volume", 0.4f).osc (0, "UniVoices", 3.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.07f).osc (0, "UniSpread", 0.7f)
                .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.55f).osc (1, "PhaseRand", 0.0f)
                .filter (FilterType::lp24, 260.0f, 0.25f, 0.0f).set (ParamIDs::fltKey, 0.2f)
                .shaper (0, synth::ShaperModel::tube, 0.0f, true)
                .ampEnvelope (2500.0f, 2000.0f, 1.0f, 2500.0f)
                .set (ParamIDs::lfo (0, "Rate"), 0.18f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        clean (pressure, 1);
        route (pressure, "lfo1", ParamIDs::fltCutoff, 0.08f);
        route (pressure, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (pressure, "macro1", ParamIDs::fltCutoff, 0.15f);
        dose (pressure, 1500.0f);
        route (pressure, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        route (pressure, "macro3", ParamIDs::fltCutoff, 0.1f);
        macros (pressure, "BRIGHT", "SPACE", "DOSE");
        reverb (pressure, RT::hall, 0.8f, 0.18f, 30.0f, 3.0f);
        space (pressure);
        pressure.set (ParamIDs::patchLevel, -9.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pressure);

        auto glass = x.make ("Glass Partials", 5, { "Pad", "Wavetable", "Metal", "Shimmer" },
            "A cloud of glassy, metallic partials hanging in the air: the Comb Sweep table drifting slowly through its "
            "frames (an LFO on INDEX), six voices spread wide and smeared by a chorus, a high, cold plate. MACROS: "
            "BRIGHT, SPACE, DOSE (the crackle thickening, the partials crushed and worn). MOD WHEEL: through the table.");
        glass.osc (0, "Wave", wavetable).osc (0, "Table", table ("Comb Sweep")).osc (0, "WtIndex", 0.4f).osc (0, "Volume", 0.5f)
             .osc (0, "UniVoices", 6.0f).osc (0, "UniMix", 0.75f).osc (0, "UniDetune", 0.12f).osc (0, "UniSpread", 1.0f)
             .filter (FilterType::lp12, 7000.0f, 0.1f, 0.0f)
             .ampEnvelope (1500.0f, 2000.0f, 1.0f, 2500.0f)
             .set (ParamIDs::lfo (0, "Rate"), 0.06f).set (ParamIDs::lfo (0, "Trigger"), (float) synth::mod::LfoTrigger::global);
        route (glass, "lfo1", ParamIDs::osc (0, "WtIndex"), 0.2f);
        route (glass, "modWheel", ParamIDs::osc (0, "WtIndex"), 0.3f);
        route (glass, "macro1", ParamIDs::fltCutoff, 0.15f);
        dose (glass, 6000.0f);
        route (glass, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        wear (glass, 0.5f);
        macros (glass, "BRIGHT", "SPACE", "DOSE");
        chorus (glass, ParamIDs::modIds, MT::chorus, 0.2f, 0.5f, 0.4f);
        reverb (glass, RT::plate, 0.85f, 0.28f, 30.0f, 6.0f);
        eqBand (glass, 0, eq::Shape::lowCut, 120.0f, 0.0f);
        space (glass);
        glass.set (ParamIDs::patchLevel, -6.7f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (glass);

        auto mourning = x.make ("Mourning Choir", 6, { "Pad", "Choir", "Voice", "Grief" },
            "A choir in mourning: two sections through the VOICE filter singing a dark \"oh\", each singer with their "
            "own slow vibrato, a breath through it, a cathedral. Basses sing the low notes, sopranos the high (SIZE "
            "follows the keys). MACROS: BRIGHT, SPACE, DOSE (the crackle thickening, the choir darkening and wearing "
            "away). MOD WHEEL: the vowel opening to \"ah\".");
        mourning.osc (0, "Volume", 0.5f).osc (0, "UniVoices", 5.0f).osc (0, "UniMix", 0.85f).osc (0, "UniDetune", 0.09f).osc (0, "UniSpread", 0.9f)
                .osc (0, "VibMode", (float) synth::VibratoMode::independent).osc (0, "VibAmount", 9.0f).osc (0, "VibRate", 4.8f).osc (0, "VibFade", 900.0f)
                .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.25f).osc (1, "Volume", 0.3f)
                .osc (1, "UniVoices", 4.0f).osc (1, "UniMix", 0.85f).osc (1, "UniDetune", 0.07f).osc (1, "UniSpread", 0.9f)
                .osc (1, "VibMode", (float) synth::VibratoMode::independent).osc (1, "VibAmount", 11.0f).osc (1, "VibRate", 5.2f).osc (1, "VibFade", 900.0f)
                .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.5f).osc (2, "NoiseColour", 0.05f).osc (2, "NoiseWidth", 2.0f).osc (2, "Volume", 0.04f)
                .ampEnvelope (900.0f, 1500.0f, 0.95f, 2200.0f)
                .set (ParamIDs::voiceOn, 1.0f).set (ParamIDs::voiceSize, 0.0f).set (ParamIDs::voiceVowel, 0.2f).set (ParamIDs::voiceSharp, 0.4f);
        for (int k = 0; k < 2; ++k)   // SIZE by the keys: 2 x (note / 127)^2
            mourning.rig.modulation.routes.push_back ({ true, "keytrack", ParamIDs::voiceSize, 1.0f, false, 0.0f, 0.5f });
        route (mourning, "modWheel", ParamIDs::voiceVowel, 0.3f, 100.0f);
        route (mourning, "macro1", ParamIDs::voiceSharp, 0.3f);
        dose (mourning, 4000.0f);
        route (mourning, "macro3", ParamIDs::voiceVowel, -0.15f);
        wear (mourning, 0.55f);
        macros (mourning, "BRIGHT", "SPACE", "DOSE");
        reverb (mourning, RT::cathedral, 0.85f, 0.3f, 40.0f, 4.0f);
        eqBand (mourning, 0, eq::Shape::lowCut, 70.0f, 0.0f);
        space (mourning);
        mourning.set (ParamIDs::patchLevel, -12.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (mourning);

        // ---- BASSES ------------------------------------------------------------------------------------------
        auto turbine = x.make ("Turbine Hum", 7, { "Bass", "Hum", "Tremolo", "Machine" },
            "The hum of a turbine hall: a low saw and a square an octave down through a dark filter, trembling with the "
            "machine (MOD's TREMOLO, fast and shallow), a clean sine holding the bottom. MACROS: BRIGHT, SPACE, DOSE (the "
            "crackle thickening, the hum driven harder through SHAPER 1). MOD WHEEL: opens the filter.");
        turbine.osc (0, "Volume", 0.4f)
               .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.5f).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.3f)
               .osc (2, "On", 1.0f).osc (2, "Wave", sine).osc (2, "Volume", 0.45f).osc (2, "PhaseRand", 0.0f)
               .filter (FilterType::lp24, 380.0f, 0.2f, 0.1f).filterEnvelope (5.0f, 400.0f, 0.5f, 300.0f).set (ParamIDs::fltKey, 0.5f)
               .shaper (0, synth::ShaperModel::soft, 0.1f, true)
               .ampEnvelope (20.0f, 800.0f, 1.0f, 400.0f)
               .mono (20.0f);
        clean (turbine, 2);
        chorus (turbine, ParamIDs::modIds, MT::tremolo, 9.0f, 0.18f, 1.0f);
        route (turbine, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (turbine, "macro1", ParamIDs::fltCutoff, 0.15f);
        dose (turbine, 1800.0f);
        route (turbine, "macro3", ParamIDs::shp (0, "Drive"), 0.5f);
        macros (turbine, "BRIGHT", "SPACE", "DOSE");
        reverb (turbine, RT::room, 0.4f, 0.06f);
        space (turbine);
        turbine.set (ParamIDs::patchLevel, -5.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (turbine);

        auto pump = x.make ("Coolant Pump", 8, { "Bass", "Step Seq", "Pump", "Breathing" },
            "A pump breathing in the dark: a held note pushed and drawn in eighths with your song by the STEP SEQUENCER, "
            "its edges soft (CONTOUR) so it heaves rather than chops, every fourth push harder, a clean sine under it. "
            "MACROS: BRIGHT, SPACE, DOSE (the crackle thickening, the pump straining: a sharper resonance, its pitch "
            "drifting). MOD WHEEL: opens the filter.");
        pump.osc (0, "Volume", 0.5f).osc (0, "UniVoices", 2.0f).osc (0, "UniMix", 0.6f).osc (0, "UniDetune", 0.06f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f)
            .filter (FilterType::lp24, 450.0f, 0.3f, 0.0f).set (ParamIDs::fltKey, 0.5f)
            .ampEnvelope (10.0f, 600.0f, 1.0f, 300.0f)
            .mono (20.0f)
            .set (ParamIDs::seqOn, 1.0f).set (ParamIDs::seqVolMix, 0.85f).set (ParamIDs::seqGate, 0.8f)
            .set (ParamIDs::seqContour, 0.6f).set (ParamIDs::seqDivision, division ("1/8")).set (ParamIDs::seqLength, 8.0f);
        clean (pump, 1);
        for (int row = 0; row < 2; ++row)
            for (int k = 0; k < synth::seq::maxSteps; ++k)
            {
                auto& step = pump.rig.modulation.stepPattern.rows[(size_t) row][(size_t) k];
                step.on = true;
                step.level = k % 4 == 0 ? 1.0f : 0.6f;
            }
        route (pump, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (pump, "macro1", ParamIDs::fltCutoff, 0.15f);
        dose (pump, 2500.0f);
        route (pump, "macro3", ParamIDs::fltRes, 0.35f);
        drift (pump, 0, 20.0f);
        macros (pump, "BRIGHT", "SPACE", "DOSE");
        reverb (pump, RT::room, 0.45f, 0.08f);
        space (pump);
        pump.set (ParamIDs::patchLevel, 0.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pump);

        auto steel = x.make ("Stressed Steel", 9, { "Bass", "Pitch Envelope", "Groan", "FM" },
            "Steel groaning under load: a deep, gritty bass whose pitch sags slowly a semitone flat as each note is held "
            "(the PITCH envelope, two seconds down), a hint of inharmonic FM in it, a clean sine underneath. MACROS: "
            "BRIGHT, SPACE, DOSE (the crackle thickening, the sag deeper, the groan harsher). MOD WHEEL: opens the "
            "filter.");
        steel.osc (0, "Volume", 0.45f).osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (1.41f)).osc (0, "FmAmount", 0.05f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Volume", 0.45f).osc (1, "PhaseRand", 0.0f).osc (1, "PitchEnv", 0.0f)
             .filter (FilterType::lp24, 420.0f, 0.3f, 0.15f).filterEnvelope (5.0f, 700.0f, 0.4f, 400.0f).set (ParamIDs::fltKey, 0.5f)
             .shaper (0, synth::ShaperModel::diode, 0.15f, true)
             .ampEnvelope (15.0f, 900.0f, 1.0f, 400.0f)
             .mono (30.0f)
             .set (ParamIDs::pitchEnvOn, 1.0f).set (ParamIDs::pitchEnvRange, 2.0f);
        steel.rig.modulation.pitchEnvelope = { { SPm { 0.0f, 0.5f, 0.0f }, SPm { 0.3f, 0.5f, 0.4f }, SPm { 2.3f, 0.25f, 0.0f } }, -1, -1, -1 };
        clean (steel, 1);
        route (steel, "modWheel", ParamIDs::fltCutoff, 0.15f);
        route (steel, "macro1", ParamIDs::fltCutoff, 0.15f);
        dose (steel, 1600.0f);
        route (steel, "macro3", ParamIDs::pitchEnvRange, 0.06f);
        route (steel, "macro3", ParamIDs::osc (0, "FmAmount"), 0.1f);
        macros (steel, "BRIGHT", "SPACE", "DOSE");
        reverb (steel, RT::room, 0.5f, 0.08f);
        space (steel);
        steel.set (ParamIDs::patchLevel, -5.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (steel);

        // ---- KEYS --------------------------------------------------------------------------------------------
        auto pipes = x.make ("Struck Pipes", 10, { "Keys", "Additive", "Metal", "Percussive" },
            "Steel pipes struck in an empty hall: four sines on a pipe's stretched, inharmonic partials (1, 2.92, 5.4 and "
            "8.4 times the note), each ringing out at its own pace, the higher the sooner, with a quick metallic clank on "
            "the strike. MACROS: BRIGHT (more of the upper partials), SPACE, DOSE (the crackle thickening, the pipes "
            "corroded and crushed).");
        constexpr std::array<float, 4> partials { 0.0f, 18.56f, 29.2f, 36.84f };   // semitones: 1, 2.92, 5.4, 8.4
        constexpr std::array<float, 4> levels { 0.55f, 0.3f, 0.2f, 0.12f };
        constexpr std::array<float, 4> seconds { 4.5f, 2.6f, 1.4f, 0.8f };
        for (int k = 0; k < 4; ++k)
        {
            const auto semis = std::floor (partials[(size_t) k]);
            pipes.osc (k, "On", 1.0f).osc (k, "Wave", sine).osc (k, "Transpose", semis)
                 .osc (k, "Fine", (partials[(size_t) k] - semis) * 100.0f).osc (k, "Volume", 0.0f).osc (k, "PhaseRand", 0.0f);
            oneShot (pipes, k, { SPm { 0.0f, 1.0f, -0.6f }, SPm { seconds[(size_t) k], 0.0f, 0.0f } });
            route (pipes, ("env" + juce::String (k + 1)).toRawUTF8(), ParamIDs::osc (k, "Volume"), levels[(size_t) k] * 0.5f);
        }
        pipes.osc (4, "On", 1.0f).osc (4, "Gain", 0.0f).osc (4, "Noise", 0.5f).osc (4, "NoiseColour", 0.1f).osc (4, "Volume", 0.0f);
        oneShot (pipes, 4, { SPm { 0.0f, 1.0f, -0.5f }, SPm { 0.03f, 0.0f, 0.0f } });
        route (pipes, "env5", ParamIDs::osc (4, "Volume"), 0.15f);
        pipes.ampEnvelope (1.0f, 4500.0f, 0.0f, 2500.0f).set (ParamIDs::ampVel, 0.5f);
        route (pipes, "macro1", ParamIDs::osc (2, "Volume"), 0.08f);
        route (pipes, "macro1", ParamIDs::osc (3, "Volume"), 0.06f);
        dose (pipes, 3500.0f);
        for (int k = 0; k < 4; ++k)
            route (pipes, "macro3", ParamIDs::osc (k, "Crush"), 0.35f);
        macros (pipes, "BRIGHT", "SPACE", "DOSE");
        reverb (pipes, RT::hall, 0.75f, 0.28f, 30.0f, 4.5f);
        space (pipes);
        pipes.set (ParamIDs::patchLevel, -4.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (pipes);

        auto upright = x.make ("Abandoned Upright", 11, { "Keys", "Piano", "Detuned", "Decayed" },
            "An upright left in an empty schoolroom for decades: a felt piano whose strings have drifted apart (its "
            "octave string flat, the note's partner sharp), a little tape wear, a cold room. MACROS: BRIGHT, SPACE, DOSE "
            "(the crackle thickening, the strings drifting further and the tape wearing through). MOD WHEEL: more of the "
            "felt's noise.");
        feltPiano (upright);
        upright.rig.modulation.routes.erase (std::remove_if (upright.rig.modulation.routes.begin(), upright.rig.modulation.routes.end(),
            [] (const ModRouteData& r) { return r.source == "macro3"; }), upright.rig.modulation.routes.end());   // DOSE's instead
        upright.osc (1, "Fine", 9.0f).osc (2, "Fine", -14.0f);
        dose (upright, 2800.0f);
        route (upright, "macro3", ParamIDs::osc (1, "Fine"), 0.12f);
        route (upright, "macro3", ParamIDs::osc (2, "Fine"), -0.15f);
        wear (upright, 0.6f);
        upright.set (ParamIDs::lofiMix, 0.15f);
        macros (upright, "BRIGHT", "SPACE", "DOSE");
        reverb (upright, RT::room, 0.6f, 0.2f);
        space (upright);
        upright.set (ParamIDs::patchLevel, -4.5f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (upright);

        auto bell = x.make ("Tolling Bell", 12, { "Keys", "FM", "Bell", "Cold" },
            "A cold, tuned bell tolling in the distance: a sine struck by another at 3.5 times its pitch, the bright "
            "strike fading over a second to a pure, long hum, an octave-down sine under it, a vast hall. MACROS: BRIGHT "
            "(a brighter strike), SPACE, DOSE (the crackle thickening, the bell cracked: crushed and harsher).");
        bell.osc (0, "Wave", sine).osc (0, "Volume", 0.6f).osc (0, "PhaseRand", 0.0f)
            .osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (3.5f)).osc (0, "FmAmount", 0.03f)
            .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.25f).osc (1, "PhaseRand", 0.0f)
            .ampEnvelope (1.0f, 6000.0f, 0.0f, 3000.0f).set (ParamIDs::ampDecayCurve, -0.4f).set (ParamIDs::ampVel, 0.5f);
        oneShot (bell, 0, { SPm { 0.0f, 1.0f, -0.6f }, SPm { 1.2f, 0.0f, 0.0f } });
        route (bell, "env1", ParamIDs::osc (0, "FmAmount"), 0.2f);
        route (bell, "macro1", ParamIDs::osc (0, "FmAmount"), 0.08f);
        dose (bell, 3200.0f);
        route (bell, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        route (bell, "macro3", ParamIDs::osc (0, "FmAmount"), 0.1f);
        macros (bell, "BRIGHT", "SPACE", "DOSE");
        reverb (bell, RT::cathedral, 0.8f, 0.3f, 30.0f, 4.0f);
        space (bell);
        bell.set (ParamIDs::patchLevel, -8.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (bell);

        // ---- ARPS --------------------------------------------------------------------------------------------
        auto counter = x.make ("Counter Ticks", 13, { "Arp", "Crackle", "Randomizer", "Ticks" },
            "A counter ticking over the chord you hold: ARP 1 plays tiny tuned ticks rolled by the RANDOMIZER from the "
            "minor scale in sixteenths, many left out, so they fall unevenly, over CRACKLE already clicking steadily. "
            "MACROS: BRIGHT, SPACE, DOSE (the count climbing into a roar). MOD WHEEL: brighter ticks.");
        counter.osc (0, "Wave", sine).osc (0, "Volume", 0.55f).osc (0, "PhaseRand", 0.0f).osc (0, "Arp", 1.0f)
               .osc (0, "FmSource", 0.0f).osc (0, "ModRate", ratio (4.0f)).osc (0, "FmAmount", 0.08f)
               .osc (1, "On", 1.0f).osc (1, "Gain", 0.0f).osc (1, "Noise", 0.5f).osc (1, "NoiseColour", 0.0f).osc (1, "Volume", 0.1f).osc (1, "Arp", 1.0f)
               .ampEnvelope (0.5f, 70.0f, 0.0f, 50.0f)
               .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::up)
               .set (ParamIDs::arp (0, "Division"), division ("1/16")).set (ParamIDs::arp (0, "Octaves"), 2.0f)
               .set (ParamIDs::arp (0, "Gate"), 0.3f);
        {
            auto& r = counter.rig.modulation.arpRandomizers[0];
            r.scale = synth::arp::scaleNames.indexOf ("Minor");
            r.restChance = 0.55f;
            r.lengthChances = { 1.0f, 0.0f, 0.0f, 0.0f };
            r.octaveChances = { 0.0f, 0.2f, 1.0f, 0.6f, 0.0f };
            r.velocityLow = 0.35f;
            r.velocityHigh = 1.0f;
            r.seed = 86;
            auto& p = counter.rig.modulation.arps[0].patterns[0];
            p = synth::arp::roll (r, 16, synth::arp::Patterns::makeDefault().patterns[0], r.seed);
        }
        route (counter, "modWheel", ParamIDs::osc (0, "FmAmount"), 0.15f);
        route (counter, "macro1", ParamIDs::osc (0, "FmAmount"), 0.1f);
        dose (counter, 3800.0f, 6.0f, -30.0f, 0.22f);   // its LEVEL is raised ~11 dB for the quiet ticks: the crackle starts lower and rises less
        macros (counter, "BRIGHT", "SPACE", "DOSE");
        reverb (counter, RT::room, 0.5f, 0.15f);
        space (counter);
        counter.set (ParamIDs::patchLevel, 10.8f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (counter);

        auto relay = x.make ("Relay Bank", 14, { "Arp", "Mechanical", "Pulse", "Machine" },
            "A bank of relays switching somewhere below: ARP 1 plays the chord you hold in eighths as hard, dry clacks "
            "(a short square with a click of noise and a low thud an octave down), an irregular pattern of switches "
            "with gaps and doubled hits. MACROS: BRIGHT, SPACE, DOSE (the crackle thickening, the relays arcing: crushed "
            "and drifting). MOD WHEEL: opens the filter.");
        relay.osc (0, "Wave", pulse).osc (0, "Width", 0.5f).osc (0, "Volume", 0.4f).osc (0, "Arp", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", sine).osc (1, "Transpose", -12.0f).osc (1, "Volume", 0.4f).osc (1, "Arp", 1.0f).osc (1, "PhaseRand", 0.0f)
             .osc (2, "On", 1.0f).osc (2, "Gain", 0.0f).osc (2, "Noise", 0.5f).osc (2, "NoiseColour", 0.1f).osc (2, "Volume", 0.12f).osc (2, "Arp", 1.0f)
             .filter (FilterType::lp24, 1800.0f, 0.2f, 0.3f).filterEnvelope (0.0f, 60.0f, 0.0f, 40.0f).set (ParamIDs::fltKey, 0.5f)
             .ampEnvelope (0.5f, 90.0f, 0.0f, 60.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::asPlayed)
             .set (ParamIDs::arp (0, "Division"), division ("1/8")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.4f);
        clean (relay, 1);
        relay.osc (1, "Arp", 1.0f);
        {
            auto& p = relay.rig.modulation.arps[0].patterns[0];
            p = {};
            constexpr bool on[16] = { true, false, true, true, false, false, true, false, true, true, false, true, false, false, true, false };
            for (int k = 0; k < 16; ++k)
            {
                auto& block = p.blocks[(size_t) k];
                block.on = on[k];
                block.velocity = k % 4 == 0 ? 1.0f : 0.7f;
                block.ratchet = k == 8 || k == 14 ? 2 : 1;
            }
        }
        route (relay, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (relay, "macro1", ParamIDs::fltCutoff, 0.15f);
        dose (relay, 2400.0f);
        route (relay, "macro3", ParamIDs::osc (0, "Crush"), 0.4f);
        drift (relay, 0, 25.0f);
        macros (relay, "BRIGHT", "SPACE", "DOSE");
        reverb (relay, RT::room, 0.5f, 0.12f);
        space (relay);
        relay.set (ParamIDs::patchLevel, 5.6f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (relay);

        auto alarm = x.make ("Distant Siren", 15, { "Arp", "Alarm", "Echoes", "Distant" },
            "An alarm sounding somewhere far off, through walls: ARP 1 swings between the notes you hold in quarter "
            "notes on a hollow, filtered tone that slides into each (SLIDE), coming back again and again from ECHOES a "
            "beat behind, lost in a huge, dark hall. MACROS: BRIGHT (nearer), SPACE, DOSE (the crackle thickening, the "
            "siren wavering and wearing). MOD WHEEL: opens the filter.");
        alarm.osc (0, "Wave", triangle).osc (0, "Volume", 0.6f).osc (0, "Arp", 1.0f)
             .osc (1, "On", 1.0f).osc (1, "Wave", pulse).osc (1, "Width", 0.3f).osc (1, "Volume", 0.2f).osc (1, "Arp", 1.0f)
             .filter (FilterType::lp12, 1300.0f, 0.2f, 0.0f).set (ParamIDs::fltKey, 0.4f)
             .ampEnvelope (60.0f, 500.0f, 0.8f, 400.0f)
             .set (ParamIDs::glide, 180.0f)
             .set (ParamIDs::arp (0, "On"), 1.0f).set (ParamIDs::arp (0, "Mode"), (float) synth::arp::Mode::upDown)
             .set (ParamIDs::arp (0, "Division"), division ("1/4")).set (ParamIDs::arp (0, "Octaves"), 1.0f)
             .set (ParamIDs::arp (0, "Gate"), 0.95f);
        {
            auto& p = alarm.rig.modulation.arps[0].patterns[0];
            p = {};
            for (int k = 0; k < 4; ++k)
            {
                p.blocks[(size_t) k].on = true;
                p.blocks[(size_t) k].slide = true;
                p.blocks[(size_t) k].velocity = 0.85f;
            }
        }
        alarm.set (ParamIDs::arp (0, "Length"), 4.0f);
        route (alarm, "modWheel", ParamIDs::fltCutoff, 0.2f);
        route (alarm, "macro1", ParamIDs::fltCutoff, 0.2f);
        echoes (alarm, EMm::echo, 0, 0.35f, 0.25f, 0.6f);
        dose (alarm, 3000.0f);
        drift (alarm, 0, 30.0f);
        wear (alarm, 0.5f);
        macros (alarm, "BRIGHT", "SPACE", "DOSE");
        reverb (alarm, RT::hall, 0.9f, 0.32f, 60.0f, 3.0f);
        space (alarm);
        alarm.set (ParamIDs::patchLevel, -2.0f);   // LEVEL: balanced against the others (measured, not by ear)
        x.add (alarm);
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
    nightdrive ({ Rig::numBanks + 9, rigs });
    unspoken ({ Rig::numBanks + 10, rigs });
    cybershrine ({ Rig::numBanks + 11, rigs });
    stowaway ({ Rig::numBanks + 12, rigs });
    chromefury ({ Rig::numBanks + 13, rigs });
    ascension ({ Rig::numBanks + 14, rigs });
    refraction ({ Rig::numBanks + 15, rigs });
    psiStorm ({ Rig::numBanks + 16, rigs });
    bloodwave ({ Rig::numBanks + 17, rigs });
    dustfall ({ Rig::numBanks + 18, rigs });
    superposition ({ Rig::numBanks + 19, rigs });
    severed ({ Rig::numBanks + 20, rigs });
    slingblade ({ Rig::numBanks + 21, rigs });
    paradox ({ Rig::numBanks + 22, rigs });
    meltdown ({ Rig::numBanks + 23, rigs });
    return rigs;
}
