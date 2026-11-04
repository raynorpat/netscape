"""Publish EDT HTML and selected assets to loopback HTTP and FTP fixtures."""
import argparse
import base64
import contextlib
import http.server
from html.parser import HTMLParser
import os
from pathlib import Path
import posixpath
import socket
import socketserver
import subprocess
import tempfile
import threading


class HTTP(http.server.BaseHTTPRequestHandler):
    def do_PUT(self):
        body = self.rfile.read(int(self.headers.get('Content-Length', 0)))
        if self.path.startswith('/auth/'):
            expected = 'Basic ' + base64.b64encode(b'gold@user%+:p@ss%+ word').decode('ascii')
            if self.headers.get('Authorization') != expected:
                self.server.challenges += 1
                self.send_response(401)
                self.send_header('WWW-Authenticate', 'Basic realm="Gold publish fixture"')
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
        self.server.uploads.append((self.path, body))
        # The native smoke deliberately publishes once to /error/, retries,
        # then cancels /cancel/ without waiting for a server response.
        if '/cancel/' in self.path:
            self.server.cancel_seen.set()
            self.server.stop.wait(5)
            return
        self.send_response(503 if '/error/' in self.path else 201)
        self.send_header('Content-Length', '0')
        self.end_headers()

    def log_message(self, *args):
        pass


class HTMLText(HTMLParser):
    def __init__(self):
        super().__init__()
        self.text = []

    def handle_data(self, value):
        self.text.append(value)


class FTP(socketserver.StreamRequestHandler):
    def handle(self):
        passive = None
        directory = '/'
        def reply(value):
            self.wfile.write(value.encode('ascii') + b'\r\n')
            self.wfile.flush()
        reply('220 Loopback Gold fixture')
        try:
            for raw in self.rfile:
                line = raw.decode('ascii').strip()
                command, _, value = line.partition(' ')
                command = command.upper()
                if command == 'USER': reply('331 Password required')
                elif command == 'PASS': reply('230 Logged in')
                elif command == 'SYST': reply('215 UNIX Type: L8')
                elif command == 'PWD': reply('257 "/" is current directory')
                elif command == 'TYPE': reply('200 Type set')
                elif command == 'CWD':
                    target = posixpath.normpath(posixpath.join(directory, value))
                    if target in {'/', '/publish', '/error', '/cancel', '/auth'}:
                        directory = target
                        reply('250 Directory changed')
                    else:
                        # A real server rejects CWD into a file. Netlib probes
                        # the full upload path before falling back to its parent.
                        reply('550 Not a directory')
                elif command == 'PASV':
                    passive = socket.socket()
                    passive.bind(('127.0.0.1', 0))
                    passive.listen(1)
                    passive.settimeout(10)
                    port = passive.getsockname()[1]
                    reply(f'227 Entering Passive Mode (127,0,0,1,{port // 256},{port % 256})')
                elif command == 'STOR':
                    reply('150 Opening binary data connection')
                    connection, _ = passive.accept()
                    with connection:
                        chunks = []
                        while True:
                            block = connection.recv(65536)
                            if not block: break
                            chunks.append(block)
                    passive.close()
                    passive = None
                    self.server.uploads.append((posixpath.normpath(posixpath.join(directory, value)), b''.join(chunks)))
                    if '/cancel' in directory:
                        self.server.cancel_seen.set()
                        self.server.stop.wait(5)
                        return
                    reply('550 Upload refused' if '/error' in directory else '226 Transfer complete')
                elif command == 'QUIT':
                    reply('221 Goodbye')
                    return
                elif command == 'NOOP': reply('200 OK')
                else: reply('502 Command not implemented')
        except (ConnectionError, OSError):
            pass
        finally:
            if passive: passive.close()


@contextlib.contextmanager
def fixture(handler, http=False):
    server_class = http_server if http else ftp_server
    with server_class(('127.0.0.1', 0), handler) as server:
        server.uploads = []
        server.challenges = 0
        server.cancel_seen = threading.Event()
        server.stop = threading.Event()
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield server
        finally:
            server.stop.set()
            server.shutdown()
            thread.join(5)


class http_server(http.server.ThreadingHTTPServer):
    daemon_threads = True


class ftp_server(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--timeout', type=int, default=75)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='Gold publish % ', dir=output))
    image = base64.b64decode('R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7')
    source = run / 'source.html'
    source.write_text('<html><head><title>Publish fixture</title></head><body>'
                      '<h1>Publish preserved heading</h1><p>Preserve <b>bold</b>.</p>'
                      '<img src="pixel.gif"><img src="unselected.gif"></body></html>', encoding='ascii')
    (run / 'pixel.gif').write_bytes(image)
    (run / 'unselected.gif').write_bytes(image)
    (run / 'empty.bin').write_bytes(b'')
    with fixture(HTTP, True) as httpd, fixture(FTP) as ftpd:
        for protocol, server in [('http', httpd), ('ftp', ftpd)]:
            report = run / (protocol + '.txt')
            destination = f'{protocol}://127.0.0.1:{server.server_address[1]}/publish/'
            env = os.environ.copy()
            env['NETSCAPE_PROFILE_DIR'] = str(run / ('profile-' + protocol))
            try:
                result = subprocess.run([args.executable, '--editor-publish-smoke', str(report),
                                         source.as_uri(), destination], env=env,
                                        capture_output=True, text=True, timeout=args.timeout)
            except subprocess.TimeoutExpired as exc:
                raise AssertionError('Editor publish smoke did not finish within its bounded timeout') from exc
            assert report.exists(), ('Missing publish smoke report', result.returncode, result.stderr)
            values = dict(line.split('=', 1) for line in report.read_text().splitlines() if '=' in line)
            assert result.returncode == 0, (result.returncode, values, result.stderr)
            required = ['passed', 'saved', 'published', 'selection', 'invalid_destination',
                        'dirty_retained', 'failed', 'retry', 'cancelled', 'native_dialog',
                        'native_cancel', 'capture', 'credentials']
            assert all(values.get(key) == '1' for key in required), values
            uploads = [(path, body) for path, body in server.uploads if path.startswith('/publish/')]
            assert len(uploads) == 3, [(path, len(body)) for path, body in uploads]
            bodies = dict(uploads)
            assert bodies['/publish/pixel.gif'] == image
            assert bodies['/publish/empty.bin'] == b''
            assert not any('unselected.gif' in path for path, _ in server.uploads)
            html = bodies['/publish/source.html'].decode('latin1')
            parsed = HTMLText()
            parsed.feed(html)
            visible = ' '.join(' '.join(parsed.text).split())
            assert 'Publish preserved heading' in visible and 'Native Gold publish' in visible, visible
            assert 'pixel.gif' in html and '<B>' in html.upper(), html
            # Compare with the actual EDT local serialization byte for byte.
            assert bodies['/publish/source.html'] == Path(str(report) + '.saved.html').read_bytes()
            if protocol == 'http':
                authenticated = dict((path, body) for path, body in server.uploads if path.startswith('/auth/'))
                assert server.challenges >= 1, 'No HTTP authentication challenge was exercised'
                assert set(authenticated) == {'/auth/source.html', '/auth/pixel.gif', '/auth/empty.bin'}, list(authenticated)
                assert authenticated['/auth/source.html'] == bodies['/publish/source.html']
                assert authenticated['/auth/pixel.gif'] == image
                assert authenticated['/auth/empty.bin'] == b''
                secrets = [b'p@ss%+ word', b'p%40ss%25%2B%20word',
                           base64.b64encode(b'gold@user%+:p@ss%+ word')]
                assert not any(secret.decode() in result.stdout + result.stderr for secret in secrets), 'Password leaked into process output'
                profile = run / ('profile-' + protocol)
                assert not any(secret in path.read_bytes() for path in profile.rglob('*') if path.is_file()
                               for secret in secrets), 'Password persisted in profile'
            bitmap = Path(str(report) + '.publish.bmp').read_bytes()
            assert bitmap[:2] == b'BM' and len(set(bitmap[54:])) > 16, 'Missing native Publish dialog capture'
        assert httpd.cancel_seen.is_set(), 'Cancel did not reach the fixture'
        assert ftpd.cancel_seen.is_set(), 'FTP cancel did not reach the fixture'
    print(f'Gold EDT serialization and selected assets published through original HTTP/FTP netlib: {run}')


if __name__ == '__main__':
    main()
