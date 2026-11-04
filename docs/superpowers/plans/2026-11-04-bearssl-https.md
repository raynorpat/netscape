# Portable BearSSL HTTPS Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Load authenticated HTTPS pages through the native Windows browser using portable BearSSL code and Mozilla-derived CA certificates.

**Architecture:** Keep legacy SSL/S-MIME disabled. Add a portable, callback-driven BearSSL engine and trust loader, a socket adapter for netlib, and Windows readiness/preferences integration. Reuse the existing HTTP engine, including CONNECT tunnels, and retain separate plaintext and legacy-security configurations.

**Tech Stack:** C, existing C++14/MFC frontend, Win32 x86/MSVC, CMake/CTest, Python fixture orchestration, a pinned BearSSL revision, curl's Mozilla-derived PEM bundle.

**Spec:** `docs/superpowers/specs/2026-11-04-bearssl-https-design.md` (approved 2026-11-04). Read both documents before execution.

## Global Constraints

- Native Win32 is the first supported integration; no Linux runtime claim.
- TLS 1.2 is the default; compatibility mode permits TLS 1.0 through 1.2.
- TLS 1.3, SSL 2/3, client certificates, mail TLS and S/MIME are out of scope.
- Fail closed on certificate, hostname, entropy and TLS errors; no bypass.
- Exclude RC4, DES, 3DES, NULL and export suites in both modes.
- No blocking handshake, secret logging, automatic downgrade retry or session resumption.
- Ship an updateable Mozilla-derived CA bundle; explicit override replaces it.
- No revocation checks, automatic intermediate downloads or background CA updater.
- ASCII DNS HTTPS first; explicitly reject IP literals and non-ASCII hostnames.
- Preserve existing style and unrelated work; never restore CVS metadata.
- Implementation takes place in a separate worktree, with separate build directories.

## Review Focus

These additional cases supplement the spec's acceptance matrix and have owning tests below.

1. A redirect changes the destination hostname: new SNI and verification identity, never inherited trust from the old origin (Task 5).
2. The peer sends application bytes and then a fatal alert: buffered data must not mask the terminal error or authenticate an incomplete result (Tasks 3 and 5).
3. A malformed replacement CA bundle follows valid entries: reject the entire replacement, never silently use a partial store (Task 2).
4. A canceled connection's socket number is reused: no stale TLS state, watcher, error or authenticated status transfers to the new request (Tasks 4 and 5).
5. Changing compatibility preferences while a load runs: existing connection retains its snapshot; new loads and idle-pool reuse obey the new generation (Task 6).

## Worktree and execution boundary

This plan does not authorize starting implementation before plan review. The user
requires a separate worktree, not a new chat. Do not create a chat or transfer
unrelated uncommitted changes.

- [ ] At execution time invoke `superpowers:using-git-worktrees`. Inspect branch,
  Git/common directories and status; use the app's managed worktree tool where
  available. Create a worktree from the commit containing this plan and approved
  spec, on `codex/bearssl-https` where named branches are supported. Record its
  resolved path and base commit. If detached by the app, follow that environment's
  workflow rather than editing the original checkout.
- [ ] Verify `git status --short` is clean in the new worktree and both documents
  exist there. Re-read applicable `AGENTS.md` and inspect current code. The
  original checkout has ongoing independent work; do not stash, reset or copy it.
- [ ] Use `out/bearssl-win32` and `out/bearssl-gold` under the new worktree. Run a
  baseline Release build and existing tests before implementation; record actual
  toolchain, failures and unavailable prerequisites. Do not reuse original
  checkout build caches.

Commands below run from the implementation worktree. Each task's commit stages
only its listed changes. Update this plan's checkboxes as execution progresses.

## File and interface map

New files:

- `security/tls/net_tls.h`: portable opaque engine/trust types and callbacks.
- `include/net_tls_types.h`: adapter-neutral TLS metadata and error categories.
- `security/tls/net_tls_trust.c`: bounded PEM decoding and immutable trust ownership.
- `security/tls/net_tls_engine.c`: BearSSL state machine, policy, read/write/error handling.
- `security/tls/net_tls_socket.h`, `security/tls/net_tls_socket.c`: socket registry and narrow netlib transport operations; no BearSSL types exported.
- `cmd/winfe/tls-platform.c`: native socket callbacks, secure entropy, installed bundle discovery and frontend initialization.
- `cmd/winfe/tls-smoke.inc`: focused browser smoke runner included by `main.cpp`, following existing smoke include patterns.
- `cmake/targets/bearssl.cmake`: pinned dependency and static adapter targets.
- `third_party/bearssl/README.md`, `third_party/bearssl/upstream/`: pristine pinned source, notices and provenance.
- `security/ca/cacert.pem`, `security/ca/README.md`: pinned Mozilla-derived roots, notices and refresh procedure.
- `cmake/tests/tls_engine_test.c`, `cmake/tests/tls_trust_test.c`, `cmake/tests/tls_socket_test.c`: focused C test drivers.
- `cmake/tests/tls_fixture.py`: local certificates, TLS peers, CONNECT relay and fragmentation support.
- `cmake/tests/check_windows_tls.py`: orchestrated engine/browser/installed verification.

Existing files to modify:

- `CMakeLists.txt`, `cmake/modules/NetscapeWindows.cmake`,
  `cmake/targets/windows/core.cmake`, `cmake/targets/windows/frontend.cmake`:
  build selection, include/link wiring, install assets and CTest registration.
- `include/xp_sock.h`, `lib/libnet/wintcp.h`: narrowly route read/write/close for
  the new backend while retaining native connect/bind/listen and `NO_SSL`.
- `lib/libnet/mkconect.c`, `lib/libnet/mkhttp.c`, `lib/libnet/mkgeturl.c`:
  secure attachment, CONNECT transition, reuse, metadata and errors.
- `include/net.h`: owned request metadata for TLS version/cipher/error, separate
  from legacy `CERTCertificate` objects.
- `cmd/winfe/platform.c`, `cmd/winfe/frontend.h`, `cmd/winfe/main.cpp`:
  readiness/pending plaintext, lifecycle and smoke dispatch.
- `cmd/winfe/preferences.h`, `cmd/winfe/preferences-settings.cpp`,
  `cmd/winfe/preferences-ui.cpp`, `cmd/winfe/preferences_resource.h`,
  `cmd/winfe/res/preferences.rc`: persisted compatibility and bundle override.
- `cmake/tests/check_windows_preferences.py`, `cmake/windows.md`, `README.md`:
  native preference coverage and factual build/runtime documentation.

Line numbers are intentionally omitted because independent frontend work is
ongoing. Locate the named functions in the committed execution base.

### Task 1: Reproducible dependency and TLS build selection

**Files:** Dependency/build files above; `security/tls/net_tls.h`; initial `tls_engine_test.c`.

**Interfaces:** Produces `netscape_bearssl` (static upstream C archive)
and `NETSCAPE_ENABLE_BEARSSL_HTTPS` (default ON
only when building the Windows frontend). Define `NETSCAPE_USE_BEARSSL` only
when selected; keep `NO_SSL` and the legacy option's meaning unchanged.

- [ ] Write configure checks rejecting BearSSL plus `NETSCAPE_ENABLE_SECURITY`,
  and rejecting BearSSL runtime integration on unsupported platforms. Add CTest
  `tls_dependency` with assertions `BR_TLS12 == 0x0303` and
  `br_ssl_client_init_full != NULL`. Run it before dependency wiring; expect a
  missing target/header failure attributable to the absent dependency.
- [ ] Inspect upstream changes since 0.6; pin a full revision and source archive
  SHA-256 in `third_party/bearssl/README.md`. Import source unchanged, retain
  upstream notices, and record any excluded tools/platform files. No configure-time
  network fetch. Build upstream portable C as a private static dependency.
- [ ] Define TLS-enabled and disabled configurations. Verify supported Windows
  x86 compilation and a clear configure failure for conflicting backends. No
  frontend behavior changes in this task.
- [ ] Configure and build with commands below; run `ctest --test-dir out/bearssl-win32 -C Release -R '^tls_dependency$' --output-on-failure`;
  expect one passing test. Commit as `build: add pinned BearSSL HTTPS backend`.

```powershell
cmake -S . -B out/bearssl-win32 -G "Visual Studio 18 2026" -A Win32 -DNETSCAPE_BUILD_WINDOWS_FRONTEND=ON -DNETSCAPE_ENABLE_BEARSSL_HTTPS=ON
cmake --build out/bearssl-win32 --config Release --parallel
```

Use another installed compatible Visual Studio generator if required and record
it consistently. Do not change platform/toolchain solely to make tests appear green.

### Task 2: Immutable CA stores and packaged Mozilla roots

**Files:** `net_tls.h`, `include/net_tls_types.h`, `net_tls_trust.c`, `tls_trust_test.c`, `security/ca/*`, CMake test/install wiring.

**Interfaces:** Opaque `NETTLSTrust`; `NETTLS_TrustLoad(const char *path,
NETTLSTrust **out, char *error, size_t error_size)` returns 1/0; output is NULL
on failure. Introduce the `netscape_tls` adapter archive here with its first
implementation source. `NETTLS_TrustRetain(NETTLSTrust *)` and
`NETTLS_TrustRelease(NETTLSTrust *)` own references. Internal trust-anchor views
stay private. In `include/net_tls_types.h`, define `NETTLSError` categories and fixed `NETTLSInfo` containing
`unsigned short version, cipher_suite; int authenticated; NETTLSError error;`.

- [ ] Add C tests `valid_bundle`, `unknown_pem_object`, `empty_bundle`,
  `malformed_tail`, `unsupported_anchor`, `oversized_bundle`, `ownership`.
  Assertions include `assert(!load("valid-then-malformed.pem"));`,
  `assert(out == NULL);`, and `assert(load("test-ca.pem"));` with stable
  anchors after parser buffers are released. Expected first run: unavailable loader.
- [ ] Implement bounded decoding: maximum file 4 MiB, 512 certificates, 64 KiB
  per DER certificate; reject malformed framing, trailing garbage and unsupported
  certificate keys. Allow whitespace, comments and informational text surrounding
  certificate PEM blocks as found in curl's actual bundle, but reject unexpected
  PEM object types. Whole-file success is required; clean every partial allocation.
- [ ] Retrieve a dated Mozilla-derived bundle from curl's CA Extract service;
  record date, exact URL and SHA-256, and preserve MPL 2.0 notice/source attribution.
  Add a test parsing the entire shipped bundle without silently omitting roots.
  If a root cannot be represented, investigate before proceeding; never weaken
  trust loading to make the shipping bundle pass.
- [ ] Run `ctest --test-dir out/bearssl-win32 -C Release -R '^tls_trust' --output-on-failure`;
  expect every case and shipped-bundle parse to pass. Commit as
  `feat: load bounded Mozilla-derived TLS trust stores`.

### Task 3: Portable nonblocking TLS engine

**Files:** `net_tls.h`, `net_tls_engine.c`, `tls_engine_test.c`, `tls_fixture.py`.

**Interfaces:** Opaque `NETTLS`; `NETTLSIO` contains `void *arg` and callbacks
`int recv(void *, void *, size_t)`, `int send(void *, const void *, size_t)`,
`int entropy(void *, void *, size_t)` and
`int utc_time(void *, int64_t *unix_seconds)`. I/O returns bytes, 0 for EOF, -2 for
would-block, -1 for failure; entropy/time return 1/0. Convert supplied UTC time
to BearSSL's validation representation with checked arithmetic; test dates
beyond 2038 and fail closed when a trustworthy time cannot be supplied.
`NETTLS_Create(const char *hostname, NETTLSTrust *, int allow_legacy,
const NETTLSIO *, NETTLS **out)` returns 1/0.
`NETTLS_Pump(NETTLS *)` returns -1 fatal, 0 pending, 1 authenticated.
`int NETTLS_Read(NETTLS *, void *buf, size_t len)` and
`int NETTLS_Write(NETTLS *, const void *buf, size_t len)` return bytes,
0 only for clean read EOF, -2 pending, -1 fatal.
`NETTLS_Wants(const NETTLS *)` returns READ=1, WRITE=2, LOCAL=4 bits;
`NETTLS_GetInfo(const NETTLS *, NETTLSInfo *)`,
`void NETTLS_Close(NETTLS *)` (one bounded nonblocking close step),
`void NETTLS_Destroy(NETTLS *)`. No platform headers in this API.

- [ ] Write failing tests for successful TLS 1.2, legacy version rejection,
  wrong name/root/date, entropy failure and unsupported identity. Required
  assertions: `assert(info.authenticated == 0)` on every failure;
  `assert(http_bytes_sent_before_auth == 0)`;
  `assert(default_negotiated_version == 0x0303)`.
- [ ] Use a Python-controlled local TLS peer with independent modern crypto
  tooling. Generate root, intermediate and leaf certificates with explicit SANs,
  validity dates and key usage. Add test prerequisites for a fixture OpenSSL CLI;
  never install or link it into the browser. Probe its TLS 1.0/1.1 capability
  explicitly; configure legacy fixture policy only inside test processes.
- [ ] Implement BearSSL engine initialization, explicit time input, secure entropy
  injection, original-hostname SNI/verification and stable context/buffer ownership.
  Disable resumption and renegotiation. Start with ECDHE_RSA/ECDHE_ECDSA
  AES-128/256-GCM and ChaCha20-Poly1305 suites supported by the pin; legacy mode
  adds ECDHE_RSA/ECDHE_ECDSA AES-128-CBC-SHA. Confirm exact suite constants against
  upstream. Exclude forbidden suites regardless of peer offers.
- [ ] Add fragmented record/handshake and backpressure tests, with deterministic
  callback limits of 1, 7 and 4096 bytes. Assert byte-for-byte payload integrity,
  eventual progress, pending-output preservation and bounded pump iterations.
  Test pending plaintext without socket readability, fatal alert after plaintext,
  clean close versus TCP truncation, canceled handshake and unsupported client auth.
- [ ] Run `ctest --test-dir out/bearssl-win32 -C Release -R '^tls_engine' --output-on-failure`;
  expect all cases passing. Missing legacy fixtures remain explicitly unverified
  and block claiming the full compatibility acceptance matrix complete. Commit
  as `feat: implement portable nonblocking BearSSL client transport`.

### Task 4: Socket ownership and Windows transport services

**Files:** `net_tls_socket.h/.c`, `tls-platform.c`, `tls_socket_test.c`, `frontend.h`, `platform.c`, CMake wiring.

**Interfaces:** Use `typedef uintptr_t NETTLSSocket` without Win32 types in
the shared header. `NETTLS_Attach(NETTLSSocket, const char *host, unsigned short port)`
returns 1/0 and snapshots the active trust/policy generation.
`int NETTLS_SocketRead(NETTLSSocket, void *, int)` and
`int NETTLS_SocketWrite(NETTLSSocket, const void *, int)` return normal netlib
bytes/EOF/-1 with native would-block errors translated appropriately.
`int NETTLS_SocketClose(NETTLSSocket)` destroys state and closes the native socket once;
`unsigned NETTLS_SocketWants(NETTLSSocket)` returns engine readiness bits;
`int NETTLS_SocketInfo(NETTLSSocket, NETTLSInfo *)` returns 1 if attached, else 0.
`int NETTLS_CanReuse(NETTLSSocket, const char *, unsigned short)` compares origin and
current generation. `int WFE_TLSInit(void)` / `void WFE_TLSShutdown(void)` initialize and
tear down native services. Define `void NETTLS_Configure(NETTLSTrust *, int allow_legacy)`
to atomically publish a retained store/policy snapshot and increment generation.
Keep socket operations injectable: `void NETTLS_SocketsInit(const NETTLSSocketOps *)`
copies callbacks for `int recv(NETTLSSocket, void *, size_t)`,
`int send(NETTLSSocket, const void *, size_t)`, `int close(NETTLSSocket)`,
`void set_error(int native_error)`, and the Task 3 entropy/time signatures.
Include explicit native would-block/fatal error codes in this operations struct.
Only `tls-platform.c` includes Windows headers or calls Windows APIs; the shared
registry invokes supplied callbacks. Native I/O callbacks translate to Task 3's
byte/EOF/-2/-1 convention; socket wrappers restore native errors for netlib.

- [ ] Add plaintext pass-through, secure attachment, attach-twice rejection,
  close-once and descriptor-reuse tests. Assert a new socket with the reused
  number has no prior authenticated state. Run; expect missing adapter failures.
- [ ] Implement the registry and callbacks; native Winsock calls bypass adapter
  macros to avoid recursion. Use `BCryptGenRandom` with the system-preferred RNG
  and link `bcrypt`; fail closed on its failure. Avoid changing other NSPR RNGs.
- [ ] Resolve default bundle relative to the executable: installed
  `../share/netscape/ca/cacert.pem`; copy the same relative data layout into the
  build output for development. Never search the working directory. Missing CA
  data disables HTTPS with an error while ordinary HTTP remains usable.
- [ ] Hook initialization/shutdown after Winsock setup and before its cleanup;
  release registry entries and trust stores even after failed init or canceled loads.
- [ ] Run `ctest --test-dir out/bearssl-win32 -C Release -R '^(tls_socket|windows_core_smoke|windows_socket_smoke)' --output-on-failure`;
  expect secure lifecycle and existing native socket tests passing. Commit as
  `feat: connect BearSSL transport to native Windows sockets`.

### Task 5: HTTPS, CONNECT and event-loop integration

**Files:** `xp_sock.h`, `wintcp.h`, `mkconect.c`, `mkhttp.c`, `mkgeturl.c`,
`net.h`, `platform.c`, `main.cpp`, `tls-smoke.inc`, `check_windows_tls.py`.

**Interfaces:** Under `NETSCAPE_USE_BEARSSL`, route read/write/close through
Task 4 while other socket calls remain native. Add fixed `NETTLSInfo` request
metadata through an adapter-neutral header (no BearSSL type in `net.h`).
Produce smoke CLI `--tls-smoke-test <report> <url>`; report fields include
`completed`, `status`, `tls_authenticated`, `tls_version`, `tls_cipher`,
`tls_error`, `body_sha256`, `requests_completed`, `ui_responsive`.
Never report secret keys or payload contents in TLS diagnostics.

- [ ] Add browser fixtures for TLS 1.2 document/subresource, cross-host redirect,
  HTTPS download bytes and CONNECT relay. Assert authenticated version `0x0303`,
  expected rendered title and payload hash, target SNI and no HTTP request at
  the peer when authentication fails. Initially expect unsupported HTTPS failure.
- [ ] Audit `NO_SSL`, `HAVE_SECURITY` and `NADA_VERSION` paths in affected netlib
  code. Keep old security disabled; add explicit new-backend branches for HTTPS
  setup/status. Do not resurrect old cert/nav APIs, secure news or S/MIME.
  If `NADA_VERSION` rewrites HTTPS to an HTTP help page, suppress that behavior
  only for the new backend. With TLS off, reject HTTPS before sending plaintext.
- [ ] Attach TLS once TCP connects for direct HTTPS, and only after successful
  CONNECT for proxied HTTPS. Preserve any bytes already read beyond the CONNECT
  headers by feeding them to TLS rather than discarding them. Verify destination
  host/port, including after redirects; gate keep-alive by Task 4's reuse check.
- [ ] Make the poller honor TLS network wants instead of blindly mapping logical
  reads to Winsock reads. Schedule LOCAL progress for buffered plaintext and
  drain pending TLS output with fairness budgets. Make teardown safe under
  callbacks that cancel other loads. Do not busy-poll every open TLS connection.
- [ ] Add no-handshake-progress timeout, CONNECT rejection, split CONNECT headers,
  canceled/reused descriptor, interleaved HTTP/HTTPS, concurrent loads, partial
  writes, keep-alive, fatal-alert-after-data and truncated-response cases.
  Ensure new TLS failures propagate to request completion even after HTTP headers.
  A complete Content-Length alone must not hide an already observed fatal alert.
- [ ] Run `python cmake/tests/check_windows_tls.py --executable out/bearssl-win32/Release/netscape.exe --output out/bearssl-win32/tls-browser --mode browser`;
  expect explicit per-case passes, title/hash matches and responsive UI.
  Run existing HTTP, downloads, images and forms CTests. Commit as
  `feat: enable authenticated HTTPS in Windows netlib`.

### Task 6: Transactional native TLS preferences

**Files:** Native preference files/resources, `tls-platform.c`, `check_windows_preferences.py`, `check_windows_tls.py`.

**Interfaces:** Add `bool allow_legacy_tls = false` and `std::string ca_bundle`
to `GeneralPreferences`. Persist profile keys `allow_legacy_tls` and `ca_bundle`.
Expose `int WFE_PreferenceAllowLegacyTLS(void)` and
`const char *WFE_PreferenceCABundle(void)` for platform initialization.
Publish policy through Task 4's `NETTLS_Configure`; bundle changes take effect
on restart as specified. New protocol policy applies immediately to new sockets.

- [ ] Extend preference tests: absent/malformed legacy boolean defaults false,
  save/reopen persists it, Cancel changes nothing, failed Save changes neither
  active policy nor persisted data. Assert default bundle override is empty.
- [ ] Add native checkbox `Allow obsolete TLS 1.0 and TLS 1.1` and a CA-file path
  field with restart note in the existing preferences dialog style. Validate
  explicit absolute override paths and reject invalid files on Save; no silently
  replacing an invalid override with the public default.
- [ ] Retire idle connections from older policy generations while preserving the
  snapshot for active loads. Add `policy_generation` tests: an active legacy
  load can finish after disabling compatibility; a subsequent legacy connection
  fails; an idle legacy connection cannot be reused by the subsequent load.
- [ ] Run preference CTests plus `check_windows_tls.py ... --mode policy` using
  the same executable/output flags as Task 5. Assert TLS 1.0 and 1.1 each fail
  by default and succeed only with the setting, while bad certificates fail in
  both modes. Commit as `feat: add explicit legacy TLS and CA preferences`.

### Task 7: User-visible errors and connection details

**Files:** `mkhttp.c`, `mkgeturl.c`, `frontend.h`, `main.cpp`, `tls-smoke.inc`, `check_windows_tls.py`.

**Interfaces:** Consume `NETTLSInfo` and existing `URL_Struct.security_on` /
`key_cipher` fields; populate them only after authentication. Reuse existing
status/document information paths with a TLS version/cipher line. Keep legacy
`certificate` NULL for BearSSL; no fake certificate conversion.

- [ ] Write assertions distinguishing unknown root, wrong hostname, invalid date,
  missing/malformed bundle, handshake failure, truncation and cancellation. Test
  navigating from authenticated HTTPS to HTTP or failed HTTPS clears old status.
- [ ] Map BearSSL/native errors into stable categories and concise messages.
  Preserve useful DNS/TCP/proxy errors rather than relabeling everything as TLS.
  Expose protocol/cipher in connection details; ensure cached/history content is
  not represented as a newly authenticated live connection without evidence.
- [ ] Verify real native windows and capture representative success, certificate
  failure and preference states. Use the existing computer-use workflow for
  visual verification when available; screenshots alone do not prove networking.
- [ ] Run `check_windows_tls.py ... --mode errors` and relevant history/cache tests;
  expect distinct stable categories and no stale authentication state. Commit as
  `feat: report HTTPS authentication and transport failures`.

### Task 8: Installed validation, regression matrix and handoff

**Files:** CMake install/test wiring, `check_windows_tls.py`, `cmake/windows.md`, `README.md`, dependency/bundle provenance docs.

**Interfaces:** Register CTests `windows_tls_browser`, `windows_tls_policy`,
`windows_tls_errors`, `windows_tls_installed`; installed checker accepts
`--executable <installed exe> --output <dir> --mode installed`. Missing legacy
fixture capability must be visibly reported and prevent a full-support success claim.

- [ ] Install to `out/bearssl installed 100%`; run from an unrelated working
  directory. Verify bundled roots, explicit override, paths with spaces and `%`,
  missing/malformed default, and no accidental fallback to original-checkout data.
  Validate explicit TLS-disabled build rejects HTTPS before any HTTP payload.
- [ ] Build and run the full existing and new native CTest suite in Release and
  Debug. Configure Gold separately with `NETSCAPE_BUILD_WINDOWS_EDITOR=ON` and
  verify browser HTTPS plus existing editor/network regressions. Re-run failing
  cases after fixes; do not claim Linux compatibility from C source inspection.
- [ ] Run at least two public TLS 1.2-capable HTTPS sites as supplemental checks;
  record date, endpoint, negotiated protocol and fetch/render outcomes separately.
  Treat network restrictions as unverified public checks, never as local TLS passes.
- [ ] Document exact build prerequisites, pinned versions/hashes, CA replacement
  and restart behavior, compatibility setting, cipher policy, no revocation,
  unsupported identities and TLS 1.3 deferral. Record commands actually run and
  remaining limits. Commit as `test: verify installed BearSSL HTTPS workflows`.
- [ ] Perform a whole-branch review against the approved design and resolve
  findings. Keep work in its branch/worktree for user review; do not merge or
  delete the worktree automatically.

```powershell
cmake --build out/bearssl-win32 --config Release --parallel
ctest --test-dir out/bearssl-win32 -C Release --output-on-failure
cmake --build out/bearssl-win32 --config Debug --parallel
ctest --test-dir out/bearssl-win32 -C Debug --output-on-failure
cmake --install out/bearssl-win32 --config Release --prefix "out/bearssl installed 100%"
python cmake/tests/check_windows_tls.py --executable "out/bearssl installed 100%/bin/netscape.exe" --output out/bearssl-win32/tls-installed --mode installed
```

## Planning self-review

Spec coverage: Tasks 1/8 own dependency/build/package provenance; 2 owns trust;
3 owns protocols, entropy and certificate verification; 4 owns socket lifecycle;
5 owns HTTP/CONNECT/readiness and regressions; 6 owns policy; 7 owns status/errors.
All five Review Focus cases have tests assigned above. No implementation tests
have run while preparing this plan. Worktree creation and baseline validation
are deliberately execution steps, not claims of completed work.
