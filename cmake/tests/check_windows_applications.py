"""Check native Apps controls and actual child-process arguments/source bytes."""
import argparse
import http.server
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import time

SOURCE = b'<html><head><title>Supporting apps fixture</title></head><body>Original source &amp; bytes.<!-- \xe9 --></body></html>'


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests.append(self.path)
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=iso-8859-1')
        self.send_header('Content-Length', str(len(SOURCE) + (100 if self.path == '/truncated' else 0)))
        self.end_headers()
        try:
            if self.path in ('/slow', '/slowclose'):
                self.wfile.write(SOURCE[:16])
                time.sleep(.3)
                self.wfile.write(SOURCE[16:])
            else:
                self.wfile.write(SOURCE)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_POST(self):
        self.rfile.read(int(self.headers.get('Content-Length', '0')))
        self.do_GET()

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    parser.add_argument('--probe', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='Apps % ', dir=output))
    program = run / 'Terminal & % Probe.exe'
    shutil.copy2(args.probe, program)
    temporary = run / 'Temp & spaces %'
    temporary.mkdir()
    long_directory = run / ('L' * (245 - len(str(run)) - 1))
    long_directory.mkdir()
    env = os.environ.copy()
    env['NETSCAPE_PROFILE_DIR'] = str(run / 'profile')
    env['NETSCAPE_APP_LONG_TEMP'] = str(long_directory)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    server.requests = []
    thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .05}, daemon=True)
    thread.start()
    try:
        for reopen, missing in [(False, False), (True, False), (True, True)]:
            if missing:
                temporary.rename(run / 'Retired temp')
            report = run / ('fallback.txt' if missing else 'reopen.txt' if reopen else 'applications.txt')
            ini = run / 'profile' / 'browser.ini'
            old_ini = ini.read_bytes() if missing else None
            result = subprocess.run([args.executable, '--applications-reopen-smoke' if reopen else '--applications-smoke',
                                     str(report), 'http://127.0.0.1:%d/' % server.server_port, str(program), str(temporary)],
                                    env=env, capture_output=True, timeout=110)
            Path(str(report) + '.process.txt').write_bytes(result.stdout + result.stderr)
            values = dict(line.split('=', 1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
            print(report, values)
            assert result.returncode == 0, (result.returncode, values)
            assert all(value == '1' for value in values.values()), values
            if missing:
                assert values.get('startup_fallback') == '1' and ini.read_bytes() == old_ini, values
                continue
            assert Path(str(report) + '.source.bytes').read_bytes() == SOURCE
            assert Path(str(report) + '.closed.bytes').read_bytes() == SOURCE
            assert Path(str(report) + '.long.bytes').read_bytes() == SOURCE
            quoted = dict(line.split('=', 1) for line in Path(str(report) + '.quote.argv').read_text(encoding='latin1').splitlines())
            assert [quoted['arg%d' % n] for n in range(1, int(quoted['argc']))] == ['quoted "word"\\', '%PATH%', 'semi;&', ''], quoted
            for name, expected in [('telnet', ['-l', 'john.doe', 'terminal.test', '2323']),
                                   ('tn3270', ['-l', 'jane_doe-user', 'terminal.test', '3270'])]:
                data = dict(line.split('=', 1) for line in Path(str(report) + '.' + name + '.argv').read_text(encoding='latin1').splitlines())
                actual = [data['arg%d' % n] for n in range(1, int(data['argc']))]
                assert actual == expected, (name, actual, expected)
            data = dict(line.split('=', 1) for line in Path(str(report) + '.source.argv').read_text(encoding='latin1').splitlines())
            assert data['argc'] == '2' and str(temporary) in data['arg1'], data
        assert server.requests.count('/post') == 2, server.requests
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
    print('Supporting application preferences and process arguments verified')


if __name__ == '__main__':
    main()
