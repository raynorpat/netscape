# Mail/News reference parity and first-run welcome

**Goal:** Match the original Gold Mail/News controls in the supplied screenshots
and restore the built-in Mozilla Welcome message for fresh mail profiles.

**Architecture:** Keep native Windows colors and the original MSG/MIME/LO
engines. Correct Mail/News-specific chrome and column defaults without changing
the browser geometry. Restore FE_AboutData("mailintro") using the original
Windows payload. Let msg_EnsureInboxExists perform original first-run selection
and reading; use a Windows atomic, exclusive file-creation adapter so existing
mail is never overwritten. Default folders are created only when needed.

## Item 1: Mail/News UI

- [x] Add failing checks for 41x41 toolbar buttons, 8px groups, 53px chrome,
  Mail's 333px folder pane, News's 303px pane, fixed Subject/expanding Date and
  folder numeric-column widths.
- [x] Correct toolbar/pane/header geometry, native tree folder/article artwork
  and indentation, preserving splitters, sorting, keyboard and subscription UI.
- [x] Run native Mail/News tests, inspect captures, document and commit.

## Item 2: Original Mozilla Welcome message

- [x] Recover and retain the Windows Gold welcome payload and provenance;
  connect the owned mailintro callback and remove the empty Inbox precreation.
- [x] Add checked atomic first-run Inbox creation through the existing core
  path, preserving existing/empty mailboxes and failed-save/retry behavior.
- [x] Test fresh profile, automatic reader selection, exact headers/body,
  restart/idempotence, existing mail preservation and failure cases. Keep
  native tests offline while preserving the message's historical URLs.
- [x] Run complete standard/Gold Debug/Release suites, document results and
  commit the welcome item separately.
- [ ] Refresh and verify Release ZIPs after the composition-toolbar follow-up.

The screenshots establish default geometry at the observed desktop scale;
broader DPI behavior and unrelated mail features are separate verification work.

UI evidence: initial Mail/News geometry tests failed against 40x40 buttons,
51px chrome and an expanding Subject/fixed Date column. Corrected captures
match the supplied reference widths. Pixel comparison identifies original
News host bitmap 702 and article bitmap 723 with all 78/109 ink pixels matching,
respectively. Mail now preserves the backend's real wasted-space status.
The default Date header indicator follows the actual core sort key; Mail's
Windows default is set at initialization, avoiding interruption of News LIST.
Gold Debug targeted artwork, mail/news, empty-news, chrome, Mail/News layout and
composition checks pass 7/7 (58.09s). Full configurations follow the welcome item.

Welcome evidence: the initial native regression found zero messages and five
eagerly created folders. The restored core path creates Inbox/Sent, selects and
renders Mozilla Welcome, and preserves the exact Windows payload on restart.
Existing empty/user Inboxes remain unchanged. Blocking-parent, all-name
collision, retry, existing-target and failed-rename cleanup tests pass. About
buffers return to zero; the built-in logo is decoded without remote requests.
Gold complete suites pass 28/28 Debug (144.56s) and Release (157.46s).
Standard complete suites pass 25/25 Debug (131.79s) and Release (110.08s).

## Item 3: Composition toolbar screenshot follow-up

The user's original composition capture establishes 41x41 buttons at (5,3),
8px gaps before Quote and Stop, and a 32px framed logo at top 1px/right 6px.
Keep the existing original bitmap 755 glyphs, captions, native palette and
backend enablement; correct the independent composer geometry. Retain header,
attachment, quotation/Stop and delivery behavior. Record all five native button
bounds and logo bounds/style, reproduce the mismatch, then verify affected
composition/delivery checks in both editions/configurations and commit separately.
