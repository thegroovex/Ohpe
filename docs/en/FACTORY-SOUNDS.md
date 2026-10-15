# Factory sounds

*English · Deutsch: [../de/FACTORY-SOUNDS.md](../de/FACTORY-SOUNDS.md)*

Ohpe ships with **162 part presets** for all eight engines, **10 project presets** (demo patterns
that use all four parts, the sequencers and the drum kit), **six drum kits** and **ten recorded
instruments** for the Sampler and Grain engines. Open them with **LIBRARY**: pick a category on the
left (Bass, Keys, Pads, Leads, Plucks, Drums, FX, Arp, Textures, Vocal, Boom), or an engine, and click
through the list – every preset loads at once, so you hear it while you browse.

## Playing the presets

* A **part preset** loads into the edit part (part selector 1–4). Its **Part Gain** (`pN.gain`, dB)
  matches its loudness to the other factory sounds: all of them sit around −18 … −12 dBFS RMS when
  played, so switching presets does not jump in level.
* The **FX slot** (chorus, phaser, bitcrush, filter drive, delay, reverb) exists on **part 1 only**.
  Presets that use it (the electric pianos' chorus, the leads' delay, the plucks' reverb …) sound
  their best on part 1; on parts 2–4 the slot is ignored and the sound is dry – add the mixer's
  send effects (send A delay, send B reverb) there instead.
* The macros on the four encoders are the engine's first four parameters; most presets are built so
  that turning them gives musical results (cutoff, tone, position, grain size …). The modulation
  envelope and the LFO are part of every preset.
* Presets with a recorded instrument (Sampler, Grain) need the factory sample bank, which is built
  into the plug-in and the app.

## Project presets (demo patterns)

A **project preset** replaces the whole instrument: four parts, drum kit, mixer, effects, tempo and
the patterns. Press **Play** (Sequencer Run) to hear them. They show how the sequencers combine:
the step sequencer, Euclid and Orbit can each play a part – or, since 0.4, the **drum kit**
(`seq.step.drums`, `seq.euclid.drums`, `seq.orbit.drums`). Orbit's four lanes make a natural drum
machine (one lane per drum, each with its own length); Euclid's four rings play polyrhythmic drums.
Parts without a sequencer are left for you to play along.

## Boom (0.6)

**Boom** is a category of its own (library chip *Boom*, German *Boom*): deep, tuned kicks and sub
basses – the synthesised **Boom** kit (project *Boom Kit*), *Sub Kick*, the sub basses (*Long Sub*,
*Dirty Sub*, *Punch Sub*, *Sine Slide*) and five new ones built on a pure sine sub with a pitch dive
(the 0.6 pitch envelope): *Boom Long Sub* (long, tuned, with a dive), *Boom Distorted* (driven hard),
*Boom Kick Bass* (short: kick and bass in one), *Boom Slide* (overlap notes and it glides – legato
portamento, for trap-style slides) and *Boom Click* (a bright click so it cuts on small speakers).
Play them in the key of the song; *Tune* and the amp *Decay* shape the length. The project *Sub & Glide*
is tagged Boom too.

## Lo-fi (0.6)

The project **Lo-Fi Beat** is classic lo-fi hip-hop: 78 BPM with a lazy, heavy swing, muffled kick,
soft snare and quiet shuffled hats on the Lo-Fi kit, rootless ninth voicings (Fm9 – Bb13 – Ebmaj9 –
C7#9) on **Dusty Keys** (strong tape wow, a bit-reduced edge), a warm round bass (**Mellow Lofi
Bass**) on the roots and **Vinyl Crackle** under it all; the master is darkened by the 0.6 master
low-pass (4.2 kHz) and tape saturation. **Lofi Kit** is the same kit set-up as a starting point for
your own beats. *Vinyl Crackle* is the Analog engine with its oscillators off (0.6 `analog.osclevel`)
and the new crackle generator (`analog.crackle`) – hold any note.

## Drum kits

`drum.kit` chooses the recordings the 24 pads play. All kits have the same pad layout, so every
pattern plays on every kit. Your own pad samples always win over the kit.

| Kit | Sound | Made from |
|---|---|---|
| **Studio** | the original kit: close, room-style and overhead microphones mixed per piece; since 0.5 with punchier kicks (shorter ring, scooped box, beater click, a short pitched sub layer) | *Virtuosity Drums* (CC0), Ohpe's own processing |
| **Tight** | dry and short: close microphones, muted toms, short cymbals, a recorded hand clap; punchy kicks (0.5) | *Virtuosity Drums*, VCSL claps (CC0) |
| **Room** | big and live: room and overhead microphones, long tails, a layered clap with a short room tail | *Virtuosity Drums*, VCSL claps (CC0) |
| **Lo-Fi** | dry close microphones (0.5; before: the source's roomy vintage microphone) with a weighty kick, then band-limited, saturated and quantised to 12 bits – dusty but punchy | *Virtuosity Drums*, VCSL claps (CC0) |
| **Hybrid** | electronic kick, toms, hats, cowbell, claves and congas synthesised by Ohpe, layered with acoustic transients, real cymbals and claps | Ohpe's own synthesis, *Virtuosity Drums*, VCSL (CC0) |
| **Boom** (0.5) | every pad synthesised: a deep sub kick with a pitch envelope on pad 1 (C2), a long, driven sub kick on pad 2 (C#2) – tune and shorten them with the pad's *Tune* and *Decay* – a punchy snare, a crisp layered clap, closed / open / pedal hats, cowbell, rim, toms and percussion | Ohpe's own synthesis (no recordings) |

## Recorded instruments

`pN.sampler.inst` (Sampler) and `pN.grain.inst` (Grain) choose a factory instrument. The Sampler
plays it when no sample of your own is loaded; Grain granulates one of its notes. All recordings are
from the *Versatile Community Sample Library* (VCSL, CC0), multisampled (4–8 notes, marimba with two
velocity layers), level-matched and mono.

| Instrument | Notes | Loop |
|---|---|---|
| Kalimba | 7 | – |
| Marimba | 7 × 2 velocity layers | – |
| Vibraphone | 6 | – |
| Glockenspiel | 6 | – |
| Harp | 8 | – |
| Harpsichord | 7 | – |
| Recorder | 7 | yes – set *Loop* to *Forward* |
| Singing Glass | 4 | yes |
| Zither | 7 | – |
| Bowed Metal (bowed vibraphone) | 6 | yes |

Sustained instruments carry loop points; the presets that use them switch the Sampler's *Loop* to
*Forward*. Licences and sources: [THIRD-PARTY.md](../../THIRD-PARTY.md).

## How the sounds were checked

Every factory preset is rendered offline playing a typical phrase of its category (a bass line,
chords, a lead line, an arpeggio, a groove …) and measured: loudness (target window −18 … −12 dBFS
RMS), peak (no clipping, never more than −1.2 dBFS for part presets), DC, denormals, whether tails
end, and energy above 16 kHz (no fizz unless intended, e.g. hi-hats). Spectrograms of all renders
were reviewed and weak presets reworked. The plug-in tests repeat these checks for every embedded
preset (*"factory sounds: every factory preset renders cleanly …"*).

Tools, for contributors:

* `tools/presets/factory_parts.py`, `factory_projects.py` – the preset definitions;
  `make_factory.py` writes `resources/presets/factory/*.ohpepreset`, `make_docs.py` the lists below.
* `ohpe_render` (built with the core tests) renders presets to WAV and prints the metrics;
  `tools/presets/analyse.py` runs it on every factory preset, flags problems, draws spectrograms and
  writes the level calibration (`levels.json`).
* `tools/samples/build_samples.py` builds the drum kits and instruments (`resources/samples`) from
  the CC0 sources at pinned revisions.

## Preset list

<!-- presets:begin -->
### Bass (26)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Acid Line | Analog | Leads | Squelchy resonant saw with a sub for weight, accent-friendly velocity and a short glide. |
| Bounce Bass | Analog | – | Bright, punchy saw bass for octave-bouncing eighths. |
| Dirty Sub | Analog | Boom | Sub driven into the ladder filter: growling harmonics that stay audible on small speakers. |
| Fat Saw Bass | Analog | – | Two detuned saws and a sub through the ladder filter; big but not boomy. |
| Glide Sine Bass | FM | – | Pure sine bass with legato glide for sliding bass lines; detached notes stay clean. |
| Grit Bass | Wavetable | – | Growly Grit table with an envelope sweep; aggressive but controlled. |
| Growl FM Bass | FM | – | Feedback-edged FM bass with a slow timbre LFO for movement. |
| House Organ Bass | Analog | – | Hollow square with an octave-down second oscillator; short and punchy. |
| Lofi Bass | Analog | – | Round, warm bass: a soft square body over a strong sine sub, gently driven. |
| Log Drum Bass | Sampler | – | Low marimba bars played as a bass: woody, round and short. |
| Long Sub | Analog | Boom | Deep, long-decaying sub that slides between overlapping notes (glide); a soft click on the attack. |
| Mellow Lofi Bass | Analog | – | Warm, round bass: a soft, low-passed body over a sine sub with gentle saturation and a hint of glide. |
| Punch FM Bass | FM | – | Solid digital bass with a bright attack and a clean sine body. |
| Punch Sub | Analog | Boom | Short, punchy sub with a snappy filter click; locks in with the kick. |
| Reese Drift | Analog | – | Wide detuned saws slowly beating against each other; dark and moving. |
| Rise Bass | Analog | – | Driven bass that slides up a fifth into every note; great for syncopated lines. |
| Rolling Bass | Analog | – | Tight, driven techno bass for rolling sixteenths: short filter snap, solid sub. |
| Round Sub | Analog | – | Warm square and sub oscillator with a soft filter blip; sits under anything. |
| Rubber Bass | Analog | Plucks | Bouncy plucked bass: fast filter decay, round sub. |
| Sine Slide | FM | Boom | Nearly pure sine sub with a long glide for sliding bass lines. |
| Slap FM Bass | FM | – | Percussive FM bass: the index snaps shut like a slapped string. |
| Table Sub | Wavetable | – | Near-sine wavetable bass with a sub; pure low end with a touch of edge. |
| Talking Bass | Wavetable | – | Formant table swept by the envelope: a vowel 'wow' on every note. |
| Unison Bass | Cluster | – | Octave-stacked saws with gentle detune; thick and wide for drops. |
| Upright Pluck | Pluck | Plucks | Plucked string with a big wooden body: an acoustic-style bass. |
| Wobble Bass | Analog | FX | Tempo-ish filter wobble on a driven saw stack; turn the LFO rate to taste. |

### Keys (22)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Cluster Organ | Cluster | – | Octave stack of soft squares: a cheesy combo organ. |
| Digital EP | FM | – | Brighter electric piano with more bark when played hard. |
| Drawbar Organ | FM | – | Additive organ: six sine drawbars, gentle vibrato and a rotary-style chorus. |
| Dub Stab | Analog | Pads | Dark detuned chord stab whose filter drifts slowly open and shut; give it delay and reverb. |
| Dusty EP | FM | – | Warm electric piano with tape wow and flutter (slow pitch LFO) and chorus; made for lo-fi chords. |
| Dusty Keys | FM | – | Mellow, dark electric piano for lo-fi chords: strong tape wow on the pitch, a bit-reduced edge, soft attack. |
| FM Vibes | FM | Plucks | Mellow metallic bars with tremolo. |
| Funk Clav | FM | Plucks | Snappy clavinet-style keys; play short and syncopated. |
| Glass Keys | Wavetable | – | Bell-like Glass table that darkens as the note decays. |
| Glide Poly Synth | Analog | Leads | Poly synth whose chords always slide from the chord before (poly glide). |
| Harmonic Organ | Wavetable | – | Additive Harmonic table: a soft, slightly breathy organ. |
| Harpsichord | Sampler | Plucks | Recorded harpsichord (seven notes, CC0); baroque lines and bright comping. |
| Mallet Keys | Pluck | Plucks | Physically modelled wooden bars; soft mallet. |
| Marimba | Sampler | Plucks | Recorded marimba with two velocity layers. |
| Minor Stab | Cluster | – | One finger plays a minor-seventh chord stab; detuned and punchy. |
| Music Box | Sampler | Plucks | Glockenspiel softened into a music box; lullabies and toy melodies. |
| Pop Brass | Analog | Pads | Lush, detuned poly brass with a quick swell and chorus: classic synth-pop chords. |
| Scoop Keys | FM | – | Electric piano where every note scoops up a whole tone: soulful, a little lazy. |
| Soft Brass | Analog | Pads | Polyphonic brass: a slow filter swell on detuned saws. |
| Tine Piano | FM | – | Electric piano: soft body plus a bell-like tine that fades with the key. Chorus in the FX slot. |
| Velvet Stab | Analog | – | Short filtered chord stab for house and disco. |
| Vibraphone | Sampler | – | Recorded vibraphone with a gentle motor tremolo. |

### Pads (28)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Analog Dawn | Analog | – | Pulse pad whose filter opens slowly, with a slow phaser. |
| Angel Pad | Vocal | Vocal | High 'oo' voices morphing slowly towards 'ah'. |
| Bowed Haze | Grain | – | Bowed vibraphone grains: a soft metallic halo. |
| Bowed Metal | Sampler | Textures | Bowed vibraphone recordings (CC0), looped: glassy and eerie. |
| Brass Slide | Analog | Keys | Swelling poly brass with quick portamento between chords. |
| Breath Choir Pad | Vocal | Vocal | Airy 'ah' choir with plenty of breath. |
| Crystal FM Pad | FM | – | Five slightly detuned sine carriers sharing one modulator. |
| Crystal Slide | FM | – | Shimmering FM pad: detuned sine carriers sliding between chords. |
| Drift Stack | Cluster | Textures | Detuned saw stacks drifting slowly from chord to chord. |
| Fall Haze | Wavetable | FX | Hazy pad whose notes sink a fifth from above into the chord. |
| Fifth Cloud | Cluster | – | Open fifths with slow drift: cinematic and neutral. |
| Frozen Cloud | Grain | Textures | Granular freeze of the built-in harmonic texture. |
| Glass Glide | Wavetable | Textures | Wide glass pad with slow, dreamy slides between chords. |
| Glass Grains | Grain | Textures | Grains of a singing wine glass (CC0 recording), slowly scanned. |
| Glass Shimmer | Wavetable | Textures | Glass table with wide unison: airy, glittering pad. |
| Glide Strings | Analog | – | Ensemble strings whose chords slide into each other (poly glide); legato chord changes. |
| Harmonic Swell | Wavetable | – | Harmonics bloom in as the pad swells. |
| Night Drive | Analog | Textures | Dark resonant pad breathing with a slow filter LFO. |
| Rise Bloom | FM | – | Shimmering FM pad blooming up from an octave below; long and bright. |
| Rise Choir | Vocal | Vocal | Airy 'ah' choir that scoops up a fourth into every chord. |
| Rise Glass | Wavetable | Textures | Wide glass pad: each note glides up a fifth into place, dreamy and slow. |
| Rise Strings | Analog | – | Ensemble strings whose notes rise an octave up into the chord on every new note. |
| Rise Warmth | Cluster | – | Soft detuned saw stack whose notes lean up a whole tone into the chord. |
| Slide Choir | Vocal | Vocal | Airy 'ah' choir; each voice slides to the next chord. |
| Super Stack | Cluster | – | Seven detuned saws per key, wide and lush. |
| Sus Horizon | Cluster | – | Suspended-second stacks that never resolve; ambient beds. |
| Vowel Pad | Wavetable | – | Formant table drifting between vowels; slow attack. |
| Warm Strings | Analog | – | Ensemble strings: detuned saws, an octave up, gentle chorus. |

### Leads (18)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Classic Mono Lead | Analog | – | Saw lead with a little vibrato, glide and a dotted delay. |
| Digital Lead | Wavetable | – | Warped Digital table with unison: modern and cutting. |
| FM Brass Lead | FM | – | Brassy FM lead: the index rises with a short swell. |
| Fall Lead | Analog | FX | Square lead that drops a fifth from above into every note. |
| Formant Lead | Wavetable | Vocal | Vowel-like lead that opens up with the envelope. |
| Hot Wire | Analog | – | Driven, resonant saw for rock-ish solos. |
| Legato Glide Lead | Analog | – | Mono saw lead with legato portamento: overlapping notes glide, detached notes do not retrigger a slide. |
| Octave Lead | Cluster | – | Octave-stacked pulses with a quick filter blip. |
| Pop Lead | Analog | – | Bright saw and an octave square with vibrato: a clear, singing synth-pop lead. |
| Portamento Square | Analog | – | Hollow square that always glides, at a constant rate: big jumps slide longer than small steps. |
| Recorder | Sampler | – | Recorded alto recorder, looped, with light vibrato. |
| Rise Lead | Analog | – | Saw lead that swoops up an octave into every note; a dotted delay behind it. |
| Saw Unison Lead | Wavetable | – | Classic table at the saw position with wide unison. |
| Sine Whistle | FM | – | Almost pure sine with a breath of modulation; delicate melodies. |
| Soft Flute Lead | Analog | – | Breathy, resonant low-pass on a square: a soft whistle tone. |
| Square Hero | Analog | – | Hollow square plus an octave: a bright, singing lead. |
| Super Lead | Cluster | – | Seven-saw unison lead: festival size. |
| Vox Lead | Vocal | Vocal | Solo synthetic voice with expressive vibrato and glide. |

### Plucks (14)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Concert Harp | Sampler | – | Recorded concert harp (eight notes, CC0). |
| FM Koto | FM | – | Plucked FM: a bright attack that quickly mellows. |
| Glass Pluck | Wavetable | – | Glass table pluck with a bell-like sparkle. |
| Harp Pluck | Pluck | – | Long-ringing harp-like string with a soft attack. |
| Kalimba | Sampler | Keys | Recorded thumb piano with its buzzy, woody tines (CC0). |
| Koto Pluck | Pluck | – | Bright, twangy string with a resonant body. |
| Muted Pick | Analog | – | Short palm-muted square pluck for fast lines. |
| Nylon Guitar | Pluck | – | Warm nylon string plucked near the soundhole. |
| Poly Pluck | Analog | – | Classic synth pluck: snappy filter on a detuned saw pair. |
| Steel Drum FM | FM | Keys | Tropical pan tones from inharmonic FM. |
| Steel String | Pluck | – | Bright steel string plucked near the bridge. |
| Table Pluck | Wavetable | – | Wavetable pluck: bright saw frame closing fast. |
| Zip Pluck | Wavetable | – | Bright pluck with a fast upward zip into the note. |
| Zither | Sampler | – | Recorded zither: shimmering plucked strings (CC0). |

### Drums (11)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Cluster Hat | Cluster | – | Stacked narrow pulses detuned into a metallic hat. |
| FM Snare | FM | – | Feedback noise plus a body tone: a crisp digital snare. |
| FM Tom | FM | – | Pitched FM drum with a woody knock. |
| Filter Kick | Analog | – | Kick from a self-oscillating ladder swept down by the envelope. Play around C1-C2. |
| Metal Hat FM | FM | – | Inharmonic FM noise: closed and open hats depending on the key length. |
| Noise Snare | Analog | – | Band-passed noise and tone: an electronic snare. |
| Sub Kick | Analog | Bass, Boom | Tunable deep kick: a self-oscillating filter tuned to the key, swept down from above; long boom, driven. |
| Tick Hat | Analog | – | High-passed noise tick for hi-hat patterns. |
| Tuned Tom | Pluck | – | Modelled drum-like bar with a big body; play it in octaves. |
| Wood Block | Pluck | – | Hard mallet on a short wooden bar. |
| Zap Perc | Wavetable | FX | Digital table snapped shut: zaps, clicks and lasers. |

### FX (10)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Alien Signal | FM | – | Random-stepping FM timbre: radio chatter from far away. |
| Detune Swarm | Cluster | Textures | Seven wildly drifting oscillators: a swarm. |
| Gong Hit | FM | Textures | Inharmonic FM gong with a long shimmer. |
| Granular Rise | Grain | – | Grains scan forward and climb while the key is held. |
| Grit Sweep | Wavetable | – | Noisy Grit table opening over two seconds. |
| Laser Drop | Analog | Drums | Resonant falling sweep; short hits become lasers. |
| Noise Riser | Analog | – | Hold a key: filtered noise climbs for four seconds. |
| Reverse Spray | Grain | Textures | Mostly reversed harp grains sprayed across the stereo field. |
| Stutter Glitch | Grain | – | Tiny, sparse grains: glitchy stutters from the marimba. |
| Table Scanner | Wavetable | Textures | Digital table swept by a slow LFO: robotic movement. |

### Arp (9)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Analog Arp | Analog | Plucks | Resonant square blips with a ping-pong delay. |
| Bubble Arp | Analog | – | Narrow pulse through a resonant filter: bubbly and round. |
| Digital Pluck Arp | FM | – | Clicky digital pluck that loves fast sixteenths. |
| FM Arp Bell | FM | Keys | Short glassy FM bells for sequences. |
| Formant Arp | Wavetable | Vocal | Vowel blips for talking sequences. |
| Glock Arp | Sampler | Keys | Recorded glockenspiel for sparkling arpeggios (CC0). |
| Pizzicato Arp | Pluck | Plucks | Short plucked strings for ostinatos. |
| Sparkle Bell | FM | Plucks | Short, bright FM bell for sparkling arpeggios over chords. |
| Table Arp | Wavetable | – | Wavetable sequence sound: the position snaps with each note. |

### Textures (11)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Backwards Kalimba | Sampler | FX | The kalimba played in reverse: swells that end in a pluck. |
| Breath Texture | Vocal | Vocal | Nearly unvoiced breath through slowly moving vowels. |
| Fourths Drift | Cluster | Pads | Stacked fourths with heavy drift: an unresolved haze. |
| Grit Drone | Wavetable | – | Slowly churning Grit table; hold one low note. |
| Harp Mist | Grain | Pads | Long, soft harp grains blurred into a mist. |
| Kalimba Dust | Grain | – | Tiny kalimba grains scattered like dust. |
| Marimba Rain | Grain | – | Sparse marimba grains falling like rain. |
| Recorder Breath | Grain | Vocal | Breathy recorder grains: airy, vocal-like tones. |
| Singing Glass | Sampler | Pads | Rubbed wine-glass recordings (CC0), looped: pure and haunting. |
| Vinyl Crackle | Analog | FX | Record crackle and soft hiss (Analog engine: oscillators off, crackle and noise on); hold a note under a beat. |
| Zither Cloud | Grain | – | Shimmering zither grains, octave sprinkles. |

### Vocal (8)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Aah Choir | Vocal | Pads | Mixed choir singing 'ah'. |
| Formant Voices | Wavetable | Pads | Formant wavetable with unison: synthetic choir. |
| Gospel Hum | Vocal | – | Closed-mouth hum: warm, low and intimate. |
| Male Ensemble | Vocal | Pads | Low voices on 'oh': a men's choir. |
| Ooh Choir | Vocal | Pads | Soft 'oo' choir, round and dark. |
| Solo Soprano | Vocal | Leads | High solo voice: bright formants, generous vibrato. |
| Talking Synth | Vocal | Leads | The LFO walks through the vowels: a talking lead. |
| Vowel Morph | Vocal | – | Each note sings 'ee' and opens to 'ah' with the envelope. |

### Boom (10)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Boom Click | Analog | Bass | Sine sub with a bright click on the attack, so the boom cuts through on small speakers. |
| Boom Distorted | Analog | Bass | Sine sub driven hard into the ladder filter and the FX slot: a growling, distorted boom. |
| Boom Kick Bass | Analog | Drums, Bass | Short boom: a two-octave pitch dive into a tuned sine - kick and bass in one. |
| Boom Long Sub | Analog | Bass | Long, tuned sine sub with a quick pitch dive on the attack; rings out for seconds. |
| Boom Slide | Analog | Bass | Long sine sub made for trap-style slides: overlap notes and it glides (legato, constant time). |
| Dirty Sub | Analog | Bass | Sub driven into the ladder filter: growling harmonics that stay audible on small speakers. |
| Long Sub | Analog | Bass | Deep, long-decaying sub that slides between overlapping notes (glide); a soft click on the attack. |
| Punch Sub | Analog | Bass | Short, punchy sub with a snappy filter click; locks in with the kick. |
| Sine Slide | FM | Bass | Nearly pure sine sub with a long glide for sliding bass lines. |
| Sub Kick | Analog | Drums, Bass | Tunable deep kick: a self-oscillating filter tuned to the key, swept down from above; long boom, driven. |

### Projects / demo patterns (10)

| Name | Categories | Description |
|---|---|---|
| Lo-Fi Beat | Drums, Keys, Bass | Classic lo-fi hip-hop at 78 BPM with a lazy, heavy swing: muffled kick, soft snare and quiet shuffled hats on the Lo-Fi kit (Euclid), rootless ninth voicings on Dusty Keys with strong tape wow (Orbit lanes 1-3), a warm round bass on the roots (step sequencer), vinyl crackle (Orbit lane 4). Fm9 - Bb13 - Ebmaj9 - C7#9; dark master (low-pass, tape saturation). Press Play. |
| Techno Groove | Drums, Bass | Driving four-on-the-floor on the Hybrid kit from Orbit (claps on 2 and 4, rims, accented 16th hats, off-beat open hat), a rolling bass from the step sequencer and dark seventh stabs from Euclid with a slowly drifting filter and a synced dotted-eighth delay. |
| Ambient Drift | Pads, Textures | A slow fifths pad, a generative kalimba from four Orbit lanes of 5, 7, 11 and 16 steps with a tempo-synced dotted-eighth delay, sparse singing-glass tones from Euclid. No drums. |
| Synth-Pop | Drums, Bass, Leads | Bright four-on-the-floor on the Tight kit from Euclid (snare and clap on 2 and 4, eighth hats), an octave-bouncing bass, lush poly brass and a sparkle arpeggio from three Orbit lanes over Am-F-C-G, and a lead melody from the step sequencer. |
| Euclid Polyrhythm | Drums, Arp | Four Euclid rings of 8, 12, 16 and 9 steps on the Room kit, a marimba from Orbit lanes of 5, 6, 7 and 8 steps and a pedal bass. |
| Arp Sequence | Arp, Drums | A 64-step arpeggio over Am, Fmaj7, G and Em on an A pedal, Studio-kit drums from Orbit. The arpeggiator is latched on part 3: play chords. |
| Deep House | Drums, Bass, Keys | Shuffled hats and a deep kick on the Tight kit (Orbit), a syncopated organ bass (step sequencer) and Euclid chord stabs. |
| Sub & Glide | Pads, Bass, Drums, Boom | Seventh chords that slide into each other (Glide Strings, poly glide), a long sliding sub on Orbit lane 1 and a half-time beat on the Boom kit from Orbit lanes 2-4, sparkle bells from Euclid. Cm7 - Abmaj7 - Ebmaj7 - Bb7. |
| Lofi Kit | Drums | The Lo-Fi kit set up for lo-fi hip-hop: muffled, round kick, soft snare with ghost notes and a rim, dull shuffled hats with velocity variation (Orbit, lazy swing), vinyl crackle on part 4 (Orbit lane 4) and a dark master. Play your own keys on part 1. |
| Boom Kit | Drums, Bass, Boom | The synthesised Boom kit in a half-time beat with rolling hats (Orbit), Boom Slide on the step sequencer - overlapping notes glide (legato portamento) - and a dark pad that rises into its chord (Rise Glass, Euclid). C minor, 70 BPM. |

**Engines:** FM 28, Wavetable 24, Analog 51, Pluck 9, Grain 11, Vocal 13, Cluster 13, Sampler 13
<!-- presets:end -->
