# BearSSL HTTPS native verification - 2026-11-04

Final validation used reviewed source commit `4a3a1a3` on Windows, after the
certificate-purpose correction. Task 8 started at `8eb3e63`; `f3a4641` contains
installed/off-build preflight checks and fixes, while `4a3a1a3` is the separately
reviewed portable purpose guard. The final builds and matrices below all exited 0.
No source changes were needed during the final run.

## Environment and provenance

- Windows 11 IoT Enterprise LTSC 10.0.26100 x64 host; native Win32/x86 targets.
  CMake 4.4.4, Visual Studio 18 2026 Professional, MSVC 19.51 (14.51.36231),
  matching x86 MFC/shared runtime and Windows SDK 10.0.26100.
- Python 3.13.9, cryptography 46.0.3, Python ssl OpenSSL 3.0.18; fixture CLI
  OpenSSL 3.5.6 at `C:/Users/raynorpat/AppData/Local/Programs/Git/mingw64/bin/openssl.exe`.
  Fixture OpenSSL is neither linked nor installed with Netscape.
- BearSSL `7bea48e5e850ab4cafbe68d3765cdaba13a86d6f`, import archive SHA-256
  `ed397832b1613582737d9c1ce01c1af74ad6a9e9c52a341e24433987cda0f372`.
  Preflight compared all 319 imported files byte for byte with the extracted
  official archive: zero differences. Later changes are adapter-only.
- curl/Mozilla CA Extract 2026-10-20: 118 roots, SHA-256
  `796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe`.
  Runtime tests verify this hash/count, the full BearSSL pin, MIT license hash,
  and retained BearSSL, Mozilla MPL and WinFE notices/provenance.

## Final matrix

All commands ran from the isolated implementation worktree. Builds used
`--parallel 8`; native UI suites ran sequentially. The final matrix ran on
2026-11-04, 22:32-22:47 EST (2026-11-05, 03:32-03:47 UTC).

| Configuration/check | Result | CTest duration |
| --- | --- | --- |
| Standard Release, complete configured suite | 118/118 passed | 245.30 s |
| Standard Debug, complete configured suite | 118/118 passed | 255.37 s |
| Separate Gold Release, editor/network/HTTPS selection | 17/17 passed | 125.09 s |
| Separate TLS-disabled Release, complete configured suite | 24/24 passed | 102.24 s |
| Explicit installed Release prefix, independent runtime checker | 13/13 cases plus bundle/notices passed | not a CTest invocation |

Both standard suites include the legacy-peer prerequisite, all purpose/engine/
trust/socket tests, 20 DER/streaming purpose subcases, browser (27), policy (27),
native displayed errors (23) and installed (13). Gold includes its editor, workflow
and publishing fixtures, ordinary browser/network regressions, all four native
TLS groups and its installed Runtime check. TLS-off proves negative HTTPS
completion before any TCP connection/plaintext, while ordinary HTTP still renders.
The prerequisite fails visibly if real legacy TLS 1.0/1.1 fixture capability is
missing; a missing fixture is never a full-support pass.

Actual final commands:

```powershell
cmake --build out/bearssl-win32 --config Release --parallel 8
cmake --build out/bearssl-win32 --config Debug --parallel 8
cmake --build out/bearssl-gold --config Release --parallel 8
cmake --build out/bearssl-disabled --config Release --parallel 8
ctest --test-dir out/bearssl-win32 -C Release --output-on-failure
ctest --test-dir out/bearssl-win32 -C Debug --output-on-failure
ctest --test-dir out/bearssl-gold -C Release -R '^(windows_gold_.*|windows_frontend_.*|windows_browser_.*|windows_downloads|windows_tls_.*|windows_supporting_applications)$' --output-on-failure
ctest --test-dir out/bearssl-disabled -C Release --output-on-failure
cmake --install out/bearssl-win32 --config Release --prefix "out/bearssl installed 100%"
python cmake/tests/check_windows_tls.py --executable "out/bearssl installed 100%/bin/netscape.exe" --output out/bearssl-win32/task8-final-installed --mode installed
python out/bearssl-win32/task8-public.py
```

The configurations were created using the VS18 2026 Win32 commands in
[the Windows guide](../../../cmake/windows.md#native-browser-https-bearssl), with
Gold editor ON and a separate explicit BearSSL-OFF directory. The enabled cache
uses the exact fixture OpenSSL path above. CMake regenerated Gold's updated
purpose targets before its final build. Each installed CTest first installs its
selected configuration's Runtime under the build directory.

Installed checks run from an unrelated CWD with trusted fixture bundles planted
in likely fallback locations. They verify executable-relative discovery after
relocation, spaces and `%`, omitted/`-` smoke CA arguments preserving startup,
absolute profile override replacement, missing/malformed default and override,
relative rejection, no merging/source/CWD fallback, and HTTP with broken trust.
Failing trust cases deliver no HTTP request to the peer. Default CA configuration
error13 is required instead of acceptance or source-bundle unknown-issuer11.
Only relocated copies are mutated; original installed roots remain byte-identical.
Native standard/Gold purpose rejection, relocated success and missing-default
Document Info captures were inspected and remain readable within their dialogs.

## Supplemental public HTTPS after the purpose fix

Fresh installed-browser profiles used the shipped bundle, no fixture CA and no
proxy. Requests ran **2026-11-04 at 22:46 EST**, equivalent to
**2026-11-05 03:46:58-03:47:01 UTC**. Each actual process used
`netscape.exe --tls-smoke-test <report> https://<host>/ - - ui`.

| Endpoint | Actual transport/fetch | Native rendering observed |
| --- | --- | --- |
| `https://example.com/` | Authenticated TLS 1.2, `0xCCA9` ECDHE ECDSA ChaCha20-Poly1305, completed response | Correct Example Domain title and readable explanatory body |
| `https://www.bearssl.org/` | Authenticated TLS 1.2, `0xC02F` ECDHE RSA AES-128-GCM, completed response | BearSSL title and readable navigation/body with scrolling |
| `https://www.cloudflare.com/` | Authenticated TLS 1.2, `0xCCA9`, completed encrypted response | Readable origin denial `error code: 1010`; not successful Cloudflare page rendering |

All three returned native status1, completed1, authenticated1, version771,
error0 and capture_ok1. Actual window captures were inspected. Fetch success is
separate from rendering; no modern HTML/CSS/JavaScript or full-site parity is
claimed. These dated public observations supplement deterministic local tests.

## Policy, limits and preflight findings

The adapter enforces EKU on every presented leaf/intermediate/root: absent,
serverAuth or anyExtendedKeyUsage is allowed; unrelated-only purpose fails with
appended error16 before application data. Configured anchor records are not
reparsed. The unchanged minimal validator's failures always win, so valid critical
EKU remains unsupported even for serverAuth. Each presented certificate is capped
at 65,536 DER bytes; one reusable buffer adds 64 KiB per live TLS context.

TLS 1.2 is the default; explicit compatibility permits TLS 1.0-1.2 under the
restricted cipher policy documented in the Windows guide. There is no TLS 1.3,
SSL2/3, client-certificate, mail-TLS/S/MIME, revocation, intermediate-download or
background-CA-update support. IP/non-ASCII DNS identities are unsupported.
CA changes require restart; overrides replace the bundle. See the guide and
[CA provenance](../../../security/ca/README.md) for exact replacement/checksum
maintenance and ANSI/MAX_PATH limits. No Linux runtime or TCP-refusal-specific
presentation claim is made. Installation/relocation was tested on this same
Windows machine, not a separate clean machine. Historical macro/type/format and
SDK-default warnings remain; no warning-free build or memory-leak audit is claimed.

Preflight evidence is separate: earlier Release83/83 preceded the purpose guard.
Initial Debug82/83 exposed a test-only large allocation, corrected to XP_HUGE;
initial TLS-off23/24 exposed a zero callback status on rejected HTTPS, now negative.
The first installed check found missing BearSSL notices, now packaged. A separate
purpose probe found clientAuth-only acceptance; `4a3a1a3` corrected it and passed
independent security review before the final matrices above. Whole-branch
acceptance review remains controller-owned after this task's review.

## Local evidence

These ignored build outputs remain in the worktree for review:

- [Exact commands, timestamps and exit codes](../../../out/bearssl-win32/task8-final-matrix.json).
- [Release build](../../../out/bearssl-win32/task8-final-release-build.log),
  [Release suite](../../../out/bearssl-win32/task8-final-release-tests.log),
  [detailed Release log](../../../out/bearssl-win32/task8-final-release-tests-LastTest.log).
- [Debug build](../../../out/bearssl-win32/task8-final-debug-build.log),
  [Debug suite](../../../out/bearssl-win32/task8-final-debug-tests.log),
  [detailed Debug log](../../../out/bearssl-win32/task8-final-debug-tests-LastTest.log).
- [Gold build](../../../out/bearssl-win32/task8-final-gold-build.log),
  [Gold suite](../../../out/bearssl-win32/task8-final-gold-tests.log).
- [TLS-off build](../../../out/bearssl-win32/task8-final-disabled-build.log),
  [TLS-off suite](../../../out/bearssl-win32/task8-final-disabled-tests.log).
- [Explicit installed checker](../../../out/bearssl-win32/task8-final-installed-manual.log),
  [public reports/ciphers/timestamps](../../../out/bearssl-win32/task8-public-final/results.json),
  [upstream byte comparison](../../../out/bearssl-win32/task8-upstream-verification.json).
- Native captures: `out/bearssl-win32/task8-public-final/<host>.txt.window.bmp`,
  `out/bearssl-win32/task8-final-installed/*.txt.info.bmp`,
  `out/bearssl-win32/tls-errors` and `out/bearssl-gold/tls-errors`.
