# Gold remaining workflows implementation plan

**Goal:** Finish the Gold features left disabled after the initial native editor
port: printing, publishing, templates/page creation and editor preferences.

**Architecture:** Keep EDT responsible for editable HTML and serialization, LO
responsible for document layout, and netlib responsible for uploads. Add focused
native modules beside the existing editor adapter; retain original menu IDs,
captions and artwork. Printing consumes the existing LO/GDI document view.

**Scope:** The user's instruction continues the original-engine/native-Windows
approach. This does not include the separate Java, Mocha, security or NPAPI ports.
Template/wizard creation must work offline instead of depending on retired sites.

**Constraints:** Win32/MSVC/MFC; preserve standard/Gold variants; no external
publishing during tests; isolate fixture profiles; do not store credentials in
plaintext settings. Parent owns shared integration, GUI tests and commits.

## Item 1: Printing, Page Setup and Preview

- [ ] Add a failing native smoke for multipage vector output and invalid margins.
- [ ] Implement `printing.cpp/.h`: native printer/page setup dialogs, paginated
  EMF preview, checked print/cancel paths and profile persistence.
- [ ] Add `WFE_PrintDocumentSize` and `WFE_PrintDocumentPage` adapters in main.cpp
  that paint the original LO document without mutating selection, scroll or dirty
  state. Wire browser/Mail/News/Gold commands and available-state updates.
- [ ] Verify vector records, page ordering/ranges, preview, setup cancellation,
  actual document content and state retention; run affected suites and commit.

## Item 2: Preferences, Templates and Page Wizard

- [ ] Add failing workflow tests for preference persistence/cancel and independent
  template documents. Use literal fixture HTML as the input and inspect EDT output.
- [ ] Implement `editor-preferences.cpp/.h` and `editor-create.cpp/.h`: classic
  preferences, author/autosave/save policy, template and publishing locations,
  native template/page creation and safe external source editing.
- [ ] Apply preferences at document load/new document and save; open copies as
  new documents so saving cannot overwrite the original template.
- [ ] Exercise native controls, validation/cancel, cross-process persistence,
  template independence, wizard HTML and autosave; enable menu items and commit.

## Item 3: Publishing

- [ ] Add a loopback fixture that fails while Publish is unavailable, then
  captures original netlib HTTP/FTP uploads and actual EDT HTML/assets.
- [ ] Implement `editor-publish.cpp/.h`: destination/file selection, progress,
  cancellation, checked completion, retry and safe context teardown.
- [ ] Preserve local editor address and unsaved content through failed/cancelled
  uploads. Persist nonsecret destinations only; use native credential prompts.
- [ ] Verify exact bytes, failures, cancellation/retry and editor closure; wire
  Publish/browse-location commands and commit.

## Final verification

- [ ] Compare enabled compiled menus with original resource inventories; retain
  disabled states only for facilities still unavailable in this build.
- [ ] Run Win32 standard and Gold Debug/Release regression suites, review native
  captures, verify installed executable and refresh the Gold ZIP.
- [ ] Update the Windows roadmap/reference/build guide with actual coverage and
  remaining limits, and verify the worktree is clean after item commits.

Review focus: printing must retain editor state; print ranges must not duplicate
pages; failed preference writes must not apply settings; templates must never
be overwritten by ordinary Save; cancellation/closure during network callbacks
must not dereference a destroyed editor or report a partial upload as success.
