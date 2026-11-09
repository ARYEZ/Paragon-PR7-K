#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <string_view>

// Parameter IDs are saved in DAW projects and presets, so never rename one
// once it has shipped. Add new parameters with a new ID instead.
//
// PR7-K started afresh at 0.1.0: none of PR7-E's parameters carried over
// (its EQ, COMP, DOMINION and LIMITER code is kept for the effects rack, a
// later stage, which will give them new IDs of its own).
namespace ParamIDs
{
    // Global: not part of a preset.
    inline constexpr auto outputGain = "outputGain";   // the OUTPUT knob
    inline constexpr auto bypass     = "bypass";
    inline constexpr auto tune       = "tune";         // the whole synth, in cents (SETTINGS)

    // Saved with each preset: its own level, to balance presets against each other.
    inline constexpr auto patchLevel = "patchLevel";

    // The 8 oscillator tabs: "osc1On", "osc1Wave"... `what` is one of oscSettings.
    inline constexpr int numOscillators = 8;
    inline juce::String osc (int index, const char* what) { return "osc" + juce::String (index + 1) + what; }
    // Stage 2 (0.2.0) appended its unison, vibrato and chorder settings ("Ch1On"... for the chorder's 4 notes),
    // stage 3 (0.3.0) the WAVETABLE's: which factory table, INDEX, its mode and FRAMES,
    // 0.4.0 SHAPE's: SKEW, PINCH, FORMANT and its mode, CRUSH and its mode,
    // 0.5.0 SYNC (with its envelope), FM, AM and their modulator's RATE, GRID and PHASE LOCK,
    // 0.6.0 the FFT editor's SWEEP, FLIP, LP, HP, SLOPE, VSPEED and VSTRENGTH (its curve is kept with the preset),
    // 0.8.0 which SHAPERs the oscillator goes through ("Shp1" to "Shp4"),
    // 0.11.0 whether it follows the PITCH module's envelope.
    inline constexpr const char* oscSettings[] = {
        "On", "Wave", "Volume", "Pan", "Transpose", "Fine", "Random", "PhaseRand", "Phase", "Gain", "Width", "FadeIn", "Invert",
        "Noise", "NoiseColour", "NoiseWidth", "Sub", "SubWave", "SubOct", "SubInvert", "Filter", "Voices",
        "UniVoices", "UniMix", "UniDetune", "UniSpread", "UniOctaves",
        "VibMode", "VibWave", "VibRate", "VibAmount", "VibFade", "VibCurve",
        "Ch1On", "Ch1Coarse", "Ch1Fine", "Ch1Pan", "Ch1Vol", "Ch2On", "Ch2Coarse", "Ch2Fine", "Ch2Pan", "Ch2Vol",
        "Ch3On", "Ch3Coarse", "Ch3Fine", "Ch3Pan", "Ch3Vol", "Ch4On", "Ch4Coarse", "Ch4Fine", "Ch4Pan", "Ch4Vol",
        "Table", "WtIndex", "WtMode", "WtFrames",
        "Skew", "Pinch", "Formant", "FormantMode", "Crush", "CrushMode",
        "SyncAmount", "SyncMode", "SyncEnv", "SyncEnvTime", "SyncEnvCurve", "FmAmount", "FmSource", "AmAmount", "AmShape",
        "ModRate", "ModGrid", "ModLock",
        "FftSweep", "FftFlip", "FftLp", "FftHp", "FftSlope", "FftVSpeed", "FftVStrength",
        "Shp1", "Shp2", "Shp3", "Shp4",
        "PitchEnv",
        // Stage 4e: TRAVEL (its envelope, steps and strengths are kept with the preset, in the mod matrix).
        "TrOn", "TrEditors", "TrTrigger", "TrMode", "TrPatternMode", "TrSteps", "TrStrength", "TrVelocity", "TrVelCurve",
        "TrSmooth", "TrPhase", "TrFlip", "TrRate", "TrSync", "TrDivision",
        // Stage 5a: which ARP its notes come from.
        "Arp",
        // Stage 7b: how much it sends to DELAY and to REVERB.
        "SendDelay", "SendReverb",
        // Stage 7d: the insert bus it plays into (NONE, A to D).
        "Insert",
        // 0.39.0: whether it goes through the VOICE filter (when that's on).
        "Voice",
        // 0.81.0: SFZ KEY (0 the key played; n: every note plays key n - 1's samples, at their own pitch: a drum on any key).
        "SfzKey",
        // 0.83.0: SFZ KEY TRACK (on: that key's samples pitched by the key played, from the chosen key: one gong or drone
        // across the keyboard).
        "SfzKeyTrack" };

    // Where a setting is in oscSettings, worked out by the compiler (a typo won't compile).
    consteval int oscSetting (std::string_view what)
    {
        for (int i = 0; i < (int) std::size (oscSettings); ++i)
            if (std::string_view (oscSettings[i]) == what)
                return i;
        throw "not an oscillator setting";
    }

    // The FILTER and AMP. Numbered from 1: Avenger has four of each, and
    // more may come in a later stage (an oscillator's Filter switch, now on or
    // off for filter 1, can become a choice of filters without breaking presets).
    // The VOICE filter (0.39.0): vowel formants at fixed frequencies, after the FILTER.
    inline constexpr auto voiceOn     = "voiceOn";
    inline constexpr auto voiceVowel  = "voiceVowel";     // 0 OO to 1 EE
    inline constexpr auto voiceSize   = "voiceSize";      // 0 bass to 1 soprano
    inline constexpr auto voiceSharp  = "voiceSharp";     // 0 wide to 1 narrow
    inline constexpr auto voiceMix    = "voiceMix";       // 0 to 1
    inline constexpr auto fltOn       = "flt1On";
    inline constexpr auto fltType     = "flt1Type";
    inline constexpr auto fltCutoff   = "flt1Cutoff";     // Hz
    inline constexpr auto fltRes      = "flt1Res";        // 0 to 1
    inline constexpr auto fltDrive    = "flt1Drive";      // 0 to 1
    inline constexpr auto fltKey      = "flt1Key";        // keytrack, 0 to 1
    inline constexpr auto fltVel      = "flt1Vel";        // cutoff velocity, 0 to 1
    inline constexpr auto fltResVel   = "flt1ResVel";
    inline constexpr auto fltEnv      = "flt1Env";        // envelope amount, -1 to 1
    inline constexpr auto fltEnvVel   = "flt1EnvVel";
    inline constexpr auto fltAttack   = "flt1Attack";     // ms
    inline constexpr auto fltHold     = "flt1Hold";
    inline constexpr auto fltDecay    = "flt1Decay";
    inline constexpr auto fltSustain  = "flt1Sustain";    // 0 to 1
    inline constexpr auto fltRelease  = "flt1Release";
    inline constexpr auto fltAttackCurve  = "flt1AttackCurve";    // -1 to 1
    inline constexpr auto fltDecayCurve   = "flt1DecayCurve";
    inline constexpr auto fltReleaseCurve = "flt1ReleaseCurve";

    inline constexpr auto ampVolume   = "amp1Volume";     // 0 to 2
    inline constexpr auto ampVel      = "amp1Vel";        // 0 to 1
    inline constexpr auto ampPan      = "amp1Pan";        // -1 to 1
    inline constexpr auto ampPanKey   = "amp1PanKey";
    inline constexpr auto ampSpread   = "amp1Spread";
    inline constexpr auto ampSpike    = "amp1Spike";      // 0 to 1
    inline constexpr auto ampSpikeCurve = "amp1SpikeCurve";
    inline constexpr auto ampSpikeVel = "amp1SpikeVel";
    inline constexpr auto ampAttack   = "amp1Attack";
    inline constexpr auto ampHold     = "amp1Hold";
    inline constexpr auto ampDecay    = "amp1Decay";
    inline constexpr auto ampSustain  = "amp1Sustain";
    inline constexpr auto ampRelease  = "amp1Release";
    inline constexpr auto ampAttackCurve  = "amp1AttackCurve";
    inline constexpr auto ampDecayCurve   = "amp1DecayCurve";
    inline constexpr auto ampReleaseCurve = "amp1ReleaseCurve";

    // The SHAPERs (stage 3e, 0.8.0): four, numbered "shp1" to "shp4", each with these settings.
    inline constexpr int numShapers = 4;
    inline constexpr const char* shaperSettings[] = {
        "On", "Model", "Drive", "DriveVel", "Eq1Gain", "Eq1Freq", "Eq1Q", "Eq2Gain", "Eq2Freq", "Eq2Q", "EqSplit", "Place", "Mix", "Output" };
    inline juce::String shp (int index, const char* what) { return "shp" + juce::String (index + 1) + what; }
    consteval int shaperSetting (std::string_view what)
    {
        for (int i = 0; i < (int) std::size (shaperSettings); ++i)
            if (std::string_view (shaperSettings[i]) == what)
                return i;
        throw "not a shaper setting";
    }

    // MODULATION (stage 4a, 0.9.0): four LFOs ("lfo1" to "lfo4", each with
    // these settings), three macro knobs and two macro buttons. The mod
    // matrix itself isn't parameters: it's kept with the preset (ModMatrix).
    inline constexpr int numLfos = 4;
    inline constexpr const char* lfoSettings[] = {
        "Wave", "Rate", "Sync", "Division", "Trigger", "Phase", "Offset", "Delay", "FadeIn", "OneShot" };
    inline juce::String lfo (int index, const char* what) { return "lfo" + juce::String (index + 1) + what; }
    consteval int lfoSetting (std::string_view what)
    {
        for (int i = 0; i < (int) std::size (lfoSettings); ++i)
            if (std::string_view (lfoSettings[i]) == what)
                return i;
        throw "not an LFO setting";
    }
    // The modulation envelopes (stage 4b, 0.10.0): "menv1" to "menv8". Their points are kept with the preset (ModMatrix).
    inline constexpr int numEnvelopes = 8;
    inline constexpr const char* envelopeSettings[] = { "Mode", "Trigger", "Sync", "Speed", "Steps", "Flip", "Offset" };
    inline juce::String env (int index, const char* what) { return "menv" + juce::String (index + 1) + what; }
    consteval int envelopeSetting (std::string_view what)
    {
        for (int i = 0; i < (int) std::size (envelopeSettings); ++i)
            if (std::string_view (envelopeSettings[i]) == what)
                return i;
        throw "not an envelope setting";
    }
    // The ARPEGGIATORS (stage 5a, 0.15.0): "arp1" to "arp8". Their patterns are kept with the preset (ModMatrix).
    inline constexpr int numArps = 8;
    inline constexpr const char* arpSettings[] = { "On", "Mode", "Division", "Timing", "Octaves", "Gate", "Length", "Velocity",
                                                   "LowKey", "HighKey", "Humanize", "Strum", "Pattern", "Chain", "Trigger",
                                                   "NoteSource" };   // 5b (0.16.0): appended
    inline juce::String arp (int index, const char* what) { return "arp" + juce::String (index + 1) + what; }
    consteval int arpSetting (std::string_view what)
    {
        for (int i = 0; i < (int) std::size (arpSettings); ++i)
            if (std::string_view (arpSettings[i]) == what)
                return i;
        throw "not an arp setting";
    }
    inline constexpr const char* macros[] = { "macro1", "macro2", "macro3" };
    inline constexpr const char* macroButtons[] = { "macroButton1", "macroButton2" };

    // The PITCH module (stage 4c, 0.11.0). Its envelope's points are kept with the preset (ModMatrix).
    inline constexpr auto pitchEnvOn    = "pitchEnvOn";
    inline constexpr auto pitchEnvMode  = "pitchEnvMode";
    inline constexpr auto pitchEnvSpeed = "pitchEnvSpeed";
    inline constexpr auto pitchEnvRange = "pitchEnvRange";   // semitones, 0 to 24
    inline constexpr auto pitchEnvSteps = "pitchEnvSteps";
    inline constexpr auto pitchEnvFlip  = "pitchEnvFlip";
    inline constexpr auto pitchKeytrack = "pitchKeytrack";   // -1 to 1
    inline constexpr auto bendLag       = "bendLag";         // ms
    inline constexpr auto glideCurve    = "glideCurve";      // -1 to 1
    inline constexpr auto glideSplit    = "glideSplit";      // a note (0: off)

    // The STEP SEQUENCER (stage 4d, 0.12.0). Its steps are kept with the preset (ModMatrix).
    inline constexpr auto seqOn       = "seqOn";
    inline constexpr auto seqTrigger  = "seqTrigger";    // choice: Song, Voice, First Note, Last Note
    inline constexpr auto seqDivision = "seqDivision";   // SPEED: 8/1 to 1/64
    inline constexpr auto seqTiming   = "seqTiming";     // choice: Straight, Triplet, Dotted
    inline constexpr auto seqMode     = "seqMode";       // choice: Mono, Stereo
    inline constexpr auto seqLength   = "seqLength";     // 2 to 32 steps
    inline constexpr auto seqContour  = "seqContour";
    inline constexpr auto seqGate     = "seqGate";
    inline constexpr auto seqVolMix   = "seqVolMix";
    inline constexpr auto seqWidth    = "seqWidth";
    inline constexpr auto seqDecay    = "seqDecay";      // ms (0: off)
    inline constexpr auto seqFadeIn   = "seqFadeIn";     // ms
    inline constexpr auto seqDelay    = "seqDelay";      // ms
    inline constexpr auto seqLowKey   = "seqLowKey";     // the key zone
    inline constexpr auto seqHighKey  = "seqHighKey";
    inline constexpr auto shuffle     = "shuffle";       // the synth's SHUFFLE (the step sequencer and arpeggiator share it)

    // How the keyboard plays it.
    inline constexpr auto playMode  = "playMode";     // choice: Poly, Mono, Mono Low, Mono High
    inline constexpr auto legato    = "legato";
    inline constexpr auto glide     = "glide";        // ms
    inline constexpr auto glideMode = "glideMode";    // choice: Always, Legato
    inline constexpr auto bendUp    = "bendUp";       // semitones
    inline constexpr auto bendDown  = "bendDown";
    inline constexpr auto voices    = "voices";       // notes at once

    // ARPS TOGETHER (stage 5b, 0.16.0): every arp's steps counted from the first key held, so they stay in step.
    inline constexpr auto arpsTogether = "arpsTogether";
    // SIROCCO (0.58.0, version hint 30, synth::sirocco): a harmony answering every note, snapped to a scale.
    inline constexpr auto siroccoOn       = "siroccoOn";
    inline constexpr auto siroccoKey      = "siroccoKey";        // choice: synth::sirocco::keyNames
    inline constexpr auto siroccoScale    = "siroccoScale";      // choice: synth::arp::scaleNames
    inline constexpr auto siroccoInterval = "siroccoInterval";   // choice: synth::sirocco::intervalNames
    inline constexpr auto siroccoVoices   = "siroccoVoices";     // 1 to 3
    inline constexpr auto siroccoDelay    = "siroccoDelay";      // ms, 0 to 500
    inline constexpr auto siroccoLevel    = "siroccoLevel";      // 0 to 1
    inline constexpr auto siroccoDetune   = "siroccoDetune";     // cents, 0 to 30
    inline constexpr auto siroccoSpread   = "siroccoSpread";     // 0 to 1

    // 0.54.0: THE DRUMS (6a-6c: drumsOn, drumsRoot, drumsLevel, drumsVelocity, drum1-12..., dseq*) were removed at the
    // owner's word (PR7-D is the family's drum plugin): never reuse those IDs.

    // CONTROLLER TO MOD WHEEL (stage 4f, 0.14.0): these controllers drive the mod wheel too (the "mw" switches).
    inline constexpr auto mwBreath     = "mwBreath";
    inline constexpr auto mwExpression = "mwExpression";
    // 0.52.0 (the owner's ask): whether controller 11 (the EXPR wheel, an expression pedal) swells the volume of a
    // preset that doesn't route EXPRESSION (0.49.0). Off: it does nothing there. Global, not in presets.
    inline constexpr auto exprVolume = "exprVolume";
    inline constexpr auto mwAftertouch = "mwAftertouch";
    inline constexpr auto mwSustain    = "mwSustain";

    // THE RACK (stage 7a, 0.17.0): PR7-E 0.18.1's four effects, with PR7-E's IDs (so their presets load in either
    // plugin) and its comments below (its "stomps" are our rack units; "rigs" our presets).
    // ECHELON, the EQ (inspired by FabFilter Pro-Q 4; see dsp/eq).
    inline constexpr auto eqOn        = "eqOn";
    inline constexpr auto eqOutput    = "eqOutput";
    inline constexpr auto eqGainScale = "eqGainScale";
    inline constexpr auto eqAutoGain  = "eqAutoGain";
    inline constexpr int eqNumBands = 24;
    // Each band's settings: "eqB1Used", "eqB1Freq"... `what` is one of the names below.
    // From 0.5.0, its dynamics: Dyn (the range), DynAuto, Thresh, Attack, Release.
    inline juce::String eqBand (int band, const char* what) { return "eqB" + juce::String (band + 1) + what; }
    inline constexpr const char* eqBandSettings[] = { "Used", "On", "Shape", "Freq", "Gain", "Q", "Slope", "Place",
                                                      "Dyn", "DynAuto", "Thresh", "Attack", "Release" };

    // Stomp 2, COMP (0.9.0): PR7-C's compressor, ported. After the EQ.
    inline constexpr auto compOn        = "compOn";
    inline constexpr auto compThreshold = "compThreshold";   // dBFS
    inline constexpr auto compRatio     = "compRatio";       // 1 = SOFT, 30 = infinite
    inline constexpr auto compAttack    = "compAttack";      // ms
    inline constexpr auto compRelease   = "compRelease";     // ms
    inline constexpr auto compAuto      = "compAuto";        // AUTO release
    inline constexpr auto compMakeUp    = "compMakeUp";      // dB
    inline constexpr auto compCharacter = "compCharacter";   // choice: VCA, OPTO, FET
    inline constexpr auto compLink      = "compLink";        // both channels together
    // COMP's bands (0.13.0): multiband compression and expansion, inspired by
    // FabFilter Pro-MB (our own design, see dsp/effects/MultibandComp.h).
    // The bands come first, then the full-band compressor above, which
    // compWide switches (on by default, so older rigs sound as they did).
    inline constexpr auto compWide    = "compWide";
    inline constexpr auto mbLookahead = "mbLookahead";   // COMP 20 ms late, the bands hearing ahead
    inline constexpr auto mbMix       = "mbMix";         // %, 0 to 200
    inline constexpr int mbNumBands = 6;
    // "mbB1Used", "mbB1Low"... `what` is one of the names below.
    inline juce::String mbBand (int band, const char* what) { return "mbB" + juce::String (band + 1) + what; }
    inline constexpr const char* mbBandSettings[] = { "Used", "On", "Low", "High", "Slope", "Thresh", "Range", "Mode", "Ratio",
                                                      "Knee", "Attack", "Release", "Char", "Stereo", "Link", "Out", "Look" };

    // Stomp 5, DOMINION (0.10.0): dynamic resonance suppression, inspired by
    // oeksound soothe 3 (our own design, see dsp/dominion/Dominion.h). After
    // the EQ, before COMP.
    inline constexpr auto domOn      = "domOn";
    inline constexpr auto domMode    = "domMode";      // choice: Soft, Hard
    inline constexpr auto domDepth   = "domDepth";     // 0 to 20
    inline constexpr auto domDetail  = "domDetail";    // 0 to 100
    inline constexpr auto domAttack  = "domAttack";    // 0 to 100
    inline constexpr auto domRelease = "domRelease";   // 0 to 100
    inline constexpr auto domStereo  = "domStereo";    // choice: L/R, M/S
    inline constexpr auto domLink    = "domLink";      // %
    inline constexpr auto domFocus   = "domFocus";     // -100 to 100
    inline constexpr auto domDetailLow = "domDetailLow", domDetailHigh = "domDetailHigh",
                          domAttackLow = "domAttackLow", domAttackHigh = "domAttackHigh",
                          domReleaseLow = "domReleaseLow", domReleaseHigh = "domReleaseHigh";   // the tilts, -100 to 100
    inline constexpr auto domMaxCut  = "domMaxCut";    // dB (40: no limit)
    inline constexpr auto domWetTrim = "domWetTrim";   // dB
    inline constexpr auto domMix     = "domMix";       // %
    inline constexpr auto domOut     = "domOut";       // dB
    inline constexpr auto domDelta   = "domDelta";
    inline constexpr int domNumBands = 8;
    // The depth curve's bands: "domB1Used", "domB1Freq"... `what` is one of the names below.
    inline juce::String domBand (int band, const char* what) { return "domB" + juce::String (band + 1) + what; }
    inline constexpr const char* domBandSettings[] = { "Used", "On", "Shape", "Freq", "Depth", "Q", "Slope", "Focus" };

    // Stomp 7, the LIMITER (0.8.0): PR7-C's limiter and output trim, ported.
    // The last thing in the chain, after OUTPUT, RIG LEVEL and the volume pedal.
    inline constexpr auto limitOn        = "limitOn";
    inline constexpr auto limitThreshold = "limitThreshold";   // dBFS
    inline constexpr auto limitMakeUp    = "limitMakeUp";      // dB
    inline constexpr auto limitMode      = "limitMode";        // choice: Brick, Soft, Auto
    inline constexpr auto limitMix       = "limitMix";         // %
    inline constexpr auto limitLink      = "limitLink";        // both channels together
    inline constexpr auto limitTrim      = "limitTrim";        // OUTPUT TRIM, dB, after the limiter
    // From 0.16.0, inspired by Pro-L 2 (version hint 36): GAIN drives into the
    // OUTPUT ceiling. limitThreshold and limitMakeUp became GAIN and OUTPUT
    // (projects and rigs from before are converted when they load); they're
    // kept only so old automation and files still find them.
    inline constexpr auto limitGain          = "limitGain";          // dB
    inline constexpr auto limitCeiling       = "limitCeiling";       // OUTPUT, dBFS (dBTP with TRUE PEAK)
    inline constexpr auto limitLookahead     = "limitLookahead";     // ms
    inline constexpr auto limitAttack        = "limitAttack";        // ms, the release stage
    inline constexpr auto limitRelease       = "limitRelease";       // ms
    inline constexpr auto limitLinkTransient = "limitLinkTransient"; // %
    inline constexpr auto limitLinkRelease   = "limitLinkRelease";   // %
    inline constexpr auto limitTruePeak      = "limitTruePeak";
    inline constexpr auto limitOversampling  = "limitOversampling";  // choice: Off, 2x, 4x, 8x
    inline constexpr auto limitDither        = "limitDither";        // choice: Off, 24 ... 16 bits
    inline constexpr auto limitShaping       = "limitShaping";       // choice: None, Basic, Optimized, Weighted
    inline constexpr auto limitUnity         = "limitUnity";         // listening aids: not in rigs
    inline constexpr auto limitDelta         = "limitDelta";
    inline constexpr auto limitDcFilter      = "limitDcFilter";


    // THE SENDS (stage 7b, 0.18.0): PR7-G 0.52.0's DELAY and REVERB, with PR7-G's IDs, ranges and defaults; each
    // oscillator's SendDelay / SendReverb (oscSettings) feeds them, and their MIX is the return's level.
    inline constexpr auto delayOn        = "delayOn";
    inline constexpr auto delayTime      = "delayTime";       // ms (SYNC off)
    inline constexpr auto delaySync      = "delaySync";       // choice: effects::Delay::syncNames
    inline constexpr auto delayFeedback  = "delayFeedback";   // 0 to 0.95
    inline constexpr auto delayTone      = "delayTone";       // 0 to 10
    inline constexpr auto delayMix       = "delayMix";        // 0 to 1: the return's level
    inline constexpr auto delayPingPong  = "delayPingPong";
    inline constexpr auto delayType      = "delayType";       // choice: effects::Delay::typeNames
    inline constexpr auto delayDuck      = "delayDuck";       // 0 to 1
    inline constexpr auto reverbOn       = "reverbOn";
    inline constexpr auto reverbSize     = "reverbSize";      // 0 to 1
    inline constexpr auto reverbTone     = "reverbTone";      // 0 to 10
    inline constexpr auto reverbMix      = "reverbMix";       // 0 to 1: the return's level
    inline constexpr auto reverbType     = "reverbType";      // choice: effects::Reverb::typeNames
    inline constexpr auto reverbPreDelay = "reverbPreDelay";  // ms
    inline constexpr auto reverbDuck     = "reverbDuck";      // 0 to 1
    inline constexpr auto delayToReverb  = "delayToReverb";   // PR7-K's own: how much of DELAY's echoes go on into REVERB

    // MOD, MOD 2 and LO-FI (stage 7c, version hint 18): PR7-G 0.52.0's IDs, ranges and defaults, so the same names
    // mean the same knobs across the family. Not PR7-G's MOD POSITION: PR7-K has no amp to put them before.
    inline constexpr auto modOn      = "modOn";
    inline constexpr auto modType    = "modType";      // choice: effects::Modulation::typeNames
    inline constexpr auto modRate    = "modRate";      // Hz (SYNC off)
    inline constexpr auto modDepth   = "modDepth";     // 0 to 1
    inline constexpr auto modMix     = "modMix";       // 0 to 1 (E.LEVEL for the Vortex)
    inline constexpr auto modSync    = "modSync";      // choice: effects::Delay::syncNames ("Off" first)
    inline constexpr auto modEq      = "modEq";        // the Vortex's EQ, -1 to 1
    inline constexpr auto modStereo  = "modStereo";    // the Vortex's STEREO
    inline constexpr auto modManual  = "modManual";    // AEON's MANUAL, 0 to 10
    inline constexpr auto modRes     = "modRes";       // AEON's RES, 0 to 10
    inline constexpr auto modHadron  = "modHadron";    // Hadron's SCRIPT / BLOCK circuit
    inline constexpr auto modWave    = "modWave";      // Razor's WAVE, 0 to 10
    inline constexpr auto mod2On     = "mod2On";
    inline constexpr auto mod2Type   = "mod2Type";
    inline constexpr auto mod2Rate   = "mod2Rate";
    inline constexpr auto mod2Depth  = "mod2Depth";
    inline constexpr auto mod2Mix    = "mod2Mix";
    inline constexpr auto mod2Sync   = "mod2Sync";
    inline constexpr auto mod2Eq     = "mod2Eq";
    inline constexpr auto mod2Stereo = "mod2Stereo";
    inline constexpr auto mod2Manual = "mod2Manual";
    inline constexpr auto mod2Res    = "mod2Res";
    inline constexpr auto mod2Hadron = "mod2Hadron";
    inline constexpr auto mod2Wave   = "mod2Wave";

    // A MOD block's controls, so MOD and MOD 2 share their code (as on PR7-G).
    struct ModIds
    {
        const char *on, *type, *rate, *depth, *mix, *sync, *eq, *stereo, *manual, *res, *hadron, *wave;
    };
    inline constexpr ModIds modIds { modOn, modType, modRate, modDepth, modMix, modSync, modEq, modStereo, modManual, modRes, modHadron, modWave };
    inline constexpr ModIds mod2Ids { mod2On, mod2Type, mod2Rate, mod2Depth, mod2Mix, mod2Sync, mod2Eq, mod2Stereo, mod2Manual, mod2Res, mod2Hadron, mod2Wave };

    inline constexpr auto lofiOn     = "lofiOn";
    inline constexpr auto lofiType   = "lofiType";     // choice: effects::LoFi::typeNames
    inline constexpr auto lofiAmount = "lofiAmount";   // 0 to 10
    inline constexpr auto lofiNoise  = "lofiNoise";    // 0 to 10
    inline constexpr auto lofiMix    = "lofiMix";      // 0 to 1
    inline constexpr auto lofiLevel  = "lofiLevel";    // dB

    // ECHOES (0.53.0, version hint 27): the past played back under the present (effects::Echoes), after LO-FI.
    inline constexpr auto echoOn       = "echoOn";
    inline constexpr auto echoMode     = "echoMode";       // choice: effects::Echoes::modeNames
    inline constexpr auto echoDistance = "echoDistance";   // choice: effects::Echoes::distanceNames
    inline constexpr auto echoMix      = "echoMix";        // 0 to 1
    inline constexpr auto echoFeedback = "echoFeedback";   // 0 to 1 (LOOP: repeats)
    inline constexpr auto echoAge      = "echoAge";        // 0 to 1
    // CRACKLE (0.56.0, version hint 28, effects::Crackle): a radiation counter's random clicks.
    inline constexpr auto crackleOn      = "crackleOn";
    inline constexpr auto crackleRate    = "crackleRate";      // clicks a second, 0.5 to 2000 (log)
    inline constexpr auto crackleCluster = "crackleCluster";   // 0 to 1
    inline constexpr auto crackleTone    = "crackleTone";      // Hz, 300 to 12000 (log)
    inline constexpr auto crackleWidth   = "crackleWidth";     // 0 to 1
    inline constexpr auto crackleFollow  = "crackleFollow";    // 0 to 1
    inline constexpr auto crackleLevel   = "crackleLevel";     // dB, -48 to +6
    // DOWNPOUR (0.57.0, version hint 29, effects::Downpour): rain on the glass, the drops on the notes sounding.
    inline constexpr auto downpourOn       = "downpourOn";
    inline constexpr auto downpourRain     = "downpourRain";       // 0 to 1
    inline constexpr auto downpourDrops    = "downpourDrops";      // 0 to 1
    inline constexpr auto downpourOctave   = "downpourOctave";     // choice: effects::Downpour::octaveNames
    inline constexpr auto downpourDistance = "downpourDistance";   // 0 to 1
    inline constexpr auto downpourSpace    = "downpourSpace";      // 0 to 1: into the REVERB's send
    inline constexpr auto downpourWidth    = "downpourWidth";      // 0 to 1
    inline constexpr auto downpourFollow   = "downpourFollow";     // 0 to 1
    inline constexpr auto downpourLevel    = "downpourLevel";      // dB, -48 to +6
    // INFERNAL (0.59.0, version hint 31, effects::Infernal): parallel destruction, HELLFORGE's signature.
    inline constexpr auto infernalOn       = "infernalOn";
    inline constexpr auto infernalDrive    = "infernalDrive";      // 0 to 1
    inline constexpr auto infernalTight    = "infernalTight";      // 0 to 1: the clean split, 30-400 Hz
    inline constexpr auto infernalFuzz     = "infernalFuzz";       // 0 to 1: each chain's blend
    inline constexpr auto infernalCrush    = "infernalCrush";
    inline constexpr auto infernalAmp      = "infernalAmp";
    inline constexpr auto infernalNoise    = "infernalNoise";
    inline constexpr auto infernalFeedback = "infernalFeedback";   // 0 to 1: the AMP's bloom
    inline constexpr auto infernalCab      = "infernalCab";        // 0 to 1: the cabinet dark to bright
    inline constexpr auto infernalMix      = "infernalMix";        // 0 to 1
    inline constexpr auto infernalLevel    = "infernalLevel";      // dB, -24 to +12
    // AFTERGLOW (0.60.0, version hint 32): STARCOURT's signature. AGE and DRIFT go to the synth's notes
    // (synth::Settings::afterglow), ENSEMBLE and TAPE to effects::Afterglow.
    inline constexpr auto afterglowOn       = "afterglowOn";
    inline constexpr auto afterglowAge      = "afterglowAge";       // 0 to 1: each note's own tuning, filter and timing
    inline constexpr auto afterglowDrift    = "afterglowDrift";     // 0 to 1: each note's slow wander
    inline constexpr auto afterglowEnsemble = "afterglowEnsemble";  // 0 to 1
    inline constexpr auto afterglowTape     = "afterglowTape";      // 0 to 1
    // LIGHTTRAIL (0.61.0, version hint 33, effects::LightTrail): DATASTREAM's signature.
    inline constexpr auto trailOn       = "trailOn";
    inline constexpr auto trailDivision = "trailDivision";   // LightTrail::divisionNames
    inline constexpr auto trailLength   = "trailLength";     // 0 to 1
    inline constexpr auto trailRise     = "trailRise";       // semitones each repeat, -12 to +12
    inline constexpr auto trailTone     = "trailTone";       // 0 to 1
    inline constexpr auto trailSpread   = "trailSpread";     // 0 to 1
    inline constexpr auto trailMix      = "trailMix";        // 0 to 1
    // BLACKHOLE (0.62.0, version hint 34, effects::BlackHole): LAST LIGHT's signature.
    inline constexpr auto blackholeOn    = "blackholeOn";
    inline constexpr auto blackholeHold  = "blackholeHold";    // 0 to 1: 0.5 s to 20 s
    inline constexpr auto blackholeSize  = "blackholeSize";    // 0 to 1: 0.3 s to 3 s back
    inline constexpr auto blackholePull  = "blackholePull";    // 0 to 1: 0 to -2 semitones a pass
    inline constexpr auto blackholeTone  = "blackholeTone";    // 0 to 1
    inline constexpr auto blackholeWidth = "blackholeWidth";   // 0 to 1
    inline constexpr auto blackholeMix   = "blackholeMix";     // 0 to 1
    // DILATION (0.63.0, version hint 35, effects::Dilation): SIMULACRUM's signature.
    inline constexpr auto dilationOn     = "dilationOn";
    inline constexpr auto dilationSlow   = "dilationSlow";     // 0 to 1: speed 1 to 1/4
    inline constexpr auto dilationRamp   = "dilationRamp";     // 0 to 1: 0.05 s to 4 s
    inline constexpr auto dilationReturn = "dilationReturn";   // Dilation::returnNames
    inline constexpr auto dilationReach  = "dilationReach";    // 0 to 1: 1 s to 8 s behind at most
    inline constexpr auto dilationTone   = "dilationTone";     // 0 to 1
    inline constexpr auto dilationMix    = "dilationMix";      // 0 to 1
    // REMNANT (0.64.0, version hint 36, effects::Remnant): DREAMSTATE's signature.
    inline constexpr auto remnantOn     = "remnantOn";
    inline constexpr auto remnantLayers = "remnantLayers";   // Remnant::layerNames: 1 to 3
    inline constexpr auto remnantStep   = "remnantStep";     // Remnant::stepNames
    inline constexpr auto remnantLength = "remnantLength";   // 0 to 1: 0.4 s to 4 s
    inline constexpr auto remnantTaper  = "remnantTaper";    // 0 to 1: each deeper layer 0 to 12 dB quieter
    inline constexpr auto remnantTone   = "remnantTone";     // 0 to 1
    inline constexpr auto remnantWidth  = "remnantWidth";    // 0 to 1
    inline constexpr auto remnantMix    = "remnantMix";      // 0 to 1
    // PROXIMITY (0.65.0, version hint 37, effects::Proximity): NEGATIVE SPACE's signature.
    inline constexpr auto proximityOn    = "proximityOn";
    inline constexpr auto proximityNoise = "proximityNoise";   // 0 to 1: the touches, -48 to -6 dB (0 none)
    inline constexpr auto proximityTone  = "proximityTone";    // 0 to 1: 800 Hz to 6.4 kHz
    inline constexpr auto proximityRoom  = "proximityRoom";    // 0 to 1: -72 to -30 dB (0 none)
    inline constexpr auto proximityHum   = "proximityHum";     // Proximity::humNames
    inline constexpr auto proximityClose = "proximityClose";   // 0 to 1: 0 to +6 dB at 180 Hz
    inline constexpr auto proximityWidth = "proximityWidth";   // 0 to 1
    // HAZE (0.66.0, version hint 38, effects::Haze): NIGHTDRIVE's signature.
    inline constexpr auto hazeOn       = "hazeOn";
    inline constexpr auto hazeDivision = "hazeDivision";   // Haze::divisionNames: a pass every half bar to 4 bars
    inline constexpr auto hazeDepth    = "hazeDepth";      // 0 to 1
    inline constexpr auto hazeDoppler  = "hazeDoppler";    // 0 to 1: 0 to 40 cents at the steepest
    inline constexpr auto hazeWidth    = "hazeWidth";      // 0 to 1
    inline constexpr auto hazeTone     = "hazeTone";       // 0 to 1: far cutoff 1.5 to 12 kHz
    inline constexpr auto hazeWhoosh   = "hazeWhoosh";     // 0 to 1
    // SPECTRE (0.67.0, version hint 39, effects::Spectre): CYBERSHRINE's signature.
    inline constexpr auto spectreOn     = "spectreOn";
    inline constexpr auto spectreMix    = "spectreMix";      // 0 to 1
    inline constexpr auto spectreTrail  = "spectreTrail";    // 0 to 1: 0 to 800 ms behind
    inline constexpr auto spectreLinger = "spectreLinger";   // 0 to 1: 0.1 to 4 s to fall 60 dB
    inline constexpr auto spectreAir    = "spectreAir";      // 0 hollow to 1 breathy
    inline constexpr auto spectreDrift  = "spectreDrift";    // 0 to 1
    inline constexpr auto spectreShift  = "spectreShift";    // -1 to 1: an octave down to an octave up
    // LEVIATHAN (0.68.0, version hint 40, effects::Leviathan): UNSPOKEN's signature.
    inline constexpr auto leviathanOn    = "leviathanOn";
    inline constexpr auto leviathanMix   = "leviathanMix";     // 0 to 1
    inline constexpr auto leviathanDepth = "leviathanDepth";   // Leviathan::depthNames: 1 or 2 octaves below
    inline constexpr auto leviathanDrift = "leviathanDrift";   // 0 to 1: the vowel moves every 6 s down to 0.8 s
    inline constexpr auto leviathanGrowl = "leviathanGrowl";   // 0 to 1
    inline constexpr auto leviathanDelay = "leviathanDelay";   // 0 to 1: 0 to 1.5 s behind
    inline constexpr auto leviathanSize  = "leviathanSize";    // 0 to 1: formants x 1 down to x 0.55
    // NUCLEARSPRING (0.69.0, version hint 41, effects::NuclearSpring): ATOMIC AGE's signature.
    inline constexpr auto springOn    = "springOn";
    inline constexpr auto springMix   = "springMix";     // 0 to 1
    inline constexpr auto springDecay = "springDecay";   // 0 to 1: 0.8 to 5 s
    inline constexpr auto springDrip  = "springDrip";    // 0 to 1
    inline constexpr auto springTone  = "springTone";    // 0 to 1: 2 to 8 kHz
    inline constexpr auto springSize  = "springSize";    // 0 to 1: the springs x 0.7 to x 1.4
    inline constexpr auto springBoing = "springBoing";   // 0 to 1
    inline constexpr auto springWidth = "springWidth";   // 0 to 1
    // LIGHTSPEED (0.73.0-0.85.0, version hint 42) was removed in 0.86.0 at the owner's word: never reuse its IDs
    // (jumpOn jumpAmount jumpRise jumpStreak jumpWhoosh jumpWidth jumpTone) or version hint 42.
    // BLOOM (0.74.0, version hint 43, effects::Bloom): GRAND CINEMA's signature.
    inline constexpr auto bloomOn        = "bloomOn";
    inline constexpr auto bloomAmount    = "bloomAmount";      // 0 to 1: how much (the macro's)
    inline constexpr auto bloomResonance = "bloomResonance";   // 0 to 1: the sympathetic strings' share
    inline constexpr auto bloomRing      = "bloomRing";        // 0 to 1: the strings' fall, 1.5 to 12 s
    inline constexpr auto bloomSize      = "bloomSize";        // 0 to 1: the hall's fall, 2 to 12 s
    inline constexpr auto bloomSwell     = "bloomSwell";       // 0 to 1: the hall opening, 0.15 to 4 s
    inline constexpr auto bloomTone      = "bloomTone";        // 0 to 1: 1.5 to 15 kHz
    inline constexpr auto bloomWidth     = "bloomWidth";       // 0 to 1
    // LEGION (0.76.0, version hint 44, effects::Legion): MAESTRO's signature.
    inline constexpr auto legionOn     = "legionOn";
    inline constexpr auto legionSize   = "legionSize";     // 0 to 1: one player to a section of nine (the macro's)
    inline constexpr auto legionSpread = "legionSpread";   // 0 to 1: how wide the stage
    inline constexpr auto legionLoose  = "legionLoose";    // 0 to 1: the latest player 6 to 30 ms behind
    inline constexpr auto legionDetune = "legionDetune";   // 0 to 1: drifting up to 0 to 14 cents
    inline constexpr auto legionTone   = "legionTone";     // 0 to 1: the players' top, 3 to 16 kHz
    // OVERCHARGE (0.77.0, version hint 45, effects::Overcharge): TWIN SPIRES' signature.
    inline constexpr auto overchargeOn     = "overchargeOn";
    inline constexpr auto overchargeCharge = "overchargeCharge";   // 0 to 1: the first half charges, the second overloads (the macro's)
    inline constexpr auto overchargeGlow   = "overchargeGlow";     // 0 to 1: the crystal tones' share
    inline constexpr auto overchargeChaos  = "overchargeChaos";    // 0 to 1: how violent and how often the overload bursts
    inline constexpr auto overchargeTone   = "overchargeTone";     // 0 to 1: the glow's top, 3 to 16 kHz
    inline constexpr auto overchargeWidth  = "overchargeWidth";    // 0 to 1: how far the glow spreads

    // AFTERMATH (0.79.0, version hint 46, effects::Aftermath): STOLEN SKY's signature.
    inline constexpr auto aftermathOn    = "aftermathOn";
    inline constexpr auto aftermathSeal  = "aftermathSeal";    // 0 to 1: open sky to sealed bunker (the macro's)
    inline constexpr auto aftermathRoom  = "aftermathRoom";    // 0 to 1: the bunker's size
    inline constexpr auto aftermathRing  = "aftermathRing";    // 0 to 1: how long the walls ring
    inline constexpr auto aftermathDark  = "aftermathDark";    // 0 to 1: how dark when sealed, 6 kHz down to 1.2 kHz
    inline constexpr auto aftermathHum   = "aftermathHum";     // 0 to 1: the machinery, -66 to -30 dB (0 none)
    inline constexpr auto aftermathHumHz = "aftermathHumHz";   // Aftermath::humNames
    inline constexpr auto aftermathWidth = "aftermathWidth";   // 0 to 1: the room's width
    // SUBMERGE (0.80.0, version hint 47, effects::Submerge): DROWNED DECO's signature.
    inline constexpr auto submergeOn      = "submergeOn";
    inline constexpr auto submergeDepth   = "submergeDepth";     // 0 to 1: dry land to the sea floor (the macro's)
    inline constexpr auto submergeMurk    = "submergeMurk";      // 0 to 1: how dark at full depth, 4 kHz down to 400 Hz
    inline constexpr auto submergeCurrent = "submergeCurrent";   // 0 to 1: the pressure's pitch bend, up to about +-20 cents
    inline constexpr auto submergeBubbles = "submergeBubbles";   // 0 to 1: how many bubbles rise
    inline constexpr auto submergeSea     = "submergeSea";       // 0 to 1: the sea's wash
    inline constexpr auto submergeWidth   = "submergeWidth";     // 0 to 1: how wide the bubbles and the sea
    // LOCKSTEP (0.81.0, version hint 48, effects::Lockstep): NEW EDEN's signature.
    inline constexpr auto lockstepOn       = "lockstepOn";
    inline constexpr auto lockstepAmount   = "lockstepAmount";     // 0 to 1: free to drilled (the macro's)
    inline constexpr auto lockstepDivision = "lockstepDivision";   // Lockstep::divisionNames
    inline constexpr auto lockstepGate     = "lockstepGate";       // 0 to 1: each step open 15% to 90%
    inline constexpr auto lockstepAccent   = "lockstepAccent";     // 0 to 1: the off-beat steps lower
    inline constexpr auto lockstepSqueeze  = "lockstepSqueeze";    // 0 to 1: how flat
    inline constexpr auto lockstepStamp    = "lockstepStamp";      // 0 to 1: the boots

    // COLOSSUS (0.82.0, version hint 50, effects::Colossus): SANDSEA's signature.
    inline constexpr auto colossusOn     = "colossusOn";
    inline constexpr auto colossusSize   = "colossusSize";     // 0 to 1: as it is to enormous (the macro's)
    inline constexpr auto colossusDepth  = "colossusDepth";    // 0 to 1: the octave under
    inline constexpr auto colossusWeight = "colossusWeight";   // 0 to 1: how hard it's driven
    inline constexpr auto colossusHall   = "colossusHall";     // 0 to 1: the hall, 2 to 10 s
    inline constexpr auto colossusBoom   = "colossusBoom";     // 0 to 1: the impact under each note
    inline constexpr auto colossusTone   = "colossusTone";     // 0 to 1: dark to bright
    inline constexpr auto colossusWidth  = "colossusWidth";    // 0 to 1: the hall's width

    // CANYON (0.84.0, version hint 52, effects::Canyon): the western bank's signature.
    inline constexpr auto canyonOn      = "canyonOn";
    inline constexpr auto canyonAmount  = "canyonAmount";    // 0 to 1: how much goes into the canyon (the macro's)
    inline constexpr auto canyonSize    = "canyonSize";      // 0 to 1: the near walls, 45 to 200 ms
    inline constexpr auto canyonFar     = "canyonFar";       // effects::Canyon::farNames: the far wall, in time with the song
    inline constexpr auto canyonRepeats = "canyonRepeats";   // 0 to 1: how many replies come back
    inline constexpr auto canyonTone    = "canyonTone";      // 0 to 1: dark to bright
    inline constexpr auto canyonWidth   = "canyonWidth";     // 0 to 1: how far to the sides the walls are

    // JACK-IN (0.85.0, version hint 53, effects::JackIn): MACHINE SUPREMACY's signature.
    inline constexpr auto jackinOn     = "jackinOn";
    inline constexpr auto jackinAmount = "jackinAmount";   // 0 to 1: how much is taken over (the macro's)
    inline constexpr auto jackinSlice  = "jackinSlice";    // effects::JackIn::sliceNames: the slices, in time with the song
    inline constexpr auto jackinReach  = "jackinReach";    // effects::JackIn::reachNames: how far back the taken slices come from
    inline constexpr auto jackinChaos  = "jackinChaos";    // 0 to 1: replays and stutters to reverses and octaves
    inline constexpr auto jackinCrush  = "jackinCrush";    // 0 to 1: how often and how hard taken slices are crushed
    inline constexpr auto jackinDrops  = "jackinDrops";    // 0 to 1: how often a beat ends in a tape stop
    // HYPERJUMP (0.86.0, version hint 54, effects::HyperJump): HYPERDRIVE's build and drop.
    inline constexpr auto hyperjumpOn     = "hyperjumpOn";
    inline constexpr auto hyperjumpAmount = "hyperjumpAmount";   // 0 to 1: how big (the macro's)
    inline constexpr auto hyperjumpLength = "hyperjumpLength";   // effects::HyperJump::lengthNames
    inline constexpr auto hyperjumpMode   = "hyperjumpMode";     // effects::HyperJump::modeNames
    inline constexpr auto hyperjumpRise   = "hyperjumpRise";     // 0 to 1: the climb's top, 0 to +12 semitones
    inline constexpr auto hyperjumpChop   = "hyperjumpChop";     // 0 to 1
    inline constexpr auto hyperjumpHit    = "hyperjumpHit";      // 0 to 1: the drop's boom and space
    // SWEEP (0.87.0, version hint 55, effects::Sweep): LAST DAY's song-locked resonant sweep.
    inline constexpr auto sweepOn        = "sweepOn";
    inline constexpr auto sweepAmount    = "sweepAmount";      // 0 to 1 (the macro's)
    inline constexpr auto sweepLength    = "sweepLength";      // effects::Sweep::lengthNames
    inline constexpr auto sweepShape     = "sweepShape";       // effects::Sweep::shapeNames
    inline constexpr auto sweepFilter    = "sweepFilter";      // effects::Sweep::filterNames
    inline constexpr auto sweepCenter    = "sweepCenter";      // 0 to 1: 150 Hz to 4 kHz
    inline constexpr auto sweepRange     = "sweepRange";       // 0 to 1: 2 to 9 octaves
    inline constexpr auto sweepResonance = "sweepResonance";   // 0 to 1
    inline constexpr auto sweepPhaser    = "sweepPhaser";      // 0 to 1
    // NAVICOMPUTER (0.86.0, version hint 54, synth::navi): every note moved by a pattern in key, in time. In presets.
    inline constexpr auto naviOn       = "naviOn";
    inline constexpr auto naviAmount   = "naviAmount";     // 0 to 1: how many steps move (the macro's)
    inline constexpr auto naviPattern  = "naviPattern";    // synth::navi::patternNames
    inline constexpr auto naviDivision = "naviDivision";   // synth::navi::divisionNames
    inline constexpr auto naviKey      = "naviKey";        // synth::sirocco::keyNames
    inline constexpr auto naviScale    = "naviScale";      // synth::arp::scaleNames
    inline constexpr auto naviChaos    = "naviChaos";      // 0 to 1
    inline constexpr auto naviGlide    = "naviGlide";      // 0 to 1

    // THE INSERT BUSES (stage 7d, version hint 19): A to D, each "ins1"... "ins4" plus a setting below: PR7-G's
    // DRIVE and GLITCH (their IDs after the prefix, capitalised), then MOD and LO-FI as above, then the bus's SENDS.
    inline constexpr int numInsertBuses = 4;
    inline juce::String ins (int bus, const char* what) { return "ins" + juce::String (bus + 1) + what; }
    inline const juce::StringArray insertBusNames { "None", "A", "B", "C", "D" };   // osc{n}Insert, by index: only append
    inline constexpr const char* insDriveSettings[] = { "DriveOn", "DriveModel", "DriveGain", "DriveTone", "DriveLevel", "DriveAttack",
                                                        "DriveGate", "DriveHigh", "DriveLow", "DriveMiddle", "DriveMidFreq" };
    inline constexpr const char* insGlitchSettings[] = { "GlitchOn", "GlitchMode", "GlitchSync", "GlitchTime", "GlitchDivision",
                                                         "GlitchDensity", "GlitchPitch", "GlitchChaos", "GlitchMix" };
    inline constexpr const char* insModSettings[] = { "ModOn", "ModType", "ModRate", "ModDepth", "ModMix", "ModSync", "ModEq", "ModStereo",
                                                      "ModManual", "ModRes", "ModHadron", "ModWave" };   // ModIds' order
    inline constexpr const char* insLofiSettings[] = { "LofiOn", "LofiType", "LofiAmount", "LofiNoise", "LofiMix", "LofiLevel" };
    // 0.51.0 (version hint 25): the bus's WAH, first on it, before the DRIVE (effects::Wah).
    inline constexpr const char* insWahSettings[] = { "WahOn", "WahMode", "WahRange", "WahPosition", "WahResonance", "WahSensitivity",
                                                      "WahSpeed", "WahDepth", "WahRate", "WahSync", "WahMix", "WahLevel" };
    // A bus's MOD as ModIds (the strings made once and kept).
    inline const ModIds& insModIds (int bus)
    {
        static const auto table = []
        {
            std::array<std::array<juce::String, std::size (insModSettings)>, numInsertBuses> names;
            for (int b = 0; b < numInsertBuses; ++b)
                for (size_t k = 0; k < std::size (insModSettings); ++k)
                    names[(size_t) b][k] = ins (b, insModSettings[k]);
            return names;
        }();
        static const auto ids = []
        {
            std::array<ModIds, numInsertBuses> result {};
            for (size_t b = 0; b < (size_t) numInsertBuses; ++b)
            {
                const auto& n = table[b];
                result[b] = { n[0].toRawUTF8(), n[1].toRawUTF8(), n[2].toRawUTF8(), n[3].toRawUTF8(), n[4].toRawUTF8(), n[5].toRawUTF8(),
                              n[6].toRawUTF8(), n[7].toRawUTF8(), n[8].toRawUTF8(), n[9].toRawUTF8(), n[10].toRawUTF8(), n[11].toRawUTF8() };
            }
            return result;
        }();
        return ids[(size_t) juce::jlimit (0, numInsertBuses - 1, bus)];
    }

    // Global settings: kept when switching presets. Everything else is part of a preset.
    // The LIMITER's UNITY and DELTA are for listening while you set it, not part of a sound.
    inline const juce::StringArray notInRigs { outputGain, bypass, tune, mwBreath, mwExpression, mwAftertouch, mwSustain, limitUnity, limitDelta, exprVolume };
}

namespace ParamRanges
{
    // COMP's, as on PR7-C.
    inline constexpr float compThresholdMin = -50.0f, compThresholdMax = 0.0f;
    inline constexpr float compAttackMin = 0.1f, compAttackMax = 30.0f;          // ms
    inline constexpr float compReleaseMin = 50.0f, compReleaseMax = 3000.0f;     // ms
    inline constexpr float compMakeUpMax = 20.0f;
    inline const juce::StringArray compCharacters { "VCA", "OPTO", "FET" };    // saved by index: only append
    inline constexpr float compRatioMarks[] = { 1.0f, 1.5f, 2.0f, 4.0f, 10.0f, 30.0f };   // SOFT, then to infinite

    // The LIMITER's, as on PR7-C.
    inline constexpr float limitThresholdMin = -20.0f, limitMakeUpMax = 12.0f;
    inline constexpr float limitTrimMin = -12.0f, limitTrimMax = 12.0f;
    // Its styles: PR7-C's three, then (0.16.0) eight of our own after Pro-L 2's (effects::limiter::Style).
    inline const juce::StringArray limitModes { "Brick", "Soft", "Auto",   // saved by index: only append
                                                "Clear", "Punchy", "Dynamic", "All-round", "Aggressive", "Modern", "Glue", "Safe" };
    inline constexpr int limitDefaultMode = 8;   // Modern
    inline constexpr float limitGainMax = 30.0f, limitCeilingMin = -30.0f;

    inline constexpr float gainMinDb = -24.0f;
    inline constexpr float gainMaxDb = 12.0f;
    inline constexpr float outputDefaultDb = -6.0f;   // headroom for chords (Avenger's master gain defaults to -3 dB)
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
void addRackParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);   // RackParameters.cpp
void addSendParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);   // RackParameters.cpp (stage 7b)
void addModFxParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);   // RackParameters.cpp (stage 7c)
void addInsertParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);
void addEchoesParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);
void addCrackleParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);   // RackParameters.cpp (0.56.0)
void addDownpourParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);  // RackParameters.cpp (0.57.0)
void addInfernalParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);  // RackParameters.cpp (0.59.0)
void addAfterglowParameters (juce::AudioProcessorValueTreeState::ParameterLayout&); // RackParameters.cpp (0.60.0)
void addLightTrailParameters (juce::AudioProcessorValueTreeState::ParameterLayout&); // RackParameters.cpp (0.61.0)
void addBlackHoleParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);  // RackParameters.cpp (0.62.0)
void addDilationParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);   // RackParameters.cpp (0.63.0)
void addRemnantParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);    // RackParameters.cpp (0.64.0)
void addProximityParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);  // RackParameters.cpp (0.65.0)
void addHazeParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);       // RackParameters.cpp (0.66.0)
void addSpectreParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);    // RackParameters.cpp (0.67.0)
void addLeviathanParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);  // RackParameters.cpp (0.68.0)
void addNuclearSpringParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);   // RackParameters.cpp (0.69.0)
void addBloomParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);        // RackParameters.cpp (0.74.0)
void addLegionParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);       // RackParameters.cpp (0.76.0)
void addOverchargeParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);   // RackParameters.cpp (0.77.0)
void addAftermathParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);    // RackParameters.cpp (0.79.0)
void addSubmergeParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);     // RackParameters.cpp (0.80.0)
void addLockstepParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);     // RackParameters.cpp (0.81.0)
void addColossusParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);     // RackParameters.cpp (0.82.0)
void addCanyonParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);       // RackParameters.cpp (0.84.0)
void addJackInParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);       // RackParameters.cpp (0.85.0)
void addHyperJumpParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);    // RackParameters.cpp (0.86.0)
void addSweepParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);        // RackParameters.cpp (0.87.0)
   // RackParameters.cpp (0.53.0)   // RackParameters.cpp (stage 7d)
