# Zupfen-Engine (Pluck)

*Deutsch · English: [../../en/engines/pluck.md](../../en/engines/pluck.md)*

Zupfen bildet gezupfte und angeschlagene Klangkörper nach. Die **Saite** ist ein Wellenleiter:
eine kurze Verzögerungsleitung, in der ein geformter Rauschstoß kreist und bei jedem Durchlauf
etwas Energie und Helligkeit verliert. Die Stimmung gleicht die Laufzeit der Schleifenfilter exakt
aus, daher ist jeder Ton über die ganze Tastatur sauber gestimmt. Der **Schlägel** besteht aus
fünf gestimmten Resonatoren (wie ein angeschlagener Holz- oder Metallstab), angeregt von einem
weichen oder harten Schlägel. **Material** blendet zwischen beiden; ein kleiner resonanter
**Korpus** färbt beide. Die Saite klingt von selbst aus: Solange die Taste gehalten wird, verklingt
sie natürlich, die Amp-Hüllkurve formt nur den Gesamtpegel und das Loslassen.

| Regler | Parameter | Bereich | Wirkung |
|---|---|---|---|
| 1 | Material | 0–1 | 0 = Saite, 1 = Schlägel, dazwischen gemischt |
| 2 | Dämpfung | 0–1 | Abklingzeit (langes Klingen → kurzer Schlag) |
| 3 | Helligkeit | 0–1 | wie schnell die Obertöne verklingen; Mod-Hüllkurve und LFO *Klangfarbe* bewegen sie |
| 4 | Korpus | 0–1 | Anteil der Korpusresonanz |
| – | Anregung | 0–1 | weich → hart gezupft / angeschlagen (Anschlagstärke verstärkt sie) |
| – | Anschlagpunkt | 0,02–0,5 | wo die Saite gezupft wird (am Ende = dünn und hell, Mitte = hohl) |
| – | Korpusgröße | 0–1 | kleiner → großer Korpus |

Parameter-IDs: `p1.pluck.material`, `.damping`, `.bright`, `.body`, `.excite`, `.pickpos`,
`.bodysize`.

**Ausgangspunkte:** Nylonsaite – Helligkeit 0,35, Korpus 0,5, Anschlagpunkt 0,25; Harfe –
Dämpfung 0,2, Helligkeit 0,6, langes Amp-Release; marimbaartig – Material 1, Anregung 0,4,
Dämpfung 0,5.
