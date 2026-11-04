# Portable BearSSL HTTPS, Windows first

Date: 2026-11-04. Status: approved by the user on 2026-11-04.

Builder check: a fresh C networking/security reviewer reported "nothing
important left" on 2026-11-04. This checks planning completeness, not code or
cryptographic correctness.

## Intent and agreed scope

Enable HTTPS fetching through the existing browser networking and rendering
engines, with native Win32 as the first supported integration. Reuse a portable
TLS implementation so later Linux and other OS ports can share the work.

The user selected BearSSL and accepted deferring TLS 1.3. TLS 1.2 is the default;
an explicit compatibility setting permits TLS 1.0 and 1.1. Use an updateable
Mozilla-derived CA bundle, with a configurable bundle path for private CAs and
tests. This is a project, not a protocol-version toggle.

The choices below marked as proposals are engineering calls for approval, not
additional requirements attributed to the user.

## Goals and acceptance

- Load an HTTPS HTML page and its HTTPS subresources in the native browser.
- Preserve ordinary HTTP, FTP, mail/news and local-file behavior.
- Send SNI for DNS hostnames and verify the requested server identity, certificate
  validity dates and a chain to a configured trust anchor before sending HTTP
  application data. Fail closed on authentication or TLS errors.
- Negotiate TLS 1.2 by default. Compatibility mode permits 1.0 through 1.2;
  it never disables certificate validation or enables SSL 2/3.
- Keep networking nonblocking during handshake, application I/O and shutdown;
  cancellation and multiple concurrent loads must remain responsive.
- Support HTTPS through netlib's existing explicit HTTP CONNECT proxy path.
- Report TLS failures meaningfully and expose the negotiated protocol/cipher
  without representing an unverified connection as authenticated.
- Install a relocatable browser package containing its required TLS assets.

Acceptance is deterministic local integration testing plus a small, separately
reported public-site smoke check. Public-site rendering or reachability alone
does not establish TLS correctness.

## Non-goals and limits

TLS 1.3, HTTP/2/3, a newer layout/JavaScript engine, client certificates,
S/MIME, mail STARTTLS/implicit TLS, TLS server APIs and certificate-management
UI are out of scope. Linux runtime integration is deferred, although the shared
adapter must not depend on Win32 or MFC types. No proxy service or external
browser is used to fetch pages on Netscape's behalf.

Proposal: the first version has no certificate-error bypass, online CA updater,
OCSP/CRL fetching, or automatic intermediate-certificate downloads. BearSSL's
minimal validator lacks revocation checking and expects an ordered chain.
Document these limits. A Mozilla-derived PEM bundle is not equivalent to
Firefox's full trust policy: curl explicitly notes that some additional root
constraints are absent from the PEM conversion. Do not claim Firefox-equivalent
certificate validation or suitability for routine secure browsing.

## Current code and constraints

- `security/lib/ssl/sslproto.h` defines SSL 2/3 versions. The old crypto and
  SSL engine are not the implementation to extend for this project.
- `include/xp_sock.h` selects native or `SSL_*` socket calls. The Windows
  platform CMake definitions and `lib/libnet/wintcp.h` force `NO_SSL` today.
- `lib/libnet/mkconect.c` contains secure-socket setup, while `mkhttp.c` has
  HTTPS and CONNECT handling and consumes legacy certificate/status APIs.
- `mkgeturl.c` dispatches HTTPS and also owns legacy certificate cleanup and
  information paths. Audit these consumers rather than switching off `NO_SSL`
  globally and accidentally pulling in the whole old security implementation.
- `cmd/winfe/platform.c` polls Winsock read/connect readiness and invokes
  `NET_ProcessNet`. TLS can need a write while servicing a logical read, or
  have decrypted data available without new socket readability.
- Native Windows currently targets x86 with MSVC/clang-cl. NSPR and the
  historical socket interfaces remain in place.
- Existing uncommitted work includes frontend, networking and preferences
  files. Implementation must reconcile current contents and preserve that work;
  this design adds no product-code changes.

## Approaches

1. Extend the original SSL/crypto engine. Portable in principle, but requires
   protocol machinery, modern primitives and substantial cryptographic review.
   The user accepted a portable replacement backend instead.
2. Put a maintained portable TLS backend behind the existing networking
   interface. OpenSSL was initially recommended for TLS 1.0-1.3; wolfSSL was
   also discussed. The user selected BearSSL, accepting TLS 1.3 deferral.
   This retains a small C TLS engine while avoiding a new TLS implementation.
3. Use Schannel for Windows. Avoids shipping a TLS library but makes subsequent
   ports use a different engine and protocol availability depend on Windows.
   This does not match the user's portability priority.

TinyTls was considered during selection. The Anthony-Mai project documents
TLS 1.2/1.3 only and restricted key support, so it does not cover the agreed
compatibility range. BearSSL upstream documents TLS 1.0-1.2, not TLS 1.3.

## Shared adapter and build

Proposal: create a separate portable C TLS adapter/archive under `security/`,
using BearSSL's low-level engine interface. Keep BearSSL types private and expose
the subset of netlib's socket/security contract needed by HTTPS. Preserve
existing `SSL_*` call sites where useful; add narrow explicit adapter calls for
readiness and status where the old API is inadequate. Do not fabricate legacy
`CERTCertificate` objects or implement unused legacy security APIs as success
stubs. Adapt certificate/status consumers explicitly.

Keep TLS-enabled HTTPS separate from the existing `NETSCAPE_ENABLE_SECURITY`
option, which means legacy SSL/S-MIME. A new build option enables BearSSL HTTPS;
reject conflicting backends. Proposal: enable it by default for Windows
frontend builds, leave Linux backend selection unchanged, and preserve an
explicit TLS-disabled build that rejects HTTPS clearly.

Pin an upstream BearSSL revision and archive checksum. Start by evaluating the
latest upstream source, including changes since release 0.6, rather than assuming
the 2018 release is the appropriate dependency. The builder chooses and records
the exact revision after checking upstream fixes and Win32 compilation.
Keep upstream code distinguishable from local changes, retain notices and supply
a reproducible acquisition/build path. Builds must not silently fetch moving
upstream HEAD. Dependency source placement and static linkage are builder calls;
prefer static linkage for the initial Windows package.

## Connection lifecycle and readiness

Each secure connection owns its TLS context, X.509 context, stable buffers,
hostname, pending output, terminal error and a reference to an immutable trust
store. Create and destroy these with the connection; wipe secret buffers on
teardown. Plaintext sockets continue using native operations.

Seed BearSSL from a cryptographically secure OS entropy source before any
handshake, through a portable platform callback (a Windows system RNG in the
first integration). Fail closed if entropy acquisition fails; never fall back
to timestamps, process IDs, `rand()` or the legacy RNG without review. Proposal:
disable session resumption for the first version to avoid adding session-cache
and trust-policy invalidation complexity; HTTP keep-alive still works.

Perform TCP connection first, then CONNECT when applicable, then start TLS
against the destination hostname, never the proxy hostname. Keep TLS state
attached to the socket through keep-alive reuse. Never reuse it across a
different origin or incompatible policy/trust configuration.

Drive the TLS engine until it needs socket input/output, has application data,
finishes, or reaches a bounded work budget. Retain partially sent records and
return the existing would-block convention without dropping or duplicating
application bytes. Distinguish clean TLS closure, TCP truncation and protocol
failure; unclean termination must not turn an incomplete response into success.

Add portable queries for required network readiness and buffered application
data. The Windows poller uses these alongside netlib's logical watch flags.
It must schedule progress on buffered plaintext and pending output without
busy looping, starving other loads, or treating handshake completion as TCP
connection completion twice. Bound close-notify work; cancellation never waits
indefinitely for the peer. Ensure descriptor reuse cannot inherit old TLS state.

## Protocol and cipher policy

Proposal: store a profile-local `allow_legacy_tls` boolean, false by default.
It permits TLS 1.0/1.1 in the same negotiation as 1.2; no silent retry with weaker
settings after a failed handshake. Apply changes to new connections and retire
idle reusable connections created under the old policy.

Expose the setting as a native preference labelled to explain obsolete-protocol
compatibility. This is the only required new preference control; detailed visual
layout remains a builder call consistent with the existing native preferences.

Prefer forward-secret AES-GCM and ChaCha20-Poly1305 suites for TLS 1.2. Use an
explicit reviewed suite allowlist; do not inherit every full-profile suite
automatically. Compatibility mode may additionally permit the AES-CBC suites
needed for 1.0/1.1, retaining BearSSL's relevant mitigations. Exclude RC4, DES,
3DES, NULL and export suites in both modes. Exact compatible suites are a
builder decision supported by interoperability tests, not a promise to connect
to every obsolete server.

## Certificates and CA bundle

Proposal: ship curl's Mozilla-derived `cacert.pem`, pinned by date and SHA-256,
with source URL and MPL 2.0 notice. Load from an application-relative installed
data path, with a documented development-build path. A profile-local explicit
CA-bundle path replaces the default bundle; do not implicitly search the current
directory or import historical built-in Netscape roots.

Decode PEM certificates into BearSSL trust anchors once per immutable store,
preserving required names, key material and CA flags. Reject malformed input,
unsupported trust-anchor types and empty stores with useful diagnostics; never
silently accept an empty or partially parsed trust configuration. Bound file
size, certificate count and decoding allocations. Record bundle identity for
diagnostics and connection reuse. Refresh on application restart initially.

Verify DNS names using the original destination, with SNI supplied to BearSSL;
avoid ad hoc wildcard matching. Proposal: support ASCII DNS hostnames first and
reject non-ASCII hostnames and IP-literal HTTPS with an explicit unsupported
identity error unless correct normalization/SAN verification is implemented and
tested. Local fixtures can connect `localhost` to loopback. Do not disable name
checks to accommodate test endpoints.

CA updates are maintainer-controlled package updates or explicit file
replacement, with provenance/hash documented. Tests use a separate generated
test CA and never modify the shipping bundle. Do not imply that extracting the
Mozilla roots carries all Firefox policy or that certificate revocation is
checked.

## Error reporting and observable behavior

Map failures into distinguishable categories: DNS/TCP, proxy rejection,
handshake/protocol, hostname mismatch, expired/not-yet-valid certificate,
untrusted chain, malformed/missing CA bundle, truncation and cancellation.
Keep diagnostics free of keys and plaintext payloads. Status should distinguish
an authenticated encrypted response from plaintext and failed authentication.
Proposal: concise status/error text and connection details are sufficient for
this phase; a security-dialog or toolbar redesign is not required.

## Validation

Use isolated profiles and a controllable TLS fixture with modern crypto tooling,
separate from the browser's dependency. Required cases include:

- TLS 1.2 page plus subresource, SNI selection, redirects and download bytes.
- TLS 1.0/1.1 refused by default and accepted only in compatibility mode;
  SSL 2/3 never enabled. Fixture tooling must actually support the legacy
  handshake; an unavailable fixture is an unverified case, not a passing test.
- Valid chain/name/date accepted; unknown root, wrong name, invalid dates,
  malformed certificate and missing/malformed/empty bundle rejected.
- CONNECT success and rejection; origin identity verified inside the tunnel.
- Fragmented handshakes/records, partial writes, buffered plaintext, large
  responses, cancellation, concurrent loads, keep-alive and socket reuse.
- Clean shutdown versus truncated response and dropped handshake.
- Existing HTTP/socket and relevant frontend regressions in Debug and Release.
- Installed/relocated package run from a path containing spaces, correct bundle
  discovery, explicit override and TLS-disabled configuration behavior.

Capture protocol/cipher and certificate outcomes through a deterministic smoke
interface without exposing cryptographic secrets. Public HTTPS checks are
supplementary and may reveal site/legacy rendering incompatibilities separately.
Run actual native builds/tests before claiming success. Linux and other OS
runtime behavior remain explicitly unverified until their own integrations.

## Work left to the implementation plan

Choose exact dependency revision, adapter file/API names, allowed suites,
fixture tool and native preference placement; audit all `NO_SSL`,
`HAVE_SECURITY`, `NADA_VERSION`, certificate and socket consumers before edits.
Confirm BearSSL build and Mozilla-bundle parsing as an early implementation
milestone. Keep source/build integration, TLS transport, trust/policy,
frontend reporting and installed verification as independently reviewable work.

## References

- BearSSL API: https://bearssl.org/api1.html
- BearSSL certificate validation: https://bearssl.org/x509.html
- BearSSL protocol support: https://bearssl.org/support.html
- BearSSL TLS 1.3 status: https://bearssl.org/tls13.html
- Mozilla-derived PEM distribution and its policy limitations:
  https://curl.se/docs/caextract.html
- TinyTls scope: https://github.com/Anthony-Mai/TinyTls
