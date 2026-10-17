# Werksklänge

*Deutsch · English: [../en/FACTORY-SOUNDS.md](../en/FACTORY-SOUNDS.md)*

Ohpe bringt **282 Part-Presets** für alle acht Engines mit, **14 Projekt-Presets** (Demo-Patterns,
die alle vier Parts, die Sequenzer und das Drum-Kit nutzen), **sechs Drum-Kits**, **zehn
aufgenommene Instrumente** und (0.8) **vier Tasteninstrumente** (Flügel, Klavier, Tine- und
Zungen-E-Piano) für die Sampler- und Grain-Engine. Du öffnest sie mit **BIBLIOTHEK**:
links eine Kategorie wählen (Bass, Tasten, Flächen, Leads, Zupfer, Drums, FX, Arp, Texturen, Vokal, Boom, Hybrid, Klavier)
oder eine Engine, dann durch die Liste klicken – jedes Preset lädt sofort, du hörst es beim Stöbern.

## Presets spielen

* Ein **Part-Preset** lädt in den bearbeiteten Part (Part-Wahl 1–4). Sein **Part-Gain**
  (`pN.gain`, dB) gleicht die Lautheit an die anderen Werksklänge an: alle liegen beim Spielen bei
  etwa −18 … −12 dBFS RMS, beim Umschalten springt der Pegel also nicht.
* Das **FX-Rack** (bis zu sechs Effekte pro Part, siehe [BAND-MISCHER-FX.md](BAND-MISCHER-FX.md))
  gehört zum Preset und **jeder Part** hat es, ein Preset klingt also in jedem Part gleich. (Die
  Projekte von vor 0.8 lassen ihre Parts 2–4 trocken, wie sie gebaut wurden.)
* Die Makros auf den vier Drehreglern sind die ersten vier Parameter der Engine; die meisten Presets
  sind so gebaut, dass Drehen musikalisch klingt (Cutoff, Klang, Position, Korngröße …).
  Modulationshüllkurve und LFO gehören zu jedem Preset.
* Presets mit einem aufgenommenen Instrument (Sampler, Grain) brauchen die Werks-Samplebank, die im
  Plug-in und in der App eingebaut ist.

## Projekt-Presets (Demo-Patterns)

Ein **Projekt-Preset** ersetzt das ganze Instrument: vier Parts, Drum-Kit, Mixer, Effekte, Tempo
und die Patterns. **Play** (Sequencer Start) drücken. Sie zeigen, wie die Sequenzer zusammenspielen:
Step-Sequenzer, Euklid und Orbit spielen jeweils einen Part – oder seit 0.4 das **Drum-Kit**
(`seq.step.drums`, `seq.euclid.drums`, `seq.orbit.drums`). Die vier Spuren von Orbit sind eine
natürliche Drum-Machine (eine Spur pro Trommel, jede mit eigener Länge); die vier Ringe von Euklid
spielen polyrhythmische Drums. Parts ohne Sequenzer bleiben zum Mitspielen frei.

## Sound-Revision 3 (0.8)

120 neue Part-Presets und vier Projekte, gemacht für moderne Produktionen und zum Spielen:

* **Bassmusik:** Reese-Bässe (klassisch verstimmt, gephast, geflangt, hohler Kammfilter, verzerrter
  Neuro-, Vokal- und bewegter Reese) auf dem neuen Unisono-Stapel der Wavetable-Engine (bis zu acht
  Stimmen) mit monotauglichem Tiefbass (*Bass mono* unter 150 Hz nach dem FX-Rack); Drum-&-Bass-Subs
  und Roller; Wobble- und Growl-Bässe mit dem tempo-synchronen *Wobble* (startet mit jeder Note neu),
  dem Vokalfilter *Formant*, FM auf der Wavetable, Wavefolding (*Verzerrung*) und *Multiband*-Squash –
  Ketten aus bis zu sechs Effekten pro Preset.
* **Leads** mit Stereo-Stapeln, Delays und Hallräumen; **Stereo-Effekte** (Riser, Impacts, Zaps,
  Whooshes).
* **Pianos** (Kategorie *Klavier*): Flügel- und Klavier-Aufnahmen, modellierte Tine- und
  Zungen-E-Pianos (Koffer-Autopan, Tremolo, Traum-Chorus) und ein FM-E-Piano mit Anschlags-Bellen.
* **Dichtere Grain-Klänge und vollere Arps:** acht Grain-Stimmen mit bis zu 95 Grains pro Sekunde,
  Instrumente und Pianos als Quelle, Echos und Hall; Arps mit Stapeln, Ping-Pong-Delays und Hall.
* **Sich entwickelnde Atmosphären:** langsame Wavetable-Scans, Zwei-Takt-Autopans und
  Filter-Gezeiten, driftende Kammfilter und endlose Hallräume.
* **Hybrid** (Kategorie *Hybrid*, 18 Presets): zwei Engines in einem Part mit der neuen
  **Layer-Engine** und Kreuzmodulation ([engines/layer.md](engines/layer.md)) – ein Zupfton, der sich in
  eine Grain-Wolke auflöst, ein Reese mit singendem Vokal-Layer, ringmodulierte FM-Grains, ein
  Flügel mit Heiligenschein aus seiner eigenen eingefrorenen Aufnahme, eine Fläche, die eine
  Grain-Hüllkurve zerhackt.
* **Bewegung:** jedes neue Preset bewegt sich, wenn eine Note gehalten wird (langsame LFOs auf
  Wavetable-Position, Filter oder Panorama, synchrone Effekte).
* **MPE:** 93 der neuen Presets tragen den freien Tag **MPE**: Druck, Timbre und Pitchbend-Slides pro
  Note sind passend zum Klang zugeordnet (siehe [MIDI.md](MIDI.md)); Druck wirkt auch mit jeder
  Aftertouch-Tastatur.

Neue Projekte: **Neuro Engine** (Neurofunk, 174 BPM), **Halftime Pressure** (Dubstep, 140 BPM),
**Liquid Roll** (Liquid Drum & Bass, 174 BPM), **Evolving Space** (Ambient, 66 BPM).

## Boom (0.6)

**Boom** ist eine eigene Kategorie (Bibliotheks-Chip *Boom*): tiefe, gestimmte Kicks und Sub-Bässe –
das synthetisierte **Boom**-Kit (Projekt *Boom Kit*), *Sub Kick*, die Sub-Bässe (*Long Sub*,
*Dirty Sub*, *Punch Sub*, *Sine Slide*) und fünf neue auf einem reinen Sinus-Sub mit Pitch-Dive (die
Tonhöhen-Hüllkurve aus 0.6): *Boom Long Sub* (lang, gestimmt, mit Dive), *Boom Distorted* (hart
angezerrt), *Boom Kick Bass* (kurz: Kick und Bass in einem), *Boom Slide* (überlappende Noten
gleiten – Legato-Portamento, für Slides im Trap-Stil) und *Boom Click* (ein heller Klick, damit er
auch auf kleinen Lautsprechern durchkommt). In der Tonart des Songs spielen; *Stimmung* und das
Amp-*Decay* bestimmen die Länge. Das Projekt *Sub & Glide* trägt ebenfalls den Tag Boom.

## Lo-Fi (0.6)

Das Projekt **Lo-Fi Beat** ist klassischer Lo-Fi-Hip-Hop: 78 BPM mit trägem, schwerem Swing, dumpfe
Kick, weiche Snare und leise geshuffelte Hats auf dem Lo-Fi-Kit, grundtonlose Nonen-Voicings (Fm9 –
Bb13 – Ebmaj9 – C7#9) auf **Dusty Keys** (starkes Band-Leiern, bitreduzierte Kante), ein warmer,
runder Bass (**Mellow Lofi Bass**) auf den Grundtönen und **Vinyl Crackle** darunter; der Master wird
vom Master-Tiefpass aus 0.6 (4,2 kHz) und Bandsättigung abgedunkelt. **Lofi Kit** ist dasselbe
Kit-Setup als Ausgangspunkt für eigene Beats. *Vinyl Crackle* ist die Analog-Engine mit
ausgeschalteten Oszillatoren (0.6 `analog.osclevel`) und dem neuen Knister-Generator
(`analog.crackle`) – eine beliebige Note halten.

## Leben (p15)

Acht Part-Presets nutzen **LEBEN**, damit gehaltene Töne weiterleben: *Analog Dawn*, *Warm Strings*, *Angel Pad*,
*Evolving Tide*, *Crystal FM Pad*, *Bowed Haze*, *Warm Horizon* (15 – 40 %) und *Dusty EP* (ein Hauch, 15 %).
LEBEN auf 0 ergibt die statische Fassung ([BEDIENUNG.md](BEDIENUNG.md#leben)); die Liste steht in
`tools/presets/factory_parts_life.py`.

## Modulationsrad (p29)

Jedes Werks-Preset reagiert auf das **Modulationsrad** (das MOD-Rad auf dem Bildschirm, CC 1 vom Controller): eine
Zuweisung der Modulationsmatrix des Parts mit der Quelle (oder Via) *Modulationsrad*, passend zur Rolle des Klangs.
Mit dem Rad unten klingt jedes Preset genau wie vorher.

| Klänge | Rad hoch |
|---|---|
| Leads, Solostimmen, Orgeln, Bläser | Vibrato (etwa ±0,35 Halbtöne; LFO 1, wo er schon im Vibrato-Tempo läuft, sonst LFO 2 mit 5,3 Hz und kurzem Einblenden) |
| Tines, Reeds, Vibes mit Auto-Pan | tieferes Auto-Pan / Tremolo (LFO 1 Tiefe) |
| Flächen, Bässe, Zupfer, Tasten, FX | Filter / Klang öffnet sich (Klangfarbe: Cutoff, FM-Index, Zupf-Helligkeit); wo er schon ganz offen ist (aufgenommene Klaviere und Instrumente bei vollem Klang), schließt er sich – weicher, dunkler; angeschlagene Stäbe und Platten werden härter angeschlagen |
| Grain-Flächen, Texturen und FX | Korn-Streuung |
| Chöre der Vokal-Engine | der Vokal wandert |
| Subs, Booms, Kicks | härterer, angezerrter Charakter (Charakter Hart) |
| Arps mit Delay / Echo | Echo-Wurf (längeres Delay- / Echo-Feedback, höchstens 0,75) |
| Morph-Presets | Morph X (schon bisher, über die Rad-Steuerung des Morph-Pads) |
| Projekt-Presets | jeder Part nach seiner Engine (Streuung, Vokal, Klang) |

Die Zuweisung liegt im ersten freien Matrix-Slot und lässt sich in der Mod-Ansicht ändern oder entfernen. Die Mengen
wurden mit internen Renderings geprüft: mit dem Rad ganz oben wird kein Preset mehr als 3 dB lauter oder übersteuert.
Das Skript ist `tools/presets/add_modwheel.py`; die Core-Tests prüfen, dass jedes Werks-Preset eine Rad-Zuweisung hat
und das Rad ganz oben den Klang sauber verändert.

## Showcase-Presets (p44)

48 Part-Presets und 4 Projekte rund um die neueren Funktionen: Leben, das Morph-Pad, eigene Wavetables, analysiert aus den Werksaufnahmen, der Resonanzkörper (Stufe und Rack-Effekt), die Charakter-Makros, die 3D-Bewegungspfade (am besten mit Kopfhörern), die SUB-Ebene, die Ausdrucks-Seite (Anschlag / Aftertouch), Glide-Druck und tempo-synchrone Bewegung (LFO 2 / 3 und in Takten bemessene Bewegungspfade folgen dem Tempo, ob das Metronom klickt oder nicht). Die Projekte zeigen, was nur ein Projekt trägt: das Folgen des Harmonie-Gehirns, aufgenommene Bewegungsspuren, die Einstellungen der Performance-Pads und vier 3D-Parts gleichzeitig. Jedes ist auf den Werks-Median eingepegelt, hat ein musikalisches Modrad und reagiert auf Anschlag; nur die Glide-Presets gleiten. Sie stehen in der Bibliothek bei den anderen Werks-Presets (Suche nach einer Funktion, z. B. *3D* oder *Morph*).

<!-- showcase:begin -->
| Name | Kategorie | Engine | Funktionen | Beschreibung |
|---|---|---|---|---|
| Body Slap Bass | Bass | Zupfen | Resonanzkörper, Ausdruck | Ein gezupfter Saitenbass, der einen kurzen Holzkörper auf jeder Note mitschwingen lässt: runde, holzige Schläge. Anschlag schlägt härter und heller an; das Modrad macht den Körper von Holz zu Metall. |
| Living Reese | Bass | Wavetable | Leben, Charakter-Makros, SUB-Ebene | Ein breiter Reese, dessen Unisono-Stimmen nie zur Ruhe kommen: Leben lässt jede Stimme eigen driften, Lebendig und Weit ziehen den Stapel auseinander, ein Mono-Sinus-Sub hält das Fundament. Das Modrad öffnet den Wavetable-Ton. |
| Pressure Glide Bass | Bass | Analog | Glide-Druck, Ausdruck | Ein Legato-Monobass, der zwischen überlappenden Noten gleitet. Druck dehnt das Gleiten (Glide-Druck): leicht gespielt bleibt es knapp, mit Druck wird es zum langsamen Bogen. Das Modrad öffnet das Filter. |
| Round Square Sub | Bass, Boom | FM | SUB-Ebene, Ausdruck | Ein tiefer FM-Sinusbass, dessen Gewicht vom weichen Rechteck der SUB-Ebene eine Oktave tiefer kommt: hohler Druck in den unteren Mitten, sauber auf großen Anlagen. Das Modrad bringt FM-Körnung, der Anschlag bestimmt den Punch. |
| Sub Pressure Bass | Bass | Analog | SUB-Ebene, Ausdruck, Charakter-Makros | Ein straffer Sägezahnbass über einem sauberen Sinus-Sub zwei Oktaven tiefer, der unter dem geschlossenen Filter voll bleibt. Anschlag beißt ins Filter, Druck öffnet es; das Modrad macht Attack und Drive härter. |
| Tempo Wobble Sub | Bass | Analog | Tempo-synchron, SUB-Ebene | Ein knurrendes Filter-Wobble, fest im Tempo (LFO 2 in Achteln, mit jeder Note neu gestartet), über einem ruhigen Mono-Sub, der nie mitwackelt. Das Modrad vertieft das Wobble. |
| Alive Tine Keys | Tasten, Klavier | Sampler | Charakter-Makros, Leben, Ausdruck | Das aufgenommene Zungen-E-Piano mit den Charakter-Makros Richtung Lebendig und etwas dunkler: langsame Drift, ein lebendiger Ton und ein weiches Stereo-Aufblühen. Anschlag hellt auf, das Modrad vertieft das Tremolo. |
| Breathing Organ | Tasten | Analog | Leben, Charakter-Makros | Eine zugriegelartige Orgel, die nie ganz stillsteht: Leben lässt jede Stimme ein paar Cent driften und den Ton atmen, wie ein altes Instrument, das warm wird. Das Modrad beschleunigt den kreisenden Chorus. |
| Morph Living Keys | Tasten, Morph | Analog | Morph-Pad | Vier Showcase-Tasteninstrumente auf vier Engines: die atmende Orgel zu Hause, rechts die Harfe mit ihren Resonanzsaiten, darüber das FM-Piano, in der fernen Ecke die angeschlagenen Platten. Modrad = rechts, Druck = hoch. |
| Piano Scan Table | Tasten | Wavetable | Eigene Wavetable, Ausdruck | Eine Wavetable, analysiert aus dem aufgenommenen Flügel, acht Frames vom Hammer bis zum Ausklang. Die Mod-Hüllkurve fährt bei jeder Note hindurch, jede Taste beginnt hell und setzt sich wie beim Flügel; Anschlag öffnet den Ton. Das Modrad hält sie auf den hellen Frames. |
| Pressure Reed Keys | Tasten, Klavier | Sampler | Ausdruck | Das aufgenommene Zungen-E-Piano mit Stimmzungen, gemacht für Aftertouch: in einen Akkord drücken, und er knurrt (Drive) und schwebt (Vibrato); der Anschlag bestimmt das Bellen. Das Modrad bringt ein pulsierendes Auto-Pan. |
| Struck Plate Keys | Tasten | Cluster | Resonanzkörper, Ausdruck | Weiche Schlegeltasten auf kleinen, gestimmten Metallplatten: der Anschlag blüht zu Glocken-Teiltönen auf, die jeder Note folgen. Anschlag trifft die Platte härter; das Modrad lässt sie länger klingen. |
| Sympathetic Harp Keys | Tasten, Zupfer | Zupfen | Resonanzkörper, Ausdruck | Ein heller, harfenartiger Zupfer weckt Resonanzsaiten, gestimmt auf den gehaltenen Akkord: Akkorde schimmern nach dem Loslassen weiter. Anschlag zupft härter; das Modrad holt die Resonanzsaiten nach vorn. |
| Velocity Glass EP | Tasten | FM | Ausdruck | Ein FM-E-Piano, das den Händen folgt: leise Noten sind rund und blühen langsam auf, harte bellen mit gläserner Zunge; Druck gibt ein sanftes Vibrato. Das Modrad bringt ein Stereo-Tremolo. |
| Bright Wide Pad | Flächen | Wavetable | Charakter-Makros, Ausdruck | Eine Fläche, gespielt über ihre Charakter-Makros: Druck dreht Hell auf (Ton, Warp und FM öffnen gemeinsam), das Modrad dreht Weit auf (Unisono- und Layer-Breite). In Ruhe weich und schmal, riesig, wenn man drückt. |
| Halo Spiral Pad | Flächen, 3D | Wavetable | 3D-Bewegungspfad, Tempo-synchron | Eine gläserne Wavetable-Fläche, die in sechzehn Takten spiralförmig um den Kopf nach oben steigt und unten neu beginnt (Bewegungspfad Spirale aufwärts, in Takten: folgt dem Tempo). Am besten mit Kopfhörern; das Modrad beschleunigt die Spirale. |
| Living Strings | Flächen | Analog | Leben, Charakter-Makros | Eine Streicherfläche, die sich beim Halten weiterbewegt: Leben gibt jeder Stimme eigene langsame Drift und einen atmenden Ton, Lebendig bringt Verstimmungsbewegung. Das Modrad öffnet das Filter. |
| Morph Pressure Pads | Flächen, Morph | Analog | Morph-Pad | Lebendige Streicher zu Hause, die zu einer dunklen Fläche mit Sub-Fundament (Modrad), zu einer tempo-synchronen Wavetable-Gezeit (Druck) oder zu einem Chor über Resonanzsaiten (beides) werden. Modrad = rechts, Druck = hoch. |
| Overhead Swell | Flächen, 3D | Vocal | 3D-Bewegungspfad, Ausdruck | Hauchige Stimmen steigen vor dir auf, ziehen über den Kopf und landen hinter dir (Bewegungspfad Über den Kopf, vier Takte, im Tempo). Druck lässt sie anschwellen; das Modrad gibt ein Chor-Vibrato. Am besten mit Kopfhörern. |
| Recorder Breath Table | Flächen | Wavetable | Eigene Wavetable, Leben, Tempo-synchron | Eine Wavetable, analysiert aus der aufgenommenen Blockflöte, acht Frames vom hauchigen Einsatz bis zum singenden Ton, langsam in Zwei-Takt-Zyklen des Tempos durchfahren und mit Leben driftend: eine weiche, holzige Fläche. Das Modrad schiebt sie zu den singenden Frames. |
| Sub Floor Pad | Flächen | Analog | SUB-Ebene, Ausdruck | Eine dunkle, warme Fläche mit einer runden SUB-Ebene eine Oktave tiefer, die Akkorden ein Fundament gibt – ohne Bass-Part. Anschlag bringt mehr Sub; das Modrad öffnet die Fläche darüber. |
| Sympathy Choir | Flächen, Vokal | Vocal | Resonanzkörper, Leben | Ein weicher 'Ah'-Chor bringt einen Kranz von Resonanzsaiten im gehaltenen Akkord zum Summen, Leben lässt jede Stimme driften. Die Saiten klingen nach, wenn der Chor endet; das Modrad gibt ein Chor-Vibrato. |
| Tempo Tide Pad | Flächen | Wavetable | Tempo-synchron, Ausdruck | Eine warme Wavetable-Fläche, deren Ton über zwei Takte steigt und fällt und über einen Takt durch das Stereofeld wandert – beides im Tempo, passend zu jedem Songtempo. Das Modrad vertieft die Gezeiten. |
| Weave Cluster 3D | Flächen, 3D | Cluster | 3D-Bewegungspfad, Tempo-synchron | Eine breite Cluster-Fläche, die in acht Takten links-rechts und vorn-hinten um den Hörer webt (Bewegungspfad Weben, in Takten: folgt dem Tempo). Druck hebt sie über den Kopf; das Modrad verbreitert den Stapel. |
| Bowed Body Lead | Leads | Analog | Resonanzkörper, Ausdruck | Ein weicher Sägezahn streicht ein langes, offenes Rohr, auf jede Note gestimmt: ein holziger, celloartiger Lead. Druck drückt den Bogen (das Material wird holziger); das Modrad verschiebt die Streichposition. |
| Hard Edge FM Lead | Leads | FM | Charakter-Makros, Ausdruck | Ein schneidender FM-Lead, abgestimmt mit dem Makro Hart (kürzerer Attack, mehr Feedback, mehr Kante). Anschlag gibt Biss, Druck Vibrato; das Modrad treibt Hart weiter bis zum Fauchen. |
| Living Flute Lead | Leads | Vocal | Leben, Ausdruck | Eine hauchige, flötenartige Stimme, deren Tonhöhe und Atem mit Leben leicht driften, wie bei einem echten Spieler. Druck bringt Vibrato und mehr Atem, das Modrad ein weiteres Vibrato. |
| Morph Lead Characters | Leads, Morph | Analog | Morph-Pad | Eine Melodie, vier Spieler: ein gestrichenes Holzrohr zu Hause, rechts eine hauchige Flöte, darüber ein harter FM-Lead, in der fernen Ecke ein beißender Digital-Lead. Modrad = rechts, Druck = hoch. |
| Pendulum Lead | Leads, 3D | Analog | 3D-Bewegungspfad, Tempo-synchron | Ein heller Puls-Lead, der wie ein Pendel alle zwei Takte von einer Seite des Kopfes zur anderen schwingt (Bewegungspfad Pendel, im Tempo). Druck gibt Vibrato; das Modrad verlangsamt das Schwingen. Am besten mit Kopfhörern. |
| Pressure Glide Lead | Leads | Analog | Glide-Druck, Ausdruck | Ein singender Mono-Sägezahn-Lead für Legato: Druck dehnt das Gleiten (Glide-Druck), die Expression-Seite gibt beim Hineindrücken Vibrato dazu. Das Modrad öffnet das Filter. |
| Velocity Bite Lead | Leads | Wavetable | Ausdruck, Charakter-Makros | Ein digitaler Wavetable-Lead, der härter zubeißt, je härter man spielt: Anschlag öffnet den Ton und treibt ihn an (Hart), Druck gibt Vibrato. Das Modrad fügt Wavetable-FM hinzu. |
| Glass Plate Pluck | Zupfer | FM | Resonanzkörper, Ausdruck | Ein kurzer FM-Zupfer schlägt eine dünne Glasplatte an, auf die Note gestimmt: knackiger Anschlag, dann aufblühende Glocken-Teiltöne. Anschlag trifft härter; das Modrad lässt die Platte länger klingen. |
| Kalimba Body | Zupfer | Sampler | Resonanzkörper, Ausdruck | Die aufgenommene Kalimba über einem kleinen Holzkasten, der mit jeder Note mitschwingt: wärmer, runder, etwas hohl. Anschlag hellt auf; das Modrad öffnet den Kasten (mehr Körper). |
| Living Nylon | Zupfer | Zupfen | Leben, Ausdruck | Eine warme Nylonsaite mit etwas Leben: jede Note driftet und atmet ein wenig anders, wie eine von Hand gespielte Gitarre. Anschlag zupft heller; das Modrad verschiebt den Anschlag Richtung Steg. |
| Ping-Pong Space Pluck | Zupfer, 3D | Zupfen | 3D-Bewegungspfad, Tempo-synchron | Ein heller Zupfer, der jeden halben Takt von vorn nach hinten springt (Bewegungspfad Ping-Pong vorn/hinten, im Tempo), in einem kleinen Raum. Anschlag zupft härter; das Modrad beschleunigt das Springen. Am besten mit Kopfhörern. |
| Living Bell Arp | Arp | FM | Leben, Charakter-Makros | Weiche FM-Glocken in Sechzehnteln arpeggiert; Leben und Lebendig machen jede Wiederholung ein wenig anders – Drift, Ton und Panorama bewegen sich von selbst. Das Modrad wirft die Echos. |
| Orbit Space Arp | Arp, 3D | FM | 3D-Bewegungspfad, Tempo-synchron | Glocken-Arpeggios, die den Hörer einmal pro Takt umkreisen (Bewegungspfad Kreis, ein Takt, im Tempo), auf Armlänge, in einem weichen Raum. Druck zieht sie näher; das Modrad rückt die Umlaufbahn weiter weg. Am besten mit Kopfhörern. |
| Resonant Comb Arp | Arp | Zupfen | Resonanzkörper, Tempo-synchron | Ein Arpeggio aus gezupften Saiten durch den Resonanzkörper als Rack-Effekt: ein Saitenkörper im gehaltenen Akkord klingt unter jedem Schritt. Das Modrad lässt den Körper länger klingen, das Echo ist eine synchrone punktierte Achtel. |
| Tempo Gate Arp | Arp | Wavetable | Tempo-synchron, Ausdruck | Ein gläsernes Arpeggio, zerhackt von einem Sechzehntel-Gate im Tempo (LFO 2 auf den Pegel, mit jeder Note neu gestartet). Akkord halten, spielt bei jedem Tempo im Takt; das Modrad hebt die Echos an. |
| Bowed Metal Body | Texturen | Grain | Resonanzkörper, Ausdruck | Das aufgenommene gestrichene Metall, gedehnt zu einer langsamen Grain-Fläche und in einem gedackten Rohr im Akkord nachklingend: dunkel, singend, endlos. Druck wandert durch die Aufnahme; das Modrad ändert das Material des Rohrs. |
| Slow Weather | Texturen | Cluster | Leben, Charakter-Makros, Tempo-synchron | Ein Cluster-Drone, der sich ändert wie das Wetter: volles Leben, Lebendig aufgedreht und ein Tonschwellen in Vier-Takt-Zyklen des Tempos. Eine Note halten und zuhören; das Modrad hellt den Himmel auf. |
| Swarm Field 3D | Texturen, 3D | Grain | 3D-Bewegungspfad, Leben | Eine Wolke gläserner Grains, die um den Hörer schwärmt (Bewegungspfad Schwarm, acht Takte, im Tempo), Leben streut sie weiter. Druck holt den Schwarm näher; das Modrad gibt mehr Grain-Streuung. |
| Whisper Room | Texturen, 3D | Vocal | 3D-Bewegungspfad, Ausdruck | Atem und geflüsterte Vokale nah am Ohr, die sich bewegen, als ginge jemand um dich herum (Bewegungspfad Flüstern, vier Takte, im Tempo). Druck gibt mehr Atem; das Modrad ändert den Vokal. Am besten mit Kopfhörern. |
| Distance Walk Drone | FX, 3D | Cluster | 3D-Bewegungspfad, Leben | Ein tiefer Drone, der in acht Takten auf dich zu und wieder weg geht (Bewegungspfad Entfernungsgang, im Tempo): er schwillt an, wird in der Ferne dunkler, füllt nah den Raum. Das Modrad hebt ihn über dich. |
| Resonant Impact | FX | Analog | Resonanzkörper, Ausdruck | Ein Rauschstoß, der in eine große, auf die Note gestimmte Metallplatte kracht: ein filmischer Schlag mit langem, metallischem Nachklang. Anschlag schlägt härter; das Modrad verkleinert die Platte zu einem kleineren, helleren Klang. |
| Rising Halo Sweep | FX, 3D | Wavetable | 3D-Bewegungspfad, Tempo-synchron | Ein Riser, der sich in vier Takten spiralförmig über den Kopf hebt (Bewegungspfad Spirale aufwärts, im Tempo), während sein Ton aufgeht. Für den Aufbau eine Note halten; das Modrad beschleunigt ihn. Am besten mit Kopfhörern. |
| Living Choir | Vokal, Flächen | Vocal | Leben, Charakter-Makros | Ein breiter 'Ah'-Chor, in dem jede Stimme eigen driftet (Leben voll, Lebendig und Weit an): klingt wie eine Gruppe, nicht wie eine Kopie. Das Modrad gibt ein weiteres Chor-Vibrato. |
| Pressure Vowel | Vokal, Leads | Vocal | Ausdruck, Glide-Druck | Eine Solostimme, die beim Drücken den Mund öffnet: Aftertouch bewegt den Vokal von 'U' nach 'A', der Anschlag bestimmt die Lautstärke. Überlappende Noten gleiten, mit Druck länger. Das Modrad gibt Vibrato. |
| Harmony Follow Jam | Tasten, Bass, Arp | Projekt | Harmonie-Folgen, Ausdruck | Akkorde spielen, die Band folgt: Orbit spielt Am-F-C-G auf dem Glas-Piano (Part 1, vom Harmonie-Gehirn gehört); der Bass spielt ein festes Riff und folgt dem Grundton, die Glocken spielen feste Töne und rasten auf Akkordtöne ein, die Fläche hält einen Ton, der zum nächsten Akkordton wandert. Eigene Akkorde auf Part 1 spielen. Play drücken. |
| Motion Lanes Groove | Bass, Drums, Flächen | Projekt | Bewegungsspuren, Tempo-synchron, Charakter-Makros | Ein Groove, dessen Klänge sich von selbst bewegen: der Bewegungsrekorder spielt auf dem Bass eine Filter- und Resonanz-Spur, die alle fünf Sechzehntel wiederholt, auf der Fläche eine Hell-Spur über zwölf Sechzehntel und auf den Platten eine Ausklang-Spur über sieben – gegen den Vierviertel-Beat auf dem Tight-Kit laufen sie auseinander und treffen sich wieder. Eigene aufnehmen: LFO-Modus, Bewegung. Play drücken. |
| Pads Performance Set | Drums, Bass, FX | Projekt | Performance-Pads, Tempo-synchron | Ein House-Loop, eingerichtet für die Performance-Pads (Effekt-Modus, Pads): Stottern in Sechzehnteln, ein Bandstopp über einen Takt, ein Tiefpass-Sweep über zwei Takte und ein Hall-Freeze, alles im Takt. Während der Sweep schließt, öffnet die Fläche ihr eigenes Filter dagegen; solange ein Pad gehalten wird, treten die Zupfer zurück. Play drücken, dann die Pads halten. |
| Binaural Scene | Flächen, 3D, Texturen | Projekt | 3D-Bewegungspfad, Tempo-synchron | Vier Klänge bewegen sich gleichzeitig um den Kopf: eine Fläche steigt in sechzehn Takten spiralförmig auf, Glocken kreisen einmal pro Takt, Zupfer springen jeden halben Takt von vorn nach hinten, dazu ein gläserner Grain-Schwarm. Jeder Pfad ist in Takten bemessen, die ganze Szene folgt dem Tempo. Kopfhörer auf, Play drücken. |
<!-- showcase:end -->

## Drum-Kits

`drum.kit` wählt die Aufnahmen, die die 24 Pads spielen. Alle Kits haben dieselbe Pad-Belegung,
jedes Pattern spielt also auf jedem Kit. Eigene Pad-Samples haben immer Vorrang vor dem Kit.

| Kit | Klang | Gemacht aus |
|---|---|---|
| **Studio** | das ursprüngliche Kit: Nah-, Raum- und Overhead-Mikrofone pro Instrument gemischt; seit 0.5 mit druckvolleren Kicks (kürzeres Nachklingen, weniger Pappe, Schlägelklick, kurze gestimmte Sub-Schicht) | *Virtuosity Drums* (CC0), Ohpes eigene Bearbeitung |
| **Trocken** (Tight) | trocken und kurz: Nahmikrofone, abgedämpfte Toms, kurze Becken, ein aufgenommenes Händeklatschen | *Virtuosity Drums*, VCSL-Claps (CC0) |
| **Raum** (Room) | groß und live: Raum- und Overhead-Mikrofone, lange Ausklänge, geschichteter Clap mit kurzem Raum | *Virtuosity Drums*, VCSL-Claps (CC0) |
| **Lo-Fi** | trockene Nahmikrofone (0.5; vorher das raumlastige Vintage-Mikrofon der Quelle) mit gewichtiger Kick, dann bandbegrenzt, gesättigt und auf 12 Bit quantisiert – staubig, aber druckvoll | *Virtuosity Drums*, VCSL-Claps (CC0) |
| **Hybrid** | elektronische Kick, Toms, Hats, Cowbell, Claves und Congas von Ohpe synthetisiert, mit akustischen Transienten, echten Becken und Claps geschichtet | Ohpes eigene Synthese, *Virtuosity Drums*, VCSL (CC0) |
| **Boom** (0.5) | jedes Pad synthetisiert: eine tiefe Sub-Kick mit Tonhöhenhüllkurve auf Pad 1 (C2), eine lange, angezerrte Sub-Kick auf Pad 2 (Cis2) – mit *Tune* und *Decay* des Pads stimmen und kürzen –, eine druckvolle Snare, ein knackiger geschichteter Clap, geschlossene / offene / Pedal-Hats, Cowbell, Rim, Toms und Percussion | Ohpes eigene Synthese (keine Aufnahmen) |

## Aufgenommene Instrumente

`pN.sampler.inst` (Sampler) und `pN.grain.inst` (Grain) wählen ein Werksinstrument. Der Sampler
spielt es, wenn kein eigenes Sample geladen ist; Grain granuliert einen seiner Töne. Alle Aufnahmen
stammen aus der *Versatile Community Sample Library* (VCSL, CC0), mehrfach gesampelt (4–8 Töne,
Marimba mit zwei Anschlagstufen), im Pegel angeglichen und mono.

| Instrument | Töne | Loop |
|---|---|---|
| Kalimba | 7 | – |
| Marimba | 7 × 2 Anschlagstufen | – |
| Vibrafon | 6 | – |
| Glockenspiel | 6 | – |
| Harfe | 8 | – |
| Cembalo | 7 | – |
| Blockflöte | 7 | ja – *Loop* auf *Vorwärts* stellen |
| Klangglas | 4 | ja |
| Zither | 7 | – |
| Gestrichenes Metall (gestrichenes Vibrafon) | 6 | ja |

Gehaltene Instrumente haben Loop-Punkte; die Presets, die sie nutzen, stellen *Loop* des Samplers
auf *Vorwärts*. Lizenzen und Quellen: [THIRD-PARTY.md](../../THIRD-PARTY.md).

## Wie die Klänge geprüft wurden

Jedes Werks-Preset wird offline mit einer typischen Phrase seiner Kategorie gerendert (Basslinie,
Akkorde, Lead-Linie, Arpeggio, Groove …) und gemessen: Lautheit (Zielfenster −18 … −12 dBFS RMS),
Spitzenpegel (keine Übersteuerung, bei Part-Presets nie über −1,2 dBFS), Gleichanteil, Denormals,
ob Ausklänge enden, und Energie über 16 kHz (kein Zischeln, außer gewollt, z. B. Hi-Hats).
Spektrogramme aller Renderings wurden gesichtet und schwache Presets überarbeitet. Die
Plug-in-Tests wiederholen diese Prüfungen für jedes eingebaute Preset.

Werkzeuge, für Mitwirkende:

* `tools/presets/factory_parts.py`, `factory_projects.py` – die Preset-Definitionen;
  `make_factory.py` schreibt `resources/presets/factory/*.ohpepreset`, `make_docs.py` die Listen unten.
* `ohpe_render` (wird mit den Core-Tests gebaut) rendert Presets als WAV und gibt die Messwerte aus;
  `tools/presets/analyse.py` lässt es über alle Werks-Presets laufen, markiert Probleme, zeichnet
  Spektrogramme und schreibt die Pegel-Kalibrierung (`levels.json`).
* `tools/samples/build_samples.py` baut Drum-Kits und Instrumente (`resources/samples`) aus den
  CC0-Quellen mit festgelegten Revisionen.

## Preset-Liste

<!-- presets:begin -->
### Bass (68)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Acid Line | Analog | Leads | Schmatzender, resonanter Sägezahn mit Sub für Gewicht, Akzent über die Anschlagstärke und kurzem Glide. |
| Analog Reese | Analog | MPE | Zwei weit verstimmte analoge Sägezähne durch Ladder-Filter, Phaser und Squash: warm und fett. |
| Bounce Bass | Analog | – | Heller, druckvoller Sägezahn-Bass für hüpfende Oktav-Achtel. |
| Classic Reese | Wavetable | MPE | Zwei verstimmte Sägezähne, die langsam gegeneinander schweben, darunter ein sauberer Sub; mono unter 120 Hz. |
| Click Sub | Analog | – | Sinus-Sub mit scharfem Klick obenauf, damit die Basslinie auch auf kleinen Lautsprechern trägt. |
| Dark Reese | Wavetable | MPE | Tiefer, rauchiger Reese: dunkles Filter, langsamer Chorus, leichter Squash – für Liquid und Deep. |
| Deep Pluck Sub | Analog | – | Gezupfter Sub mit hellem Anschlag, der in einen sauberen Sinus übergeht; straff für schnelle Linien. |
| Digital Monster | Wavetable | Hybrid, MPE | Digitale FM-Tabelle, gegatet von einem Glas-Grain-Layer eine Oktave tiefer, dann gewobbelt: instabil und riesig. |
| Dirty Sub | Analog | Boom | Sub, ins Ladder-Filter gefahren: knurrende Obertöne, die auch auf kleinen Lautsprechern hörbar bleiben. |
| Distorted Reese | Wavetable | MPE | Röhrenverzerrter Reese mit mahlenden Mitten; der Squash hält ihn dicht und präsent. |
| Fat Saw Bass | Analog | – | Zwei verstimmte Sägezähne und ein Sub durch das Ladder-Filter; groß, aber nicht dröhnend. |
| Flanged Reese | Wavetable | MPE | Sägezahn-Stapel mit langsamem Jet-Flanger und Chorus: wirbelnde Stereo-Höhen, stabiler Mono-Bass. |
| Fold Bass | Wavetable | Hybrid, MPE | Sinus-Sub und analoger Sägezahn, ineinander gefaltet: die Faltmenge lässt ihn brüllen. |
| Glide Reese | Wavetable | MPE | Reese mit Portamento konstanter Rate: überlappende Noten gleiten wie ein bundloses Gummiband. |
| Glide Sine Bass | FM | – | Reiner Sinus-Bass mit Legato-Glide für gleitende Basslinien; abgesetzte Noten bleiben sauber. |
| Grind Bass | Wavetable | MPE | Mahlende, gefaltete Grit-Tabelle, hart gesquasht: eine konstante, dichte Wand aus Mitten. |
| Grit Bass | Wavetable | – | Knurrende Grit-Tabelle mit Hüllkurven-Sweep; aggressiv, aber kontrolliert. |
| Growl FM Bass | FM | – | FM-Bass mit Feedback-Kante und langsamem Klangfarben-LFO. |
| Growl Machine | Wavetable | MPE | Formant-Tabelle mit FM und LFO auf der Position, synchronisiertes Vokalfilter und schwerer Squash: ein sprechendes Growl. |
| Half-Step Wobble | Wavetable | MPE | Wobble in punktierten Vierteln, der über den Taktstrich läuft – schwer und schief. |
| Halftime Sub | Wavetable | – | Langer, schwerer Sinus-Sub mit immer aktivem Glide: langsame Slides für Half-Time-Teile. |
| Hollow Reese | Wavetable | MPE | Reese durch einen langsam driftenden Kammfilter mit negativem Feedback: hohl, metallisch und dunkel. |
| House Organ Bass | Analog | – | Hohles Rechteck mit zweitem Oszillator eine Oktave tiefer; kurz und druckvoll. |
| Laser Wobble | Wavetable | MPE | Steigender Sägezahn-LFO auf einem Bandpass-Wobble in Sechzehnteln: zappende, laserartige Bewegung. |
| Liquid Sub | FM | – | Reiner, warmer Sinus-Sub mit weicher zweiter Harmonischer; gleitet zwischen überlappenden Noten. |
| Lofi Bass | Analog | – | Runder, warmer Bass: weicher Rechteck-Körper über kräftigem Sinus-Sub, sanft angezerrt. |
| Log Drum Bass | Sampler | – | Tiefe Marimba-Stäbe als Bass: holzig, rund und kurz. |
| Long Sub | Analog | Boom | Tiefer, lang ausklingender Sub, der zwischen überlappenden Noten gleitet (Glide); weicher Klick im Anschlag. |
| Mellow Lofi Bass | Analog | – | Warmer, runder Bass: weicher, tiefpassgefilterter Körper über einem Sinus-Sub mit sanfter Sättigung und einem Hauch Glide. |
| Metal Growl | Wavetable | MPE | Scheppernder, metallischer Growl: FM-Tabelle, resonanter Kammfilter obenauf und harter Squash. |
| Moving Reese | Wavetable | MPE | Reese, dessen Tabellenposition und Filter mit einem langsamen LFO atmen – Bewegung auf gehaltenen Noten ohne Automation. |
| Neuro Hybrid | FM | Hybrid, MPE | FM-Growl und Wavetable-Reese, die sich gegenseitig ringmodulieren, gefaltet und gesquasht: ein Neuro-Monster. |
| Neuro Reese | Wavetable | MPE | Verzerrter sechsstimmiger Reese: angefahrenes Filter, gefaltete Obertöne und ein harter Mono-Tiefbass – zum Automatisieren gemacht. |
| Neuro Swirl | Wavetable | 3D, MPE | Sprechender Formant-Growl, der einmal pro Takt eine Acht zieht – der Vokal bewegt sich, der Raum bewegt sich, der Sub nicht. |
| Orbit Reese | Wavetable | 3D, MPE | Neurofunk-Reese, dessen knurrende Mitten alle zwei Takte um deinen Kopf kreisen, während der Sub unter 160 Hz genau in der Mitte und mono bleibt – clubtaugliche Bewegung. |
| Pendulum Wobble | Wavetable | 3D, MPE | Halftime-Dubstep-Wobble, der mit dem Takt über deinen Kopf und zurück schwingt: Wobble und Raum atmen zusammen. |
| Phase Reese | Wavetable | MPE | Vierstimmiger Sägezahn-Stapel durch einen langsamen, tiefen Phaser: der klassische schwebende Drum-&-Bass-Reese. |
| Punch FM Bass | FM | – | Solider digitaler Bass mit hellem Anschlag und sauberem Sinus-Körper. |
| Punch Sub | Analog | Boom | Kurzer, druckvoller Sub mit knackigem Filterklick; rastet mit der Kick ein. |
| Reese Drift | Analog | – | Breite, verstimmte Sägezähne, die langsam gegeneinander schweben; dunkel und bewegt. |
| Riddim Saw | Wavetable | MPE | Hart geclippter Sägezahn mit Viertel-Bandpass-Wobble: sparsam, schwer und minimal. |
| Rise Bass | Analog | – | Angezerrter Bass, der bei jeder Note eine Quinte von unten hineingleitet; ideal für synkopierte Linien. |
| Roller Bass | Analog | – | Druckvoller, rechteckiger Roller: kurzer Filter-Snap auf jeder Note, solider Sub, dicht durch Squash. |
| Rolling Bass | Analog | – | Straffer, angezerrter Techno-Bass für rollende Sechzehntel: kurzer Filter-Snap, solider Sub. |
| Rolling Sub | Wavetable | – | Runder Sub mit winzigem Filter-Blip und sanfter Sättigung für rollende Basslinien; mono und stabil. |
| Round Sub | Analog | – | Warmes Rechteck mit Sub-Oszillator und weichem Filter-Blip; passt unter alles. |
| Rubber Bass | Analog | Zupfer | Federnder gezupfter Bass: schneller Filterverlauf, runder Sub. |
| Screech Bass | Wavetable | MPE | Kreischende FM mit hohem Verhältnis und gefalteten Höhen: für die hohen, aggressiven Bass-Antworten. |
| Sine Slide | FM | Boom | Fast reiner Sinus-Sub mit langem Glide für gleitende Basslinien. |
| Slap FM Bass | FM | – | Perkussiver FM-Bass: der Index schnappt zu wie eine geslappte Saite. |
| Square Wobble | Wavetable | MPE | Harter Sechzehntel-An/Aus-Wobble mit Rechteck-LFO – zerhackt und rhythmisch. |
| Step Sub | Wavetable | – | Sub, der in jede Note eine Quinte hineinfällt: ein kleiner Pitch-Kick für Half-Time-Linien. |
| Sub Reese | Wavetable | MPE | Sanfter Reese über einem großen Sinus-Sub – der Sub bleibt mono und ruhig, oben bewegt es sich. |
| Table Sub | Wavetable | – | Fast-Sinus-Wavetable-Bass mit Sub; reiner Tiefbass mit etwas Kante. |
| Talker Bass | Wavetable | MPE | Formant-Tabelle, von schnellem LFO und Hüllkurve gefahren: jede Note sagt etwas anderes. |
| Talking Bass | Wavetable | – | Formant-Tabelle, von der Hüllkurve gefahren: ein Vokal-„Wow“ auf jeder Note. |
| Tear Reese | Wavetable | MPE | Aggressiver Neuro-Reese: digitale Tabelle mit FM-Kante, schwerer Mehrband-Squash mit angehobenen Höhen. |
| Tech Stab Bass | FM | – | Harter, kurzer Tech-Bass: FM-Biss, Hard-Clipper und Squash für klinische, perkussive Linien. |
| Triplet Wobble | Wavetable | MPE | Achteltriolen-Wobble auf einem angezerrten Sägezahn-Stapel: der hüpfende Half-Time-Klassiker. |
| Unison Bass | Cluster | – | Oktav-gestapelte Sägezähne, leicht verstimmt; dick und breit. |
| Upright Pluck | Zupfen | Zupfer | Gezupfte Saite mit großem Holzkorpus: ein akustischer Bass. |
| Vowel Growl | Wavetable | MPE | Viertel-Vokalsweep (a – e – i – o – u) über einem gefalteten Sägezahn: der klassische „sprechende“ Bass. |
| Vowel Reese | Wavetable | MPE | Ein Reese, der spricht: Formant-Tabelle unter einem LFO plus synchronisiertes Vokalfilter, das a – o – i durchläuft. |
| Vox Reese | Wavetable | Hybrid, MPE | Ein Reese mit singendem Vokal-Layer obenauf: die Formanten reiten auf dem Bass, ein synchrones Vokalfilter lässt ihn sprechen. |
| Warm Roller | Analog | – | Ladder-gefilterter Sägezahn mit Sub für warme, runde Rolling-Basslinien in Liquid-Tracks. |
| Wide Reese | Wavetable | MPE | Achtstimmiger Reese über das ganze Stereofeld, mit Haas-Spreizung obenauf und Mono-Tiefbass. |
| Wobble Bass | Analog | FX | Filter-Wobble auf einem angezerrten Sägezahn-Stapel; LFO-Tempo nach Geschmack. |
| Wub Machine | Wavetable | MPE | Sägezahn-Stapel durch ein tempo-synchrones Achtel-Wobble-Filter, das mit jeder Note neu startet. |

### Tasten (38)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Bright Grand | Sampler | Klavier, MPE | Flügel, für Pop geöffnet: heller, leicht gesquasht, kurzer Raum. |
| Bright Poly Wide | Analog | MPE | Klassischer heller Poly-Synth, mit Chorus und Breite – Akkorde, die den Raum füllen. |
| Classic Tine | Sampler | Klavier, MPE | Modelliertes Tine-E-Piano: glockig bei leichtem Anschlag, bellend bei hartem. |
| Cluster Organ | Cluster | – | Oktav-Stapel weicher Rechtecke: eine kitschige Combo-Orgel. |
| Concert Grand | Sampler | Klavier, MPE | Flügel-Aufnahmen, zwei Anschlagsstufen, ein Hauch Hall. |
| Crystal Organ | FM | Hybrid, MPE | Additive FM-Orgel mit Glas-Wavetable-Layer, die mit langsamem Autopan im Stereofeld kreist. |
| Digital EP | FM | – | Helleres E-Piano, das bei hartem Anschlag mehr bellt. |
| Drawbar Organ | FM | – | Additive Orgel: sechs Sinus-Zugriegel, sanftes Vibrato, Rotor-artiger Chorus. |
| Dream Tine | Sampler | Klavier, Flächen, MPE | Tine-Piano, getränkt in Chorus, punktiertes Delay und langen Hall. |
| Dub Stab | Analog | Flächen | Dunkler, verstimmter Akkord-Stab, dessen Filter langsam auf- und zugeht; mit Delay und Hall spielen. |
| Dusty EP | FM | – | Warmes E-Piano mit Band-Wow und -Flutter (langsames Tonhöhen-LFO) und Chorus; für Lo-Fi-Akkorde gemacht. |
| Dusty Keys | FM | – | Weiches, dunkles E-Piano für Lo-Fi-Akkorde: starkes Band-Leiern auf der Tonhöhe, bitreduzierte Kante, weicher Anschlag. |
| FM Bark Piano | FM | Klavier, MPE | FM-E-Piano aus zwei Paaren: das Tine-Paar klingt, das Bark-Paar beißt mit der Anschlagstärke. |
| FM Vibes | FM | Zupfer | Weiche metallische Klangstäbe mit Tremolo. |
| Felt Grand | Sampler | Klavier, MPE | Dunkler, weicher Flügel: Höhen abgesenkt, langsames Release und warmer Hall – intim. |
| Funk Clav | FM | Zupfer | Knackige Clavinet-artige Tasten; kurz und synkopiert spielen. |
| Glass Body Keys | Cluster | MPE | Weiche Tasten durch eine Glasplatte, auf jede Note gestimmt: ein gläserner Schimmer über dem Akkord. |
| Glass Keys | Wavetable | – | Glockige Glass-Tabelle, die beim Ausklingen dunkler wird. |
| Glide Poly Synth | Analog | Leads | Poly-Synth, dessen Akkorde immer vom vorigen Akkord herübergleiten (Poly-Glide). |
| Halo Grand | Sampler | Hybrid, Klavier, MPE | Flügel mit Heiligenschein: seine eigene Aufnahme, eingefroren zu einer Grain-Wolke, die hinter jedem Akkord aufblüht. |
| Harmonic Organ | Wavetable | – | Additive Harmonic-Tabelle: eine weiche, leicht hauchige Orgel. |
| Harpsichord | Sampler | Zupfer | Aufgenommenes Cembalo (sieben Töne, CC0); Barock-Linien und helle Begleitung. |
| Mallet Keys | Zupfen | Zupfer | Physikalisch modellierte Holzstäbe; weicher Schlägel. |
| Marimba | Sampler | Zupfer | Aufgenommene Marimba mit zwei Anschlagstufen. |
| Minor Stab | Cluster | – | Ein Finger spielt einen Moll-Septakkord-Stab; verstimmt und druckvoll. |
| Music Box | Sampler | Zupfer | Glockenspiel, weich gefiltert zur Spieluhr; Wiegenlieder und Spielzeug-Melodien. |
| Pop Brass | Analog | Flächen | Üppiges, verstimmtes Poly-Brass mit kurzem Anschwellen und Chorus: klassische Synth-Pop-Akkorde. |
| Reed Keys | Sampler | Klavier, MPE | Modelliertes Zungen-E-Piano: nasal und holzig, mit rauem Bellen, wenn man es anfährt. |
| Reed Tremolo | Sampler | Klavier, MPE | Zungen-Piano durch sein klassisches Lautstärke-Tremolo – Soul und Gospel. |
| Scoop Keys | FM | – | E-Piano, bei dem jede Note einen Ganzton von unten anschleift: soulig, ein wenig lässig. |
| Soft Brass | Analog | Flächen | Polyphones Blech: langsames Filter-Anschwellen auf verstimmten Sägezähnen. |
| Suitcase Tine | Sampler | Klavier, MPE | Tine-Piano mit dem Stereo-Autopan eines Koffer-Verstärkers und kleinem Raum. |
| Tape Ghost Keys | Sampler | Hybrid, Klavier, MPE | Tine-Piano mit einem Geist seiner selbst: eine verstimmte Grain-Kopie, gecrusht und geflangt wie ein altes, neu aufgenommenes Band. |
| Tine Piano | FM | – | E-Piano: weicher Körper und glockiger Zungen-Anteil, der mit der Taste verklingt. Chorus im FX-Slot. |
| Upright Keys | Sampler | Klavier, MPE | Klavier-Aufnahmen (Upright) mit etwas Raum: ehrlich und nah. |
| Velvet Stab | Analog | – | Kurzer gefilterter Akkord-Stab für House und Disco. |
| Vibraphone | Sampler | – | Aufgenommenes Vibrafon mit sanftem Motor-Tremolo. |
| Wide Organ | Wavetable | – | Zugriegelartige Orgel mit rotierendem Stereo-Pan und kurzem Hall. |

### Flächen (48)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Analog Dawn | Analog | – | Puls-Fläche, deren Filter sich langsam öffnet, mit langsamem Phaser. |
| Angel Pad | Vocal | Vokal | Hohe „Uh“-Stimmen, die langsam zu „Ah“ wandern. |
| Aurora Scan | Wavetable | Texturen, MPE | Glas-Tabelle, langsam abgetastet, die mit einem Zwei-Takt-Autopan im Stereofeld kreist. |
| Bowed Haze | Grain | – | Körner eines gestrichenen Vibrafons: ein weicher metallischer Schein. |
| Bowed Metal | Sampler | Texturen | Gestrichenes Vibrafon (CC0-Aufnahmen), geloopt: gläsern und unheimlich. |
| Brass Slide | Analog | Tasten | Anschwellendes Poly-Brass mit schnellem Portamento zwischen den Akkorden. |
| Breath Choir Pad | Vocal | Vokal | Luftiger „Ah“-Chor mit viel Atem. |
| Breath Grains | Grain | Texturen, MPE | Blockflöten-Atem, in eine weiche, breite Grain-Fläche geschnitten. |
| Chop Pad | Cluster | Hybrid, MPE | Eine breite Akkordfläche, zerhackt und gestottert von der Hüllkurve eines Kalimba-Grain-Layers. |
| Crystal FM Pad | FM | – | Fünf leicht verstimmte Sinus-Träger mit einem gemeinsamen Modulator. |
| Crystal Slide | FM | – | Schimmernde FM-Fläche: verstimmte Sinus-Träger, die zwischen Akkorden gleiten. |
| Drift Stack | Cluster | Texturen | Verstimmte Sägezahn-Stapel, die langsam von Akkord zu Akkord driften. |
| Evolving Tide | Wavetable | MPE | Harmonische Tabelle, die mit sehr langsamem LFO durch ihre Frames driftet, achtstimmiger Stapel, Flanger und Hall. |
| Fall Haze | Wavetable | FX | Dunstige Fläche, deren Noten eine Quinte von oben in den Akkord sinken. |
| Fifth Cloud | Cluster | – | Offene Quinten mit langsamer Drift: filmisch und neutral. |
| Frozen Cloud | Grain | Texturen | Granulares Einfrieren der eingebauten harmonischen Textur. |
| Frozen Vibes | Grain | Texturen, MPE | Vibrafon-Grains, stillgehalten, mit langsamem Flanger: ein gefrorener, schimmernder Akkord. |
| Ghost Organ | Wavetable | Tasten | Hohle harmonische Orgel mit langsamem Autopan und höhlenartigem Hall. |
| Glass Engine | Grain | Hybrid, MPE | Klangglas-Grain-Wolke, verschmolzen mit einem harmonischen Wavetable-Layer, der langsam scannt; ein Hauch Ringmodulation. |
| Glass Glide | Wavetable | Texturen | Breite Glas-Fläche mit langsamen, verträumten Gleitern zwischen den Akkorden. |
| Glass Grains | Grain | Texturen | Körner eines singenden Weinglases (CC0-Aufnahme), langsam abgetastet. |
| Glass Shimmer | Wavetable | Texturen | Glass-Tabelle mit breitem Unisono: luftige, glitzernde Fläche. |
| Glide Strings | Analog | – | Ensemble-Streicher, deren Akkorde ineinander gleiten (Poly-Glide); Legato-Akkordwechsel. |
| Halo Drift | Cluster | 3D, MPE | Verstimmter Cluster-Chor, der auf einem langsamen Zufallsorbit um und über dir wandert – nie zweimal am selben Ort. |
| Harmonic Swell | Wavetable | – | Obertöne blühen auf, während die Fläche anschwillt. |
| Infinite Hall | Cluster | MPE | Major-Septakkord-Stapel in einem riesigen Hall, der nie zu enden scheint. |
| Metal Strings | Zupfen | Hybrid, MPE | Metallische gezupfte Saiten, gefaltet in eine langsame Cluster-Fläche – ein Orchester aus Draht. |
| Night Drive | Analog | Texturen | Dunkle resonante Fläche, die mit einem langsamen Filter-LFO atmet. |
| Orbit Atmos | Wavetable | 3D, MPE | Breite Wavetable-Atmosphäre, die alle acht Takte auf einer leicht geneigten Bahn um deinen Kopf kreist, in einem weichen Raum. Am besten mit Kopfhörern: Slide hebt sie an, Druck holt sie näher. |
| Orbit Pad | Grain | Hybrid, MPE | Harfen-Grains, die einen FM-Layer umkreisen, sanft ringmoduliert und über zwei Takte pannend. |
| Overhead Choir | Vocal | Vokal, 3D, MPE | Hauchiger „Uh“-Chor, der von vorn über deinen Kopf nach hinten und zurück schwingt – ein Pendel durch den Raum. |
| Piano Grains | Grain | Texturen, Klavier, MPE | Flügel-Aufnahmen, eingefroren zu einer langsam ziehenden Grain-Wolke: ein Klavier, das atmet. |
| Rise Bloom | FM | – | Schimmernde FM-Fläche, die von einer Oktave darunter aufblüht; lang und hell. |
| Rise Choir | Vocal | Vokal | Luftiger „Ah“-Chor, der bei jedem Akkord eine Quarte von unten hineinschleift. |
| Rise Glass | Wavetable | Texturen | Breite Glas-Fläche: jede Note gleitet eine Quinte von unten an ihren Platz, verträumt und langsam. |
| Rise Strings | Analog | – | Ensemble-Streicher, deren Noten bei jeder neuen Note eine Oktave von unten in den Akkord steigen. |
| Rise Warmth | Cluster | – | Weicher, verstimmter Sägezahn-Stapel, dessen Noten sich einen Ganzton von unten in den Akkord lehnen. |
| Slide Choir | Vocal | Vokal | Luftiger Ah-Chor; jede Stimme gleitet zum nächsten Akkord. |
| Slow Motion Choir | Vocal | Vokal, MPE | Ooh-Chor, der langsam anschwillt, mit Chorus und Breite, in einem langen Kathedralen-Hall. |
| Spectral Morph | Wavetable | Hybrid, MPE | Formant-Tabelle, die unter einem gehauchten Vokal-Layer morpht, beide durch einen driftenden Kammfilter: Spektren, die ineinander schmelzen. |
| Super Stack | Cluster | – | Sieben verstimmte Sägezähne pro Taste, breit und üppig. |
| Sus Horizon | Cluster | – | Sus2-Stapel, die sich nie auflösen; Ambient-Teppiche. |
| Sympathetic Pad | Analog | MPE | Eine warme Fläche weckt Resonanzsaiten, gestimmt auf den gehaltenen Akkord – sie klingen nach dem Loslassen weiter. |
| Tidal Pad | Analog | MPE | Warme Sägezahn-Fläche, deren Filter über zwei Takte wie Gezeiten kommt und geht. |
| Tine Bloom | Grain | Texturen, Klavier, MPE | Tine-Piano-Grains, die langsam aufblühen, mit Chorus und punktiertem Echo. |
| Vowel Pad | Wavetable | – | Formant-Tabelle, die zwischen Vokalen treibt; langsamer Einsatz. |
| Warm Horizon | Cluster | MPE | Weiche, breite Add9-Fläche mit sanftem Drift: warm und offen, sitzt hinter allem. |
| Warm Strings | Analog | – | Ensemble-Streicher: verstimmte Sägezähne, eine Oktave höher, sanfter Chorus. |

### Leads (34)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Binaural Lead | Wavetable | 3D, MPE | Ausdrucksstarkes Lead vor dir: Slide hebt es über deinen Kopf, Druck holt es bis ans Ohr, eine langsame Drift hält es lebendig. Für MPE-Controller und Kopfhörer. |
| Bowed Tube | Analog | MPE | Hauchiges Rauschen streicht ein gedacktes Rohr: ein hohler, klarinettenartiger Ton, der mit dem Druck anschwillt. |
| Breath Lead | Vocal | Hybrid, MPE | Gehauchter, gesungener Ton, verschmolzen mit einem weichen Sägezahn-Stapel: ein Lead zwischen Stimme und Synth. |
| Chip Square Lead | Wavetable | MPE | Schmales Puls-Lead mit knusprigem Bitcrusher und schnellem Vibrato – retro, aber breit. |
| Classic Mono Lead | Analog | – | Sägezahn-Lead mit etwas Vibrato, Glide und punktiertem Delay. |
| Digital Lead | Wavetable | – | Verbogene Digital-Tabelle mit Unisono: modern und durchsetzungsstark. |
| Dream Lead | Wavetable | Flächen, MPE | Weiches Dreieck-Lead mit langsamem Flanger und langem, luftigem Hall. |
| FM Brass Lead | FM | – | Blechiges FM-Lead: der Index steigt mit einem kurzen Anschwellen. |
| Fall Lead | Analog | FX | Rechteck-Lead, das bei jeder Note eine Quinte von oben hineinfällt. |
| Formant Lead | Wavetable | Vokal | Vokalartiges Lead, das sich mit der Hüllkurve öffnet. |
| Growl Lead | Wavetable | MPE | Beißendes FM-Formant-Lead, das mit einem LFO knurrt: ein Bassmusik-Lead für die Antwortphrase im Drop. |
| Hot Wire | Analog | – | Angezerrter, resonanter Sägezahn für rockige Soli. |
| Hyper Saw Lead | Cluster | MPE | Riesiger verstimmter Sägezahn-Stapel, nach vorn gesquasht, mit Stereo-Delay: Main-Stage-Energie. |
| Legato Glide Lead | Analog | – | Mono-Sägezahn-Lead mit Legato-Portamento: überlappende Noten gleiten, abgesetzte Noten beginnen ohne Gleiten. |
| Mono Glide Lead | Wavetable | MPE | Ausdrucksstarkes Mono-Lead mit Legato-Glide, leichtem Drive und punktiertem Ping-Pong-Delay. |
| Octave Lead | Cluster | – | Oktav-gestapelte Pulse mit kurzem Filter-Blip. |
| Pluck Lead Wide | Wavetable | Zupfer, MPE | Knackiges gezupftes Lead: schneller Filter-Snap, Stereo-Stapel und punktiertes Delay. |
| Pop Lead | Analog | – | Heller Sägezahn plus Oktav-Rechteck mit Vibrato: ein klarer, singender Synth-Pop-Lead. |
| Portamento Square | Analog | – | Hohles Rechteck, das immer gleitet, mit fester Rate: große Sprünge gleiten länger als kleine Schritte. |
| Recorder | Sampler | – | Aufgenommene Altblockflöte, geloopt, mit leichtem Vibrato. |
| Ring Machine | Analog | Hybrid, MPE | Sägezahn-Lead, ringmoduliert von einem FM-Sinus eine Quinte höher: metallisch, vokal und aggressiv, mit Delay. |
| Rise Lead | Analog | – | Sägezahn-Lead, das bei jeder Note eine Oktave von unten heranschwingt; ein punktiertes Delay dahinter. |
| Saw Unison Lead | Wavetable | – | Classic-Tabelle in Sägezahn-Stellung mit breitem Unisono. |
| Screamer Lead | Analog | MPE | Resonantes, übersteuertes Sägezahn-Lead, das in der Höhe schreit; das Ping-Pong-Delay hält es breit. |
| Shimmer Lead | FM | Tasten, MPE | Gläsernes FM-Lead mit Glockenkante, Chorus und langem, hellem Hall. |
| Sine Whistle | FM | – | Fast reiner Sinus mit einem Hauch Modulation; zarte Melodien. |
| Soft Flute Lead | Analog | – | Hauchiger, resonanter Tiefpass auf einem Rechteck: ein weicher Pfeifton. |
| Square Hero | Analog | – | Hohles Rechteck plus Oktave: ein helles, singendes Lead. |
| Stereo Saw Lead | Analog | MPE | Klassisches Zwei-Sägezahn-Lead, breit gemacht: Chorus, Haas-Spreizung und kurzer Raum. |
| Super Lead | Cluster | – | Sieben-Sägezahn-Unisono-Lead: Festival-Format. |
| Super Wide Lead | Wavetable | MPE | Achtstimmiger Sägezahn-Stapel über das ganze Stereofeld, Chorus, Ping-Pong-Delay und großer Hall. |
| Twist Lead | Wavetable | MPE | Verdrehtes, Sync-artiges Lead: die Hüllkurve biegt den Warp, der Stapel macht es breit. |
| Vox Lead | Vocal | Vokal | Synthetische Solostimme mit ausdrucksvollem Vibrato und Glide. |
| Vox Synth Lead | Vocal | Vokal, MPE | Gesungenes Synth-Lead: ein „Ah“ mit Vibrato, verbreitert durch Haas-Spreizung, mit Delay. |

### Zupfer (23)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Concert Harp | Sampler | – | Aufgenommene Konzertharfe (acht Töne, CC0). |
| Drone Strings | Zupfen | MPE | Gezupfter Stahl über Resonanzsaiten im Akkord: schnarrend, dröhnend, endlos nachklingend. |
| Echo Harp | Zupfen | MPE | Gezupfte, nylonartige Saite mit punktierten Ping-Pong-Echos und breitem Raum. |
| FM Koto | FM | – | Gezupftes FM: heller Anschlag, der schnell weicher wird. |
| Future Chord | Cluster | Tasten, MPE | Breiter Moll-Septakkord-Stab mit Squash und kurzem Schimmer – eine Taste, ein Akkord. |
| Gamelan Plate | Zupfen | MPE | Kurze Schlegelschläge auf eine lange Bronzeplatte, gespreizte Teiltöne und ein schimmernder Nachklang. |
| Glass Pluck | Wavetable | – | Glass-Tabellen-Pluck mit glockigem Funkeln. |
| Harp Pluck | Zupfen | – | Lang klingende, harfenartige Saite mit weichem Anschlag. |
| Kalimba | Sampler | Tasten | Aufgenommenes Daumenklavier mit schnarrenden, holzigen Zungen (CC0). |
| Koto Pluck | Zupfen | – | Helle, sirrende Saite mit resonantem Korpus. |
| Muted Pick | Analog | – | Kurzer, abgedämpfter Rechteck-Pluck für schnelle Linien. |
| Nylon Guitar | Zupfen | – | Warme Nylonsaite, nahe am Schallloch gezupft. |
| Pluck Nebula | Zupfen | Hybrid, MPE | Eine gezupfte Saite, die sich in eine Kalimba-Grain-Wolke auflöst: die Saite verklingt, die Wolke schwillt an und bleibt. |
| Plucked Plate | Zupfen | MPE | Ein heller Zupfer schlägt eine gestimmte Metallplatte an: glockenartige Teiltöne, die nach dem Anschlag aufblühen. |
| Poly Pluck | Analog | – | Klassischer Synth-Pluck: knackiges Filter auf einem verstimmten Sägezahn-Paar. |
| Resonator Hit | Zupfen | Hybrid, MPE | Ein angeschlagener Metallstab, der eine kurze Grain-Resonanz und einen gestimmten Kammfilter speist: perkussiv, klingend, seltsam. |
| Steel Drum FM | FM | Tasten | Tropische Steeldrum-Töne aus unharmonischer FM. |
| Steel String | Zupfen | – | Helle Stahlsaite, nahe am Steg gezupft. |
| Stereo Pluck | Wavetable | MPE | Heller sechsstimmiger Pluck, breit gespreizt, mit punktiertem Ping-Pong-Delay und Hall. |
| Table Pluck | Wavetable | – | Wavetable-Pluck: heller Sägezahn-Frame, der schnell schließt. |
| Wide Glass Pluck | FM | Tasten, MPE | FM-Glas-Pluck mit Haas-Spreizung und hellem, luftigem Hall. |
| Zip Pluck | Wavetable | – | Heller Pluck mit schnellem Zip von unten in die Note. |
| Zither | Sampler | – | Aufgenommene Zither: schimmernde gezupfte Saiten (CC0). |

### Drums (11)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Cluster Hat | Cluster | – | Gestapelte schmale Pulse, zu einer metallischen Hat verstimmt. |
| FM Snare | FM | – | Feedback-Rauschen plus Körperton: eine knackige digitale Snare. |
| FM Tom | FM | – | Gestimmte FM-Trommel mit holzigem Klopfen. |
| Filter Kick | Analog | – | Kick aus einem selbstoszillierenden Ladder-Filter, von der Hüllkurve nach unten gefahren. Um C1–C2 spielen. |
| Metal Hat FM | FM | – | Unharmonisches FM-Rauschen: geschlossene und offene Hats je nach Tastenlänge. |
| Noise Snare | Analog | – | Bandpass-Rauschen und Ton: eine elektronische Snare. |
| Sub Kick | Analog | Bass, Boom | Stimmbare tiefe Kick: ein selbstoszillierendes Filter, auf die Taste gestimmt und von oben heruntergezogen; langer Boom, angezerrt. |
| Tick Hat | Analog | – | Hochpass-Rauschen als Tick für Hi-Hat-Muster. |
| Tuned Tom | Zupfen | – | Modellierter, trommelartiger Stab mit großem Korpus; in Oktaven spielen. |
| Wood Block | Zupfen | – | Harter Schlägel auf einem kurzen Holzstab. |
| Zap Perc | Wavetable | FX | Digital-Tabelle, schlagartig geschlossen: Zaps, Klicks und Laser. |

### FX (19)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Alien Signal | FM | – | Zufällig springende FM-Klangfarbe: Funkverkehr aus der Ferne. |
| Detune Swarm | Cluster | Texturen | Sieben wild driftende Oszillatoren: ein Schwarm. |
| Doppler Fly-By | Wavetable | 3D | Ein Sirenenton rast von links nach rechts und zurück an dir vorbei – die Tonhöhe biegt sich beim Vorbeiflug durch den Doppler-Effekt. |
| Downlifter | Analog | – | Fallender Rauschen- und Sägezahn-Sweep über drei Sekunden mit Jet-Flanger – das Ende des Builds. |
| Glitch Spray | Grain | Texturen | Winzige, schnelle Grains, gecrusht und in ein Stereo-Delay gestreut: digitaler Sprühnebel. |
| Gong Hit | FM | Texturen | Unharmonischer FM-Gong mit langem Schimmer. |
| Granular Rise | Grain | – | Körner tasten vorwärts und steigen, solange die Taste gehalten wird. |
| Grit Sweep | Wavetable | – | Rauschige Grit-Tabelle, die sich über zwei Sekunden öffnet. |
| Impact Boom | FM | Boom | Sub-Impact mit metallischem Knall, gesquasht und in einen riesigen Hall geworfen. |
| Laser Drop | Analog | Drums | Resonanter fallender Sweep; kurze Anschläge werden zu Lasern. |
| Noise Riser | Analog | – | Taste halten: gefiltertes Rauschen steigt vier Sekunden lang an. |
| Reverse Spray | Grain | Texturen | Überwiegend rückwärts gespielte Harfenkörner, über das Stereofeld verteilt. |
| Siren Pan | FM | – | Heulende FM-Sirene, deren Tonhöhe schwingt und über die Lautsprecher wandert. |
| Spiral Riser | Analog | 3D | Taste halten: Rauschen und Sägezähne steigen vier Sekunden lang und schrauben sich dabei von unterhalb der Ohren hoch über deinen Kopf. |
| Stereo Zap | Wavetable | – | Laser-Zap, der zwei Oktaven abtaucht, von einem Ping-Pong-Delay nach links und rechts geworfen. |
| Stutter Glitch | Grain | – | Winzige, spärliche Körner: glitchiges Stottern aus der Marimba. |
| Table Scanner | Wavetable | Texturen | Digital-Tabelle, von einem langsamen LFO gefahren: robotische Bewegung. |
| Whoosh | Analog | – | Gefiltertes Rauschen, von einem Ein-Takt-Bandpass-Wobble gefahren, breit gespreizt: ein vorbeiziehendes Whoosh. |
| Wide Riser | Analog | – | Rauschen und Sägezähne, die über vier Sekunden ansteigen, geflangt und breit, in einen langen Hall. |

### Arp (21)

Presets mit *Arp:* schalten den Arpeggiator für den Part ein, in den sie geladen werden; er spielt, solange Tasten gehalten werden, und stoppt beim Loslassen (*Halten* auf der Sequenzer-Seite lässt ihn weiterlaufen). Ein Preset ohne Arp in diesem Part schaltet ihn wieder aus. Siehe [PRESETS.md](PRESETS.md#arp-block).

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Analog Arp | Analog | Zupfer | Resonante Rechteck-Blips mit Ping-Pong-Delay. *Arp: Auf/Ab · 1/16 · 2 Okt.* |
| Bell Cascade Arp | FM | Tasten | FM-Glocken, die in Triolen über drei Oktaven herabfallen, mit Echos und hellem Hall. *Arp: Abwärts · 1/16T · 3 Okt.* |
| Bubble Arp | Analog | – | Schmaler Puls durch ein resonantes Filter: blubbernd und rund. *Arp: Aufwärts · 1/16 · 2 Okt.* |
| Chord Stab Arp | Cluster | Tasten | Rhythmische Akkord-Stabs (Akkord-Modus, Achtel) aus einem breiten Sägezahn-Stapel – sofort House-Groove. *Arp: Akkord · 1/8 · 1 Okt.* |
| Digital Pluck Arp | FM | – | Klickender digitaler Pluck für schnelle Sechzehntel. *Arp: Aufwärts · 1/16 · 2 Okt.* |
| Dream Tine Arp | Sampler | Tasten | Tine-Piano-Arpeggio in Achteln mit Chorus, Delay und Hall: verträumt und voll. *Arp: Auf/Ab · 1/8 · 2 Okt.* |
| FM Arp Bell | FM | Tasten | Kurze gläserne FM-Glocken für Sequenzen. *Arp: Auf/Ab · 1/16 · 2 Okt.* |
| Figure Eight Arp | Wavetable | 3D | Digitales Arpeggio, das eine liegende Acht um dich zeichnet – links vorn, quer, rechts hinten – über zwei Takte. *Arp: Auf/Ab · 1/16 · 2 Okt.* |
| Formant Arp | Wavetable | Vokal | Vokal-Blips für sprechende Sequenzen. *Arp: Aufwärts · 1/16 · 2 Okt.* |
| Glock Arp | Sampler | Tasten | Aufgenommenes Glockenspiel für funkelnde Arpeggien (CC0). *Arp: Aufwärts · 1/16 · 2 Okt.* |
| Mallet Echo Arp | Sampler | Zupfer | Glockenspiel-Arpeggio mit langen punktierten Echos – funkelnd und breit. *Arp: Aufwärts · 1/8 · 3 Okt.* |
| Neuro Arp | Wavetable | Bass | Knurrende FM-Sechzehntel in einer Oktave: eine rollende Neuro-Sequenz aus einer gehaltenen Note. *Arp: Wie gespielt · 1/16 · 1 Okt.* |
| Orbit Arp | Zupfen | 3D | Sechzehntel-Zupf-Arpeggio, das einmal pro Takt um deinen Kopf fliegt – nah, hell und schnell. *Arp: Aufwärts · 1/16 · 2 Okt.* |
| Pizzicato Arp | Zupfen | Zupfer | Kurze gezupfte Saiten für Ostinati. *Arp: Auf/Ab · 1/16 · 2 Okt.* |
| Random Spark Arp | FM | FX | Zufällige FM-Blips in Zweiunddreißigsteln über vier Oktaven durch ein Ping-Pong-Delay: Funken. *Arp: Zufall · 1/32 · 4 Okt.* |
| Ripple Arp | Analog | – | Analoges Sägezahn-Arp in Sechzehnteltriolen mit schwebendem Phaser und Stereo-Echo. *Arp: Aufwärts · 1/16T · 2 Okt.* |
| Sparkle Bell | FM | Zupfer | Kurze, helle FM-Glocke für funkelnde Arpeggios über Akkorden. *Arp: Aufwärts · 1/16 · 2 Okt.* |
| Spiral Arp | FM | 3D | Glocken-Arpeggio, das sich über vier Takte von Ohrhöhe bis über deinen Kopf hochschraubt und für die nächste Runde wieder fällt. *Arp: Aufwärts · 1/16 · 3 Okt.* |
| Table Arp | Wavetable | – | Wavetable-Sequenzklang: die Position springt mit jeder Note. *Arp: Auf/Ab · 1/16 · 2 Okt.* |
| Trance Gate Arp | Cluster | Leads | Supersaw-Stapel auf einem Sechzehntel-Arp über zwei Oktaven, Ping-Pong-Delay und Hall. *Arp: Aufwärts · 1/16 · 2 Okt.* |
| Wide Pluck Arp | Wavetable | Zupfer | Sechsstimmiger Stereo-Pluck, auf/ab über drei Oktaven, punktiertes Ping-Pong-Delay. *Arp: Auf/Ab · 1/16 · 3 Okt.* |

### Texturen (26)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Backwards Kalimba | Sampler | FX | Die Kalimba rückwärts: Anschwellen, das in einem Zupfer endet. |
| Breath Bore | Vocal | MPE | Eine flüsternde Stimme, durch ein offenes Rohr geblasen: luftige Flötentöne mit weicher Kante. |
| Breath Texture | Vocal | Vokal | Fast stimmloser Atem durch langsam wandernde Vokale. |
| Breathing Machine | Vocal | Vokal, MPE | Gehauchter Vokal-Drone, dessen Vokal sich über zwei Takte öffnet und schließt. |
| Dark Matter | Wavetable | Flächen, MPE | Tiefe Grit-Tabelle mit kriechendem Phaser und langsam wandernder Position – bedrohlich. |
| Deep Space Drone | Cluster | Flächen, MPE | Tief geschichtete Quarten, ein driftender Kammfilter obenauf und ein endloser Hall. |
| Dust Orchestra | Grain | MPE | Zither-Grains, vor- und rückwärts geworfen, mit Stereo-Delay-Spur – ein Orchester aus Staub. |
| FM Grains | Grain | Hybrid, MPE | Grains aus gestrichenem Metall, ringmoduliert von einem FM-Glocken-Layer: Grains, die gestimmt scheppern. |
| Fourths Drift | Cluster | Flächen | Gestapelte Quarten mit starker Drift: ein ungelöster Dunst. |
| Glacier | Grain | MPE | Glas-Grains, die mit gletscherhafter Scan-Rate kriechen, sehr breit, endloser Hall. |
| Glass Swarm | Grain | Flächen, MPE | Dichte Wolke aus Klangglas-Grains, acht Stimmen tief, mit Chorus in einen riesigen Hall. |
| Grit Drone | Wavetable | – | Langsam brodelnde Grit-Tabelle; eine tiefe Note halten. |
| Harp Cloud | Grain | Flächen, MPE | Harfen-Aufnahmen, zu einer dichten Wolke verstreut, mit Ping-Pong-Echos. |
| Harp Mist | Grain | Flächen | Lange, weiche Harfenkörner, zu einem Nebel verwischt. |
| Kalimba Dust | Grain | – | Winzige Kalimba-Körner, verstreut wie Staub. |
| Kalimba Storm | Grain | MPE | Kalimba-Grains in voller Dichte mit rückwärts gespielten Fragmenten – ein rasselnder, gestimmter Sturm. |
| Marimba Rain | Grain | – | Spärliche Marimba-Körner, die wie Regen fallen. |
| Metal Choir | Grain | Flächen, MPE | Grains aus gestrichenem Metall zu einem Chor gestapelt; langsamer Scan, schimmerndes Stereo. |
| Night Rain | Grain | MPE | Zufällige Kalimba-Tropfen, die in ein Ping-Pong-Delay und einen dunklen Hall fallen. |
| Night Swarm | FM | 3D, MPE | Gläserne FM-Glocken auf einem schnellen Zufallsorbit in wechselnden Entfernungen: ein Lichterschwarm um deinen Kopf. |
| Recorder Breath | Grain | Vokal | Hauchige Blockflöten-Körner: luftige, stimmähnliche Töne. |
| Rust Texture | Grain | FX, MPE | Grains aus gestrichenem Metall, rückwärts durch einen langsamen Kammfilter: rostig, knarzend, lebendig. |
| Singing Glass | Sampler | Flächen | Geriebene Weinglas-Aufnahmen (CC0), geloopt: rein und gespenstisch. |
| Starfield | FM | Tasten, MPE | Zufällige hohe FM-Glocken, die durch das Stereofeld in Delay und Hall treiben. |
| Vinyl Crackle | Analog | FX | Platten-Knistern und leises Rauschen (Analog-Engine: Oszillatoren aus, Knistern und Rauschen an); eine Note unter einem Beat halten. |
| Zither Cloud | Grain | – | Schimmernde Zither-Körner. |

### Vokal (9)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Aah Choir | Vocal | Flächen | Gemischter Chor auf „Ah“. |
| Choir Grains | Vocal | Hybrid, MPE | Ah-Chor, verdoppelt von einer Blockflöten-Grain-Wolke eine Oktave höher – menschlich und synthetisch zugleich. |
| Formant Voices | Wavetable | Flächen | Formant-Wavetable mit Unisono: synthetischer Chor. |
| Gospel Hum | Vocal | – | Summen mit geschlossenem Mund: warm, tief und intim. |
| Male Ensemble | Vocal | Flächen | Tiefe Stimmen auf „Oh“: ein Männerchor. |
| Ooh Choir | Vocal | Flächen | Weicher „Uh“-Chor, rund und dunkel. |
| Solo Soprano | Vocal | Leads | Hohe Solostimme: helle Formanten, großzügiges Vibrato. |
| Talking Synth | Vocal | Leads | Der LFO wandert durch die Vokale: ein sprechendes Lead. |
| Vowel Morph | Vocal | – | Jede Note singt „Ih“ und öffnet sich mit der Hüllkurve zu „Ah“. |

### Boom (11)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Boom Click | Analog | Bass | Sinus-Sub mit hellem Klick im Anschlag, damit der Boom auch auf kleinen Lautsprechern durchkommt. |
| Boom Distorted | Analog | Bass | Sinus-Sub, hart ins Ladder-Filter und den FX-Slot gefahren: ein knurrender, verzerrter Boom. |
| Boom Kick Bass | Analog | Drums, Bass | Kurzer Boom: ein Pitch-Dive über zwei Oktaven in einen gestimmten Sinus – Kick und Bass in einem. |
| Boom Long Sub | Analog | Bass | Langer, gestimmter Sinus-Sub mit schnellem Pitch-Dive im Anschlag; klingt sekundenlang aus. |
| Boom Slide | Analog | Bass | Langer Sinus-Sub für Slides im Trap-Stil: Noten überlappen und er gleitet (Legato, feste Zeit). |
| Dirty Sub | Analog | Bass | Sub, ins Ladder-Filter gefahren: knurrende Obertöne, die auch auf kleinen Lautsprechern hörbar bleiben. |
| Impact Boom | FM | FX | Sub-Impact mit metallischem Knall, gesquasht und in einen riesigen Hall geworfen. |
| Long Sub | Analog | Bass | Tiefer, lang ausklingender Sub, der zwischen überlappenden Noten gleitet (Glide); weicher Klick im Anschlag. |
| Punch Sub | Analog | Bass | Kurzer, druckvoller Sub mit knackigem Filterklick; rastet mit der Kick ein. |
| Sine Slide | FM | Bass | Fast reiner Sinus-Sub mit langem Glide für gleitende Basslinien. |
| Sub Kick | Analog | Drums, Bass | Stimmbare tiefe Kick: ein selbstoszillierendes Filter, auf die Taste gestimmt und von oben heruntergezogen; langer Boom, angezerrt. |

### Hybrid (18)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Breath Lead | Vocal | Leads, MPE | Gehauchter, gesungener Ton, verschmolzen mit einem weichen Sägezahn-Stapel: ein Lead zwischen Stimme und Synth. |
| Choir Grains | Vocal | Vokal, MPE | Ah-Chor, verdoppelt von einer Blockflöten-Grain-Wolke eine Oktave höher – menschlich und synthetisch zugleich. |
| Chop Pad | Cluster | Flächen, MPE | Eine breite Akkordfläche, zerhackt und gestottert von der Hüllkurve eines Kalimba-Grain-Layers. |
| Crystal Organ | FM | Tasten, MPE | Additive FM-Orgel mit Glas-Wavetable-Layer, die mit langsamem Autopan im Stereofeld kreist. |
| Digital Monster | Wavetable | Bass, MPE | Digitale FM-Tabelle, gegatet von einem Glas-Grain-Layer eine Oktave tiefer, dann gewobbelt: instabil und riesig. |
| FM Grains | Grain | Texturen, MPE | Grains aus gestrichenem Metall, ringmoduliert von einem FM-Glocken-Layer: Grains, die gestimmt scheppern. |
| Fold Bass | Wavetable | Bass, MPE | Sinus-Sub und analoger Sägezahn, ineinander gefaltet: die Faltmenge lässt ihn brüllen. |
| Glass Engine | Grain | Flächen, MPE | Klangglas-Grain-Wolke, verschmolzen mit einem harmonischen Wavetable-Layer, der langsam scannt; ein Hauch Ringmodulation. |
| Halo Grand | Sampler | Tasten, Klavier, MPE | Flügel mit Heiligenschein: seine eigene Aufnahme, eingefroren zu einer Grain-Wolke, die hinter jedem Akkord aufblüht. |
| Metal Strings | Zupfen | Flächen, MPE | Metallische gezupfte Saiten, gefaltet in eine langsame Cluster-Fläche – ein Orchester aus Draht. |
| Neuro Hybrid | FM | Bass, MPE | FM-Growl und Wavetable-Reese, die sich gegenseitig ringmodulieren, gefaltet und gesquasht: ein Neuro-Monster. |
| Orbit Pad | Grain | Flächen, MPE | Harfen-Grains, die einen FM-Layer umkreisen, sanft ringmoduliert und über zwei Takte pannend. |
| Pluck Nebula | Zupfen | Zupfer, MPE | Eine gezupfte Saite, die sich in eine Kalimba-Grain-Wolke auflöst: die Saite verklingt, die Wolke schwillt an und bleibt. |
| Resonator Hit | Zupfen | Zupfer, MPE | Ein angeschlagener Metallstab, der eine kurze Grain-Resonanz und einen gestimmten Kammfilter speist: perkussiv, klingend, seltsam. |
| Ring Machine | Analog | Leads, MPE | Sägezahn-Lead, ringmoduliert von einem FM-Sinus eine Quinte höher: metallisch, vokal und aggressiv, mit Delay. |
| Spectral Morph | Wavetable | Flächen, MPE | Formant-Tabelle, die unter einem gehauchten Vokal-Layer morpht, beide durch einen driftenden Kammfilter: Spektren, die ineinander schmelzen. |
| Tape Ghost Keys | Sampler | Tasten, Klavier, MPE | Tine-Piano mit einem Geist seiner selbst: eine verstimmte Grain-Kopie, gecrusht und geflangt wie ein altes, neu aufgenommenes Band. |
| Vox Reese | Wavetable | Bass, MPE | Ein Reese mit singendem Vokal-Layer obenauf: die Formanten reiten auf dem Bass, ein synchrones Vokalfilter lässt ihn sprechen. |

### Klavier (14)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Bright Grand | Sampler | Tasten, MPE | Flügel, für Pop geöffnet: heller, leicht gesquasht, kurzer Raum. |
| Classic Tine | Sampler | Tasten, MPE | Modelliertes Tine-E-Piano: glockig bei leichtem Anschlag, bellend bei hartem. |
| Concert Grand | Sampler | Tasten, MPE | Flügel-Aufnahmen, zwei Anschlagsstufen, ein Hauch Hall. |
| Dream Tine | Sampler | Tasten, Flächen, MPE | Tine-Piano, getränkt in Chorus, punktiertes Delay und langen Hall. |
| FM Bark Piano | FM | Tasten, MPE | FM-E-Piano aus zwei Paaren: das Tine-Paar klingt, das Bark-Paar beißt mit der Anschlagstärke. |
| Felt Grand | Sampler | Tasten, MPE | Dunkler, weicher Flügel: Höhen abgesenkt, langsames Release und warmer Hall – intim. |
| Halo Grand | Sampler | Tasten, Hybrid, MPE | Flügel mit Heiligenschein: seine eigene Aufnahme, eingefroren zu einer Grain-Wolke, die hinter jedem Akkord aufblüht. |
| Piano Grains | Grain | Flächen, Texturen, MPE | Flügel-Aufnahmen, eingefroren zu einer langsam ziehenden Grain-Wolke: ein Klavier, das atmet. |
| Reed Keys | Sampler | Tasten, MPE | Modelliertes Zungen-E-Piano: nasal und holzig, mit rauem Bellen, wenn man es anfährt. |
| Reed Tremolo | Sampler | Tasten, MPE | Zungen-Piano durch sein klassisches Lautstärke-Tremolo – Soul und Gospel. |
| Suitcase Tine | Sampler | Tasten, MPE | Tine-Piano mit dem Stereo-Autopan eines Koffer-Verstärkers und kleinem Raum. |
| Tape Ghost Keys | Sampler | Tasten, Hybrid, MPE | Tine-Piano mit einem Geist seiner selbst: eine verstimmte Grain-Kopie, gecrusht und geflangt wie ein altes, neu aufgenommenes Band. |
| Tine Bloom | Grain | Flächen, Texturen, MPE | Tine-Piano-Grains, die langsam aufblühen, mit Chorus und punktiertem Echo. |
| Upright Keys | Sampler | Tasten, MPE | Klavier-Aufnahmen (Upright) mit etwas Raum: ehrlich und nah. |

### 3D (13)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Binaural Lead | Wavetable | Leads, MPE | Ausdrucksstarkes Lead vor dir: Slide hebt es über deinen Kopf, Druck holt es bis ans Ohr, eine langsame Drift hält es lebendig. Für MPE-Controller und Kopfhörer. |
| Doppler Fly-By | Wavetable | FX | Ein Sirenenton rast von links nach rechts und zurück an dir vorbei – die Tonhöhe biegt sich beim Vorbeiflug durch den Doppler-Effekt. |
| Figure Eight Arp | Wavetable | Arp | Digitales Arpeggio, das eine liegende Acht um dich zeichnet – links vorn, quer, rechts hinten – über zwei Takte. *Arp: Auf/Ab · 1/16 · 2 Okt.* |
| Halo Drift | Cluster | Flächen, MPE | Verstimmter Cluster-Chor, der auf einem langsamen Zufallsorbit um und über dir wandert – nie zweimal am selben Ort. |
| Neuro Swirl | Wavetable | Bass, MPE | Sprechender Formant-Growl, der einmal pro Takt eine Acht zieht – der Vokal bewegt sich, der Raum bewegt sich, der Sub nicht. |
| Night Swarm | FM | Texturen, MPE | Gläserne FM-Glocken auf einem schnellen Zufallsorbit in wechselnden Entfernungen: ein Lichterschwarm um deinen Kopf. |
| Orbit Arp | Zupfen | Arp | Sechzehntel-Zupf-Arpeggio, das einmal pro Takt um deinen Kopf fliegt – nah, hell und schnell. *Arp: Aufwärts · 1/16 · 2 Okt.* |
| Orbit Atmos | Wavetable | Flächen, MPE | Breite Wavetable-Atmosphäre, die alle acht Takte auf einer leicht geneigten Bahn um deinen Kopf kreist, in einem weichen Raum. Am besten mit Kopfhörern: Slide hebt sie an, Druck holt sie näher. |
| Orbit Reese | Wavetable | Bass, MPE | Neurofunk-Reese, dessen knurrende Mitten alle zwei Takte um deinen Kopf kreisen, während der Sub unter 160 Hz genau in der Mitte und mono bleibt – clubtaugliche Bewegung. |
| Overhead Choir | Vocal | Flächen, Vokal, MPE | Hauchiger „Uh“-Chor, der von vorn über deinen Kopf nach hinten und zurück schwingt – ein Pendel durch den Raum. |
| Pendulum Wobble | Wavetable | Bass, MPE | Halftime-Dubstep-Wobble, der mit dem Takt über deinen Kopf und zurück schwingt: Wobble und Raum atmen zusammen. |
| Spiral Arp | FM | Arp | Glocken-Arpeggio, das sich über vier Takte von Ohrhöhe bis über deinen Kopf hochschraubt und für die nächste Runde wieder fällt. *Arp: Aufwärts · 1/16 · 3 Okt.* |
| Spiral Riser | Analog | FX | Taste halten: Rauschen und Sägezähne steigen vier Sekunden lang und schrauben sich dabei von unterhalb der Ohren hoch über deinen Kopf. |

### Projekte / Demo-Patterns (14)

| Name | Kategorien | Beschreibung |
|---|---|---|
| Lo-Fi Beat | Drums, Tasten, Bass | Klassischer Lo-Fi-Hip-Hop mit 78 BPM und trägem, schwerem Swing: dumpfe Kick, weiche Snare und leise geshuffelte Hats auf dem Lo-Fi-Kit (Euklid), grundtonlose Nonen-Voicings auf Dusty Keys mit starkem Band-Leiern (Orbit-Spuren 1–3), ein warmer, runder Bass auf den Grundtönen (Step-Sequenzer), Vinyl-Knistern (Orbit-Spur 4). Fm9 – Bb13 – Ebmaj9 – C7#9; dunkler Master (Tiefpass, Bandsättigung). Play drücken. |
| Techno Groove | Drums, Bass | Treibender Four-on-the-floor auf dem Hybrid-Kit aus Orbit (Claps auf 2 und 4, Rims, akzentuierte 16tel-Hats, offene Offbeat-Hat), ein rollender Bass aus dem Step-Sequenzer und dunkle Septakkord-Stabs aus Euklid mit langsam wanderndem Filter und synchronisiertem punktiertem Achtel-Delay. |
| Ambient Drift | Flächen, Texturen | Langsame Quinten-Fläche, eine generative Kalimba aus vier Orbit-Spuren mit 5, 7, 11 und 16 Steps mit tempo-synchronem punktiertem Achtel-Delay, spärliche Klangglas-Töne aus Euklid. Ohne Drums. |
| Synth-Pop | Drums, Bass, Leads | Heller Four-on-the-floor auf dem Trocken-Kit aus Euklid (Snare und Clap auf 2 und 4, Achtel-Hats), ein hüpfender Oktav-Bass, üppiges Poly-Brass und ein funkelndes Arpeggio aus drei Orbit-Spuren über Am–F–C–G und eine Lead-Melodie aus dem Step-Sequenzer. |
| Euclid Polyrhythm | Drums, Arp | Vier Euklid-Ringe mit 8, 12, 16 und 9 Steps auf dem Room-Kit, eine Marimba aus Orbit-Spuren mit 5, 6, 7 und 8 Steps und ein Orgelpunkt-Bass. |
| Arp Sequence | Arp, Drums | Ein 64-Step-Arpeggio über Am, Fmaj7, G und Em auf einem A-Orgelpunkt, Studio-Kit-Drums aus Orbit. Der Arpeggiator spielt Part 3: Akkorde halten. |
| Deep House | Drums, Bass, Tasten | Geshuffelte Hats und eine tiefe Kick auf dem Tight-Kit (Orbit), ein synkopierter Orgel-Bass (Step-Sequenzer) und Euklid-Akkord-Stabs. |
| Sub & Glide | Flächen, Bass, Drums, Boom | Septakkorde, die ineinander gleiten (Glide Strings, Poly-Glide), ein langer gleitender Sub auf Orbit-Spur 1 und ein Halftime-Beat auf dem Boom-Kit aus den Orbit-Spuren 2–4, funkelnde Glocken aus Euklid. Cm7 – Abmaj7 – Ebmaj7 – Bb7. |
| Lofi Kit | Drums | Das Lo-Fi-Kit für Lo-Fi-Hip-Hop eingestellt: dumpfe, runde Kick, weiche Snare mit Ghost Notes und Rim, matte geshuffelte Hats mit Anschlags-Variation (Orbit, träger Swing), Vinyl-Knistern auf Part 4 (Orbit-Spur 4) und ein dunkler Master. Eigene Keys auf Part 1 spielen. |
| Boom Kit | Drums, Bass, Boom | Das synthetisierte Boom-Kit in einem Halftime-Beat mit rollenden Hats (Orbit), Boom Slide auf dem Step-Sequenzer – überlappende Noten gleiten (Legato-Portamento) – und eine dunkle Fläche, die in ihren Akkord steigt (Rise Glass, Euklid). c-Moll, 70 BPM. |
| Neuro Engine | Bass, Drums, Hybrid, 3D | Neurofunk mit 174 BPM in f-Moll: Two-Step-Beat auf dem Hybrid-Kit (Orbit), Neuro Reese riffend auf dem Step-Sequenzer, Growl Machine antwortet aus Euklid, darunter eine dunkle Septakkord-Fläche. Mono-Tiefbass, gesquasht und laut. Play drücken. In 3D: die Growl-Antworten wirbeln um deinen Kopf, die Fläche treibt über dir (am besten mit Kopfhörern). |
| Halftime Pressure | Bass, Drums | Moderner Dubstep mit 140 BPM in e-Moll: Half-Time-Drums auf dem Studio-Kit (Orbit), Wub Machine auf dem Step-Sequenzer – der Achtel-Wobble startet mit jeder Note neu –, Vowel Growl antwortet aus Euklid, dazu eine kalte Glas-Textur. Play drücken. |
| Liquid Roll | Bass, Drums, Tasten | Liquid Drum & Bass mit 174 BPM in d-Moll: rollender Beat auf dem Tight-Kit (Orbit), verträumte Tine-Piano-Septakkorde und eine warme Fläche (Orbit-Spuren 3 und 4), ein weicher, gleitender Sub (Step-Sequenzer) und gläserne Plucks aus Euklid. Play drücken. |
| Evolving Space | Flächen, Texturen, Hybrid, 3D | Ambient mit 66 BPM in a-Moll: Evolving-Tide-Akkorde, die durch ihre Wavetable driften (Step-Sequenzer), ein generativer Flügel mit Grain-Heiligenschein aus drei Orbit-Spuren mit 5, 7 und 11 Schritten, ein Gletscher-Grain-Drone und zufällige Sternenfeld-Glocken (Euklid). Einfach laufen lassen. In 3D: die Akkorde kreisen langsam um dich, die Sternenfeld-Glocken verstreuen sich ringsum und die Drone schwingt über dir (am besten mit Kopfhörern). |

**Engines:** FM 42, Wavetable 79, Analog 68, Zupfen 17, Grain 27, Vocal 20, Cluster 23, Sampler 26
<!-- presets:end -->
