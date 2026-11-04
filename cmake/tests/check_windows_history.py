"""Verify native session-history traversal, form snapshots, and reload behavior."""

import argparse
from contextlib import contextmanager
import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
from urllib.parse import parse_qs, urlsplit


PAGES = {
    "/history-a.html": (
        "<HTML><HEAD><TITLE>History A</TITLE></HEAD><BODY>"
        "<H1>History A</H1>"
        "<FORM><INPUT TYPE='text' NAME='query' VALUE='initial'>"
        "<SELECT NAME='pick'><OPTION VALUE='one' SELECTED>One"
        "<OPTION VALUE='two'>Two</SELECT></FORM>"
        "<P>History A first paragraph.</P><P>History A second paragraph.</P>"
        "<P>History A third paragraph.</P>"
        "<P><A HREF='/history-b.html'>Next page</A></P>"
        "<P><A HREF='/history-c.html'>Third page</A></P>"
        + "".join("<P>History A scroll line {:02d}</P>".format(number) for number in range(60))
        + "</BODY></HTML>"
    ),
    "/history-b.html": (
        "<HTML><HEAD><TITLE>History B</TITLE></HEAD><BODY>"
        "<H1>History B</H1>"
        "<P><A HREF='/history-c.html'>Third page</A></P>"
        "</BODY></HTML>"
    ),
    "/history-c.html": (
        "<HTML><HEAD><TITLE>History C</TITLE></HEAD><BODY>"
        "<H1>History C</H1><P>Reload keeps the current history entry.</P>"
        "<FORM METHOD='POST' ACTION='/history-post'>"
        "<INPUT TYPE='text' NAME='query' VALUE='post once'>"
        "<INPUT TYPE='submit' NAME='go' VALUE='Post'></FORM>"
        "</BODY></HTML>"
    ),
}


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def do_GET(self):
        path = urlsplit(self.path).path
        self.server.requests.append(path)
        html = PAGES.get(path)
        if html is None:
            self.send_error(404)
            return
        body = html.encode("ascii")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=us-ascii")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

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
        path = urlsplit(self.path).path
        self.server.posts.append({
            "path": path, "body": body.decode("ascii", errors="replace"),
            "content_length": length, "received_length": len(body),
            "content_type": self.headers.get("Content-Type", ""),
            "fields": parse_qs(body.decode("ascii", errors="replace"), keep_blank_values=True),
        })
        if path != "/history-post" or len(body) != length:
            self.send_error(400)
            return
        response = (b"<HTML><HEAD><TITLE>History POST</TITLE></HEAD><BODY>"
                    b"<H1>History POST</H1></BODY></HTML>")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=us-ascii")
        self.send_header("Content-Length", str(len(response)))
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Expires", "0")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(response)

    def log_message(self, format, *values):
        pass


@contextmanager
def fixture_server(output):
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server.requests = []
    server.posts = []
    worker = threading.Thread(target=server.serve_forever,
                              kwargs={"poll_interval": 0.05}, daemon=True)
    worker.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=2)
        (output / "history-requests.json").write_text(
            json.dumps(server.requests, indent=2) + "\n", encoding="utf-8")
        (output / "history-posts.json").write_text(
            json.dumps(server.posts, indent=2) + "\n", encoding="utf-8")


def run_smoke(executable, output, server, baseline):
    report = output / "history-smoke.txt"
    report.unlink(missing_ok=True)
    profile = output / "history profile with spaces 100%"
    profile.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    url = "http://127.0.0.1:{}/history-a.html".format(server.server_port)
    command = [str(executable), "--smoke-test" if baseline else "--history-smoke", str(report), url]
    try:
        process = subprocess.run(command, env=environment, cwd=str(output),
                                 capture_output=True, text=True, encoding="utf-8",
                                 errors="replace", timeout=35, check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / ("history-" + name + ".txt")).write_text(value or "", encoding="utf-8")
        raise RuntimeError("History exceeded the 35-second timeout") from error
    for name, value in (("stdout", process.stdout), ("stderr", process.stderr)):
        (output / ("history-" + name + ".txt")).write_text(value, encoding="utf-8")
    if not baseline and process.returncode:
        raise RuntimeError("History UI exited with status {}".format(process.returncode))
    text = report.read_text(encoding="utf-8", errors="replace")
    missing = [path for path in PAGES if path not in server.requests]
    if missing:
        raise RuntimeError("History traversal omitted page requests: " + ", ".join(missing))
    if server.requests.count("/history-c.html") < 2:
        raise RuntimeError("Reload did not fetch the current page again")
    fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    for name in ("traversal", "form_restored", "scroll_restored", "forward_truncated", "reload",
                 "command_state", "history_dialog", "global_recent", "post_cancelled"):
        if fields.get(name) != "1":
            raise RuntimeError("History report has unexpected {}:\n{}".format(name, text))
    if len(server.posts) != 1:
        raise RuntimeError("History form sent {} POST requests, expected exactly one".format(len(server.posts)))
    post = server.posts[0]
    if (post["path"] != "/history-post" or post["fields"] != {"query": ["post once"], "go": ["Post"]}
            or post["received_length"] != post["content_length"]
            or post["content_type"].split(";", 1)[0].lower() != "application/x-www-form-urlencoded"):
        raise RuntimeError("History form POST differs: " + repr(post))
    reopen_report = output / "history-reopen-smoke.txt"
    reopen_report.unlink(missing_ok=True)
    try:
        reopened = subprocess.run([str(executable), "--history-reopen-smoke", str(reopen_report), url],
                                  env=environment, cwd=str(output), capture_output=True, text=True,
                                  encoding="utf-8", errors="replace", timeout=35, check=False)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("History reopen exceeded the 35-second timeout") from error
    for name, value in (("stdout", reopened.stdout), ("stderr", reopened.stderr)):
        (output / ("history-reopen-" + name + ".txt")).write_text(value, encoding="utf-8")
    if reopened.returncode:
        raise RuntimeError("History reopen exited with status {}".format(reopened.returncode))
    reopen_text = reopen_report.read_text(encoding="utf-8", errors="replace")
    reopened_fields = dict(line.split("=", 1) for line in reopen_text.splitlines() if "=" in line)
    if reopened_fields.get("persisted") != "1":
        raise RuntimeError("Global history did not persist across processes:\n" + reopen_text)
    if len(server.posts) != 1:
        raise RuntimeError("History reopen repeated the POST submission")


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
        print("Windows history smoke failed: " + str(error), file=sys.stderr)
        return 1
    print("Windows history smoke passed: traversal, form/scroll restore, reload, POST cancellation, persisted global history")
    return 0


if __name__ == "__main__":
    sys.exit(main())
