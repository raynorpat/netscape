"""Verify classic native browser chrome, navigation, and a real GDI capture.

The helper receives --chrome-smoke REPORT URL SNAPSHOT.bmp. All fixture pages
and bitmap inspection use the Python standard library; no public site is used.
"""

import argparse
import ctypes
import base64
from contextlib import contextmanager
import http.server
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import threading
from urllib.parse import urlsplit


TOOLBAR_LABELS = "Back|Forward|Home|Reload|Images|Open|Print|Find|Stop"
GOLD_TOOLBAR_LABELS = "Back|Forward|Home|Edit|Reload|Images|Open|Print|Find|Stop"
MENU_LABELS = "File|Edit|View|Go|Bookmarks|Options|Directory|Window|Help"
PAGES = {
    "/": (
        "<HTML><HEAD><TITLE>Classic chrome A</TITLE></HEAD><BODY>"
        "<H1>Classic chrome A</H1><P>Classic toolbar needle</P>"
        "<P><A HREF='/chrome-next'>Next classic page</A></P>"
        "<P>Classic toolbar needle appears again.</P></BODY></HTML>"
    ).encode("ascii"),
    "/auto-first": b'<html><body>Automatic first<img src="/automatic-a.gif"></body></html>',
    "/auto-second": b'<html><body>Automatic second<img src="/automatic-b.gif"></body></html>',
    "/chrome-next": (
        "<HTML><HEAD><TITLE>Classic chrome B</TITLE></HEAD><BODY>"
        "<H1>Classic chrome B</H1><P>Classic toolbar needle</P>"
        "<P><A HREF='/'>Previous classic page</A></P>"
        "<P>Classic toolbar needle appears again.</P></BODY></HTML>"
    ).encode("ascii"),
}


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def do_GET(self):
        path = urlsplit(self.path).path
        self.server.requests.append(path)
        body = PAGES.get(path)
        if path in ("/automatic-a.gif", "/automatic-b.gif"):
            body = base64.b64decode("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7")
        if body is None:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", "image/gif" if path.endswith(".gif") else "text/html; charset=us-ascii")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *values):
        pass


@contextmanager
def fixture_server(output):
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server.requests = []
    worker = threading.Thread(target=server.serve_forever,
                              kwargs={"poll_interval": 0.05}, daemon=True)
    worker.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=2)
        (output / "chrome-requests.json").write_text(
            json.dumps(server.requests, indent=2) + "\n", encoding="utf-8")


def check_bitmap(path, dpi=96):
    """Check actual captured pixels rather than accepting a helper success flag."""
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise RuntimeError("Chrome capture is not a Windows BMP")
    size, offset = struct.unpack_from("<I4xI", data, 2)
    header_size, width, height, planes, bits, compression = struct.unpack_from(
        "<IiiHHI", data, 14)
    if (size != len(data) or header_size < 40 or offset < 14 + header_size
            or width < 640 or width > 4096 or abs(height) < 400 or abs(height) > 4096
            or planes != 1 or bits not in (24, 32) or compression != 0):
        raise RuntimeError("Chrome capture has invalid size, dimensions, or pixel format")
    stride = ((width * bits + 31) // 32) * 4
    if len(data) < offset + stride * abs(height):
        raise RuntimeError("Chrome capture is missing bitmap scanlines")
    pixel_bytes = bits // 8
    face = ctypes.windll.user32.GetSysColor(15)  # COLOR_BTNFACE
    face_bgr = bytes(((face >> 16) & 255, (face >> 8) & 255, face & 255))
    x, y = width - 100, 10
    row = y if height < 0 else height - 1 - y
    pixel = offset + row * stride + x * pixel_bytes
    if data[pixel:pixel + 3] != face_bgr:
        raise RuntimeError('Toolbar background does not use the native Windows face color')
    if dpi < 48 or dpi > 384:
        raise RuntimeError("Chrome capture has an unsupported UI DPI")
    # Sample only the interior icon area of the disabled Back button. The
    # button border and the caption below it cannot make an absent icon pass.
    # Native captures normally use Windows' virtualized 96-DPI coordinates;
    # a DPI-aware helper can supply ui_dpi to scale this reference rectangle.
    left, top, right, bottom = [(value * dpi + 48) // 96 for value in (12, 6, 33, 24)]
    if right > width or bottom > abs(height):
        raise RuntimeError("Chrome capture does not include the Back button icon area")
    ink = 0
    for y in range(top, bottom):
        row = y if height < 0 else height - 1 - y
        for x in range(left, right):
            pixel = offset + row * stride + x * pixel_bytes
            if any(abs(channel - background) > 12 for channel, background in zip(data[pixel:pixel + 3], face_bgr)):
                ink += 1
    if ink < 4:
        raise RuntimeError("Chrome capture is missing the disabled Back button artwork")
    colors = set()
    for row in range(abs(height)):
        start = offset + row * stride
        for column in range(width):
            pixel = start + column * pixel_bytes
            colors.add(data[pixel:pixel + 3])
            if len(colors) >= 8:
                return width, abs(height)
    raise RuntimeError("Chrome capture lacks rendered text and artwork (fewer than eight colors)")


def check_report(report, edition="standard"):
    text = report.read_text(encoding="utf-8", errors="replace")
    fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    if fields.get("edition") != edition:
        raise RuntimeError("Chrome report edition does not match {} fixture:\n{}".format(edition, text))
    for name in ("passed", "completed", "toolbar_state", "location_enter", "history_state",
                 "menu_auto_images", "menu_document_source", "menu_browser_find_again", "menu_window_list", "menu_window_activate", "menu_go_history",
                 "find_text", "view_toggles", "narrow_layout", "location_dropdown", "about_page"):
        if fields.get(name) != "1":
            raise RuntimeError("Chrome report has unexpected {}:\n{}".format(name, text))
    toolbar = GOLD_TOOLBAR_LABELS if edition == "gold" else TOOLBAR_LABELS
    for name, expected in (("toolbar_labels", toolbar), ("menu_labels", MENU_LABELS),
                           ("toolbar_count", "10" if edition == "gold" else "9"), ("directory_count", "6")):
        if fields.get(name) != expected:
            raise RuntimeError("Chrome report has unexpected {}:\n{}".format(name, text))
    if int(fields.get("status_panes", "0")) < 3:
        raise RuntimeError("Chrome status bar lacks the classic separate panes")
    client = [int(value) for value in fields.get("narrow_client", "").split(",")]
    view = [int(value) for value in fields.get("narrow_view", "").split(",")]
    if len(client) != 2 or len(view) != 4:
        raise RuntimeError("Chrome report lacks native narrow-window geometry")
    width, height = client
    left, top, right, bottom = view
    if (width <= 0 or width > 360 or height <= 0 or height > 520
            or left < 0 or top < 0 or right > width or bottom > height
            or right - left < 120 or bottom - top < 80):
        raise RuntimeError("Narrow chrome clips the document or leaves no usable view: "
                           + repr((client, view)))
    return fields


def run_smoke(executable, output, server, edition="standard"):
    report = output / "chrome-smoke.txt"
    snapshot = output / "chrome-smoke.bmp"
    for path in (report, snapshot):
        path.unlink(missing_ok=True)
    profile = output / "chrome profile with spaces 100%"
    profile.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    url = "http://127.0.0.1:{}/".format(server.server_port)
    command = [str(executable), "--chrome-smoke", str(report), url, str(snapshot)]
    try:
        process = subprocess.run(command, env=environment, cwd=str(output),
                                 capture_output=True, text=True, encoding="utf-8",
                                 errors="replace", timeout=35, check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / ("chrome-" + name + ".txt")).write_text(value or "", encoding="utf-8")
        raise RuntimeError("Chrome UI exceeded the 35-second timeout") from error
    for name, value in (("stdout", process.stdout), ("stderr", process.stderr)):
        (output / ("chrome-" + name + ".txt")).write_text(value, encoding="utf-8")
    if process.returncode:
        raise RuntimeError("Chrome UI exited with status {}".format(process.returncode))
    missing = [path for path in PAGES if path not in server.requests]
    if missing:
        raise RuntimeError("Chrome navigation omitted actual HTTP requests: " + ", ".join(missing))
    subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / "tools" / "generate_windows_menus.py"), "--compare", str(report) + ".menus.tsv", "--edition", edition], check=True)
    for image in ("/automatic-a.gif", "/automatic-b.gif"):
        if server.requests.count(image) != 1:
            raise RuntimeError("Image menu did not defer/fetch exactly once: " + repr(server.requests))
    fields = check_report(report, edition)
    width, height = check_bitmap(snapshot, int(fields.get("ui_dpi", "96")))
    (output / "chrome-capture.json").write_text(
        json.dumps({"width": width, "height": height}, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", "--exe", type=Path, required=True)
    parser.add_argument("--output", "--output-dir", type=Path, required=True)
    parser.add_argument("--edition", choices=("standard", "gold"), default="standard")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        with fixture_server(output) as server:
            run_smoke(args.executable.resolve(), output, server, args.edition)
    except (OSError, RuntimeError, ValueError) as error:
        print("Windows chrome smoke failed: " + str(error), file=sys.stderr)
        return 1
    print("Windows chrome smoke passed: native commands, menus, location, resizing, and BMP capture")
    return 0


if __name__ == "__main__":
    sys.exit(main())
