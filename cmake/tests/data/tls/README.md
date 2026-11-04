# Trust-loader test certificates

Generated solely for tests using Python cryptography 46.0.3. RSA-2048 and
P-256 CA certificates and a leaf for `test.example` are valid 2020-01-01
through 2040-01-01. The leaf is signed by the RSA test CA. The Ed25519 CA
is intentionally unsupported by BearSSL 0.6. No private keys are retained.
These certificates confer no trust outside the isolated tests.

`padded-ca.der` is a signed RSA CA containing a large noncritical test-only
extension (OID 1.3.6.1.4.1.55555.1), sized to exactly 65,536 DER bytes. It tests
the inclusive certificate-size boundary. Its private key was discarded too.
