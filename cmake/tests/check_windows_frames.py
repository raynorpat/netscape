"""Exercise native child contexts, streamed targets, resizing, and nested frames.

Window-Target responses delay their bodies to exercise transfer of active
network watches, including transfer to the top context as its child closes.
All documents and images come from an ephemeral loopback HTTP server. The
--baseline option runs the old renderer smoke to expose missing child loads.
"""

import argparse
import base64
from contextlib import contextmanager
import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import time
from urllib.parse import parse_qs, urlsplit


GIF = base64.b64decode("R0lGODdhAgACAIEAAP8AAAD/AAAA////ACwAAAAAAgACAAAIBwABBBAwICAAOw==")
PAGES = {
    "/frames.html": (
        "<HTML><HEAD><TITLE>Windows frames smoke</TITLE></HEAD>"
        "<FRAMESET COLS='30%,70%'>"
        "<FRAME NAME='navigation' SRC='/navigation.html'>"
        "<FRAME NAME='content' SRC='/content.html'>"
        "</FRAMESET></HTML>"
    ),
    "/navigation.html": (
        "<HTML><HEAD><TITLE>Frame navigation</TITLE></HEAD><BODY>"
        "<H1>Navigation frame</H1>"
        "<P><A HREF='/header-target.html'>Header target</A></P>"
        "<P><A HREF='/targeted.html' TARGET='content'>Open target</A></P>"
        "</BODY></HTML>"
    ),
    "/content.html": (
        "<HTML><HEAD><TITLE>Frame content</TITLE></HEAD><BODY>"
        "<H1>Initial content frame</H1>"
        "<P><IMG SRC='/frame.gif' WIDTH='16' HEIGHT='16' ALT='Frame image'></P>"
        "<FORM METHOD='GET' ACTION='/frame-submit'>"
        "<INPUT TYPE='text' NAME='query' VALUE='frame default'>"
        "<INPUT TYPE='submit' NAME='go' VALUE='Submit'>"
        "</FORM></BODY></HTML>"
    ),
    "/targeted.html": (
        "<HTML><HEAD><TITLE>Targeted frame</TITLE></HEAD><BODY>"
        "<H1>Targeted content frame</H1>"
        "<P><A HREF='/nested.html' TARGET='content'>Open nested</A></P>"
        "</BODY></HTML>"
    ),
    "/nested.html": (
        "<HTML><HEAD><TITLE>Nested frames</TITLE></HEAD>"
        "<FRAMESET ROWS='50%,50%'>"
        "<FRAME NAME='nestedTop' SRC='/nested-top.html'>"
        "<FRAME NAME='nestedBottom' SRC='/nested-bottom.html'>"
        "</FRAMESET></HTML>"
    ),
    "/nested-top.html": (
        "<HTML><HEAD><TITLE>Nested top</TITLE></HEAD><BODY>"
        "<P>Nested top frame</P>"
        "<P><A HREF='/parent-target.html' TARGET='_parent'>Parent target</A></P>"
        "</BODY></HTML>"
    ),
    "/nested-bottom.html": (
        "<HTML><HEAD><TITLE>Nested bottom</TITLE></HEAD><BODY>"
        "<P>Nested bottom frame</P></BODY></HTML>"
    ),
    "/parent-target.html": (
        "<HTML><HEAD><TITLE>Parent target</TITLE></HEAD><BODY>"
        "<P>Parent target frame</P>"
        "<P><A HREF='/top-target.html'>Top target</A></P>"
        "</BODY></HTML>"
    ),
    "/top-target.html": (
        "<HTML><HEAD><TITLE>Top target</TITLE></HEAD><BODY>"
        "<P>Top target document</P></BODY></HTML>"
    ),
}
HEADER_PREFIX = b"<HTML><BODY>"
HEADER_PADDING = b"<!--" + b"x" * (8192 - 7) + b"-->"
HEADER_FINISH = b"<P>Header transfer finished</P></BODY></HTML>"
HEADER_BODY = (HEADER_PREFIX + b"<H1>Header target frame</H1>" +
               HEADER_PADDING * 4 + HEADER_FINISH)
TOP_FINISH = b"<P>Top transfer finished</P></BODY></HTML>"
TOP_BODY = (HEADER_PREFIX + b"<H1>Top target document</H1>" +
            HEADER_PADDING * 4 + TOP_FINISH)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def do_GET(self):
        parsed = urlsplit(self.path)
        self.server.requests.append({"path": parsed.path, "query": parsed.query})
        if parsed.path in ("/header-target.html", "/top-target.html"):
            top = parsed.path == "/top-target.html"
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=us-ascii")
            self.send_header("Content-Length", str(len(TOP_BODY if top else HEADER_BODY)))
            self.send_header("Window-Target", "_top" if top else "content")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(HEADER_PREFIX)
            self.wfile.flush()
            time.sleep(0.15)
            self.wfile.write(b"<H1>Top target document</H1>" if top else b"<H1>Header target frame</H1>")
            for unused in range(4):
                self.wfile.write(HEADER_PADDING)
                self.wfile.flush()
            self.wfile.write(TOP_FINISH if top else HEADER_FINISH)
            return
        content_type = "text/html; charset=us-ascii"
        if parsed.path == "/frame.gif":
            body = GIF
            content_type = "image/gif"
        elif parsed.path == "/frame-submit":
            self.server.submissions.append(parse_qs(parsed.query, keep_blank_values=True))
            body = b"<HTML><BODY>Frame form submitted</BODY></HTML>"
        elif parsed.path in PAGES:
            body = PAGES[parsed.path].encode("ascii")
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", content_type)
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
    server.submissions = []
    worker = threading.Thread(target=server.serve_forever,
                              kwargs={"poll_interval": 0.05}, daemon=True)
    worker.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=2)
        (output / "frames-requests.json").write_text(
            json.dumps(server.requests, indent=2) + "\n", encoding="utf-8")
        (output / "frames-submissions.json").write_text(
            json.dumps(server.submissions, indent=2) + "\n", encoding="utf-8")


def run_smoke(executable, output, server, baseline):
    report = output / "frames-smoke.txt"
    report.unlink(missing_ok=True)
    profile = output / "frames profile with spaces 100%"
    profile.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    url = "http://127.0.0.1:{}/frames.html".format(server.server_port)
    command = [str(executable), "--smoke-test" if baseline else "--frames-smoke", str(report), url]
    try:
        process = subprocess.run(command, env=environment, cwd=str(output),
                                 capture_output=True, text=True, encoding="utf-8",
                                 errors="replace", timeout=35, check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / ("frames-" + name + ".txt")).write_text(value or "", encoding="utf-8")
        raise RuntimeError("Frames exceeded the 35-second timeout") from error
    for name, value in (("stdout", process.stdout), ("stderr", process.stderr)):
        (output / ("frames-" + name + ".txt")).write_text(value, encoding="utf-8")
    if not baseline and process.returncode:
        raise RuntimeError("Frames UI exited with status {}".format(process.returncode))
    text = report.read_text(encoding="utf-8", errors="replace")
    paths = [request["path"] for request in server.requests]
    missing = [path for path in tuple(PAGES) + ("/frame.gif", "/header-target.html") if path not in paths]
    if missing:
        raise RuntimeError("Native frames omitted child requests: " + ", ".join(missing))
    fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    for name in ("initial_render", "form", "header_target", "targeted", "resized", "nested",
                 "parent_target", "top_target", "cleanup"):
        if fields.get(name) != "1":
            raise RuntimeError("Frames report has unexpected {}:\n{}".format(name, text))
    if int(fields.get("child_count", "0")) < 2:
        raise RuntimeError("Frames report contains fewer than two child contexts:\n" + text)
    if server.submissions != [{"query": ["frame & value"], "go": ["Submit"]}]:
        raise RuntimeError("Child frame form submission differs: " + repr(server.submissions))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", "--exe", type=Path, required=True)
    parser.add_argument("--output", "--output-dir", type=Path, required=True)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        with fixture_server(output) as server:
            run_smoke(args.executable.resolve(), output, server, args.baseline)
    except (OSError, RuntimeError, ValueError) as error:
        print("Windows frames smoke failed: " + str(error), file=sys.stderr)
        return 1
    print("Windows frames smoke passed: child rendering, form, named/parent/top targets, resize, nested frames, cleanup")
    return 0


if __name__ == "__main__":
    sys.exit(main())
