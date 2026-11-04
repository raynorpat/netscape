# Windows browser images and forms implementation plan

> **For agentic workers:** Use superpowers:executing-plans for inline integration and superpowers:dispatching-parallel-agents for independent adapters and fixture work. Track verified results here.

**Goal:** Replace native image and form placeholders with functional browser controls using the original 3.02 engines.

**Architecture:** Keep network, image decoding, layout, MIME, and form submission in the existing core. Add focused native adapters that own GDI resources and HWND controls, connected through the existing frontend callback table. Preserve the working browser and mail/news window registry.

**Tech stack:** Win32/x86, MFC, GDI, MSVC, CMake, Python loopback fixtures.

**Spec:** [Windows backlog](../../../cmake/windows-roadmap.md).

## Constraints

- Finish and commit mail/news before implementation of this plan.
- Preserve legacy source encoding and other platform behavior.
- Keep optional Java, Mocha and security disabled until their own native targets and runtime checks exist.
- Run native network fixtures outside sandbox socket restrictions, with isolated profiles.
- Use the original IL and LO APIs; do not introduce another browser engine.

## Review focus

- Shared image cache ownership across windows and navigation.
- Transparent masks, row orientation and channel ordering.
- Failed image/background loads must unblock text layout.
- Widget cleanup during navigation and resize; controls must follow scrolling.
- Exact successful-control form values for GET/POST and reset.

## Task 1: Inline images

Files: new `cmd/winfe/images.cpp` and `images.h`; integrate `frontend.c`, `frontend.h`, `main.cpp`, `cmake/targets/windows/frontend.cmake`; extend native browser fixture tests.

- [ ] Add failing local/HTTP fixtures checking actual rendered pixels, intrinsic/explicit dimensions, transparency, shared images, broken loads, and navigation cleanup.
- [ ] Implement IL truecolor initialization, `IL_GetImage`, size/data/delete callbacks, shared DIB ownership, clipping and element release.
- [ ] Connect native rendering, background tiling and image visibility through small `WFE_` functions.
- [ ] Run Release/Debug fixtures and existing mail/news/core regressions; inspect resource lifetime.
- [ ] Update Windows support documentation and commit verified image work.

## Task 2: HTML forms

Files: new `cmd/winfe/forms.cpp` and `forms.h`; connect frontend callbacks and view command dispatch; add `cmake/tests/check_windows_forms.py`.

- [ ] Add failing native control and loopback GET/POST tests for text, password, textarea, checkbox/radio, select, hidden values, submit and reset.
- [ ] Implement HWND lifecycle and sizing, LO data synchronization, selection/toggle/reset, activation and submission through `LO_SubmitForm`.
- [ ] Position controls with document scrolling, clean them up on document discard, and preserve browser/mail reader behavior.
- [ ] Run Release/Debug fixtures and existing regressions, review then document/commit.

## Further work

After these slices, continue the prioritized Windows backlog: frame contexts, save/download streams, history/bookmarks and printing/preferences, then separately verified plugin and optional engine targets. The backlog records available Windows AWT sources and target gaps; building archives alone does not establish application feature support.
