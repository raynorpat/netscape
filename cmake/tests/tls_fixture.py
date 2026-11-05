"""Independent local TLS peer. Weak legacy settings exist only in this fixture.

Requires Python cryptography, Python ssl/OpenSSL and an OpenSSL fixture CLI;
none is linked or installed with Netscape. Certificates/keys are temporary.
"""
import argparse
import datetime as dt
import socket
import ssl
import subprocess
import tempfile
import threading
import warnings
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, ec
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID

PAYLOAD = bytes((i * 13 + 5) & 255 for i in range(32791))
SUITES = {
    'rsa128': 'ECDHE-RSA-AES128-GCM-SHA256',
    'rsa256': 'ECDHE-RSA-AES256-GCM-SHA384',
    'rsachacha': 'ECDHE-RSA-CHACHA20-POLY1305',
    'ec128': 'ECDHE-ECDSA-AES128-GCM-SHA256',
    'ec256': 'ECDHE-ECDSA-AES256-GCM-SHA384',
    'ecchacha': 'ECDHE-ECDSA-CHACHA20-POLY1305',
}

def key():
    return rsa.generate_private_key(public_exponent=65537, key_size=2048)

def certificate(subject, public, issuer, signing_key, ca=False, path=None):
    builder = (x509.CertificateBuilder().subject_name(x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, subject)]))
        .issuer_name(issuer).public_key(public).serial_number(x509.random_serial_number())
        .not_valid_before(dt.datetime(2020, 1, 1, tzinfo=dt.timezone.utc))
        .not_valid_after(dt.datetime(2045, 1, 1, tzinfo=dt.timezone.utc))
        .add_extension(x509.BasicConstraints(ca, path), critical=True)
        .add_extension(x509.KeyUsage(True, False, False, False, False, ca, ca,
                                     False, False), critical=True))
    if not ca:
        builder = (builder.add_extension(x509.SubjectAlternativeName([
            x509.DNSName('fixture.test')]), critical=False)
            .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), False))
    return builder.sign(signing_key, hashes.SHA256())

def material(folder):
    root_key, inter_key = key(), key()
    root_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Fixture root')])
    root = certificate('Fixture root', root_key.public_key(), root_name, root_key, True, 1)
    inter = certificate('Fixture intermediate', inter_key.public_key(), root.subject, root_key, True, 0)
    wrong_key = key()
    wrong = certificate('Wrong root', wrong_key.public_key(), root_name, wrong_key, True, 1)
    for name, cert in [('root', root), ('wrong', wrong), ('inter', inter)]:
        (folder / (name + '.pem')).write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    for name, leaf_key in [('rsa', key()), ('ec', ec.generate_private_key(ec.SECP256R1()))]:
        cert = certificate('fixture.test', leaf_key.public_key(), inter.subject, inter_key)
        (folder / (name + '.pem')).write_bytes(cert.public_bytes(serialization.Encoding.PEM) +
                                                    inter.public_bytes(serialization.Encoding.PEM))
        (folder / (name + '.key')).write_bytes(leaf_key.private_bytes(
            serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
            serialization.NoEncryption()))

def context(folder, version=ssl.TLSVersion.TLSv1_2, suite=None, cert='rsa', client_auth=False):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        ctx.minimum_version = ctx.maximum_version = version
    # SECLEVEL=0 is restricted to the legacy test peer, never production code.
    ctx.set_ciphers(suite or 'ECDHE-RSA-AES128-GCM-SHA256')
    ctx.load_cert_chain(folder / (cert + '.pem'), folder / (cert + '.key'))
    ctx.options |= ssl.OP_NO_TICKET
    if client_auth:
        ctx.load_verify_locations(folder / 'root.pem')
        ctx.verify_mode = ssl.CERT_OPTIONAL
    return ctx

def listener():
    sock = socket.socket()
    sock.bind(('127.0.0.1', 0))
    sock.listen(1)
    sock.settimeout(20)
    return sock

def probe(folder, openssl):
    print(ssl.OPENSSL_VERSION)
    print(subprocess.check_output([openssl, 'version'], text=True).strip())
    for version, flag in [(ssl.TLSVersion.TLSv1, '-tls1'), (ssl.TLSVersion.TLSv1_1, '-tls1_1')]:
        ctx = context(folder, version, 'ECDHE-RSA-AES128-SHA:@SECLEVEL=0')
        sock = listener()
        errors = []
        def serve():
            try:
                conn, _ = sock.accept()
                conn.settimeout(10)
                with ctx.wrap_socket(conn, server_side=True) as tls:
                    assert tls.version() == ('TLSv1' if flag == '-tls1' else 'TLSv1.1')
            except Exception as exc:
                errors.append(exc)
        thread = threading.Thread(target=serve)
        thread.start()
        result = subprocess.run([openssl, 's_client', '-connect', f'127.0.0.1:{sock.getsockname()[1]}',
            flag, '-cipher', 'ECDHE-RSA-AES128-SHA:@SECLEVEL=0', '-CAfile', str(folder / 'root.pem'),
            '-verify_hostname', 'fixture.test', '-verify_return_error'], input=b'', capture_output=True, timeout=15)
        thread.join(15)
        sock.close()
        # OpenSSL 3 reports unexpected EOF because this probe closes after handshake.
        assert not errors and not thread.is_alive(), repr(errors)
        assert b'Verification: OK' in result.stdout, result.stdout + result.stderr
        print(f'Independent OpenSSL CLI {flag} handshake verified')

def hello(conn, legacy):
    # Read complete initial record, then replay it through the OpenSSL BIO peer.
    record = bytearray()
    while len(record) < 5 or len(record) < 5 + int.from_bytes(record[3:5], 'big'):
        data = conn.recv(65536)
        if not data:
            raise ConnectionError('peer canceled before ClientHello')
        record.extend(data)
    assert record[0] == 22 and record[5] == 1
    body = record[9:]
    session_len = body[34]
    assert session_len == 0, 'session resumption offered'
    pos = 35 + session_len
    size = int.from_bytes(body[pos:pos+2], 'big')
    offered = {int.from_bytes(body[i:i+2], 'big') for i in range(pos+2, pos+2+size, 2)}
    allowed = {0xcca8, 0xcca9, 0xc02f, 0xc02b, 0xc030, 0xc02c, 0x00ff}
    if legacy:
        allowed |= {0xc013, 0xc009}
    assert offered <= allowed, f'forbidden suite offered: {offered - allowed}'
    assert offered - {0x00ff} == allowed - {0x00ff}, 'missing policy suite'
    pos += 2 + size
    pos += 1 + body[pos]  # compression methods
    end = pos + 2 + int.from_bytes(body[pos:pos+2], 'big')
    pos += 2
    extensions = set()
    while pos < end:
        kind = int.from_bytes(body[pos:pos+2], 'big')
        size = int.from_bytes(body[pos+2:pos+4], 'big')
        extensions.add(kind)
        pos += 4 + size
    assert 35 not in extensions, 'session ticket extension offered'
    return bytes(record)


def bio_peer(ctx, conn, legacy, case, received, expect_failure):
    incoming, outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
    tls = ctx.wrap_bio(incoming, outgoing, server_side=True)
    incoming.write(hello(conn, legacy))
    def flush():
        while outgoing.pending:
            conn.sendall(outgoing.read())
    def operation(call):
        while True:
            try:
                result = call()
                flush()
                return result
            except ssl.SSLWantReadError:
                flush()
                data = conn.recv(65536)
                if not data:
                    incoming.write_eof()
                else:
                    incoming.write(data)
            except ssl.SSLError:
                flush()
                raise
    operation(tls.do_handshake)
    assert not tls.session_reused
    while len(received) < len(PAYLOAD):
        data = operation(lambda: tls.read(len(PAYLOAD) - len(received)))
        if not data:
            break
        received.extend(data)
        assert operation(lambda: tls.write(data)) == len(data)
    if not expect_failure:
        assert received == PAYLOAD
        if case == 'truncate':
            return
        if case == 'alert':
            # Make the independent TLS peer emit a correctly protected fatal
            # bad_record_mac alert, after the echoed plaintext was delivered.
            incoming.write(b'\x17\x03\x03\x00\x20' + bytes(32))
            try:
                tls.read(1)
                raise AssertionError('peer accepted invalid protected record')
            except ssl.SSLError:
                assert outgoing.pending > 5
                flush()
        elif case == 'malformed_alert':
            conn.sendall(b'\x15\x03\x03\x00\x02\x02\x28')
        else:
            try:
                operation(tls.unwrap)
            except (ssl.SSLError, OSError):
                pass  # client may destroy after receiving close_notify


def run(folder, executable, case):
    mode, limit, legacy = case, 4096, 0
    version, suite, cert = ssl.TLSVersion.TLSv1_2, None, 'rsa'
    if case.startswith('success_'):
        mode, limit = 'success', int(case.split('_')[1])
    if case in SUITES:
        suite = SUITES[case]
        cert = 'ec' if case.startswith('ec') else 'rsa'
        mode = 'success'
    if case in ('fail_tls10', 'fail_tls11', 'legacy10', 'legacy11', 'legacy_ec'):
        version = ssl.TLSVersion.TLSv1_1 if case.endswith('11') else ssl.TLSVersion.TLSv1
        suite = 'ECDHE-RSA-AES128-SHA:@SECLEVEL=0'
        if case == 'legacy_ec':
            suite, cert = 'ECDHE-ECDSA-AES128-SHA:@SECLEVEL=0', 'ec'
        if case.startswith('legacy'):
            mode, legacy = 'success', int(version)
    if case in ('fail_forbidden', 'fail_default_cbc'):
        suite = 'AES128-SHA:@SECLEVEL=0' if case == 'fail_forbidden' else 'ECDHE-RSA-AES128-SHA:@SECLEVEL=0'
    if case == 'fail_forbidden_legacy':
        suite, legacy = 'AES128-SHA:@SECLEVEL=0', int(ssl.TLSVersion.TLSv1_2)
    ctx = context(folder, version, suite, cert, case == 'fail_client_auth')
    sock = listener()
    errors, received = [], bytearray()
    sni = []
    ctx.set_servername_callback(lambda _sock, name, _ctx: sni.append(name))
    expect_failure = case.startswith('fail_') or case in ('cancel', 'close_pending')
    no_connect = case in ('fail_entropy', 'fail_identity', 'fail_ipv4hex', 'fail_ipv6', 'fail_nonascii', 'fail_time',
                           'fail_time_range', 'fail_time_negative_range')
    def serve():
        try:
            conn, _ = sock.accept()
            conn.settimeout(20)
            with conn:
                bio_peer(ctx, conn, legacy, case, received, expect_failure)
        except (ssl.SSLError, ConnectionError, OSError) as exc:
            if not expect_failure:
                errors.append(repr(exc))
        except Exception as exc:
            errors.append(repr(exc))
    thread = None
    if not no_connect:
        thread = threading.Thread(target=serve)
        thread.start()
    root = folder / ('wrong.pem' if case == 'fail_root' else 'root.pem')
    result = subprocess.run([executable, str(sock.getsockname()[1]), str(root), mode, str(limit), str(legacy)],
                            capture_output=True, text=True, timeout=40)
    if thread:
        thread.join(25)
        assert not thread.is_alive(), 'peer failed to finish'
    sock.close()
    assert result.returncode == 0, result.stdout + result.stderr
    assert not errors, errors
    if expect_failure:
        assert not received, f'HTTP bytes leaked before authentication: {len(received)}'
    elif not no_connect:
        assert sni == ['FiXtUrE.TeSt' if case == 'mixed_case' else 'fixture.test'], sni
    print(f'{case}: {result.stdout.strip() or "fail-closed assertions passed"}')

def renegotiation(folder, executable, openssl):
    # /reneg is OpenSSL s_server's test-only request for server renegotiation.
    reservation = listener()
    port = reservation.getsockname()[1]
    reservation.close()
    server = subprocess.Popen([openssl, 's_server', '-accept', f'127.0.0.1:{port}',
        '-cert', str(folder / 'rsa.pem'), '-cert_chain', str(folder / 'inter.pem'),
        '-key', str(folder / 'rsa.key'), '-tls1_2', '-no_ticket', '-naccept', '1',
        '-cipher', 'ECDHE-RSA-AES128-GCM-SHA256', '-www', '-msg'],
        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    trace = bytearray()
    ready = threading.Event()
    def drain():
        for line in iter(server.stdout.readline, b''):
            trace.extend(line)
            if b'ACCEPT' in line:
                ready.set()
    reader = threading.Thread(target=drain)
    reader.start()
    try:
        assert ready.wait(5), 's_server did not become ready'
        client = subprocess.run([executable, str(port), str(folder / 'root.pem'),
            'renegotiate', '4096', '0'], capture_output=True, text=True, timeout=20)
        server.wait(timeout=15)
        reader.join(5)
        assert client.returncode == 0, client.stdout + client.stderr
        assert b'HelloRequest' in trace, 'server did not request renegotiation'
        assert trace.count(b'ClientHello') == 1, 'client started another handshake'
        assert b'no_renegotiation' in trace, 'client did not refuse renegotiation'
        print('server HelloRequest refused; exactly one ClientHello; no renegotiation')
    finally:
        if server.poll() is None:
            server.kill()
            server.wait()
        reader.join(5)
        server.stdout.close()
        server.stderr.close()



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--openssl', required=True)
    parser.add_argument('--client')
    parser.add_argument('--case', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='netscape-tls-') as temp:
        folder = Path(temp)
        material(folder)
        if args.case == 'prerequisites':
            probe(folder, args.openssl)
        elif args.case == 'renegotiate':
            renegotiation(folder, args.client, args.openssl)
        else:
            run(folder, args.client, args.case)

if __name__ == '__main__':
    main()
