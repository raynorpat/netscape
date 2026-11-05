# Windows address book and recipients implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan inline, with a final review.

**Goal:** Enable Window > Address Book, Mail/News Add to Address Book and the
composer Address picker using the original 3.02 BM engine.

**Architecture:** A persistent process-lifetime MWContextAddressBook owns the
original BM tree and profile-local address.htm. Native MFC tree, properties and
recipient dialogs adapt BM; the existing MIME APIs quote addresses and remove
duplicates. Composer receives committed To/Cc/Bcc values only after OK. The
address context survives window closure as the existing bookmark context does.

**Tech stack:** Win32/MFC, BM, MSG/MIME, RC, CMake and native/Python CTest fixtures.

**Spec:** Standing user Windows-roadmap instructions and Gold UI references;
original runtime inspection confirmed Address Book - address.htm with
File/Edit/Item menus and a root tree. Retain native system colors and controls.
Record unverified original workflows separately from tested port behavior.

## Constraints and review focus

- Preserve existing address files and bookmarks; never substitute a new store.
- Test new/edited/deleted contacts, nicknames and mailing lists across restart.
- Test quoted names, duplicate addresses, CR/LF rejection and malformed input.
- Test Cancel leaves both composer recipients and existing contacts unchanged.
- Test failed save keeps changes available for retry and preserves prior disk data.
- Test composer owner closure during a modal picker, stale entries and empty books.
- No delivery outside loopback fixtures; never enable an inert command.

## Task 1: Address engine/window and menus

Files: new cmd/winfe/addressbook.h/.cpp and addressbook-resource.h/.rc;
modify services.c BMFE routing/FE_GetAddressBook, frontend.c shutdown, main.cpp
Window/MSG routes, netscape.rc includes and cmake/targets/windows/frontend.cmake.
Expose WFE_AddressBookContext(view), Show, Changed, Edit, Shutdown and Smoke.

- [x] Add native/Python regression proving FE_GetAddressBook currently returns
  NULL and Window > Address Book is disabled; run it red.
- [x] Create/init the address context without displaying it for MSG lookups;
  load address.htm through BM, retain it across window closure, save explicitly.
- [x] Add native tree, File/Edit/Item commands and contact/list property dialogs;
  route callbacks by context so bookmarks stay independent. Keep writes checked.
- [x] Enable shared Window command and selected-message MSG_EditAddress action.
- [x] Verify persistence, mutation/cancellation, nickname expansion, read/save
  failure and retry, inspect native captures; document and commit.

## Task 2: Composer recipients

Files: addressbook picker module/resources; composer.cpp toolbar/recipient
integration and native smoke; Python UI checks and address-book integration test.
Expose bool WFE_PickRecipients(owner, to, cc, bcc) for staged native edits.

- [x] Add failing native test for Address activation and To/Cc/Bcc selection.
- [x] Implement picker with available addresses and staged recipient rows;
  original MIME quoting/duplicate removal, explicit OK/Cancel, safe owner lifetime.
- [x] Enable Address only when composition can accept recipients; reveal Bcc
  if populated, sync MSG headers, preserve body/attachments and dirty state.
- [x] Test empty/repeated/cancel/owner-close and actual queued/delivered MIME
  through existing loopback tests. Build standard/Gold Debug/Release, run
  affected checks serially, refresh/verify release ZIPs, document and commit.

## Ledger

- Initial tree is clean at bf0f76d.
- Ruling: use BM, which libmsg already calls through FE_GetAddressBook, rather
  than the later addrbook.h database API with absent native implementation.
- Exposed thin BM_NewAddress/BM_NewAlias constructors rather than synthesizing
  contacts as URLs or importing temporary files. Lists must contain aliases;
  the original reader promotes direct contact children to the root.
- BM_SelectItem with extend=false calls BMFE_EditItem. Native selection clears
  first and extends without editing; explicit core edits are queued/coalesced.
- BM_GetFullAddress now clears its temporary marks, making repeated queries
  independent. Existing bookmark/messaging regression checks remain green.
- Item 1 evidence: initial native context assertion failed (address_context=0).
  Gold Debug seven targeted checks pass (66.17s); extended failed-read, retry,
  native menu and restart checks pass (1.52s). Native capture inspected.

- Review added a single Properties undo batch and File > Retry Load. A strict
  nickname-expansion regression failed before the shared BM fix: NULL removal
  omitted its reverse undo event and left freed nicknames non-null. Clearing
  through bm_CopyStringWithUndo fixes Undo/Redo and reload cleanup; same-entry
  case-insensitive nickname updates are accepted. Read-only final review found
  no additional correctness issues. Native address fixture passes (3.99s)
  before the final full-matrix run.

- Final source builds in standard/Gold Win32 Debug/Release. The address-book
  fixture passes all four (Gold 4.09s/2.53s, standard 2.77s/2.36s), including
  real MIME drafts; existing composition and loopback delivery pass in all four.
- Full suites run serially: Gold 26/29 each, standard 24/26 each. Welcome and
  News time out; Gold also has background-preferences timeouts. An isolated
  background retry passes. Main-thread stacks block in Win32 AlphaBlend and
  tooltip WindowFromPoint, and the previous packaged executable reproduces
  the Welcome timeout. These are recorded limits, not claimed passing tests.

- Both installed Release editions pass the address-book/picker fixture from
  paths containing spaces. Both CPack ZIPs were refreshed; CRC, exact tested
  executable bytes, installed executable bytes, welcome provenance and current
  packaged Windows documentation were verified.
