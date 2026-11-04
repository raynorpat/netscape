"""Verify native bookmark editing, legacy input and persistence across processes."""

import argparse
from html.parser import HTMLParser
import os
from pathlib import Path
import subprocess
import sys


FOLDER = "Native & folders"
EDITED_TITLE = 'Native "quoted" & <bookmark> &lt;'
RETRY_TITLE = 'Native retry "quoted" & <bookmark> &lt;'
EDITED_URL = 'https://example.invalid/path?q="one"&tag=<bookmark>&literal=&lt;' + "&" * 600
EXPECTED = [
    ("Second bookmark", "https://example.invalid/second"),
    (EDITED_TITLE, EDITED_URL),
]


class BookmarkParser(HTMLParser):
    """Read Netscape H3/DL folders and A entries without external libraries."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = {"children": []}
        self.stack = [self.root]
        self.pending_folder = None
        self.capture = None
        self.text = []
        self.url = None

    def handle_starttag(self, tag, attrs):
        if tag == "dl":
            self.stack.append(self.pending_folder or self.stack[-1])
            self.pending_folder = None
        elif tag in ("h3", "a"):
            self.capture = tag
            self.text = []
            self.url = dict(attrs).get("href", "") if tag == "a" else None

    def handle_data(self, data):
        if self.capture:
            self.text.append(data)

    def handle_endtag(self, tag):
        if tag == "dl":
            if len(self.stack) > 1:
                self.stack.pop()
        elif tag == self.capture:
            title = "".join(self.text)
            if tag == "h3":
                node = {"title": title, "children": []}
                self.stack[-1]["children"].append(node)
                self.pending_folder = node
            else:
                self.stack[-1]["children"].append({"title": title, "url": self.url})
            self.capture = None
            self.text = []


def walk(node):
    yield node
    for child in node.get("children", []):
        yield from walk(child)


def check_bookmark_file(path, retry=False):
    text = path.read_text(encoding="utf-8", errors="strict")
    parsed = BookmarkParser()
    parsed.feed(text)
    parsed.close()
    nodes = list(walk(parsed.root))
    folders = [node for node in nodes if node.get("title") == FOLDER and "children" in node]
    if len(folders) != 1:
        raise RuntimeError("Saved bookmarks do not contain exactly one native fixture folder")
    entries = [(node.get("title"), node["url"]) for node in folders[0]["children"] if "url" in node]
    expected = [EXPECTED[0], (RETRY_TITLE if retry else EDITED_TITLE, EDITED_URL)]
    if entries != expected:
        raise RuntimeError("Saved folder entries/order differ: expected {!r}, got {!r}".format(expected, entries))
    bookmarks = [node for node in nodes if "url" in node]
    if len(bookmarks) != 2:
        raise RuntimeError("Expected two persisted bookmarks, got {}".format(len(bookmarks)))
    if "https://example.invalid/deleted" in text or "Delete me" in text:
        raise RuntimeError("Deleted bookmark remains in the saved HTML")
    # Quotes may remain literal in element text, but attribute quotes must be
    # escaped. A literal entity in either value needs a second ampersand escape.
    for escaped in ("Native &amp; folders", "&lt;bookmark&gt; &amp;lt;",
                    "q=&quot;one&quot;&amp;tag=&lt;bookmark&gt;&amp;literal=&amp;lt;"):
        if escaped not in text:
            raise RuntimeError("Saved HTML is missing escaped bookmark text: " + escaped)


def run_phase(executable, output, profile, phase):
    report = output / ("bookmarks-" + phase + "-smoke.txt")
    report.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    try:
        process = subprocess.run([str(executable), "--bookmarks-smoke", str(report), phase],
                                 env=environment, cwd=str(output), capture_output=True,
                                 text=True, encoding="utf-8", errors="replace", timeout=35,
                                 check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / ("bookmarks-" + phase + "-" + name + ".txt")).write_text(
                value or "", encoding="utf-8")
        raise RuntimeError("Bookmark {} phase exceeded the 35-second timeout".format(phase)) from error
    for name, value in (("stdout", process.stdout), ("stderr", process.stderr)):
        (output / ("bookmarks-" + phase + "-" + name + ".txt")).write_text(value, encoding="utf-8")
    if process.returncode:
        raise RuntimeError("Bookmark {} phase exited with status {}".format(phase, process.returncode))
    text = report.read_text(encoding="utf-8", errors="replace")
    fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    if phase == "seed":
        required = ("created", "edited", "add_page", "deleted", "manager", "open_requested", "persisted")
    elif phase == "save-failure":
        required = ("save_failed", "preserved", "retried")
    else:
        required = ("loaded",)
    for key in required:
        if fields.get("bookmarks_" + key) != "1":
            raise RuntimeError("Bookmark {} report has unexpected {}:\n{}".format(phase, key, text))
    if phase == "seed":
        for name in ("menu_populated", "menu_cached", "menu_rebuilt", "menu_open_requested"):
            if fields.get("bookmarks_" + name) != "1":
                raise RuntimeError("Bookmark native menu check failed: " + name + "\n" + text)
    if phase in ("verify", "verify-retry", "verify-legacy") and fields.get("bookmarks_count") != "2":
        raise RuntimeError("Fresh browser did not load two saved bookmarks:\n" + text)
    if phase == "verify-legacy" and fields.get("bookmarks_legacy_description") != "1":
        raise RuntimeError("Native reader did not preserve the boundary-size folder description:\n" + text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", "--exe", type=Path, required=True)
    parser.add_argument("--output", "--output-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    profile = output / "bookmarks profile with spaces 100%"
    profile.mkdir(parents=True, exist_ok=True)
    bookmark_file = profile / "bookmark.htm"
    try:
        bookmark_file.unlink(missing_ok=True)
        run_phase(executable, output, profile, "seed")
        check_bookmark_file(bookmark_file)
        run_phase(executable, output, profile, "verify")
        check_bookmark_file(bookmark_file)
        original = bookmark_file.read_bytes()
        for name in ("bookmarks-before-failed-save.htm", "bookmarks-after-failed-save.htm"):
            (output / name).unlink(missing_ok=True)
        run_phase(executable, output, profile, "save-failure")
        for name in ("bookmarks-before-failed-save.htm", "bookmarks-after-failed-save.htm"):
            if (output / name).read_bytes() != original:
                raise RuntimeError("Failed save changed the existing bookmark bytes: " + name)
        check_bookmark_file(bookmark_file, retry=True)
        run_phase(executable, output, profile, "verify-retry")
        check_bookmark_file(bookmark_file, retry=True)
        imported = bookmark_file.read_bytes()
        title = b">Second bookmark</A>"
        folder = b"Native &amp; folders</H3>"
        if imported.count(title) != 1 or imported.count(folder) != 1:
            raise RuntimeError("Cannot locate the legacy import fixture insertion points")
        imported = imported.replace(title, b">Second\n" + b"X" * 6000 + b" bookmark</A>")
        imported = imported.replace(folder, folder + b"\n<DD>" + b"A" * 2042 + b"\n")
        bookmark_file.write_bytes(imported)
        run_phase(executable, output, profile, "verify-legacy")
        if bookmark_file.read_bytes() != imported:
            raise RuntimeError("Reading the legacy continuation/description fixture rewrote its bytes")
    except (OSError, RuntimeError, ValueError) as error:
        print("Windows bookmarks smoke failed: " + str(error), file=sys.stderr)
        return 1
    print("Windows bookmarks smoke passed: native edits, escaped HTML, persistence, failed-save preservation and retry")
    return 0


if __name__ == "__main__":
    sys.exit(main())
