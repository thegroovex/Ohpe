#!/usr/bin/env python3
# Ohpe manual generator - designed by groovex.
#
# Builds docs/manual/Ohpe-Manual-EN.pdf and docs/manual/Ohpe-Manual-DE.pdf from the Markdown docs
# (docs/en, docs/de, docs/PARAMETERS.md, README) with HTML + CSS and WeasyPrint.
#
#   pip install -r tools/manual/requirements.txt
#   python3 tools/manual/build_manual.py              # both languages -> docs/manual/
#   python3 tools/manual/build_manual.py --lang de    # one language
#   python3 tools/manual/build_manual.py --check      # only check links / images, no PDF
#   python3 tools/manual/build_manual.py --html-only  # write the HTML next to the PDFs (debugging)
#
# The run fails (exit 1) on a broken image link in any doc or README, on a chapter file that is
# missing, or on a link inside the manual that points at nothing.
import argparse
import datetime
import hashlib
import html
import io
import os
import re
import subprocess
import sys
import urllib.parse
from pathlib import Path

from markdown_it import MarkdownIt

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
REPO_URL = "https://github.com/thegroovex/Ohpe"
sys.path.insert(0, str(HERE))
from chapters import CHAPTERS, STRINGS, DROP_SECTIONS  # noqa: E402

# ------------------------------------------------------------------------------------------ helpers


def version():
    m = re.search(r"project\(Ohpe VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())
    return m.group(1) if m else "0"


def gh_slug(text, seen):
    """GitHub's heading anchor: lower case, punctuation removed, spaces -> '-', duplicates -n."""
    s = text.strip().lower()
    s = re.sub(r"[^\w\- ]", "", s, flags=re.UNICODE)
    s = s.replace(" ", "-")
    base, n = s, 0
    while s in seen:
        n += 1
        s = f"{base}-{n}"
    seen.add(s)
    return s


MARKER_TAIL = re.compile(r"\s*\((?:p\d+[a-z]?|\d+\.\d+(?:\.\d+)?|package [A-Z])\)")
MARKER_HEAD = re.compile(r"^(?:Phase \d+|\d+\.\d+(?: punch pass)?|p\d+)\s*[–:\-]\s*")


def clean_heading(text):
    t = MARKER_TAIL.sub("", text)
    t = MARKER_HEAD.sub("", t).strip()
    return t[:1].upper() + t[1:] if t else text


BODY_MARKER = re.compile(r"\s\((?:p\d+[a-z]?)\)")


def inline_text(tok):
    return "".join(c.content for c in (tok.children or []) if c.type in ("text", "code_inline"))


# ------------------------------------------------------------------------------------------ images


class Images:
    def __init__(self, cache, lang):
        self.cache = cache
        self.lang = lang
        self.cache.mkdir(parents=True, exist_ok=True)
        self.used = []

    def resolve(self, src, base):
        """Absolute path of an image, the German variant for the DE manual where one exists."""
        p = (base / src).resolve()
        if self.lang == "de" and not p.stem.endswith("-de"):
            de = p.with_name(p.stem + "-de" + p.suffix)
            if de.exists():
                p = de
        return p

    def prepare(self, p):
        """Screenshots go in as high-quality JPEG (much smaller PDF); SVG and small files as they are."""
        self.used.append(p)
        if p.suffix.lower() != ".png":
            return p.as_uri()
        from PIL import Image

        key = hashlib.sha1(p.read_bytes()).hexdigest()[:12]
        out = self.cache / f"{p.stem}-{key}.jpg"
        if not out.exists():
            im = Image.open(p)
            if im.mode in ("RGBA", "LA", "P"):
                im = im.convert("RGBA")
                bg = Image.new("RGB", im.size, (244, 240, 232))
                bg.paste(im, mask=im.split()[-1])
                im = bg
            else:
                im = im.convert("RGB")
            im.save(out, "JPEG", quality=88, optimize=True, progressive=False)
        return out.as_uri()


# ------------------------------------------------------------------------------------------ markdown


def strip_lang_line(md):
    """Drop the language switch line(s) right under the title ("*English · Deutsch: ...*" etc.)."""
    lines = md.split("\n")
    out, seen_title = [], False
    for i, ln in enumerate(lines):
        if i < 8 and seen_title and ln.strip():
            s = ln.strip()
            if (re.search(r"(English|Deutsch)\b", s) and ("](" in s) and len(s) < 260) or \
                    re.fullmatch(r"\*?Ohpe – designed by groovex\.?\*?", s):
                continue
            if s.startswith("Ohpe – designed by groovex.") and len(s) < 160:
                continue
        if ln.startswith("# "):
            seen_title = True
        out.append(ln)
    return "\n".join(out)


def drop_sections(md, patterns):
    """Remove '## ...' sections whose title matches one of the patterns (maintainer / developer notes)."""
    out, skip, level = [], False, 0
    in_code = False
    dropped = []
    for ln in md.split("\n"):
        if ln.lstrip().startswith("```"):
            in_code = not in_code
        m = None if in_code else re.match(r"^(#{2,6})\s+(.*)$", ln)
        if m:
            lv = len(m.group(1))
            if skip and lv <= level:
                skip = False
            if not skip and any(re.search(p, m.group(2), re.I) for p in patterns):
                skip, level = True, lv
                dropped.append(m.group(2))
        if not skip:
            out.append(ln)
    return "\n".join(out), dropped


def readme_section(path, start_title, end_title, chapter_title):
    md = path.read_text(encoding="utf-8")
    m = re.search(rf"^## {re.escape(start_title)}\s*$(.*?)^## {re.escape(end_title)}\s*$", md, re.S | re.M)
    if not m:
        raise SystemExit(f"README section '{start_title}' not found in {path}")
    return f"# {chapter_title}\n{m.group(1)}"


class Chapter:
    def __init__(self, cid, path, md, kind="md"):
        self.cid, self.path, self.md, self.kind = cid, path, md, kind
        self.tokens = None
        self.anchors = {}  # original GitHub slug -> manual id
        self.title = ""


def make_md():
    md = MarkdownIt("commonmark", {"html": True, "typographer": False}).enable("table").enable("strikethrough")
    return md


def parse(ch, mdit):
    ch.tokens = mdit.parse(ch.md)
    seen = set()
    toks = ch.tokens
    first_h1 = True
    for i, t in enumerate(toks):
        if t.type == "heading_open":
            inl = toks[i + 1]
            raw = inline_text(inl)
            slug = gh_slug(raw, seen)
            if t.tag == "h1" and first_h1:
                mid = ch.cid
                first_h1 = False
                ch.title = clean_heading(raw)
            else:
                mid = f"{ch.cid}--{slug}"
            ch.anchors[slug] = mid
            t.attrSet("id", mid)
            # clean the visible text
            for c in inl.children or []:
                if c.type == "text":
                    c.content = MARKER_TAIL.sub("", c.content)
            if inl.children and inl.children[0].type == "text":
                c0 = inl.children[0]
                c0.content = MARKER_HEAD.sub("", c0.content)
                c0.content = c0.content[:1].upper() + c0.content[1:]
        elif t.type == "inline":
            for c in t.children or []:
                if c.type == "text":
                    c.content = BODY_MARKER.sub("", c.content)


class LinkMap:
    def __init__(self, chapters):
        self.by_path = {}
        for ch in chapters:
            if ch.path is not None:
                self.by_path.setdefault(ch.path.resolve(), ch)
        self.errors = []

    def resolve(self, href, ch):
        if re.match(r"^[a-z]+:", href) or href.startswith("//"):
            return href
        href = urllib.parse.unquote(href)
        if href.startswith("#"):
            a = href[1:]
            if a in ch.anchors:
                return "#" + ch.anchors[a]
            self.errors.append(f"{ch.cid}: anchor {href} not found")
            return "#" + ch.cid
        file, _, frag = href.partition("#")
        base = ch.path.parent if ch.path is not None else ROOT
        target = (base / file).resolve()
        tch = self.by_path.get(target)
        if tch is not None:
            if frag:
                if frag in tch.anchors:
                    return "#" + tch.anchors[frag]
                self.errors.append(f"{ch.cid}: {href} - anchor not found in {tch.cid}")
            return "#" + tch.cid
        try:
            rel = target.relative_to(ROOT).as_posix()
        except ValueError:
            return href
        if not target.exists():
            self.errors.append(f"{ch.cid}: link target {href} does not exist")
        kind = "tree" if target.is_dir() else "blob"
        return f"{REPO_URL}/{kind}/main/{rel}" + (f"#{frag}" if frag else "")


def render(ch, mdit, links, images, broken):
    base = ch.path.parent if ch.path is not None else ROOT
    for t in ch.tokens:
        if t.type == "inline":
            for c in t.children or []:
                if c.type == "link_open":
                    c.attrSet("href", links.resolve(c.attrGet("href") or "", ch))
                elif c.type == "image":
                    src = c.attrGet("src") or ""
                    if re.match(r"^[a-z]+:", src):
                        continue
                    p = images.resolve(src, base)
                    if not p.exists():
                        broken.append(f"{ch.cid}: {src}")
                        continue
                    c.attrSet("src", images.prepare(p))
    out = mdit.renderer.render(ch.tokens, mdit.options, {})
    # a paragraph holding only an image becomes a figure with its alt text as caption
    out = re.sub(r'<p><img src="([^"]+)" alt="([^"]*)"\s*/?></p>',
                 lambda m: f'<figure><img src="{m.group(1)}" alt="{m.group(2)}"><figcaption>{m.group(2)}</figcaption></figure>',
                 out)
    # wide tables get a smaller font
    def table_class(m):
        head = m.group(0)
        ncols = head.count("<th")
        cls = "wide" if ncols >= 5 else ("mid" if ncols == 4 else "")
        return f'<table class="{cls}">' + head[len("<table>"):]
    out = re.sub(r"<table>\s*<thead>.*?</thead>", table_class, out, flags=re.S)
    return out


# ------------------------------------------------------------------------------------------ checks


def check_all_doc_images():
    """Every image link in docs/**.md and the READMEs must point at an existing file."""
    bad = []
    files = list((ROOT / "docs").rglob("*.md")) + [ROOT / "README.md", ROOT / "README.de.md"]
    pat = re.compile(r"!\[[^\]]*\]\(\s*<?([^)\s>]+)>?(?:\s+\"[^\"]*\")?\s*\)|<img[^>]+src=\"([^\"]+)\"")
    for f in files:
        txt = f.read_text(encoding="utf-8")
        txt = re.sub(r"```.*?```", "", txt, flags=re.S)
        for m in pat.finditer(txt):
            src = m.group(1) or m.group(2)
            if re.match(r"^[a-z]+:", src):
                continue
            if not (f.parent / src.split("#")[0]).resolve().exists():
                bad.append(f"{f.relative_to(ROOT)}: {src}")
    return bad


# ------------------------------------------------------------------------------------------ build


def git_date():
    env = os.environ.get("SOURCE_DATE_EPOCH")
    if env:
        return datetime.datetime.fromtimestamp(int(env), datetime.timezone.utc)
    try:
        ts = subprocess.check_output(["git", "-C", str(ROOT), "log", "-1", "--format=%ct"], text=True).strip()
        return datetime.datetime.fromtimestamp(int(ts), datetime.timezone.utc)
    except Exception:
        return datetime.datetime(2026, 1, 1, tzinfo=datetime.timezone.utc)


def load_chapters(lang):
    S = STRINGS[lang]
    parts = []
    for part_key, items in CHAPTERS:
        chs = []
        for item in items:
            if item == "quickstart":
                frag = (HERE / f"quickstart.{lang}.html").read_text(encoding="utf-8")
                c = Chapter("quickstart", None, frag, kind="html")
                c.title = S["quickstart"]
                chs.append(c)
                continue
            if item == "readme-features":
                rp = ROOT / ("README.md" if lang == "en" else "README.de.md")
                md = readme_section(rp, S["readme_start"], S["readme_end"], S["features"])
                chs.append(Chapter("features", rp, md))
                continue
            rel = item[lang] if isinstance(item, dict) else item
            p = ROOT / rel
            if not p.exists():
                raise SystemExit(f"chapter file missing: {rel}")
            md = p.read_text(encoding="utf-8")
            md = strip_lang_line(md)
            md, dropped = drop_sections(md, DROP_SECTIONS)
            if dropped:
                print(f"  [{lang}] {rel}: left out {dropped}")
            if rel == "docs/PARAMETERS.md":
                note = f"\n\n{S['params_note']}\n" if S["params_note"] else ""
                md = re.sub(r"^# .*$", lambda m: f"# {S['params']}{note}", md, count=1, flags=re.M)
            cid = re.sub(r"[^a-z0-9]+", "-", rel.lower().replace("docs/", "").replace(".md", "")).strip("-")
            chs.append(Chapter(cid, p, md))
        parts.append((part_key, chs))
    return parts


def build(lang, outdir, check_only=False, html_only=False):
    S = STRINGS[lang]
    ver = version()
    mdit = make_md()
    parts = load_chapters(lang)
    allch = [c for _, chs in parts for c in chs]
    for c in allch:
        if c.kind == "md":
            parse(c, mdit)
    # README: links in README chapter are relative to the repo root, docs links relative to their file
    links = LinkMap([c for c in allch if c.kind == "md"])
    images = Images(ROOT / "build" / "manual-cache", lang)
    broken = []
    body = []
    toc = []
    for pi, (pkey, chs) in enumerate(parts):
        ptitle = S["parts"][pkey]
        pid = f"part-{pi + 1}"
        toc.append(f'<li class="toc-part"><a href="#{pid}">{html.escape(ptitle)}</a></li>')
        body.append(f'<section class="partpage" id="{pid}"><div class="partno">{S["part"]} {pi + 1}</div>'
                    f'<div class="parttitle">{html.escape(ptitle)}</div></section>')
        for c in chs:
            if c.kind == "html":
                frag = c.md
                # images in the fragment: name="file" relative to docs/images
                def img(m):
                    p = images.resolve(m.group(1), ROOT / "docs" / "images")
                    if not p.exists():
                        broken.append(f"quickstart: {m.group(1)}")
                        return m.group(0)
                    return f'src="{images.prepare(p)}"'
                frag = re.sub(r'src="([^"]+)"', img, frag)
                frag = frag.replace("{VERSION}", ver)
                html_c = f'<section class="chapter" id="{c.cid}">{frag}</section>'
                # intra-manual links in the fragment: href="@FILE.md#anchor" -> resolved
                def lk(m):
                    target = ROOT / m.group(1)
                    ch2 = links.by_path.get(target.resolve())
                    if ch2 is None:
                        links.errors.append(f"quickstart: {m.group(1)} not in manual")
                        return 'href="#"'
                    frag2 = m.group(2)
                    if frag2 and frag2 in ch2.anchors:
                        return f'href="#{ch2.anchors[frag2]}"'
                    if frag2:
                        links.errors.append(f"quickstart: #{frag2} not in {ch2.cid}")
                    return f'href="#{ch2.cid}"'
                html_c = re.sub(r'href="@([^"#]+)(?:#([^"]*))?"', lk, html_c)
            else:
                html_c = f'<section class="chapter">{render(c, mdit, links, images, broken)}</section>'
            body.append(html_c)
            toc.append(f'<li class="toc-ch"><a href="#{c.cid}">{html.escape(c.title)}</a></li>')
    errors = list(links.errors)
    if broken:
        errors += [f"broken image: {b}" for b in broken]
    if errors:
        print(f"[{lang}] {len(errors)} problem(s):")
        for e in errors:
            print("   ", e)
        return False, 0
    if check_only:
        print(f"[{lang}] links and images ok ({len(allch)} chapters, {len(set(images.used))} images)")
        return True, 0

    css = (HERE / "manual.css").read_text(encoding="utf-8")
    fonts = (ROOT / "resources" / "fonts").as_uri()
    css = css.replace("{FONTS}", fonts)
    icon = (ROOT / "resources" / "icon" / "ohpe-icon.svg").as_uri()
    date = git_date()
    doc = f"""<!DOCTYPE html><html lang="{lang}"><head><meta charset="utf-8">
<title>{html.escape(S['doc_title'])}</title>
<meta name="author" content="groovex"><meta name="description" content="{html.escape(S['doc_title'])} {ver}">
<meta name="generator" content="Ohpe manual generator">
<meta name="dcterms.created" content="{date.strftime('%Y-%m-%dT%H:%M:%SZ')}">
<meta name="dcterms.modified" content="{date.strftime('%Y-%m-%dT%H:%M:%SZ')}">
<style>{css}</style></head><body>
<section class="cover"><img class="icon" src="{icon}"><div class="wm">OHPE<span></span></div>
<div class="sub">{html.escape(S['cover_sub'])}</div>
<div class="by">DESIGNED BY GROOVEX · {S['version'].upper()} {ver}</div></section>
<section class="toc" id="contents"><h1 class="toc-title">{html.escape(S['contents'])}</h1><ul>{''.join(toc)}</ul></section>
{''.join(body)}
</body></html>"""
    outdir.mkdir(parents=True, exist_ok=True)
    name = f"Ohpe-Manual-{lang.upper()}"
    if html_only:
        (outdir / f"{name}.html").write_text(doc, encoding="utf-8")
        print(f"[{lang}] wrote {outdir / (name + '.html')}")
        return True, 0
    from weasyprint import HTML
    rendered = HTML(string=doc, base_url=str(HERE)).render()
    pdf = outdir / f"{name}.pdf"
    rendered.write_pdf(pdf)
    n = len(rendered.pages)
    print(f"[{lang}] wrote {pdf} - {n} pages, {pdf.stat().st_size / 1e6:.1f} MB")
    return True, n


def main():
    ap = argparse.ArgumentParser(description="Build the Ohpe PDF manual (EN + DE).")
    ap.add_argument("--lang", choices=["en", "de", "all"], default="all")
    ap.add_argument("--out", default=str(ROOT / "docs" / "manual"))
    ap.add_argument("--check", action="store_true", help="only check links and images")
    ap.add_argument("--html-only", action="store_true")
    a = ap.parse_args()
    ok = True
    bad = check_all_doc_images()
    if bad:
        ok = False
        print(f"{len(bad)} broken image link(s) in the docs:")
        for b in bad:
            print("   ", b)
    langs = ["en", "de"] if a.lang == "all" else [a.lang]
    for lang in langs:
        r, _ = build(lang, Path(a.out), a.check, a.html_only)
        ok = ok and r
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
