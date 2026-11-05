"""Real native browser HTTPS integration against an independent OpenSSL peer."""
import argparse
from contextlib import contextmanager
import hashlib
import http.server
import os
from pathlib import Path
import select
import socket
import socketserver
import ssl
import subprocess
import threading
import time
from urllib.parse import urlsplit
import tls_fixture
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization

BODY = b'<html><head><title>TLS browser fixture</title></head><body><h1>Authenticated document</h1><img src="/image.gif"></body></html>'
DOWNLOAD = bytes((i * 19 + 7) & 255 for i in range(262177))
GIF = bytes.fromhex('47494638396101000100800000000000ffffff21f90401000000002c00000000010001000002024401003b')

class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, request, address):
        import sys
        error = sys.exception()
        if not isinstance(error,OSError):
            self.errors.append(repr(error))
            super().handle_error(request,address)
    def get_request(self):
        conn, address = super().get_request()
        conn.settimeout(12)
        if self.mode == 'timeout':
            time.sleep(11)
            conn.close()
            raise OSError('intentional stalled handshake')
        if self.mode.startswith('alert'):
            try: self.alert(conn)
            finally: conn.close()
            raise OSError('intentional protected fatal alert')
        if self.ctx and not (self.mode=='dual' and conn.recv(1,socket.MSG_PEEK)!=b'\x16'):
            try: conn = self.ctx.wrap_socket(conn, server_side=True)
            except ssl.SSLError:
                conn.close()
                raise OSError('TLS rejected')
        return conn, address

    def alert(self, conn):
        incoming, outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
        tls = self.ctx.wrap_bio(incoming,outgoing,server_side=True)
        def flush():
            while outgoing.pending: conn.sendall(outgoing.read())
        def operation(call):
            while True:
                try:
                    result=call();flush();return result
                except ssl.SSLWantReadError:
                    flush();data=conn.recv(65536)
                    if not data: raise OSError('browser canceled')
                    incoming.write(data)
        operation(tls.do_handshake)
        request=b''
        while b'\r\n\r\n' not in request: request+=operation(lambda:tls.read(65536))
        self.requests.append('/alert')
        body=b'<html><head><title>TLS browser fixture</title></head><body>Protected data before fatal alert</body></html>'
        header=b'HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n'
        if self.mode=='alert-length': header+=f'Content-Length: {len(body)}\r\n'.encode()
        header+=b'Connection: close\r\n\r\n'
        tls.write(header+body)
        # Ask OpenSSL to construct a protected fatal bad_record_mac alert. Put
        # it in the same native write as the response, after application data.
        incoming.write(b'\x17\x03\x03\x00\x20'+bytes(32))
        try: tls.read(1)
        except ssl.SSLError: pass
        flush()

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.0'
    def do_GET(self):
        path = urlsplit(self.path).path
        self.server.requests.append(path)
        self.server.connections.append(self.connection.getpeername()[1])
        self.server.records.append((path,self.connection.getpeername()[1]))
        self.server.transports.append((path,isinstance(self.connection,ssl.SSLSocket)))
        if path=='/headers': self.server.large_header=self.headers.get('X-Large','')
        if path == '/redirect':
            self.send_response(302)
            self.send_header('Location', f'https://fixture.test:{self.server.server_port}/index')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        body, kind = (DOWNLOAD, 'application/octet-stream') if path == '/download' else (GIF, 'image/gif') if path == '/image.gif' else (BODY, 'text/html')
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(body) + (100 if path == '/truncated' else 0)))
        self.send_header('Connection', 'keep-alive' if path.startswith('/keepalive') else 'close')
        self.end_headers()
        if path == '/cancel':
            self.wfile.write(body[:40]);self.wfile.flush()
            time.sleep(.4)
            try: self.wfile.write(body[40:]);self.wfile.flush()
            except OSError: return
        else:
            self.wfile.write(body)
            self.wfile.flush()
        if path == '/keepalive-stale':
            time.sleep(.08)
            self.connection.shutdown(socket.SHUT_RDWR)
            self.close_connection = True
            return
        if isinstance(self.connection, ssl.SSLSocket) and path not in ('/truncated','/keepalive'):

            try: self.connection.unwrap()
            except (OSError, ssl.SSLError): pass
    def do_POST(self):
        self.server.requests.append('/post')
        size = int(self.headers['Content-Length'])
        body = bytearray()
        time.sleep(.1)  # force queued TLS output before reading the producer
        while len(body) < size:
            block = self.rfile.read(min(2048,size-len(body)))
            if not block: break
            body.extend(block)
        self.server.post_hashes.append(hashlib.sha256(body).hexdigest())
        self.send_response(200)
        self.send_header('Content-Type','text/html')
        self.send_header('Content-Length',str(len(BODY)))
        self.end_headers()
        self.wfile.write(BODY);self.wfile.flush()
        try: self.connection.unwrap()
        except (OSError,ssl.SSLError): pass
    def log_message(self, *args): pass

class Proxy(socketserver.ThreadingTCPServer):
    def handle_error(self, request, address):
        # Authentication failures/cancellation intentionally reset relay sockets.
        import sys
        if not isinstance(sys.exception(),OSError):
            self.errors.append(repr(sys.exception()))
            super().handle_error(request,address)
    allow_reuse_address = True
    daemon_threads = True
class Relay(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(15)
        data = b''
        while b'\r\n\r\n' not in data and b'\n\n' not in data:
            block = self.request.recv(1)
            if not block: return
            data += block
        self.server.requests.append(data.splitlines()[0].decode('ascii'))
        if self.server.reject:
            response = b'<html><head><title>TLS browser fixture</title></head><body>Proxy error</body></html>\n' if self.server.reject=='malformed' else b'HTTP/1.0 403 Forbidden\r\nContent-Length: 0\r\n\r\n'
            self.request.sendall(response)
            return
        upstream = socket.create_connection(('127.0.0.1', self.server.target), timeout=12)
        with upstream:
            for chunk in (b'HTTP/1.0 200 Con', b'nection established\r\nX-Split: yes\r\n', b'\r\n'+self.server.tail):
                self.request.sendall(chunk)
                time.sleep(.015)
            peers = [self.request, upstream]
            while True:
                ready, _, _ = select.select(peers, [], [], 12)
                if not ready: return
                for peer in ready:
                    block = peer.recv(4096)
                    if not block: return
                    (upstream if peer is self.request else self.request).sendall(block)

@contextmanager
def running(server):
    thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .02}, daemon=True)
    thread.start()
    try: yield server
    finally:
        server.shutdown()
        server.server_close()
        thread.join(2)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=['browser'], default='browser')
    parser.add_argument('--red', action='store_true')
    parser.add_argument('--case', default='')
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    tls_fixture.material(output)
    # Add localhost to the fixture leaf without changing the shared engine peer.
    from cryptography.hazmat.primitives.asymmetric import rsa
    root_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    import datetime as dt
    from cryptography.x509.oid import NameOID
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Browser fixture CA')])
    root = tls_fixture.certificate('Browser fixture CA', root_key.public_key(), name, root_key, True, 1)
    leaf_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    leaf = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(leaf_key.public_key())
        .serial_number(x509.random_serial_number()).not_valid_before(dt.datetime(2020,1,1,tzinfo=dt.timezone.utc))
        .not_valid_after(dt.datetime(2045,1,1,tzinfo=dt.timezone.utc))
        .add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost'), x509.DNSName('fixture.test')]),False)
        .add_extension(x509.BasicConstraints(False,None),True).sign(root_key,hashes.SHA256()))
    (output/'browser-root.pem').write_bytes(root.public_bytes(serialization.Encoding.PEM))
    (output/'browser-leaf.pem').write_bytes(leaf.public_bytes(serialization.Encoding.PEM))
    (output/'browser-leaf.key').write_bytes(leaf_key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    ctx.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256'); ctx.load_cert_chain(output/'browser-leaf.pem',output/'browser-leaf.key')
    names = []; ctx.set_servername_callback(lambda sock, name, context: names.append(name))
    server = Server(('127.0.0.1',0), Handler); server.ctx = ctx; server.requests = []; server.post_hashes = []; server.connections = []; server.records = []; server.transports = []; server.errors = []; server.mode = ''
    proxy = Proxy(('127.0.0.1',0),Relay); proxy.target = server.server_port; proxy.requests = []; proxy.reject = False; proxy.tail = b''; proxy.errors = []
    env = os.environ.copy()
    plain = Server(('127.0.0.1',0),Handler);plain.ctx=None;plain.requests=[];plain.connections=[];plain.records=[];plain.transports=[];plain.errors=[];plain.mode=''
    with running(server), running(proxy), running(plain):
        def run(case, path='/index', ca='browser-root.pem', proxied=False, success=True, extra='', host='localhost'):
            if args.case and args.case != case: return None
            profile = output/('profile-'+case); profile.mkdir(exist_ok=True)
            env['NETSCAPE_PROFILE_DIR'] = str(profile)
            report = output/(case+'.txt'); report.unlink(missing_ok=True)
            url = f'https://{host}:{server.server_port}{path}'
            command = [str(args.executable.resolve()), '--smoke-test' if args.red else '--tls-smoke-test', report.name, url]
            if not args.red: command += [ca, f'127.0.0.1:{proxy.server_address[1]}' if proxied else '-', extra or '-']
            result = subprocess.run(command, env=env, cwd=output,capture_output=True,text=True,timeout=30)
            fields = dict(line.split('=',1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
            assert fields.get('completed') == '1', (case,result.returncode,fields,result.stderr)
            if success:
                assert fields.get('tls_authenticated') == '1' and fields.get('tls_version') == '771', (case,fields)
                assert int(fields['status']) >= 0, (case,fields)
                assert 'TLS browser fixture' in fields.get('title',''), (case,fields)
                assert fields.get('ui_responsive') == '1', (case,fields)
                if extra == 'download': assert fields.get('body_sha256') == hashlib.sha256(DOWNLOAD).hexdigest(), fields
            else:
                assert int(fields['status']) < 0 and int(fields.get('tls_error','0')) != 0, (case,fields)
            if extra in ('reuse','policy','cancel','scheme') or extra.startswith('mixed:'):
                assert fields.get('scenario_ok') == '1', (case,fields)
            print('PASS',case,flush=True)
            return fields
        document = run('document-subresource')
        if document: assert document.get('rendered')=='1' and document.get('images_rendered')=='1', document
        assert args.case or '/image.gif' in server.requests and names[0] == 'localhost', (server.requests,names)
        run('large-post-memory',path='/post',extra='post')
        run('large-post-file',path='/post',extra='post-file')
        if server.post_hashes: assert all(h == hashlib.sha256(DOWNLOAD).hexdigest() for h in server.post_hashes), server.post_hashes
        run('partial-request-headers',path='/headers',extra='headers')
        if not args.case or args.case=='partial-request-headers': assert server.large_header=='a'*40000
        run('download',extra='download')
        run('mixed-concurrent',extra=f'mixed:{plain.server_port}')
        if not args.case or args.case=='mixed-concurrent': assert plain.requests, 'ordinary HTTP made no progress'
        run('cancel-descriptor-reuse',path='/cancel',extra='cancel')
        before = len(server.records)
        run('keepalive',path='/keepalive',extra='reuse')
        if not args.case or args.case=='keepalive': assert len({port for path,port in server.records[before:] if path=='/keepalive'})==1, server.records[before:]
        before = len(server.records)
        run('stale-keepalive',path='/keepalive-stale',extra='reuse')
        if not args.case or args.case=='stale-keepalive': assert len({port for path,port in server.records[before:] if path.startswith('/keepalive')})==2, server.records[before:]
        before = len(server.records)
        run('policy-retires-reuse',path='/keepalive',extra='policy')
        if not args.case or args.case=='policy-retires-reuse': assert len({port for path,port in server.records[before:] if path.startswith('/keepalive')})==2, server.records[before:]
        server.mode='dual'
        before=len(server.transports)
        run('scheme-origin-isolation',path='/keepalive',extra='scheme')
        if not args.case or args.case=='scheme-origin-isolation':
            assert [secure for path,secure in server.transports[before:] if path=='/keepalive']==[True,False,True], server.transports[before:]
        server.mode=''
        run('connect-split',proxied=True)
        proxy.tail = b'\x15\x03\x03\x00\x02\x02\x28'
        before = len(server.requests)
        run('connect-buffered-alert',proxied=True,success=False)
        assert len(server.requests)==before, 'CONNECT TLS tail was discarded'
        proxy.tail = b''
        before = len(server.requests)
        run('hostname-failure',host='wrong.test',proxied=True,success=False)
        assert len(server.requests)==before, 'HTTP request leaked after hostname failure'
        run('ip-rejected',host='127.0.0.1',success=False)
        assert len(server.requests)==before, 'HTTP request leaked for an IP literal'
        run('redirect-sni',path='/redirect',proxied=True)
        assert args.case or 'fixture.test' in names, names
        before = len(server.requests)
        run('untrusted',ca='wrong.pem',success=False)
        assert len(server.requests) == before, 'HTTP request leaked before authentication'
        proxy.reject = True
        run('connect-rejected',proxied=True,success=False)
        proxy.reject = 'malformed'
        run('connect-malformed',proxied=True,success=False)
        proxy.reject = False
        run('truncated',path='/truncated',success=False)
        server.mode='alert-length'
        run('fatal-alert-after-content-length',success=False)
        server.mode='alert-eof'
        run('fatal-alert-after-data',success=False)
        server.mode = 'timeout'
        run('handshake-timeout',success=False)
    assert not (server.errors or plain.errors or proxy.errors), (server.errors,plain.errors,proxy.errors)
    print('All browser TLS cases passed')
if __name__ == '__main__': main()
