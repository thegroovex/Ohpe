# Manual generator

*Ohpe – designed by groovex*

Builds the PDF manual `docs/manual/Ohpe-Manual-EN.pdf` and `docs/manual/Ohpe-Manual-DE.pdf` from the
Markdown docs (`docs/en`, `docs/de`, `docs/PARAMETERS.md`, the README feature table) with HTML + CSS and
WeasyPrint. Fonts: `resources/fonts` (Space Grotesk), icon: `resources/icon/ohpe-icon.svg`, screenshots:
`docs/images` (the `-de` variant in the German manual wherever one exists).

```sh
pip install -r tools/manual/requirements.txt     # Linux: also libpango (apt install libpango-1.0-0 libpangoft2-1.0-0)
python3 tools/manual/build_manual.py              # both PDFs -> docs/manual/
python3 tools/manual/build_manual.py --check      # links and images only (fast)
python3 tools/manual/build_manual.py --strict     # fail on layout problems
```

| File | What it holds |
|---|---|
| `chapters.py` | chapter order (EN / DE file per chapter), parts, fixed strings, left-out maintainer sections |
| `quickstart.en.html`, `quickstart.de.html` | the quick start chapter |
| `manual.css` | page design: dark cover, matte warm-white pages, footer, tables, figures |
| `verify_layout.py` | layout checks on the rendered pages: headings alone at a page bottom, text or images past the right edge, lists / tables leaving one item on a page |

The run fails on a broken image link anywhere in the docs or READMEs, on a missing chapter file and on a
link inside the manual that points at nothing. Links to docs that are not in the manual become links to
the repository on GitHub. Milestone markers in headings (`(p26)`, `(0.8)`) are left out of the manual.

A new feature doc: add it to `chapters.py` in both languages and rebuild. CI (job *Manual (PDF)*) builds
both PDFs on every run and uploads them as the artifact **Ohpe-Manual**; commit the rebuilt PDFs with
doc changes so `docs/manual/` stays current.
