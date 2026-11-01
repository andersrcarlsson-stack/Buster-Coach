#!/usr/bin/env python3
"""help_pages.py — makes the Help pages from app/help/help.md (release_design.md 5, step 9).

help.md holds the whole text; each "## Title {#page}" starts a page (the first one is index.html). Links between
pages are written "(#page)". This writes app/help/<page>.html (HTML that Qt's QTextBrowser shows: no colours of its
own, so light and dark mode both work) and app/help/help.qrc (the resource list: the pages and the pictures, built
into the app as qrc:/help/...). Run it after every edit of help.md, then build the app.

Usage: python3 tools/help_pages.py        (needs the python3 "markdown" package: Debian/Ubuntu python3-markdown)
"""

import re
from pathlib import Path

import markdown

HELP = Path(__file__).resolve().parent.parent / "app" / "help"
HEADING = re.compile(r"^## (.+?) \{#(\w+)\}\s*$", re.M)
PAGE = """<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>{title}</title>
<style>
  body {{ margin: 12px; }}
  h1 {{ font-size: x-large; }}
  td, th {{ padding: 3px 6px; }}
  .nav {{ font-size: small; }}
</style></head>
<body>
{nav}<h1>{title}</h1>
{body}
{footer}
</body></html>
"""


def main():
    text = (HELP / "help.md").read_text(encoding="utf-8")
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)       # the file's own notes
    heads = list(HEADING.finditer(text))
    pages = []
    for i, head in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        pages.append((head.group(2), head.group(1), text[head.end():end]))

    for i, (name, title, body) in enumerate(pages):
        body = re.sub(r"\(#(\w+)\)", r"(\1.html)", body)     # (#signs) -> (signs.html)
        body = re.sub(r"(?m)^(?![ \t]|[-|]|\s*$)(.+)\n(?=- )", r"\1\n\n", body)   # a list right after a sentence: a
                                                                                   # blank line first (Markdown needs
                                                                                   # it); not after an item's indented
                                                                                   # second line (a loose list)
        body = re.sub(r"(?m)^(\s*(?:<img[^>]*>\s*)+)\n(?!\s*\n)", r"\1\n\n", body)   # a line of pictures: always
                                                                                     # its own paragraph (the text below)
        html = markdown.markdown(body, extensions=["tables"])
        html = html.replace("<table>", '<table border="1" cellspacing="0">')   # QTextBrowser draws no lines otherwise
        nav = "" if name == "index" else '<p class="nav"><a href="index.html">← Contents</a></p>\n'
        # At the bottom, the pages in the file's order, as in a book: "← Previous: … · Next: … →".
        links = []
        if i > 0:
            links.append(f'<a href="{pages[i - 1][0]}.html">← Previous: {pages[i - 1][1]}</a>')
        if i + 1 < len(pages):
            links.append(f'<a href="{pages[i + 1][0]}.html">Next: {pages[i + 1][1]} →</a>')
        footer = '<hr>\n<p class="nav">' + " &nbsp;·&nbsp; ".join(links) + "</p>" if links else ""
        footer += "\n<p>&nbsp;</p>" * 3        # room under the links (QTextBrowser ignores the last block's margin)
        (HELP / f"{name}.html").write_text(PAGE.format(title=title, nav=nav, body=html, footer=footer),
                                           encoding="utf-8")

    files = [f"{name}.html" for name, _, _ in pages]
    files += [f"images/{p.name}" for p in sorted((HELP / "images").glob("*.png"))]
    qrc = ['<!-- help.qrc — written by tools/help_pages.py: the Help pages and their pictures (qrc:/help/...). -->',
           "<RCC>", '    <qresource prefix="/help">']
    qrc += [f"        <file>{f}</file>" for f in files]
    qrc += ["    </qresource>", "</RCC>", ""]
    (HELP / "help.qrc").write_text("\n".join(qrc), encoding="utf-8")
    print(f"{len(pages)} pages and {len(files) - len(pages)} pictures into {HELP}")


if __name__ == "__main__":
    main()
