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

Start by reviewing `config/README` (if present) and the platform configuration under `config/` to determine whether this source tree supports your host. The top-level build uses `gmake` and the project’s historical make rules. Build configuration and required tools vary by platform; there is no single modern, portable build command guaranteed to work out of the box.

For the browser export target, the top-level makefile defines:

```sh
make netscape-export
```

Run it only after configuring the tree for a supported platform. The build may require platform-specific dependencies and patches.

## Historical context

This is legacy software, preserved as source code. Use it for archival, research, and carefully scoped maintenance work. Do not assume it is suitable for secure or routine web browsing on a current system.
