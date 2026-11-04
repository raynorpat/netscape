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

- [x] Add failing local/HTTP fixtures checking actual rendered pixels, intrinsic/explicit dimensions, transparency, shared images, broken loads, and navigation cleanup.
- [x] Implement IL truecolor initialization, `IL_GetImage`, size/data/delete callbacks, shared DIB ownership, clipping and element release.
- [x] Connect native rendering, background tiling and image visibility through small `WFE_` functions.
- [x] Run Release/Debug fixtures and existing mail/news/core regressions; inspect resource lifetime.
- [x] Update Windows support documentation and commit verified image work.

## Task 2: HTML forms

Files: new `cmd/winfe/forms.cpp` and `forms.h`; connect frontend callbacks and view command dispatch; add `cmake/tests/check_windows_browser_features.py`.

- [x] Add failing native control and loopback GET/POST tests for text, password, textarea, checkbox/radio, select, hidden values, submit and reset.
- [x] Implement HWND lifecycle and sizing, LO data synchronization, selection/toggle/reset, activation and submission through `LO_SubmitForm`.
- [x] Position controls with document scrolling, clean them up on document discard, and preserve browser/mail reader behavior.
- [x] Run Release/Debug fixtures and existing regressions, review then document/commit.

## Further work

After these slices, continue the prioritized Windows backlog: frame contexts, save/download streams, history/bookmarks and printing/preferences, then separately verified plugin and optional engine targets. The backlog records available Windows AWT sources and target gaps; building archives alone does not establish application feature support.

## Execution record

- Mail/news committed separately as `a74ac29` after six Release/Debug tests and a Linux browser rebuild.
- Ruling: image/form adapters were integrated in parallel with shared shell glue and committed together, because both use the same view/context lifecycle. Their independent CTests retain separate failure reports.
- Failing native baselines showed no image requests, only the address edit control, and no form submission. Integrated checks then verified actual GDI pixels and exact HTTP values.
- Review found deferred image-info context lifetime and image-click routing gaps; both were fixed and covered by cleanup/coordinate checks.
- Local file, HTTP, broken background, shared client, pending relayout, GET/POST/Enter/reset/scrolling and image-submit checks passed. Optional engines and the remaining roadmap are outside these completed slices.
