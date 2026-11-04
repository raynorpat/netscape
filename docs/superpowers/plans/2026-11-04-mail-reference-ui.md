# Mail and composition reference UI plan

> **For agentic workers:** Use parallel agents for the independent pane helper,
> composer and fixture; coordinate shared main/chrome/resource integration.

**Goal:** Match the two user-supplied Netscape Mail and Message Composition
references while preserving the existing 3.02 mail/news backend.

**Design:** Mail has a single compact icon/text toolbar and logo, its own
File/Edit/View/Message/Go/Options/Window/Help menu, a short upper strip containing
folder and message lists, and a full-width reader underneath. Add real movable
dividers, folder/envelope icons, Sender/status/Subject/Date columns, selected
message caption and mailbox status. Browser Location/directory bars do not
appear in mail. News shares this arrangement with appropriate commands.

Composition uses File/Edit/View/Options/Window menus, a Send/Quote/Attach/Address/
Stop toolbar and logo, compact Mail To/Cc/Subject/Attachment rows, a large plain
text editor, and segmented status. Draft, Send Later and advanced headers stay
available through menus. Existing send/cancel/draft/attachment ownership remains
authoritative. Unavailable features stay visibly disabled.

Keep native MFC windows, classic fonts/colors and original source artwork;
Windows continues to draw nonclient decorations. The screenshots are the
approved visual direction within the continuing native UI work, with version
3.02 retained. Reuse the existing renderer and MIME header output.

## Tasks

- [x] Establish failing mail/composer geometry, chrome and command fixtures.
- [x] Add mail pane/icon/column helper without backend ownership changes.
- [x] Integrate mail-specific toolbar, menus, navigation, title and status.
- [x] Match compact composer controls, menus and toolbar; preserve hidden headers.
- [x] Check actual populated-message captures and resize/selection behavior.
- [x] Review and run Release/Debug regressions; commit mail and composer separately.
- [x] Refresh install/ZIP and verify the installed UI.

## Validation focus

- Empty folders/news must have correct disabled commands.
- Folder selection and Previous/Next use the MSG backend and retain native selection.
- Horizontal reader width spans both lists; both splitters clamp to useful sizes.
- Resize, splitter capture loss and close cannot leave stale window/context pointers.
- Composer menu/keyboard commands retain drafts, attachments and delivery cancellation.
- Do not send to external servers during tests; use isolated profiles and loopback fixtures.

Starting commit: `1cdaf22`. Record outcomes and any scoped decisions below.

## Mail implementation outcome

The old layout failed the new native fixture (browser menus/bars, eight toolbar
buttons, partial-width reader and no folder icons). The replacement passes with
real populated MIME messages, selection, navigation, scrollbar-page updates,
both divider clamps and narrow windows. Actual GDI captures also exposed the
wrong original folder glyph and an unintended border on MIME header tables;
both were corrected and their pixel checks pass. Mail/news keep backend folder
and message ownership. Windows continues to draw native title bars.

## Composition implementation outcome

The prior composition UI failed the native reference checks (missing menus,
only two matching toolbar buttons and no compact header arrangement). The
replacement preserves all backend headers, drafts, attachments and Send Later.
Menu-toggle regressions were reproduced before correcting compact Bcc/Cc
state. Address remains disabled. Quote uses the original translator; a narrow
additive MSG API reports and aborts that separate context. Loopback tests prove
that explicit Quote marks the editor dirty, Stop retains the composition and a
second Quote succeeds. Active SMTP/NNTP send cancellation preserves recipients,
body and attachment, and retry delivers exactly one message.

All 15 CTests passed in Release (102.59 seconds) and Debug (89.63 seconds).
The Linux browser rebuilt and reported 3.02 after the shared quotation change.
Independent reviews covered pane lifetime, geometry and composer command/cancel
behavior; reported defects were fixed and the relevant fixtures rerun. Actual
full-window captures were checked against both references. Windows still owns
system decorations; DPI checks beyond 96 DPI remain future work.

Mail UI commit: `5c58f6a`. Composer is committed as the following roadmap item.

The refreshed Release installation passed both reference-layout fixtures,
including the loopback quotation test, from a directory containing spaces.
CPack generated the Windows x86 ZIP; its executable exactly matches the tested
Release and installed binaries, its PE machine is 0x14c and its Windows guide
matches the current source. Shared Visual C++/MFC runtimes remain required.
