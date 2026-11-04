"""Exercise native Colors controls, cross-process persistence and actual IL tiles."""
import argparse
import base64
import http.server
import os
import re
from pathlib import Path
import struct
import subprocess
import tempfile
import threading


def gif(color):
    data = bytearray(base64.b64decode('R0lGODlhAQABAIAAAAD/AP///yH5BAEAAAEALAAAAAABAAEAAAICRAEAOw=='))
    data[13:16] = bytes(color)
    return data


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests.append(self.path)
        if self.path == '/page.gif':
            body, kind = gif((30, 60, 220)), 'image/gif'
        else:
            background = ' background="/page.gif"' if self.path in ('/pagebg', '/override') else ''
            body = ('<html><body%s><p>Background preference fixture</p></body></html>' % background).encode('ascii')
            kind = 'text/html'
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def color_at(path):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    assert bits == 32 and width >= 700 and abs(height) >= 500
    row = 400 if height < 0 else height - 401
    return tuple(reversed(data[offset + row * width * 4 + 600 * 4:offset + row * width * 4 + 600 * 4 + 3]))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='Background % ', dir=output))
    image = run / 'my backdrop %.gif'
    image.write_bytes(gif((220, 40, 30)))
    env = os.environ.copy()
    env['NETSCAPE_PROFILE_DIR'] = str(run / 'profile')
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.requests = []
    server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .05}, daemon=True)
    thread.start()
    try:
        for reopen, missing in [(False, False), (True, False), (True, True)]:
            if missing:
                image.unlink()
            report = run / ('fallback.txt' if missing else 'reopen.txt' if reopen else 'background.txt')
            ini = run / 'profile' / 'browser.ini'
            old_ini = ini.read_bytes() if missing else None
            command = '--background-reopen-smoke' if reopen else '--background-smoke'
            result = subprocess.run([args.executable, command, str(report),
                                     'http://127.0.0.1:%d/' % server.server_port, str(image)],
                                    env=env, capture_output=True, timeout=80)
            Path(str(report) + '.process.txt').write_bytes(result.stdout + result.stderr)
            values = dict(line.split('=', 1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
            print(report, values)
            assert result.returncode == 0, (result.returncode, values)
            if missing:
                assert values.get('fallback') == '1' and ini.read_bytes() == old_ini, values
                assert color_at(Path(str(report) + '.clear.bmp')) == (255, 255, 255)
                continue
            required = ['ui', 'persisted', 'default_image', 'page_image', 'override_image', 'passed']
            if not reopen:
                required += ['controls', 'cancel', 'invalid', 'save_failure', 'saved']
            assert all(values.get(key) == '1' for key in required), values
            edited = Path(str(report) + '.editor.html')
            if edited.exists():
                assert not re.search(r'\bbackground\s*=', edited.read_text(encoding='latin1'), re.I)
            for name, color in [('default', (220, 40, 30)), ('page', (30, 60, 220)), ('override', (220, 40, 30)), ('clear', (255, 255, 255))]:
                actual = color_at(Path(str(report) + '.' + name + '.bmp'))
                assert actual == color, (name, actual, color)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
    print('Background preferences, persistence and rendered tiles verified')


if __name__ == '__main__':
    main()
