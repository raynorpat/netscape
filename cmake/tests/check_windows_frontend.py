"""Exercise the native browser's DNS, HTTP, parser, layout, and GDI path."""

import argparse
import http.server
import os
from pathlib import Path
import subprocess
import sys
import threading
from urllib.parse import urlsplit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    profile = output / "profile with spaces 100%"
    profile.mkdir(parents=True, exist_ok=True)
    report = output / "frontend-http-smoke.txt"
    report.unlink(missing_ok=True)
    pages = {
        "/index.html": (
            "<html><head><title>Native frontend smoke</title></head>"
            "<body><h1>Windows core renderer</h1>"
            "<p>Hello <b>Netscape</b> 3.02 over local HTTP.</p>"
            "<ul><li>Generated HTTP page</li></ul>"
            "<a href='/linked.html'>Another generated page</a></body></html>"
        ).encode("ascii"),
        "/linked.html": (
            "<html><head><title>Native frontend smoke</title></head>"
            "<body><h1>Windows core renderer</h1>"
            "<p>Netscape generated linked page.</p></body></html>"
        ).encode("ascii"),
    }
    requests = []
    user_agents = []

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.0"

        def setup(self):
            super().setup()
            self.connection.settimeout(5)

        def do_GET(self):
            path = urlsplit(self.path).path
            requests.append(path)
            if path == "/index.html":
                user_agents.append(self.headers.get("User-Agent", ""))
            body = pages.get(path)
            if body is None:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=iso-8859-1")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, format, *values):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    worker = threading.Thread(
        target=server.serve_forever, kwargs={"poll_interval": 0.05}, daemon=True
    )
    worker.start()
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    url = "http://localhost:{}/index.html".format(server.server_port)
    failure = None
    try:
        process = subprocess.run(
            [str(executable), "--smoke-test", str(report), url],
            env=environment,
            cwd=str(output),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=25,
            check=False,
        )
        (output / "frontend-http-stdout.txt").write_text(process.stdout, encoding="utf-8")
        (output / "frontend-http-stderr.txt").write_text(process.stderr, encoding="utf-8")
        if process.returncode:
            raise RuntimeError("Browser exited with status {}".format(process.returncode))
        text = report.read_text(encoding="utf-8", errors="replace")
        fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
        if fields.get("passed") != "1" or fields.get("completed") != "1":
            raise RuntimeError("Browser did not complete its render smoke test:\n" + text)
        if int(fields.get("status", "-1")) < 0:
            raise RuntimeError("Browser reported an HTTP load failure:\n" + text)
        if "Native frontend smoke" not in fields.get("title", ""):
            raise RuntimeError("Expected HTTP document title is missing:\n" + text)
        if "Windows core renderer" not in text or "Netscape" not in text:
            raise RuntimeError("Expected HTTP document text was not rendered:\n" + text)
        if "/index.html" not in requests:
            raise RuntimeError("The local server did not receive the document request")
        if not user_agents or any(value != "Mozilla/3.02 (Win32)" for value in user_agents):
            raise RuntimeError("Unexpected HTTP User-Agent: {!r}".format(user_agents))
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / ("frontend-http-" + name + ".txt")).write_text(
                value or "", encoding="utf-8"
            )
        failure = "Browser exceeded the 25-second timeout"
    except (OSError, RuntimeError, ValueError) as error:
        failure = str(error)
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=2)
        (output / "frontend-http-requests.txt").write_text(
            "\n".join(requests) + "\n", encoding="utf-8"
        )
        (output / "frontend-http-user-agents.txt").write_text(
            "\n".join(user_agents) + "\n", encoding="utf-8"
        )
        if worker.is_alive() and failure is None:
            failure = "The local HTTP server did not stop within two seconds"
    if failure:
        print("Windows frontend HTTP smoke failed: " + failure, file=sys.stderr)
        return 1
    print("Windows frontend HTTP smoke passed: async localhost DNS, HTTP, layout, and rendering")
    return 0


if __name__ == "__main__":
    sys.exit(main())
