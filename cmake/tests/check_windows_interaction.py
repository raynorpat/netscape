"""Exercise native mouse gestures through the original LO engine, on loopback."""
import argparse
import http.server
import os
from pathlib import Path
import subprocess
import tempfile
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests.append(self.path)
        if self.path == '/selectionframes':
            body = b'<html><frameset cols="50%,50%"><frame name="selectionLeft" src="/selection"><frame src="/blank"></frameset></html>'
        elif self.path == '/selection':
            body = ('<html><head><title>Browser mouse selection</title></head><body>'
                    '<pre>Alpha Bravo Charlie Delta Echo</pre>'
                    '<p><a href="/clicked">Drag this link</a></p>' +
                    ''.join('<p>Scrolling selection line %d</p>' % n for n in range(80)) +
                    '</body></html>').encode('ascii')
        else:
            body = b'<html><body>Clicked link</body></html>'
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='Browser gestures ', dir=output))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    server.requests = []
    thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .05}, daemon=True)
    thread.start()
    report = run / 'interaction.txt'
    env = os.environ.copy()
    env['NETSCAPE_PROFILE_DIR'] = str(run / 'profile')
    try:
        result = subprocess.run([args.executable, '--interaction-smoke', str(report),
                                 'http://127.0.0.1:%d/' % server.server_port],
                                env=env, capture_output=True, timeout=75)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
    (run / 'process.txt').write_bytes(result.stdout + result.stderr)
    values = dict(line.split('=', 1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
    print('Report:', report, values)
    assert result.returncode == 0, (result.returncode, values)
    assert all(values.get(key) == '1' for key in ['passed', 'drag_link', 'capture', 'capture_cancel',
                                                'navigation_cancel', 'autoscroll', 'capture_released',
                                                'clicked', 'frame_selection']), values
    for name, expected in [('forward', 'Bravo Charlie'), ('backward', 'Bravo Charlie'),
                           # LO's original word selection includes following spaces.
                           ('clipboard', 'Bravo Charlie'), ('word', 'Bravo '),
                           ('shift', 'Bravo Charlie Delta'), ('linkdrag', 'Drag this lin'),
                           ('frameclipboard', 'Scrolling')]:
        actual = Path(str(report) + '.' + name).read_text(encoding='latin1')
        assert actual == expected, (name, repr(actual), expected)
    assert server.requests.count('/clicked') == 1, server.requests
    print('Native selection gestures and clipboard verified')


if __name__ == '__main__':
    main()
