# Ohpe manual - chapter list and fixed strings (EN / DE). designed by groovex.
#
# A chapter is a doc file (same file for both languages, or {"en": ..., "de": ...}), "quickstart"
# (tools/manual/quickstart.<lang>.html) or "readme-features" (the feature table of the README).
# New feature docs: add them here in both languages; the CI job fails if a file is missing.


def both(en, de):
    return {"en": en, "de": de}


CHAPTERS = [
    ("start", [
        "quickstart",
        "readme-features",
        both("docs/en/USER-INTERFACE.md", "docs/de/BEDIENUNG.md"),
    ]),
    ("engines", [
        both("docs/en/GRAIN-VOCAL-SAMPLER-DRUM.md", "docs/de/GRAIN-VOKAL-SAMPLER-DRUM.md"),
        both("docs/en/engines/wavetable.md", "docs/de/engines/wavetable.md"),
        both("docs/en/engines/pluck.md", "docs/de/engines/pluck.md"),
        both("docs/en/engines/cluster.md", "docs/de/engines/cluster.md"),
        both("docs/en/engines/layer.md", "docs/de/engines/layer.md"),
    ]),
    ("arrange", [
        both("docs/en/SEQUENCERS.md", "docs/de/SEQUENZER.md"),
        both("docs/en/TAPE-MIXER-FX.md", "docs/de/BAND-MISCHER-FX.md"),
        both("docs/en/TAPE-MEMORY.md", "docs/de/TAPE-MEMORY.md"),
        both("docs/en/BOUNCE.md", "docs/de/BOUNCE-INS-INSTRUMENT.md"),
        both("docs/en/MOTION.md", "docs/de/BEWEGUNG.md"),
        both("docs/en/PADS.md", "docs/de/PERFORMANCE-PADS.md"),
    ]),
    ("design", [
        both("docs/en/MODULATION.md", "docs/de/MODULATION.md"),
        both("docs/en/EXPRESSION.md", "docs/de/AUSDRUCK.md"),
        both("docs/en/SUB.md", "docs/de/SUB-EBENE.md"),
        both("docs/en/KEY-GRAVITY.md", "docs/de/TONART-GRAVITATION.md"),
        both("docs/en/HARMONY.md", "docs/de/HARMONIE.md"),
        both("docs/en/RESONATOR.md", "docs/de/RESONANZKOERPER.md"),
        both("docs/en/SPACE.md", "docs/de/RAUM.md"),
        both("docs/en/MORPH.md", "docs/de/MORPH-RAUM.md"),
        both("docs/en/VOICE-CONTROL.md", "docs/de/STIMMSTEUERUNG.md"),
        both("docs/en/BREEDING.md", "docs/de/ZUCHT.md"),
        both("docs/en/LIVING-DISPLAY.md", "docs/de/LEBENDES-DISPLAY.md"),
    ]),
    ("sounds", [
        both("docs/en/PRESETS.md", "docs/de/PRESETS.md"),
        both("docs/en/FACTORY-SOUNDS.md", "docs/de/FACTORY-SOUNDS.md"),
        both("docs/en/CONTENT-PACKS.md", "docs/de/INHALTSPAKETE.md"),
    ]),
    ("midi", [
        both("docs/en/MIDI.md", "docs/de/MIDI.md"),
        both("docs/en/CONTROLLER.md", "docs/de/CONTROLLER.md"),
    ]),
    ("setup", [
        both("docs/en/INSTALL.md", "docs/de/INSTALLATION.md"),
        both("docs/en/UPDATES.md", "docs/de/AKTUALISIERUNG.md"),
        both("docs/en/SETTINGS.md", "docs/de/EINSTELLUNGEN.md"),
        both("docs/en/PERFORMANCE.md", "docs/de/LEISTUNG.md"),
    ]),
    ("appendix", [
        "docs/PARAMETERS.md",
    ]),
]

# Sections for maintainers and developers stay in the repository docs and are left out of the manual.
DROP_SECTIONS = [
    r"maintainer", r"developer", r"entwickler", r"^tests?$", r"^building packs", r"pakete bauen",
    r"^extending", r"^erweitern",
]

STRINGS = {
    "en": {
        "doc_title": "Ohpe – Manual",
        "cover_sub": "Manual",
        "version": "Version",
        "contents": "Contents",
        "part": "Part",
        "quickstart": "Quick start",
        "features": "Feature overview",
        "readme_start": "Features",
        "readme_end": "Screenshots",
        "params": "Parameters",
        "params_note": "",
        "parts": {
            "start": "Getting started",
            "engines": "Sound engines",
            "arrange": "Sequencing, tape and mixing",
            "design": "Sound design",
            "sounds": "Presets and sounds",
            "midi": "MIDI and controllers",
            "setup": "Installation and settings",
            "appendix": "Appendix",
        },
    },
    "de": {
        "doc_title": "Ohpe – Handbuch",
        "cover_sub": "Handbuch",
        "version": "Version",
        "contents": "Inhalt",
        "part": "Teil",
        "quickstart": "Schnellstart",
        "features": "Funktionen im Überblick",
        "readme_start": "Funktionen",
        "readme_end": "Bilder",
        "params": "Parameter",
        "params_note": "Die Tabellen nennen zu jeder Parameter-ID den englischen und den deutschen Namen; "
                       "die Erläuterungen dieses Anhangs sind englisch.",
        "parts": {
            "start": "Erste Schritte",
            "engines": "Klangerzeuger",
            "arrange": "Sequencer, Band und Mixer",
            "design": "Klanggestaltung",
            "sounds": "Presets und Klänge",
            "midi": "MIDI und Controller",
            "setup": "Installation und Einstellungen",
            "appendix": "Anhang",
        },
    },
}
