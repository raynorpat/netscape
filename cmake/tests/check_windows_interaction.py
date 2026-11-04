"""Exercise native mouse gestures through the original LO engine, on loopback."""
import argparse
import base64
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests.append(self.path)
        if self.path == '/pixel.gif':
            body = base64.b64decode('R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7')
        elif self.path == '/divider-left':
            body = b'<html><body><form><input name="retain" value="default"></form><p>Divider left</p></body></html>'
        elif self.path == '/dividers':
            body = (b'<html><frameset cols="40%,60%"><frame name="dividerLeft" src="/divider-left">'
                    b'<frameset rows="50%,50%"><frame name="dividerUpper" src="/links">'
                    b'<frame name="dividerLower" src="/selection"></frameset></frameset></html>')
        elif self.path == '/noresize':
            body = b'<html><frameset cols="40%,60%"><frame noresize src="/divider-left"><frame src="/blank"></frameset></html>'
        elif self.path == '/links':
            body = (b'<html><body><p><a href="/opened?x=1&amp;y=two" target="linkTarget">Targeted text link</a></p>'
                    b'<p><a href="/image-link"><img src="/pixel.gif" width="40" height="30"></a></p>'
                    b'<p><img src="/pixel.gif" width="40" height="30" usemap="#map"></p>'
                    b'<map name="map"><area shape="rect" coords="0,0,39,29" href="/mapped" target="linkTarget"></map>'
                    b'<p><a href="/server-map"><img src="/pixel.gif" width="40" height="30" ismap></a></p></body></html>')
        elif self.path == '/linkframes':
            body = b'<html><frameset cols="50%,50%"><frame name="linkSource" src="/links"><frame name="linkTarget" src="/blank"></frameset></html>'
        elif self.path == '/selectionframes':
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
        self.send_header('Content-Type', 'image/gif' if self.path == '/pixel.gif' else 'text/html')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--mode', choices=['selection', 'links', 'dividers'], default='selection')
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
        command = {'selection': '--interaction-smoke', 'links': '--links-smoke', 'dividers': '--dividers-smoke'}[args.mode]
        result = subprocess.run([args.executable, command, str(report),
                                 'http://127.0.0.1:%d/' % server.server_port],
                                env=env, capture_output=True, timeout=75)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
    (run / 'process.txt').write_bytes(result.stdout + result.stderr)
    (run / 'requests.json').write_text(json.dumps(server.requests, indent=2))
    values = dict(line.split('=', 1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
    print('Report:', report, values)
    assert result.returncode == 0, (result.returncode, values)
    if args.mode == 'dividers':
        required = ['passed', 'vertical', 'horizontal', 'bounds', 'cancel', 'noresize',
                    'history', 'form', 'navigation', 'tracking', 'cursor', 'escape', 'resize']
        assert all(values.get(key) == '1' for key in required), values
        # Moving dividers must reuse the current document. Later independent
        # framesets may legitimately fetch their own copy of the same URL.
        assert server.requests[:server.requests.index('/blank')].count('/divider-left') == 1, server.requests
        assert Path(str(report) + '.frames.bmp').read_bytes()[:2] == b'BM'
        print('Native frame divider gestures, bounds and retained state verified')
        return
    if args.mode == 'links':
        assert all(value == '1' for value in values.values()), values
        base = 'http://127.0.0.1:%d/' % server.server_port
        for name, suffix in [('copylink', 'opened?x=1&y=two'), ('copyimage', 'pixel.gif'),
                             ('copymap', 'mapped'), ('copyservermap', 'server-map?5,6'),
                             ('busycopy', 'busy-copy')]:
            assert Path(str(report) + '.' + name).read_text(encoding='latin1') == base + suffix, name
        assert server.requests.count('/opened?x=1&y=two') >= 1, server.requests
        print('Native link menus, image maps, hover and target routing verified')
        return
    assert all(values.get(key) == '1' for key in ['passed', 'drag_link', 'capture', 'capture_cancel',
                                                'navigation_cancel', 'autoscroll', 'capture_released',
                                                'clicked', 'frame_selection', 'capture_link_cancel',
                                                'navigation_link_cancel']), values
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
