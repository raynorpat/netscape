# Original Windows 3.02 comparison

Observed on 2026-11-04 using the preserved English Win32 release and this
repository's native frontend on the same modern Windows desktop. This is a
record of specific checks, not a claim of complete behavioral or pixel parity.

## Provenance

The [Netscape Archive](https://netscape.retropc.se/) supplied both installers:

| File | Download | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Standard | [n32e302.exe](https://netscape.retropc.se/3/n32e302.exe) | 3686912 | `51f3444e400a40b4ad8459a0744db76c927d00414387f25cbc4eb87425b9428f` |
| Gold | [g32e302.exe](https://netscape.retropc.se/3/g32e302.exe) | 3728384 | `d053540b580809af7d2afb1a770cd26960c3996328fea89f7e2d1641acb1f117` |

These hashes identify the downloaded copies; no publisher signature or separate
historical checksum was authenticated. The standard extracted `netscape.exe`
is 2992128 bytes, SHA-256
`26e86c5b7acebc3af39a66441955720301af4c58c33b9e853f44e44ca3ae0794`.
Its file/product version and running About page identify version 3.02. The Gold
version resource identifies Gold 3.02. The later Gold runtime checks described
below were limited to a blank editor and a single physical keypress.

The installers use classic InstallShield archives. The inspected Python
[setup30 extractor](https://github.com/ostrich/setup30) unpacked DATA.Z,
SYSTEM.Z and JAVA.Z without running installation. A local output-path check
was added before extraction. 7-Zip's explicit `-tPE` mode extracted executable
resources for menu/version inspection. No binary decompilation was needed.

All downloads, extracted files and comparison captures remain in ignored
`out/research/navigator302/`; none are distributed in the source or Windows ZIP.
The runtime directory contains the original application, help/license file and
its bundled DLL dependencies. Its mailbox fixtures and generated data are local
to that directory. The port uses a separate `NETSCAPE_PROFILE_DIR`.
This was a native desktop run, not an isolated virtual machine; isolation of the
original program's registry writes was not established. No installer, file
association changes, external mail delivery or news posting was performed.

## Checks and differences

| Area | Original observation | Port comparison / remaining work |
| --- | --- | --- |
| Browser | Running standard 3.02 About page; nine icon/caption toolbar buttons, Location, six directory shortcuts and nine top-level menus. Later comparison also used the running Gold 3.02 browser and the user's side-by-side capture. | Standard nine-button/Gold ten-button toolbars use original Windows artwork. The browser now follows the observed button grouping, Location icon/input, directory widths, logo and status-pane layout. Native printing/preview is implemented as described in windows.md; original printer-output equivalence remains unverified. |
| Gold editor | Opened a blank original Gold editor; three icon-only toolbar bars and editor MENU 101. A physical `g` keypress produced a visible character. Sky's Unicode `type_text` injection did not enter the intended text. | Optional native EDT integration uses the same menu inventory, bitmap cells and editor icon, with a 104px toolbar area at 96 DPI and chosen 1105 x 765 outer reference size. Native editing/save fixtures pass as described below; these observations do not establish general original input, formatting, save or property-dialog equivalence. |
| Mail | Opened Inbox containing the same two synthetic MIME messages used by our fixture; selected and read the base64 message. Ten toolbar buttons, eight menus, folder/message lists above a full-width reader. | The later Gold capture establishes a 333px Mail folder column and 203px upper lists, now used by the port. Original list left the fixture's US-ASCII encoded-word headers literal; the port decodes them. |
| Composer | Opened a new unsent composition. Five toolbar buttons, five menus, compact Mail To/Cc/Subject/Attachment rows and fixed-width editor. | Structure agrees. Original Address is enabled; the port now opens a native recipient picker. Fresh original window was approximately 534 x 953 pixels; the port now uses those dimensions, constrained to the monitor work area. |
| News | Opened the original empty News window: twelve buttons, eight menus, server/group tree, three subscribed default groups and unknown unread counts. Default host `news` failed DNS. | Original reply-button order and Re:Both caption, separate priority/flag columns, Date, yellow subscription boxes and white empty reader now match. No original article retrieval was verified. |
| Preferences | Opened and captured Appearance, Fonts, Colors, Images, Apps, Helpers and Language. | All seven native pages now exist; supported settings persist and affect the browser. Unsupported controls are disabled as detailed below. |

Captures for browser About, all seven original Preferences pages, empty/populated
Mail, empty News and Message Composition are in the ignored `captures/`
subdirectory, with accessibility text where available. `standard-menus.json`
records the parsed original MENU resources (browser 2, Mail 221, composer 224,
News 225).
Window measurements are approximate outer dimensions at the observed desktop
scale. The port uses current Windows colors and window decorations. Original
Gold controls determine the chrome layout and artwork; only the browser's
recessed Location combo uses classic control rendering with those system colors.

## Browser chrome comparison

The user's side-by-side capture and running Gold browser supplied these 96-DPI
reference dimensions: toolbar buttons 49x41 at (5,3), 8px group separators before
Edit, Reload and Stop, Location icon 17x20 at (5,52), Location combo at (78,52),
and a 50px framed logo. Directory buttons begin at (9,75), have widths
88/89/84/73/62/62 and are 21px high. The default chrome is 107px high. The port's
native fixture checks these actual window bounds rather than just a button count.

The status icon, progress and mail panes are 50/96/50px; the text pane stretches.
Message/progress edges are thin, while icon areas are unframed. The complete
unknown-mail indicator and native sizing grip are visible. Pixel checks cover
the Location pen, arrow, recessed edge and grip; F4/Escape and native grip hit
testing are exercised. MFC idle and menu help no longer erase the stored
document status, including a hovered link leaving during menu tracking.

## Menu transcription and native command verification

The checked-in `cmake/tests/fixtures/windows302-menus.json` records the four
original menu trees, original command IDs/flags, mapped native commands and
indexed RT_STRING routes. Browser/Mail/News RC files and historical links are
generated by `python cmake/tools/generate_windows_menus.py`; `--check` detects
stale outputs. The native chrome fixture loads all four compiled MENU resources
and compares labels, accelerator text, hierarchy, separators, mapped IDs and
explicitly disabled facilities against that record.

Window placeholders expand into the original Mail/News, Address Book,
Bookmarks, History and live browser/composition entries. Go history entries use
live session-history IDs; bookmark submenus use the original BM tree with owned
URL snapshots. Source uses the original color converter and its document-only
window hints. Auto Load Images controls real cache-only versus normal IL
requests, including a Load Images override limited to the current document.

Mail/News menus route sorting, thread/flag selection, read/unread/flag changes,
header and attachment modes, message finding, next/previous/unread/flagged
navigation, filing and folder/news actions to MSG. Native availability/check
states come from the backend. News host entry now uses a native prompt.
Network/security settings, other encodings, address-book operations and general
browser file upload remain disabled while their integrations are missing.
Historical Help/Directory URLs are reproduced exactly; modern availability of
those destinations is not established.

Message composition supports the original header/attachment visibility,
quotation paste, wrap and immediate/deferred delivery choices. Save As exports
body text; the retained Save Draft extension preserves the full composition.
Remove All Attachments is the other recorded native extension. Changing wrap
preserves text, selection and scroll but clears the native EDIT undo history.

## Preferences details confirmed against the running original

- Appearance has Pictures/Text/Pictures and Text, Browser/Mail/News startup,
  blank/home location and link/history controls. A fresh original profile used
  nine expiration days; the supplied screenshot's eight is not the default.
- Fonts has encoding and proportional/fixed font choices. The port currently
  supports Latin-1; other encodings need separate rendering work.
- Colors has custom link, followed-link and text choices, default/custom
  background and document override. Background-image selection is disabled in
  the port pending integration.
- Images has Automatic/Dither/Substitute Colors and while/after loading choices.
  These use the original image library preferences in the port; true-color
  output alone cannot demonstrate palette-dithering equivalence.
- Apps lists Telnet, TN3270, View Source and Temporary Directory. Unsupported
  external-application configuration is disabled in the port.
- Helpers exposes MIME handling. The port displays supported browser/save
  actions; editing and external helper launching remain disabled.
- Language has available/accepted lists, add/remove and a custom language field.
  The port validates and bounds the resulting HTTP header for the legacy engine.

The original Preferences measured approximately 502 x 442 pixels; the port's
capture now matches 502 x 442 at 96 DPI. The port provides standard tab keyboard
navigation and applies Expire Now only after a successful OK, preserving Cancel
semantics. Destructive history operations were not exercised in the original.

## Gold resource and runtime scope

The optional `NETSCAPE_BUILD_WINDOWS_EDITOR=ON` build transcribes the Gold
editor's RT_MENU 101 and browser's RT_MENU 2, independently of the standard
browser menu. The browser transcription is recorded in
`cmake/tests/fixtures/windows302-gold-browser-menu.json`; the editor fixture is
`cmake/tests/fixtures/windows302-gold-editor-menu.json`. Its 177 rows, including
33 separators, come directly from the original MENU 101. Both complete compiled
trees are checked by the native fixtures. Toolbar artwork consists of unchanged 20 x 18 pixel
cells cropped from original RT_BITMAP 2001, 2004 and 2005 resources, with source
details in [the Gold artwork record](../cmd/winfe/res/gold/README.md).

The native implementation uses the original 3.02 EDT document engine and LO/GDI
rendering, with native commands for basic formatting, insertions, property
dialogs and local saving. The three bars occupy 104px at 96 DPI. The 1105 x 765
outer window size is a selected comparison size, constrained to the monitor work
area, rather than an independently established original default. Remaining
Gold workflows are documented below.

Original runtime evidence currently consists of opening the blank Gold editor
and confirming a visible `g` from a physical key event. The failed Unicode text
injection limits that automation method; it does not establish a limitation of
ordinary typing in the original program. No original formatted-document,
clipboard, save/reopen, table or property-dialog workflow was verified.

The port's eighteen-test Gold suite passed in Win32 Debug and Release on
2026-11-04. Its native EDT fixture exercises typing, selection, rich clipboard,
format/undo/redo, new documents, HTML save/reopen/browser viewing, relative
links/images, failed saves, buffered final-write errors and cancellation of
close/navigation with nested close events. Native captures confirm the 104px
bars, editor icon, disabled glyphs and corrected Save As caption. These tests
establish the port's observed behavior, not untested original-release equivalence.

## Coverage limits and next comparisons

The subsequent Gold completion uses the original General/Publish Editor
Preferences templates (resources 2008/2189), with source hashes in the artwork
record. New native workflows provide vector print preview/page setup, local
template copies, an explicitly labeled offline page wizard/starter, external
HTML-source import and HTTP/FTP publishing. These do not depend on the original
retired hosted wizard/catalog. Native captures of Preferences, Preview and
Publish Document were inspected; original hosted-service behavior, third-party
editor applications and physical printer output were not compared.

The native fixtures inspect actual vector text, printed form values/password
masking, owner-close cancellation, preferences Cancel/persistence and EDT
autosave. Publishing tests use loopback servers for exact HTML/assets/empty
files, final rejection, cancel/retry, and reserved-character HTTP Basic
credentials. FTP directory probes are checked against a server which rejects
file paths as directories. No public server or user account was used.

## Mail and News screenshot follow-up

The user's Gold captures establish 41x41 toolbar buttons starting at (5,3),
8px group gaps and 53px chrome. Mail uses a 333px folder pane; News uses 303px.
Both upper lists are 203px high. Folder columns are 243/42/46px for Mail and
243/16/42px for News. Sender/priority/flag/Subject are 150/16/16/150px; Date
fills the remaining space. Its initial sort indicator now reflects the backend
Date sort rather than an unrelated default message-number sort.

The original expanded News host and article images are bitmaps 702 and 723.
Comparison against the supplied screenshots matched all 78 and 109 ink pixels,
respectively. Mail leaf folders retain their closed-folder image when selected.
Mail status displays the original backend's mailbox path/wasted-space text.
The Gold Debug artwork, messaging, empty-News, chrome, Mail/News layout and
composition tests pass 7/7; native captures were inspected at 96 DPI.

The original first-run Windows Inbox supplied the 3,513-byte Mozilla Welcome
payload, dated December 13, 1995. Its exact headers/HTML are now embedded for the
original MSG first-run path; they differ from the Unix welcome resource.
Fresh profiles show Inbox/Sent and automatically read Welcome. Existing empty
or populated Inboxes are preserved. The historical remote banner is retained
as a URL; the built-in Netscape logo is decoded locally. Native fixtures check
rendered text, automatic selection and exact bytes, plus retry/collision/rename
failures. See [payload provenance](../cmd/winfe/res/mailintro-provenance.md).

The subsequent composition screenshot establishes the same 41x41 button
height/width with gaps before Quote and Stop, beginning at (5,3), and a framed
32x32 logo at top 1px/right 6px. The composer now uses that separate layout;
the earlier 42x38 controls and stretched 40px logo differed. Original glyphs
and captions are retained. Address now opens the native recipient picker.

Original runtime inspection covered browser chrome/About, local mail reading,
empty News, new composition, the seven General Preferences pages and the limited
blank Gold editor input observation above. It did not verify
original POP/SMTP/NNTP sessions, sending, printing, address-book workflows,
plugins, Java, JavaScript, security, broader Gold editing, all menu actions, or startup
combinations. Our loopback tests establish the port's tested behavior, not
automatic equivalence with every original operation.

Follow up with a controlled original NNTP session and matching-size pane
captures, then compare each newly implemented printing, address-book, helper,
cache and optional-engine workflow against the original. Keep functional gaps
in [windows-roadmap.md](windows-roadmap.md) until implemented and verified.

## Address book and Select Addresses follow-up

Original Gold runtime inspection on 2026-11-04 confirmed the
Address Book - address.htm title, root Address Book tree and File/Edit/Item
menus, including Add User, Add List and Properties. The composition Address
button opens a compact Select Addresses list with To/Cc/Bcc buttons and OK;
Escape/window close cancels it. The port implements that structure using native
controls and system colors. Native captures were inspected at 96 DPI.

The port's persistence, validation, contact/list mutation, nickname expansion,
recipient deduplication, draft MIME, cancellation and recovery tests establish
its behavior. Original Properties workflows, clipboard operations, import/export
and full pixel parity were not established by this inspection. These remain
separate from the observed title/menu/picker structure.
