# Original Windows 3.02 UI parity implementation plan

**Goal:** Correct observed geometry, captions, News columns/checkmarks and all
four applications' menu structure, connecting supported actions to the 3.02 core.
**Architecture:** Keep native MFC windows and original backends. Transcribe the
original MENU resources into a reviewable fixture; generate RC menus with mapped
native command IDs. Native command status comes from MSG or document state.
**Tech stack:** MSVC Win32 C++14/MFC, RC, Python fixture servers and CTest.
**Reference:** cmake/windows-reference.md and locally verified standard 3.02.

The user's latest instruction selects original-release parity over earlier
screenshot differences. Keep modern Windows decorations and narrow-screen
clamping; unsupported engines/printing remain explicitly disabled. Preserve
working MIME decoding, atomic settings and delivery cancellation/retry.

- [x] Regressions first: pin original toolbar count/order, five message columns,
  authentic checkmark pixels, initial pane sizes and composer menu actions.
- [x] Correct chrome/panes/artwork and integrate message cell/sort indices.
  Use approximately 303px folder and 203px upper lists at 96 DPI, with clamps.
- [x] Recreate browser/Mail/News menus from the resource fixture. Route existing
  commands and new MSG actions through their actual availability/check states.
  Provide source/info, Find Again, frame reload and dynamic shared Window menu.
- [x] Correct composer menus, individually visible headers, quotation paste,
  delivery mode and body Save As. Fit original 534x953 outer bounds to workarea.
- [x] Verify compiled native menu trees against the fixture and actual menu
  effects on loopback messages/documents. Run full Release/Debug suites,
  inspect real captures, install/package, update reference notes and commit.

Parallel ownership: pane worker owns mailui/chrome/artwork; composer worker owns
composer.cpp/composer.rc; menu worker owns generated browser/mail/news RC and
manifest/generator. Root owns main integration, shared Window menu, fixture
orchestration and build/test execution. GUI tests run sequentially.

Review focus: command IDs must never dispatch unrelated actions; unavailable
items must remain disabled; selection changes must not discard multi-selection;
mail/news sorts must match the clicked column; dynamic windows must not retain
destroyed HWNDs; canceled pickers and POST reflow must not send data; saved files
and attachments must retain exact bytes; small displays must keep Close and
editing controls reachable.

## Verification record

All 17 CTests passed in Release (102.32s) and Debug (107.59s) after the final
selection, source-window chrome and activation fixes. Generated resources pass
`--check`; compiled menus match all four original inventories with the two
explicit native composer extensions. Native snapshots for Mail, News, composer,
browser and 502x442 Preferences were inspected. Review findings were corrected
and exercised through real core/native state rather than resource-only checks.
The original HTML editor is the next separate milestone; no editor engine was
enabled by this parity change.
