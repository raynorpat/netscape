# CMake build

The root `CMakeLists.txt` loads archive and executable definitions from `cmake/targets/`. Shared platform configuration, dependency discovery, Sun Java source targets and optional archive imports, and generation helpers live in `cmake/modules/`.

## Linux browser

The default configuration builds `netscape-export`, the legacy core and hash archives, the JPEG decoder, DBM, and the icon/header generators. SSL/S-MIME defaults to disabled, matching the original `config/myconfig.mk`; the additional security archives remain available as explicit targets. Linux x86, x86_64, and AArch64 are recognized. Source compatibility and runtime behavior still depend on the toolchain; recognizing an architecture does not certify a working browser on it.

Install a C compiler, CMake 3.20 or newer, Python 3, Perl, gperf, a POSIX shell and text utilities, zlib development files, and X11/Motif development libraries (Xt, Xmu, Xpm, Xext, SM, ICE, Xm). Configure and build out of tree:

```sh
cmake -S . -B out/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build out/cmake --parallel
```

Set `CMAKE_PREFIX_PATH` to a dependency prefix when libraries are outside the system search paths. The build-tree executable is `out/cmake/netscape-export`. Installation provides public `bin/netscape` and `bin/netscape-export` launchers and a private executable under `libexec/netscape/`. Browser, mail and news are part of this single XFE executable. Mail/news can also be opened with `netscape mailbox:` and `netscape news:`; desktop actions provide the same entry points.

## Options

| Option | Default | Purpose |
| --- | --- | --- |
| `NETSCAPE_BUILD_BROWSER` | `ON` | Build the Linux XFE browser and GUI helper archives. |
| `NETSCAPE_BUILD_LEGACY` | Browser option | Build core, networking, layout, mail, security, and NSPR archives; requires X11/Motif headers. |
| `NETSCAPE_BUILD_MOCHA_SHELL` | `OFF` | Build the file-input shell; requires Mocha and enables its `MOCHAFILE` interface. |
| `NETSCAPE_ENABLE_SECURITY` | `OFF` | Enable legacy SSL/S-MIME and link the additional security archives. |
| `NETSCAPE_ENABLE_MOCHA` | `OFF` | Add the Mocha engine, browser bindings, layout sources. |
| `NETSCAPE_ENABLE_JAVA` | `OFF` | Add Java browser bindings with a source or imported runtime provider. |
| `NETSCAPE_BUILD_SUN_JAVA` | `OFF` | Build standalone Sun Java source targets. |
| `NETSCAPE_JAVA_PROVIDER` | `SOURCE` | Select `SOURCE` or `IMPORTED` when browser Java is enabled. |
| `NETSCAPE_JAVA_ROOT` | empty | Legacy distribution prefix containing Java `lib/` archives and `include/` headers. |

With `NETSCAPE_JAVA_PROVIDER=IMPORTED`, Java runtime archives must match the compiler and target ABI: nsn, njs, nsa, javai, javart, sunmd, awt and mmedia. CMake checks their locations when Java is enabled. The source provider and standalone Sun Java targets are documented in [sun-java.md](sun-java.md), including bootstrap rules and known source compatibility failures. The editor, alternate platform front ends, packaging variants, and legacy test suites are not selected by this migration.

To build only the independently verified JPEG and DBM archives plus the NSPR CPU header generator:

```sh
cmake -S . -B out/portable -DNETSCAPE_BUILD_BROWSER=OFF -DNETSCAPE_BUILD_LEGACY=OFF
cmake --build out/portable --parallel
```

`cmake/toolchains/linux-x86.cmake` selects a native 32-bit multilib build with `-m32`. It requires matching 32-bit C runtime and development libraries. Cross compilation is currently rejected because the NSPR CPU configuration and icon tools must run during the build.

## Generation and dependencies

Generated files stay in the build directory. NSPR's original `gencfg` produces `generated/nspr/prcpucfg.h`; exported NSPR/Mocha headers are staged there without using the checked-in `dist/*.OBJ` artifacts. MIME types, license data, e-kit data and fallback resources are regenerated when their inputs change. `netscape_xfe_resources` rebuilds US about/splash pages, HTML/message headers and embedded image/audio headers from their original inputs. `netscape_parser_tables` uses gperf and `pa_tags.h` with `cmake/templates/pa_hash.c.in` to regenerate lookup/reverse tables while retaining the branch's STYLE alias. `netscape_security_resources` regenerates the security dialog and shared `allxpstr.h` using the original conversion scripts and Perl. These generated headers are actually consumed by staged translation units, rather than shadowed by checked-in copies. `mkicons` links image/JPEG/XP archives and generates icon data from the ordered GIF inputs. Java's build-number header is derived from `build/build_number` when enabled.

The browser link follows `cmd/xfe/Makefile`: XmL/HPane; xlate/net/layout/i18n/image/JPEG/misc/parser/mail/MIME/plugin/DBM; optional Java and Mocha; security archives, jar, XP and NSPR; then platform libraries. A GNU archive rescan group resolves cycles between the static archives. The alternate `libmime2` tree is excluded, matching the active root Makefile traversal. The unused `nspr-` backup and `nspr.tar.gz` archive were removed; active builds use `nspr/`.

## Source portability provenance

The Linux portability source changes were selectively imported from [9M2PJU's fork at f68b77ca9443d8b50ab58b595a1bda87ac9e3629](https://github.com/9M2PJU/netscape-communicator-3-02-for-linux/commit/f68b77ca9443d8b50ab58b595a1bda87ac9e3629), relative to the shared base `ec144cc852e4cec98ba0114663b5f270c13c4ad7`. They include NSPR ucontext/libc socket support, fixed-width NPAPI types, Motif font/constraint handling, pointer-width Xt varargs terminators and current libc declarations. The GCC attribute-version check in both `cdefs.h` copies was additionally corrected locally so GCC 15 retains the attributes required by current glibc. Generated binaries, CVS metadata, packaging and copied distribution headers were excluded.

## Verification

With GCC 15.2 and staged Ubuntu 26.04 x86_64 development packages, the baseline browser and the build with Mocha and security enabled compiled and linked successfully. The baseline executable also ran `-version`; the optional Mocha shell compiled and linked. JPEG, DBM and generated NSPR configuration had additionally been built independently. The source-Java browser with Mocha also compiled and linked after fixing variadic argument forwarding in both JRI and the Mocha bridge. GUI operation, networking, security behavior, imported Java archive integration, AArch64, and 32-bit configurations have not been verified. Compiler warnings remain in the legacy source.

## Installation and packages

```sh
cmake --build out/cmake --parallel
cmake --install out/cmake --prefix /your/prefix
cpack --config out/cmake/CPackConfig.cmake -G TGZ -B out/packages
```

Build before installing or packaging. A staged installation is supported with `DESTDIR`. Launchers derive all private/data/class paths relative to their installed location, so a TGZ can be extracted under a different prefix. Packages rely on matching system X11/Motif and C runtime libraries; they do not copy system libraries into the archive.

The runtime tree includes application defaults, `XKeysymDB`, plain localized pages/messages, a desktop file with Mail and News actions, documentation, and original bundled notices. The generated US resources are embedded too. Java-enabled/source-Java builds install the runtime class archive, private VM/`javah`/`javap` binaries and namespaced `netscape-java`, `netscape-javah` and `netscape-javap` launchers, avoiding replacement of a system Java installation. Compiler launchers are installed only when `javac.zip` is supplied via `NETSCAPE_JAVA_COMPILER_ARCHIVE` or produced by `NETSCAPE_REBUILD_JAVA_CLASSES`.

Security builds install the original signed policy archives as `policyMoz40P2` in the data directory and language subdirectories. `NETSCAPE_POLICY_VARIANT` selects `export` (default), `us` or `france`; signatures are preserved and no signing keys are needed. The launcher sets `MOZILLA_HOME` to this data directory unless the user has provided a value.

`NETSCAPE_INSTALL_DEVELOPMENT=ON` adds the built, non-excluded static archives and legacy headers. It defaults on for library-only configurations and off for browser/VM configurations. This is the legacy archive/header layout, not an exported modern CMake SDK package.

CPack defaults to TGZ. DEB and RPM metadata are configured as well:

```sh
cpack --config out/cmake/CPackConfig.cmake -G DEB
cpack --config out/cmake/CPackConfig.cmake -G RPM
cpack --config out/cmake/CPackSourceConfig.cmake -G TGZ
```

DEB requires Debian packaging tools and installed library package metadata for automatic shared-library dependencies. RPM requires `rpmbuild`. Set `CPACK_PACKAGE_CONTACT` at configure time for distributor metadata. Source archives exclude Git metadata, build output and the old distribution/object trees while retaining the shipped Java bootstrap class data.

The baseline and Java/Mocha/security configurations were built, staged and packaged as TGZ on Linux x86_64. Installed browser launchers passed `-version`, including after extraction into a relocated directory containing spaces. Java tools/class data and security policies were checked in the feature package; GUI, mail/news networking and Java execution remain unverified. DEB/RPM creation has not been verified here.

Resource tests and installed-package checks can be run explicitly:

```sh
python3 cmake/tests/test_resources.py
python3 cmake/tests/check_install.py /staged/prefix
python3 cmake/tests/check_package.py out/packages/netscape-legacy-3.02-Linux-x86_64.tar.gz
```

The resource suite checks binary/text round trips and compiles the generated parser for every tag, case variants, unknown/null inputs and the STYLE alias. Set `GPERF` to a nonstandard gperf path when needed. Add `--java --security` to the installed/package checks for those configurations.

## Remaining build coverage

Optional GNU `movemail`, sample plugin shared libraries, standalone security/diagnostic programs, CTest registration and legacy test suites remain outside the default migrated build. The browser's built-in movemail implementation is compiled already. The original `nsinstall` helper is replaced by CMake build/install operations. Nonexistent directories named in old Makefiles, such as `cmd/patcher` and `lib/libcgi`, are not missing source targets. Editor and alternate OS backends remain outside the Linux configuration.
