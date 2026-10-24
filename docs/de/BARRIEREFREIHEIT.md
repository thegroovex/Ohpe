# Barrierefreiheit und UI-Größe

Ohpe lässt sich ohne Maus spielen und bearbeiten, von einem Screenreader vorlesen und in sechs Größen zeigen. Nichts
davon ändert Klang oder Projekt: Die UI-Größe steht in den Einstellungen dieses Computers (`settings.json`), nie in
einem Projekt, Preset oder Host-Projekt, und es kam kein Host-Parameter hinzu.

## UI-Größe (75 – 200 %)

Sechs Größen: **75 / 100 / 125 / 150 / 175 / 200 %** der 1200 × 800 großen Platte.

- **Einstellungen › Display › Fenstergröße** oder
- **Rechtsklick aufs Gehäuse** (eine freie Stelle der Platte) oder auf die **Zahnrad-Taste** – das Menü *UI-Größe*.

Das Fenster (Standalone) bzw. der Plug-in-Editor nimmt die Größe sofort an und lässt sich weiter an der Ecke ziehen; es
behält das Seitenverhältnis der Platte (3 : 2) zwischen 67 % und 200 %. Alles ist als Vektor gezeichnet: Beschriftungen,
Piktogramme, Leuchtbalken und Display bleiben in jeder Größe scharf, und auf Bildschirmen mit hoher Pixeldichte (Retina /
HiDPI) wird das Gehäuse in der Pixeldichte des Bildschirms gerendert. Auf kleinen Bildschirmen eine passende Größe wählen
– das Fenster wird nie größer als eingestellt.

## Bedienung mit der Tastatur

Ein Klick verschiebt den Tastaturfokus nie: Nach der Maus spielt die Computertastatur weiter Noten. Der Fokus wandert
nur mit diesen Tasten:

| Taste | Aktion |
|---|---|
| Tab / Shift+Tab | nächstes / voriges Bedienelement, Bereich für Bereich: Kopfzeile (Version, Parts, Preset, BIBLIOTHEK … Einstellungen), Engines und LEBEN, Display (Display, Tonart-Gravitation, TAP, Tempo), die vier Encoder, die Modus-Tasten, Transport und Tempo, Oktav-/MIDI-/Grid-Tasten, Räder, Portamento, Glide und die Tastatur |
| F6 / Shift+F6 (oder Strg+Tab) | erstes Element des nächsten / vorigen Bereichs |
| ← → ↑ ↓ | auf einem Wert (Encoder, Rad, LEBEN, Portamento, Glide, Tonart-Gravitation, Tempo, Part): ein Schritt – eine Auswahl eine Position, ein stufenloser Wert 1 % |
| Shift + Pfeil | feiner (0,1 %) |
| Bild ↑ / Bild ↓ | zehn Schritte (Auswahl: drei Positionen; Tempo: 10 BPM) |
| Entf / Rücktaste | der Standardwert (wie ein Doppelklick) |
| Enter / Leertaste | die fokussierte Taste drücken; auf der Preset-Leiste: Bibliothek öffnen; auf dem Tempo: Tempo eintippen |
| Shift+F10 | das Menü der fokussierten Taste (MIDI-Learn, wie ein Rechtsklick) |
| 1 – 4 | auf der Part-Auswahl: Part 1 – 4 |
| ← → und 1 – 9 | auf dem Display: Seite zurück / vor und die Softkeys (von links gezählt) |
| Esc | Fokus zurück zur Tastatur (Noten auf der Computertastatur) |

Das fokussierte Element trägt einen **Fokusring**: eine dunkle Kontur mit dem gelben Edit-Licht innen – sichtbar auf dem
hellen Gehäuse, dem Nachtgehäuse und dem dunklen Display. Er erscheint nur, solange man mit Tab / F6 wandert; ein Klick
blendet ihn aus.

**Noten auf der Computertastatur** spielen, solange die Bildschirm-Tastatur den Fokus hat (standardmäßig, nach Esc oder
einem Klick auf die Tasten): A W S E D F T G Y H U J K O L P spielen C bis Dis eine Oktave höher. Keine der
Navigationstasten oben ist eine Notentaste – Tab, F6, Pfeile, Bild ↑/↓, Enter, Leertaste, Entf und Esc spielen nie Noten.
Hat ein anderes Element den Fokus, spielen die Buchstaben keine Noten, sondern gehen an die Kürzel (Standalone: R, L, I,
O, T, K). T, K, L und O sind zugleich Noten und Standalone-Kürzel: Mit fokussierter Tastatur spielen sie Noten; einmal Tab
drücken, um sie als Kürzel zu nutzen. Die Leertaste ist in der Standalone-App START / Stopp – außer eine Taste hat den
Fokus, dann drückt die Leertaste diese Taste.

## Screenreader

Jedes Bedienelement hat einen Namen und einen Wert in der Sprache der Oberfläche (Deutsch oder Englisch, wechselt mit der
Sprache): Tasten (mit Zustand, z. B. ist eine gewählte Engine oder ein gewählter Modus *aktiviert*), Encoder (der volle
Name des gebundenen Parameters und sein Wert wie auf der Platte), Räder, LEBEN, Portamento, Glide, Part-Auswahl,
Preset-Leiste, Tempo, TAP, Tonart-Gravitation, Tonart-Anzeige, Metronom-Leiste, Band- und Tastatur-Anzeige und das
Display (Titel der Kopfzeile, Seite, Status und die nummerierten Softkeys). Werte lassen sich vom Screenreader setzen (in
Prozent ihres Bereichs). Tooltips werden als Hilfetext gelesen (jedes
Bedienelement hat einen, deutsch und englisch); **Strg / Cmd + F** öffnet die Hilfesuche, die Themen, Parameter und
Bedienelemente per Eingabe findet (siehe *Tooltips und Hilfesuche* im Kapitel Bedienung). Ohpe nutzt die Barrierefreiheits-Schnittstelle des
Betriebssystems (VoiceOver unter macOS, Sprachausgabe / NVDA unter Windows; unter Linux soweit der Desktop es unterstützt).

## Kontrast

Beide Erscheinungen erfüllen WCAG AA, wo das Design es zulässt:

- Beschriftungen auf Gehäuse und Tasten, Display-Text und die gedimmten Display-Texte: mindestens 4,5 : 1 (hell und Nacht);
- jeder leuchtende Leuchtbalken hat eine feine Kante in seiner eigenen Farbe – in der hellen Erscheinung vertieft, in der
  Nacht angehoben – mit mindestens 3 : 1 zur Taste (Nicht-Text-Kontrast); der gelbe Balken auf dem warmweißen Gehäuse ist
  nicht mehr nur eine Ahnung;
- der Fokusring: mindestens 3 : 1 auf jeder Fläche (Gehäuse, Panels, Tasten, Display).

Die farbigen Display-Akzente (Violett, Rot) behalten ihre Farbe; sie stehen immer mit Text oder Symbol, nie als einziges
Zeichen.

Siehe auch: [Bedienung](BEDIENUNG.md) · [Einstellungen](EINSTELLUNGEN.md).
