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
- [ ] Match compact composer controls, menus and toolbar; preserve hidden headers.
- [x] Check actual populated-message captures and resize/selection behavior.
- [ ] Review and run Release/Debug regressions; commit mail and composer separately.
- [ ] Refresh install/ZIP and verify the installed UI.

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
