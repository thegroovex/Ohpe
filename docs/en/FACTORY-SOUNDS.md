# Factory sounds

*English · Deutsch: [../de/FACTORY-SOUNDS.md](../de/FACTORY-SOUNDS.md)*

Ohpe ships with **282 part presets** for all eight engines, **14 project presets** (demo patterns
that use all four parts, the sequencers and the drum kit), **six drum kits**, **ten recorded
instruments** and (0.8) **four keyboard instruments** (grand and upright piano, tine and reed
electric piano) for the Sampler and Grain engines. Open them with **LIBRARY**: pick a category on the
left (Bass, Keys, Pads, Leads, Plucks, Drums, FX, Arp, Textures, Vocal, Boom, Hybrid, Piano), or an engine, and click
through the list – every preset loads at once, so you hear it while you browse.

## Playing the presets

* A **part preset** loads into the edit part (part selector 1–4). Its **Part Gain** (`pN.gain`, dB)
  matches its loudness to the other factory sounds: all of them sit around −18 … −12 dBFS RMS when
  played, so switching presets does not jump in level.
* The **FX rack** (up to six effects per part, see [TAPE-MIXER-FX.md](TAPE-MIXER-FX.md)) is part of
  the preset and exists on **every part**, so a preset sounds the same in any part. (The projects
  made before 0.8 keep their parts 2–4 dry, as they were designed.)
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

## Sound revision 3 (0.8)

120 new part presets and four projects, made for modern productions and for playing:

* **Bass music:** reese basses (classic detuned, phased, flanged, hollow comb, distorted neuro,
  vowel and moving reeses) built on the Wavetable engine's new unison stack (up to eight voices)
  with a mono-safe low end (*Bass Mono* below 150 Hz after the FX rack); drum & bass subs and rollers;
  wobble and growl basses with the tempo-synced *Wobble* (it restarts with every note), the vowel
  *Formant* filter, FM on the wavetable, wavefolding (*Distortion*) and *Multiband* squash – chains of
  up to six effects per preset.
* **Leads** with stereo stacks, delays and halls; **stereo effects** (risers, impacts, zaps,
  whooshes).
* **Pianos** (category *Piano*): grand and upright piano recordings, modelled tine and reed electric
  pianos (suitcase autopan, tremolo, dream chorus) and an FM electric piano with velocity bark.
* **Thicker granular sounds and fuller arps:** eight grain voices with up to 95 grains per second,
  instrument and piano sources, echoes and halls; arps with stacks, ping-pong delays and halls.
* **Evolving atmospheres:** slow wavetable scans, two-bar autopans and filter tides, drifting
  combs and endless halls.
* **Hybrid** (category *Hybrid*, 18 presets): two engines in one part with the new **layer engine**
  and cross-modulation ([engines/layer.md](engines/layer.md)) – a pluck dissolving into a grain cloud,
  a reese with a singing vocal layer, ring-modulated FM grains, a piano with a halo of its own frozen
  recording, a pad chopped by a grain envelope.
* **Movement:** every new preset moves when a note is held (slow LFOs on the wavetable position,
  filter or pan, synced effects).
* **MPE:** 93 of the new presets carry the free tag **MPE**: pressure, timbre and per-note bend
  slides are mapped to suit each sound (see [MIDI.md](MIDI.md)); pressure also works from any
  aftertouch keyboard.

New projects: **Neuro Engine** (neurofunk, 174 BPM), **Halftime Pressure** (dubstep, 140 BPM),
**Liquid Roll** (liquid drum & bass, 174 BPM), **Evolving Space** (ambient, 66 BPM).

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
### Bass (65)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Acid Line | Analog | Leads | Squelchy resonant saw with a sub for weight, accent-friendly velocity and a short glide. |
| Analog Reese | Analog | MPE | Two analog saws detuned wide through the ladder filter, phaser and squash: warm and fat. |
| Bounce Bass | Analog | – | Bright, punchy saw bass for octave-bouncing eighths. |
| Classic Reese | Wavetable | MPE | Two detuned saws beating slowly against each other, a clean sub underneath; mono below 120 Hz. |
| Click Sub | Analog | – | Sine sub with a sharp click on top, so the bass line reads on small speakers. |
| Dark Reese | Wavetable | MPE | Low, smoky reese: dark filter, slow chorus, light squash - for liquid and deep tracks. |
| Deep Pluck Sub | Analog | – | Plucked sub with a bright attack that fades into a clean sine; tight for fast lines. |
| Digital Monster | Wavetable | Hybrid, MPE | Digital FM table gated by a glass-grain layer an octave down, then wobbled: unstable and huge. |
| Dirty Sub | Analog | Boom | Sub driven into the ladder filter: growling harmonics that stay audible on small speakers. |
| Distorted Reese | Wavetable | MPE | Tube-driven reese with a grinding midrange; the squash keeps it dense and upfront. |
| Fat Saw Bass | Analog | – | Two detuned saws and a sub through the ladder filter; big but not boomy. |
| Flanged Reese | Wavetable | MPE | Saw stack with a slow jet flanger and chorus: swirling stereo top, solid mono bottom. |
| Fold Bass | Wavetable | Hybrid, MPE | Sine sub and an analog saw folded into each other: the fold amount makes it roar. |
| Glide Reese | Wavetable | MPE | Reese with constant-rate portamento: overlap notes and it slides like a fretless rubber band. |
| Glide Sine Bass | FM | – | Pure sine bass with legato glide for sliding bass lines; detached notes stay clean. |
| Grind Bass | Wavetable | MPE | Grinding, folded Grit table squashed hard: a constant, dense wall of mid-range. |
| Grit Bass | Wavetable | – | Growly Grit table with an envelope sweep; aggressive but controlled. |
| Growl FM Bass | FM | – | Feedback-edged FM bass with a slow timbre LFO for movement. |
| Growl Machine | Wavetable | MPE | Formant table with FM and an LFO on the position, a synced vowel filter and heavy squash: a talking growl. |
| Half-Step Wobble | Wavetable | MPE | Dotted-quarter wobble that runs across the bar line - heavy and lopsided. |
| Halftime Sub | Wavetable | – | Long, heavy sine sub with always-on glide: slow slides for half-time sections. |
| Hollow Reese | Wavetable | MPE | Reese through a negative-feedback comb that drifts slowly: hollow, metallic and dark. |
| House Organ Bass | Analog | – | Hollow square with an octave-down second oscillator; short and punchy. |
| Laser Wobble | Wavetable | MPE | Rising saw LFO on a band-pass wobble at sixteenth notes: zapping, laser-like movement. |
| Liquid Sub | FM | – | Pure, warm sine sub with a soft second harmonic; slides between overlapping notes. |
| Lofi Bass | Analog | – | Round, warm bass: a soft square body over a strong sine sub, gently driven. |
| Log Drum Bass | Sampler | – | Low marimba bars played as a bass: woody, round and short. |
| Long Sub | Analog | Boom | Deep, long-decaying sub that slides between overlapping notes (glide); a soft click on the attack. |
| Mellow Lofi Bass | Analog | – | Warm, round bass: a soft, low-passed body over a sine sub with gentle saturation and a hint of glide. |
| Metal Growl | Wavetable | MPE | Clanging metallic growl: FM table, a resonant comb on top and hard squash. |
| Moving Reese | Wavetable | MPE | Reese whose table position and filter breathe with a slow LFO - movement on held notes without automation. |
| Neuro Hybrid | FM | Hybrid, MPE | FM growl and a wavetable reese ring-modulating each other, folded and squashed: a neuro monster. |
| Neuro Reese | Wavetable | MPE | Distorted six-voice reese: driven filter, folded harmonics and a hard mono low end - made to be automated. |
| Phase Reese | Wavetable | MPE | Four-voice saw stack through a slow, deep phaser: the classic sweeping drum & bass reese. |
| Punch FM Bass | FM | – | Solid digital bass with a bright attack and a clean sine body. |
| Punch Sub | Analog | Boom | Short, punchy sub with a snappy filter click; locks in with the kick. |
| Reese Drift | Analog | – | Wide detuned saws slowly beating against each other; dark and moving. |
| Riddim Saw | Wavetable | MPE | Hard-clipped saw with a quarter-note band-pass wobble: sparse, heavy and minimal. |
| Rise Bass | Analog | – | Driven bass that slides up a fifth into every note; great for syncopated lines. |
| Roller Bass | Analog | – | Punchy square-ish roller: a short filter snap on every note, solid sub, dense with squash. |
| Rolling Bass | Analog | – | Tight, driven techno bass for rolling sixteenths: short filter snap, solid sub. |
| Rolling Sub | Wavetable | – | Round sub with a tiny filter blip and gentle saturation for rolling bass lines; mono and steady. |
| Round Sub | Analog | – | Warm square and sub oscillator with a soft filter blip; sits under anything. |
| Rubber Bass | Analog | Plucks | Bouncy plucked bass: fast filter decay, round sub. |
| Screech Bass | Wavetable | MPE | Screaming high-ratio FM with a folded top: for the high, aggressive bass answers. |
| Sine Slide | FM | Boom | Nearly pure sine sub with a long glide for sliding bass lines. |
| Slap FM Bass | FM | – | Percussive FM bass: the index snaps shut like a slapped string. |
| Square Wobble | Wavetable | MPE | Hard sixteenth on / off wobble with a square LFO - choppy and rhythmic. |
| Step Sub | Wavetable | – | Sub that drops a fifth into every note: a little pitch kick for steppy, half-time lines. |
| Sub Reese | Wavetable | MPE | Gentle reese on top of a big sine sub - the sub stays mono and steady, the top moves. |
| Table Sub | Wavetable | – | Near-sine wavetable bass with a sub; pure low end with a touch of edge. |
| Talker Bass | Wavetable | MPE | Formant table walked by a fast LFO and an envelope: each note says something different. |
| Talking Bass | Wavetable | – | Formant table swept by the envelope: a vowel 'wow' on every note. |
| Tear Reese | Wavetable | MPE | Aggressive neuro reese: FM-edged digital table, heavy multiband squash with lifted highs. |
| Tech Stab Bass | FM | – | Hard, short tech bass: FM bite, hard clipper and squash for clinical, percussive lines. |
| Triplet Wobble | Wavetable | MPE | Eighth-triplet wobble on a driven saw stack: the bouncing half-time classic. |
| Unison Bass | Cluster | – | Octave-stacked saws with gentle detune; thick and wide for drops. |
| Upright Pluck | Pluck | Plucks | Plucked string with a big wooden body: an acoustic-style bass. |
| Vowel Growl | Wavetable | MPE | Quarter-note vowel sweep (a - e - i - o - u) over a folded saw: the classic 'talking' bass. |
| Vowel Reese | Wavetable | MPE | Reese that speaks: formant table under an LFO plus a synced vowel filter walking a - o - i. |
| Vox Reese | Wavetable | Hybrid, MPE | A reese with a singing vocal layer on top: the formants ride the bass, a synced vowel filter makes it talk. |
| Warm Roller | Analog | – | Ladder-filtered saw with sub for warm, round rolling basslines in liquid tracks. |
| Wide Reese | Wavetable | MPE | Eight-voice reese spread across the stereo field, with a Haas spread on top and a mono low end. |
| Wobble Bass | Analog | FX | Tempo-ish filter wobble on a driven saw stack; turn the LFO rate to taste. |
| Wub Machine | Wavetable | MPE | Saw stack through a tempo-synced eighth-note wobble filter that restarts with every note. |

### Keys (37)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Bright Grand | Sampler | Piano, MPE | Grand piano opened up for pop: brighter, lightly squashed, short room. |
| Bright Poly Wide | Analog | MPE | Classic bright poly synth, chorused and widened - chords that fill the room. |
| Classic Tine | Sampler | Piano, MPE | Modelled tine electric piano: bell-like when soft, barking when hit hard. |
| Cluster Organ | Cluster | – | Octave stack of soft squares: a cheesy combo organ. |
| Concert Grand | Sampler | Piano, MPE | Grand piano recordings, two velocity layers, a touch of hall. |
| Crystal Organ | FM | Hybrid, MPE | Additive FM organ with a glass wavetable layer, circling in stereo with a slow autopan. |
| Digital EP | FM | – | Brighter electric piano with more bark when played hard. |
| Drawbar Organ | FM | – | Additive organ: six sine drawbars, gentle vibrato and a rotary-style chorus. |
| Dream Tine | Sampler | Piano, Pads, MPE | Tine piano drenched in chorus, dotted delay and a long hall. |
| Dub Stab | Analog | Pads | Dark detuned chord stab whose filter drifts slowly open and shut; give it delay and reverb. |
| Dusty EP | FM | – | Warm electric piano with tape wow and flutter (slow pitch LFO) and chorus; made for lo-fi chords. |
| Dusty Keys | FM | – | Mellow, dark electric piano for lo-fi chords: strong tape wow on the pitch, a bit-reduced edge, soft attack. |
| FM Bark Piano | FM | Piano, MPE | Two-pair FM electric piano: the tine pair rings, the bark pair bites with velocity. |
| FM Vibes | FM | Plucks | Mellow metallic bars with tremolo. |
| Felt Grand | Sampler | Piano, MPE | Dark, soft grand: the top rolled off, a slow release and a warm hall - intimate. |
| Funk Clav | FM | Plucks | Snappy clavinet-style keys; play short and syncopated. |
| Glass Keys | Wavetable | – | Bell-like Glass table that darkens as the note decays. |
| Glide Poly Synth | Analog | Leads | Poly synth whose chords always slide from the chord before (poly glide). |
| Halo Grand | Sampler | Hybrid, Piano, MPE | Grand piano with a halo: its own recording frozen into a grain cloud that blooms behind every chord. |
| Harmonic Organ | Wavetable | – | Additive Harmonic table: a soft, slightly breathy organ. |
| Harpsichord | Sampler | Plucks | Recorded harpsichord (seven notes, CC0); baroque lines and bright comping. |
| Mallet Keys | Pluck | Plucks | Physically modelled wooden bars; soft mallet. |
| Marimba | Sampler | Plucks | Recorded marimba with two velocity layers. |
| Minor Stab | Cluster | – | One finger plays a minor-seventh chord stab; detuned and punchy. |
| Music Box | Sampler | Plucks | Glockenspiel softened into a music box; lullabies and toy melodies. |
| Pop Brass | Analog | Pads | Lush, detuned poly brass with a quick swell and chorus: classic synth-pop chords. |
| Reed Keys | Sampler | Piano, MPE | Modelled reed electric piano: nasal and woody, with a gritty bark when driven. |
| Reed Tremolo | Sampler | Piano, MPE | Reed piano through its classic amplitude tremolo - soul and gospel. |
| Scoop Keys | FM | – | Electric piano where every note scoops up a whole tone: soulful, a little lazy. |
| Soft Brass | Analog | Pads | Polyphonic brass: a slow filter swell on detuned saws. |
| Suitcase Tine | Sampler | Piano, MPE | Tine piano with the stereo autopan of a suitcase amp and a small room. |
| Tape Ghost Keys | Sampler | Hybrid, Piano, MPE | Tine piano with a ghost of itself: a detuned grain copy, crushed and flanged like an old resampled tape. |
| Tine Piano | FM | – | Electric piano: soft body plus a bell-like tine that fades with the key. Chorus in the FX slot. |
| Upright Keys | Sampler | Piano, MPE | Upright piano recordings with a little room: honest and close. |
| Velvet Stab | Analog | – | Short filtered chord stab for house and disco. |
| Vibraphone | Sampler | – | Recorded vibraphone with a gentle motor tremolo. |
| Wide Organ | Wavetable | – | Drawbar-like organ with a rotating stereo pan and a short hall. |

### Pads (44)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Analog Dawn | Analog | – | Pulse pad whose filter opens slowly, with a slow phaser. |
| Angel Pad | Vocal | Vocal | High 'oo' voices morphing slowly towards 'ah'. |
| Aurora Scan | Wavetable | Textures, MPE | Glass table scanned slowly, circling in stereo with a two-bar autopan. |
| Bowed Haze | Grain | – | Bowed vibraphone grains: a soft metallic halo. |
| Bowed Metal | Sampler | Textures | Bowed vibraphone recordings (CC0), looped: glassy and eerie. |
| Brass Slide | Analog | Keys | Swelling poly brass with quick portamento between chords. |
| Breath Choir Pad | Vocal | Vocal | Airy 'ah' choir with plenty of breath. |
| Breath Grains | Grain | Textures, MPE | Recorder breath sliced into a soft, wide grain pad. |
| Chop Pad | Cluster | Hybrid, MPE | A wide chord pad chopped and stuttered by the envelope of a kalimba grain layer. |
| Crystal FM Pad | FM | – | Five slightly detuned sine carriers sharing one modulator. |
| Crystal Slide | FM | – | Shimmering FM pad: detuned sine carriers sliding between chords. |
| Drift Stack | Cluster | Textures | Detuned saw stacks drifting slowly from chord to chord. |
| Evolving Tide | Wavetable | MPE | Harmonic table drifting through its frames on a very slow LFO, eight-voice stack, flanger and hall. |
| Fall Haze | Wavetable | FX | Hazy pad whose notes sink a fifth from above into the chord. |
| Fifth Cloud | Cluster | – | Open fifths with slow drift: cinematic and neutral. |
| Frozen Cloud | Grain | Textures | Granular freeze of the built-in harmonic texture. |
| Frozen Vibes | Grain | Textures, MPE | Vibraphone grains held still, with a slow flanger: a frozen, shimmering chord. |
| Ghost Organ | Wavetable | Keys | Hollow harmonic organ with a slow autopan and a cavernous hall. |
| Glass Engine | Grain | Hybrid, MPE | Singing-glass grain cloud fused with a harmonic wavetable layer that scans slowly; a touch of ring modulation. |
| Glass Glide | Wavetable | Textures | Wide glass pad with slow, dreamy slides between chords. |
| Glass Grains | Grain | Textures | Grains of a singing wine glass (CC0 recording), slowly scanned. |
| Glass Shimmer | Wavetable | Textures | Glass table with wide unison: airy, glittering pad. |
| Glide Strings | Analog | – | Ensemble strings whose chords slide into each other (poly glide); legato chord changes. |
| Harmonic Swell | Wavetable | – | Harmonics bloom in as the pad swells. |
| Infinite Hall | Cluster | MPE | Major-seventh stack in an enormous hall that never seems to end. |
| Metal Strings | Pluck | Hybrid, MPE | Metallic plucked strings folded into a slow cluster pad - an orchestra made of wire. |
| Night Drive | Analog | Textures | Dark resonant pad breathing with a slow filter LFO. |
| Orbit Pad | Grain | Hybrid, MPE | Harp grains orbiting an FM layer, ring-modulated softly and panning over two bars. |
| Piano Grains | Grain | Textures, Piano, MPE | Grand piano recordings frozen into a slow-moving grain cloud: a piano that breathes. |
| Rise Bloom | FM | – | Shimmering FM pad blooming up from an octave below; long and bright. |
| Rise Choir | Vocal | Vocal | Airy 'ah' choir that scoops up a fourth into every chord. |
| Rise Glass | Wavetable | Textures | Wide glass pad: each note glides up a fifth into place, dreamy and slow. |
| Rise Strings | Analog | – | Ensemble strings whose notes rise an octave up into the chord on every new note. |
| Rise Warmth | Cluster | – | Soft detuned saw stack whose notes lean up a whole tone into the chord. |
| Slide Choir | Vocal | Vocal | Airy 'ah' choir; each voice slides to the next chord. |
| Slow Motion Choir | Vocal | Vocal, MPE | Ooh choir that swells in slowly, chorused and wide, in a long cathedral hall. |
| Spectral Morph | Wavetable | Hybrid, MPE | Formant table morphing under a breathy vocal layer, both through a drifting comb: spectra that melt into each other. |
| Super Stack | Cluster | – | Seven detuned saws per key, wide and lush. |
| Sus Horizon | Cluster | – | Suspended-second stacks that never resolve; ambient beds. |
| Tidal Pad | Analog | MPE | Warm saw pad whose filter rolls in and out over two bars like a tide. |
| Tine Bloom | Grain | Textures, Piano, MPE | Tine-piano grains blooming slowly, chorused, with a dotted echo. |
| Vowel Pad | Wavetable | – | Formant table drifting between vowels; slow attack. |
| Warm Horizon | Cluster | MPE | Soft, wide add-nine pad with gentle drift: warm and open, sits behind everything. |
| Warm Strings | Analog | – | Ensemble strings: detuned saws, an octave up, gentle chorus. |

### Leads (32)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Breath Lead | Vocal | Hybrid, MPE | Breathy sung tone fused with a soft saw stack: a lead between a voice and a synth. |
| Chip Square Lead | Wavetable | MPE | Narrow pulse lead with a crunchy bit crusher and a quick vibrato - retro, but wide. |
| Classic Mono Lead | Analog | – | Saw lead with a little vibrato, glide and a dotted delay. |
| Digital Lead | Wavetable | – | Warped Digital table with unison: modern and cutting. |
| Dream Lead | Wavetable | Pads, MPE | Soft triangle lead with a slow flanger and a long, airy reverb. |
| FM Brass Lead | FM | – | Brassy FM lead: the index rises with a short swell. |
| Fall Lead | Analog | FX | Square lead that drops a fifth from above into every note. |
| Formant Lead | Wavetable | Vocal | Vowel-like lead that opens up with the envelope. |
| Growl Lead | Wavetable | MPE | Biting FM-formant lead that growls with an LFO: a bass-music lead for the drop's answer phrase. |
| Hot Wire | Analog | – | Driven, resonant saw for rock-ish solos. |
| Hyper Saw Lead | Cluster | MPE | Huge detuned saw stack squashed upfront, with a stereo delay: festival main-room energy. |
| Legato Glide Lead | Analog | – | Mono saw lead with legato portamento: overlapping notes glide, detached notes do not retrigger a slide. |
| Mono Glide Lead | Wavetable | MPE | Expressive mono lead with legato glide, light drive and a dotted ping-pong delay. |
| Octave Lead | Cluster | – | Octave-stacked pulses with a quick filter blip. |
| Pluck Lead Wide | Wavetable | Plucks, MPE | Snappy plucked lead: a quick filter snap, stereo stack and a dotted delay. |
| Pop Lead | Analog | – | Bright saw and an octave square with vibrato: a clear, singing synth-pop lead. |
| Portamento Square | Analog | – | Hollow square that always glides, at a constant rate: big jumps slide longer than small steps. |
| Recorder | Sampler | – | Recorded alto recorder, looped, with light vibrato. |
| Ring Machine | Analog | Hybrid, MPE | Saw lead ring-modulated by an FM sine a fifth up: metallic, vocal and aggressive, with delay. |
| Rise Lead | Analog | – | Saw lead that swoops up an octave into every note; a dotted delay behind it. |
| Saw Unison Lead | Wavetable | – | Classic table at the saw position with wide unison. |
| Screamer Lead | Analog | MPE | Resonant, overdriven saw lead that screams on high notes; ping-pong delay keeps it wide. |
| Shimmer Lead | FM | Keys, MPE | Glassy FM lead with a bell edge, chorus and a long bright hall. |
| Sine Whistle | FM | – | Almost pure sine with a breath of modulation; delicate melodies. |
| Soft Flute Lead | Analog | – | Breathy, resonant low-pass on a square: a soft whistle tone. |
| Square Hero | Analog | – | Hollow square plus an octave: a bright, singing lead. |
| Stereo Saw Lead | Analog | MPE | Classic two-saw lead made wide: chorus, Haas spread and a short room. |
| Super Lead | Cluster | – | Seven-saw unison lead: festival size. |
| Super Wide Lead | Wavetable | MPE | Eight-voice saw stack spread across the stereo field, chorus, ping-pong delay and a big hall. |
| Twist Lead | Wavetable | MPE | Twisted, sync-like lead: the envelope bends the warp, the stack spreads it wide. |
| Vox Lead | Vocal | Vocal | Solo synthetic voice with expressive vibrato and glide. |
| Vox Synth Lead | Vocal | Vocal, MPE | Sung synth lead: an 'ah' with vibrato, widened by a Haas spread, with delay. |

### Plucks (20)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Concert Harp | Sampler | – | Recorded concert harp (eight notes, CC0). |
| Echo Harp | Pluck | MPE | Plucked nylon-like string with dotted ping-pong echoes and a wide room. |
| FM Koto | FM | – | Plucked FM: a bright attack that quickly mellows. |
| Future Chord | Cluster | Keys, MPE | Wide minor-seventh chord stab with squash and a short shimmer - one key, one chord. |
| Glass Pluck | Wavetable | – | Glass table pluck with a bell-like sparkle. |
| Harp Pluck | Pluck | – | Long-ringing harp-like string with a soft attack. |
| Kalimba | Sampler | Keys | Recorded thumb piano with its buzzy, woody tines (CC0). |
| Koto Pluck | Pluck | – | Bright, twangy string with a resonant body. |
| Muted Pick | Analog | – | Short palm-muted square pluck for fast lines. |
| Nylon Guitar | Pluck | – | Warm nylon string plucked near the soundhole. |
| Pluck Nebula | Pluck | Hybrid, MPE | A plucked string that dissolves into a kalimba grain cloud: the string fades, the cloud swells and lingers. |
| Poly Pluck | Analog | – | Classic synth pluck: snappy filter on a detuned saw pair. |
| Resonator Hit | Pluck | Hybrid, MPE | A struck metal bar feeding a short grain resonance and a tuned comb: percussive, ringing, strange. |
| Steel Drum FM | FM | Keys | Tropical pan tones from inharmonic FM. |
| Steel String | Pluck | – | Bright steel string plucked near the bridge. |
| Stereo Pluck | Wavetable | MPE | Bright six-voice pluck spread wide with a dotted ping-pong delay and hall. |
| Table Pluck | Wavetable | – | Wavetable pluck: bright saw frame closing fast. |
| Wide Glass Pluck | FM | Keys, MPE | FM glass pluck with a Haas spread and a bright, airy reverb. |
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

### FX (17)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Alien Signal | FM | – | Random-stepping FM timbre: radio chatter from far away. |
| Detune Swarm | Cluster | Textures | Seven wildly drifting oscillators: a swarm. |
| Downlifter | Analog | – | Falling noise and saw sweep over three seconds with a jet flanger - the end of the build. |
| Glitch Spray | Grain | Textures | Tiny, fast grains crushed and scattered into a stereo delay: digital spray. |
| Gong Hit | FM | Textures | Inharmonic FM gong with a long shimmer. |
| Granular Rise | Grain | – | Grains scan forward and climb while the key is held. |
| Grit Sweep | Wavetable | – | Noisy Grit table opening over two seconds. |
| Impact Boom | FM | Boom | Sub impact with a metallic crack, squashed and thrown into a huge hall. |
| Laser Drop | Analog | Drums | Resonant falling sweep; short hits become lasers. |
| Noise Riser | Analog | – | Hold a key: filtered noise climbs for four seconds. |
| Reverse Spray | Grain | Textures | Mostly reversed harp grains sprayed across the stereo field. |
| Siren Pan | FM | – | Wailing FM siren whose pitch swings and pans across the speakers. |
| Stereo Zap | Wavetable | – | Laser zap diving two octaves, thrown left and right by a ping-pong delay. |
| Stutter Glitch | Grain | – | Tiny, sparse grains: glitchy stutters from the marimba. |
| Table Scanner | Wavetable | Textures | Digital table swept by a slow LFO: robotic movement. |
| Whoosh | Analog | – | Filtered noise swept by a one-bar band-pass wobble, spread wide: a passing whoosh. |
| Wide Riser | Analog | – | Noise and saws rising over four seconds, flanged and wide, into a long hall. |

### Arp (18)

Presets marked *Arp:* switch the arpeggiator on for the part they load into (latched: press a key once, it keeps playing; press a new key or chord to change it). Loading a preset without arp into that part switches it off again. See [PRESETS.md](PRESETS.md#arp-block).

| Name | Engine | Also in | Description |
|---|---|---|---|
| Analog Arp | Analog | Plucks | Resonant square blips with a ping-pong delay. *Arp: Up/Down · 1/16 · 2 oct · latch* |
| Bell Cascade Arp | FM | Keys | FM bells cascading down three octaves in triplets, with echoes and a bright hall. *Arp: Down · 1/16T · 3 oct · latch* |
| Bubble Arp | Analog | – | Narrow pulse through a resonant filter: bubbly and round. *Arp: Up · 1/16 · 2 oct · latch* |
| Chord Stab Arp | Cluster | Keys | Rhythmic chord stabs (chord mode, eighths) from a wide saw stack - instant house groove. *Arp: Chord · 1/8 · 1 oct · latch* |
| Digital Pluck Arp | FM | – | Clicky digital pluck that loves fast sixteenths. *Arp: Up · 1/16 · 2 oct · latch* |
| Dream Tine Arp | Sampler | Keys | Tine piano arpeggio in eighths with chorus, delay and hall: dreamy and full. *Arp: Up/Down · 1/8 · 2 oct · latch* |
| FM Arp Bell | FM | Keys | Short glassy FM bells for sequences. *Arp: Up/Down · 1/16 · 2 oct · latch* |
| Formant Arp | Wavetable | Vocal | Vowel blips for talking sequences. *Arp: Up · 1/16 · 2 oct · latch* |
| Glock Arp | Sampler | Keys | Recorded glockenspiel for sparkling arpeggios (CC0). *Arp: Up · 1/16 · 2 oct · latch* |
| Mallet Echo Arp | Sampler | Plucks | Glockenspiel arpeggio with long dotted echoes - sparkling and wide. *Arp: Up · 1/8 · 3 oct · latch* |
| Neuro Arp | Wavetable | Bass | Growling FM sixteenths in one octave: a rolling neuro sequence from a single held note. *Arp: As Played · 1/16 · 1 oct · latch* |
| Pizzicato Arp | Pluck | Plucks | Short plucked strings for ostinatos. *Arp: Up/Down · 1/16 · 2 oct · latch* |
| Random Spark Arp | FM | FX | Random thirty-second FM blips over four octaves through a ping-pong delay: sparks. *Arp: Random · 1/32 · 4 oct · latch* |
| Ripple Arp | Analog | – | Analog saw arp in sixteenth triplets with a sweeping phaser and stereo echo. *Arp: Up · 1/16T · 2 oct · latch* |
| Sparkle Bell | FM | Plucks | Short, bright FM bell for sparkling arpeggios over chords. *Arp: Up · 1/16 · 2 oct · latch* |
| Table Arp | Wavetable | – | Wavetable sequence sound: the position snaps with each note. *Arp: Up/Down · 1/16 · 2 oct · latch* |
| Trance Gate Arp | Cluster | Leads | Supersaw stack on a sixteenth arp over two octaves, ping-pong delay and hall. *Arp: Up · 1/16 · 2 oct · latch* |
| Wide Pluck Arp | Wavetable | Plucks | Six-voice stereo pluck, up / down over three octaves, dotted ping-pong delay. *Arp: Up/Down · 1/16 · 3 oct · latch* |

### Textures (24)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Backwards Kalimba | Sampler | FX | The kalimba played in reverse: swells that end in a pluck. |
| Breath Texture | Vocal | Vocal | Nearly unvoiced breath through slowly moving vowels. |
| Breathing Machine | Vocal | Vocal, MPE | Breathy vocal drone whose vowel opens and closes over two bars. |
| Dark Matter | Wavetable | Pads, MPE | Low Grit table with a crawling phaser and a slowly moving position - ominous. |
| Deep Space Drone | Cluster | Pads, MPE | Fourths stacked low, a drifting comb on top and an endless hall. |
| Dust Orchestra | Grain | MPE | Zither grains thrown backwards and forwards, a stereo delay trail - an orchestra of dust. |
| FM Grains | Grain | Hybrid, MPE | Bowed-metal grains ring-modulated by an FM bell layer: grains that clang in tune. |
| Fourths Drift | Cluster | Pads | Stacked fourths with heavy drift: an unresolved haze. |
| Glacier | Grain | MPE | Glass grains crawling at a glacial scan rate, very wide, endless reverb. |
| Glass Swarm | Grain | Pads, MPE | Dense cloud of singing-glass grains, eight voices deep, chorused into a huge hall. |
| Grit Drone | Wavetable | – | Slowly churning Grit table; hold one low note. |
| Harp Cloud | Grain | Pads, MPE | Harp recordings scattered into a dense cloud with ping-pong echoes. |
| Harp Mist | Grain | Pads | Long, soft harp grains blurred into a mist. |
| Kalimba Dust | Grain | – | Tiny kalimba grains scattered like dust. |
| Kalimba Storm | Grain | MPE | Kalimba grains at full density with reversed fragments - a rattling, tuned storm. |
| Marimba Rain | Grain | – | Sparse marimba grains falling like rain. |
| Metal Choir | Grain | Pads, MPE | Bowed-metal grains stacked into a choir; slow scan, shimmering stereo. |
| Night Rain | Grain | MPE | Random kalimba droplets falling into a ping-pong delay and a dark hall. |
| Recorder Breath | Grain | Vocal | Breathy recorder grains: airy, vocal-like tones. |
| Rust Texture | Grain | FX, MPE | Bowed metal grains played backwards through a slow comb: rusty, creaking, alive. |
| Singing Glass | Sampler | Pads | Rubbed wine-glass recordings (CC0), looped: pure and haunting. |
| Starfield | FM | Keys, MPE | Random high FM bells drifting across the stereo field into delay and hall. |
| Vinyl Crackle | Analog | FX | Record crackle and soft hiss (Analog engine: oscillators off, crackle and noise on); hold a note under a beat. |
| Zither Cloud | Grain | – | Shimmering zither grains, octave sprinkles. |

### Vocal (9)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Aah Choir | Vocal | Pads | Mixed choir singing 'ah'. |
| Choir Grains | Vocal | Hybrid, MPE | Ah choir doubled by a recorder grain cloud an octave up - human and synthetic at once. |
| Formant Voices | Wavetable | Pads | Formant wavetable with unison: synthetic choir. |
| Gospel Hum | Vocal | – | Closed-mouth hum: warm, low and intimate. |
| Male Ensemble | Vocal | Pads | Low voices on 'oh': a men's choir. |
| Ooh Choir | Vocal | Pads | Soft 'oo' choir, round and dark. |
| Solo Soprano | Vocal | Leads | High solo voice: bright formants, generous vibrato. |
| Talking Synth | Vocal | Leads | The LFO walks through the vowels: a talking lead. |
| Vowel Morph | Vocal | – | Each note sings 'ee' and opens to 'ah' with the envelope. |

### Boom (11)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Boom Click | Analog | Bass | Sine sub with a bright click on the attack, so the boom cuts through on small speakers. |
| Boom Distorted | Analog | Bass | Sine sub driven hard into the ladder filter and the FX slot: a growling, distorted boom. |
| Boom Kick Bass | Analog | Drums, Bass | Short boom: a two-octave pitch dive into a tuned sine - kick and bass in one. |
| Boom Long Sub | Analog | Bass | Long, tuned sine sub with a quick pitch dive on the attack; rings out for seconds. |
| Boom Slide | Analog | Bass | Long sine sub made for trap-style slides: overlap notes and it glides (legato, constant time). |
| Dirty Sub | Analog | Bass | Sub driven into the ladder filter: growling harmonics that stay audible on small speakers. |
| Impact Boom | FM | FX | Sub impact with a metallic crack, squashed and thrown into a huge hall. |
| Long Sub | Analog | Bass | Deep, long-decaying sub that slides between overlapping notes (glide); a soft click on the attack. |
| Punch Sub | Analog | Bass | Short, punchy sub with a snappy filter click; locks in with the kick. |
| Sine Slide | FM | Bass | Nearly pure sine sub with a long glide for sliding bass lines. |
| Sub Kick | Analog | Drums, Bass | Tunable deep kick: a self-oscillating filter tuned to the key, swept down from above; long boom, driven. |

### Hybrid (18)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Breath Lead | Vocal | Leads, MPE | Breathy sung tone fused with a soft saw stack: a lead between a voice and a synth. |
| Choir Grains | Vocal | Vocal, MPE | Ah choir doubled by a recorder grain cloud an octave up - human and synthetic at once. |
| Chop Pad | Cluster | Pads, MPE | A wide chord pad chopped and stuttered by the envelope of a kalimba grain layer. |
| Crystal Organ | FM | Keys, MPE | Additive FM organ with a glass wavetable layer, circling in stereo with a slow autopan. |
| Digital Monster | Wavetable | Bass, MPE | Digital FM table gated by a glass-grain layer an octave down, then wobbled: unstable and huge. |
| FM Grains | Grain | Textures, MPE | Bowed-metal grains ring-modulated by an FM bell layer: grains that clang in tune. |
| Fold Bass | Wavetable | Bass, MPE | Sine sub and an analog saw folded into each other: the fold amount makes it roar. |
| Glass Engine | Grain | Pads, MPE | Singing-glass grain cloud fused with a harmonic wavetable layer that scans slowly; a touch of ring modulation. |
| Halo Grand | Sampler | Keys, Piano, MPE | Grand piano with a halo: its own recording frozen into a grain cloud that blooms behind every chord. |
| Metal Strings | Pluck | Pads, MPE | Metallic plucked strings folded into a slow cluster pad - an orchestra made of wire. |
| Neuro Hybrid | FM | Bass, MPE | FM growl and a wavetable reese ring-modulating each other, folded and squashed: a neuro monster. |
| Orbit Pad | Grain | Pads, MPE | Harp grains orbiting an FM layer, ring-modulated softly and panning over two bars. |
| Pluck Nebula | Pluck | Plucks, MPE | A plucked string that dissolves into a kalimba grain cloud: the string fades, the cloud swells and lingers. |
| Resonator Hit | Pluck | Plucks, MPE | A struck metal bar feeding a short grain resonance and a tuned comb: percussive, ringing, strange. |
| Ring Machine | Analog | Leads, MPE | Saw lead ring-modulated by an FM sine a fifth up: metallic, vocal and aggressive, with delay. |
| Spectral Morph | Wavetable | Pads, MPE | Formant table morphing under a breathy vocal layer, both through a drifting comb: spectra that melt into each other. |
| Tape Ghost Keys | Sampler | Keys, Piano, MPE | Tine piano with a ghost of itself: a detuned grain copy, crushed and flanged like an old resampled tape. |
| Vox Reese | Wavetable | Bass, MPE | A reese with a singing vocal layer on top: the formants ride the bass, a synced vowel filter makes it talk. |

### Piano (14)

| Name | Engine | Also in | Description |
|---|---|---|---|
| Bright Grand | Sampler | Keys, MPE | Grand piano opened up for pop: brighter, lightly squashed, short room. |
| Classic Tine | Sampler | Keys, MPE | Modelled tine electric piano: bell-like when soft, barking when hit hard. |
| Concert Grand | Sampler | Keys, MPE | Grand piano recordings, two velocity layers, a touch of hall. |
| Dream Tine | Sampler | Keys, Pads, MPE | Tine piano drenched in chorus, dotted delay and a long hall. |
| FM Bark Piano | FM | Keys, MPE | Two-pair FM electric piano: the tine pair rings, the bark pair bites with velocity. |
| Felt Grand | Sampler | Keys, MPE | Dark, soft grand: the top rolled off, a slow release and a warm hall - intimate. |
| Halo Grand | Sampler | Keys, Hybrid, MPE | Grand piano with a halo: its own recording frozen into a grain cloud that blooms behind every chord. |
| Piano Grains | Grain | Pads, Textures, MPE | Grand piano recordings frozen into a slow-moving grain cloud: a piano that breathes. |
| Reed Keys | Sampler | Keys, MPE | Modelled reed electric piano: nasal and woody, with a gritty bark when driven. |
| Reed Tremolo | Sampler | Keys, MPE | Reed piano through its classic amplitude tremolo - soul and gospel. |
| Suitcase Tine | Sampler | Keys, MPE | Tine piano with the stereo autopan of a suitcase amp and a small room. |
| Tape Ghost Keys | Sampler | Keys, Hybrid, MPE | Tine piano with a ghost of itself: a detuned grain copy, crushed and flanged like an old resampled tape. |
| Tine Bloom | Grain | Pads, Textures, MPE | Tine-piano grains blooming slowly, chorused, with a dotted echo. |
| Upright Keys | Sampler | Keys, MPE | Upright piano recordings with a little room: honest and close. |

### Projects / demo patterns (14)

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
| Neuro Engine | Bass, Drums, Hybrid | Neurofunk at 174 BPM in F minor: a two-step beat on the Hybrid kit (Orbit), Neuro Reese riffing on the step sequencer, Growl Machine answering from Euclid and a dark seventh-chord pad underneath. Mono low end, squashed and loud. Press Play. |
| Halftime Pressure | Bass, Drums | Modern dubstep at 140 BPM in E minor: half-time drums on the Studio kit (Orbit), Wub Machine on the step sequencer - its eighth-note wobble restarts with every note - Vowel Growl answering from Euclid and a cold glass texture. Press Play. |
| Liquid Roll | Bass, Drums, Keys | Liquid drum & bass at 174 BPM in D minor: a rolling beat on the Tight kit (Orbit), dreamy tine-piano seventh chords and a warm pad (Orbit lanes 3 and 4), a smooth sliding sub (step sequencer) and glassy plucks from Euclid. Press Play. |
| Evolving Space | Pads, Textures, Hybrid | Ambient at 66 BPM in A minor: Evolving Tide chords drifting through their wavetable (step sequencer), a generative grand piano with a grain halo from three Orbit lanes of 5, 7 and 11 steps, a glacial grain drone and random starfield bells (Euclid). Let it run. |

**Engines:** FM 40, Wavetable 72, Analog 65, Pluck 13, Grain 27, Vocal 18, Cluster 21, Sampler 26
<!-- presets:end -->
