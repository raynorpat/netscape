# Windows preferences and supporting applications

**Goal:** Complete the Colors background-image controls and Apps page with
separate verified commits, retaining the classic 3.02 layout.

**Architecture:** Extend transactional General Preferences. Apply background
files with the original LO default-backdrop/override APIs. A small native
application module owns executable validation, Windows argument quoting and
source-download completion. Reuse the existing checked download streams and
keep the internal source viewer as the default. The user's continuation and
standing commit-per-item request authorize implementation in this checkout.

## Task 1: Background image preferences

- [x] Add a failing native Colors-page fixture for enabled controls, Cancel,
  invalid/missing files, save failure, persistence and document rendering.
- [x] Persist the selected local image mode/path; enable Browse and conditional
  edit controls, preserving Default/Custom/Image radio behavior and override.
- [x] Apply LO_SetDefaultBackdrop with file URLs; make repeated core setter
  calls release the previous default. Verify user/default/page/override images,
  local paths containing spaces and percent signs, removal and reload.
- [x] Run affected native tests, update the roadmap and commit.

## Task 2: Supporting applications

- [x] Add failing native Apps-page and child-process fixtures for View Source,
  Telnet/TN3270 and the temporary directory, including Cancel/failed-save cases.
- [x] Persist validated executable paths and an existing writable temporary
  directory. Blank executable settings retain built-in behavior/unconfigured
  terminal feedback. Use CreateProcess with explicit application and quoted
  argv, never a shell command template.
- [x] Retrieve source using checked binary download completion before launch;
  preserve POST safeguards, source bytes and failed/canceled-transfer behavior.
  Route remote-host callbacks to configured terminal programs with distinct
  host/port arguments and optional username. Apply temp directory transactionally.
- [ ] Test actual argv/source bytes using a controlled child executable, run
  full standard and Gold Debug/Release suites, review native captures, update
  docs/packages and commit.

## Review focus

- Cancel or a failed write must not change active settings or launch a program.
- Background images must obey original page background/override semantics and
  must not become saved editor document content.
- Source applications launch only after successful complete downloads, and
  separate browser closure must not expose a dangling context.
- Paths/arguments with spaces, quotes and percent signs remain separate argv;
  executable settings cannot inject shell commands or terminal options.
- A changed temp directory must keep existing active files/jobs valid; startup
  with a missing directory must retain a usable profile-local fallback.

MIME helper association editing and disk-cache controls remain separate work.
Native GUI fixtures run serially. Final review uses one read-only reviewer;
findings are reproduced and fixed before the final commit.

Task 1 verification: standard Debug 22/22 (97.26s), Gold Debug background,
preferences and editor 3/3 (19.86s). Actual background pixels, restart/fallback,
Colors capture and saved Gold HTML were inspected. Final Release verification
follows Task 2.

Task 2 verification before the shared gray fix: standard Debug 23/23 passed
(113.11s). Review found long staging paths and truncated terminal usernames;
new native regressions reproduced both. Short atomic staging names and the
Windows username parser fix pass the Apps/download fixtures (15.43s), followed
by the maximal-directory and encoded-invalid-login cases. External source uses
a fresh raw GET after a colored internal source view; a cached presentation
can otherwise stall the legacy source-copy stream. Final full configurations
will run after the requested shared gray-color item.

## Task 3: Native Windows palette (user correction)

The user superseded the fixed #C0C0C0 request with native Windows colors to
match the Gold release and reduce the contrast with window borders. The pending
pixel-remapping implementation was removed before commit.

- [x] Reproduce the toolbar mismatch with a COLOR_BTNFACE pixel regression.
- [x] Use live system face/text/brush colors, native bevels and native control
  theming across shared browser, Mail/News, message composer, Gold and utilities.
- [x] Keep the original bitmap transparency key independent of the UI color;
  preserve document colors. Replace fixed-gray preference assertions with
  native palette checks.
- [x] Verify standard and Gold native captures/workflows, update docs and commit.

Task 3 verification: the toolbar regression failed against the fixed-gray build.
The standard Debug chrome, Mail/News layouts, message composition and Preferences
fixtures then passed 5/5 (34.50s) after the native-palette change.
Gold Debug editor, workflows and Preferences passed 3/3 (9.71s), with native
Preferences captures reviewed. Final full suites follow the icon update.

## Task 4: Original Gold icons (user follow-up)

- [ ] Compare current assets with the original Gold 3.02 executable resources.
- [ ] Replace mismatched toolbar and window icons with mechanically extracted
  resources, retaining provenance, original pixels and explicit transparency.
- [ ] Verify native captures and complete standard/Gold Debug/Release suites,
  refresh packages and commit the artwork separately.
