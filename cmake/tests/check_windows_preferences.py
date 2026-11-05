"""Verify the native Preferences dialog, transactional store and real UI effects."""
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
import tempfile
import threading


class PreferenceHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests.append({"path": self.path, "language": self.headers.get("Accept-Language", "")})
        if self.path == "/dot.gif":
            content = base64.b64decode("R0lGODlhAQABAIAAAAD/AP///yH5BAEAAAEALAAAAAABAAEAAAICRAEAOw==")
            kind = "image/gif"
        else:
            content = (b'<html><head><title>Preferences fixture</title></head>'
                       b'<body bgcolor="#ffffff" text="#000000"><h1>Preferences fixture</h1>'
                       b'<p>Document font and color preferences.</p><a href="/unvisited">Preference link</a>'
                       b'<pre>Fixed font sample</pre><img src="/dot.gif" width="16" height="16"></body></html>')
            kind = "text/html"
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, *args):
        pass


@contextmanager
def server(output):
    fixture = http.server.ThreadingHTTPServer(("127.0.0.1", 0), PreferenceHandler)
    fixture.daemon_threads = True
    fixture.requests = []
    worker = threading.Thread(target=fixture.serve_forever, kwargs={"poll_interval": .05}, daemon=True)
    worker.start()
    try:
        yield fixture
    finally:
        fixture.shutdown()
        fixture.server_close()
        worker.join(2)
        (output / "requests.json").write_text(json.dumps(fixture.requests, indent=2) + "\n")


def fields(path):
    return dict(line.split("=", 1) for line in path.read_text(errors="replace").splitlines() if "=" in line)


def pixels(path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise RuntimeError("Preferences capture is not a real BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, planes, bits, compression = struct.unpack_from("<iiHHI", data, 18)
    if planes != 1 or bits not in (24, 32) or compression != 0 or width < 450 or abs(height) < 380:
        raise RuntimeError("Invalid Preferences capture dimensions/format")
    stride = ((width * bits + 31) // 32) * 4
    if offset + stride * abs(height) > len(data):
        raise RuntimeError("Truncated Preferences capture")
    return data, offset, width, abs(height), stride, bits // 8


def run(executable, output):
    output.mkdir(parents=True, exist_ok=True)
    profile = Path(tempfile.mkdtemp(prefix="preferences profile with spaces 100% ", dir=output))
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    snapshot = output / "preferences.bmp"
    with server(output) as fixture:
        for reopen in (False, True):
            report = output / ("preferences-reopen.txt" if reopen else "preferences.txt")
            report.unlink(missing_ok=True)
            url = "http://127.0.0.1:{}/{}".format(fixture.server_port, "reopen" if reopen else "page")
            mode = "--preferences-reopen-smoke" if reopen else "--preferences-ui-smoke"
            result = subprocess.run([str(executable), mode, str(report), str(snapshot), url],
                                    cwd=output, env=environment, capture_output=True, timeout=60)
            (output / (report.stem + "-stderr.txt")).write_bytes(result.stderr)
            (output / (report.stem + "-stdout.txt")).write_bytes(result.stdout)
            actual = fields(report) if report.exists() else {}
            if result.returncode:
                raise RuntimeError("Preferences {} exited {}: {}".format(mode, result.returncode, actual))
            required = ["root_passed", "store", "persisted", "text_toolbar", "pictures_toolbar",
                        "both_toolbar", "font_effect", "page_loaded", "image_effect", "startup_effect", "restored"]
            if not reopen:
                if actual.get("width") != "502" or actual.get("height") != "442":
                    raise RuntimeError("Preferences sizing differs from original 3.02: " + repr(actual))
                store = fields(Path(str(report) + ".store"))
                for name in ("tls_defaults", "tls_invalid_boolean", "tls_persisted", "tls_failed_save", "tls_path_validation", "tls_invalid_override_retained", "tls_fixture_reset"):
                    if store.get(name) != "1":
                        raise RuntimeError("Preferences omitted TLS transaction check: " + name)
                if actual.get("tls_controls") != "1":
                    raise RuntimeError("Preferences omitted native TLS controls")
                if store.get("language_header_boundary") != "1":
                    raise RuntimeError("Preferences omitted the legacy HTTP language boundary check")
                required += ["menu", "dialog", "tabs", "appearance", "layout", "keyboard", "cancel",
                             "validation", "page_retention", "save_failure", "save", "reopen", "capture"]
            for name in required:
                if actual.get(name) != "1":
                    raise RuntimeError("Preferences check failed for {}: {}".format(name, actual))
        pages = [r for r in fixture.requests if r["path"] in ("/page", "/reopen")]
        if len(pages) != 2 or any(r["language"].replace(" ", "") != "fr,en" for r in pages):
            raise RuntimeError("Preferences omitted persisted Accept-Language on actual HTTP requests: " + repr(pages))
        if not any(r["path"] == "/dot.gif" for r in fixture.requests):
            raise RuntimeError("Preferences image settings bypassed actual image fetching")
    data, offset, width, height, stride, size = pixels(snapshot)
    face = ctypes.windll.user32.GetSysColor(15)  # COLOR_BTNFACE
    face_bgr = bytes(((face >> 16) & 255, (face >> 8) & 255, face & 255))
    for x, y in [(10, 430), (480, 430)]:
        pixel = data[offset + y * stride + x * size:offset + y * stride + x * size + 3]
        if pixel != face_bgr:
            raise RuntimeError('Preferences margin does not match the native Windows face color')
    gray = dark = 0
    for y in range(height):
        for x in range(width):
            pixel = data[offset + y * stride + x * size:offset + y * stride + x * size + 3]
            gray += pixel == face_bgr
            dark += max(pixel) < 100
    if gray < width * height // 3 or dark < 500:
        raise RuntimeError("Preferences capture lacks native controls/text/system-color surfaces")
    data, offset, width, height, stride, size = pixels(Path(str(snapshot) + ".page.bmp"))
    beige = sum(data[i:i + 3] == bytes((200, 230, 242))
                for y in range(height) for i in range(offset + y * stride, offset + y * stride + width * size, size))
    if beige < width * height // 4:
        raise RuntimeError("Actual page did not honor custom document colors")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        run(args.executable.resolve(), args.output.resolve())
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired, struct.error) as error:
        print("Windows Preferences failed: " + str(error), file=sys.stderr)
        return 1
    print("Windows Preferences passed: native controls, atomic persistence, rendering, startup and HTTP language")
    return 0


if __name__ == "__main__":
    sys.exit(main())
