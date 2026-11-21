#include "DistinctPass.h"

// THE DISTINCT PASS (1.8.0): every expansion preset that measured within 3 of a preset in another bank (or its own),
// changed to be its own. The earlier bank's preset of each pair is kept as approved; the later one changes here, its
// name and part kept (a saloon piano stays a saloon piano), its character its own. Each group's lines say what it was
// near and what makes it different now. Our own choices, measured (the [.audit] test), not tuned by ear.
const juce::String& distinctPassTable()
{
    static const juce::String table = R"PASS(# Each preset's lines; the first line of a group says what it was near (the measure, 1.8.0's audit) and what
# makes it its own now. Then LEVEL set so each is as loud as before (the probe's power, within 1 dB).
# --- Pianos built from the shared felt and soft piano recipes (about 26 measured within 3 of one another)
# Echo Piano: a slow bloom, dark, a shimmer a twelfth up
Echo Piano|synth|p|amp1Decay=11000
# Soft Felt Piano: brighter, shorter, more of the 3rd harmonic
Soft Felt Piano|synth|p|flt1Cutoff=1400
Soft Felt Piano|synth|p|amp1Decay=4000
# Home Piano: an upright at home: shorter, brighter, its second string 4 cents off and louder
Home Piano|synth|p|amp1Decay=2500
Home Piano|synth|p|flt1Cutoff=1700
Home Piano|synth|p|osc2Fine=4
Home Piano|synth|p|osc2Volume=0.6
# Lament Piano: long, dark and bare (no 2nd harmonic)
Lament Piano|synth|p|flt1Cutoff=560
Lament Piano|synth|p|amp1Decay=12000
Lament Piano|synth|p|osc3Volume=0.0
# Distant Piano: far off: very dark, a soft 18 ms attack
Distant Piano|synth|p|flt1Cutoff=420
Distant Piano|synth|p|amp1Attack=18
Distant Piano|synth|p|amp1Decay=8000
# Intimate Felt: the softest felt: dark, a soft attack, no octave or thump
Intimate Felt|synth|p|amp1Decay=5000
Intimate Felt|synth|p|osc3Volume=0.0
Intimate Felt|synth|p|osc4Volume=0.0
# Lone Melody: held a little (sustain 0.28), brighter, an octave shimmer
Lone Melody|synth|p|flt1Cutoff=1700
Lone Melody|synth|p|osc3Volume=0.22
# Prepared Grand: metallic: the hammer tone 35 cents off the octave, struck harder
Prepared Grand|synth|p|osc2Transpose=12
Prepared Grand|synth|p|osc2Fine=35
Prepared Grand|synth|p|osc1FmAmount=0.2
# Saloon Piano: honky-tonk: a second string 22 cents sharp, bright, a short ring
Saloon Piano|synth|p|osc5On=1
Saloon Piano|synth|p|osc5Wave=3
Saloon Piano|synth|p|osc5PhaseRand=0
Saloon Piano|synth|p|flt1Cutoff=2600
# Player Piano: bright and mechanical: hard hammers, a short ring
Player Piano|synth|p|flt1Cutoff=3600
Player Piano|synth|p|osc1FmAmount=0.13
Player Piano|synth|p|amp1Decay=2500
Player Piano|synth|p|osc3Volume=0.26
# Unwound Piano: out of tune: its octave string 45 cents sharp, a wow in the pitch
Unwound Piano|synth|p|flt1Cutoff=1000
Unwound Piano|synth|p|osc1VibRate=0.6
# Abandoned Upright: a short, dull ring, its octave string flat
Abandoned Upright|synth|p|flt1Cutoff=780
Abandoned Upright|synth|p|amp1Decay=2000
Abandoned Upright|synth|p|osc3Fine=-32
Abandoned Upright|synth|p|osc3Volume=0.18
# Contraband Piano: muffled and short, played in hiding
Contraband Piano|synth|p|osc3Volume=0.0
# Pawn Shop Piano: tinny: bright, a loud octave string, a short ring
Pawn Shop Piano|synth|p|flt1Cutoff=3000
Pawn Shop Piano|synth|p|osc3Volume=0.42
Pawn Shop Piano|synth|p|amp1Decay=1500
# Cold Morning Piano: glassy: pure, a faint string two octaves up, a long ring
Cold Morning Piano|synth|p|amp1Decay=10000
# Elevator Muzak: an electric piano's bell (FM at 2:1)
Elevator Muzak|synth|p|osc2Transpose=12
Elevator Muzak|synth|p|osc1FmAmount=0.12
Elevator Muzak|synth|p|amp1Decay=3200
# Falling Ash Piano: high and fragile: bright, an octave shimmer, a short ring
Falling Ash Piano|synth|p|flt1Cutoff=2700
Falling Ash Piano|synth|p|amp1Decay=1200
Falling Ash Piano|synth|p|osc3Volume=0.32
# Quiet Kitchen Piano: a domestic upright: shorter, its second string 5 cents off
Quiet Kitchen Piano|synth|p|amp1Decay=3400
Quiet Kitchen Piano|synth|p|flt1Cutoff=1450
Quiet Kitchen Piano|synth|p|osc2Fine=5
Quiet Kitchen Piano|synth|p|osc2Volume=0.55
# Underground Piano: a bunker piano: dark, a body an octave below
Underground Piano|synth|p|amp1Decay=5000
# Memory Loop: a tape loop: a slow wow in the pitch, dark
Memory Loop|synth|p|osc1VibAmount=12
Memory Loop|synth|p|osc1VibRate=0.7
Memory Loop|synth|p|osc2VibAmount=12
Memory Loop|synth|p|osc2VibRate=0.7
Memory Loop|synth|p|flt1Cutoff=680
# Campfire Piano: an outdoor upright: short, a second string 8 cents off
Campfire Piano|synth|p|amp1Decay=2200
Campfire Piano|synth|p|flt1Cutoff=1900
Campfire Piano|synth|p|osc5On=1
Campfire Piano|synth|p|osc5Wave=3
Campfire Piano|synth|p|osc5Fine=8
Campfire Piano|synth|p|osc5Volume=0.45
Campfire Piano|synth|p|osc5PhaseRand=0
# Rationed Piano: worn metal: the hammer tone at a fifth (inharmonic), short
Rationed Piano|synth|p|flt1Cutoff=900
Rationed Piano|synth|p|amp1Decay=2800
Rationed Piano|synth|p|osc3Fine=18
# Cocktail Piano: a bright jazz piano: harder hammers, held a little
Cocktail Piano|synth|p|amp1Decay=4500
Cocktail Piano|synth|p|osc3Volume=0.15
Echo Piano|synth|p|amp1Attack=90
Echo Piano|synth|p|flt1Cutoff=520
# Heartbreak Piano: warm and held: more hammer, a held sustain
Heartbreak Piano|synth|p|flt1Cutoff=1700
Cold Morning Piano|synth|p|flt1Cutoff=4000
Cold Morning Piano|synth|p|osc1FmAmount=0.02
Cold Morning Piano|synth|p|osc3Transpose=24
Cold Morning Piano|synth|p|osc3Volume=0.14
# Concert Hall Grand: a big bright concert grand: more hammer (FM), open filter, a long ring
Concert Hall Grand|synth|p|osc3Volume=0.12
Underground Piano|synth|p|osc3Transpose=-12
Underground Piano|synth|p|osc3Volume=0.35
Underground Piano|synth|p|flt1Cutoff=520
Unwound Piano|synth|p|osc3Fine=45
Unwound Piano|synth|p|osc3Volume=0.28
Unwound Piano|synth|p|amp1Decay=4000
Unwound Piano|synth|p|osc1VibAmount=14
Lone Melody|synth|p|amp1Sustain=0.28
Lone Melody|synth|p|amp1Decay=2500
Saloon Piano|synth|p|amp1Decay=1800
Saloon Piano|synth|p|osc5Volume=1.0
Saloon Piano|synth|p|osc5Fine=22
Cocktail Piano|synth|p|osc1FmAmount=0.16
Cocktail Piano|synth|p|flt1Cutoff=3000
Cocktail Piano|synth|p|amp1Sustain=0.12
Intimate Felt|synth|p|amp1Attack=28
Intimate Felt|synth|p|flt1Cutoff=500
Contraband Piano|synth|p|flt1Cutoff=350
Contraband Piano|synth|p|amp1Release=150
Soft Felt Piano|synth|p|osc4Volume=0.06
Echo Piano|synth|p|osc3Transpose=19
Echo Piano|synth|p|osc3Volume=0.22
Rationed Piano|synth|p|osc2Transpose=7
Rationed Piano|synth|p|osc1FmAmount=0.09
Contraband Piano|synth|p|amp1Decay=1500
Heartbreak Piano|synth|p|amp1Decay=5000
Heartbreak Piano|synth|p|amp1Sustain=0.18
Heartbreak Piano|synth|p|osc1FmAmount=0.17
Heartbreak Piano|synth|p|osc3Volume=0.0
Concert Hall Grand|synth|p|osc1FmAmount=0.24
Concert Hall Grand|synth|p|flt1Cutoff=6000
Concert Hall Grand|synth|p|amp1Decay=12000
# --- The sine leads: one recipe (a sine, a quiet octave, vibrato) in eight banks
# Saucer Song: its upper partial a twelfth up, a wider vibrato
Saucer Song|synth|p|osc2Transpose=19
Saucer Song|synth|p|osc2Volume=0.14
# Mad Theremin: a wide unstable warble, a sour fifth above
Mad Theremin|synth|p|osc1VibAmount=60
Mad Theremin|synth|p|osc1VibRate=7.5
Mad Theremin|synth|p|osc2VibAmount=60
Mad Theremin|synth|p|osc2VibRate=7.5
Mad Theremin|synth|p|osc2Fine=25
# Stairwell Whistle: breathy and pure (no octave)
Stairwell Whistle|synth|p|osc3Volume=0.12
Stairwell Whistle|synth|p|osc2Volume=0
Stairwell Whistle|synth|p|flt1Cutoff=5000
# Lonesome Whistle: pure, a slow swell, a deeper vibrato
Lonesome Whistle|synth|p|osc2Volume=0
Lonesome Whistle|synth|p|osc1VibRate=5.8
Lonesome Whistle|synth|p|osc1VibAmount=22
Lonesome Whistle|synth|p|amp1Attack=90
# Host Signal: machine-made: more FM, no vibrato
Host Signal|synth|p|osc1FmAmount=0.12
Host Signal|synth|p|osc1VibAmount=0
Host Signal|synth|p|osc3VibAmount=0
# Lonely Flute: more breath, darker
Lonely Flute|synth|p|amp1Attack=60
# Signal Tone: a body an octave below, a slow drift
Signal Tone|synth|p|osc2Transpose=-12
Signal Tone|synth|p|osc2Volume=0.25
Signal Tone|synth|p|osc1VibAmount=6
Saucer Song|synth|p|osc1VibAmount=40
Saucer Song|synth|p|osc2VibAmount=40
Mad Theremin|synth|p|osc2Transpose=7
Mad Theremin|synth|p|osc2Volume=0.2
Mad Theremin|synth|p|amp1Attack=20
Lonely Flute|synth|p|osc3Volume=0.18
Lonely Flute|synth|p|osc2Volume=0.3
Lonely Flute|synth|p|flt1Cutoff=1800
)PASS"
R"PASS(# --- Organs, basses, keys and voices that measured within 3 of another bank's
Airlock Seal|synth|p|osc3Volume=0.08
Airlock Seal|synth|p|osc2Volume=0.5
Airlock Seal|synth|p|flt1Cutoff=320
Airlock Seal|synth|p|flt1Res=0.45
Down Deep|synth|p|osc2Volume=0.7
Down Deep|synth|p|amp1Decay=2500
Wind-Up Box|synth|p|osc3Transpose=31
Wind-Up Box|synth|p|osc2Fine=-35
Wind-Up Box|synth|p|amp1Decay=2500
Soprano Aria|synth|p|osc1Width=0.15
Soprano Aria|synth|p|osc1VibRate=6.2
Soprano Aria|synth|p|osc1VibAmount=30
Soprano Aria|synth|p|osc2VibRate=6.2
Soprano Aria|synth|p|osc2VibAmount=30
Soprano Aria|synth|p|amp1Attack=200
Harpsichord Duel|synth|p|osc2Transpose=24
Harpsichord Duel|synth|p|osc2Volume=0.25
Harpsichord Duel|synth|p|flt1Cutoff=5000
Harpsichord Duel|synth|p|amp1Decay=900
Cold Pulse Bass|synth|p|osc1Width=0.2
Cold Pulse Bass|synth|p|flt1Res=0.45
Cold Pulse Bass|synth|p|flt1Cutoff=450
Cold Pulse Bass|synth|p|amp1Decay=400
Cold Pulse Bass|synth|p|amp1Sustain=0.5
Glass Lab Keys|synth|p|osc2Transpose=24
Glass Lab Keys|synth|p|osc2Volume=0.25
Glass Lab Keys|synth|p|osc1FmAmount=0.08
Glass Lab Keys|synth|p|amp1Decay=700
Awakening Cello|synth|p|osc2Fine=14
Awakening Cello|synth|p|flt1Cutoff=2200
Awakening Cello|synth|p|osc1VibRate=5.5
Awakening Cello|synth|p|osc1VibAmount=25
Awakening Cello|synth|p|osc2VibRate=5.5
Awakening Cello|synth|p|osc2VibAmount=25
Awakening Cello|synth|p|amp1Attack=200
Sanctuary Organ|synth|p|osc1Volume=0.05
Sanctuary Organ|synth|p|osc3Volume=0.45
Sanctuary Organ|synth|p|osc4Volume=0.25
Sanctuary Organ|synth|p|flt1Cutoff=1600
Founder's Organ|synth|p|osc2Wave=1
Founder's Organ|synth|p|osc2Volume=0.3
Founder's Organ|synth|p|osc2VibAmount=8
Founder's Organ|synth|p|osc2VibRate=6
Pipe Organ Mass|synth|p|osc5Volume=0.25
Pipe Organ Mass|synth|p|osc4Volume=0.25
Pipe Organ Mass|synth|p|osc1Transpose=-24
Pipe Organ Mass|synth|p|osc1Volume=0.3
Earth Drum Bass|synth|p|osc2Volume=0.45
Taiko Bass|synth|p|pitchEnvOn=1
Taiko Bass|synth|p|pitchEnvRange=12
Taiko Bass|synth|p|osc1PitchEnv=1
Taiko Bass|synth|p|osc2PitchEnv=1
Down Deep|synth|p|osc1Wave=0
Down Deep|synth|p|flt1Res=0.5
Down Deep|synth|p|flt1Cutoff=220
Earth Drum Bass|synth|p|osc3Volume=0.3
Earth Drum Bass|synth|p|osc2Wave=0
Earth Drum Bass|synth|p|amp1Decay=180
# --- Library versions on the same recording with nearly the same settings
Down Deep|lib|s|0=Programs/02_arco_3vel.sfz
Down Deep|lib|p|flt1Cutoff=900
Down Deep|lib|p|amp1Attack=60
Awakening Cello|lib|s|0=Sonatina Symphonic Orchestra/Strings - Notation/Celli Sustain.sfz
Masked Trumpet|lib|s|0=Sonatina Symphonic Orchestra/Brass - Notation/Trumpets Sustain (looped).sfz
Night Shred|lib|s|0=Programs/07-black_twang.sfz
Bell Tower|lib|s|0=Idiophones/Struck Idiophones/Tubular Bells 2.sfz
Gospel Choir|lib|s|0={sfz:epic-choir}/T_ah.sfz
Gospel Choir|lib|s|1={sfz:epic-choir}/S_ah.sfz
Gospel Choir|lib|p|osc2On=1
Gospel Choir|lib|p|osc2Wave=6
Gospel Choir|lib|p|osc2Volume=1
Gospel Choir|lib|p|osc2Filter=0
Gospel Choir|lib|p|osc1SfzHold=1
Gospel Choir|lib|p|osc2SfzHold=1
Choir of Thousands|lib|s|0={sfz:epic-choir}/T_oh.sfz
Choir of Thousands|lib|s|1={sfz:epic-choir}/A_oh.sfz
Choir of Thousands|lib|p|osc2On=1
Choir of Thousands|lib|p|osc2Wave=6
Choir of Thousands|lib|p|osc2Volume=1
Choir of Thousands|lib|p|osc2Filter=1
Choir of Thousands|lib|p|osc1SfzHold=1
Choir of Thousands|lib|p|osc2SfzHold=1
Dusty Upright|lib|p|flt1Env=0.1
Saloon Upright|lib|p|flt1Cutoff=2500
Saloon Upright|lib|p|amp1Sustain=0.3
Saloon Upright|lib|p|amp1Decay=500
# Player Piano: bright and mechanical: hard hammers, a short ring
Player Piano|lib|p|osc5Fine=6
Player Piano|lib|p|flt1Cutoff=8000
Player Piano|lib|p|amp1Release=200
Ruin Harp|lib|p|flt1Res=0.5
Raven Pluck|lib|p|flt1Cutoff=900
Raven Pluck|lib|p|flt1Res=0.4
Harpsichord Mass|lib|p|flt1Cutoff=1800
Harpsichord Mass|lib|p|flt1Env=0.2
Harpsichord Duel|lib|s|1=Sonatina Symphonic Orchestra/Harpsichord/Harpsichord Full.sfz
Harpsichord Duel|lib|p|osc2On=1
Harpsichord Duel|lib|p|osc2Wave=6
Harpsichord Duel|lib|p|osc2Transpose=12
Harpsichord Duel|lib|p|osc2Filter=0
# Pawn Shop Piano: tinny: bright, a loud octave string, a short ring
Pawn Shop Piano|lib|p|osc5Fine=30
Walking Gospel|lib|p|osc1Filter=1
Dusty Upright|lib|p|osc1Filter=1
Ruin Harp|lib|p|osc1Filter=1
Harpsichord Mass|lib|p|osc1Filter=1
Corrupted Choir|lib|p|lofiAmount=8
Corrupted Choir|lib|p|lofiMix=0.9
# Lone Melody: held a little (sustain 0.28), brighter, an octave shimmer
Lone Melody|lib|p|flt1Cutoff=7000
Lone Melody|lib|p|amp1Release=2500
Pawn Shop Piano|lib|p|flt1Cutoff=1600
Harpsichord Duel|lib|p|osc2Volume=0.9
Saloon Upright|lib|p|osc1Filter=1
Saloon Upright|lib|p|flt1Res=0.35
Dusty Upright|lib|p|flt1Cutoff=280
Ruin Harp|lib|p|flt1Cutoff=850
Walking Gospel|lib|p|osc5On=1
Walking Gospel|lib|p|osc5Wave=3
Walking Gospel|lib|p|osc5Filter=0
Walking Gospel|lib|p|osc5PhaseRand=0
Walking Gospel|lib|p|osc5Volume=0.08
# --- LEVEL: as loud as before
# Abandoned Upright: a short, dull ring, its octave string flat
Abandoned Upright|synth|p|patchLevel=-0.6
Airlock Seal|synth|p|patchLevel=-9.2
# Campfire Piano: an outdoor upright: short, a second string 8 cents off
Campfire Piano|synth|p|patchLevel=-6.7
# Cold Morning Piano: glassy: pure, a faint string two octaves up, a long ring
Cold Morning Piano|synth|p|patchLevel=-9.9
Cold Pulse Bass|synth|p|patchLevel=-0.5
# Contraband Piano: muffled and short, played in hiding
Contraband Piano|synth|p|patchLevel=-2.7
Down Deep|synth|p|patchLevel=-6.2
Earth Drum Bass|synth|p|patchLevel=5.4
# Echo Piano: a slow bloom, dark, a shimmer a twelfth up
Echo Piano|synth|p|patchLevel=-1.6
# Elevator Muzak: an electric piano's bell (FM at 2:1)
Elevator Muzak|synth|p|patchLevel=-7.9
# Falling Ash Piano: high and fragile: bright, an octave shimmer, a short ring
Falling Ash Piano|synth|p|patchLevel=-0.6
Harpsichord Duel|synth|p|patchLevel=-0.9
# Heartbreak Piano: warm and held: more hammer, a held sustain
Heartbreak Piano|synth|p|patchLevel=-6.0
# Intimate Felt: the softest felt: dark, a soft attack, no octave or thump
Intimate Felt|synth|p|patchLevel=-9.5
# Lament Piano: long, dark and bare (no 2nd harmonic)
Lament Piano|synth|p|patchLevel=-5.9
# Pawn Shop Piano: tinny: bright, a loud octave string, a short ring
Pawn Shop Piano|synth|p|patchLevel=-3.2
Pipe Organ Mass|synth|p|patchLevel=-12.4
# Player Piano: bright and mechanical: hard hammers, a short ring
Player Piano|synth|p|patchLevel=-7.0
# Quiet Kitchen Piano: a domestic upright: shorter, its second string 5 cents off
Quiet Kitchen Piano|synth|p|patchLevel=-6.4
# Rationed Piano: worn metal: the hammer tone at a fifth (inharmonic), short
Rationed Piano|synth|p|patchLevel=-8.0
Sanctuary Organ|synth|p|patchLevel=-14.1
# Soft Felt Piano: brighter, shorter, more of the 3rd harmonic
Soft Felt Piano|synth|p|patchLevel=-3.3
Awakening Cello|lib|p|patchLevel=1.3
Bell Tower|lib|p|patchLevel=-1.3
Choir of Thousands|lib|p|patchLevel=-0.9
Down Deep|lib|p|patchLevel=-1.5
Dusty Upright|lib|p|patchLevel=4.7
Saloon Upright|lib|p|patchLevel=7.0
Walking Gospel|lib|p|patchLevel=0.6

)PASS";
    return table;
}
