# Werksklänge

*Deutsch · English: [../en/FACTORY-SOUNDS.md](../en/FACTORY-SOUNDS.md)*

Ohpe bringt **162 Part-Presets** für alle acht Engines mit, **10 Projekt-Presets** (Demo-Patterns,
die alle vier Parts, die Sequenzer und das Drum-Kit nutzen), **sechs Drum-Kits** und **zehn
aufgenommene Instrumente** für die Sampler- und Grain-Engine. Du öffnest sie mit **BIBLIOTHEK**:
links eine Kategorie wählen (Bass, Tasten, Flächen, Leads, Zupfer, Drums, FX, Arp, Texturen, Vokal, Boom)
oder eine Engine, dann durch die Liste klicken – jedes Preset lädt sofort, du hörst es beim Stöbern.

## Presets spielen

* Ein **Part-Preset** lädt in den bearbeiteten Part (Part-Wahl 1–4). Sein **Part-Gain**
  (`pN.gain`, dB) gleicht die Lautheit an die anderen Werksklänge an: alle liegen beim Spielen bei
  etwa −18 … −12 dBFS RMS, beim Umschalten springt der Pegel also nicht.
* Der **FX-Slot** (Chorus, Phaser, Bitcrush, Filter-Drive, Delay, Hall) gibt es **nur auf Part 1**.
  Presets, die ihn nutzen (der Chorus der E-Pianos, das Delay der Leads, der Hall der Zupfer …),
  klingen auf Part 1 am besten; auf Part 2–4 wird der Slot ignoriert und der Klang ist trocken –
  dort stattdessen die Send-Effekte des Mixers nehmen (Send A Delay, Send B Hall).
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
### Bass (26)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Acid Line | Analog | Leads | Schmatzender, resonanter Sägezahn mit Sub für Gewicht, Akzent über die Anschlagstärke und kurzem Glide. |
| Bounce Bass | Analog | – | Heller, druckvoller Sägezahn-Bass für hüpfende Oktav-Achtel. |
| Dirty Sub | Analog | Boom | Sub, ins Ladder-Filter gefahren: knurrende Obertöne, die auch auf kleinen Lautsprechern hörbar bleiben. |
| Fat Saw Bass | Analog | – | Zwei verstimmte Sägezähne und ein Sub durch das Ladder-Filter; groß, aber nicht dröhnend. |
| Glide Sine Bass | FM | – | Reiner Sinus-Bass mit Legato-Glide für gleitende Basslinien; abgesetzte Noten bleiben sauber. |
| Grit Bass | Wavetable | – | Knurrende Grit-Tabelle mit Hüllkurven-Sweep; aggressiv, aber kontrolliert. |
| Growl FM Bass | FM | – | FM-Bass mit Feedback-Kante und langsamem Klangfarben-LFO. |
| House Organ Bass | Analog | – | Hohles Rechteck mit zweitem Oszillator eine Oktave tiefer; kurz und druckvoll. |
| Lofi Bass | Analog | – | Runder, warmer Bass: weicher Rechteck-Körper über kräftigem Sinus-Sub, sanft angezerrt. |
| Log Drum Bass | Sampler | – | Tiefe Marimba-Stäbe als Bass: holzig, rund und kurz. |
| Long Sub | Analog | Boom | Tiefer, lang ausklingender Sub, der zwischen überlappenden Noten gleitet (Glide); weicher Klick im Anschlag. |
| Mellow Lofi Bass | Analog | – | Warmer, runder Bass: weicher, tiefpassgefilterter Körper über einem Sinus-Sub mit sanfter Sättigung und einem Hauch Glide. |
| Punch FM Bass | FM | – | Solider digitaler Bass mit hellem Anschlag und sauberem Sinus-Körper. |
| Punch Sub | Analog | Boom | Kurzer, druckvoller Sub mit knackigem Filterklick; rastet mit der Kick ein. |
| Reese Drift | Analog | – | Breite, verstimmte Sägezähne, die langsam gegeneinander schweben; dunkel und bewegt. |
| Rise Bass | Analog | – | Angezerrter Bass, der bei jeder Note eine Quinte von unten hineingleitet; ideal für synkopierte Linien. |
| Rolling Bass | Analog | – | Straffer, angezerrter Techno-Bass für rollende Sechzehntel: kurzer Filter-Snap, solider Sub. |
| Round Sub | Analog | – | Warmes Rechteck mit Sub-Oszillator und weichem Filter-Blip; passt unter alles. |
| Rubber Bass | Analog | Zupfer | Federnder gezupfter Bass: schneller Filterverlauf, runder Sub. |
| Sine Slide | FM | Boom | Fast reiner Sinus-Sub mit langem Glide für gleitende Basslinien. |
| Slap FM Bass | FM | – | Perkussiver FM-Bass: der Index schnappt zu wie eine geslappte Saite. |
| Table Sub | Wavetable | – | Fast-Sinus-Wavetable-Bass mit Sub; reiner Tiefbass mit etwas Kante. |
| Talking Bass | Wavetable | – | Formant-Tabelle, von der Hüllkurve gefahren: ein Vokal-„Wow“ auf jeder Note. |
| Unison Bass | Cluster | – | Oktav-gestapelte Sägezähne, leicht verstimmt; dick und breit. |
| Upright Pluck | Zupfen | Zupfer | Gezupfte Saite mit großem Holzkorpus: ein akustischer Bass. |
| Wobble Bass | Analog | FX | Filter-Wobble auf einem angezerrten Sägezahn-Stapel; LFO-Tempo nach Geschmack. |

### Tasten (22)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Cluster Organ | Cluster | – | Oktav-Stapel weicher Rechtecke: eine kitschige Combo-Orgel. |
| Digital EP | FM | – | Helleres E-Piano, das bei hartem Anschlag mehr bellt. |
| Drawbar Organ | FM | – | Additive Orgel: sechs Sinus-Zugriegel, sanftes Vibrato, Rotor-artiger Chorus. |
| Dub Stab | Analog | Flächen | Dunkler, verstimmter Akkord-Stab, dessen Filter langsam auf- und zugeht; mit Delay und Hall spielen. |
| Dusty EP | FM | – | Warmes E-Piano mit Band-Wow und -Flutter (langsames Tonhöhen-LFO) und Chorus; für Lo-Fi-Akkorde gemacht. |
| Dusty Keys | FM | – | Weiches, dunkles E-Piano für Lo-Fi-Akkorde: starkes Band-Leiern auf der Tonhöhe, bitreduzierte Kante, weicher Anschlag. |
| FM Vibes | FM | Zupfer | Weiche metallische Klangstäbe mit Tremolo. |
| Funk Clav | FM | Zupfer | Knackige Clavinet-artige Tasten; kurz und synkopiert spielen. |
| Glass Keys | Wavetable | – | Glockige Glass-Tabelle, die beim Ausklingen dunkler wird. |
| Glide Poly Synth | Analog | Leads | Poly-Synth, dessen Akkorde immer vom vorigen Akkord herübergleiten (Poly-Glide). |
| Harmonic Organ | Wavetable | – | Additive Harmonic-Tabelle: eine weiche, leicht hauchige Orgel. |
| Harpsichord | Sampler | Zupfer | Aufgenommenes Cembalo (sieben Töne, CC0); Barock-Linien und helle Begleitung. |
| Mallet Keys | Zupfen | Zupfer | Physikalisch modellierte Holzstäbe; weicher Schlägel. |
| Marimba | Sampler | Zupfer | Aufgenommene Marimba mit zwei Anschlagstufen. |
| Minor Stab | Cluster | – | Ein Finger spielt einen Moll-Septakkord-Stab; verstimmt und druckvoll. |
| Music Box | Sampler | Zupfer | Glockenspiel, weich gefiltert zur Spieluhr; Wiegenlieder und Spielzeug-Melodien. |
| Pop Brass | Analog | Flächen | Üppiges, verstimmtes Poly-Brass mit kurzem Anschwellen und Chorus: klassische Synth-Pop-Akkorde. |
| Scoop Keys | FM | – | E-Piano, bei dem jede Note einen Ganzton von unten anschleift: soulig, ein wenig lässig. |
| Soft Brass | Analog | Flächen | Polyphones Blech: langsames Filter-Anschwellen auf verstimmten Sägezähnen. |
| Tine Piano | FM | – | E-Piano: weicher Körper und glockiger Zungen-Anteil, der mit der Taste verklingt. Chorus im FX-Slot. |
| Velvet Stab | Analog | – | Kurzer gefilterter Akkord-Stab für House und Disco. |
| Vibraphone | Sampler | – | Aufgenommenes Vibrafon mit sanftem Motor-Tremolo. |

### Flächen (28)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Analog Dawn | Analog | – | Puls-Fläche, deren Filter sich langsam öffnet, mit langsamem Phaser. |
| Angel Pad | Vocal | Vokal | Hohe „Uh“-Stimmen, die langsam zu „Ah“ wandern. |
| Bowed Haze | Grain | – | Körner eines gestrichenen Vibrafons: ein weicher metallischer Schein. |
| Bowed Metal | Sampler | Texturen | Gestrichenes Vibrafon (CC0-Aufnahmen), geloopt: gläsern und unheimlich. |
| Brass Slide | Analog | Tasten | Anschwellendes Poly-Brass mit schnellem Portamento zwischen den Akkorden. |
| Breath Choir Pad | Vocal | Vokal | Luftiger „Ah“-Chor mit viel Atem. |
| Crystal FM Pad | FM | – | Fünf leicht verstimmte Sinus-Träger mit einem gemeinsamen Modulator. |
| Crystal Slide | FM | – | Schimmernde FM-Fläche: verstimmte Sinus-Träger, die zwischen Akkorden gleiten. |
| Drift Stack | Cluster | Texturen | Verstimmte Sägezahn-Stapel, die langsam von Akkord zu Akkord driften. |
| Fall Haze | Wavetable | FX | Dunstige Fläche, deren Noten eine Quinte von oben in den Akkord sinken. |
| Fifth Cloud | Cluster | – | Offene Quinten mit langsamer Drift: filmisch und neutral. |
| Frozen Cloud | Grain | Texturen | Granulares Einfrieren der eingebauten harmonischen Textur. |
| Glass Glide | Wavetable | Texturen | Breite Glas-Fläche mit langsamen, verträumten Gleitern zwischen den Akkorden. |
| Glass Grains | Grain | Texturen | Körner eines singenden Weinglases (CC0-Aufnahme), langsam abgetastet. |
| Glass Shimmer | Wavetable | Texturen | Glass-Tabelle mit breitem Unisono: luftige, glitzernde Fläche. |
| Glide Strings | Analog | – | Ensemble-Streicher, deren Akkorde ineinander gleiten (Poly-Glide); Legato-Akkordwechsel. |
| Harmonic Swell | Wavetable | – | Obertöne blühen auf, während die Fläche anschwillt. |
| Night Drive | Analog | Texturen | Dunkle resonante Fläche, die mit einem langsamen Filter-LFO atmet. |
| Rise Bloom | FM | – | Schimmernde FM-Fläche, die von einer Oktave darunter aufblüht; lang und hell. |
| Rise Choir | Vocal | Vokal | Luftiger „Ah“-Chor, der bei jedem Akkord eine Quarte von unten hineinschleift. |
| Rise Glass | Wavetable | Texturen | Breite Glas-Fläche: jede Note gleitet eine Quinte von unten an ihren Platz, verträumt und langsam. |
| Rise Strings | Analog | – | Ensemble-Streicher, deren Noten bei jeder neuen Note eine Oktave von unten in den Akkord steigen. |
| Rise Warmth | Cluster | – | Weicher, verstimmter Sägezahn-Stapel, dessen Noten sich einen Ganzton von unten in den Akkord lehnen. |
| Slide Choir | Vocal | Vokal | Luftiger Ah-Chor; jede Stimme gleitet zum nächsten Akkord. |
| Super Stack | Cluster | – | Sieben verstimmte Sägezähne pro Taste, breit und üppig. |
| Sus Horizon | Cluster | – | Sus2-Stapel, die sich nie auflösen; Ambient-Teppiche. |
| Vowel Pad | Wavetable | – | Formant-Tabelle, die zwischen Vokalen treibt; langsamer Einsatz. |
| Warm Strings | Analog | – | Ensemble-Streicher: verstimmte Sägezähne, eine Oktave höher, sanfter Chorus. |

### Leads (18)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Classic Mono Lead | Analog | – | Sägezahn-Lead mit etwas Vibrato, Glide und punktiertem Delay. |
| Digital Lead | Wavetable | – | Verbogene Digital-Tabelle mit Unisono: modern und durchsetzungsstark. |
| FM Brass Lead | FM | – | Blechiges FM-Lead: der Index steigt mit einem kurzen Anschwellen. |
| Fall Lead | Analog | FX | Rechteck-Lead, das bei jeder Note eine Quinte von oben hineinfällt. |
| Formant Lead | Wavetable | Vokal | Vokalartiges Lead, das sich mit der Hüllkurve öffnet. |
| Hot Wire | Analog | – | Angezerrter, resonanter Sägezahn für rockige Soli. |
| Legato Glide Lead | Analog | – | Mono-Sägezahn-Lead mit Legato-Portamento: überlappende Noten gleiten, abgesetzte Noten beginnen ohne Gleiten. |
| Octave Lead | Cluster | – | Oktav-gestapelte Pulse mit kurzem Filter-Blip. |
| Pop Lead | Analog | – | Heller Sägezahn plus Oktav-Rechteck mit Vibrato: ein klarer, singender Synth-Pop-Lead. |
| Portamento Square | Analog | – | Hohles Rechteck, das immer gleitet, mit fester Rate: große Sprünge gleiten länger als kleine Schritte. |
| Recorder | Sampler | – | Aufgenommene Altblockflöte, geloopt, mit leichtem Vibrato. |
| Rise Lead | Analog | – | Sägezahn-Lead, das bei jeder Note eine Oktave von unten heranschwingt; ein punktiertes Delay dahinter. |
| Saw Unison Lead | Wavetable | – | Classic-Tabelle in Sägezahn-Stellung mit breitem Unisono. |
| Sine Whistle | FM | – | Fast reiner Sinus mit einem Hauch Modulation; zarte Melodien. |
| Soft Flute Lead | Analog | – | Hauchiger, resonanter Tiefpass auf einem Rechteck: ein weicher Pfeifton. |
| Square Hero | Analog | – | Hohles Rechteck plus Oktave: ein helles, singendes Lead. |
| Super Lead | Cluster | – | Sieben-Sägezahn-Unisono-Lead: Festival-Format. |
| Vox Lead | Vocal | Vokal | Synthetische Solostimme mit ausdrucksvollem Vibrato und Glide. |

### Zupfer (14)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Concert Harp | Sampler | – | Aufgenommene Konzertharfe (acht Töne, CC0). |
| FM Koto | FM | – | Gezupftes FM: heller Anschlag, der schnell weicher wird. |
| Glass Pluck | Wavetable | – | Glass-Tabellen-Pluck mit glockigem Funkeln. |
| Harp Pluck | Zupfen | – | Lang klingende, harfenartige Saite mit weichem Anschlag. |
| Kalimba | Sampler | Tasten | Aufgenommenes Daumenklavier mit schnarrenden, holzigen Zungen (CC0). |
| Koto Pluck | Zupfen | – | Helle, sirrende Saite mit resonantem Korpus. |
| Muted Pick | Analog | – | Kurzer, abgedämpfter Rechteck-Pluck für schnelle Linien. |
| Nylon Guitar | Zupfen | – | Warme Nylonsaite, nahe am Schallloch gezupft. |
| Poly Pluck | Analog | – | Klassischer Synth-Pluck: knackiges Filter auf einem verstimmten Sägezahn-Paar. |
| Steel Drum FM | FM | Tasten | Tropische Steeldrum-Töne aus unharmonischer FM. |
| Steel String | Zupfen | – | Helle Stahlsaite, nahe am Steg gezupft. |
| Table Pluck | Wavetable | – | Wavetable-Pluck: heller Sägezahn-Frame, der schnell schließt. |
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

### FX (10)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Alien Signal | FM | – | Zufällig springende FM-Klangfarbe: Funkverkehr aus der Ferne. |
| Detune Swarm | Cluster | Texturen | Sieben wild driftende Oszillatoren: ein Schwarm. |
| Gong Hit | FM | Texturen | Unharmonischer FM-Gong mit langem Schimmer. |
| Granular Rise | Grain | – | Körner tasten vorwärts und steigen, solange die Taste gehalten wird. |
| Grit Sweep | Wavetable | – | Rauschige Grit-Tabelle, die sich über zwei Sekunden öffnet. |
| Laser Drop | Analog | Drums | Resonanter fallender Sweep; kurze Anschläge werden zu Lasern. |
| Noise Riser | Analog | – | Taste halten: gefiltertes Rauschen steigt vier Sekunden lang an. |
| Reverse Spray | Grain | Texturen | Überwiegend rückwärts gespielte Harfenkörner, über das Stereofeld verteilt. |
| Stutter Glitch | Grain | – | Winzige, spärliche Körner: glitchiges Stottern aus der Marimba. |
| Table Scanner | Wavetable | Texturen | Digital-Tabelle, von einem langsamen LFO gefahren: robotische Bewegung. |

### Arp (9)

Presets mit *Arp:* schalten den Arpeggiator für den Part ein, in den sie geladen werden (gehalten: eine Taste einmal drücken, er spielt weiter; eine neue Taste oder ein neuer Akkord wechselt). Ein Preset ohne Arp in diesem Part schaltet ihn wieder aus. Siehe [PRESETS.md](PRESETS.md#arp-block).

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Analog Arp | Analog | Zupfer | Resonante Rechteck-Blips mit Ping-Pong-Delay. *Arp: Auf/Ab · 1/16 · 2 Okt. · Halten* |
| Bubble Arp | Analog | – | Schmaler Puls durch ein resonantes Filter: blubbernd und rund. *Arp: Aufwärts · 1/16 · 2 Okt. · Halten* |
| Digital Pluck Arp | FM | – | Klickender digitaler Pluck für schnelle Sechzehntel. *Arp: Aufwärts · 1/16 · 2 Okt. · Halten* |
| FM Arp Bell | FM | Tasten | Kurze gläserne FM-Glocken für Sequenzen. *Arp: Auf/Ab · 1/16 · 2 Okt. · Halten* |
| Formant Arp | Wavetable | Vokal | Vokal-Blips für sprechende Sequenzen. *Arp: Aufwärts · 1/16 · 2 Okt. · Halten* |
| Glock Arp | Sampler | Tasten | Aufgenommenes Glockenspiel für funkelnde Arpeggien (CC0). *Arp: Aufwärts · 1/16 · 2 Okt. · Halten* |
| Pizzicato Arp | Zupfen | Zupfer | Kurze gezupfte Saiten für Ostinati. *Arp: Auf/Ab · 1/16 · 2 Okt. · Halten* |
| Sparkle Bell | FM | Zupfer | Kurze, helle FM-Glocke für funkelnde Arpeggios über Akkorden. *Arp: Aufwärts · 1/16 · 2 Okt. · Halten* |
| Table Arp | Wavetable | – | Wavetable-Sequenzklang: die Position springt mit jeder Note. *Arp: Auf/Ab · 1/16 · 2 Okt. · Halten* |

### Texturen (11)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Backwards Kalimba | Sampler | FX | Die Kalimba rückwärts: Anschwellen, das in einem Zupfer endet. |
| Breath Texture | Vocal | Vokal | Fast stimmloser Atem durch langsam wandernde Vokale. |
| Fourths Drift | Cluster | Flächen | Gestapelte Quarten mit starker Drift: ein ungelöster Dunst. |
| Grit Drone | Wavetable | – | Langsam brodelnde Grit-Tabelle; eine tiefe Note halten. |
| Harp Mist | Grain | Flächen | Lange, weiche Harfenkörner, zu einem Nebel verwischt. |
| Kalimba Dust | Grain | – | Winzige Kalimba-Körner, verstreut wie Staub. |
| Marimba Rain | Grain | – | Spärliche Marimba-Körner, die wie Regen fallen. |
| Recorder Breath | Grain | Vokal | Hauchige Blockflöten-Körner: luftige, stimmähnliche Töne. |
| Singing Glass | Sampler | Flächen | Geriebene Weinglas-Aufnahmen (CC0), geloopt: rein und gespenstisch. |
| Vinyl Crackle | Analog | FX | Platten-Knistern und leises Rauschen (Analog-Engine: Oszillatoren aus, Knistern und Rauschen an); eine Note unter einem Beat halten. |
| Zither Cloud | Grain | – | Schimmernde Zither-Körner. |

### Vokal (8)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Aah Choir | Vocal | Flächen | Gemischter Chor auf „Ah“. |
| Formant Voices | Wavetable | Flächen | Formant-Wavetable mit Unisono: synthetischer Chor. |
| Gospel Hum | Vocal | – | Summen mit geschlossenem Mund: warm, tief und intim. |
| Male Ensemble | Vocal | Flächen | Tiefe Stimmen auf „Oh“: ein Männerchor. |
| Ooh Choir | Vocal | Flächen | Weicher „Uh“-Chor, rund und dunkel. |
| Solo Soprano | Vocal | Leads | Hohe Solostimme: helle Formanten, großzügiges Vibrato. |
| Talking Synth | Vocal | Leads | Der LFO wandert durch die Vokale: ein sprechendes Lead. |
| Vowel Morph | Vocal | – | Jede Note singt „Ih“ und öffnet sich mit der Hüllkurve zu „Ah“. |

### Boom (10)

| Name | Engine | Weitere Kategorien | Beschreibung |
|---|---|---|---|
| Boom Click | Analog | Bass | Sinus-Sub mit hellem Klick im Anschlag, damit der Boom auch auf kleinen Lautsprechern durchkommt. |
| Boom Distorted | Analog | Bass | Sinus-Sub, hart ins Ladder-Filter und den FX-Slot gefahren: ein knurrender, verzerrter Boom. |
| Boom Kick Bass | Analog | Drums, Bass | Kurzer Boom: ein Pitch-Dive über zwei Oktaven in einen gestimmten Sinus – Kick und Bass in einem. |
| Boom Long Sub | Analog | Bass | Langer, gestimmter Sinus-Sub mit schnellem Pitch-Dive im Anschlag; klingt sekundenlang aus. |
| Boom Slide | Analog | Bass | Langer Sinus-Sub für Slides im Trap-Stil: Noten überlappen und er gleitet (Legato, feste Zeit). |
| Dirty Sub | Analog | Bass | Sub, ins Ladder-Filter gefahren: knurrende Obertöne, die auch auf kleinen Lautsprechern hörbar bleiben. |
| Long Sub | Analog | Bass | Tiefer, lang ausklingender Sub, der zwischen überlappenden Noten gleitet (Glide); weicher Klick im Anschlag. |
| Punch Sub | Analog | Bass | Kurzer, druckvoller Sub mit knackigem Filterklick; rastet mit der Kick ein. |
| Sine Slide | FM | Bass | Fast reiner Sinus-Sub mit langem Glide für gleitende Basslinien. |
| Sub Kick | Analog | Drums, Bass | Stimmbare tiefe Kick: ein selbstoszillierendes Filter, auf die Taste gestimmt und von oben heruntergezogen; langer Boom, angezerrt. |

### Projekte / Demo-Patterns (10)

| Name | Kategorien | Beschreibung |
|---|---|---|
| Lo-Fi Beat | Drums, Tasten, Bass | Klassischer Lo-Fi-Hip-Hop mit 78 BPM und trägem, schwerem Swing: dumpfe Kick, weiche Snare und leise geshuffelte Hats auf dem Lo-Fi-Kit (Euklid), grundtonlose Nonen-Voicings auf Dusty Keys mit starkem Band-Leiern (Orbit-Spuren 1–3), ein warmer, runder Bass auf den Grundtönen (Step-Sequenzer), Vinyl-Knistern (Orbit-Spur 4). Fm9 – Bb13 – Ebmaj9 – C7#9; dunkler Master (Tiefpass, Bandsättigung). Play drücken. |
| Techno Groove | Drums, Bass | Treibender Four-on-the-floor auf dem Hybrid-Kit aus Orbit (Claps auf 2 und 4, Rims, akzentuierte 16tel-Hats, offene Offbeat-Hat), ein rollender Bass aus dem Step-Sequenzer und dunkle Septakkord-Stabs aus Euklid mit langsam wanderndem Filter und synchronisiertem punktiertem Achtel-Delay. |
| Ambient Drift | Flächen, Texturen | Langsame Quinten-Fläche, eine generative Kalimba aus vier Orbit-Spuren mit 5, 7, 11 und 16 Steps mit tempo-synchronem punktiertem Achtel-Delay, spärliche Klangglas-Töne aus Euklid. Ohne Drums. |
| Synth-Pop | Drums, Bass, Leads | Heller Four-on-the-floor auf dem Trocken-Kit aus Euklid (Snare und Clap auf 2 und 4, Achtel-Hats), ein hüpfender Oktav-Bass, üppiges Poly-Brass und ein funkelndes Arpeggio aus drei Orbit-Spuren über Am–F–C–G und eine Lead-Melodie aus dem Step-Sequenzer. |
| Euclid Polyrhythm | Drums, Arp | Vier Euklid-Ringe mit 8, 12, 16 und 9 Steps auf dem Room-Kit, eine Marimba aus Orbit-Spuren mit 5, 6, 7 und 8 Steps und ein Orgelpunkt-Bass. |
| Arp Sequence | Arp, Drums | Ein 64-Step-Arpeggio über Am, Fmaj7, G und Em auf einem A-Orgelpunkt, Studio-Kit-Drums aus Orbit. Der Arpeggiator ist auf Part 3 gelatcht: Akkorde spielen. |
| Deep House | Drums, Bass, Tasten | Geshuffelte Hats und eine tiefe Kick auf dem Tight-Kit (Orbit), ein synkopierter Orgel-Bass (Step-Sequenzer) und Euklid-Akkord-Stabs. |
| Sub & Glide | Flächen, Bass, Drums, Boom | Septakkorde, die ineinander gleiten (Glide Strings, Poly-Glide), ein langer gleitender Sub auf Orbit-Spur 1 und ein Halftime-Beat auf dem Boom-Kit aus den Orbit-Spuren 2–4, funkelnde Glocken aus Euklid. Cm7 – Abmaj7 – Ebmaj7 – Bb7. |
| Lofi Kit | Drums | Das Lo-Fi-Kit für Lo-Fi-Hip-Hop eingestellt: dumpfe, runde Kick, weiche Snare mit Ghost Notes und Rim, matte geshuffelte Hats mit Anschlags-Variation (Orbit, träger Swing), Vinyl-Knistern auf Part 4 (Orbit-Spur 4) und ein dunkler Master. Eigene Keys auf Part 1 spielen. |
| Boom Kit | Drums, Bass, Boom | Das synthetisierte Boom-Kit in einem Halftime-Beat mit rollenden Hats (Orbit), Boom Slide auf dem Step-Sequenzer – überlappende Noten gleiten (Legato-Portamento) – und eine dunkle Fläche, die in ihren Akkord steigt (Rise Glass, Euklid). c-Moll, 70 BPM. |

**Engines:** FM 28, Wavetable 24, Analog 51, Zupfen 9, Grain 11, Vocal 13, Cluster 13, Sampler 13
<!-- presets:end -->
