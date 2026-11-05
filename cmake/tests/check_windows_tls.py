"""Real native browser HTTPS integration against an independent OpenSSL peer."""
import argparse
from contextlib import contextmanager
import hashlib
import http.server
import os
from pathlib import Path
import select
import shutil
import socket
import socketserver
import ssl
import struct
import subprocess
import threading
import time
import warnings
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
        if self.mode == 'reset':
            conn.setsockopt(socket.SOL_SOCKET,socket.SO_LINGER,struct.pack('hh',1,0))
            conn.close()
            raise OSError('intentional abortive TCP close')
        if self.mode == 'protocol':
            conn.recv(4096)
            conn.sendall(b'\x16\x03\x03\x00\x2a\x02\x00\x00\x26\x03\x04'+bytes(32)+b'\x00\xc0\x2f\x00')
            conn.close()
            raise OSError('intentional unsupported ServerHello version')
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
        if path in ('/redirect-http','/redirect-unreachable'):
            self.send_response(302)
            self.send_header('Location', self.server.plain_url if path=='/redirect-http' else self.server.failure_url)
            self.send_header('Content-Length','0')
            self.end_headers()
            return
        if path == '/cookies': self.server.cookie_headers.append(self.headers.get('Cookie',''))
        if path == '/image.gif': self.server.image_cookies.append(self.headers.get('Cookie',''))
        if path == '/redirect':
            self.send_response(302)
            self.send_header('Location', f'https://fixture.test:{self.server.server_port}/index')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        body, kind = (DOWNLOAD, 'application/octet-stream') if path == '/download' else (GIF, 'image/gif') if path == '/image.gif' else (BODY, 'text/html')
        if path == '/frames':
            body = ('<html><head><title>TLS browser fixture</title></head><frameset cols="50%,50%"><frame name="plain" src="'+self.server.plain_url+'"><frame name="secure" src="/index"></frameset></html>').encode()
        if path.startswith('/policy-'):
            body = b'<html><head><title>TLS browser fixture</title></head><body><h1>Policy snapshot</h1><p>Active transfer remains valid through policy Save.</p></body></html>'
        self.send_response(200)
        self.send_header('Content-Type', kind)
        if path == '/cookies': self.send_header('Set-Cookie','origin_cookie=good; Path=/')
        self.send_header('Content-Length', str(len(body) + (100 if path == '/truncated' else 0)))
        self.send_header('Connection', 'keep-alive' if path.startswith(('/keepalive','/policy-idle','/policy-ca')) else 'close')
        self.end_headers()
        if path == '/policy-active':
            self.wfile.write(body[:100]);self.wfile.flush()
            time.sleep(.8)
            self.wfile.write(body[100:]);self.wfile.flush()
        elif path == '/cancel':
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
        if isinstance(self.connection, ssl.SSLSocket) and path not in ('/truncated','/keepalive') and not path.startswith(('/policy-idle','/policy-ca')):

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
            for chunk in (b'HTTP/1.0 200 Con', b'nection established\r\nX-Split: yes\r\n'+self.server.headers, b'\r\n'+self.server.tail):
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

def policy(args, output):
    contexts = {}
    for label, version in [('10',ssl.TLSVersion.TLSv1),('11',ssl.TLSVersion.TLSv1_1),('12',ssl.TLSVersion.TLSv1_2)]:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', DeprecationWarning)
            context.minimum_version = context.maximum_version = version
        context.set_ciphers('ECDHE-RSA-AES128-SHA:@SECLEVEL=0' if label != '12' else 'ECDHE-RSA-AES128-GCM-SHA256')
        context.load_cert_chain(output/'browser-leaf.pem',output/'browser-leaf.key')
        contexts[label] = context
    peer = Server(('127.0.0.1',0),Handler)
    peer.mode = ''; peer.errors = []; peer.requests = []; peer.connections = []; peer.records = []
    peer.transports = []; peer.image_cookies = []; peer.ctx = contexts['12']
    count = 0
    with running(peer):
        def run(case, version='12', legacy=None, bundle='browser-root.pem', scenario='', success=True,
                profile_name=None, prepare=True, explicit=False, scheme='https', executable=None, relative=False):
            nonlocal count
            if args.case and args.case != case and not (args.case=='ca-change-restarted' and case=='ca-change-retained'): return None
            peer.ctx = contexts[version] if scheme == 'https' else None
            profile = output/('profile-'+(profile_name or case));profile.mkdir(exist_ok=True)
            if prepare:
                content = '[General]\nca_bundle='+(bundle if relative else str((output/bundle).resolve()) if bundle else '')+'\n'
                if legacy is not None: content += 'allow_legacy_tls='+str(legacy)+'\n'
                content += '[Unrelated]\nKeep=untouched\n'
                (profile/'browser.ini').write_text(content)
            env = os.environ.copy();env['NETSCAPE_PROFILE_DIR'] = str(profile)
            report = output/(case+'.txt');report.unlink(missing_ok=True)
            url = f'{scheme}://localhost:{peer.server_port}/'+(scenario or 'index')
            command = [str((executable or args.executable).resolve()),'--tls-smoke-test',str(report),url]
            if explicit or scenario: command += [str(output/'browser-root.pem') if explicit else '-', '-', scenario or '-']
            before = len(peer.requests)
            result = subprocess.run(command, env=env, cwd=output,capture_output=True,text=True,timeout=30)
            fields = dict(line.split('=',1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
            assert result.returncode == 0 and fields.get('completed') == '1', (case,result.returncode,fields,result.stderr)
            if success:
                assert int(fields['status']) >= 0, (case,fields)
                if scheme == 'https':
                    assert fields.get('tls_authenticated') == '1' and int(fields['tls_version']) == int(version)+759, (case,fields)
                else: assert fields.get('tls_authenticated') == '0', (case,fields)
            else:
                assert int(fields['status']) < 0 and int(fields.get('tls_error','0')) != 0, (case,fields)
                if scenario not in ('policy-active','policy-idle'):
                    assert len(peer.requests)==before, (case,'HTTP leaked before authentication',peer.requests[before:])
            if scenario: assert fields.get('scenario_ok') == '1', (case,fields)
            assert fields.get('ui_responsive') == '1', (case,fields)
            if scenario in ('policy-active','policy-idle'):
                assert fields.get('first_authenticated')=='1' and fields.get('first_version')==str(int(version)+759), (case,fields)
                assert fields.get('requests_completed')=='2', (case,fields)
                assert peer.requests[before:]==['/'+scenario], (case,peer.requests[before:])
            assert '[Unrelated]\nKeep=untouched' in (profile/'browser.ini').read_text(), case
            count += 1
            print('PASS',case,flush=True)
            return fields
        for version in ('10','11'):
            run('tls'+version+'-default',version,success=False)
            run('tls'+version+'-enabled',version,legacy=1)
            run('tls'+version+'-explicit-fixture',version,legacy=1,explicit=True)
            run('tls'+version+'-bad-certificate',version,legacy=1,bundle='wrong.pem',success=False)
            run('tls'+version+'-active-generation',version,legacy=1,scenario='policy-active',success=False)
            run('tls'+version+'-idle-generation',version,legacy=1,scenario='policy-idle',success=False)
            run('tls'+version+'-cancel',version,legacy=1,scenario='policy-cancel')
            run('tls'+version+'-failed-save',version,legacy=1,scenario='policy-failed')
            run('tls'+version+'-invalid-save',version,legacy=1,scenario='policy-invalid')
        run('bad-certificate-default',legacy=0,bundle='wrong.pem',success=False)
        run('bad-certificate-compatibility',legacy=1,bundle='wrong.pem',success=False)
        run('missing-ca-https',bundle='missing.pem',success=False)
        run('missing-ca-http',bundle='missing.pem',scheme='http')
        # A relative persisted path must not resolve against an incidental cwd.
        run('relative-ca-rejected',bundle='browser-root.pem',relative=True,success=False)
        missing_exe = output/'default-ca-missing'/'bin'/'netscape.exe'
        missing_exe.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(args.executable,missing_exe)
        run('missing-default-ca-https',bundle=None,executable=missing_exe,success=False)
        run('missing-default-ca-http',bundle=None,executable=missing_exe,scheme='http')
        before=len(peer.records)
        run('ca-change-retained',legacy=0,scenario='policy-ca',profile_name='ca-restart')
        if not args.case or args.case=='ca-change-retained':
            assert len({port for path,port in peer.records[before:] if path=='/policy-ca'})==2, peer.records[before:]
        run('ca-change-restarted',success=False,profile_name='ca-restart',prepare=False)
    assert not peer.errors,peer.errors
    print(f'All {count} policy TLS cases passed')

def errors(args, output):
    """Assert displayed document state from independent native network results."""
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    ctx.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256')
    ctx.load_cert_chain(output/'browser-leaf.pem',output/'browser-leaf.key')
    def peer(context):
        server = Server(('127.0.0.1',0),Handler)
        server.ctx=context;server.mode='';server.requests=[];server.connections=[]
        server.records=[];server.transports=[];server.errors=[];server.image_cookies=[]
        return server
    server, plain = peer(ctx), peer(None)
    server.plain_url=f'http://localhost:{plain.server_port}/index'
    proxy=Proxy(('127.0.0.1',0),Relay)
    proxy.target=server.server_port;proxy.requests=[];proxy.reject=False
    proxy.tail=b'';proxy.headers=b'';proxy.errors=[]
    count=0
    with running(server), running(plain), running(proxy):
        def run(case, error=0, phrase='TLS 1.2', ca='browser-root.pem', path='/index',
                scenario='ui', host='localhost', proxied=False, url=None, profile_ca=None):
            nonlocal count
            if args.case and args.case!=case: return
            profile=output/('profile-'+case);profile.mkdir(exist_ok=True)
            if profile_ca is not None:
                (profile/'browser.ini').write_text('[General]\nca_bundle='+profile_ca+'\n')
                ca='-'
            env=os.environ.copy();env['NETSCAPE_PROFILE_DIR']=str(profile)
            report=output/(case+'.txt');report.unlink(missing_ok=True)
            command=[str(args.executable.resolve()),'--tls-smoke-test',str(report),
                url or f'https://{host}:{server.server_port}{path}',ca,
                f'127.0.0.1:{proxy.server_address[1]}' if proxied else '-',scenario]
            before=len(server.requests)
            result=subprocess.run(command,env=env,cwd=output,capture_output=True,text=True,timeout=35)
            fields=dict(line.split('=',1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
            assert result.returncode==0 and fields.get('completed')=='1', (case,result.returncode,fields,result.stderr)
            assert fields.get('display_error')==str(error), (case,'wrong displayed category',fields)
            assert phrase.lower() in fields.get('document_info','').lower(), (case,'missing diagnostic',fields)
            assert 'invalid-ca-override' not in fields.get('document_info',''), (case,fields)
            if url is not None:
                assert int(fields['status'])<0 and fields.get('display_authenticated')=='0' and fields.get('tls_error')=='0', (case,fields)
            if scenario=='ui-cache' or scenario.startswith('ui-history:'):
                assert fields.get('rendered')=='1', (case,fields)
            if error:
                assert fields.get('display_authenticated')=='0' and 'Authenticated connection' not in fields['document_info'], (case,fields)
                if error not in (14,15): assert len(server.requests)==before, (case,'request leaked before authentication',server.requests[before:])
            if scenario.startswith('ui-') or scenario=='cancel':
                assert fields.get('scenario_ok')=='1', (case,fields,result.stderr)
            if scenario=='ui' and not error and url is None:
                assert fields.get('display_authenticated')=='1' and 'ECDHE_RSA_WITH_AES_128_GCM_SHA256' in fields['document_info'], (case,fields)
                assert fields.get('certificate_null')=='1' and 'TLS 1.2' in fields.get('key_cipher',''), (case,fields)
            assert fields.get('capture_ok')=='1', (case,fields)
            count+=1;print('PASS',case,flush=True)
        run('authenticated-details')
        run('unknown-root',11,'not trusted',ca='wrong.pem')
        run('wrong-hostname',9,'hostname',host='wrong.test',proxied=True)
        ctx.load_cert_chain(output/'browser-expired.pem',output/'browser-leaf.key')
        run('invalid-date',10,'expired')
        ctx.load_cert_chain(output/'browser-leaf.pem',output/'browser-leaf.key')
        run('missing-ca',13,'Cannot open CA bundle',ca='missing.pem')
        (output/'malformed.pem').write_text('not a certificate bundle')
        run('malformed-ca',13,'Incomplete or trailing CA bundle',ca='malformed.pem')
        run('invalid-persisted-ca',13,'CA bundle configuration',profile_ca='relative.pem')
        server.mode='protocol'
        run('protocol-failure',7,'protocol')
        server.mode='timeout'
        run('handshake-failure',8,'handshake')
        server.mode=''
        run('truncated-transfer',14,'truncated',path='/truncated')
        run('canceled-transfer',0,'TLS 1.2',path='/cancel',scenario='cancel')
        run('redirect-final-location',0,'Location: http://localhost:',path='/redirect-http',scenario='ui-redirect')
        run('https-to-http',0,'No authenticated',scenario=f'ui-http:{plain.server_port}')
        run('https-to-failed-https',0,'No authenticated',scenario='ui-fail')
        run('history-no-stale-auth',0,'No authenticated',scenario=f'ui-history:{plain.server_port}')
        run('cache-no-live-claim',0,'No authenticated',scenario='ui-cache')
        run('window-isolation',0,'TLS 1.2',scenario=f'ui-windows:{plain.server_port}')
        run('frame-isolation',0,'TLS 1.2',scenario=f'ui-frames:{plain.server_port}',path='/frames')
        run('pre-attachment-failure-preserved',0,'Netscape is unable to locate the server:',url='https://localhost:25/unreachable')
        run('dns-failure-preserved',0,'Netscape is unable to locate the server:',url='https://netscape-tls-fixture.invalid/index')
        plain.mode='reset'
        run('tcp-reset-preserved',0,'A network error occurred while Netscape',url=f'http://localhost:{plain.server_port}/reset')
        plain.mode=''
        proxy.reject=True
        run('proxy-failure-preserved',8,'proxy',proxied=True)
    assert not (server.errors or plain.errors or proxy.errors), (server.errors,plain.errors,proxy.errors)
    print(f'All {count} displayed TLS error cases passed')


def installed(args, output):
    """Exercise an installed package, with all mutations confined to copies."""
    prefix = args.executable.resolve().parent.parent
    assert args.executable.is_file(), args.executable
    assert not output.is_relative_to(prefix), 'test output must be outside installed prefix'
    bundle = prefix/'share/netscape/ca/cacert.pem'
    original = bundle.read_bytes()
    assert hashlib.sha256(original).hexdigest() == '796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe', 'installed CA provenance mismatch'
    assert original.count(b'-----BEGIN CERTIFICATE-----') == 118
    for notice in ('CA-bundle-provenance.md','LICENSE-MPL-2.0.txt',
                   'BearSSL-provenance.md','licenses/BearSSL-MIT.txt',
                   'Windows-source-provenance.md','licenses/WinFE-NPL-1.0'):
        assert (prefix/'share/doc/netscape'/notice).is_file(), ('missing installed notice',notice)
    assert hashlib.sha256((prefix/'share/doc/netscape/licenses/BearSSL-MIT.txt').read_bytes()).hexdigest() == '771bf18c8633ea69ec9b07d2e604c4b99b7bef41b0f5ce6385d24df4448f61ca', 'installed BearSSL license mismatch'
    assert '7bea48e5e850ab4cafbe68d3765cdaba13a86d6f' in (prefix/'share/doc/netscape/BearSSL-provenance.md').read_text(), 'installed BearSSL pin missing'
    print('PASS installed-bundle-and-notices',flush=True)
    # A CWD containing a trusted fixture must never become the default trust store.
    cwd = output/'unrelated cwd 100%';cwd.mkdir(exist_ok=True)
    for relative in ('cacert.pem','security/ca/cacert.pem','share/netscape/ca/cacert.pem'):
        poison=cwd/relative;poison.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(output/'browser-root.pem',poison)
    ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version=ctx.maximum_version=ssl.TLSVersion.TLSv1_2
    ctx.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256')
    ctx.load_cert_chain(output/'browser-leaf.pem',output/'browser-leaf.key')
    peer=Server(('127.0.0.1',0),Handler)
    peer.ctx=ctx;peer.mode='';peer.requests=[];peer.connections=[]
    peer.records=[];peer.transports=[];peer.errors=[];peer.image_cookies=[]
    # Copy only the Runtime package, not build-tree helpers or fixture secrets.
    copy=output/'relocated package 100%'
    assert copy.resolve().is_relative_to(output), 'test copy must stay inside output directory'
    assert not copy.resolve().is_relative_to(prefix), 'test copy must be outside installed prefix'
    shutil.copytree(prefix,copy,dirs_exist_ok=True)
    copied_bundle=copy/'share/netscape/ca/cacert.pem'
    executable=copy/'bin'/args.executable.name
    count=0
    with running(peer):
        def run(case, error=0, override=None, omitted=False, scheme='https', exe=executable):
            nonlocal count
            if args.case and args.case!=case: return
            profile=output/('profile-'+case);profile.mkdir(exist_ok=True)
            (profile/'browser.ini').write_text('[General]\nca_bundle='+str(override or '')+'\n')
            env=os.environ.copy();env['NETSCAPE_PROFILE_DIR']=str(profile)
            report=output/(case+'.txt');report.unlink(missing_ok=True)
            command=[str(exe), '--tls-smoke-test', str(report),
                     f'{scheme}://localhost:{peer.server_port}/index']
            if not omitted: command+=['-','-','ui']
            before=len(peer.requests)
            result=subprocess.run(command,env=env,cwd=cwd,capture_output=True,text=True,timeout=35)
            fields=dict(line.split('=',1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
            assert result.returncode==0 and fields.get('completed')=='1', (case,result.returncode,fields,result.stderr)
            assert fields.get('tls_error')==str(error), (case,fields)
            if error:
                assert int(fields['status'])<0 and fields.get('tls_authenticated')=='0', (case,fields)
                assert len(peer.requests)==before, (case,'HTTP before authentication',peer.requests[before:])
            else:
                assert int(fields['status'])>=0 and fields.get('rendered')=='1', (case,fields)
                assert fields.get('tls_authenticated')==('1' if scheme=='https' else '0'), (case,fields)
                if scheme=='https': assert fields.get('tls_version')=='771', (case,fields)
            if not omitted: assert fields.get('capture_ok')=='1', (case,fields)
            count+=1;print('PASS',case,flush=True)
        run('shipped-default-loads',11,exe=args.executable.resolve())
        shutil.copy2(output/'browser-root.pem',copied_bundle)
        run('relocated-bundle-discovered')
        run('omitted-ca-keeps-startup',omitted=True)
        override=output/'explicit CA 100%.pem';shutil.copy2(output/'browser-root.pem',override)
        copied_bundle.write_bytes(original)
        run('profile-override-replaces-bundle',override=override)
        copied_bundle.unlink()
        run('missing-default-no-fallback',13)
        run('override-with-missing-default',override=override)
        copied_bundle.write_text('malformed certificate bundle')
        run('malformed-default-no-fallback',13)
        run('override-with-malformed-default',override=override)
        shutil.copy2(output/'browser-root.pem',copied_bundle)
        run('missing-override-no-fallback',13,override=output/'missing CA 100%.pem')
        malformed=output/'malformed CA 100%.pem';malformed.write_text('not PEM')
        run('malformed-override-no-fallback',13,override=malformed)
        run('wrong-override-no-merge',11,override=output/'wrong.pem')
        run('relative-override-no-fallback',13,override='cacert.pem')
        copied_bundle.unlink()
        peer.ctx=None
        run('http-with-missing-default',scheme='http')
    assert not peer.errors,peer.errors
    assert bundle.read_bytes()==original, 'installed package was mutated'
    print(f'All {count} installed TLS cases passed')


def disabled(args, output):
    """A real TLS-disabled browser must reject HTTPS without opening a peer."""
    profile=output/'profile-disabled';profile.mkdir(exist_ok=True)
    env=os.environ.copy();env['NETSCAPE_PROFILE_DIR']=str(profile)
    with socket.socket() as listener:
        listener.bind(('127.0.0.1',0));listener.listen();listener.setblocking(False)
        report=output/'disabled-https.txt';report.unlink(missing_ok=True)
        result=subprocess.run([str(args.executable.resolve()),'--smoke-test',str(report),
            f'https://localhost:{listener.getsockname()[1]}/index'],env=env,cwd=output,
            capture_output=True,text=True,timeout=20)
        fields=dict(line.split('=',1) for line in report.read_text().splitlines() if '=' in line)
        assert result.returncode==4 and fields.get('completed')=='1' and int(fields['status'])<0, (result.returncode,fields,result.stderr)
        assert not select.select([listener],[],[],.2)[0], 'TLS-disabled browser connected to HTTPS peer'
    print('PASS disabled-https-no-connection-no-plaintext',flush=True)
    peer=Server(('127.0.0.1',0),Handler)
    peer.ctx=None;peer.mode='';peer.requests=[];peer.connections=[];peer.records=[];peer.transports=[];peer.errors=[];peer.image_cookies=[]
    with running(peer):
        report=output/'disabled-http.txt';report.unlink(missing_ok=True)
        result=subprocess.run([str(args.executable.resolve()),'--smoke-test',str(report),
            f'http://localhost:{peer.server_port}/index'],env=env,cwd=output,capture_output=True,text=True,timeout=20)
        fields=dict(line.split('=',1) for line in report.read_text().splitlines() if '=' in line)
        assert fields.get('completed')=='1' and int(fields['status'])>=0 and int(fields['text_runs'])>0 and '/index' in peer.requests, (result.returncode,fields,peer.requests)
    assert not peer.errors,peer.errors
    print('PASS disabled-http-still-renders',flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=['browser','policy','errors','installed','disabled'], default='browser')
    parser.add_argument('--red', action='store_true')
    parser.add_argument('--case', default='')
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    if args.mode == 'disabled':
        return disabled(args, output)
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
    expired = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(leaf_key.public_key())
        .serial_number(x509.random_serial_number()).not_valid_before(dt.datetime(2010,1,1,tzinfo=dt.timezone.utc))
        .not_valid_after(dt.datetime(2011,1,1,tzinfo=dt.timezone.utc))
        .add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]),False)
        .add_extension(x509.BasicConstraints(False,None),True).sign(root_key,hashes.SHA256()))
    (output/'browser-expired.pem').write_bytes(expired.public_bytes(serialization.Encoding.PEM))
    if args.mode == 'installed':
        return installed(args, output)
    if args.mode == 'errors':
        return errors(args, output)
    if args.mode == 'policy':
        return policy(args, output)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    ctx.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256'); ctx.load_cert_chain(output/'browser-leaf.pem',output/'browser-leaf.key')
    names = []; ctx.set_servername_callback(lambda sock, name, context: names.append(name))
    server = Server(('127.0.0.1',0), Handler); server.ctx = ctx; server.requests = []; server.post_hashes = []; server.connections = []; server.records = []; server.transports = []; server.errors = []; server.mode = ''
    proxy = Proxy(('127.0.0.1',0),Relay); proxy.target = server.server_port; proxy.requests = []; proxy.reject = False; proxy.tail = b''; proxy.errors = []; proxy.headers = b''
    env = os.environ.copy()
    plain = Server(('127.0.0.1',0),Handler);plain.ctx=None;plain.requests=[];plain.connections=[];plain.records=[];plain.transports=[];plain.errors=[];plain.mode=''
    server.plain_url=f'http://localhost:{plain.server_port}/index'
    # Netlib rejects port 25 before DNS/TCP/TLS, independent of listener timing.
    server.failure_url='https://localhost:25/unreachable'
    server.cookie_headers=[];server.image_cookies=[]
    plain.cookie_headers=[];plain.image_cookies=[]
    with running(server), running(proxy), running(plain):
        def run(case, path='/index', ca='browser-root.pem', proxied=False, success=True, extra='', host='localhost', transport='tls'):
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
                if transport=='tls':
                    assert fields.get('tls_authenticated') == '1' and fields.get('tls_version') == '771', (case,fields)
                else:
                    assert all(fields.get(key)=='0' for key in ('tls_authenticated','tls_version','tls_cipher','tls_error','security_on')), (case,fields)
                assert int(fields['status']) >= 0, (case,fields)
                assert 'TLS browser fixture' in fields.get('title',''), (case,fields)
                assert fields.get('ui_responsive') == '1', (case,fields)
                if extra == 'download': assert fields.get('body_sha256') == hashlib.sha256(DOWNLOAD).hexdigest(), fields
            else:
                assert int(fields['status']) < 0, (case,fields)
                if transport=='unattached':
                    assert all(fields.get(key)=='0' for key in ('tls_authenticated','tls_version','tls_cipher','tls_error','security_on')), (case,fields)
                else: assert int(fields.get('tls_error','0')) != 0, (case,fields)
            if extra in ('connect-isolation','connect-isolation-fail'):
                assert fields.get('proxy_cookie')=='0' and fields.get('refresh_present')=='0', (case,fields)
                if success: assert fields.get('origin_cookie')=='1', (case,fields)
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
        proxy.headers=b'Set-Cookie: proxy_injected=evil; Path=/\r\nRefresh: 0; url=/proxy-refresh\r\n'
        isolated=run('connect-origin-header-isolation',path='/cookies',proxied=True,extra='connect-isolation')
        if isolated:
            assert not any('proxy_injected=evil' in h for h in server.cookie_headers+server.image_cookies), (server.cookie_headers,server.image_cookies)
            assert any('origin_cookie=good' in h for h in server.image_cookies), server.image_cookies
        before=len(server.requests)
        run('connect-origin-header-isolation-failed-auth',path='/cookies',proxied=True,ca='wrong.pem',success=False,extra='connect-isolation-fail')
        assert len(server.requests)==before, 'origin request leaked after failed authentication'
        proxy.headers=b''
        run('redirect-http-metadata',path='/redirect-http',extra='redirect-metadata',transport='plain')
        run('redirect-before-attach-metadata',path='/redirect-unreachable',success=False,extra='redirect-metadata',transport='unattached')
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
