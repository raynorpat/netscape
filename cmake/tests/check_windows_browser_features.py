"""Verify native Windows image drawing and forms using local HTTP and files.

The runtime fixtures and assertions use only the Python standard library.
--baseline uses the old render-only CLI to demonstrate missing image/form work.
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
from urllib.parse import parse_qs, urlsplit


GIF = base64.b64decode("R0lGODdhAgACAIEAAP8AAAD/AAAA////ACwAAAAAAgACAAAIBwABBBAwICAAOw==")
TRANSPARENT_GIF = base64.b64decode("R0lGODlhAgACAIEAAAD///8AAAAA/wAAACH5BAEAAAAALAAAAAACAAIAAAgHAAEEEAAgIAA7")
BACKGROUND_GIF = base64.b64decode("R0lGODdhAgACAIEAAAD//wAAAAAAAAAAACwAAAAAAgACAAAIBgABCAQQEAA7")
JPEG = base64.b64decode(
    "/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/"
    "2wBDAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/wAARCAAQABADAREAAhEBAxEB/"
    "8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2Jy"
    "ggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5"
    "usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcF"
    "BAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hp"
    "anN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAM"
    "BAAIRAxEAPwD8X6/ynP8Av4P6UK/y3P8Awrz+Dev/AGmD/qwP9iiv/AHP9UD/2Q=="
)
XBM = (b"#define checker_width 2\n#define checker_height 2\n"
       b"static unsigned char checker_bits[] = { 0x01, 0x02 };\n")
IMAGE_PATHS = ("/quadrants.gif", "/quadrants.jpg", "/checker.xbm", "/transparent.gif")
IMAGE_HTML = (
    "<HTML><HEAD><TITLE>Windows image smoke</TITLE></HEAD>"
    "<BODY BGCOLOR='#00ffff' BACKGROUND='/background.gif'>"
    "<H1>Native image pixels</H1>"
    "<P><IMG SRC='/quadrants.gif' ALT='GIF quadrants'></P>"
    "<P><IMG SRC='/quadrants.jpg' ALT='JPEG quadrants'></P>"
    "<P><IMG SRC='/checker.xbm' ALT='XBM checker'></P>"
    "<P><IMG SRC='/transparent.gif' ALT='Transparent quadrants'></P>"
    "<P><IMG SRC='/quadrants.gif' WIDTH='32' HEIGHT='32' ALT='Scaled GIF'></P>"
    "<P><IMG SRC='/missing.gif' ALT='Expected missing image'></P>"
    "<P>Images finished</P>"
    "</BODY></HTML>"
).encode("ascii")
EXPECTED_FORM = {
    "query": ["native & value"],
    "secret": ["password"],
    "note": ["line one\r\nline two"],
    "choice": ["b"],
    "pick": ["two"],
    "multi": ["one", "three"],
    "hidden": ["fixed"],
    "go": ["Submit"],
}


def form_html(method, image_submit=False):
    html = ("""<HTML><HEAD><TITLE>Windows forms smoke</TITLE></HEAD><BODY>
<H1>Native form controls</H1>
<FORM METHOD='{method}' ACTION='/submit-{lower}'>
<P>Query <INPUT TYPE='text' NAME='query' VALUE='default query'></P>
<P>Password <INPUT TYPE='password' NAME='secret' VALUE='default secret'></P>
<P>Note <TEXTAREA NAME='note' ROWS='3' COLS='30'>default note</TEXTAREA></P>
<P><INPUT TYPE='checkbox' NAME='enabled' VALUE='yes' CHECKED>Enabled</P>
<P><INPUT TYPE='radio' NAME='choice' VALUE='a' CHECKED>Choice A
<INPUT TYPE='radio' NAME='choice' VALUE='b'>Choice B</P>
<P><SELECT NAME='pick'><OPTION VALUE='one' SELECTED>One
<OPTION VALUE='two'>Two<OPTION VALUE='three'>Three</SELECT></P>
<P><SELECT NAME='multi' MULTIPLE SIZE='3'><OPTION VALUE='one'>One
<OPTION VALUE='two' SELECTED>Two<OPTION VALUE='three'>Three</SELECT></P>
<INPUT TYPE='hidden' NAME='hidden' VALUE='fixed'>
<INPUT TYPE='submit' NAME='go' VALUE='Submit'><INPUT TYPE='reset' VALUE='Reset'>
</FORM></BODY></HTML>""").format(method=method, lower=method.lower()).encode("ascii")
    if image_submit:
        html = html.replace(b"<INPUT TYPE='submit' NAME='go' VALUE='Submit'>",
                            b"<INPUT TYPE='image' NAME='point' SRC='/quadrants.gif'>")
    return html


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def respond(self, body, content_type="text/html; charset=us-ascii"):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def record(self, body=b""):
        parsed = urlsplit(self.path)
        entry = {"method": self.command, "path": parsed.path,
                 "query": parsed.query, "content_type": self.headers.get("Content-Type", ""),
                 "body": body.decode("ascii", errors="replace")}
        self.server.requests.append(entry)
        return parsed

    def submission(self, parsed, body=b""):
        encoded = parsed.query if self.command == "GET" else body.decode("ascii", errors="replace")
        self.server.submissions.append({
            "method": self.command, "path": parsed.path,
            "content_type": self.headers.get("Content-Type", ""),
            "fields": parse_qs(encoded, keep_blank_values=True),
        })
        self.respond(b"<HTML><HEAD><TITLE>Form submitted</TITLE></HEAD><BODY>Native form received</BODY></HTML>")

    def do_GET(self):
        parsed = self.record()
        if parsed.path == "/submit-get":
            self.submission(parsed)
            return
        resources = {
            "/images.html": (IMAGE_HTML, "text/html; charset=us-ascii"),
            "/images-broken-background.html": (
                IMAGE_HTML.replace(b"BACKGROUND='/background.gif'", b"BACKGROUND='/missing-background.gif'"),
                "text/html; charset=us-ascii"),
            "/quadrants.gif": (GIF, "image/gif"),
            "/quadrants.jpg": (JPEG, "image/jpeg"),
            "/checker.xbm": (XBM, "image/x-xbitmap"),
            "/transparent.gif": (TRANSPARENT_GIF, "image/gif"),
            "/background.gif": (BACKGROUND_GIF, "image/gif"),
            "/forms-get": (form_html("GET"), "text/html; charset=us-ascii"),
            "/forms-post": (form_html("POST"), "text/html; charset=us-ascii"),
            "/forms-image": (form_html("GET", image_submit=True), "text/html; charset=us-ascii"),
        }
        resource = resources.get(parsed.path)
        if resource is None:
            self.send_error(404)
            return
        self.respond(*resource)

    def do_POST(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.send_error(400)
            return
        if length < 0 or length > 65536:
            self.send_error(413)
            return
        body = self.rfile.read(length)
        parsed = self.record(body)
        if parsed.path != "/submit-post":
            self.send_error(404)
            return
        self.submission(parsed, body)

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
        (output / "browser-feature-requests.json").write_text(
            json.dumps(server.requests, indent=2) + "\n", encoding="utf-8")
        (output / "browser-feature-submissions.json").write_text(
            json.dumps(server.submissions, indent=2) + "\n", encoding="utf-8")


def run_browser(executable, output, server, mode, page, baseline,
                source_url=None, artifact_name=None):
    name = artifact_name or (Path(page).stem if mode == "images" else "forms-" + page.rsplit("-", 1)[-1])
    report = output / (name + "-smoke.txt")
    report.unlink(missing_ok=True)
    profile = output / (name + " profile with spaces 100%")
    profile.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    url = source_url or "http://127.0.0.1:{}{}".format(server.server_port, page)
    command = [str(executable), "--smoke-test" if baseline else "--" + mode + "-smoke", str(report), url]
    try:
        result = subprocess.run(command, env=environment, cwd=str(output),
                                capture_output=True, text=True, encoding="utf-8",
                                errors="replace", timeout=35, check=False)
    except subprocess.TimeoutExpired as error:
        for label, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / (name + "-" + label + ".txt")).write_text(value or "", encoding="utf-8")
        raise RuntimeError(name + " exceeded the 35-second timeout") from error
    for label, value in (("stdout", result.stdout), ("stderr", result.stderr)):
        (output / (name + "-" + label + ".txt")).write_text(value, encoding="utf-8")
    if result.returncode:
        raise RuntimeError("{} exited with status {}".format(name, result.returncode))
    text = report.read_text(encoding="utf-8", errors="replace")
    fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    return text, fields


def check_image_report(text, fields):
    for name, expected in (("title", "Windows image smoke"), ("image_pixels_ok", "1"), ("noerrors", "1")):
        if fields.get(name) != expected:
            raise RuntimeError("Image report has unexpected {}:\n{}".format(name, text))
    if int(fields.get("image_count", "0")) < 5:
        raise RuntimeError("Fewer than five images were decoded and drawn:\n" + text)


def check_images(executable, output, server, baseline):
    for page, background in (("/images.html", "/background.gif"),
                             ("/images-broken-background.html", "/missing-background.gif")):
        before = len(server.requests)
        text, fields = run_browser(executable, output, server, "images", page, baseline)
        paths = [request["path"] for request in server.requests[before:]]
        missing = [path for path in IMAGE_PATHS + (background, "/missing.gif") if path not in paths]
        if missing:
            raise RuntimeError("Native image loading omitted requests: " + ", ".join(missing))
        check_image_report(text, fields)
    local = output / "local images with spaces 100%"
    local.mkdir(parents=True, exist_ok=True)
    for name, data in (("quadrants.gif", GIF), ("quadrants.jpg", JPEG), ("checker.xbm", XBM),
                       ("transparent.gif", TRANSPARENT_GIF), ("background.gif", BACKGROUND_GIF)):
        (local / name).write_bytes(data)
    (local / "missing.gif").unlink(missing_ok=True)
    html = local / "images.html"
    html.write_bytes(IMAGE_HTML.replace(b"SRC='/", b"SRC='").replace(b"BACKGROUND='/", b"BACKGROUND='"))
    text, fields = run_browser(executable, output, server, "images", "/images.html", baseline,
                               source_url=str(html), artifact_name="images-local")
    check_image_report(text, fields)


def check_forms(executable, output, server, baseline):
    for suffix, method in (("get", "GET"), ("post", "POST"), ("image", "GET")):
        before = len(server.submissions)
        requests_before = len(server.requests)
        text, fields = run_browser(executable, output, server, "forms", "/forms-" + suffix, baseline)
        received = server.submissions[before:]
        if len(received) != 1:
            raise RuntimeError("{} form produced {} submissions, expected one".format(method, len(received)))
        submission = received[0]
        if submission["method"] != method or submission["path"] != "/submit-" + method.lower():
            raise RuntimeError("Form used the wrong HTTP method or action: " + repr(submission))
        if method == "POST" and submission["content_type"].split(";", 1)[0].lower() != "application/x-www-form-urlencoded":
            raise RuntimeError("POST form used the wrong Content-Type: " + repr(submission))
        expected = dict(EXPECTED_FORM)
        if suffix == "image":
            del expected["go"]
            expected.update({"point.x": ["1"], "point.y": ["1"]})
            if not any(request["path"] == "/quadrants.gif" for request in server.requests[requests_before:]):
                raise RuntimeError("Image submit control did not request its GIF")
        if submission["fields"] != expected:
            raise RuntimeError("{} form fields differ:\nexpected {!r}\nreceived {!r}".format(
                suffix, expected, submission["fields"]))
        for name in ("reset_ok", "submitted"):
            if fields.get(name) != "1":
                raise RuntimeError("{} form report is missing {}:\n{}".format(method, name, text))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", "--exe", type=Path, required=True)
    parser.add_argument("--output", "--output-dir", type=Path, required=True)
    parser.add_argument("--mode", choices=("images", "forms", "all"), default="all")
    parser.add_argument("--baseline", action="store_true",
                        help="Use the old renderer smoke CLI to demonstrate missing features")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        with fixture_server(output) as server:
            if args.mode in ("all", "images"):
                check_images(args.executable.resolve(), output, server, args.baseline)
            if args.mode in ("all", "forms"):
                check_forms(args.executable.resolve(), output, server, args.baseline)
    except (OSError, RuntimeError, ValueError) as error:
        print("Windows browser feature smoke failed: " + str(error), file=sys.stderr)
        return 1
    print("Windows {} smoke passed against isolated fixtures".format(args.mode))
    return 0


if __name__ == "__main__":
    sys.exit(main())
