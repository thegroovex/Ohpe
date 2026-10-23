# Ohpe manual - layout checks on the rendered WeasyPrint document. designed by groovex.
# Finds headings left alone at the bottom of a page, lists / tables that leave a single item behind
# on a page, and text or images that run past the right edge of the text area.
from weasyprint.formatting_structure import boxes as B


def _walk(box):
    yield box
    for c in getattr(box, "children", []) or []:
        yield from _walk(c)


def _heading_text(box):
    return "".join(b.text for b in _walk(box) if isinstance(b, B.TextBox)).strip()[:60]


def check(document, page_offset=0):
    issues = []
    frags = {}  # element -> list of (page, n items)
    for pno, page in enumerate(document.pages, start=1):
        pb = page._page_box
        content = [c for c in pb.children if not isinstance(c, B.MarginBox)]
        if not content:
            continue
        root = content[0]
        right = root.content_box_x() + root.width + 0.5
        bottom_content = 0
        heads = []
        for b in _walk(root):
            if isinstance(b, (B.TextBox, B.ReplacedBox)):
                bottom_content = max(bottom_content, b.position_y + b.margin_height())
                if b.position_x + b.margin_width() > right + 1.5:
                    issues.append(f"p{pno}: overflow by {b.position_x + b.margin_width() - right:.1f}px: "
                                  f"{getattr(b, 'text', '[image]')[:40]!r}")
            if isinstance(b, B.BlockBox) and b.element_tag in ("h1", "h2", "h3", "h4", "h5") \
                    and "partpage" not in (b.element.get("class") or ""):
                heads.append(b)
            if isinstance(b, B.BlockBox) and b.element_tag in ("ul", "ol") or \
                    (isinstance(b, B.TableBox) and b.element_tag == "table"):
                if isinstance(b, B.TableBox):
                    n = sum(1 for g in b.children for r in getattr(g, "children", [])
                            if isinstance(r, B.TableRowBox) and not g.is_header)
                else:
                    n = sum(1 for c in b.children if getattr(c, "element_tag", "") == "li")
                frags.setdefault(b.element, []).append((pno, n))
        for h in heads:
            hb = h.position_y + h.margin_height()
            if hb >= bottom_content - 0.5 and h.element_tag != "h1":
                issues.append(f"p{pno}: heading alone at the bottom: {_heading_text(h)!r}")
    for el, parts in frags.items():
        if len(parts) < 2:
            continue
        total = sum(n for _, n in parts)
        for pno, n in parts:
            if n == 1 and total >= 3:
                kind = el.tag
                issues.append(f"p{pno}: {kind} split leaves one item on this page ({total} items)")
    return issues
