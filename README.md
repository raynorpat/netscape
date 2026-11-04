# Netscape source tree

This repository contains a legacy Netscape browser source tree. It includes the browser front end, shared libraries, Java and JavaScript components, networking support, security code, localization files, and build configuration.

The top-level `Makefile` includes `config/config.mk` and builds the source directories listed there. Its primary browser targets include `netscape-export` and `netscape-export.pure`. This is a historical build system: it expects a compatible Unix-like environment and the project’s original toolchain and configuration. A modern compiler or operating system may not be compatible without additional work.

## Repository layout

- `cmd/` — browser and other command-line applications, including the XFE front end.
- `config/` — shared make rules and platform configuration.
- `include/` — public and shared headers.
- `lib/` — browser libraries for layout, parsing, images, mail, networking, Java, and related services.
- `nspr/`, `nspr-/` — Netscape Portable Runtime sources and related tree.
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

The browser requires Linux, Python 3, POSIX text utilities, and zlib/X11/Motif development libraries. Java and Mocha default to disabled. See the CMake guide for library-only builds, dependency prefixes, Sun Java source targets and imported-runtime mode, and verification limits.

## Historical context

This is legacy software, preserved as source code. Use it for archival, research, and carefully scoped maintenance work. Do not assume it is suitable for secure or routine web browsing on a current system.
