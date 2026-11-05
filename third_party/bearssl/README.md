# BearSSL dependency

## Pin and provenance

- Upstream repository: <https://www.bearssl.org/git/BearSSL>
- Full revision: `7bea48e5e850ab4cafbe68d3765cdaba13a86d6f`
- Revision date: 2026-04-06; inspected on 2026-11-04.
- Source archive: <https://www.bearssl.org/gitweb/?p=BearSSL;a=snapshot;h=7bea48e5e850ab4cafbe68d3765cdaba13a86d6f;sf=tgz>
- Archive SHA-256: `ed397832b1613582737d9c1ce01c1af74ad6a9e9c52a341e24433987cda0f372`
- Archive root directory: `BearSSL-7bea48e/`.
- License: MIT, retained in `upstream/LICENSE.txt` and the upstream files.

The `upstream/inc/` and `upstream/src/` directories, `upstream/LICENSE.txt`
and `upstream/README.txt` are copied byte for byte from that archive. There are
no local patches. Git attributes preserve their original bytes on Windows.
The archive is only an import/update input: CMake uses the checked-in files and
never downloads dependencies during configuration or compilation.

To reproduce an import, download the full-revision archive, verify its SHA-256,
extract it, and copy only the four paths above into `upstream/`. Compare all
imported file hashes with the extracted originals before committing. Review
upstream changes and rerun the native tests for every future pin update.

## Changes inspected since 0.6

The [upstream history](https://www.bearssl.org/gitweb/?p=BearSSL;a=log;h=7bea48e5e850ab4cafbe68d3765cdaba13a86d6f)
was reviewed back through the 0.6 release commit
`8ef7680081c61b486622f2d983c0d3d21e83caad`. Important intervening changes include:

- Private-key decoder buffer-overflow and maximum-size RSA buffer corrections.
- P-256 m62/m64 carry propagation fixes and elliptic-curve point length handling.
- RDRAND workaround for affected AMD CPUs and Unix getrandom/getentropy support.
- RSA-PSS verification correction when hash and salt lengths differ.
- X.509 date-range callbacks and UTF-8 codepoint handling corrections.
- Discarding unread application data on explicit close, and decoder error handling.
- The 2026-04-06 invalid incoming-record-length fix for CBC implementations.
- New 64-bit elliptic-curve implementations, standalone RSA-PSS and SHAKE,
  client static-ECDH selection, build dependencies and compiler/documentation fixes.

This pin includes these fixes instead of importing the unmodified 2018 release.
Library capabilities are broader than the Netscape HTTPS transport policy;
building an algorithm does not enable it in the browser's TLS policy.

## Build selection and exclusions

`NETSCAPE_ENABLE_BEARSSL_HTTPS` defaults to ON when configuring the Windows
frontend and OFF otherwise. It may be explicitly enabled for a native Win32
core-only build. It requires the native Windows core and is mutually exclusive
with `NETSCAPE_ENABLE_SECURITY`, which still selects the historical SSL/S-MIME
implementation. The new selection defines `NETSCAPE_USE_BEARSSL`; it preserves
`NO_SSL`, which controls legacy interfaces. The native frontend uses the separate Netscape TLS adapter for HTTPS;
upstream files remain unchanged.

`netscape_bearssl` is a static archive of the unmodified upstream `src/**/*.c`
files. It has its own includes and does not inherit Netscape's platform macros.
Upstream compiler/CPU detection and optional intrinsic implementations remain
unchanged. Windows CryptoAPI linkage supplies upstream's system RNG symbols.
The library is a private netlib dependency and is not installed as a public SDK.

The import excludes upstream `build/`, `conf/`, `mk/`, `samples/`, `test/`,
`tools/`, `T0/`, `.gitignore`, `Doxyfile`, `Makefile` and `T0Comp.exe`.
These are upstream build scripts/configurations, documentation configuration,
command-line tools, tests, samples and the T0 generator. Generated C sources in
`src/` are retained, so the C# generator is unnecessary. No upstream `src/`
platform file is excluded or modified. Only the static C library and the
Netscape adapter/transport tests are built; upstream tools/tests are not built.

For the supported frontend configuration:

```powershell
cmake -S . -B out/bearssl-win32 -G "Visual Studio 18 2026" -A Win32 -DNETSCAPE_BUILD_WINDOWS_FRONTEND=ON -DNETSCAPE_ENABLE_BEARSSL_HTTPS=ON
cmake --build out/bearssl-win32 --config Release --parallel
ctest --test-dir out/bearssl-win32 -C Release -R '^tls_dependency$' --output-on-failure
```

Use a separate directory for the explicitly disabled configuration:

```powershell
cmake -S . -B out/bearssl-disabled -G "Visual Studio 18 2026" -A Win32 -DNETSCAPE_BUILD_WINDOWS_FRONTEND=ON -DNETSCAPE_ENABLE_BEARSSL_HTTPS=OFF
cmake --build out/bearssl-disabled --config Release --parallel
```

Use an installed compatible Visual Studio generator if needed. These commands
select native x86; they do not claim a supported Linux TLS runtime.


The Runtime install includes `share/doc/netscape/BearSSL-provenance.md` and
`share/doc/netscape/licenses/BearSSL-MIT.txt`. Netscape links the static library;
no OpenSSL DLL or fixture executable is part of the installed TLS backend.
See [native browser policy and verification](../../cmake/windows.md#native-browser-https-bearssl)
for the enabled protocols, suites, trust settings and tested platform.


The adapter adds a presented-chain Extended Key Usage purpose guard outside
upstream code. Absent EKU or serverAuth/anyExtendedKeyUsage is permitted;
unrelated-only EKU is rejected with appended error category 16. Underlying
validation failures retain precedence, so critical EKU remains unsupported even
for serverAuth. A fixed 65,536-byte presented-certificate buffer is reused per
live TLS context, adding 64 KiB per context and imposing that per-certificate cap.
Configured trust anchor records are not reparsed by the presented-chain guard.
The guard changes no upstream files and supplies no independent PKIX acceptance.
