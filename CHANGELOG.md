# Changelog

Each version's notes appear on its GitHub Release page.

## 0.74.0

**GRAND CINEMA, a cinematic piano expansion, built round BLOOM.**

- **GRAND CINEMA (GC01-GC18)**, the modern film and trailer piano in a balanced mix of intimate, vast, epic and dark: Lone Melody, Octave Theme and Halo Lead (leads); Piano Over Strings, Reverse Swell (the piano heard backwards, swelling up and cut off) and Cathedral Air (pads); Trailer Boom, Low Drone Piano and Muted Ostinato Bass (basses); Intimate Felt (the centrepiece: a thick felt, heard from inside the lid, hammers and pedal audible), Concert Hall Grand and Epic Grand (keys); Rising Ostinato, Tension Pulse and Cascading Light (arps); Prepared Grand, Muted Grand and Detuned Upright (plucks). Our own sounds and phrases.
- **Every preset plays the real piano** once the free Salamander Grand is downloaded (the same download as the bank A pianos), keeping its extra parts: octaves, a synth halo, strings, a drone, the boom, a detuned second string. Until then each plays PR7-K's own synthesized piano.
- **MACRO 3 is BLOOM on every GRAND CINEMA preset**: at the left the piano as built, turning it up brings in the strings ringing along and the hall opening out.
- **BLOOM, a new signature effect** (the SIGNATURE slot's seventeenth): a piano's undamped strings ringing in sympathy with what you play (tuned to the notes sounding, an octave below, an octave and an octave and a fifth above, ringing on after the keys let go for as long as RING says; a note a half step off sets nothing ringing), and a hall that opens out the longer you hold (SWELL). AMOUNT, STRINGS, RING, SIZE, SWELL, TONE, WIDTH; AMOUNT, STRINGS and SIZE can be moved by the mod matrix. Its page shows a piano's strings from above, the ringing ones shimmering, the hall glowing behind. At AMOUNT 0 it leaves the sound untouched. Our own design, about 1.3% of a CPU core.
- Levels set by measurement: about -17 loudness, full BLOOM adds about 2.5 dB; the percussive presets are set by their peaks. Reverse Swell on the real piano is the quietest (a recorded note fades as the backwards swell rises).

## 0.73.1

**You can now see the orchestra (or piano) downloading.**

- In 0.73.0, clicking DOWNLOAD on a HYPERDRIVE preset showed nothing: the download's progress was only on an oscillator's SFZ line, which the front panel hides and which those presets don't have until the orchestra is in. The download itself ran; you just couldn't tell.
- Now the front panel's screen shows DOWNLOADING THE ORCHESTRA with the percentage and a bar (then UNPACKING), and the header shows ORCHESTRA n% on every page.
- If a download fails, the screen says why, and PR7-K offers it once more with the reason.

## 0.73.0

**HYPERDRIVE, a new expansion with a real orchestra: GET THE ORCHESTRA, looping samples, and LIGHTSPEED.**

- **HYPERDRIVE (HD01-HD18)**, inspired by the late-'70s and early-'80s space-opera film scores: heroic brass fanfares (Hero's Fanfare), a lonely horn at sunset (Desert Horizon), a robot's chirping whistle (Cockpit Chirp), a men's choir (Starbound Choir), shimmering strings (Galaxy Strings), an old mystical hum (Elder Hum), a marching low-brass ostinato (Iron March), a starfighter's idling engines (Sublight Drone), a spaceport dance band's plucked bass, combo organ and riff in seven (Dive Bar Bass, Spaceport Organ, Seven Step Band), a twinkling celesta (Starfield Celesta), a dark low piano (Ominous Piano), racing strings (Racing Strings), a navigation computer (Navigation Beeps), and three plucks (Royal Harp, Battle Pizzicato, Moon Tines). Our own sounds and phrases; nothing from the films is named.
- **MACRO 3 is LIGHTSPEED on every HYPERDRIVE preset**: at the left the sound as built, turning it up makes the jump to lightspeed and back.
- **LIGHTSPEED, a new signature effect** (the SIGNATURE slot's sixteenth): JUMP sends streaks of the sound up in pitch (up to two octaves, spooling up like engines and falling back), out to the sides, with a whoosh sweeping up behind them that follows how loud you play, while the sound itself rushes past. RISE, STREAKS, WHOOSH, WIDTH and TONE shape it; JUMP, RISE and WHOOSH can be moved by the mod matrix. Its page draws a starfield stretching into streaks. At JUMP 0 it leaves the sound untouched.
- **GET THE ORCHESTRA.** Twelve HYPERDRIVE presets play real recordings once the free Sonatina Symphonic Orchestra (about 1.4 GB, by Mattias Westlund and Paul Eastman, CC Sampling Plus 1.0) is downloaded, the same way the piano presets use the Salamander: trumpets and horns, a solo horn, a large choir, the string section, trombones and tuba, a solo double bass, celesta, staccato strings, clarinet, harp and pizzicato (Ominous Piano uses the Salamander). Until then they play PR7-K's own synthesized orchestra. Loading one asks once whether to get it; once it's in, a preset you haven't edited switches over by itself. Galaxy Strings keeps its synth strings underneath, quieter: the library's section strings stop after about four seconds, so the synth layer carries long chords.
- **SFZ: looped samples and velocity crossfades.** Samples with loop points (in the SFZ file or the WAV itself) now hold for as long as you hold the key, and loop-sustain ones stop looping when you let go. Instruments that blend velocity layers into each other (xfin/xfout) blend smoothly. Still not played: the files' own filters and envelopes (soft notes are quieter, not darker) and mod-wheel crossfades.
- Every level was set by measurement (about -17 loudness, peaks under -3 dBFS at rest and under -2 at full LIGHTSPEED).

## 0.72.0

**The real piano in the expansions: seven expansion piano presets play the Salamander Grand once it's downloaded.**

- **Seven expansion presets now have two versions.** Felt Memory and Tesseract (LAST LIGHT), Echo Piano (DREAMSTATE), Memory Loop (UNSPOKEN), Lament Piano (ASCENSION), Distant Piano (DUSTFALL) and Home Piano (SUPERPOSITION) play the sampled Salamander Grand when it's on your computer, with everything else about the preset kept: its echoes, reverbs and other effects, its macros, and its MACRO 3 centrepiece (GRAVITY, DREAM, LEVIATHAN and so on). Each is a little darker or brighter to suit its mood (the FILTER follows the keys, so BRIGHT and the other filter macros still work).
- **Without the piano, nothing changes.** Until the Salamander is downloaded, these seven play PR7-K's own synthesized piano, exactly as before (checked: every one renders byte for byte as in 0.71.0). Loading one asks once whether to get the piano; once the download finishes, a preset you haven't edited switches over by itself.
- **Levels.** The sampled versions were set by measurement to sit where the synthesized ones do (about -17 to -18.5 loudness). Where a real hammer strike would have peaked over -2 dBFS, they sit up to 1.5 dB quieter instead. The LIMITER holds a -1 dBFS ceiling.
- A project or a saved preset keeps whichever version was playing when it was saved.

## 0.71.0

**A sampled grand piano: the new SFZ oscillator, seven piano presets, and GET THE PIANO.**

Built for Windows and Linux; there's no macOS build from this version on.

- **SFZ, a new oscillator WAVE.** Any oscillator can now play a multi-sampled instrument from an SFZ file (the open format many free and paid sample libraries come in). Each key plays its own recording, and it goes through the FILTER, VOICE, SHAPERs, sends, insert buses and every effect like any other wave; TRANSPOSE, FINE, the pitch wheel, glide, vibrato and the PITCH envelope move it too. Load one from the oscillator's SFZ line (or drop an .sfz file on the oscillator); the picture shows its key map. What it reads:
  - each sample's keys and the key it was recorded at, its tuning, volume and pan;
  - velocity layers, and how much softer notes get inside a layer;
  - round-robins and random alternates;
  - **release samples**, played when a key lets go (after the pedal), quieter the longer the note was held;
  - samples that ring on after the key (a piano's undamped top strings), and the sustain pedal's own sounds;
  - regions that play only with the pedal down or up.
  What it leaves out (loops, the file's own filters and envelopes, key switches) is listed in the SFZ line's menu; the AMP shapes the notes instead.
- **Streamed from disk.** Only the first quarter second of each sample is kept in memory; the rest is read from disk as it plays, so a big library loads in seconds and fits easily. Live, PR7-K never waits for the disk; a bounce always does, so it comes out whole.
- **Presets and projects remember where the file is**, never the samples. If you move a library to another drive, load any SFZ from its new folder once and your presets find it again. One that can't be found says NOT LOADED and plays nothing.
- **Seven piano presets, A41-A47**, on the Salamander Grand Piano (a Yamaha C5 recorded at 16 strengths, by Alexander Holm, free under CC-BY): Salamander Grand, Felt Grand, Pop Grand, Tape Piano, Ambient Piano, Piano And Pad and Starlight Piano. Each has MACRO 1 (ROOM, FELT, GRIT, WEAR, HAZE, PAD, TRAIL) and MACRO 2 SPACE. Their levels keep the piano's real dynamics: a medium touch sits a little under the synth presets, and the LIMITER stops the hardest chords at -1 dBFS.
- **GET THE PIANO.** The piano isn't inside the plugin (it would make every download about 750 MB). The first time you load a piano preset, PR7-K offers to download it from freepats.zenvoid.org: about 742 MB, unpacked to about 714 MB in PR7-K's presets folder, once per computer. The oscillator's SFZ line shows the progress and can cancel it. ABOUT credits the library.
- Linux builds now use libcurl for the download (Windows and macOS use their own).

## 0.70.0

**Plucks: three new plucks in every expansion, 75 in all.**

Every expansion now has a sixth kind of preset, PLUCK, in slots 16-18 (NR16-NR18, HF16-HF18 and so on). Your existing presets keep their slot numbers.

- **Built round each bank's centrepiece.** A pluck's short notes leave gaps, and MACRO 3's effect lives in them: the rain's drops land on NEON RAIN's plucks, SIROCCO answers SANDSEA's, LIGHTTRAIL's trails climb behind DATASTREAM's, ECHOES plays PARADOX's back in reverse, as tape or as a loop, and the spring tank clangs under ATOMIC AGE's.
- **Built the way real plucked things sound.** Each one has its own attack (a pick, a finger, a hammer, a mallet or a breath), its brightness fades faster than its loudness as a real string's does, and harder playing is brighter.
- **Built differently from each other.** No bank's three use the same method, and the methods are spread across all 25 banks: filter snaps, FM strikes (kalimbas, mallets, bells, a toy piano), harps made of separate overtones, plucked strings struck from noise, sync and wavetable sweeps, pitch chirps, sung syllables through the VOICE filter, one-key chords, pizzicato sections, palm mutes, ring-modulated metal, an FFT brick-wall sweep, formant and crush sweeps, a wah and drives.
- **The 75:**
  - **NEON RAIN:** Wet Neon Pluck, Street Zither, Glass Rain Taps.
  - **HELLFORGE:** Rivet Gun, Hellwire Pick, Ember Pluck.
  - **STARCOURT:** Bike Spokes, Cassette Harp, Funhouse Mallet.
  - **SANDSEA:** Desert Oud, Sandglass Kanun, Heat Shimmer Pluck.
  - **DATASTREAM:** Gridline Pluck, Pixel Ping, Laser Lattice.
  - **LAST LIGHT:** Starlight Harp, Ticking Pizzicato, Orbit Kalimba.
  - **SIMULACRUM:** Code Drip, Rooftop Pick, Glyph Ring.
  - **DREAMSTATE:** Sinking Harp, Staccato Horns, Folding Guitar.
  - **NEGATIVE SPACE:** Muted Felt, Server Clicks, Prepared Strings.
  - **NIGHTDRIVE:** Chrome Pluck, Dashboard Pulse, Tail Light Bell.
  - **UNSPOKEN:** Hummed Pluck, Shell Harp, Deep Pizzicato.
  - **CYBERSHRINE:** Wire Shamisen, Lantern Pluck, Glass Ghost.
  - **STOWAWAY:** Sonar Ping, Hull Pizzicato, Condensation Drip.
  - **CHROMEFURY:** Spark Plug, Chain Snap, Exhaust Stab.
  - **ASCENSION:** Choir Pluck, Ancient Harp, Monastery Mallet.
  - **REFRACTION:** Prism Pluck, Fungal Kalimba, Bloom Formant.
  - **PSI STORM:** Chant Pluck, Neon Spoke, Psychic Ping.
  - **BLOODWAVE:** VHS Pluck, Rage Stab, Neon Ping Pong.
  - **DUSTFALL:** Snowflake Pluck, Wasteland Pick, Hologram Pluck.
  - **SUPERPOSITION:** Either Way, Probability Pluck, Observer Mallet.
  - **SEVERED:** Desk Toy Piano, Corridor Pick, Cubicle Pizzicato.
  - **SLINGBLADE:** Gilded Harp, Slingshot Wah, Iron Pick.
  - **PARADOX:** Cave Drip, Pendulum Clock, Tape Loop Pluck.
  - **MELTDOWN:** Dosimeter Ping, Rusted Wire, Control Room Mallet.
  - **ATOMIC AGE:** Tank Twang, Luau Ukulele, Rocket Ping.
- **Levels:** balanced by measurement against the banks' other presets. The most percussive (pizzicato, clicks, chord stabs) sit a few dB lower so their peaks stay safe.

## 0.69.0

**NUCLEARSPRING, and a new expansion: ATOMIC AGE.**

**NUCLEARSPRING** (a new signature effect, our own design, in the SIGNATURE slot): a vintage spring reverb tank, the drippy, twangy reverb of 1950s and 60s guitar amps and home organs.
- **DRIP:** how wet and chirpy the springs sound: from a plain echo to the "boing-drip" of surf guitar. A real spring sends the high notes through faster than the low ones; this copies that.
- **BOING:** how hard a sudden hit kicks the tank, like knocking an amp with a spring reverb in it.
- **DECAY** (0.8 to 5 seconds), **TONE** (springs are dark), **SIZE** (a small amp's short tank to a long organ tank), **WIDTH**.
- **MIX:** how much of the tank under your sound. At 0 your sound passes untouched.
- Springs don't like bass, so the lows are cut going in, as on the real thing: on a bass sound you mostly hear the tank's upper twang.
- AMP, ORGAN and SURF on its unit set the tank's size, decay and drip together. Its page shows the tank opened up, the six springs shaking as your sound runs through them, the frame flashing on a hard hit.
- MIX, DRIP and BOING can be moved by the mod matrix (FX Nuclearspring). About 3% of a core.

**ATOMIC AGE** (AA01-AA15, a new expansion): bright, cheerful 1950s atomic-age optimism, the retro-futurist, mid-century-modern side of a post-nuclear world (our own sounds; nothing from any show or game is named).
- **Leads:** Saucer Song (a B-movie sci-fi singing sine, wide and wobbly), Surf Baritone (a twangy guitar: a pick click, each note starting a hair sharp like a struck string, a little tube amp; the mod wheel is a whammy bar), Harmon Horn (a muted jazz trumpet; the mod wheel is the hand over the mute, for the wah-wah).
- **Pads:** Doo-Wop Oohs (a vocal group on "ooh", each singer with their own vibrato, sliding up into each chord), Sunday Strings (violins and cellos, each player with their own vibrato, the bow biting in), Atomic Dawn (a gleaming sunrise shimmer).
- **Basses:** Walking Upright, Pedal Organ, Tube Swing.
- **Keys:** Spinning Vibes (a vibraphone with its motor on), Cocktail Organ, Jukebox Celesta.
- **Arps:** Jingle Sparkle (an advertising-jingle glockenspiel), Punch Card Brain (a retro computer thinking out loud, a new pattern every pass), Tiki Marimba.
- **MACRO 3 is SPRING on every preset:** NUCLEARSPRING brought in, each preset with its own tank. At 0 there's no tank; at full it sits under the sound like a real amp's reverb, adding less than 1 dB.

**Also:** long effect names on the rack units (NUCLEARSPRING) now fit their title and the small screen under EDIT, instead of running into the lights or being cut off.

## 0.68.0

**LEVIATHAN, UNSPOKEN gets its centrepiece, and PR7-K runs lighter with the sound unchanged.**

**LEVIATHAN** (a new signature effect, our own design, in the SIGNATURE slot): a vast, slow, wordless voice singing far beneath what you play, like something answering from the fog. It's a new voice, not a copy of your sound.
- **Its note:** the lowest note the synth is playing, one or two octaves down (DEPTH), sliding slowly to each new note. It takes the note from the synth itself, so a chord gets one deep voice under its lowest note and nothing can fool it.
- **Its throat:** a soft buzz shaped into vowels that wander on their own between OO and EE (DRIFT). SIZE makes the throat bigger.
- **GROWL:** rougher, creaking, with clicks in its throat. **DELAY:** it answers up to 1.5 s behind you.
- **MIX:** how loud it is. Its level follows yours, so it breathes with your playing, and silence stays silent. At 0 your sound passes untouched.
- DISTANT, CALLING and RISING on its unit set how near it comes. Its page shows the voice as a vast shape in the dark, the note it's singing and its vowel.
- MIX, GROWL and DRIFT can be moved by the mod matrix (FX Leviathan). About 0.8% of a core.

**UNSPOKEN: MACRO 3 is now LEVIATHAN on all 15 presets.**
- At 0 every preset sounds exactly as before.
- Turning it up brings in each preset's voice beneath it, some one octave down, some two, each with its own vowel, roughness and delay.
- At full it adds about 0.5 to 1.5 dB. Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

**Lighter on your computer, the sound unchanged.** Five parts now do the same work with less waiting. Recordings of each, made before and after, are identical sample for sample. Measured on a held chord (share of one processor core, including the synth):
- **DOMINION:** 12.2% down to 6.5% (in Hard mode with Mid/Side, 15.7% to 11.1%).
- **LIMITER with true peak:** 7.2% to 5.6%.
- **Everything, always:** settings are now found through a quick index instead of a search, 1.28% to 0.96% for a plain chord. This also removes a rare cause of glitches when the mod matrix moved an effect knob.
- **INFERNAL:** about 15-20% lighter. **SPECTRE** at DRIFT 0 a little lighter.
- Your own computer's numbers will differ, but the proportions should hold.

## 0.67.0

**SPECTRE, and CYBERSHRINE gets its centrepiece.**

**SPECTRE** (a new signature effect, our own design, in the SIGNATURE slot): a whispered ghost following your sound. Each note leaves a shadow of itself made of breath: the shape of its tone (its vowel, its brightness) without its pitch.
- **MIX:** how loud the ghost is, under your sound (which passes unchanged).
- **TRAIL:** how far behind it follows, up to 800 ms. **LINGER:** how long it stays after the sound has gone, 0.1 to 4 seconds.
- **AIR:** hollow and whistling, or wide and breathy. **DRIFT:** how far the ghost wanders between the sides.
- **SHIFT:** the ghost's outline moved up or down to an octave: a smaller ghost or a larger one.
- At 0 your sound passes untouched, and silence stays silent. WHISPER, SHADOW and WRAITH on its unit set how strongly it haunts. Its page shows your sound's outline with its ghost as a pale veil around it.
- MIX, LINGER and SHIFT can be moved by the mod matrix (FX Spectre).
- About 3% of a core: more than most signature effects (it reads and rebuilds the sound in 24 bands on each side).

**CYBERSHRINE: MACRO 3 is now SPECTRE on all 15 presets.**
- At 0 every preset sounds exactly as before.
- Turning it up brings in each preset's ghost: the choirs a larger, lower shadow choir, the pads long drifting ghosts, the basses' ghosts lifted above them so the whisper can be heard.
- At full it adds about 0.5 to 1.5 dB: a whisper under the sound, not a second sound.
- Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

## 0.66.0

**HAZE, and NIGHTDRIVE gets its centrepiece.**

**HAZE** (a new signature effect, our own design, in the SIGNATURE slot): streetlights passing a car at night. In time with the song, your sound sweeps past you, left to right, then back right to left on the next pass.
- **DEPTH:** how much it moves. As it passes it's nearer, brighter and louder; out at the edges it's further, darker and quieter. It slows as it turns at each edge, so nothing jumps. A whole pass stays as loud as your sound.
- **PASS:** half a bar, 1 bar, 2 bars or 4 bars a pass, following the song (its own count while stopped).
- **DOPPLER:** the pitch bends a little up as it comes and down as it goes (up to 40 cents), from its distance changing, as in the air.
- **WIDTH:** how far it leans to each side. **TONE:** how dark it gets far off.
- **WHOOSH:** a breath of wind as it goes by, only while you play, so silence stays silent.
- At 0 your sound passes untouched. CRUISE, PASSING and RUSH on its unit set how much. Its page shows a night road with streetlights and the headlights where your sound is.
- DEPTH, DOPPLER and WHOOSH can be moved by the mod matrix (FX Haze).

**NIGHTDRIVE: MACRO 3 is now HAZE on all 15 presets.**
- At 0 every preset sounds exactly as before.
- Turning it up sends the sound past you: leads and keys a pass a bar, pads every two bars, arps and Retro Stab every half bar, the basses moving less so the low end stays centred.
- Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

## 0.65.0

**PROXIMITY, and NEGATIVE SPACE gets its centrepiece.**

**PROXIMITY** (a new signature effect, our own design, in the SIGNATURE slot): the sound of playing, heard up close. Every noise is made by the synth itself (no recordings), on the exact moment of each note, the arpeggiator's included.
- **NOISE:** the touches of playing. A felt hammer's thump as each note starts (lower and longer on low notes, louder the harder you play), a key's click and the damper settling as you let go, the sustain pedal's thud going down and the dampers' hush coming back up.
- **TONE:** the felt and clicks soft or crisp. **WIDTH:** each note's noises placed where it is on the keyboard, low notes left, high notes right.
- **ROOM:** the room's air and a mains **HUM** (off, 50 or 60 Hz), rising with the music and stopping a moment after it, so silence stays silent.
- **CLOSE:** the extra bass a microphone picks up close to an instrument (up to +6 dB).
- At 0 your sound passes untouched. DISTANT, NEAR and INSIDE on its unit set how close. Its page shows a keyboard from above, each touch lighting its key, with a microphone that lowers as CLOSE rises.
- NOISE, ROOM and CLOSE can be moved by the mod matrix (FX Proximity).

**NEGATIVE SPACE: MACRO 3 is now CLOSER on all 15 presets.**
- At 0 every preset sounds exactly as before.
- Turning it up brings you up close: the pianos' soft felt and room, the electronics' clicks and hum, a little of the close-up low end on the basses.
- Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

## 0.64.0

**REMNANT, and DREAMSTATE gets its centrepiece.**

**REMNANT** (a new signature effect, our own design, in the SIGNATURE slot): dreams within dreams. Your sound plays exactly as you play it; beneath it, up to three layers of what you just played, each a level deeper: slower and lower than the one above, all running at once.
- Each layer is a dream of the one above it: the second hears the first, not you, so each deeper one is slower and more smeared again.
- **LAYERS:** one to three. **STEP:** how much slower and lower each layer is than the one above: an octave (half speed), a fifth or a fourth.
- **LENGTH:** how long a stretch of the past each layer replays (0.4 to 4 seconds) before renewing itself, smoothly, from the present. Nothing slides or jumps: unlike DILATION, your sound is never slowed or held back.
- **TAPER:** each deeper layer quieter. **TONE:** each deeper layer darker. **WIDTH:** the layers leaning to alternate sides. **MIX:** how loud the layers are; at 0 nothing at all is added.
- SHALLOW, DEEPER and DEEPEST on its unit set how many layers. Its page shows nested rings: your sound outside, each layer further in, turning slower.
- MIX, TONE and TAPER can be moved by the mod matrix (FX Remnant).

**DREAMSTATE: MACRO 3 is now DREAM on all 15 presets.**
- At 0 every preset sounds exactly as before.
- Turning it up brings the layers in beneath: deep stacks on the pads, keys and arps, a single layer a fourth or fifth down on the basses so the low end stays clean.
- Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

**Also:** a test that throws random settings at the synth now allows extreme but stable loudness (it failed 0.63.0's first build without anything being wrong); no sound changed.

## 0.63.0

**DILATION, and SIMULACRUM gets its centrepiece.**

**DILATION** (a new signature effect, our own design, in the SIGNATURE slot): time slowing down. Not a haze or an echo added to your sound: the sound itself is played back slower, pitch and speed dropping together like a tape winding down, so what you play comes out stretched, lower and dragging behind real time.
- **SLOW:** how slow, down to a quarter speed (two octaves down).
- **RAMP:** how long the slide into slow motion (and back) takes, from a sudden lurch to a four-second wind-down.
- **RETURN:** how time catches up when SLOW comes back. GLIDE speeds up past real time (the pitch rises a little) until it's caught up; SNAP jumps straight back to the present.
- **REACH:** how far behind real time it can fall (1 to 8 seconds). There it fades back to the present and starts slowing again: it can't stay behind for ever.
- **TONE:** slowed tape loses its top; low, the slower it goes the darker it gets. **MIX:** the slowed sound against the sound as played.
- Your reverb and echoes slow down too. At SLOW 0 your sound passes untouched.
- DRIFT, SLOWMO and CRAWL on its unit set how slow. Its page is a clock: the dim hand keeps real time, the bright hand is what you hear, trailing behind with ghost hands as it slows.
- SLOW, TONE and MIX can be moved by the mod matrix (FX Dilation).

**SIMULACRUM: MACRO 3 is now DILATE on all 15 presets.**
- At 0 every preset sounds exactly as before.
- Turning it up slows time: the leads and pads an octave or more with slow slides, the basses only about half an octave so the low end stays usable, the arps snapping back in time.
- Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

## 0.62.0

**BLACKHOLE, and LAST LIGHT gets its centrepiece.**

**BLACKHOLE** (a new signature effect, our own design, in the SIGNATURE slot): the last moments of your sound caught and stretched into a haze that hangs under the next notes, as if time were slowing down.
- Short slices of what you've just played are lifted out, overlapped and blurred into a steady haze, which goes on after you let go.
- **HOLD:** how long the haze hangs, from half a second to twenty. It always dies away in the end.
- **PULL:** the haze sinks in pitch a little more each time round, falling in.
- **SIZE:** how far back it reaches. **TONE:** darker or brighter. **WIDTH:** each side its own haze, or both alike. **MIX:** how much haze.
- The sound itself passes unchanged; at MIX 0 nothing at all is added.
- FAINT, DEEP and ENDLESS on its unit set how long it hangs. Its page shows the slices circling a dark disc, drawn inward as they play.
- HOLD, PULL, TONE and MIX can be moved by the mod matrix (FX Blackhole).

**LAST LIGHT: MACRO 3 is now GRAVITY on all 15 presets.**
- At 0 every preset sounds exactly as before: no haze at all.
- Turning it up pulls each preset into its own haze: long and barely sinking behind the pianos, deep wide clouds that sink further on the pads, a light dark haze on the basses so the low end stays clear.
- Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

## 0.61.0

**LIGHTTRAIL, and DATASTREAM gets its centrepiece.**

**LIGHTTRAIL** (a new signature effect, our own design, in the SIGNATURE slot): every note leaves a glowing trail behind it, in time with your song.
- **DIVISION:** how far apart the repeats come (1/32 to a quarter note, triplets too).
- **RISE:** each repeat steps up or down in pitch: up, the trail climbs; down, it falls away; at 0 it stays on the note.
- **SPREAD:** each repeat swings to the other side, so the trail crosses the stereo field.
- **LENGTH:** from a short flick to a long streak. **TONE:** each repeat darker, or keeping its shine. **MIX:** how loud the trail is under your sound.
- The sound itself passes unchanged; at MIX 0 nothing at all is added.
- FLAT, CLIMB (a fifth a repeat) and FALL (an octave down) on its unit set the direction. Its page draws the trail as it's set.
- LENGTH, RISE, TONE and MIX can be moved by the mod matrix (FX Lighttrail).
- One limit: when RISE is on, the pitch step makes each repeat land a few milliseconds behind the beat.

**DATASTREAM: MACRO 3 is now TRAIL on all 15 presets.**
- At 0 every preset sounds exactly as before: no trail at all.
- Turning it up brings each preset's own trail in and lengthens it: Light Trail climbs a fifth every sixteenth, Bitfall Bass falls an octave, Grid Horizon rises an octave every quarter note, Digital Sky swings side to side on the note.
- Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

## 0.60.0

**AFTERGLOW, and STARCOURT rebuilt to exaggerate the 1980s synths.**

**AFTERGLOW** (a new signature effect, our own design, in the SIGNATURE slot): the glow of an old 1980s polysynth, in the notes themselves and after them.
- **AGE:** every note gets its own slightly different tuning (up to 14 cents either way), filter brightness and envelope timing, the way an old polysynth's voices never quite matched. A chord breathes and shimmers on its own.
- **DRIFT:** each held note's tuning and brightness wander slowly, differently from every other note's, as old circuits did while they warmed up.
- **ENSEMBLE:** a lush three-voice chorus, as the era's string machines and polysynths had, spreading the sound wide.
- **TAPE:** as if the whole sound went down to a cassette: a slow pitch sway (about 10 cents), warm saturation and a softer top.
- It keeps the level: turning it up changes the character, not the volume. Off, or at 0, the sound is exactly as before.
- FRESH, WORN and RELIC on its unit set all four at once (RELIC is the exaggerated end). Its page shows eight voice cards set apart by AGE and DRIFT, the ensemble's voices circling, and the tape's sway.
- AGE, DRIFT, ENSEMBLE and TAPE can be moved by the mod matrix (FX Afterglow).

**STARCOURT, rebuilt round AFTERGLOW** (your ask: "exaggerate the 1980s glorious synths"). MACRO 3 is now GLOW on all 15: from tight and modern to a huge, glowing, wobbly old polysynth.
- **Kept:** Arcade Hero, Basement Glow, Night Drive Sub, Brass '84 and Dark Ostinato. With GLOW at 0 they sound exactly as before; turning it up brings AFTERGLOW in, along with each one's old MACRO 3 job (Dark Ostinato's extra resonance is a little gentler now, since GLOW adds level of its own).
- **New:**
  - Leads: **Mixtape Lead** (the big soaring score lead: two saws and a square, a lazy glide, vibrato blooming late), **Radio Ghost** (a thin, haunted triangle drifting out of tune).
  - Pads: **Dusk Strings** (a string machine, its pulses shimmering, through the ensemble), **Polysynth Dream** (the full-blown polysynth chord, swelling open, every voice its own).
  - Basses: **Eighth Note Pulse** (the driving eighth-note bass), **Saw Wall Bass** (a wide wall of saws holding the chords up).
  - Keys: **Lantern Keys** (a warm, glassy FM electric piano), **Neon Stabs** (bright polysynth stabs).
  - Arps: **Rising Arp** (a minor chord climbing over two octaves in sixteenths, the filter slowly opening), **Cassette Arp** (a bright arpeggio worn and wobbling on tape).
- Gone: Static Wail, Phantom Sync, Other Side, Cassette Choir, Pulse Runner, Gated Octave, VHS Piano, Lab Chimes, Gated Sequence and Midnight Run. A project that used one of them keeps its sound: the settings are saved in the project.

## 0.59.0

**INFERNAL, and HELLFORGE rebuilt as a premiere bank.**

**INFERNAL** (a new signature effect, our own design, in the SIGNATURE slot): parallel destruction, after the way the industrial-metal game scores you pointed to were described being made: plain synth sounds sent through several chains of pedals at once and mixed back together.
- **TIGHT** keeps everything under it clean (30 to 400 Hz), so the sub stays solid under the dirt.
- Everything above goes through four chains side by side, each with its own blend:
  - **FUZZ:** a hard, lopsided fuzz that buzzes rather than fizzes.
  - **CRUSH:** few bits and a lowered sample rate, a digital grind.
  - **AMP:** a guitar amp's two gain stages into a speaker cabinet (**CAB** sets dark to bright). This is what makes a synth lead sound played on a guitar. **FEEDBACK** makes held notes bloom an octave up, as a guitar feeds back.
  - **NOISE:** noise pulsed at the pitch of the notes you play, so it growls in tune and in time with the riff instead of hissing.
- **DRIVE** pushes every chain, a glue compressor holds them together, **MIX** blends the whole (at 0 the sound passes exactly as it was), **LEVEL** sets the destroyed part.
- Its page shows each chain's level, the feedback bloom and the glue compressor's work.
- DRIVE, MIX, NOISE and FEEDBACK can be moved by the mod matrix (FX Infernal).

**HELLFORGE, rebuilt round INFERNAL** (your ask: "a premiere rig for us"). MACRO 3 is DESTROY on all 15.
- **Kept:** Gut Chug, Tectonic Drop, Iron Riff and Gatling Pulse. With MACRO 3 at 0 they sound exactly as before; turning it up now also brings INFERNAL in.
- **New:**
  - Leads: **Hellfire Lead** (played like a guitar through the AMP and cabinet, feedback blooming on held notes, the pitch wheel diving an octave, the MOD WHEEL feeding back harder), **Whammy Wraith** (a squealing fuzz lead), **Plasma Scream** (a hard-synced scream through broken electronics).
  - Pads: **Screamer Choir** (voices scooping up into pitch, the vowel tearing open, rasping), **Hellfire Drone** (clean sines, noise growling in tune), **Riff Shadow** (a layer to sit under a down-tuned guitar).
  - Bass: **Sine Engine** (two pure sines, the sub clean, the upper one fuzzed and crushed, with noise growling on top).
  - Keys: **Profane Organ** (a clean cathedral organ that DESTROY drags through INFERNAL), **Anvil Stab** (hammer-hit chords).
  - Arps: **Chainsaw Pulse** (a sine kick on every beat, a buzzing saw idling between), **Berserk Run** (shredding runs over two octaves through the AMP).
- Gone: Molten Riff, Warning Siren, Signal Scream, Ash Choir, Furnace Drone, Corrupted Signal, Grinder Bass, Rust Organ, Ritual Bells, Shrapnel Grid and War Machine. A project that used one of them keeps its sound: the settings are saved in the project.

## 0.58.0

**SIROCCO, the SIGNATURE slot, and SANDSEA gets its centrepiece.**

**The SIGNATURE slot** (your design): the FX page's MOD / LO-FI tab is four units again, at full size. The fourth is the SIGNATURE slot: click the small screen under its EDIT to pick ECHOES, CRACKLE, DOWNPOUR or SIROCCO. Each keeps its own POWER, so presets that use two at once (MELTDOWN's Warning Tone and Distant Siren) are unchanged; the screen shows "+1", "+2" when others are also on. Loading a preset shows its own signature effect.

**SIROCCO** (a new signature effect, our own design): a harmony that answers every note you play, and the arps' notes, kept to a scale.
- **KEY** and **SCALE** (every scale the arp randomizer has: Phrygian dominant, double harmonic, pelog and the rest). A note outside the scale is answered in parallel.
- **INTERVAL:** a 3rd, 4th, 5th, 6th or octave, above or below, in steps of the scale (in D Phrygian dominant, D is answered by F#, E-flat by G).
- **VOICES:** up to three answers stacked in 3rds, a small choir. **LEVEL** brings them in one at a time.
- **DELAY:** the answer comes a moment after the note, like a reply. **DETUNE** and **SPREAD** make the answers wider and more human.
- The answers are played by the synth itself (the same oscillators, filter and envelopes as your note), so they sound as clean as the note. They glide with a mono lead and let go with their note. Turn LEVEL (or MACRO 3) up while holding a chord and the answers come in.
- Its page shows a keyboard: the scale marked, the notes you play in orange, the answers in blue.
- LEVEL, DETUNE and SPREAD can be moved by the mod matrix (FX Sirocco).

**SANDSEA: MACRO 3 is now SIROCCO on all 15 presets**, in D Phrygian dominant.
- At 0, every preset sounds exactly as before: no answers at all.
- Turning it up brings the answers in and spreads them wider, each preset its own way: horns joining below, a second flute a breath behind, a choir of three on Mirage, a second gong struck after the first, runs in 3rds on Sand Ripple.
- Each preset's old MACRO 3 job still comes with it, and the preset notes say so.

## 0.57.0

**DOWNPOUR, and NEON RAIN gets its centrepiece.** The first of the early expansion banks to get one sound idea running through every preset, as the newer banks have.

**DOWNPOUR** (a new effect, our own design: the sixth unit on the FX page's MOD / LO-FI tab): rain on the glass, added to your sound.
- **Patter:** a bed of tiny droplets, like rain on a window. RAIN sets how hard it falls, from nothing at all to a downpour.
- **Drops that play your notes:** now and then a big drop rings out on one of the notes you're holding (OCTAVE: one, two or three octaves up). Each starts a little flat and slides up onto its note, as a real water drop does. Hold a chord and the rain picks out its notes, never in rhythm.
- **DISTANCE** moves the rain from right on the glass (sharp, bright) to out in the street (dull, soft). **SPACE** sends some of it into the preset's own reverb. **WIDTH** spreads it, **LEVEL** sets how loud.
- **FOLLOW:** the rain falls only while you play, and stops about a second after you let go, so sounds still end in silence.
- Its page is a window: each drop a ripple where it lands, named by the note it rang on.
- RAIN, DROPS, DISTANCE and LEVEL can be moved by the mod matrix (FX Downpour).

**NEON RAIN: MACRO 3 is now STORM on all 15 presets.**
- At 0, every preset sounds exactly as before: no rain at all.
- Turning it up brings the rain, from a few drops to a downpour, coming closer as it builds, with its drops ringing on your notes.
- Each preset's old MACRO 3 job (GROWL, SCOOP, VOWEL, FAT and so on) still comes with it, and the preset notes say so.

## 0.56.0

**MELTDOWN**, a new expansion (MD01-MD15), and **CRACKLE**, the effect it's built round.

### CRACKLE (new effect)
The crackle of a radiation counter, added on top of your sound (which passes through unchanged). Like a real counter, the clicks come at random moments and never fall into a rhythm.
- **RATE:** how many clicks a second, from one every two seconds to a 2,000-a-second roar.
- **CLUSTER:** bunches them into bursts and lulls.
- **TONE:** a dull tick or a sharp snap.
- **WIDTH:** spreads each click across left and right.
- **FOLLOW:** clicks only while the sound plays, so it falls silent when you stop.
- **LEVEL:** how loud the clicks are.

It's the fifth unit on the FX page's MOD / LO-FI tab (the units there are a little smaller now so all five fit), with a counter page showing every click in the last 2.5 seconds. The mod matrix can move RATE, CLUSTER and LEVEL. Switched off it changes nothing; it uses about 0.4% of a core even at 2,000 clicks a second. Our own design.

### MELTDOWN (expansion)
Inspired by the score of a 2019 series about a nuclear disaster, made almost entirely from the sounds of a power plant: pressure drones, groaning and ringing metal, pumps breathing, a counter's crackle, a lone voice rising out of the noise. Invisible danger, dread and grief. Built from the synth (no recordings); our own sounds, nothing from the series on the plugin.

- **Leads:** Lone Voice, Warning Tone, Bowed Rim
- **Pads:** Pressure Front, Glass Partials, Mourning Choir
- **Basses:** Turbine Hum, Coolant Pump, Stressed Steel
- **Keys:** Struck Pipes, Abandoned Upright, Tolling Bell
- **Arps:** Counter Ticks, Relay Bank, Distant Siren

**MACRO 3 is DOSE** on every preset. At 0 there's just a rare, faint tick. Turn it up and the clicks thicken into a roar while the sound corrodes, each preset its own way: crushed, driven, drifting out of tune, worn like old tape, or ringing harsher.

Levels are measured, not set by ear.

## 0.55.0

**Four presets get their movement back.** After the drums moved to PR7-D, these four sat in their banks' arp slots but only held a chord. Each now plays a real arp of its own:

- **Battle Cry** (CHROMEFURY): hammers the chord you hold in sixteenths over two octaves, up and down, on driven saws. Accents land on the beat, and quick bursts of 2 to 3 repeated hits come before each bar turns.
- **Festival Bronze** (PSI STORM): a fast gamelan figure in sixteenths that leaps up an octave and back. Two strikes a bar are left out so it breathes, and each bronze strike rings over the next.
- **Ritual Choir** (CYBERSHRINE): sings the chord one voice at a time, a note a beat, climbing and falling over two octaves. Each note swells from "oo" to "ah" and keeps ringing, so the choir builds as it climbs.
- **Sacred Chant** (ASCENSION): a monks' chant in eighths, walking up the notes you hold with a repeating fifth-and-octave figure. Every syllable opens from a hum into "ah". Hold one note for a plain chant.

Levels are measured, not set by ear.

## 0.54.0

**PR7-K is now purely a synth.** The drums have moved out: PR7-D is the family's drum plugin.

### Removed
- The DRUMS tab and page: the 12-slot kit, the drum sequencer, the sample editor, kit and sequence files, and MIDI export of beats. The other tabs now share its space in the header.
- The on-screen keyboard no longer tints the kit's keys.

### Your songs and presets
- Anything saved with the drums still opens: the synth part loads as before, and the drum settings and samples are simply left out (and not saved again).

### Presets
- Bank A's three drum presets (Kit And Bass, Beat And Bass, Tight Kit) are gone. **Sung Vowels moves from A43 to A40.**
- The 18 expansion presets that played a beat keep their synth part, without the beat. Their descriptions are rewritten and their levels re-measured. Six were renamed:
  - War Drums is now **War Pulse** (SANDSEA)
  - Ritual Drums is now **Ritual Choir** (CYBERSHRINE)
  - Battle Drums is now **Battle Cry** (CHROMEFURY)
  - Sacred Drums is now **Sacred Chant** (ASCENSION)
  - Festival Drums is now **Festival Bronze** (PSI STORM)
  - Drum Machine Run is now **Offbeat Run** (BLOODWAVE)
- The others keep their names: Alarm Pulse, Bootstrap Sequence, System Breach, Machine City, Ticking Clock, Cold Corridor, Countdown, Steady Rows, Midnight Run, Night Runner, Machine Shift and Threshold.

Levels are measured, not set by ear.

## 0.53.0

**PARADOX**, a new expansion (PX01-PX15), and **ECHOES**, the effect it's built round.

### ECHOES (new effect)
Your own playing comes back from the past, in time with your song. ECHOES keeps a running recording of what you play (up to 32 seconds) and plays it back under what you're playing now, from a set distance in the song: 1 beat to 8 bars. Four modes:
- **ECHO:** the sound from that distance ago, exactly (a long, bar-synced echo). FEEDBACK gives echoes of echoes.
- **REVERSE:** each stretch comes back backwards while the next plays, so it swells into its own start.
- **TAPE:** the start of each phrase comes back at half speed, an octave down.
- **LOOP:** a stretch is caught and repeated 1 to 8 times (or forever) while you play on over it.

AGE wears each pass down (darker, a little saturated), so older echoes sound older. It's the fourth unit on the FX page's MOD / LO-FI tab, with its own page showing the recording and the part coming back. The mod matrix can move MIX, FEEDBACK and AGE. Switched off it changes nothing; it uses about 0.2% of a core. Our own design.

### PARADOX (expansion)
Inspired by the score of a German series about a small town where time runs in a loop: crushing sub drones, walls of noise, bowed strings, a choir, ticking clocks and rain, rumbling underground passages, melancholy glitchy electronica. Our own sounds; nothing from the series on the plugin.

Every preset hears its own past come back through ECHOES, each in its own way: Yesterday's Voice (answered backwards), Caught In The Loop, Slow Tape Ghost, Wall Of Static, Bowed In Reverse, Choir Of Ages, Pressure Below, Decay Pulse, Underground Rumble, Worn Upright, Music Box Memory, Chapel Organ, Second Hand, Rain On The Window and Bootstrap Sequence (with a drum beat; the kit on C2 to B2).

**MACRO 3 is ERA** on every preset, and loads in the middle: the present. Turn it left to go back in time (a worn cassette, a slow tape wobble, older, darker echoes) and right to go forward (crushed, colder, sharper, a glassy edge). The left end plays a few dB quieter, as an old tape does.

Levels are measured, not set by ear.

## 0.52.0

**A switch for the EXPR wheel's volume** (your ask). In SETTINGS, under TUNE, there's a new **EXPR WHEEL** section with a **CONTROLS VOLUME** switch.

- **On** (as before, and the default): on presets that don't use EXPRESSION, the EXPR wheel (or your keyboard's expression pedal) is a volume pedal. Left at the bottom, those presets are silent.
- **Off:** on those presets the wheel and pedal do nothing, so a pedal left at the heel (say after rocking a SLINGBLADE wah) can never silence the next preset.
- Presets that use EXPRESSION, like the SLINGBLADE wahs, follow the wheel either way.
- Like TUNE, it's your setting, not the sound's: it stays as you set it when you change presets, and it's saved with your project.

## 0.51.0

**The WAH** (your ask): a wah on every insert bus (FX > INSERTS, buses A to D), first in line, before the DRIVE, so a drive works on the swept peak like a wah into a distortion pedal.

- **Our own design**, starting from PR7-G's wah but in stereo, and fuller: the resonant peak plus some of the low end under it, so the heel sounds dark and throaty rather than thin.
- **Three modes:** PEDAL (you rock it), AUTO (it opens with how hard you play, then falls back), LFO (it sweeps by itself, free or in time with your song).
- **Three ranges:** CLASSIC (350 Hz to 2.2 kHz, the classic pedal), WIDE (220 Hz to 3.5 kHz) and LOW (90 to 900 Hz, for basses).
- **Knobs:** RESONANCE, plus SENS, SPEED and DEPTH for AUTO, RATE or a note length for the LFO, MIX and LEVEL.
- **Works with the EXPR wheel:** each bus's wah pedal can be moved by the MOD matrix. Route EXPRESSION to "FX Insert A > Wah Pedal" and the EXPR wheel, or your keyboard's expression pedal, rocks it. While EXPRESSION is routed there, it no longer changes the volume. Until you first move the wheel or pedal, it counts as all the way up, as the wheel is drawn.
- **Its page** shows the filter's curve where the wah is now (orange), and at the heel and the toe (grey), moving as you play.
- **Nothing changes in your projects:** the wah is off by default.

**New expansion: SLINGBLADE** (SB01 to SB15), your theme and name: brutal heaviness, soaring epicness and gilded opulence, in three acts. Every category has one sound from each act: the mines below, the gilded court at war, the dawn of the rebellion. MACRO 3 is **RISE** on every preset, taking each sound from held down to defiant. Our own sounds; nothing from the books on the plugin.

- **Leads:** Drillbit Scream (a hard-synced saw through the wah on EXPR into a RAT-style drive), Gilded Fanfare (regal brass with the fifth stacked), Dawnbreaker (a supersaw anthem scooping into each note, the wah on EXPR).
- **Pads:** Under The Crust (a dark drone, an FM rumble and tunnel wind), Glass Palace (a shimmering wavetable pad), Rising Tide Choir (the VOICE choir swelling from a hum to "ah").
- **Basses:** Pick And Hammer (the AUTO wah opening on every note, into an overdrive), Warship Hull (a huge drifting reese), Uprising (a pumping step-sequenced bass).
- **Keys:** Lullaby Below (a fragile FM music box), Ballroom Harpsichord, Victory Bells (bells built from a bell's partials, with a major third for triumph).
- **Arps:** Machine Shift (a heavy drum beat and clanging metal through the LFO wah), Siege Clock (a ticking pattern rolled from the minor scale), Fleet Assault (three arps locked together, the wah on EXPR).

## 0.50.0

**The front panel now matches PR7-G** (your four tweaks, with PR7-G as the reference):

- **PR7-G's pearl black.** The face was painted in a dark purple-grey of my own. It's now drawn by PR7-G's own code for the unit: the same pearl black rim and satin plate, its sheen and grain, and the red light around the edge.
- **PR7-G's logo.** "PR7-K" was typed in an ordinary italic font. The badge is now PR7-G's: PARAGON stamped over the PR7-K mark in the slanted, speed-cut letters with the red 7.
- **The wheels have the expression pedal's tread.** MOD and EXPR are now neon pink-to-orange rubber with fine ribs, as on PR7-G's pedal. They're shaded where the wheel curves away, and the ribs roll as you turn them.
- **Gradient knobs.** PR7-K had no finish setting, so it fell back to pearl white with black knobs. It's now a pearl black unit, so the big knobs have PR7-G's neon gradient. The same finish lights the FX page's power switches, small keys and knobs neon, as on PR7-G's pearl black unit.

The knobs and wheels also sit a little higher, so their values no longer touch the bottom rim.

## 0.49.0

**A new face for the SYNTH page: the PR7 front panel** (your design). Where the OSCILLATORS and FILTER panels were, PR7-K now opens on a PR7 unit's front panel, a cleaner look for playing:

- **The LCD screen** in the middle (the floor unit's screen: chrome frame, black bezel, orange glow): the preset's slot and bank (SV01 · SEVERED), its name in big letters, and its notes scrolling along the bottom; EDITED lights when you've changed the sound. Click it to open the preset browser; the ▲ ▼ keys beside it step through the presets.
- **The three macros** as big red pedal knobs under the screen, named after the preset's own macros (BRIGHT / SPACE / DREAD...).
- **Four hero knobs:** CUTOFF and RESONANCE (FILTER) on the left, ATTACK and RELEASE (AMP) on the right. Like every knob, they take MIDI learn and dropped modulation, and show their modulation rings.
- **Two wheels** at the far left: **MOD**, the mod wheel (MIDI controller 1, as your keyboard's), and **EXPR**, an expression pedal (controller 11). Drag them; double-click returns them to rest. They follow your keyboard's own wheel and pedal.
- **SHOW RACK / HIDE RACK** (under OUTPUT) swaps the front panel for the full OSCILLATORS and FILTER panels in the same space, and back. PR7-K remembers which you left it on. The envelopes, AMP, PLAY and the keyboard stay as they are.

**Expression now swells the volume.** Controller 11 (the EXPR wheel, or an expression pedal on your keyboard) sets the whole sound's volume, as on most synths, unless the preset routes EXPRESSION in its mod matrix (then it drives those routes) or EXPRESSION is sent to the mod wheel (SETTINGS). Full until any comes. Note: a DAW track or MIDI file that sends expression at 0 will now quiet the synth until it comes back up.

## 0.48.0

**SEVERED**, the twenty-first expansion: a bank of its own under EXPANSIONS (SV01 to SV15), named by you. Inspired by the score of a 2022 series about office workers whose work memories are split from their home lives: a cheerful, sterile corporate surface (muzak, chimes, retro screens, humming lights, long white corridors) over quiet dread and a melancholy piano. Our own sounds; no title, company, departments, characters, terms or music from the series.

- **MACRO 3 is DREAD on every preset**: the cheerful surface turns uneasy (a tritone or half-step clash creeps in, the sound grits up, the tape wobbles). MACRO 1 brightens, MACRO 2 is SPACE as in every expansion.
- **Each preset is built round a different part of the synth**, so no two are made alike:
  - **Leads:** Cheerful Memo (a hand-drawn wave evened out by EDGE's SYMMETRY), Number Sorter (a stepped retro-screen lead: CRUSH in RATE mode), Hallway Whistle (FORMANT's X-FADE through a band-pass on the note, long echoes).
  - **Pads:** Fluorescent Hum (harmonics flickering: the FFT editor's VSPEED / VSTRENGTH, with ECHELON's dynamic band), Wellness Room (soothing, but leaning out of shape: SKEW and PINCH moved by slow LFOs), Elevator Descent (the Organ Stack table through a rotary speaker, sinking slowly).
  - **Basses:** Quarterly Bass (the step sequencer with tied steps), Severed Sub (a clean sub, and a saw above it chopped by GLITCH's KILLSWITCH), Break Room Bass (a bark at every note from SYNC's own envelope).
  - **Keys:** Lobby Piano (the felt piano as muzak through LO-FI's AM RADIO), Incentive Marimba (a bright FM marimba), Memo Chimes (chimes whose colour changes as they ring: TRAVEL's strength curves).
  - **Arps:** Data Refinement (two arps together: CHORDS-mode stabs and picked notes between them), Sorting Pulse (ratchets and slides), Cold Corridor (reversed drum hits swelling into each beat, kit on C2 to B2, under a cold pulse).
- **Levels** were balanced by measurement, not by ear: listen and adjust to taste.

**The expansions now scroll.** The EXPANSIONS list in the preset browser is a scrolling list (your choice), so there's room for any number of banks. The buttons are back to full-size rows (with 21 banks they had shrunk to 16 px), a thin scroll bar appears when they don't all fit, and the list keeps the bank you chose in view.

## 0.47.0

**SUPERPOSITION**, the twentieth expansion: a bank of its own under EXPANSIONS (SP01 to SP15), named by you. Inspired by the score of a 2024 science-fiction series about a physicist thrown into other versions of his own life, searching for the way home: cold, cerebral and quietly dreadful, the same sound in many versions (copies drifting apart, notes never the same twice, patterns that never repeat), and under it the warm thread of home. Our own sounds; no title, characters, places, terms or music from the series.

- **MACRO 3 is SPLIT on every preset**: the sound pulled into more, and more different, versions of itself (copies further out of tune, a second voice, more unison, a wider roll). MACRO 1 brightens, MACRO 2 is SPACE as in every expansion.
- **Leads:** Other Self (a second voice arriving a moment late, nine cents apart, on the other side), Collapse Lead (hard sync rolled at random on every note: never the same twice), Lucid Line (a hand-drawn wave, clean and close).
- **Pads:** Interference (a chord flickering between two colours), Quantum Fog (breath sung through VOICE, its vowel drifting), Infinite Echo (trailing off into echoes of itself).
- **Basses:** Probability Bass (TRAVEL changing its colour beat by beat, with your song), Event Horizon (a deep FM growl; each note falls an octave when you let go), Fracture Bass (crushed and gritty).
- **Keys:** Home Piano (the warm felt piano), Many Worlds Keys (each key three copies, a little out of tune, spread wide), Lab Glass (a cold struck-glass tone).
- **Arps:** Branching Paths (a new pattern every pass), Countdown Pulse (a ticking sixteenth pulse), Threshold (a slow heartbeat on the drums, C2 to B2, under a dark throb, with your song).
- **Levels** were balanced by measurement, not by ear: listen and adjust to taste. Countdown Pulse sits a few dB quieter, so its sharp ticks stay under the peak limit.

**Fixed: clicks when the FFT editor's LP or HP moves with SLOPE turned up.** The cutoff blends two of the wave's levels; with SLOPE above 0 that pair moved one level at a time, so a sweeping cutoff jumped each time it crossed one (heard as faint clicking or zipper noise, in DUSTFALL's Searchlight for one). The pair now moves so the sound changes smoothly. With SLOPE at 0 nothing changes; with SLOPE up, a cutoff sitting still sounds very slightly different from before.

## 0.46.0

**DUSTFALL**, the nineteenth expansion: a bank of its own under EXPANSIONS (DF01 to DF15), named by you. Inspired by the score of a 2017 science-fiction film, a sequel set thirty years on: colder, bigger and more brutal, with colossal distorted brass blasts and crushing sub drones, orange dust and grey snow over empty cities, machines at work, and, in between, fragile, lonely beauty. Our own sounds; no title, characters, places, terms or music from the film.

- **MACRO 3 is WEIGHT on every preset**: more low end (a clean sub an octave down on most), more distortion or a bigger space. MACRO 1 brightens, MACRO 2 is SPACE as in every expansion.
- **Leads:** Hologram Voice (sung by the VOICE filter, a glassy "ee" opening to "ah", flickering faintly), Warning Blast (an enormous distorted brass blare lurching up into each note), Memory Thread (a fragile sine wavering like an old recording, long echoes).
- **Pads:** Orange Haze (the Analog Morph table drifting, worn by cassette dust), Grey Snowfall (a thin, glassy wave from the FFT editor with bright noise flakes falling through it), Monolith (a vast low distorted drone: play it low).
- **Basses:** Sub Collapse (sags in pitch after each hit), Engine Throb (throbs like an idling machine, slower low and faster high), Iron Wall (a fuzzed wall through BANSHEE and a hard shaper).
- **Keys:** Ghost Keys (a glassy FM electric piano, half there), Distant Piano (NEGATIVE SPACE's soft felt piano, darkened as if from far away), Cold Bells (a struck metal bar's own overtones: it rings, not sings).
- **Arps:** Data Pulse (cold, precise sixteenths), Searchlight (slow swells with a sweep passing over them), Machine City (heavy low industrial drums on C2 to B2 under a pulsing low synth, with your song).
- **Levels** were balanced by measurement, not by ear: listen and adjust to taste.

## 0.45.0

**MIMIC, part 2: the MATCH and REFINE.** MIMIC now plays what it made and compares it with your recording, then improves it on its own.

- **MATCH:** when you MAKE THIS SOUND, MIMIC plays the made sound in the background (on a private copy of the synth, so what you're playing isn't touched), the note it was recorded at, held as long, and compares it with the recording: the **tone** (the spectrum, moment by moment, as a shape) and the **loudness** over time, each as how many dB apart on average. The MATCH percentage sums them up (1 dB apart is 90%, 3 dB 74%, 6 dB 55%: our own scale). The volume is matched for you (the made sound's LEVEL is set so it's as loud as the recording), so PLAY ORIGINAL and PLAY RESULT compare fairly.
- **The picture** draws the made sound's loudness (dashed, white) over the recording's, so you can see where they part.
- **REFINE:** nudges, one at a time, what MIMIC had to estimate (the attack, decay, sustain and release and the shapes of their falls, the noise's level, unison's detune, vibrato's depth, how fast the tone moves), each up and down, keeping a nudge only if the match improves; three passes, finer each time (about 30 to 50 plays, a few seconds). The refined sound is put in place (one undo takes you back to the first one), the match shown before and after.
- **Honest limits:** REFINE improves the sound, not necessarily each setting: some settings can trade off against each other (a slightly lower sustain making up for a different decay shape). The match is our own measure, not your ears: trust what you hear over the number. Tested on sounds PR7-K made with known settings: the match rose (82% to 87% on a straight-line envelope MIMIC first guessed as natural curves), the attack and release came nearer the truth, and the volume matched within 1.5 dB.

## 0.44.0

**MIMIC**: make a sound from a recording of one (your ask and name, like the "preset cloners" for other synths). On the PRESETS page: **MIMIC A SOUND...** in the right column. Load a recording of one note (or drop it on the panel) and MIMIC listens to it, then builds a sound for PR7-K from what it heard. Our own design: it measures, nothing is learned from other people's presets.

- **What it takes from the recording:**
  - **its tone**: its own waves, cycle by cycle, as a wavetable (up to 64 frames from the start of the note to where it settles), walked by TRAVEL as the note plays, so a sound that brightens, darkens or changes as it goes does the same.
  - **its loudness**: attack, decay, sustain and release fitted to how its level moves (held sounds and dying ones, like plucks and bells).
  - **its movement**: vibrato (how fast, how deep, how late it starts), a swoop into the note (the PITCH envelope), unison (copies a little out of tune: heard from how its harmonics beat), noise or breath (oscillator 2), and stereo width.
- **The panel** shows the recording's loudness with where the note starts, settles and is let go, the stretch its tone is taken from, its pitch, and what was found (the note to play it at, the envelope times, vibrato, swoop, unison, noise). Toggles choose what to take (TONE, LOUDNESS, VIBRATO, SWOOP, UNISON, NOISE; what wasn't found is greyed). **MAKE THIS SOUND** puts it in place as a new sound (one undo takes it back; save it like any other). **PLAY ORIGINAL** and **PLAY RESULT** let you compare (the result plays the note it was recorded at, held as long).
- **Best on** one note, held, fairly dry: leads, basses, pads, plucks, keys. Chords, drums and whole mixes have no single pitch to follow (it says so). Reverb and echoes are taken as part of the sound. Play the result near the note it was recorded at: far from it, its tone moves with the keys.
- **Honest limits:** it's close, not a copy (our oscillators and envelopes are our own), and some figures are estimates (how unison's beating maps to detune, how loud the noise should be). Checked by measurement on sounds made with known settings (pitch within 3 cents, tone within 1.5 dB harmonic by harmonic, sustain, release, vibrato, swoop, unison, noise); not by ear here. **MIMIC's second part** will play the result back, score how close it is, and refine it automatically.

## 0.43.0

**BLOODWAVE**, the eighteenth expansion: a bank of its own under EXPANSIONS (BW01 to BW15), named by you. Inspired by the soundtrack of a 2012 top-down action game set in a neon-soaked 1989 city: the dark, dirty side of the '80s, with driving distorted synths at club tempos, basses pumping under a drum machine, hazy, feverish detuned loops, worn tape and bit-crushed grit. Our own sounds; no title, characters, masks, the city, artists or music from the game.

- **MACRO 3 is RAGE on every preset**: more drive, crush or wobble.
- Leads: **Neon Blade** (a bit-crushed square through a screaming drive), **Fever Lead** (warbles like a stretched tape), **Siren Scream** (a distorted saw that dives an octave every time you let go of a note).
- Pads: **Pink Haze** (a very wide, detuned haze on worn tape), **Night Static** (a chord heard down a crackling phone line), **Comedown** (a dark drone that sags flat and closes over eight seconds).
- Basses: **Pump Bass** (ducks on every beat, the club's breathing: hold the notes), **Darkgrind Bass** (filthy and distorted, the filter chewing in sixteenths with your song), **Sub Thump** (a sine that drops into every hit, like a drum-machine bass).
- Keys: **Woozy Keys** (an electric piano whose pitch swims), **VHS Bells** (glassy bells off a worn tape), **Club Stabs** (one key plays a whole minor-seventh chord).
- Arps: **Rampage Arp** (fast and driven, the odd glitch stutter), **Hypno Loop** (two triangles pulled out of tune, spinning echoes), **Drum Machine Run** (four-on-the-floor kick, claps, hats and an offbeat bass: play above C3, the kit is on C2 to B2).
- Levels balanced by measurement against the other expansions (Club Stabs set so a three-key chord, twelve notes, doesn't clip). Made without listening here, so your ears decide.

## 0.42.0

**PSI STORM**, the seventeenth expansion: a bank of its own under EXPANSIONS (PS01 to PS15), named by you. Inspired by the score of a 1988 animated cyberpunk film about a biker gang and a boy whose psychic power breaks loose: shimmering bronze gamelan and deep bamboo, hypnotic group chant and slow monks' chant, a neon city at night, light trails, and power swelling out of control. Our own sounds; no title, characters, the city, the band, terms or music from the film.

- **MACRO 3 is POWER on every preset**: more drive, clang, swell or a sharper voice: the energy breaking loose.
- Leads: **Chant Leader** (a man's voice calling out, its vowel stepping through syllables in time with your song), **Tail Light Streak** (drops into every note from an octave up, fast echoes trailing behind), **Psychic Scream** (grows the longer you hold it: the pitch climbs a whole step, it tears and distorts, then cracks at the top).
- Pads: **Requiem Chant** (low monks chanting in parallel fifths), **Ruined City** (cold, shifting metal with wind through it), **Power Surge** (swells over six seconds into a bright, unstable roar).
- Basses: **Bamboo Bass** (hollow struck tubes in beating pairs), **Night Ride** (a chase bass jumping octaves in sixteenths with your song: hold a note), **Mutant Flesh** (a bloated, uneven wobble).
- Keys: **Bronze Gamelan** (each note on two instruments tuned slightly apart, so they shimmer, as gamelan pairs do), **Temple Gong** (blooms after the strike and sags in pitch: play low), **Bamboo Marimba** (woody and bright).
- Arps: **Interlock** (two bronze parts filling each other's gaps in the pelog scale: hold just the root key), **Chant Rhythm** (barked shouts in a 3-3-2 rhythm), **Festival Drums** (big low drums with rolls under the bronze: play above C3, the kit is on C2 to B2).
- **New scale for the arp's RANDOMIZER: Pelog**, the gamelan's own five notes (its common form, to the nearest semitones). Added at the end of the list, so no existing preset changes.
- **The preset browser fits more expansions**: with 17, PSI STORM's button was cut off at the bottom; the rows now pack tighter when space runs short (about 21 fit).
- Levels balanced by measurement against the other expansions. The gamelan, gong and bamboo are synthesized, not recorded instruments. Made without listening here, so your ears decide.

## 0.41.0

**REFRACTION**, the sixteenth expansion: a bank of its own under EXPANSIONS (RF01 to RF15), named by you. Inspired by the score of a 2018 science-fiction film about an expedition into a zone where everything is refracted and copied: a quiet fingerpicked string, light bending into colours, sounds that mutate and double as you hold them, and a vast, wailing alien swell. Otherworldly, as you asked. Our own sounds; no title, characters, places, terms or music from the film.

- **MACRO 3 is MUTATE on every preset**: it pushes each sound from lovely to wrong in its own way (pitch pulled into microtones, a bent wave, a stranger voice, more crunch, more twinkles).
- **Each preset is built round a different part of the synth**, so the leads and pads don't sound like the other banks'.
- Leads: **Refraction** (one note splitting like light as you hold it: a fifth fading in on the left and an octave on the right, each sliding into tune from a little off), **Mirror Voice** (a double that copies you a moment late, sliding up from a half step under, sung as an inhuman "ee" by the VOICE filter), **Hollow Wail** (the vast alien swell: a saw whose SYNC opens as the note grows, scooping into every note and gliding slowly between them, through a drive).
- Pads: **Prism Veil** (glass from the FFT editor, swinging slowly between two versions of itself so its overtones rise and fall), **Overgrowth** (a wavetable that keeps turning into something else over about twelve seconds, starting somewhere different on every note), **Spore Drift** (whispered noise drifting through vowels, high tones twinkling on and off at random).
- Basses: **Mutation** (each note starts plain and turns strange over three seconds), **Root Rot** (a sub with a damp, off-pitch growl breathing in a hand-drawn, irregular shape), **Cell Division** (a pulse in eighths with your song that splits off an octave copy on every beat: hold a note).
- Keys: **Folk Pluck** (a quiet fingerpicked string: guitar-like, not a sampled guitar), **Bloom Keys** (bells that open up after you strike them, into a reverse reverb that swells behind each note), **Crystal Trees** (brittle, grainy chimes, a little out of tune with each other).
- Arps: **Doubling** (two arps mirroring each other, one climbing, one falling, a slow flanger through both), **Bioluminescence** (random blips in a dreamy whole-tone scale, each chirping down into its pitch, a new pattern every pass), **Pulse Garden** (a slow pulse that breathes a vowel on every note, with the odd flutter).
- Levels balanced by measurement against the other expansions. Made without listening here, so your ears decide.

## 0.40.0

**ASCENSION**, the fifteenth expansion: a bank of its own under EXPANSIONS (AS01 to AS15), named by you. Inspired by the score of a 2001 science-fiction game about an ancient ring-shaped world: a monks' chant growing into a vast, epic choir, heroic strings and brass, big drums, mystery and awe. Our own sounds; no title, characters, factions, places, terms or music from the game.

- **Built on the new VOICE filter** (0.39.0) for choirs that sound human: buzzy sources sung into vowels whose resonances stay fixed whatever note is sung, each section's voices with their own vibrato drifting against each other, every chord opening from a hummed "oo" into the vowel, a breath of air through it. In **Grand Choir** and **Choir Stabs** the singer's size follows the keys: low notes sung by basses, high notes by sopranos, so a wide chord sounds like a four-part choir.
- Leads: **Monk Solo** (a single low chanting voice), **Soprano Call** (a soaring soprano with an operatic vibrato), **Hero Theme** (brass and strings together, a horn's body from the VOICE filter).
- Pads: **Monk Chant** (low voices in slow unison), **Grand Choir** (the full four-part choir), **Angel Choir** (airy, breathy sopranos).
- Basses: **Low Chant** (a dark "oo" voice over a clean sub), **Battle Strings** (a driving sixteenth-note ostinato with your song: hold a note), **Deep Brass** (a low swell: press into the keys to push it).
- Keys: **Choir Stabs** (short "ah!" shouts), **Ancient Bells** (built from a real church bell's partials, each fading at its own pace), **Lament Piano** (a sad piano far back in a hall).
- Arps: **String Charge** (a racing string ostinato), **Sacred Drums** (huge slow drums under a chanting choir: play above C3, the kit is on C2 to B2), **Signal Pulse** (a pulsing electronic arp).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character (mostly VOWEL on the choirs, so they can "speak"; POWER, BITE, BLARE, AIR, HUM, ECHO, GLOW on the others). Each preset's notes say what they do.
- Levels balanced by measurement against the other expansions (Choir Stabs, Ancient Bells and Sacred Drums by their peaks). Synthesis, not a recorded choir: made without listening here, so your ears decide.

## 0.39.0

**The VOICE filter**, for choirs and voices that sound human (your ask, before the ASCENSION expansion). A new tab, **VOICE**, beside FILTER and SHP 1-4.

- **Why it was needed:** a real voice gets its vowel from resonances of the throat and mouth ("formants") that stay at the same frequencies whatever note is sung. Our choirs so far came from the Vowels wavetable, whose vowels move with the note: thin and chipmunky up high, muddy down low. That's the main thing that made them sound synthetic.
- **What it does:** five resonances at a vowel's formants, fixed in frequency, shape whatever goes in (feed it something buzzy: a saw or a narrow pulse) into a sung vowel. **VOWEL** moves smoothly through OO, OH, AH, EH and EE; **SIZE** goes from a bass singer's voice through tenor and alto to a soprano's; **SHARP** makes the vowel softer and breathier or sharper and more nasal; **MIX** blends it with the sound going in. The oscillators ticked on the page go through it (all of them, unless you untick some). It comes after the FILTER, before the effects, so the reverb and echoes hear the sung vowel too.
- **Made to sound even:** narrow formants make some notes jump out (when a harmonic lands right on one, up to 10 dB louder) and others drop away; as a sampled choir's notes are levelled, so is each note here, within a few dB. And as sopranos open a vowel on high notes (singers call it vowel modification), the first formant moves up when a note goes above it, so high notes on OO and OH don't vanish.
- **Playable:** VOWEL, SIZE, SHARP and MIX are in the mod matrix: route the mod wheel or a macro to VOWEL and it speaks.
- The formant data (each vowel's frequencies, levels and widths for a bass and a soprano) is the widely published table from sound-synthesis textbooks; the blending, levelling and vowel modification are our own. Synthesis, not a recorded choir: made without listening here, so your ears decide.
- New starting preset **A43 Sung Vowels**: a small choir singing AH, the MOD WHEEL moving it towards EE.
- About half a percent of one core more for a four-note unison chord.

## 0.38.0

**CHROMEFURY**, the fourteenth expansion: a bank of its own under EXPANSIONS (CF01 to CF15), named by you. Inspired by the score of a 2015 post-apocalyptic chase film: relentless war drums, sawing string ostinatos, guitar-like screaming leads over growling low synths, engines, speed, dust and fire. Our own sounds; no title, characters, vehicles, places, phrases or music from the film.

- **Lead-heavy, as you asked: 6 leads** (and 3 pads, 3 basses, 1 keys, 2 arps), heavier than HELLFORGE's. Distortion is stacked like a guitar rig: in series (a gritty wave, a shaper before the filter and another after it, an insert drive) and in parallel (two layers through two different drive models on two buses, like two amps on one guitar), with a power chord inside each note and a clean sub kept out of the distortion so the low end stays solid. Every lead dives an octave on the pitch bend.
- Leads: **Highway Scream** (a screaming, guitar-like solo lead: RABID and BANSHEE in parallel), **Chrome Howl** (a sync howl through two shapers and COSMONAUT's fuzz), **Fury Saw** (the thickest lead in the plugin: twelve saws over two octaves through CHIMERA and SKYLINE; plays chords too), **Nitro Wail** (a hard FM scream, wave-folded into RABID), **Iron Throat** (a distorted lead that talks: the MOD WHEEL moves the vowel like a talk box), **Thunder Octave** (an octave-fuzz snarl through COSMONAUT).
- Pads: **Sand Storm** (a distorted, wind-blown pad), **Burning Horizon** (a heavy pad rising for four seconds), **Dust Choir** (a driven choir).
- Basses: **Engine Growl** (the MOD WHEEL revs it), **Diesel Sub** (a clean sub with a distorted growl on top), **Piston Drive** (pumping eighths with your song: hold a note).
- Keys: **Rust Stabs** (big driven chord stabs).
- Arps: **Battle Drums** (pounding low toms under a driven chord: play above C3, the kit is on C2 to B2), **Pursuit** (a sawing string ostinato with a Phrygian bite).
- Macros: 1 BRIGHT, **2 SPACE** and **3 FURY** (more gain) on every preset. Each preset's notes say what they do.
- Levels balanced by measurement against the other expansions (Battle Drums by its peaks); made without listening here, so your ears decide.

## 0.37.0

**STOWAWAY**, the thirteenth expansion: a bank of its own under EXPANSIONS (SW01 to SW15), named by you. Inspired by the score and sound of a 1979 science-fiction horror film about something hiding aboard a ship in deep space, leaning into suspense and tension as you asked: half-step clashes, pitches that creep and sag, uneven swells, and dread that never resolves. Our own sounds; no title, characters, ship, creature, company or music from the film.

- **Leads and pads built differently**, as you asked: none is the mono glide-and-vibrato lead or the unison / choir pad the earlier banks lean on. Each is built round a part of the synth no expansion had used: a hand-drawn wave, the FFT editor, drawn LFO shapes and envelopes, band-pass and notch filters, ring-style AM.
- Leads: **Echo Flute** (a breathy drawn-wave flute, played straight, into long dark repeating echoes), **Bone Horn** (a primitive rasping horn: a saw cut to its odd harmonics in the FFT editor, roughened by ring-style AM), **Cold Whistle** (a thin whistle through a narrow ringing band-pass, sagging a quarter-tone flat as you hold it).
- Pads: **Hull Hum** (a ship's room tone: a notch filter wandering through a low buzz, vents breathing above), **Cluster Strings** (a half step inside every chord, swelling unevenly: the MOD WHEEL adds a tritone), **Harmonic Shiver** (only the high, glassy partials, trembling).
- Basses: **Dread Brass** (a low half-step cluster that blasts, falls back and swells into a bigger blast), **Pressure Drone** (a sub that creeps up a semitone over twenty seconds of holding), **Steam Bass** (a bent square bass with a hiss of steam on every note).
- Keys: **Bowed Metal** (a metal ring that flares, falls back, then swells as if bowed), **Data Chatter** (crushed computer blips, each at a random pitch), **Dread Piano** (a muffled piano with a buzzing rattle in its strings).
- Arps: **Proximity Ping** (hold one note: the pings come faster and faster), **Vent Crawl** (random chromatic scratching in the walls), **Alarm Pulse** (a whooping tritone alarm over a ticking beat from the drum sequencer: play above C3, the kit is on C2 to B2).
- Macros: 1 BRIGHT, **2 SPACE** and **3 TENSION** on every preset: a clashing half step or tritone, a deeper creep or sag, or a rougher sound. Each preset's notes say what they do.
- Levels balanced by measurement against the other expansions (Vent Crawl, Dread Piano, Harmonic Shiver, Data Chatter and Alarm Pulse by their peaks); made without listening here, so your ears decide.

## 0.36.0

**CYBERSHRINE**, the twelfth expansion: a bank of its own under EXPANSIONS (CS01 to CS15), named by you. Inspired by the score of a 1995 animated cyberpunk film about a mind that may be a ghost in a machine: an ancient chorus of women's voices in close, clashing harmony, ornamented like a folk ritual, huge slow drums and deep temple bells, and a cold, rainy, reverberant machine world. Our own sounds; no title, characters, places, terms or music from the film.

- Leads: **Folk Voice** (a keening "ah" voice flicking up into every note from just below, like a folk singer's ornament: the MOD WHEEL adds vibrato), **Wire Song** (a glassy synced digital lead), **Half There** (a lead that drifts in and out of hearing).
- Pads: **Ancient Chorus** (the centrepiece: a choir with a fifth above each note and a second choir a whole step up rubbing against it), **Rain City** (a dark, wet pad with rain falling through it), **Deep Dive** (a pad that sinks from bright to dark over ten seconds of each chord: the MOD WHEEL brings it back up).
- Basses: **Temple Drum** (a huge drum you play as a bass, dropping an octave into each note), **Cold Current** (a dark bass pulsing in sixteenths with your song: hold a note), **Machine Heart** (a slow, low heartbeat with your song).
- Keys: **Temple Bell** (deep bells that beat and shimmer for eight seconds), **Silk Strings** (zither-like plucks, each pressed up a semitone just after it sounds), **Glass Net** (cold digital keys).
- Arps: **Ritual Drums** (slow, booming low drums and a deep bell each bar from the drum sequencer, with your chord above C3 sung by a choir; the kit is on C2 to B2), **Signal Ghost** (random glassy blips with long echoes, a new pattern every pass), **Dive Sequence** (a rising, watery arpeggio).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character: ORNAMENT, SYNC, SHIMMER, CLASH, RAIN, DEPTH, BOOM, PULSE, GROWL, BEAT, BEND, BITS, VOWEL, ECHO. Each preset's notes say what they do.
- Levels balanced by measurement against the other expansions (Silk Strings and Ritual Drums by their peaks); made without listening here, so your ears decide.

## 0.35.0

**UNSPOKEN**, the eleventh expansion: a bank of its own under EXPANSIONS (UN01 to UN15), named by you. Inspired by the score of a 2016 film about first contact and a language that changes how you see time: human voices breathing, murmuring and stuttering in eerie layers, vast swelling drones, circular phrases that run forwards and back, cold fog, quiet awe and grief. Our own sounds; no title, characters, terms or music from the film.

- Leads: **Glass Voice** (a clear, wordless "eh" voice drifting on its own, with a slow glide: the MOD WHEEL adds vibrato), **Distant Call** (a long call like a whale's, bending up into every note and sliding between them: the MOD WHEEL adds a growl), **Signal Tone** (a pure tone that won't quite hold still, with a slow tremolo: the MOD WHEEL deepens it).
- Pads: **Breathing Choir** (a vowel choir rocking between "oo" and "oh", breath swelling through it), **Circle Drone** (a drone whose colour keeps turning while it circles the room), **Low Mist** (a cold fog through a backwards reverb, so each chord seems to arrive before it's played).
- Basses: **Vast Hull** (a low ring like a struck metal hull), **Tidal Swell** (strings and brass rising and sinking every two bars: press into the keys to push it), **Deep Murmur** (a sub with a dark "oo" voice on top).
- Keys: **Ink Bells** (a bell strike with a soft tone blooming in after it), **Memory Loop** (NEGATIVE SPACE's felt piano, every note answered backwards), **Fog Glass** (cold, glassy keys in a cathedral).
- Arps: **Stutter Choir** (vowel syllables broken into bursts of two, three and four: hold a chord), **Circular Time** (a pattern that reads the same forwards and backwards), **Breath Pulse** (voices and breath cut into a rhythm with your song).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character: VOWEL, SCOOP, WOBBLE, BREATH, MOTION, FOG, METAL, BRASS, MOUTH, BLOOM, ECHO, SHINE, GLOW. Each preset's notes say what they do.
- Levels balanced by measurement against the other expansions (Stutter Choir and Breath Pulse by their peaks); made without listening here, so your ears decide.

## 0.34.0

**NIGHTDRIVE**, the tenth expansion: a bank of its own under EXPANSIONS (ND01 to ND15), named by you. Inspired by the score and soundtrack of a 2011 neo-noir film about a getaway driver: late-night retro synthwave, turned all the way up as you asked, with chorus on everything, big reverbs and long echoes. Our own sounds; no title, characters, songs or artists.

- Leads: **Night Cruise** (a smooth, singing lead with a slow glide), **Chrome Whistle** (a glassy, whistling square: the MOD WHEEL deepens the vibrato), **Romance Sync** (a soaring sync lead for big hooks: the MOD WHEEL pushes the sync).
- Pads: **Neon Haze** (a lush polysynth through two choruses), **Midnight Strings** (dreamy string-machine strings: press into the keys after playing to swell), **Afterglow** (a slow, warm swell with a gently gated shimmer).
- Basses: **Octave Drive** (the classic synthwave octave bass in sixteenths with your song: hold a note), **Velvet Bass** (round and warm), **Rubber Pulse** (bouncy, with a resonant snap).
- Keys: **Glass Tine** (the '80s digital electric piano: a glassy FM tine over a soft body), **Retro Stab** (a big, chorused chord stab), **Cassette Keys** (soft keys from an old tape).
- Arps: **Highway Lights** (sixteenths streaming up three octaves with dotted echoes), **Night Runner** (a drum-machine beat under a driving saw arpeggio: hold notes above C3, the kit is on C2 to B2), **Heartline** (a slow, emotional arpeggio with ping-pong echoes).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character: VIBRATO, OCTAVE, SOAR, LUSH, ENSEMBLE, GATE, SNAP, WARMTH, BOUNCE, BARK, PUNCH, WOBBLE, SPARKLE, ECHO. Each preset's notes say what they do.
- Every factory and expansion preset now has a name of its own (a test checks it): the first draft of Night Runner shared STARCOURT's Pulse Runner name.
- Levels balanced by measurement against the other expansions (Night Runner by its drum peaks); made without listening here, so your ears decide.

## 0.33.0

**NEGATIVE SPACE**, the ninth expansion: a bank of its own under EXPANSIONS (NS01 to NS15), named by you. Inspired by the modern dark-ambient film and TV scores of the 2010s: a fragile, intimate piano close up over cold, rumbling electronics, hypnotic analog pulses, glitchy broken textures and buzzing grit. Cold, tense, quietly melancholy. Our own sounds; no titles, composers, products or music.

- Keys, three pianos: **Soft Felt Piano** (the centrepiece: a soft, intimate felt piano heard up close; each note built harmonic by harmonic, the higher ones fading first as a real string's do, two strings a hair apart, the soft thump of felt, brighter and louder the harder you play, ringing on with the sustain pedal; the MOD WHEEL adds the keys' mechanical noise), **Broken Upright** (the same piano worn and out of tune, warbling like old tape, a distorted drone rising under each note), **Ghost Piano** (the same piano dissolving through a backwards reverb, a shimmer rising above it).
- Leads: **Static Lead** (a buzzing, lonely lead), **Hollow Signal** (a thin, filtered line: the MOD WHEEL sweeps it), **Glass Tone** (a pure, cold tone).
- Pads: **Low Hum** (a distorted drone rumbling under everything), **Cold Room** (a sterile, airy ambience), **Broken Signal** (a pad that keeps dropping out, in time with your song).
- Basses: **Grit Rumble** (a distorted, rumbling low end), **Pulse Bass** (a hypnotic sequenced bass in sixteenths with your song: hold a note), **Sub Throb** (a deep sub pulsing in half notes).
- Arps: **Hypnotic Motion** (a driving analog pulse building over four bars), **Glitch Pulse** (a chopped, stuttering pattern), **Steady Rows** (a metronomic beat from the drum sequencer under a rowing figure: hold notes above C3, the kit is on C2 to B2).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character: BUZZ, RESONANCE, FIFTH, GRIT, AIR, DROPOUT, SNAP, BODY, FELT, DRONE, SHIMMER, CRUSH, DRIVE. Each preset's notes say what they do.
- The pianos are made by synthesis, not recorded: an impression of a soft felted upright, not a copy of any instrument; checked by measurement from soft to hard across six octaves.
- **The preset browser:** banks A to E now sit on one row, and the expansion buttons shrink a little when there are many, so every expansion fits (the ninth had run off the bottom).
- Levels balanced by measurement against the other expansions; made without listening here, so your ears decide.

## 0.32.0

**DREAMSTATE**, the eighth expansion: a bank of its own under EXPANSIONS (DR01 to DR15), named by you. Inspired by the score of a 2010 film about dreams within dreams: a huge low brass blast, time slowed to a crawl and running at different speeds at once, relentless string ostinatos over heavy minor chords, a twangy reverb-soaked guitar, sparse piano and ticking clocks. Our own sounds; no title, characters, places, terms or music from the film.

- Leads: **Twang Guitar** (a twangy baritone-guitar-like pluck with a slow tremolo), **Fading Horn** (a soft, noble horn: press into the keys after playing for more brass), **Weightless** (a floating, breathy, hollow-voiced lead on a slow glide).
- Pads: **Folding City** (the MOD WHEEL folds the tone and pulls the upper layer down an octave), **Time Dilation** (chords swelling in backwards and sinking as they hold), **Heavy Chords** (dark, massive brass and strings).
- Basses: **Colossus Blast** (the huge, low brass blast: play one low note, C1 to C2, and let it ring; the MOD WHEEL adds grit), **Undertow** (a deep sub swelling up from nothing), **Ostinato Bass** (a driving low string bass in sixteenths with your song: hold a note).
- Keys: **Echo Piano** (LAST LIGHT's felt piano, darker, with long echoes), **Slowed Chorale** (keys that sag and warble like a tape slowing down), **Glass Memory** (cold, glassy bells).
- Arps: **Ostinato Strings** (relentless string sixteenths: push the MOD WHEEL to build), **Layers Deep** (three arpeggiators on one chord at three speeds at once), **Countdown** (a ticking clock from the drum sequencer under a pulsing figure: hold notes above C3, the kit is on C2 to B2).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character: TWANG, BRASS, VOICE, FOLD, DRAG, WEIGHT, SAG, SURGE, BITE, FELT, WARBLE, SHIMMER, DEPTH, TENSION. Each preset's notes say what they do.
- Levels balanced by measurement against the other expansions; made without listening here, so your ears decide.

## 0.31.0

**SIMULACRUM**, the seventh expansion: a bank of its own under EXPANSIONS (SM01 to SM15), named by you. Inspired by the score and soundtrack of a 1999 science-fiction film about a simulated world: a dark, atonal orchestra with brass in clashing keys, late-'90s industrial and big-beat basses and breaks, fast trance arpeggios, falling digital rain and time slowing down. Our own sounds; no title, characters, places, phrases or music from the film.

- Leads: **Rooftop Chase** (a screaming hard-synced lead through drive; the MOD WHEEL pushes the sync), **Phantom Line** (a cold, hollow lead with long echoes), **Slow Time** (every note sags as it holds, smeared through a backwards reverb; the MOD WHEEL makes it sag further).
- Pads: **Green Rain** (a tone falling over and over like digital rain, with random sparkles), **Brass Cluster** (a dark brass swell in two keys at once), **Endless Field** (a cold drone pumping slowly with your song).
- Basses: **Industrial Pulse** (a distorted bass chugging in sixteenths with your song: hold a note), **Breakbeat Bass** (a growling bass wobbling in eighths; the MOD WHEEL opens it up), **Mainframe Sub** (a deep sub that stutters like a failing machine).
- Keys: **Terminal** (cold digital chimes), **Glass Code** (brittle bit-crushed keys), **Warped Keys** (keys that melt out of shape on the MOD WHEEL).
- Arps: **System Breach** (a big-beat breakbeat from the drum sequencer under a driven bass riff: hold notes above C3, the kit is on C2 to B2), **Rain Cascade** (fast falling runs in the dark Phrygian scale: hold one key), **Firefight** (a relentless trance arpeggio building over four bars).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character: SCREAM, ECHO, SAG, GLITTER, CLASH, PUMP, DESTROY, GROWL, MALFUNCTION, RATIO, CRUSH, WARP, BITE. Each preset's notes say what they do.
- Levels balanced by measurement against the other expansions (System Breach by its drums' peaks); made without listening here, so your ears decide.

## 0.30.0

**LAST LIGHT**, the sixth expansion: a bank of its own under EXPANSIONS (LL01 to LL15), named by you. Inspired by the score of a film about space, time and leaving the people you love: sad, longing and melancholy, built round a soft, cinematic felt piano, with an enormous church organ, aching strings, a ticking clock and vast cold spaces. Our own sounds; no title, characters, places or music from the film.

- Keys: **Felt Memory** (the felt piano: a dark, rounded tone, the bright strike of the hammer dying away, the soft thump of felt on each note, brighter and louder the harder you play, ringing on with the sustain pedal; the MOD WHEEL adds the keys' mechanical noise), **Distant Felt** (the felt piano far away and long ago, on worn cassette in a vast hall), **Music Box Elegy** (a fragile music box: play it high).
- Leads: **Lonely Signal** (a pure, lonely lead with a slow glide and a late vibrato), **Cello Drift** (a slow, bowed cello-like lead: press into the keys after playing to bow harder), **Wordless Elegy** (a soft wordless voice).
- Pads: **Cathedral Organ** (an enormous pipe organ: the MOD WHEEL is its swell), **Horizon Strings** (slow, aching string swells: press into the keys to swell louder), **Fading Light** (a dark, breathing pad drifting slowly).
- Basses: **Organ Pedal** (deep organ pedal notes), **Gravity Well** (a sub rising slowly out of nothing), **Heartbeat** (a soft, low pulse: play two notes close together).
- Arps: **Ticking Clock** (a clock ticking from the drum sequencer under a quiet felt-piano figure: hold notes above C3, the kit is on C2 to B2), **Docking** (an organ figure climbing in sixteenths, building over four bars), **Tesseract** (a slow felt-piano arpeggio through a backwards reverb).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character: VIBRATO, BOW, VOWEL, STOPS, SWELL, DRIFT, RANKS, WARMTH, THUMP, FELT, WEAR, TINE. Each preset's notes say what they do.
- The felt piano is made by synthesis, not recorded: an impression of a felt piano, not a copy of one. Checked by measurement from soft to hard across six octaves; your ears decide.
- **0.29.0 was never published:** its Linux build passed every check, then crashed while closing (it didn't happen again in six runs here), so nothing went out. DATASTREAM was already inside 0.28.0; its notes are under 0.29.0 below, and everything in it is in this version.
- Levels balanced by measurement against the other expansions; made without listening here, so your ears decide.

## 0.29.0 (not published: see 0.30.0)

**DATASTREAM**, the fifth expansion: a bank of its own under EXPANSIONS (DS01 to DS15), named by you. Inspired by the scores of a digital-world science-fiction film and its sequel: precise glowing pulses locked to the beat, huge throbbing basses, orchestral-electronic swells, clean leads with long glides, a robotic voice and crisp digital bleeps. Cold and exact: no tape, no wobble. Our own sounds; no title, characters, places, terms or music from the films.

- Leads: **Light Trail** (a bright synced lead streaking between notes on a long glide), **Vector Voice** (a robotic, talk-box-like vowel lead, a little bit-crushed), **Data Burst** (a crisp square lead with a quick blip into each note).
- Pads: **Grid Horizon** (a vast, cold orchestral-synth swell), **Neon Cathedral** (an organ-like pad slowly opening its harmonics), **Digital Sky** (a shimmering pad cut into exact sixteenths, bouncing left and right).
- Basses: **Pulse Engine** (a huge bass throbbing in sixteenths with your song), **Bitfall Bass** (a digital bass that bit-crushes as you push the mod wheel), **Deep Lattice** (a deep, round octave bass).
- Keys: **Glass Console** (cold, glassy FM keys), **Brass Protocol** (a big orchestral-synth brass stab), **Bit Chimes** (crisp, bit-crushed digital bleeps).
- Arps: **Overclock** (a held chord driven up in tight sixteenths, its filter opening over four bars), **System Pulse** (whole chords chopped into sixteenths that bounce left and right), **Circuit Run** (fast random digital runs with ratchets and glitch stutters: hold one key).
- Macros: 1 BRIGHT, **2 SPACE** on every preset, 3 its character: GLOW, ROBOT, BLIP, SWELL, HARMONICS, SMOOTH, GROWL, CRUSH, WEIGHT, SHIMMER, BITE, RESONANCE. Each preset's notes say what they do.
- Also: a CPU test that failed on GitHub's slower Mac machines now allows for a machine's speed (it checks the same thing).
- Levels balanced by measurement against the other expansions; made without listening here, so your ears decide.

## 0.28.0

**SANDSEA**, the fourth expansion: a bank of its own under EXPANSIONS (SS01 to SS15), named by you. Inspired by the score of an epic desert science-fiction film: vast low drones, chant, keening voices with wide ornamented vibrato, enormous horn blasts, ritual percussion, wind and sand, melodies in the Phrygian dominant scale. Our own sounds; no title, characters, places or music from the film.

- Leads: **Desert Voice** (a keening vowel voice, scooping into each note; the MOD WHEEL widens its vibrato), **Sand Horn** (an enormous, gritty horn blast scooping up into pitch), **Bone Flute** (a breathy flute falling into each note like a grace note).
- Pads: **Deep Desert** (a vast low drone with a wind of noise gusting through it), **Throat Chant** (a low chant whose vowel sweeps slowly, so its overtones sing), **Mirage** (a shimmering, trembling pad swelling in through a backwards reverb).
- Basses: **Worm Sign** (a clean sub and a groaning saw rising out of the depths), **Quake Pulse** (a deep, dropping hit with a rumble), **Iron Ritual** (a distorted, metallic ostinato bass).
- Keys: **Copper Bells** (dark, harsh metal bells), **Stone Harp** (a plucked, oud-like string), **Ritual Gong** (a deep, clanging gong: play low).
- Arps: **War Drums** (tribal toms from the drum sequencer under a low pulse: hold notes above C3, the kit is on C2 to B2), **Sand Ripple** (fast random runs in the desert scale: hold one key), **Caravan** (a held chord walked in triplets, swaying left and right).
- Macros: 1 BRIGHT (or METAL, CLANG), **2 SPACE** on every preset, 3 its character: VOWEL, GRIT, BREATH, WIND, GROWL, SHIMMER, GROAN, DESTROY, TWANG, PULSE, SPARKLE, WARMTH. Each preset's notes say what they do.
- **The arp's randomizer has two new scales:** Phrygian Dominant and Double Harmonic (the desert scales: a flat second and a wide step up to the third). Your presets keep their scales.
- NEON RAIN's **Rain Cathedral** and **Ion Mist** sound exactly as before; only their descriptions are corrected. Their vowel and comb sweeps follow your song's beat (they move within each quarter note), not the slow seconds-long sweeps the notes described.
- Levels balanced by measurement against the other expansions; made without listening here, so your ears decide.

## 0.27.0

**STARCOURT**, the third expansion: a bank of its own under EXPANSIONS (SC01 to SC15), named by you. Inspired by the dark '80s analog-synth score of a supernatural mystery series: rising arpeggios over minor chords, detuned saws and squares, chorused polysynth pads, simple haunting leads, cassette and VHS wobble, big '80s drums. Our own sounds; no title, characters or music from the series.

- Leads: **Arcade Hero** (a bright square lead with its pulse width slowly sweeping), **Static Wail** (a gliding saw-and-sine lead into tape echo and a worn cassette), **Phantom Sync** (a thin, eerie sync lead whose sync drifts).
- Pads: **Basement Glow** (a warm, chorused '80s polysynth pad), **Other Side** (a dark pad whose second layer sinks out of tune over each chord, into a reverb that swells in backwards), **Cassette Choir** (a vowel pad on wobbling tape).
- Basses: **Night Drive Sub** (a round analog-style bass with its sub), **Pulse Runner** (a plucky bass for eighth-note lines), **Gated Octave** (a bass and its octave chopped into sixteenths, in time with your song).
- Keys: **VHS Piano** (an FM electric piano on worn video tape), **Lab Chimes** (cold, glassy FM chimes with ping-pong echoes), **Brass '84** (a punchy polysynth brass for chord stabs).
- Arps: **Dark Ostinato** (a minor chord climbing in steady eighths while the filter opens over four bars), **Gated Sequence** (whole chords in a chopped sixteenth rhythm: our imitation of a gated '80s reverb), **Midnight Run** (a sixteenth-note bass line with an '80s beat from the drum sequencer under it: hold chords above C3, the kit is on C2 to B2).
- Macros: 1 BRIGHT (or BARK, METAL), **2 SPACE** on every preset, 3 its character: WOBBLE (more tape: LO-FI and MOD), CHORUS, DRIFT, HOLLOW, HAUNT, GROWL, SNAP, SMOOTH, BITE, RESONANCE, SHIMMER, DRIVE. Each preset's notes say what they do.
- Levels balanced by measurement against the other expansions; made without listening here, so your ears decide.

## 0.26.0

**HELLFORGE**, the second expansion: a bank of its own under EXPANSIONS (HF01 to HF15). Inspired by modern industrial-metal game scores: synths played like down-tuned guitars through heavy distortion, sub drops, noise walls, a dark choir and syncopated chugs. Our own sounds; no game, composer or music from the scores.

- Leads: **Molten Riff** (a supersaw lead guitar through RABID, the pitch wheel set to dive an octave), **Warning Siren** (a sync lead winding up into each note), **Signal Scream** (a howling FM lead chopped by GLITCH).
- Pads: **Ash Choir** (a dark, distorted vowel choir), **Furnace Drone** (a wall of detuned saws and noise through ABYSS), **Corrupted Signal** (a stepping, bit-crushed wavetable through GLITCH and LO-FI, in time with your song).
- Basses: **Gut Chug** (a palm-muted chug: each hit dies inside its sixteenth), **Tectonic Drop** (a two-octave sub drop), **Grinder Bass** (a folding, chewing wavetable bass in sixteenths).
- Keys: **Rust Organ** (an overdriven organ through a rotary speaker), **Ritual Bells** (dark, clanging FM bells), **Iron Riff** (one finger plays a whole power chord with a clean sub under it: Gatling Pulse played by hand).
- Arps: **Gatling Pulse** (syncopated power-chord chugs: hold one low key), **Shrapnel Grid** (random ratcheted Phrygian fragments that never quite repeat), **War Machine** (a pumping root-octave-fifth sequence, ducked on every beat).
- Macros: 1 BRIGHT (or METAL), **2 SPACE** on every preset, 3 DESTROY (more drive) or SCREAM, HEAT, CORRUPT. Each preset's notes say what its macros and mod wheel do.
- Each uses at most one of the heavier drive models (none uses HALO), so they stay light on CPU. Levels are balanced by measurement against NEON RAIN; they were made without listening here, so your ears decide.

## 0.25.0

Expansions, and effects in the mod matrix.

- **Expansions: themed preset banks.** The preset browser has a new **EXPANSIONS** section under Bank E. Each expansion is a bank of its own with 15 presets: 3 leads, 3 pads, 3 basses, 3 keys and 3 arps. Their slots are named by two letters (NR01 to NR15).
  - You can't save into an expansion's bank: a copy you save goes into Bank A, so the built-in sounds stay as shipped.
  - < > step on from your banks into the expansions, and round.
- **NEON RAIN**, the first expansion: inspired by the 1982 neo-noir sci-fi film score. Polysynth brass, rain-soaked pads, smoky leads, glassy keys and slow neon pulses. Our own sounds; no names or music from the film.
  - Leads: Brass Requiem, Smoke And Reeds, Wire Ribbon. Pads: Rain Cathedral, Ion Mist, Memory Strings. Basses: Undercity Pulse, Distant Thunder, Velvet Noir. Keys: Electric Rain Piano, Glass Chimes, Night Shift Keys. Arps: Neon Grid, Raindrop Machine, Skyline Sequence.
  - Each uses a different mix of the synth: TRAVEL, FM from another oscillator, the PITCH envelope, shapers, aftertouch and mod-wheel routes, the arpeggiator (randomizer, chords), the step sequencer, insert buses, sends and the rack.
  - Every preset's three macros: 1 brightness, **2 SPACE** (more reverb, and more and longer echoes where the preset has them), 3 its character. Each preset's notes say what its macros and mod wheel do.
  - Levels are balanced by measurement (about the same loudness, peaks under -3 dBFS). They were made without listening here: your ears decide.
- **The mod matrix can now move the effects.** New destinations, under FX in the destination menu: DELAY's MIX, FEEDBACK, TONE and TO REVERB; REVERB's MIX, SIZE, TONE and PRE-DELAY; MOD's and MOD 2's MIX, DEPTH and RATE; LO-FI's MIX and AMOUNT. On the FX page's DELAY, REVERB, MOD and LO-FI pages their knobs show the ring of how far the matrix moves them, and take a source dragged onto them, as the synth's knobs do.
  - The effects are shared by every note, so the whole synth's sources move them: the macros and buttons, the mod wheel, pitch bend, aftertouch, breath, expression, the sustain pedal, and LFOs and envelopes not set to VOICE.
  - A note's own source (velocity, keytrack, random, the pitch envelope, the step sequencer) can't, and a route from one is greyed out in the matrix.
  - The moves are smoothed (30 ms, or the route's LAG if longer), so turning a macro doesn't step or click. Loading a preset sets them at once.
  - An effect that's off stays off: switch it on (with its MIX at 0 if you want the macro to bring it in).
  - Not yet: the insert buses' and the rack's settings.
- **Arpeggiator fix:** with the song playing, a chord written exactly on the beat waited for the next step, so the arp lost its first note. A key on a step, or up to 30 ms late (a quarter of a step if that's shorter), now plays that step.
- The browser's "n oscillators" line now counts oscillator 1 (it read one short for every preset).

## 0.24.0

**This release also brings everything in 0.23.0** (the sample editor: drawn envelopes, GAIN, MONO / POLY, DELAY with the anchor, LP / HP; preset A42 Tight Kit; and MIDI export on whole ticks). 0.23.0 was never published: its build passed every test, but GitHub's Linux plugin check (pluginval) crashed twice in a row while shutting down after its last test. It passed 12 out of 12 runs here under a debugger, and the crash is in pluginval's own Linux host, so it was released as part of this one instead. Its notes are below.

Stage 6, part 3 (second half): the rest of the drum section from Avenger's manual (page 67), as you chose. **Stage 6 is now complete.** Next come the bigger kit (your option 2: ride, a second crash, a china and more) and then the groove player.

- **Save and load kits and sequences:**
  - **KITS** on the kit's title line saves the whole kit as a file: every drum's settings, samples and drawn envelope, plus the kit's LEVEL and VELOCITY. It goes in the presets folder's "Drum Kits" folder. The same menu loads kits back and shows the folder.
  - **SEQUENCES** does the same for the drum sequencer: all four patterns and SPEED, TIMING, LENGTH and VEL AMOUNT, in "Drum Sequences".
  - **LINK** (on at first): saving or loading a kit takes the sequence of the same name with it, and the other way round, so a song's kit and beat travel together.
  - A load is one undo step. It leaves the synth, the effects and how the kit is played (DRUMS ON, ROOT) alone.
  - The buttons show the kit and sequence last loaded or saved.
  - SHUFFLE isn't saved with a sequence, because it's the synth's one swing setting, shared with the arpeggiator.
- **Several drums at once:**
  - CTRL-click rows to add or remove drums; SHIFT-click to choose a run of them. The chosen drums are outlined, and the one shown brightest.
  - A change to the bottom row (CHOKE, INSERT, TO DELAY, TO REVERB, MONO, DELAY, LP, HP) then goes to every chosen drum. For example, a high-pass on all the cymbals in one move.
  - Only changes you make on the page are copied. Automation from your song still moves just its own drum.
- **< >** next to LOAD SAMPLE load the previous or next audio file from the same folder, so you can run through a folder of kicks one by one.
  - Files go in number order ("Kick 2" before "Kick 10"), as sample packs number them.
  - Each new file plays whole.
  - Samples now remember which file they came from, in presets and projects too. Samples loaded before 0.24.0 don't know their file, so load them again once to use < >.
- **Playheads:** a line moves across each kit row's picture while that drum plays.
- **Tests:**
  - kits and sequences surviving saving and loading;
  - what a kit or sequence load changes and what it leaves alone;
  - LINK both ways;
  - undo;
  - names cleaned for the file system;
  - < > in number order, round and round;
  - several drums getting a page change but not automation;
  - the row playhead following a hit.

## 0.23.0

Stage 6, part 3 (first half): **THE SAMPLE EDITOR**, inspired by Avenger's drum sample editor (its manual's page 66). As you chose, it works on **every slot**, the drum synths as well as samples. Kit and sequence saving, choosing several drums at once, the < > sample arrows and a playhead in the rows come next, in 0.24.0.

- **Where:** the DRUMS page's DRUM view, for the chosen drum.
- **ENVELOPE:** the drum's level across its hit, drawn in a strip above the hit's picture, as on Avenger.
  - It works like the MOD page's envelopes: drag a point; drag a line up or down to curve it; double-click to add a point, or on a point to delete it.
  - Right-click for ready shapes: flat, back to front, upside down, fade in, fade out, a gate (cut halfway), a swell.
  - **A drum synth's** envelope spans its hit, from the start to where it has died away, lined up under the picture. Change DECAY and the hit gets longer or shorter, and the envelope stretches with it.
  - **A sample's** envelope spans START to END and follows the sample itself, so it stays put when you change the pitch. The file's picture draws the curve over that part.
  - New, it's flat at the top, which changes nothing (tested: exactly the same sound as before). Drawn down to nothing, the hit ends there.
- **GAIN:** more (or less) level than VOLUME gives, up to ±24 dB, for samples recorded very loud or very quiet.
- **MONO / POLY:** MONO cuts the drum's last hit short (a 2 ms fade) when a new one starts. POLY, the default, lets fast hits like a snare roll ring on over each other.
- **DELAY:**
  - **Right of centre:** the hit comes that much later, up to 1 s.
  - **Left of centre:** the **anchor**, a green line on the picture. The sequencer starts the hit that much early, so that point of it lands on the step: a clap's main slap after its flams, the peak of a swoosh.
  - **EXPORT MIDI** writes those drums early too, so played back from a track the anchor still lands on the beat.
  - As you chose, **keys you play** still start the hit at once (nothing can start in the past).
- **LP and HP:** a simple 6 dB-an-octave low-pass and high-pass per drum, as on Avenger: damp a kick, thin the hats.
  - My first version's HP came out 6 dB down at its own setting when set high (5 kHz), instead of the expected 3. The filters are now the kind that are exact at any setting (tested at 1 kHz and 5 kHz).
- **Not repeated from Avenger:** its fine PITCH (ours already goes to hundredths of a semitone) and SPIKE (our SNAP adds a spike above its middle on a sample).
- **New parameters** for each drum: GAIN, MONO, DELAY, LP, HP (automatable). The envelopes are kept in the preset, the project and undo, like the MOD page's.
- **A 0.22.0 fix:** a reversed drum synth had its VOLUME applied twice (the recording it plays backwards had VOLUME in it). Only noticeable with VOLUME away from 0 dB; fixed.
- **New preset A42, Tight Kit:**
  - A41's beat and bass;
  - the clap anchored 18 ms in, alongside the snare;
  - the open hat cut short by its envelope;
  - the kick MONO and a little darker;
  - the hats thinned with HP.
  - Our own settings, not tuned by ear.
- **Tests:**
  - what each control does to the sound:
    - the envelope's level a quarter of the way through a sample, and where it ends (also an octave up);
    - a drum synth's silent first half;
    - GAIN's level;
    - MONO against POLY on overlapping hits;
    - DELAY to the sample, and the anchor's start in the sequencer;
    - LP and HP at their settings and five times past them;
  - presets, the project and undo keeping the envelopes;
  - the page's envelope menu;
  - the strip lining up with the picture.
  - I can't listen here: how the shaped drums sound is for your ears.

## 0.22.0

Stage 6, part 2: **THE DRUM SEQUENCER**, beats for the kit, inspired by Avenger's drum sequencer (its manual's pages 81-82). Our own design and numbers where the manual gives none.

- **Where:** the DRUMS page's right-hand panel now switches between **DRUM** (the chosen drum, as before) and **SEQUENCER**.
- **The grid:** a row for each of the kit's 12 drums, level with the kit's rows on the left, so you read each row's drum off the kit list. Steps run left to right, lines brighter on each beat and each bar.
  - **PEN:** click to add a note; drag along a row to add several; drag notes to move them (hold ALT to place them off the grid, in 16ths of a step); right-click a note to remove it.
  - **SELECT:** drag a frame round notes to choose them. SHIFT adds to the choice; DELETE removes the chosen notes; CTRL+A chooses all.
  - Adding or clicking a note plays its drum and chooses it on the left.
  - A line shows where the beat is playing.
  - Notes are drawn brighter the harder they hit; rolls show as ticks; reversed notes are amber with a backwards arrow; a short gate is a line under the note.
- **Four patterns, A to D,** each up to 16 beats, polyphonic (any number of drums on one step).
- **The sequencer's settings:**
  - **TRIGGER SONG:** the chosen pattern plays while your song plays, in time with its bars (it counts from the song's start, so jumping around the song keeps it in place). Stopped, it's silent.
  - **TRIGGER KEY** (your choice): the four keys just below the kit play patterns A to D while held (with the kit on C2: G#1 A, A1 B, A#1 C, B1 D). The pattern starts from the key; pressing another while one is held switches to it on the same clock; letting go goes back to the one still held, or stops. Those four keys no longer play the synth while KEY is on. The kit's own 12 keys still play single drums over the beat.
  - **SPEED** (1/16 is four steps a beat) and **TIMING** (straight, triplet, dotted), **LENGTH** (1 to 16 beats).
  - **SHUFFLE:** every second step late, up to half a step. It's the synth's one SHUFFLE, shared with the arpeggiator and the step sequencer. The grid shows notes on their steps, not shuffled.
  - **VEL AMOUNT** (KEY): how much the key's velocity scales the pattern's.
- **Each note's own settings** (the chosen notes, in the CHOSEN NOTES box: drag up and down, double-click for the default):
  - **VELOCITY** (the level goes with its square: half is 12 dB down; drawn velocities always count, whatever the kit's VELOCITY knob says), **PAN** (from the drum's own place), **PITCH** (semitones from the drum's).
  - **GATE:** RING lets the hit ring out; lower cuts it after that much of a beat (an open hat closed early), with a 3 ms fade.
  - **ROLL:** the hit 2, 3 (a triplet), 4, 6 or 8 times within its step.
  - **REVERSE:** the hit backwards. **END SYNC** starts it early so that it ends where its step does (a swell into the beat). A SAMPLE slot plays its file (START to END) backwards; a drum synth plays a recording of its hit made when you set the note (with its pitch, decay, tone and snap in it; made again when you change them).
- **EXPORT MIDI** (your choice): click to save the pattern as a MIDI file, or drag the button onto a track. One time through, on the kit's keys, at the song's tempo. Velocities and rolls are written; pan, pitch and reverse can't be as MIDI notes, so they aren't.
- **Saved with the preset**, the project and undo, like the arpeggiator's patterns. New parameters: ON, TRIGGER, PATTERN, SPEED, TIMING, LENGTH, VEL AMOUNT (all automatable). Old presets load with the sequencer off and empty.
- **New preset A41, Beat And Bass:** A40's kit and bass with four beats: A straight, B busier (ghost snares, sixteenth hats), C half time, D a fill with a tom roll and a reversed cymbal into the next bar. Our own beats, not tuned by ear.
- **Tests** check that each hit lands on the exact sample it should (at 120 and 90 BPM, from any song position, with shuffle, rolls, triplet speed and end-synced reverses), each note setting's effect on the sound (level, left/right, pitch by counting the waveform, where a gate cuts, a reversed hit swelling to its end), the KEY keys starting, switching and stopping patterns without reaching the synth, the patterns kept by presets, the project and undo, the page's editing, and the MIDI file's notes and timing. I can't listen here: how the beats groove is for your ears.
- **Not built yet:** copying a pattern to another, and clearing one at a stroke (choose all, DELETE does it for now). Next: 6c, the sample editor (drawn envelope, filters, more).

## 0.21.1

**Choose one hit out of a sample** (your ask): many sample packs put several versions of a drum in one file, such as a kick played nine ways. Now you can play just the one you want.

- **The DRUM picture shows the whole file** when a slot is a SAMPLE, with each hit in it found and numbered (1, 2, 3...) along the top.
- **Click a hit to choose it:** the slot plays from that hit's start to just before the next one. The part that plays is bright, the rest dimmed, and the title says which it is ("0.45 s of 4.0 s (hit 4 of 9)"). You hear it straight away.
- **Fine-tune with the S (start) and E (end) handles:** drag them anywhere.
- **WHOLE FILE** plays the whole sample again.
- **A cut end fades over its last 3 ms**, so it doesn't click; the start isn't faded, so the hit's attack stays sharp (SNAP can still soften it).
- The choice is two new settings per slot, **SAMPLE START** and **SAMPLE END**. They're saved with the preset, can be undone, and can be automated. Loading a new file resets them to the whole file.
- **How the hits are found:** where the level jumps well above the quiet just before it, at least 40 ms after the last hit, moved back to where the hit's rise begins. In a test file of nine kicks, each was found within a millisecond of where it really starts.
  - Hits that run into each other with no gap may not all be found. Then drag S and E by hand.
- This is the START / END part of the sample editor (6c), brought forward because you need it now.

## 0.21.0

Stage 6, part 1: **THE DRUMS**, a kit of 12 drums on 12 keys, each one of our own drum synths or a sample you load, with its own page. (The drum sequencer is part 2, the sample editor part 3: the order you chose.)

- **How it plays:**
  - **DRUMS ON** (on the new **DRUMS** page) gives the 12 keys from **ROOT** (C2 to B2 at first) to the kit, in the General MIDI order Avenger uses: C the kick, C# the rim, D the snare, D# the clap, the toms on F, G and A, the hats on F#, G# and A#, the cymbal on B. Every other key still plays the synth, so a kit and a bass or lead share one preset.
  - The kit's keys are tinted on the on-screen keyboard while it's on.
  - **Off by default:** nothing changes until you switch it on. With the kit on, the notes outside its keys sound exactly as before (tested, sample for sample).
  - The hits are one-shots: letting go of the key doesn't cut them (as on a drum machine).
  - **VELOCITY** sets how much your playing changes the level: at 100%, a half-velocity hit is 12 dB softer; at 0 every hit is the same.
- **Each slot: one of nine drum synths, or SAMPLE.** The synths are our own designs from the classic analog ideas, not copies of any drum machine's circuit:
  - KICK: a sine whose pitch falls to 52 Hz, with a beater click;
  - SNARE: two drumhead tones and the wires (filtered noise);
  - CLAP: noise in three quick slaps and a ring;
  - CLOSED HAT, OPEN HAT and CYMBAL: six square waves at clashing pitches (that's the metal) plus noise, high-passed;
  - TOM: a falling sine with a stick click;
  - RIM: a short high and low ring;
  - COWBELL: two square waves through a band-pass.
- **Each slot's knobs:**
  - **PITCH** in semitones (and hundredths), **VOLUME**, **PAN**.
  - **DECAY:** how long it rings.
  - **TONE:** its brightness. On KICK and TOM, how far the pitch drops at the hit.
  - **SNAP:** its attack: the beater, the wires, the slaps, the sizzle.
  - **MUTE** and **SOLO** on each row.
  - **CHOKE:** a hit stops the other drums in its group. The closed and pedal hats stop the open hat, as on a real hi-hat.
- **SAMPLE: your own sounds.** Click LOAD SAMPLE, or drop a WAV, AIFF or FLAC on a drum's row or on its picture.
  - Mono or stereo, any sample rate, up to 30 seconds. It's saved inside the preset (and the project), so it plays on another computer.
  - DECAY, TONE and SNAP sit neutral at the middle on a sample. Below the middle, DECAY cuts it shorter, TONE darkens it and SNAP softens its attack; above, TONE thins it and SNAP adds a spike.
  - Loading into a drum-synth slot sets those to neutral, so the sample plays as recorded.
  - Samples from your Avenger folder work like any WAV. None ship with PR7-K: they're Vengeance's.
- **Routing, like the oscillators:** each drum goes straight into the mix (with its own TO DELAY and TO REVERB sends) or through insert bus A to D. So you could run the snare through a bus with a DRIVE and leave the kick clean.
- **The DRUMS page** (a new tab between ARP and FX):
  - **On the left, the KIT:** ON, ROOT, LEVEL, VELOCITY, then a row per drum: its key, its name, a picture of its hit, MUTE and SOLO. A row lights when its drum plays; click it to choose that drum and hear it.
  - **On the right, the chosen DRUM:** what it is, PLAY, LOAD SAMPLE and CLEAR, a large picture of its hit (drawn from a copy of it, as DRIVE's page does, with its length in seconds), its knobs, then CHOKE, INSERT and its sends.
- **A new factory preset: Kit And Bass** (A40). The kit on C2 to B2, a saw bass above. A small room on everything but the kick and the bass. Our own settings, not tuned by ear.
- **Levels and CPU, measured:** every drum peaks at about -3 dBFS at its default settings. The whole kit hit on every sixteenth note at 120 BPM costs about 4% of a core.
  - On the way I found and fixed a slowdown: fading envelopes were sinking into "denormal" numbers, which most processors handle very slowly. Fixed before release.
- I can't listen here. The drum synths' character is the part most worth your ears: tell me which drums sound wrong and how, and I'll reshape them.

## 0.20.2

**COSMONAUT runs at 4x instead of 8x: about 40% less CPU** (your choice, after 0.20.1).

- **What changed:** the drives work out their circuits at several times the sample rate, so the new high harmonics that distortion makes don't fold back down as harsh, out-of-tune tones (aliasing). COSMONAUT now works at 4 times the rate instead of 8. The other drives stay at 8x.
- **What I measured before changing it:** at 4x, COSMONAUT's aliasing was the same as at 8x (both under my measurement's floor, about 47 dB down), and its harmonics were within 0.2 dB. PR7-G's own COSMONAUT test (odd harmonics, the scooped mids) still passes.
- **Not sample-identical to 8x:** it's a different oversampling filter, so the output differs very slightly, mostly at the very top of the range. I can't listen here; if COSMONAUT sounds different to you, tell me and it goes back to 8x.
- **CPU on a bus (both sides), a played saw chord:** COSMONAUT 30% → about 17%. DRIVE's page shows the new figure.
- **Not changed:** RABID and ABYSS aliased audibly more at 4x, and CHIMERA moved one high harmonic by 6 dB, so they stay at 8x. HALO is as before (its fix is still offered).
- **PR7-G** still runs COSMONAUT at 8x; if you want this there too, it goes in PR7-G's own chat.

## 0.20.1

**Faster drives: RABID, CHIMERA, COSMONAUT and ABYSS use about a third less CPU, with exactly the same sound.**

- **What changed:** these four circuits model their diodes, and every step of the circuit solve needed two slow maths functions per diode (sinh and cosh). Both come from the same exponential, so now it's worked out once and both are made from it. Nothing else changed.
- **Same sound, checked:** I ran each changed drive against PR7-G's original code on saw chords, quiet and loud, at low and high DRIVE: the output is identical, sample for sample (not just close).
- **CPU on a bus (both sides, % of one core), a played saw chord, before and after:**

  | Model | 0.20.0 | 0.20.1 |
  |---|---|---|
  | RABID | 17% | 11% |
  | CHIMERA | 17% | 11% |
  | ABYSS | 22% | 18% |
  | COSMONAUT | 43% | 30% |
  | BANSHEE, SKYLINE | about 3.5% | unchanged |
  | HALO | about 33% | unchanged |

- **DRIVE's page now quotes these figures** (a played chord; 0.20.0's were measured on loud noise, the worst case). HALO's also says it can climb to about **120% on loud, bright sound**.
- **HALO: not sped up, and why.** HALO solves the whole fuzz circuit at once, and on loud, bright input (loud high notes, noise) its solver often doesn't settle within the steps it's given. That's what makes it heavy there, and it leaves a faint grainy residue in the sound, about 16 dB below the note on a loud high sine. Letting it take more steps fixes most of that but changes HALO's sound on those notes (and costs more), so it isn't a pure speed-up. I've left HALO exactly as PR7-G has it; fixing it is offered as a choice for both plugins, since it needs your ears.

## 0.20.0

Stage 7, part 4: **THE INSERTS**: four insert buses, A to D, each running PR7-G's **DRIVE** (all seven pedals), **GLITCH**, **MOD** and **LO-FI** on the oscillators you send to it. This is the insert-bus design you chose. It completes stage 7's effects.

- **How it works** (like a mixer's subgroups):
  - Each oscillator has an **INSERT**: NONE (straight into the mix, as before) or bus A, B, C or D. It's on the WAVE page (OUT, beside its sends) and on the new INSERTS view.
  - A bus runs DRIVE, then GLITCH, MOD and LO-FI on everything routed to it, after those oscillators' filter, AMP and pan. So a drive works on that oscillator's whole chord, as a pedal would.
  - Several oscillators can share a bus. Two oscillators on different buses never touch each other: a drive on A leaves B's oscillator clean (tested).
  - **One rule to remember:** a bus feeds DELAY and REVERB with its own two send knobs, after its effects, so the echoes carry the drive. An oscillator on a bus therefore uses the bus's sends, not its own. That's how a mixer's subgroup works, and it's the reason echoes of a distorted sound are distorted too.
  - This is also the way to put a MOD before the delay and reverb (the whole-sound MOD on the MOD / LO-FI view comes after them).
- **The FX page's new INSERTS tab** (first, as the sound goes through it):
  - **The bus strip:** BUS A to D, the oscillators on the bus shown (click 1 to 8 to route one here, again to take it off), and the bus's sends TO DELAY and TO REVERB.
  - **Four rack units for that bus:**
    - **DRIVE:** BANSHEE / RABID / COSMONAUT keys (the others on its page), DRIVE and LEVEL knobs, the bus's level on the ladder.
    - **GLITCH:** SEQUENCE / INTERFERE / KILLSWITCH keys, DENSITY and MIX knobs, LENGTH in big digits.
    - **MOD and LO-FI:** as on the MOD / LO-FI view.
  - **DRIVE's page:** its screen runs a separate copy of the chosen pedal, at your settings, on a 120 Hz sine. It shows one cycle in and out, the harmonics it adds, and what the model costs on a bus. The controls are the model's own: SKYLINE's ATTACK and GATE, ABYSS's EQ, and no TONE on HALO (the original has none).
  - **GLITCH's page:** a separate copy chops a plain riff of eighth notes at the song's tempo, so you see what it will do: the level in and out over two bars.
- **The drives and GLITCH are PR7-G 0.52.0's own code, unchanged.** I checked all seven drives against PR7-G's code, sample for sample: identical (one case differed by 0.000000007, rounding).
  - **Stereo:** they're mono pedals on PR7-G, so here each bus runs one per side. GLITCH's two copies make the same random choices, so both sides chop together. A sound on the left stays on the left through everything on a bus (tested).
- **One addition: a DC blocker after each bus's DRIVE** (5 Hz, only while the drive is on).
  - **Why:** RABID's circuit can leave its coupling capacitor charged after loud playing, a steady offset that never drains. On PR7-G the amp after the pedal blocks it; here nothing would. A real pedal's output capacitor does the same job.
  - It affects only what's below 5 Hz. Switched off, it fades out in about 50 ms (no click).
- **CPU, measured here: the drives are heavy, HALO very heavy.** PR7-G's drives solve their circuits part by part at 8 times the sample rate, and a stereo bus runs two. On a bus (both sides):

  | Model | CPU on a bus (% of one core) |
  |---|---|
  | SKYLINE | about 3% |
  | BANSHEE | about 4% |
  | CHIMERA | about 22% |
  | RABID | about 24% |
  | ABYSS | about 28% |
  | COSMONAUT | about 38% |
  | **HALO** | **about 124%: very heavy** |

  - **HALO** solves its whole germanium fuzz circuit at once, every sample. It already costs about 62% on PR7-G, which is mono; here, in stereo, it can be enough to cause dropouts.
  - DRIVE's page shows each model's cost, in red for HALO.
  - As you chose, they ship exactly as on PR7-G. A separate pass will speed up the drive circuits for PR7-G and PR7-K together. Faster solving can change the sound slightly on very loud, bright input (as I found with AEON in 0.19.0), so you'll want to check it by ear.
  - GLITCH, MOD and LO-FI are light.
  - **A bus nothing is routed to costs nothing**, whatever is switched on in it (tested).
- **Nothing changes until you use them:** with every oscillator on NONE, the sound is exactly as in 0.19.0, sample for sample. An oscillator on a bus with its effects off sounds as it did.
- **A new factory preset: Driven Lead** (A39).
  - A saw lead on bus A through RABID and a CARBONITE chorus, with ping-pong echoes taken after the drive.
  - A sine sub an octave down goes straight into the mix, so the low end stays clean.
  - Our own settings, not tuned by ear.
- **In the tests:**
  - PR7-G's own drive and GLITCH tests, run as they are (through a stand-in for PR7-G's plugin, using bus A's settings). Left out: GLITCH's expression pedal, its PRE / POST, and PR7-G's noise gate, none of which PR7-K has.
  - And, from the sound:
    - unused buses change nothing;
    - a drive works only on its oscillators (measured by what the drive adds, with a windowed measurement);
    - a bus's sends carry the driven sound into the echoes;
    - left stays left through everything on a bus;
    - buses are independent of each other;
    - no offset is left behind;
    - each model's CPU, the page, DRIVE's screen (BANSHEE's odd harmonics, HALO's strong 2nd), and the new preset.
- **Left in PR7-G,** as planned: the NAM amp and cab, the expression-pedal wah, the talk box and the XP-100 bends. Ask if you want any of them.
- **Next:** stage 6, the drums (you asked for them after stage 7). Then the drive speed-up pass, and in time the `.avxp2` import.
- I can't listen here. If a drive sounds different from PR7-G's, tell me which one and what you hear.

## 0.19.0

Stage 7, part 3: **MOD, MOD 2 and LO-FI**: PR7-G's modulation block (twice, as on PR7-G) and its lo-fi, ported from PR7-G 0.52.0 as you asked. The same code, with two kinds of change, both explained below: each side now kept on its own, and AEON made cheaper to run.

- **The FX page has a third view,** between SENDS and MASTER, in the order the sound goes through them:
  - **MOD / LO-FI:** three rack units, MOD, MOD 2 and LO-FI.
- **Where they sit:** after the sends come back, before the MASTER rack.
  - So the echoes and the reverb are chorused, panned or sent "through the phone" with everything else, as PR7-G's LO-FI does (it's last there, after the reverb).
  - The LIMITER still catches whatever comes out of them.
  - **One trade-off:** a tremolo or auto-pan chops the reverb's tail too. On PR7-G the modulation comes *before* the delay and reverb. Here the delay and reverb are sends taken from each oscillator inside the synth, so there's no single point "before" them to insert into. Effects on single oscillators, before their sends, are coming in 7d (inserts).
- **MOD and MOD 2: all twelve of PR7-G's types:**
  - Vortex, AEON, Hadron, Razor, Prism, Nebula, Rotary, Vibrato, Harmonic Tremolo, Auto-Pan, Doubler, Carbonite.
  - Each has PR7-G's own controls: SYNC to the song, AEON's MANUAL and RES, Hadron's SCRIPT / BLOCK, Razor's WAVE, the Vortex's EQ and STEREO.
  - MOD 2 starts on AEON, to pair with MOD's Vortex, as on PR7-G.
- **On each unit:**
  - **MOD and MOD 2:** < TYPE / TYPE > and SYNC keys, RATE and DEPTH knobs, the rate in big digits, the type's own settings beside it, and a ladder that rises and falls with the sweep.
  - **LO-FI:** TELEPHONE, CASSETTE and BITCRUSHER keys, AMOUNT and MIX knobs, the band it lets through (or the bitcrusher's bits and rate), and its output on the ladder.
- **On each page:**
  - **MOD:** the next two cycles of what the sweep moves, scrolling as it plays (the chip clock, the delay, the volume, the pan...), and only the controls the chosen type uses, as on PR7-G's panel.
  - **LO-FI:** its filters' response from 20 Hz to 20 kHz, or for the bitcrusher, a sine as it comes out, stepped and held.
- **Stereo, kept.** PR7-G's chain is mono up to its modulation, so a few types there summed or crossed the two sides. Here each side runs on its own:
  - **AEON:** one pedal per side, sweeping in step.
  - **Rotary:** one cabinet per side, the rotors turning together.
  - **Doubler:** each side's take comes from that side.
  - **LO-FI's TELEPHONE, AM RADIO and MEGAPHONE:** each side through its own speaker. They're mono on PR7-G, as real phones and radios are. NOISE is still one hiss, in the middle.
  - **The Vortex's STEREO** needed an adaptation of our own. On the pedal it puts the chorus alone on the left and the dry sound alone on the right. That needs a single input, and the synth's sound is already stereo. So here each side keeps its own sound, and its chorus is added on the left and taken away on the right. The chorus still sits between the speakers, and summed to mono it folds back to the dry sound.
  - **Checked against PR7-G's own code, sample for sample:** for a sound in the middle, every type comes out exactly as on PR7-G. A sound on the left stays exactly silent on the right.
- **AEON made cheaper (about 45% less CPU).**
  - PR7-G's AEON models the pedal's circuit part by part, at twice the sample rate. Two of them (one a side) cost about 17% of a core here.
  - I changed only how the maths is done, not what it computes: the op-amps' clipping curve with one power instead of two, the diode limiter's sinh and cosh from one shared exp, and three sums that never changed worked out once.
  - Now about 9%. The same sound: bit for bit identical to PR7-G's on saws, from quiet to full scale.
- **Something I found in PR7-G's AEON (and so here too):** very loud, very bright input becomes unpredictable inside its diode limiter.
  - For example, white noise above about a third of full scale, or a full-scale saw at 1760 Hz.
  - A one-in-ten-million change to the input then changes the output completely. It can come out much louder than it went in (white noise at 0.3 came out peaking near 1.9).
  - The cause: the limiter's solver takes four fixed steps per sample, and with that much treble they aren't enough to settle.
  - Ordinary playing levels are perfectly steady.
  - I haven't changed it, because it's PR7-G's sound and should change in both plugins together. If you hear crackle from AEON on hot, bright sounds, say so and I'll give the solver as many steps as it needs, in PR7-G and here.
- **CPU:** every type except AEON costs under 1% of a core, and LO-FI about 1%. A chord through AEON, the Rotary and the cassette together cost about 11%. Switched off, they cost nothing.
- **A new factory preset:** **Tape Keys** (A38): soft keys through the Vortex in STEREO, a slow auto-pan in half notes, and a lightly worn cassette, with a little room.
  - Its cassette has no hiss, because hiss would keep playing when no notes do. Turn NOISE up on the LO-FI page if you want it.
  - Our own settings, not tuned by ear.
- **Not yet:** the drives and glitch, and inserts on single oscillators (7d).
- **In the tests:**
  - PR7-G's own MOD, LO-FI and AEON tests, run as they are, through a stand-in for PR7-G's plugin. The Vortex's STEREO tests read the pedal's two outputs back from PR7-K's sides.
  - And, from the sound:
    - each side stays its own in every type;
    - switched off, they change nothing;
    - the reverb's tail goes through the phone;
    - the LIMITER holds LO-FI turned all the way up;
    - the sweep's marker moves at RATE;
    - the speakers' band edges are where the page draws them;
    - the bitcrusher holds for as long as drawn;
    - CPU, the page, and the new preset.
- I can't listen here. If a type sounds different from PR7-G's, tell me which and what you hear.

## 0.18.0

Stage 7, part 2: **THE SENDS**: PR7-G's **DELAY** and **REVERB**, fed by a send from each oscillator. They're PR7-G 0.52.0's own effects, the same code ported, as you asked.

- **The FX page now has two views,** with tabs along the top:
  - **SENDS:** DELAY and REVERB as two rack units, and a send matrix under them.
  - **MASTER:** 0.17.0's rack (ECHELON, DOMINION, CONSTRICTOR, LIMITER), unchanged.
- **How the sends work** (as on a mixing desk):
  - Each oscillator has a **DELAY** and a **REVERB** send: on its WAVE page (SENDS, beside NOTES), on the matrix, and on each effect's page.
  - The sound itself plays as before. A copy of each oscillator, at its send amount, goes to the effect, which gives back only its echoes or reverb, at its MIX.
  - So you can keep a sub dry and tight while the lead swims in reverb, or echo just one layer.
  - The sends are taken after the AMP and the note's pan, so each echo follows the note's envelope and position.
  - Sends are 100% by default, so switching DELAY or REVERB on just works on everything.
- **DELAY > REVERB** (on the matrix and the DELAY page): an amount of the echoes goes on into the reverb, so they get a space of their own.
- **The returns** come back in before the MASTER rack, so ECHELON, CONSTRICTOR and the LIMITER treat the echoes and reverb as part of the sound (the LIMITER holds them under its ceiling too).
- **DELAY**, PR7-G's, in all its types:
  - Digital, Analog, Tape, Mod, Reverse, Multi-Tap, Shimmer, Ducking;
  - TIME or SYNC to the song, FEEDBACK, TONE, MIX, PING-PONG, DUCK;
  - **on the unit:** PING-PONG, SYNC and DUCK keys, FEEDBACK and MIX knobs, the echo time in big digits, the return's level on the meter;
  - **its page** draws the echoes on a timeline: where each comes and how far down it is in dB, left above and right below with PING-PONG.
- **REVERB**, PR7-G's:
  - Classic, Room, Hall, Cathedral, Plate, Spring, Digital, Reverse;
  - SIZE, PRE-DELAY, TONE, MIX, DUCK;
  - **on the unit:** ROOM, HALL and PLATE keys to pick those quickly, SIZE and MIX, the tail's length in big digits;
  - **its page** draws the tail: silence for PRE-DELAY, then the fall to -60 dB.
- **Stereo:**
  - A sound on the left echoes on the left. It's exactly silent on the right; only TAPE's own hiss is on both sides.
  - PING-PONG takes what's sent as one sound and bounces it left, right, left, as on PR7-G. Otherwise a sound in the middle would echo on both sides at once and never bounce.
  - The reverb spreads what it's given across both sides, as a room does.
- **Nothing changes until you use them:**
  - With DELAY and REVERB off, the sound is exactly as in 0.17.0, sample for sample.
  - Switching one on while a note plays doesn't re-route the note (no click).
- **One trade-off:** oscillators with *different* send amounts each get their own copy of the filter and shapers. That's the same rule as routing them to different shapers. If the filter's DRIVE or a SHAPER is on, distorting them separately sounds a little different from distorting them together.
- **CPU:** DELAY and REVERB on a two-oscillator chord added about 2% of a core here; off, nothing.
- **A new factory preset:** **Echo Pluck** (A37). A pluck with ping-pong dotted-eighth echoes and a hall, over a sub that sends nothing. Our own settings, not tuned by ear.
- **Not yet:** inserts (effects on single oscillators: 7d), modulation and lo-fi (7c).
- **In the tests:**
  - PR7-G's own delay and reverb tests, run as they are (through a stand-in for PR7-G's plugin);
  - and, in the synth, from the sound:
    - an oscillator at 0% gets no echoes and changes nothing in them;
    - 50% gives echoes 6 dB down;
    - the reverb's tail comes only from what's sent, and DELAY > REVERB carries the echoes in;
    - left stays left (and PING-PONG bounces);
    - switching DELAY on mid-note, or different sends while it's off, changes nothing, sample for sample;
    - the LIMITER holds the echoes;
    - CPU;
    - the SENDS view, its pages and the echo timeline;
    - the new preset.
- I can't listen here. If an echo or a reverb sounds different from PR7-G's, tell me which type and what you hear.

## 0.17.0

Stage 7, part 1: **THE RACK**, a new **FX** page (in the header, after ARP) with PR7-E's four effects after the synth. They're PR7-E 0.18.1's, as you asked, with the same sound and controls: the same code, ported, not new versions.

- **The four rack units,** drawn as on PR7-E (after your rack photo), one to a row in the order the sound goes through them:
  - **ECHELON**, the EQ (inspired by FabFilter Pro-Q): up to 24 bands, dynamic bands, the analyser, and **EQ Match**.
  - **DOMINION**, the resonance smoother (inspired by soothe).
  - **CONSTRICTOR**, the compressor, with its up to six bands.
  - **The LIMITER** (inspired by Pro-L 2): its styles, look-ahead, true peak, loudness meters and dither, and 0.18.1's cleaner bass.
- **On each unit:** POWER, its meter, its screen, three keys, two knobs, and EDIT, which opens its big page in the rack's place (**BACK TO RACK** comes back).
- **Where they sit:** after the synth (its oscillators, filter, shapers and amp); then the preset's level and OUTPUT; then the LIMITER, last, so turning OUTPUT up drives into it as into a mastering limiter; then BYPASS.
- **In your presets:** the rack is saved with each preset (all four are off in the presets made before, so they sound as they did).
  - CONSTRICTOR and DOMINION also have **their own presets**, as on PR7-E. They're the same files (the "Comp Presets" and "Dominion Presets" folders in PR7-K's presets folder), so a preset saved in PR7-E can be copied over and loads the same.
  - Their parameters have PR7-E's names, so DAW automation reads the same too.
- **EQ Match on a synth:** "your sound" is the synth going into ECHELON.
  - **RECORD SYNTH** records the synth itself as the reference: play one preset as the reference, switch presets, and match the new one to it.
  - **LOAD FILE** matches to a song file.
- **Delay:** the LIMITER's look-ahead (1 ms by default, about 3.3 ms with TRUE PEAK) and CONSTRICTOR's LOOK-AHEAD (20 ms) make the sound a little late. Your DAW makes up for it on playback, but you'll feel it when playing live. Switched off, nothing is late. With the LIMITER's LOOKAHEAD at 0, TRUE PEAK and OVERSAMPLING off, it adds no delay either.
- **CPU, measured on a four-note saw chord** (one core of this machine; yours will differ):
  - the synth alone 0.7%;
  - ECHELON with six bands +0.4% (dynamic bands +0.9%);
  - CONSTRICTOR +0.3% (with three bands +1.3%);
  - the LIMITER with TRUE PEAK +3.9%;
  - **DOMINION +10.6%: by far the heaviest.** It listens with about 120 detectors and works out its cuts all the time; it costs the same on PR7-E. Switch it on where it's needed. Off, every unit costs nothing. If its weight bothers you, tell me: it could be made lighter, at some risk of changing its sound.
- **Changes from PR7-E:**
  - DOMINION's right-hand column is a little tighter, to fit PR7-K's page.
  - The text talks about the rack, presets and your sound instead of stomps, rigs and your track.
- **A new factory preset:** **Mastered Supersaw** (A36). Supersaw Lead with a low cut, a small dip in the boxy low mids, air on top, CONSTRICTOR's glue and the LIMITER at -1 dB. Our own settings, not tuned by ear.
- **Not yet:** inserts (effects on single oscillators) and sends, which come with PR7-G's delay and reverb (7b), modulation and lo-fi (7c), drives and glitch (7d).
- **In the tests:**
  - PR7-E's own tests for each effect (the limiter's came with it; the others were already here);
  - and, in the synth, from the sound:
    - off, the rack changes nothing, sample for sample;
    - ECHELON cuts exactly the band it's set to;
    - CONSTRICTOR turns a loud chord down;
    - the LIMITER holds its ceiling;
    - left and right stay apart (a note on one side leaves the other exactly silent through all four), and LINK turns both down together;
    - the delay reaches the DAW;
    - presets keep the rack;
    - the effects' own presets load;
    - the new preset;
    - CPU;
    - the FX page's layout, POWER and EDIT;
    - no parameter ID shared.
- I can't listen here. If something about the rack sounds or behaves differently from PR7-E, tell me.

## 0.16.0

Stage 5, part 2 (stage 5 complete): **chords, fixed note, the note randomizer, MIDI export, arp-note triggers and arps together**, modelled on Avenger's arpeggiator (the manual's pages 74-80; our own design and code).

**This release also brings 0.15.0's ARPEGGIATOR (stage 5a), which was never published:** the eight arps, patterns A to D, the ARP page and the preset Arp Over Pad (A34); see 0.15.0 below for all of it.
- **Why 0.15.0 never appeared:** the arp patterns made each preset's data 40 KB bigger. The plugin copies that data (for undo, loading presets, comparing) in places that use the thread's stack, which Windows keeps at 1 MB (Linux 8 MB). On Windows the tests crashed, so CI didn't publish it.
- **The fix:** the patterns now live on the heap (the copies are 7 KB again), and a new test keeps those sizes small. All the tests also pass here with Windows' stack size.

- **CHORDS mode** (MODE: CHORDS): each block plays a chord on the next key in turn. In this mode the pattern gets three more lanes:
  - **CHORD:** click for its type: Single, Major, Minor, Sus2, Sus4, Dim, Aug, 6, Min6, 7, Maj7, Min7, Min7b5, Dim7, Add9, 9, Maj9, Min9, Power, Octave.
  - **TONES:** a small switch per tone of the chord (root lit orange): click one to leave it out, e.g. a 7th without its fifth.
  - **SUB:** click for a note under the chord (an octave down); right-click for how far: 5, 7, 12, 17, 19 or 24 semitones.
  - The block's transposition moves the whole chord, so a pattern can be a progression.
- **NOTE SOURCE** (the new NOTES column, right of the pattern):
  - **KEYS:** as before.
  - **FIXED NOTE:** the arp takes the chord you hold and plays from its root alone (in your lowest key's octave), so the pattern's blocks make the tune. One key: that key.
  - **FIXED NOTE +:** the same, and the pattern follows the chord. A pattern is written as if over a minor chord; hold a major one and its minor third, sixth and seventh (+3, +8, +10, in any octave) become major (+4, +9, +11).
- **The RANDOMIZER:**
  - **ROLL** makes a new pattern from chances you set: which notes (by semitones above the key, kept to a **SCALE**: 14 to choose from, notes outside it dim), which octaves (-2 to +2), lengths (1 to 4 steps), and how likely a rest, a slide or a ratchet is, with the velocity's range.
  - **ROLLS** chooses what a roll changes (notes, octaves, lengths, velocities, rests): the rest of the pattern stays, so you can keep a rhythm and roll new notes, or the reverse.
  - Each roll has a **SEED** (shown, with - and +): the same seed and chances always make the same pattern. Undo brings the last pattern back.
  - **SYMMETRY:** the second half of a roll is the first half backwards.
  - **AUTODICE:** a new pattern rolled at the start of every time through, from the pattern written and the chances. It's the same every time you play the song (each pass has its own seed), so a bounce matches what you heard. Not with CHAIN.
- **EXPORT MIDI:** click to save the arp's notes as a MIDI file, or **drag the button onto a track** in your song.
  - It writes what the arp plays over the keys you hold on the on-screen keyboard (none held: C4), at the song's tempo.
  - Length: one time through (CHAIN: each pattern; AUTODICE: four passes).
  - It uses the same note code as the sound, so the file matches. A slide becomes notes that overlap, which most synths glide on.
- **ARP NOTES** trigger, for the LFOs and the mod envelopes (TRIGGER: ARP NOTES): restarted by every note any arp plays. One envelope shared by all notes; it lets go when no arp note sounds.
- **ARPS TOGETHER** (in the NOTES column): every arp counts its steps from the first key held, so an arp started later (a second key zone) comes in on the next step, in time with the others. Off: each arp runs from its own first key, as before.
- **Ours, where the manual says nothing:**
  - Avenger's chord types and scales aren't listed in the manual: these are the common ones.
  - How a held chord is recognised (each held note tried as the root against each chord type: most tones matched, fewest left over; the lowest key wins a tie).
  - The "+" mapping above.
  - The randomizer's chances, its defaults and how SYMMETRY mirrors.
  - That AUTODICE repeats the same passes each time you play.
- **Saved** with the preset and the project; undo covers chords, rolls and the randomizer. Presets from 0.15.0 load unchanged.
- **A new factory preset:** **Chord Stabs** (A35). Hold a chord or one key: a four-chord progression (i, VI, III, VII), stabbed in sixteenths with a sub note on each beat. Our own, not tuned by ear.
- In the tests, from the sound (each note's pitch and level) where it plays:
  - chord types, tones left out, the sub note, chords on each key in turn;
  - FIXED NOTE and "+" over major and minor chords, chord detection;
  - rolls kept to the scale and chances, the same seed giving the same pattern, ROLLS keeping what isn't rolled, SYMMETRY;
  - AUTODICE changing each pass and repeating;
  - an envelope restarted by each arp note;
  - ARPS TOGETHER's later arp waiting for the step;
  - the MIDI file's notes, times, lengths and velocities, and chords in it;
  - saving (and 0.15.0's patterns still reading);
  - the page's layout, lanes and ROLL with undo.

## 0.15.0

Stage 5, part 1: **the ARPEGGIATOR**, modelled on Avenger's (the manual's pages 74-80; our own design and code). A new **ARP** page (in the header, between MOD and PRESETS).

- **Eight arps.** Each oscillator chooses where its notes come from:
  - **OFF** (straight from the keys) or **ARP 1** to **8**. Set it on the oscillator's WAVE page (NOTES: ARP), or with the ARP page's **OSCILLATORS** buttons.
  - So one arp can run a pluck while another runs a bass, and an oscillator left off plays the chord itself.
- **Patterns A to D**, each up to 32 steps. Play the one chosen, or **CHAIN** them (those with blocks, one after another).
- **The blocks**, on a grid (steps across, transposition up and down; octaves orange, the rest amber):
  - click to put one, drag it up or down to transpose it (up to two octaves either way), drag its right edge to make it longer, right-click to delete it;
  - under each: its **VELOCITY** (drag), **SLIDE** (click: it glides into the next block with PLAY's GLIDE time, no new note) and **RATCHET** (click for up to 8 repeats in its length, right-click for fewer);
  - **-1 / +1** move every block a semitone; **START FROM** gives ready-made patterns (every step, eighths, octave bounce, up the scale, trance off-beats, slides, ratchets, long notes, clear).
- **The controls:**
  - **ON**, **MODE**, **SPEED** and **TIMING** (straight, triplet, dotted), **OCTAVES** (1 to 4), **GATE**, **LENGTH** (2 to 32 steps).
  - **MODE:** UP, DOWN, UP/DOWN, DOWN/UP, AS PLAYED, RANDOM, or POLY (every held note on each block).
  - **VELOCITY:** each note's own, the first key's, the last key's or the lowest key's, times each block's.
  - The key zone (**START NOTE** to **END NOTE**: keys outside it don't play the arp's oscillators).
  - **HUMANIZE** (each block late by up to a quarter step, at random), **STRUM** (POLY's notes one after another, over up to half a step).
  - **TRIGGER:** SONG keeps in time with your song while it plays (starting from the first key when it's stopped); KEY starts from the first key, always.
  - **SHUFFLE:** the synth's, shared with the step sequencer.
  - The sustain pedal keeps the arp going after you let go.
- **Timing:** the notes start and stop on their exact samples, wherever they fall in the DAW's audio blocks.
- **Ours, where the manual says nothing:**
  - The manual names UP, POLY and CHORDS: DOWN, UP/DOWN, DOWN/UP, AS PLAYED and RANDOM are the usual others.
  - How far HUMANIZE and STRUM go.
  - With the song playing, a first key waits for the next step (so the arp stays on the beat).
- **Not yet (stage 5b):**
  - CHORDS mode;
  - FIXED NOTE chord detection and the scale-aware "+" mode;
  - the note randomizer;
  - MIDI export;
  - envelopes and LFOs triggered by arp notes;
  - starting every arp together.
- Saved with the preset and the project; undo covers the patterns.
- **A new factory preset:** **Arp Over Pad** (A34). Hold a chord: a pluck arpeggiates through ARP 1 while a pad plays the chord. Our own, not tuned by ear.
- In the tests, from the sound (each note's pitch and level):
  - UP and GATE; every mode and OCTAVES;
  - rests, TRANSPOSE, long blocks, VELOCITY, RATCHET and SLIDE's glide;
  - the chosen pattern and CHAIN;
  - the velocity modes, SHUFFLE, HUMANIZE, STRUM;
  - routing (an unrouted oscillator playing the key under the arp), the arp switched off, the key zone, the sustain pedal;
  - SONG timing;
  - the plugin playing from a fresh start;
  - presets, projects and undo;
  - the page's layout and editing.

## 0.14.0

Stage 4, part 6 (stage 4 complete): **MIDI learn, controller to mod wheel, modulation on the knobs, drag and drop routing, and more for the matrix to move**, modelled on Avenger's (the manual's pages 106-108; our own design and code).

- **The strip above the keyboard:** every modulation source as a chip: LFO 1-4, ENV 1-8, MACRO 1-3 and the two buttons, the wheel, bend, velocity, aftertouch, keytrack, breath, expression, sustain, random, the pitch envelope, the step sequencer.
  - **Drag a chip onto a knob** to route that source to it (at +50%: change it in the MOD page's matrix).
- **The modulation ring:** a knob the matrix moves shows a white ring outside its own, from where the knob is to where the routes can take it.
  - **Right-click a knob:** its routes, each to remove, and MIDI learn.
- **MIDI learn:**
  - Right-click a knob, choose **MIDI learn**, and move a knob or fader on your controller: they're linked. Or drag the strip's **CC** chip (the last controller you moved) onto a knob.
  - A linked knob shows its controller's number beside it. The controller turns it straight away (in the same block of audio), and it's recorded as automation.
  - The links are kept with the project. **SETTINGS** lists them, each with a button to remove it, and has **SAVE AS DEFAULT** (every new PR7-K starts with them), **EXPORT**, **IMPORT** and **CLEAR ALL**.
  - One controller per knob and one knob per controller: linking either again moves the link.
- **Controller to mod wheel:** the **MW** switches on aftertouch, breath, expression and sustain make that controller drive the mod wheel too, so every sound made for the wheel follows your pedal or breath (whichever is higher, the wheel or the controller). They stay set when you change presets.
- **More the matrix can move:**
  - **SKEW** and **PINCH**. A note moved by the matrix works out its own curve every 16 samples, from curves worked out ahead at 65 settings of each. It's within a hair of the exact curve: the difference measured 66 to 72 dB down.
  - The shapers' **EQ** bands (gain, frequency, Q of both): a modulated note works out its own EQ.
  - The PITCH module's **RANGE** and **KEYTRACK**.
  - TRAVEL's **STRENGTH** and **PHASE**.
  - The step sequencer's **VOL MIX**.
  - Sounds that don't use them are exactly as before.
- **Not movable** (why, honestly):
  - **FFT SWEEP, FLIP and VSTRENGTH:** each new setting rebuilds the oscillator's tables in the background, which takes milliseconds. Too slow to follow an LFO, and per note it would need a table for every note.
  - **The LFOs', envelopes' and step sequencer's own speeds and shapes:** these are worked out before the routes, or once for every note. Moving them needs a deeper change, offered for later.
- **CPU:** SKEW moved by an LFO on every note of a 14-saw chord adds about half a point here.
- In the tests:
  - every new destination routed against the same value simply set (the same sound);
  - SKEW's curve accuracy;
  - the expression pedal driving the wheel's route;
  - MIDI learn (learning, turning, moving a link, the project, the default file, forgetting);
  - dropping a source and a controller on knobs;
  - the ring's range;
  - removing a route;
  - the strip's layout;
  - SETTINGS' list;
  - CPU.

## 0.13.0

Stage 4, part 5: **TRAVEL**, each oscillator's own movement through its wavetable, modelled on Avenger's wavetable envelope, pattern editor and strength editor (the manual's pages 48-51; our own design and code). It's on the oscillator panel's new **TRAVEL** page, separate for each of the 8 oscillators. It moves an oscillator playing a WAVETABLE.

- **The envelope (ENV):** a path through the table, drawn over one cycle: bottom is the first frame, top the last.
  - Double-click to add points, drag lines to curve them; its ends don't have to meet.
  - A new one is a straight ramp from the first frame to the last.
- **The pattern (SEQ):** up to 16 steps, each a SPEED period long, or two. Each plays the envelope its own way:
  - **x** (normal): the whole envelope, 1/2 to 16 times in the step;
  - **t** (trigger): its first part, restarted that many times;
  - **PP** (ping pong half): up its first half and back;
  - **R** (reverse) and **U** (flip, upside down);
  - **S** (silence): the oscillator goes quiet for the step (a millisecond's fade, so it doesn't click);
  - **H**, **M**, **L**: straight to the last, middle or first frame.
  - Drag a step up or down for more or fewer iterations, right-click it for its mode and length, double-click to make it one or two periods long.
- **The strengths (MOD):** under each step, a curve for how far that step moves (start, an optional middle point, end, bent).
  - Drag its left, middle or right; shift-drag bends it; double-click adds or removes the middle point.
- **The controls:**
  - **EDITORS** (Env, Env+Seq, Env+Seq+Mod).
  - **TRIGGER** (Voice, First Note, Last Note).
  - **MODE** (the envelope's: Loop, Ping Pong, One Shot) and **PATTERN** (the pattern's, the same three).
  - **STEPS**, **PHASE** (where it starts), **FLIP**.
  - **STRENGTH** (-100% to +100%), **VELOCITY** and its **VEL CURVE** (softer notes travel less far), **SMOOTH** (slows its moves, up to half a second).
  - **SPEED:** a note length at your song's tempo (1/4 to start) or **FREE** in cycles a second.
- **How it moves INDEX:** the frame played is INDEX + STRENGTH x the envelope (H, M and L set it outright). The INDEX knob, and the matrix routes on it, still work on top.
- **Our choices** (the manual doesn't give these):
  - an envelope cycle (or a step) is one SPEED period;
  - a step's iterations divide its own length;
  - PING PONG on the pattern runs it backwards, steps and all;
  - SMOOTH is up to 500 ms.
  - Avenger's fourth trigger, "global first", is the same as FIRST NOTE here: PR7-K's oscillators don't have key zones (yet).
- Saved with the preset and the project; unlike Avenger's editors, undo covers all three; its knobs can be automated.
- **A new factory preset:** **Talking Bass** (A33). The Vowels table moved by an eight-step pattern in sixteenths: sweeps, quick retriggers, a reverse, a jump to the last vowel, a silent step. Our own, not tuned by ear.
- **CPU:** TRAVEL is worked out every 16 samples per note: 16 travelling wavetable oscillators add about 0.3% of a core here.
- In the tests, measured on a two-frame table (a sine and its octave, so the octave's share is where in the table the note is):
  - the envelope's ramp and drawn shapes;
  - STRENGTH with INDEX, FLIP, PHASE, the three modes, FREE speed;
  - every step mode, iterations (2 and 1/2), SILENCE, a two-period step;
  - MOD's strength curve;
  - VELOCITY;
  - SMOOTH;
  - VOICE, FIRST NOTE and LAST NOTE with two notes;
  - the plugin moving a factory table from a fresh start;
  - presets, projects and undo;
  - the page's layout and its drags and clicks;
  - CPU.
- Not checked here: how it sounds (I can't listen).

## 0.12.0

Stage 4, part 4: **the STEP SEQUENCER** (the trance gate), modelled on Avenger's (our own design and code). It's on the MOD page, under the new **STEP** tab.

- **The steps:** up to 32 blocks.
  - Click a step to set its height; drag across to draw several.
  - Right-click a step to delete it (a rest).
  - Click the arrow under a step to link it to the next: the gate stays open from one to the other, moving to each one's height.
  - The line over the steps is the gate as it will play, so you can see GATE, CONTOUR, DECAY and SHUFFLE change it.
  - **START FROM:** an every-step trance gate, off-beats, on the beats, 3 + 3 + 2, accents, a rise, a fall, long and short (linked), a stereo ping-pong.
- **The controls:**
  - **ON:** the trance gate on the volume. **VOL MIX** sets how deep it cuts (100%: silence between steps).
  - **SPEED** (a step's length, 8 bars to 1/64) and **TIMING** (straight, triplet, dotted), at your song's tempo.
  - **TRIGGER:**
    - SONG (the default) stays in time with your song while it plays, and starts from your first note when it's stopped.
    - VOICE gives every note its own gate, from its start.
    - FIRST NOTE and LAST NOTE work as on the LFOs.
  - **MODE:** MONO, or STEREO with two rows (the top lane for the left channel, the bottom for the right), and **WIDTH** from fully apart to the same.
  - **LENGTH** (2 to 32 steps), **GATE** (how much of each step it's open), **CONTOUR** (soft edges, up to half a step), **DECAY** (each block fades out).
  - **DELAY** and **FADE IN** (from each note's start), and the key zone (**START NOTE** to **END NOTE**: other notes are left alone).
  - **SHUFFLE:** a swing. Every second step starts late, up to half a step (75% swing). It's the synth's shuffle: the arpeggiator and drums will share it when they come.
- **A matrix source, STEP SEQ** (0 to 1): the steps can move any knob, with or without the gate on the volume.
- **Our choices** (the manual doesn't give these numbers):
  - the edge is a raised-cosine fade;
  - CONTOUR 0 still has a 0.5 ms edge, so a hard gate doesn't click;
  - DECAY is the time to fall 60 dB;
  - SHUFFLE moves a step up to half a step;
  - in STEREO the matrix hears the two rows' average.
- Saved with the preset and the project; undo covers the steps; its knobs can be automated.
- **A new factory preset:** **Trance Gate** (A32). A wide supersaw cut in sixteenths: accents on the beats, off-beats bouncing left and right, a long linked step closing each bar, the filter opening on the loud steps. Our own, not tuned by ear.
- **CPU:** a step sequencer on each of 8 notes of a 14-saw chord adds well under 1% of a core here.
- **A test fix:** the shaper CPU test now keeps the fastest of three timings. A busy build machine had failed 0.11.0's first build on it; the limit is unchanged.
- In the tests:
  - the gate open for GATE of each step and shut for the rest;
  - heights, deleted and linked steps;
  - VOL MIX, LENGTH, CONTOUR's edge, DECAY's fall;
  - SHUFFLE's late steps, triplet and dotted steps;
  - STEREO's rows and WIDTH;
  - DELAY, FADE IN and the key zone;
  - SONG following the song's beats, and VOICE, FIRST NOTE and LAST NOTE with two notes;
  - STEP SEQ as a matrix source;
  - the plugin gating from a fresh start and keeping the steps through presets, projects and undo;
  - clicks in the editor;
  - the tab's layout;
  - CPU.
- Not checked here: how it sounds (I can't listen).

## 0.11.0

Stage 4, part 3: **the PITCH module**, modelled on Avenger's (our own design and code). It's on the MOD page, under the new **PITCH** tab.

- **The pitch envelope:** a multipoint envelope on the oscillators' pitch, drawn and edited like the modulation envelopes.
  - Its middle line is no change; **RANGE** (up to ±24 semitones) is how far its top and bottom go.
  - **ON**, **MODE** (one shot, sustain, sus jump, loop, ping pong and their +REL versions), **SPEED** (x0.1 to x10), **STEPS** (stepped, for arcade and FX sounds), **FLIP**.
  - **FOLLOWING IT:** which oscillators it moves (all to start with). In Avenger an oscillator is routed to a pitch module; here each has a switch.
  - It's also a source in the matrix, **PITCH ENV**, even with the envelope itself off.
  - **START FROM:** a drop, a kick-drum drop, a rise, a fall away, arpeggio steps, a wobble.
  - A new sound starts with a short drop (+6 semitones to none in 150 ms), switched off.
- **KEYTRACK:** how far the pitch follows the keys, from middle C. 100% is normal; 0% plays every key as middle C; -100% turns the keyboard upside down.
- **BEND LAG:** smooths the pitch wheel (and its automation), so jumps glide.
- **GLIDE CURVE:** the glide's path.
  - 0 is the straight glide as before (unchanged, sample for sample).
  - Below 0 it gets most of the way at once and settles (the manual's siren-like swoop); above 0 it starts slowly.
  - GLIDE's time and modes stay on the PLAY panel.
- **GLIDE SPLIT:** notes below this one never glide and aren't glided from, so a bass line can sit under a gliding lead (the manual's "split zone").
- Saved with the preset and the project; undo covers the pitch envelope's points; its knobs can be automated.
- **A new factory preset:** **Zap Bass** (A31): a fast two-octave pitch drop on each note. Our own, not tuned by ear.
- In the tests:
  - the pitch envelope moving a note by the right number of semitones over time;
  - RANGE, FLIP, STEPS;
  - an oscillator not following it staying put;
  - KEYTRACK at 0%, 50% and -100%;
  - BEND LAG easing a jump;
  - GLIDE CURVE's path, and CURVE 0 identical to before;
  - GLIDE SPLIT;
  - PITCH ENV as a matrix source;
  - the plugin playing the envelope from a fresh start and keeping it through presets, projects and undo;
  - the tab's layout.
- Not checked here: how it sounds (I can't listen).

## 0.10.0

Stage 4, part 2: **8 modulation envelopes** and **3 drawn LFO shapes**, modelled on Avenger's multipoint envelopes (our own design and code).

- **The MOD page has a new layout:** a wide **MODULATORS** panel on top with tabs **LFO 1-4**, **ENV 1-8** and **SHAPE 1-3**. The macros and the matrix are below it.
- **The multipoint editor** (for envelopes and shapes alike):
  - drag a point to move it;
  - drag a line up or down to curve it;
  - double-click empty space to add a point, or double-click a point to delete it;
  - right-click a point for its menu: delete it, put **SUSTAIN** (S) there, or put the **LOOP** points (L1 and L2) there.
  - Up to 64 points.
  - **START FROM** gives ready-made shapes to begin from:
    - envelopes: ADSR, ramps, pluck, swell, gate, wobble;
    - LFO shapes: triangle, ramps, square, steps, bounce, spike.
- **Modulation envelopes** (sources **ENV 1-8** in the matrix):
  - **MODE**, as the manual describes them:
    - **ONE SHOT:** once through, then it holds the last point.
    - **SUSTAIN:** holds at S while the key is down; let go earlier and it jumps to S.
    - **SUS JUMP:** the same, but it jumps to where the level matches, so nothing jumps.
    - **LOOP:** round L1 to L2 for ever.
    - **LOOP+REL:** loops while the key is down, then runs on to the end.
    - **PING PONG** and **PINGPONG+REL:** back and forth instead of round.
  - **TRIGGER:** **VOICE** (every note its own), **FIRST NOTE**, **LAST NOTE**. The arpeggiator and drum sequencer triggers come with those stages.
  - **SYNC:** times in beats of your song instead of seconds.
  - **SPEED** (x0.1 to x10), **STEPS** (stepped, for arcade and FX sounds), **FLIP**.
  - **OFFSET:** the level counted as zero; at 50% it pushes both ways, as the manual's "bipolar" offset does.
- **Drawn LFO shapes:** choose **CUSTOM 1, 2 or 3** as any LFO's WAVE to play the shape drawn on that SHAPE tab. Its two ends are kept level, so it loops without a click.
- Envelopes and shapes are **saved with the preset** (and the project), and undo covers them. Their knobs (MODE, SPEED...) can be automated.
- A modulator nothing uses costs nothing. The test chord with six routes per note still costs what it did.
- **Not yet:**
  - The manual says the drawn shapes can also drive the oscillators' vibrato and FM. Here they drive the LFOs only; vibrato and FM can follow in a later part.
  - Avenger's envelope "scale" control: a route's AMOUNT does that job here.
- **Two new factory presets:**
  - **Gated Pad** (A29): a looping, synced envelope chopping a pad in time.
  - **Swell Lead** (A30): a multipoint envelope on the filter, and a drawn bounce wobbling the pitch.
  - Our own, not tuned by ear.
- In the tests:
  - curved and straight lines;
  - every mode stepped through, with its release;
  - SPEED, SYNC, STEPS, FLIP, OFFSET;
  - an envelope moving a note's pitch exactly as drawn;
  - VOICE against LAST NOTE measured as level;
  - a drawn shape playing as an LFO;
  - presets, projects and undo;
  - the editor's mouse actions.
- Not checked here: how it sounds (I can't listen).

## 0.9.0

Stage 4, part 1: **modulation**: the MOD MATRIX, 4 LFOs, macros and your keyboard's controllers, modelled on Avenger's (our own design and code). Everything else in stage 4 (modulation envelopes, the pitch envelope, the step sequencer, the wavetable travel envelope, MIDI learn) plugs into this matrix.

- **The MOD page** (the new **MOD** tab beside SYNTH):
  - **LFO 1 to 4**, each with a picture of its wave:
    - **WAVE**: sine, triangle, saw up, saw down, square, S&H (a random level each cycle), smooth random.
    - **SYNC**: FREE (RATE in Hz) or locked to your song's tempo in note LENGTHS from 8 bars to 1/64, straight, **triplet** or **dotted**.
    - **TRIGGER**: **VOICE** (every note its own LFO, starting with the note), **GLOBAL** (one for all, always running, in step with the song's beats when it plays), **FIRST NOTE** (restarted when you play with no other key down), **LAST NOTE** (restarted by every note).
    - **PHASE**, **OFFSET** (moves the wave up or down and fits it in: at +100% it only ever pushes up), **DELAY**, **FADE IN**, **ONE SHOT** (once through, then it holds: an envelope).
  - **MACROS**: three knobs (double-click a name to rename it, saved with the preset) and two buttons. They're DAW parameters, so you can automate them or map hardware knobs to them.
  - An on-screen **MOD WHEEL**, for trying mod wheel routes without a keyboard.
  - **The MOD MATRIX**: up to 64 routes. Each: **ON**, a **SOURCE**, a **DESTINATION**, an **AMOUNT** (-100% to +100% of the knob's travel), **+/%**, **LAG** and **CURVE**.
    - Sources: the LFOs, macros and buttons, **mod wheel, pitch bend, velocity, aftertouch** (channel and per note), **keytrack, breath, expression, sustain pedal** and **random** (a new value for each note).
    - **+** adds the amount to the knob; **%** moves it by a share of the travel it has left, so it always reaches the end exactly at 100% (as the manual describes).
    - **LAG** slows the source down, smoothing jumps (a stepped LFO, a pressed button).
    - **CURVE** bends the source: below 0 it does most of its work early, above 0 late. (Avenger draws a curve per route; ours is a knob.)
    - Several routes to the same knob add up.
- **Every note is modulated on its own:** a velocity or a VOICE LFO moves each note's knobs separately, worked out every third of a millisecond.
- **Destinations:** every knob the synth follows live while a note plays (the oscillators' level, pan, pitch, shape, wavetable index, noise, sub, SYNC/FM/AM, unison, vibrato and the FFT editor's LP/HP; the filter; the shapers' DRIVE, MIX and OUTPUT; the AMP's volume and pan).
  - Not yet: SKEW and PINCH (their curve is built ahead of time), the FFT editor's SWEEP, FLIP and VSTRENGTH, and the shapers' EQ bands (both worked out ahead too), menus and switches. Those need changes inside the engine; I'll offer them later.
- **Saved with the preset** (and the project), and undo covers the matrix. Routes are stored by name, so presets keep working as later versions add sources and destinations.
- **CPU:** six routes on every note of the test chord cost about as much as none (4.7% against 5.1% of a core: within measuring noise). A sound with no routes runs exactly as before.
- **Three new factory presets:**
  - **Wobble Bass** (A26): a synced LFO on the filter.
  - **Mod Wheel Pad** (A27): the wheel opens the filter and adds vibrato.
  - **Velocity Keys** (A28): velocity on the filter and a shaper, and a TONE macro.
  - Our own, not tuned by ear.
- In the tests:
  - the LFOs measured as pitch: a 2 Hz square moving a note exactly 100 cents at the right times, synced and dotted rates, PHASE, OFFSET, DELAY, FADE IN, ONE SHOT;
  - the trigger modes measured as level;
  - every source;
  - + and %, CURVE, LAG, routes adding up;
  - velocity brightening the filter;
  - the plugin keeping the matrix through presets, projects and undo;
  - the page's layout and editing a route;
  - CPU.
- Not checked here: how it sounds (I can't listen).

## 0.8.0

Stage 3, part 5: **the SHAPER**, a distortion on each note, modelled on Avenger's shaper section (our own design and code). This finishes stage 3.

- **Where it is:** the FILTER panel now has tabs: **FILTER**, **SHP 1**, **SHP 2**, **SHP 3**, **SHP 4**. Each tab's light is on while that filter or shaper is.
- **What each shaper has**, as the manual describes it:
  - **MODEL** and **DRIVE** (up to +36 dB into the distortion; the level is kept about the same, so more drive means more grit, not just louder). **DRIVE VEL**: softer notes are driven less.
  - **Two EQ bands before the distortion** (GAIN, FREQ, Q each; at 0 dB a band is off): boost a band and that part distorts first.
  - **EQ SPLIT**: band 1 on the left channel only and band 2 on the right only, so the two sides distort differently and the sound gets much wider.
  - **PLACE**: before the filter (the filter then tames the buzz) or after it. Up to 4 shapers, run in number order on each side of the filter.
  - **OSCILLATORS 1 to 8**: which oscillators go through it. All of them are ticked to start with, so switching a shaper on is heard at once; untick the ones you want clean.
- **Ours, not Avenger's:**
  - **The 10 models** (the manual doesn't list Avenger's): SOFT, SMOOTH and CUBIC round off gently; HARD clips flat; TUBE and DIODE lean to one side (warmer, with even harmonics); SINE FOLD and TRI FOLD fold the wave back on itself (metallic); RECTIFY flips the bottom half up (an octave up); HALF RECT cuts it off.
  - **MIX** (below 100%: the clean and distorted sound together) and **OUTPUT**. The manual lists neither.
- **Each note is distorted on its own** (as on Avenger, "on an oscillator level"), so chords stay clean of the harsh tones you get distorting several notes at once. Oscillators on different routes each get their own filter and shapers, so a clean oscillator stays clean beside a distorted one (tested: playing both is exactly each alone, added).
- **Clean at the top:** distortion makes harmonics far above the note, and those past the top of what the sample rate can hold fold back down as harsh, out-of-tune tones. Each shaper runs at twice the sample rate, with the anti-aliasing the filter's DRIVE uses on top. Measured on a hard-driven 1.76 kHz tone: folded noise about -55 to -65 dB, against -26 to -32 dB for a plain distortion.
  - Two costs: half a millisecond of delay through a shaper (its MIX's clean sound is delayed to match), and a very gentle top roll-off from the anti-aliasing (-0.5 dB at 10 kHz, -1.6 dB at 18 kHz).
  - The FOLD models, pushed hard on high notes, still make more harmonics than this can hold. Use them lower down, or with less DRIVE, for the cleanest result.
- **CPU:** a shaper costs nothing while it's off. On the test chord (8 notes, 2 supersaws of 7 copies, through the filter): 4.5% of a core with none, 9.3% with one, 15% with two.
- **Your old sounds are untouched:** with no shaper on, the synth's output is identical, sample for sample, to 0.7.0 (checked on a test sound with three oscillators, a driven filter and chords).
- **Changing MODEL while a note plays** switches the curve at once, so it can click, like switching any waveform.
- **Two new factory presets:** Tube Bass (A24) and Wide Fold Lead (A25, EQ SPLIT). Our own, not tuned by ear.
- In the tests: every model's maths, harmonics (SOFT odd only, TUBE and DIODE adding even ones, RECTIFY an octave up), level kept as DRIVE rises, aliasing against a plain distortion for every model, the oversampling's flatness and delay, the EQ bands against their picture, SPLIT, routing, separate paths, PLACE, DRIVE VEL, MIX, OUTPUT, saving, the page's layout and CPU. Not checked here: how it sounds (I can't listen).

## 0.7.0

Stage 3, part 4b: **FREEFORM** (draw a wave by hand) and **the resampler** (choosing how a long sample is cut into wavetable frames), both modelled on Avenger's (our own design and code).

- **FREEFORM**, a new WAVE choice, and **the DRAW page** (each oscillator: WAVE, SHAPE, FM, FFT, **DRAW**, VOICES): drag to draw one cycle of the wave; hold **SHIFT** for a straight line. Drawing switches the oscillator to FREEFORM so you hear it straight away. The thin line is what you drew, the bright one what plays.
  - **START**: begin again from a flat line (Avenger's start), or a sine, saw, square or triangle to change.
  - **SMOOTH A** and **SMOOTH B** round off sharp corners (sharp corners are what sound bright and buzzy): A before **SPLIT**, B after it (SPLIT at 100%: A everywhere).
  - **EDGE** joins the end of the cycle back to its start, so there's no jump (a click on every cycle, heard as buzz). Its modes: **ADAPT** moves both ends to meet in the middle, **ZERO** fades them to silence, **SYMMETRY** blends the wave with its mirror image.
  - **NORMALIZE** makes it as loud as it can go without clipping (off: as loud as drawn).
  - **WAV** exports the cycle as it plays (2048 samples), to load anywhere as a wavetable.
  - It plays as cleanly as the basic waves (no aliasing, high up included), and everything else works on it: unison, SHAPE, SYNC / FM / AM, and the FFT editor.
  - Saved with the preset and the project; undo takes back each stroke.
  - **What's ours, not Avenger's:** the smoothing and edge maths are our own guesses at what the manual describes; the controls are as the manual lists them.
- **The resampler:** loading a WAV, AIFF or FLAC that's longer than one cycle (from TABLE's menu or by dropping it on the oscillator) now asks how to cut it:
  - **Frame length:** as the file says (if another wavetable synth marked it), or 256 to 4096 samples, or **the note it's played at** (each frame is one cycle of that note: load a sustained C2 and set C2, and every frame is exactly one cycle, so it plays in tune with no buzz).
  - **Which frames:** one after another, every 2nd / 3rd / 4th, or 16 to 256 spread evenly through the file.
  - **Start** and **End**: just a part of the file.
  - Files of one cycle or less load straight away, as before.
- **A new factory preset:** Drawn Growl (A23), a bass from a hand-drawn cycle. Our own, not tuned by ear.
- In the tests: a drawn sine plays one harmonic and a drawn saw a saw's; SMOOTH takes the high harmonics away, and SPLIT gives each half its own; each EDGE mode closes the jump; saving; no aliasing at C7; the plugin playing the drawing and keeping it through presets, projects and undo; the FFT editor on a drawing; the resampler cutting by note, every nth, spread and part; drawing on the page with the mouse. Not checked here: how it sounds (I can't listen), and the resampler's question window itself (it opens on its own, so the screenshots can't show it).

## 0.6.0

Stage 3, part 4a: **the FFT editor**, modelled on Avenger's (our own design and code). **This release also brings 0.5.0's SYNC, FM and AM** (the FM page: see 0.5.0's notes in the changelog), which didn't publish on its own: one older test, which hears random choices, failed on Windows. The tests now make the same random choices every time, on every computer.

- **The FFT page** (each oscillator: WAVE, SHAPE, FM, **FFT**, VOICES): draw how loud each of the wave's first 256 harmonics is, from +24 dB down to gone. Drag to draw; hold **SHIFT** for a straight line from where you started. The wave's own harmonics are drawn faintly behind, and what comes out over them.
  - Three views, as Avenger's: **FREE** (each octave the same width), **HARMONIC** (the first 24 harmonics as wide bars, each named by the interval it makes with the note: root, octave, fifth, third, b7..., and one bar for all the rest; our layout) and **BIN** (all 256 side by side).
  - **RANDOM** (every harmonic a random level, leaning towards cuts so it stays usable) and **RESET**.
  - **SWEEP** moves your drawing up or down the harmonics (up to 4 octaves): phasing and morphing sounds. **FLIP** turns it upside down (cuts become boosts).
  - **LP** and **HP** take away the harmonics above (or below) a given one: a filter that follows the note exactly, and **SLOPE** makes it a gentle shelf instead of a steep cut.
  - **VSPEED** and **VSTRENGTH** make the harmonics pulse, each swelling and fading at its own depth, some against the others.
  - Works on every wave, wavetables included (INDEX still moves through the edited frames). The wave picture shows the edited wave, marked (FFT).
- **How it stays light:**
  - Your drawing, SWEEP and FLIP are baked into a rebuilt copy of the wave, in the background (about a millisecond for a basic wave, a few dozen for a wavetable), so playing an edited wave costs what playing a wavetable does: the test chord, 3.3% of a core against 3.1% plain.
  - LP, HP and SLOPE are live instead, so they can move as fast as you like (and take an envelope once stage 4's modulation is in). The test chord costs about 15% with LP on.
  - VSPEED's pulsing swings between two rebuilt copies: about 24% with LP, HP and pulsing all on.
  - One thing to know: while you drag SWEEP, the rebuilt wave follows a few times a second rather than instantly (a wavetable's rebuild takes longest). LP and HP have no such lag.
- **Your curves are saved** with the preset and the project, and undo covers them.
- **Two new factory presets:** Hollow Reed (A21: every even harmonic drawn out) and Spectral Shimmer (A22: pulsing harmonics). Our own, not tuned by ear.
- In the tests: the curve, SWEEP and FLIP exact; a cut harmonic gone and the rest where they were; a flat curve the same as the plain wave (every harmonic within 0.05 dB); LP and HP cutting at the right harmonics, blending between, SLOPE's shelf, and no aliasing; the pulsing swinging as set; the plugin building in the background and keeping curves through presets, projects and undo; the page's layout; CPU. Not checked here: how it sounds (I can't listen).

## 0.5.0

(Not published on its own: its Windows build failed one older test. Everything here is in 0.6.0.)

Stage 3, part 3: **SYNC, FM and AM**, modelled on Avenger's (our own design and code). Each oscillator has a new **FM** page, between **SHAPE** and **VOICES**.

- **SYNC**: a copy of the wave restarted at the start of every cycle of the note, running faster, from the note's own pitch up to 5 octaves above: the classic tearing "sync" sound. **MODE**: **PURE** (the copy alone), **ADD** (the copy with the wave), and **PURE WIN** / **ADD WIN**, which fade the copy in and out each cycle so there's no hard edge (smoother, more vocal).
- **SYNC ENVELOPE**: SYNC's own little envelope, as on Avenger: **ENV** (how far it sweeps SYNC up, or down, from each note's start), **LENGTH** (50 ms to 3.5 s to fall back) and **CURVE**.
- **FM**: the wave read ahead and behind as a modulator swings (phase modulation, as most "FM" synths do it). **SOURCE**: a sine, triangle, saw or square at **RATE** times the note, or **another oscillator**, which then only modulates and isn't heard itself (as on Avenger, "real FM").
- **AM**: the wave's level swung by the modulator (**SHAPE**); at 100% it's ring modulation.
- **The modulator: RATE** and **GRID**, Avenger's eight: **FREE** (1/16 to 16 times the note), **HARMONIC** (octaves and fifths), **SEMI** (semitones), **OCTAVE**, and octaves with a minor third, major third, fourth or fifth. The grids step from 1 (the middle of the knob) up to 16 times the note. **LOCK** starts the modulator in step with the wave on every note, so every note sounds the same; off, each starts anywhere. The display under the controls shows the result and the ratio RATE has landed on.
- **Kept clean without oversampling**, measured on a saw at A5 (880 Hz), loudest between its harmonics: SYNC -51 dB (WIN -93), FM -119 dB, AM -116 dB, where doing it the plain way leaves -13 to -33 dB. Avenger's FM has four quality modes, the cleanest of which oversamples (its manual warns it's CPU-heavy); ours has one, which reads band-limited tables chosen with room for FM's side tones. Its table-based FM modes aren't built: tell me if you hear FM getting harsh up high and I'll add one.
- **CPU:** nothing at all until you turn SYNC, FM or AM up. The supersaw test chord (2 oscillators x 7 copies x 8 keys) took about 3% of one core plain, 9% with SYNC, 12% with AM, 14% with FM and 19% with all three. Your computer will differ.
- **Three new factory presets:** Sync Sweep (A18), FM Bell (A19, oscillator 2 modulating oscillator 1) and Ring Mod (A20). Our own, not tuned by ear.
- In the tests: SYNC at twice the note is exactly the octave, at 2.5 times only the note's harmonics; ADD half and half; the WIN modes never jump; the envelope sweeps from far up back to the note; FM matches the textbook formula sample for sample (within 1%), from a shape and from another oscillator (which isn't heard); AM at full gives just the sum and difference tones, at half the right mix; the grids' ratios; LOCK makes every note the same; how much less folds back than the plain way; CPU; the FM page's layout. Not checked here: how it sounds (I can't listen).

## 0.4.0

Stage 3, part 2: **SHAPE**, bending each oscillator's wave while it plays, modelled on Avenger's shape controls (our own design and code: the manual says what each does, not how). Each oscillator has a new **SHAPE** page, between **WAVE** and **VOICES**.

- **BEND: SKEW** squashes one side of the wave and stretches the other (right of centre the left side, left of centre the right side): the rounded, buzzy sound of a "phase distortion" synth. **PINCH** zooms into the middle of the wave (right, smoothing it there and sharpening its edges) or out of it (left).
- **FORMANT** plays the wave faster with a gap after it (left, up to 8x), which gives a fixed, vocal resonance that stays put as you play up and down; or slower (right, up to 4x), so only its start fits in each cycle. Its **MODE** says what happens where the wave is cut: **CUT** leaves a hard edge (a buzzy click, as the manual describes Avenger's), **FADE** fades its end to silence, **X-FADE** fades both ends, **X-FADE 2** fades its end into its start, so it never jumps. The default is FADE (our choice).
- **CRUSH** for lo-fi: **RATE** holds the wave in steps (down to 3 a cycle), **BIT** rounds it to fewer levels (down to a square), **BOTH** does both.
- Works on every wave, wavetables included, and on unison copies and chord notes. The oscillator's wave picture shows the bent wave; the SHAPE page also shows the plain and the bent wave side by side, large.
- **Our names:** SKEW and PINCH are what Avenger calls XCITE side and mid (that's its own name, so not used here). Tell me if you'd like other names. The four FORMANT modes are our reading of the manual's short description.
- **Kept clean without oversampling** (what makes Avenger heavy):
  - SKEW and PINCH bend the wave with curves that are smooth all the way round, and read a table level with room for how much faster they play parts of the wave. Measured at A5 (880 Hz) on a saw: nothing between its harmonics above -116 dB, where doing it the plain way leaves -22 to -32 dB.
  - FORMANT and CRUSH make hard edges on purpose. Each edge is smoothed over the two samples around it, the way a clean edge would be. At A5: FORMANT 43 to 61 dB down, CRUSH 43 to 46 dB down, 8 to 18 dB cleaner than the plain way. Near the top of the keyboard (E7) hard edges are harder to keep clean (a cut saw is about 35 dB down there): CUT's clicks are the sound it's meant to make, and the fades are cleaner.
- **CPU:** nothing at all until you turn a SHAPE control. A supersaw chord of 2 oscillators x 7 copies x 8 keys took about 3% of one core plain, 6 to 10% with one SHAPE control on, and about 19% with SKEW, FORMANT and CRUSH all on at once. Your computer will differ.
- **Three new factory presets:** Formant Vox (A15), Phase Bass (A16) and Crushed Keys (A17). Our own, not tuned by ear.
- In the tests: SKEW moves a sine's peak (earlier or later), PINCH makes its middle gentler or steeper, FORMANT plays it faster with silence after (or only its first part), each MODE's edges (only CUT jumps), RATE's 3 steps a cycle, BIT's square (its 3rd harmonic at a third, no 2nd), the curves smooth and never running backwards, how much less folds back than the plain way, wavetables and copies bent too, and the CPU figures. Not checked here: how it sounds (I can't listen).

## 0.3.0

Stage 3, part 1: **wavetables**, modelled on Avenger's wavetable oscillator (our own design and code). Stage 3 comes in parts, each its own release: next are the wave-bending controls, then sync, FM and AM, then the FFT and freeform editors, then the shaper.

- **WAVETABLE**, a new choice in each oscillator's **WAVE** menu: a row of up to 256 single-cycle waves ("frames") that **INDEX** moves through, morphing the sound as it goes.
  - **TABLE** (in place of WIDTH) chooses the table: one of ours, or **Load WAV file...** to load your own (WAV, AIFF or FLAC). You can also drop a file straight onto the oscillator panel.
  - **INDEX**: where in the table. **MODE**: **SMOOTH** blends between neighbouring frames, **STEPPY** jumps from one to the next. **FRAMES**: how many of the table's frames INDEX moves through, spread evenly from the first to the last (fewer makes broader, simpler steps).
  - The wave picture shows the frames stacked from front to back, with the one INDEX points at drawn bright, and the table's name and frame number above.
  - Unison, vibrato, the chorder, the sub and noise all work with wavetables as with the other waves.
- **Ten factory tables of our own** (generated, not copied from anyone's): Analog Morph (sine, triangle, saw, square), Pulse Width, Harmonic Rise (harmonics added one by one), Vowels (a sung "ah eh ee oh oo"), Hard Sync, Wavefold, FM Sweep, Bit Steps, Comb Sweep and Organ Stack.
- **Your own tables:** a file written by another wavetable synth is cut into frames the size it says (the "clm" marker most of them write). Otherwise a short file (4,096 samples or fewer) is taken as a single cycle, and anything longer is cut into frames of 2,048 samples, the size most wavetable packs use. Files with more than 256 frames keep 256, spread through the file. Avenger's own tables can't ship with PR7-K, but any WAV wavetable you have will load.
- **Saved with your sound:** a loaded table is kept inside the preset and the DAW project (16-bit, compressed: a 64-frame table adds about 330 KB), so it plays the same on another computer even without the file. Undo covers loading and changing tables too.
- **No aliasing, no extra CPU for it:** every frame is stored at the same 28 brightness levels as the saw, so a wavetable stays clean right up the keyboard with no oversampling (measured: a very bright frame at 2.6 kHz is 96 dB down between its harmonics). A wavetable supersaw chord (2 oscillators x 7 copies x 8 keys) took about 3.4% of one core in the tests, against 2.5% for the same chord with saws. Tables take memory instead: about 16 MB for a 64-frame table (64 MB for a full 256-frame one), built when you choose it.
- **Three new factory presets:** Vowel Pad (A12), Sync Lead (A13) and Bit Bass (A14). Our own, not tuned by ear. Nothing moves INDEX by itself yet: modulation is stage 4, so for now INDEX is a knob to set or automate from your DAW.
- Not in this part: Avenger's wavetable envelope and step pattern that move INDEX (they're modulators, so they come with stage 4), and its resampler options for loading long samples at a chosen root note (with the FFT editor, part 4).
- In the tests: each frame keeps its own harmonics, each level only the harmonics it's allowed, INDEX picks and blends frames at the right levels, SMOOTH, STEPPY and FRAMES, INDEX moving without clicks, every factory table, cutting files into frames, saving and loading, and the plugin keeping a loaded table through the project, presets and undo. Not checked here: how it sounds (I can't listen).

## 0.2.0

Stage 2: **unison and the chorder**, modelled on Avenger's V-Saw and Chorder (our own design and code). Each oscillator has a new **VOICES** page: switch between **WAVE** and **VOICES** at the right of the oscillator tabs.

- **UNISON** (the "supersaw"): up to 7 copies of each note.
  - **VOICES** (2 to 7, the note itself included) and **MIX** (the extra copies' level). At MIX 0 it's off and costs nothing: the sound is exactly as before (tested sample for sample).
  - **DETUNE**: how far apart the copies are tuned; the value shows how far the outer copies are from the note, up to a semitone. It's squared, so the low end of the knob gives fine control for subtle thickening. The copies sit evenly either side of the note (for 7: a third, two thirds and all of the detune, up and down).
  - **SPREAD**: how wide across the stereo field, the sharp copies one way and the flat ones the other.
  - **OCTAVES**: stacks the extra copies up to 3 octaves above the note, in turn.
  - Every extra copy starts at a random point in its wave, which keeps them from lining up and sounding hollow. The sub and noise stay with the note itself, not copied.
- **VIBRATO** on every copy: **GLOBAL** (all together) or **INDEP.** (each copy on its own, a little faster or slower, for a wider, livelier sound); **SHAPE** (sine, triangle, square, saw, random), **RATE**, **AMOUNT** (cents), **FADE IN** and its **CURVE**.
- **CHORDER**: up to 4 extra notes for every key, each with **ON**, **COARSE** (±2 octaves), **FINE**, **PAN** and **VOL**. Each chord note gets its own unison copies too, as on Avenger (notes x copies).
- **The copies display**, left of the VOICES page: every copy of the key and of each chord note, placed by where it sits in the stereo field and by its pitch, brighter the louder, and how many copies that makes per key.
- **Three new factory presets**: Supersaw Lead (A09), Unison Strings (A10, independent vibrato) and Fifth Stab (A11, the chorder). Our own, not tuned by ear.
- **Faster, also for stage 1's sounds:** each copy now works out its table and samples in a way the computer can do several at a time. In the tests: a 7-copy supersaw chord (8 keys, 56 copies) takes about 2.6% of one core; the worst case of 4 oscillators x 3 chord notes x 7 copies x 8 keys (672 copies) about 17%; stage 1's 8-notes-of-4-saws test went from about 3.5% to 2.9%. Your computer will differ; the tests fail if these ever reach 25% and 60%.
- Our choices, where Avenger's manual gives no numbers: the detune range and curve, what OCTAVES does exactly, the vibrato shapes and how much independent copies differ in speed (up to 15% either way).
- In the tests: copies at the pitches and levels DETUNE and MIX say, SPREAD placing them, OCTAVES stacking them, each chord note at its interval, level and pan with its own copies, vibrato's depth, rate and fade-in, global copies moving together and independent ones apart, and the CPU figures above. Not checked here: how it sounds (I can't listen).

## 0.1.0

The synthesizer of the PR7 family, based on Paragon PR7-E 0.15.0. Stage 1 of the plan: the core synth. Inspired by VPS Avenger (our own design and code: none of Avenger's presets, samples or wavetables), and built to be light on CPU.

- **An instrument:** PR7-K loads as a VST3 instrument (MIDI in, stereo out), installs beside the other PR7 plugins and keeps its own presets (in a "Paragon PR7-K" folder) and settings.
- **8 oscillators per note**, each on its own tab with an on/off LED, as on Avenger:
  - **Waves:** saw, pulse (with WIDTH), triangle and sine, all band-limited: nothing folds back below 20 kHz, on any note, without oversampling (measured: more than 110 dB down on a high saw, where a plain one is 21 dB down).
  - **Pitch:** TRANSPOSE (±4 octaves), FINE, and RANDOM (each note a little out of tune, at random or alternately sharp and flat).
  - **Start:** a fixed PHASE (the same attack every time) or a random one, a tiny FADE IN against clicks, and INVERT.
  - **NOISE** (white, pink, brown or between; stereo, half or mono) and a **SUB** (sine, triangle, square or saw, one or two octaves down, or at the note).
  - VOLUME, PAN, its own VOICES limit (1 keeps just that oscillator to one note, e.g. a bass under chords) and TO FILTER.
- **FILTER:** low-pass, high-pass, band-pass and band-stop at 6, 12, 18 and 24 dB per octave (a zero-delay state-variable design that stays clean when swept fast; our own, not a copy of any of Avenger's filter types). CUTOFF, RESONANCE (none on the 6 dB types), DRIVE (smooth saturation going in, with anti-aliasing: about 10 dB less harshness than a plain clipper on high notes), KEYTRACK, its envelope's amount (up or down), and velocity for the cutoff, the resonance and the envelope. Its display shows the response, and the tests check the sound matches it (within 0.75 dB).
- **Envelopes** for the filter and the amp: attack, hold, decay, sustain, release, with a curve for each moving stage. A retriggered note starts from where it was, never from silence, so it doesn't click.
- **AMP:** volume, velocity, pan, pan keytrack, spread (alternating or random per note) and SPIKE (an extra click of level for the first 20 ms).
- **Playing:** poly up to 64 notes (VOICES sets how many; over it, the quietest fading note makes way, with a 3 ms fade), or mono (last, lowest or highest key) with legato; glide (always, or only between overlapping notes); pitch wheel range up and down; the sustain pedal.
- **Light on CPU:** a switched-off oscillator, sub, noise, drive or filter costs nothing, and so does silence. A note ends when it decays to zero sustain, when every oscillator in it has been taken by newer notes, or when its release falls below -90 dB (that last one saves only the end of a long, curved release; most of the saving is from the others). In the tests, 8 notes of 4 saws each through a driven 24 dB filter take about 3.5% of one core on the build machine; the tests fail if it ever passes 25%.
- **The page**, in the PR7 look: oscillators, filter, the two envelopes, amp, play, OUTPUT and its meter, and a keyboard you can play with the mouse (C1 to B7). Two-way knobs (pan, envelope amount, curves) light from the centre.
- **Presets:** the family's preset browser, undo and redo. A new PR7-K is the **Init** preset (one saw, no filter). Eight starting points in bank A: Init, Classic Bass, Saw Pluck, Square Lead, Warm Pad, Sub Bass, Noise Sweep and Saw Brass. Our own, from common synth recipes, and not tuned by ear: the full factory set comes later, once there's unison, wavetables, modulation and effects to build them with.
- **OUTPUT** starts at -6 dB, for headroom when chords stack up. **TUNE** (SETTINGS) tunes the whole synth in cents; it and OUTPUT stay put when you change presets.
- PR7-E's EQ, compressor, resonance control (DOMINION) and limiter are kept in the code, still tested, for the effects rack (stage 7). They aren't in the sound yet.
- Not checked here: how it sounds (I can't listen). Everything above is measured in the tests: pitch, levels, harmonics, folded tones, envelope timing, filter response, voice counts and CPU.
