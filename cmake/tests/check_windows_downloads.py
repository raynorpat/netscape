"""Verify native downloads against bounded, deterministic loopback fixtures.

The browser implements --downloads-smoke REPORT BASEURL OUTPUTDIR. The test
independently checks saved bytes, failed-transfer preservation, cancellation,
filename sanitizing, concurrent transfers, and temporary-file cleanup.
"""

import argparse
from contextlib import contextmanager
import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import time
from urllib.parse import urlsplit


BINARY = b"Native binary download\x00line one\r\nline two\n\x1a\xff\x80\x00end\r\n"
SOURCE = (b"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\r\n"
          b"<HTML><HEAD><TITLE>Windows save source smoke</TITLE></HEAD>\r\n"
          b"<BODY><H1>Native source save</H1><P>Exact &amp; source bytes.</P></BODY></HTML>\r\n")
EXISTING = b"Keep existing download.\x00\xff\r\n"
CONCURRENT_A = b"Concurrent A\x00\r\n\xff" * 257
CONCURRENT_B = b"Concurrent B\x1a\r\n\x80" * 193
SLOW_CHUNK = (b"Slow download chunk\x00\xff\r\n" * 192)[:4096]
SLOW_CHUNKS = 512
EXPECTED_FILES = {
    "binary saved.bin": BINARY,
    "saved source.html": SOURCE,
    "redirected.bin": BINARY,
    "redirected close.bin": BINARY,
    "empty.bin": b"",
    "existing.bin": EXISTING,
    "locked destination.bin": EXISTING,
    "blocked parent": EXISTING,
    "concurrent A.bin": CONCURRENT_A,
    "concurrent B.bin": CONCURRENT_B,
    "badname.bin": BINARY,
}
ABSENT_FILES = ("cancelled.bin", "error.bin", "blocked parent/file.bin")


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def send_payload_headers(self, length, content_type="application/octet-stream", filename=None,
                             encoding=None):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(length))
        self.send_header("Connection", "close")
        if filename:
            self.send_header("Content-Disposition", 'attachment; filename="' + filename + '"')
        if encoding:
            self.send_header("Content-Encoding", encoding)
        self.end_headers()

    def do_GET(self):
        path = urlsplit(self.path).path
        self.server.requests.append(path)
        try:
            if path in ("/redirect", "/redirect-close"):
                self.send_response(302)
                self.send_header("Location", "/close.bin" if path == "/redirect-close" else "/binary.bin")
                self.send_header("Content-Length", "0")
                if path == "/redirect-close":
                    self.send_header("Content-Encoding", "identity")
                self.send_header("Connection", "close")
                self.end_headers()
                return
            if path == "/close.bin":
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(BINARY)
                self.close_connection = True
                return
            if path == "/truncated.bin":
                self.send_payload_headers(len(BINARY) + 4096, encoding="identity")
                self.wfile.write(b"Truncated\x00body")
                self.wfile.flush()
                self.close_connection = True
                return
            if path == "/slow.bin":
                self.send_payload_headers(len(SLOW_CHUNK) * SLOW_CHUNKS)
                for unused in range(SLOW_CHUNKS):
                    self.wfile.write(SLOW_CHUNK)
                    self.wfile.flush()
                    self.server.slow_bytes += len(SLOW_CHUNK)
                    time.sleep(0.01)
                return
            resources = {
                "/binary.bin": (BINARY, "application/octet-stream"),
                "/source.html": (SOURCE, "text/html; charset=us-ascii"),
                "/empty.bin": (b"", "application/octet-stream"),
                "/concurrent-a.bin": (CONCURRENT_A, "application/octet-stream"),
                "/concurrent-b.bin": (CONCURRENT_B, "application/octet-stream"),
                "/filename.bin": (BINARY, "application/octet-stream"),
            }
            resource = resources.get(path)
            if resource is None:
                self.send_error(404)
                return
            body, content_type = resource
            self.send_payload_headers(len(body), content_type, "../../badname.bin" if path == "/filename.bin" else None)
            self.wfile.write(body)
        except OSError as error:
            # Cancelling the slow transfer deliberately closes its socket.
            self.server.disconnects.append({"path": path, "error": str(error)})

    def log_message(self, format, *values):
        pass


@contextmanager
def fixture_server(output):
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server.requests = []
    server.disconnects = []
    server.slow_bytes = 0
    worker = threading.Thread(target=server.serve_forever,
                              kwargs={"poll_interval": 0.05}, daemon=True)
    worker.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=2)
        (output / "downloads-requests.json").write_text(
            json.dumps({"requests": server.requests, "slow_bytes": server.slow_bytes,
                        "disconnects": server.disconnects}, indent=2) + "\n", encoding="utf-8")


def prepare_destination(output):
    destination = output / "downloads with spaces 100%"
    destination.mkdir(parents=True, exist_ok=True)
    for name in tuple(EXPECTED_FILES) + ABSENT_FILES:
        (destination / name).unlink(missing_ok=True)
    for name in ("existing.bin", "locked destination.bin", "blocked parent"):
        (destination / name).write_bytes(EXISTING)
    return destination


def run_smoke(executable, output, destination, server, baseline):
    report = output / "downloads-smoke.txt"
    report.unlink(missing_ok=True)
    profile = output / "downloads profile with spaces 100%"
    profile.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    base_url = "http://127.0.0.1:{}".format(server.server_port)
    command = ([str(executable), "--smoke-test", str(report), base_url + "/binary.bin"] if baseline
               else [str(executable), "--downloads-smoke", str(report), base_url, str(destination)])
    try:
        process = subprocess.run(command, env=environment, cwd=str(output),
                                 capture_output=True, text=True, encoding="utf-8",
                                 errors="replace", timeout=35, check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / ("downloads-" + name + ".txt")).write_text(value or "", encoding="utf-8")
        raise RuntimeError("Downloads exceeded the 35-second timeout") from error
    for name, value in (("stdout", process.stdout), ("stderr", process.stderr)):
        (output / ("downloads-" + name + ".txt")).write_text(value, encoding="utf-8")
    if baseline:
        if "/binary.bin" not in server.requests:
            raise RuntimeError("Baseline did not request the binary fixture")
        if not (destination / "binary saved.bin").exists():
            raise RuntimeError("Baseline fetched the binary but never created the requested output file")
    if process.returncode:
        raise RuntimeError("Downloads UI exited with status {}".format(process.returncode))
    text = report.read_text(encoding="utf-8", errors="replace")
    fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    for name in ("binary", "source", "redirect", "redirect_close", "empty", "truncated", "cancel", "concurrent",
                 "filename", "http_error", "temp_cleanup", "rename_failed", "write_failed", "source_closed"):
        if fields.get(name) != "1":
            raise RuntimeError("Downloads report has unexpected {}:\n{}".format(name, text))
    for name, expected in EXPECTED_FILES.items():
        path = destination / name
        if not path.is_file() or path.read_bytes() != expected:
            raise RuntimeError("Downloaded bytes differ or file is missing: " + str(path))
    for name in ABSENT_FILES:
        if (destination / name).exists():
            raise RuntimeError("Failed or cancelled download created its destination: " + name)
    extra = [str(path.relative_to(destination)) for path in destination.rglob("*")
             if str(path.relative_to(destination)) not in EXPECTED_FILES]
    if extra:
        raise RuntimeError("Unexpected download or temporary files remain: " + ", ".join(extra))
    required = ("/binary.bin", "/source.html", "/redirect", "/redirect-close", "/close.bin", "/empty.bin", "/truncated.bin",
                "/slow.bin", "/concurrent-a.bin", "/concurrent-b.bin", "/filename.bin", "/missing.bin")
    missing = [path for path in required if path not in server.requests]
    if missing:
        raise RuntimeError("Native downloads omitted requests: " + ", ".join(missing))
    if not 0 < server.slow_bytes < len(SLOW_CHUNK) * SLOW_CHUNKS:
        raise RuntimeError("Slow transfer was not cancelled after starting")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", "--exe", type=Path, required=True)
    parser.add_argument("--output", "--output-dir", type=Path, required=True)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        destination = prepare_destination(output)
        with fixture_server(output) as server:
            run_smoke(args.executable.resolve(), output, destination, server, args.baseline)
    except (OSError, RuntimeError, ValueError) as error:
        print("Windows downloads smoke failed: " + str(error), file=sys.stderr)
        return 1
    print("Windows downloads smoke passed: exact bytes, atomic failure, cancellation, concurrency, safe filenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
