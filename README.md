# Netscape source tree

This repository contains a legacy Netscape source tree. Its single XFE executable includes the browser, mail and news clients. It includes the browser front end, shared libraries, Java and JavaScript components, networking support, security code, localization files, and build configuration.

The top-level `Makefile` includes `config/config.mk` and builds the source directories listed there. Its primary browser targets include `netscape-export` and `netscape-export.pure`. This is a historical build system: it expects a compatible Unix-like environment and the project’s original toolchain and configuration. A modern compiler or operating system may not be compatible without additional work.

## Repository layout

- `cmd/` — browser and other command-line applications, including the XFE front end.
- `config/` — shared make rules and platform configuration.
- `include/` — public and shared headers.
- `lib/` — browser libraries for layout, parsing, images, mail, networking, Java, and related services.
- `nspr/` — Netscape Portable Runtime sources.
- `sun-java/`, `mocha/` — Java and JavaScript implementation sources.
- `security/` — security libraries, policy files, tools, and tests.
- `jpeg/` — JPEG support.
- `l10n/` — localization resources.
- `dist/`, `build/` — distribution and build-related files.

`README.jwz` documents a set of personal changes to an older 3.02 plus S/MIME branch. Treat it as notes about those changes, not as a complete description of every file in this repository.

## Building

The modular CMake build is described in [cmake/README.md](cmake/README.md). It defines the Linux XFE browser, core and security archives, generated resources, and optional Mocha and Java integration. The original Makefiles remain available as the historical build reference.

```sh
cmake -S . -B out/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build out/cmake --parallel
```

The browser requires Linux, Python 3, Perl, gperf, POSIX text utilities, and zlib/X11/Motif development libraries. Java and Mocha default to disabled. See the CMake guide for library-only builds, dependency prefixes, Sun Java source targets and imported-runtime mode, and verification limits.

Native Win32 core libraries, NSPR and MFC browser/mail/news windows are available,
including image rendering, HTML forms, message reading and composition. The
optional `NETSCAPE_BUILD_WINDOWS_EDITOR=ON` configuration adds the original Gold
HTML editing engine and a native editor opened with `--edit [path]`; use a
separate `out/windows-gold` build directory. Native print/preview, Editor
Preferences, independent templates, an offline wizard and HTTP/FTP publishing
are available. Debug/Release regression and installed-workflow checks are
recorded in the Windows guide.
See [cmake/windows.md](cmake/windows.md) for MSVC build commands, runtime tests,
the recovered 1998 Windows frontend source, and remaining integration work.

Install or package the build with:

```sh
cmake --install out/cmake --prefix /your/prefix
cpack --config out/cmake/CPackConfig.cmake -G TGZ -B out/packages
```

The installed `netscape` launcher opens the browser; `netscape mailbox:` and `netscape news:` open mail and news. See the CMake guide for the relocatable layout, Java tools, optional policy data and other package formats.

## Historical context

This is legacy software, preserved as source code. Use it for archival, research, and carefully scoped maintenance work. Do not assume it is suitable for secure or routine web browsing on a current system.
