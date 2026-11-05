# Native TLS transport tests

These tests currently run in the native Windows configuration with
`NETSCAPE_ENABLE_BEARSSL_HTTPS=ON` and `BUILD_TESTING=ON`.

- `netscape_tls_dependency_test` builds `tls_dependency_test.c` and retains the
  `tls_dependency` CTest name for the original upstream initialization smoke.
- `netscape_tls_engine_test` builds `tls_engine_test.c`, exercising the portable
  adapter through its public API and injected nonblocking socket callbacks.
- `tls_engine_*` CTest cases launch `tls_fixture.py`, which generates temporary
  root/intermediate/leaf certificates and controls independent local TLS peers.

The fixtures require Python 3 with `cryptography` and `ssl`, plus an OpenSSL CLI.
Set `NETSCAPE_TLS_FIXTURE_OPENSSL` to its executable path if CMake cannot find it.
The CLI is used only by tests and is neither linked nor installed with Netscape.
The prerequisite case explicitly checks real TLS 1.0 and 1.1 handshakes; failure
blocks the compatibility cases. Weak legacy settings apply only to fixture peers.
Keys are temporary, deterministic entropy exists only in the C test driver, and
no session key logs are produced.

```powershell
cmake --build out/bearssl-win32 --config Release --target netscape_tls_engine_test netscape_tls_dependency_test
ctest --test-dir out/bearssl-win32 -C Release -R '^tls_engine' --output-on-failure
```

The matrix covers authentication, checked UTC time beyond 2038, cipher/version
policy, original-case SNI, fragmentation, backpressure, bounded progress,
buffered plaintext, close notification, truncation, fatal alerts, cancellation,
client-auth rejection, wire-level absence of resumption, and refusal of a real
server renegotiation request. The engine matrix is supplemented by native
browser, policy, error and installed workflow tests; see [the Windows guide](../windows.md#native-browser-https-bearssl).


`check_windows_tls.py --mode installed --executable <installed bin/netscape.exe>
--output <test directory>` checks package provenance/notices and uses only copies
for trust mutations. Its output directory must be outside the installed prefix.
`windows_tls_installed` performs a current-configuration Runtime install first.
With `NETSCAPE_ENABLE_BEARSSL_HTTPS=OFF`, `--mode disabled` checks real HTTPS
rejection without a TCP connection and successful ordinary HTTP rendering.
Do not run native browser/UI suites concurrently.


The adapter's `tls_x509_purpose` fixture covers bounded DER/EKU parsing and the
forwarding validator contract. Independent peer cases exercise allowed absent,
serverAuth, mixed and anyExtendedKeyUsage policies, rejected unrelated/client-only
purpose on leaves and intermediates, and preserved underlying validation errors.
Native displayed-error coverage includes purpose category 16. The per-presented-
certificate bound is 65,536 bytes, with a fixed additional 64 KiB per TLS context;
valid critical EKU remains rejected by the underlying validator. These focused
checks do not substitute for the final native matrix after the purpose fix.
