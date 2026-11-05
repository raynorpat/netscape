"""Signed certificates with deliberate EKU corruption for the private parser.

Invalid extensions may be rejected by OpenSSL before a peer starts. These
fixtures exercise DER semantics and streaming bounds without claiming trust.
"""
import argparse
import datetime as dt
from pathlib import Path
import subprocess
import tempfile
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.x509.oid import NameOID
import tls_fixture


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--client', required=True)
    args = parser.parse_args()
    key = tls_fixture.key()
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'fixture.test')])
    def cert(raw, duplicate=False, padding=0):
        builder = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
            .public_key(key.public_key()).serial_number(1)
            .not_valid_before(dt.datetime(2020,1,1,tzinfo=dt.timezone.utc))
            .not_valid_after(dt.datetime(2045,1,1,tzinfo=dt.timezone.utc)))
        if raw is not None:
            builder = builder.add_extension(x509.UnrecognizedExtension(
                x509.ObjectIdentifier('2.5.29.37'), raw), False)
        if duplicate:
            builder = builder.add_extension(x509.UnrecognizedExtension(
                x509.ObjectIdentifier('2.5.29.38'), raw), False)
        if padding:
            builder = builder.add_extension(x509.UnrecognizedExtension(
                x509.ObjectIdentifier('1.2.3.4'), bytes(padding)), False)
        data = builder.sign(key,hashes.SHA256()).public_bytes(serialization.Encoding.DER)
        if duplicate:
            assert data.count(bytes.fromhex('0603551d26')) == 1
            data = data.replace(bytes.fromhex('0603551d26'),bytes.fromhex('0603551d25'))
        return data
    server = bytes.fromhex('300a06082b06010505070301')
    cases = [
        ('server',cert(server),0), ('absent',cert(None),0),
        ('client',cert(bytes.fromhex('300a06082b06010505070302')),16),
        ('unrelated',cert(bytes.fromhex('300406022a03')),16),
        ('duplicate',cert(server,duplicate=True),12),
        ('empty',cert(bytes.fromhex('3000')),12),
        ('bad-tag',cert(bytes.fromhex('300a04082b06010505070301')),12),
        ('empty-oid',cert(bytes.fromhex('30020600')),12),
        ('unfinished-oid',cert(bytes.fromhex('3003060181')),12),
        ('server-then-bad-oid',cert(bytes.fromhex('300d06082b06010505070301060181')),12),
        ('nonminimal-oid',cert(bytes.fromhex('300406028000')),12),
        ('truncated',cert(server[:-1]),12),
        ('trailing',cert(server+b'\x00'),12),
        ('indefinite',cert(bytes.fromhex('308006082b060105050703010000')),12),
        ('nonminimal-length',cert(bytes.fromhex('30810a06082b06010505070301')),12),
        ('leading-zero-length',cert(bytes.fromhex('3082000a06082b06010505070301')),12),
        ('overflow-length',cert(bytes.fromhex('3088ffffffffffffffff')),12),
    ]
    # Make the whole, independently signed certificate exactly the buffer cap.
    padded = cert(server,padding=64000)
    padding = 64000 + 65536 - len(padded)
    exact = cert(server,padding=padding)
    assert len(exact) == 65536
    cases += [('exact-limit',exact,0),('over-limit',cert(server,padding=padding+1),12),
              ('outer-overflow',b'\x30\x88'+bytes([255])*8,12)]
    with tempfile.TemporaryDirectory(prefix='netscape-purpose-') as tmp:
        for name, data, expected in cases:
            path=Path(tmp)/(name+'.der');path.write_bytes(data)
            result=subprocess.run([args.client,str(path),str(expected)],capture_output=True,text=True,timeout=20)
            assert result.returncode == 0,(name,result.stdout,result.stderr)
            print('PASS',name)
    print(f'All {len(cases)} DER/streaming purpose cases passed')

if __name__ == '__main__':
    main()
