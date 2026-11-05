# Native Windows build

This port builds the native Win32 core libraries and an optional MFC browser
frontend adapted to the 3.02 backend interfaces. The initial frontend supports
HTML layout, GIF/JPEG/XBM images, native HTML forms, local files, HTTP/HTTPS navigation,
frames, history, bookmarks, saving/downloads, scrolling and links, plus native mail/news folder lists, message lists, a reader, and composer windows. Remaining
browser features are tracked in [windows-roadmap.md](windows-roadmap.md).
An optional Gold configuration adds the original HTML editing engine and a
native Windows editor interface; its build and verification scope are below.

## Windows browser frontend

Install the Visual Studio MFC component matching your selected C++ toolset, in
addition to the desktop C++ workload. Include the x86 MFC libraries.

```powershell
cmake -S . -B out/windows-frontend -G "Visual Studio 18 2026" -A Win32 -DNETSCAPE_BUILD_WINDOWS_FRONTEND=ON
cmake --build out/windows-frontend --config Release --parallel
ctest --test-dir out/windows-frontend -C Release --output-on-failure
.\out\windows-frontend\Release\netscape.exe
```

The CMake target is `netscape-win32`; its output is `netscape.exe`. A local
filename or URL can be supplied on the command line. The executable links the
3.02 core archives, rather than replacing the renderer with a system browser.
It uses MFC's shared runtime and the matching Visual C++ runtime.

`cmd/winfe/main.cpp` supplies the native window, location field, menu, navigation
and GDI drawing. `frontend.c` binds the 3.02 context callback table; `platform.c`
supplies profile/file APIs, nonblocking socket polling and asynchronous DNS;
`services.c` implements or explicitly declines the optional UI services.
Menus, accelerators, version information, icon and the original cross-platform
string table are compiled by the Windows resource compiler.

The frontend reuses the recovered 1998 `CCmdParse` and `CGenericEdit` components
and Communicator icon. The original Netscape Public License is retained in
`cmd/winfe/legacy/LICENSE`; provenance is recorded beside those sources. The
new application and backend adapter target the 3.02 API. The full 1998 MFC
application has not been substituted for this tree's frontend.

The default profile is `%LOCALAPPDATA%\NetscapeLegacy`. Set
`NETSCAPE_PROFILE_DIR` to use a separate directory. This initial native port uses
ANSI paths and the legacy IPv4/Winsock interfaces. It remains x86 only.

The frontend tests load an HTML fixture through the original parser and layout
engine, check title/text and draw it into a native GDI surface. They also check
missing-image teardown and that closing the first window leaves a second
window alive. The HTTP test
serves the fixture locally and exercises the frontend's DNS/socket dispatch.
Python 3 is required when frontend tests are enabled. Set `BUILD_TESTING=OFF` to
build without the test runner dependency.

Images use the original GIF, JPEG and XBM decoders with native GDI surfaces,
transparency, intrinsic/explicit sizing, clipped painting and tiled backgrounds.
Shared decoded images survive individual client-window closure. Missing images
and backgrounds unblock layout and retain a visible missing-image indicator.

Forms use native text/password/textarea, checkbox/radio, select, submit and reset
controls. The original layout engine collects successful control values and
encodes GET/POST requests. Enter in a text field submits its form; controls
follow document scrolling. File controls use a read-only path and native Browse
picker, and ignore page-provided default paths. Image links, image-map routing
and image submit controls are connected to the layout APIs.

The image tests check actual GDI pixels, transparency, scaling, shared clients,
reload, failed backgrounds, and deferred callback cancellation over local files
and HTTP. Form tests check reset, scrolling, cleanup after navigation, Enter,
GET, POST and image click coordinates against captured loopback requests.
File upload/multipart behavior, progressive/animated image timing, and full
keyboard traversal through offscreen controls remain unverified.

Plugins, Java, Mocha and historical SSL/S-MIME integration
remain incomplete. Native browser HTTPS is described below. Text and password
prompts use native dialogs.

Framesets use native child document windows with independent contexts, scrolling,
images and forms. Named links and form targets, nested frames, `_parent` and
`_top` navigation use the original context lookup. Resize uses cached documents
and core frame history so it preserves the currently navigated child page.
The frame fixture also streams server-directed `Window-Target` responses while
moving URL/socket ownership and destroying the original child. Native divider
dragging and broader interrupted-load cases remain outside current coverage.
All nine native CTests passed in Release and Debug after frame integration; the
Linux browser also rebuilt with the portable descriptor-query addition.

Saving uses File > Save page as (Ctrl+S), or the document context menu for pages,
links and images. Unknown binary responses use the native save picker. Each
transfer has its own progress/cancel window and continues if its source browser
closes. Bytes go to a unique file beside the destination and replace it only
after stream, status, length and filesystem checks succeed. Cancel/error cleanup
failures are reported with the retained temporary path.

Download fixtures verify binary/source bytes, redirects including final responses
without a length, empty files, identity-encoded truncation, concurrent transfers,
sanitized filenames, browser closure, locked destinations and invalid parent
paths. The write-failure fixture exercises file creation failure; mid-write disk
exhaustion was inspected rather than fault-injected. Save Page declines POST
responses to avoid silently resending forms. Known renderable attachment forcing,
transformed/chunked response support and large-file guarantees remain limited.
All ten native tests passed in Release and Debug; the Linux browser rebuilt after
the shared response-header and identity-encoding changes.

Back/Forward and Reload use the original session history, preserving form values,
select choices, scroll position, methods and frame history. Navigate > History
opens a native list; Open/double click loads the selected history entry. Visited
pages opens the original global-history display. Command availability follows
the core history state. History POST reloads require confirmation on cache miss,
including expired responses, rather than silently sending the form again.

History fixtures verify traversal, saved forms/scroll, forward truncation, reload,
frame-aware Back/Forward, native selection, cancelled POST reload, and visited
history across process restart. Stored dates retain the original four-byte disk
format and are explicitly converted to native time values. Expired and malformed
records and the rendered date view are checked.

Bookmarks > Add page (Ctrl+D) adds the current page to the original bookmark
tree. Manage bookmarks opens a native tree with Open, Add Page, New Folder,
Edit and Delete controls. Names and URLs are saved to the profile's
`bookmark.htm` in Netscape HTML format. The backend outlives manager windows,
so closing and reopening the manager preserves its tree. Browser visits update
the original bookmark metadata.

Bookmark fixtures check native manager actions, edited addresses, folder/order
preservation and names/URLs with quotes, ampersands, angle brackets and literal
entity text across separate processes. Long escaped URLs and legacy wrapped titles
exercise buffer growth; failed destination replacement preserves the old file and
can be retried without another edit. Import/export UI, search and drag/drop
are not yet implemented.

The complete twelve-test suite passed in Win32 Release and Debug on 2026-11-04,
including existing mail/news delivery fixtures. The Linux browser rebuilt and
reported 3.02 after the shared history, HTTP and bookmark changes. Bookmark
read-error/import guards were reviewed; allocation failure was not fault-injected.

To install or package the Release build:

```powershell
cmake --install out/windows-frontend --config Release --prefix out/windows-install
cpack --config out/windows-frontend/CPackConfig.cmake -G ZIP -C Release -B out/packages
```

The Windows archive is named `netscape-legacy-3.02-Windows-x86.zip` and contains
`bin/netscape.exe` and the documentation/licenses. It still requires the matching
shared Visual C++ and MFC runtime. Installation and ZIP generation were verified;
the installed executable passed the image/form fixtures. The native suite includes frame, download, history and bookmark fixtures alongside
the existing browser/mail checks. Configuration-specific verification is recorded
below as each roadmap item is completed.

## Optional Gold HTML editor

Use a separate build directory for Gold. `NETSCAPE_BUILD_WINDOWS_EDITOR` defaults
to `OFF` and requires both the native Windows core and MFC frontend. Enabling it
defines `EDITOR` and `GOLD` consistently across the participating targets and
builds the original `lib/layout` editor sources alongside the parser, layout and
image engines.

```powershell
cmake -S . -B out/windows-gold -G "Visual Studio 18 2026" -A Win32 -DNETSCAPE_BUILD_WINDOWS_FRONTEND=ON -DNETSCAPE_BUILD_WINDOWS_EDITOR=ON
cmake --build out/windows-gold --config Release --parallel
ctest --test-dir out/windows-gold -C Release --output-on-failure
.\out\windows-gold\Release\netscape.exe --edit
.\out\windows-gold\Release\netscape.exe --edit "C:\Documents\page.html"
```

`--edit [path]` opens a new editor or loads the supplied document. Gold browser
menus also expose new/open editor commands and editing of the current page.
The document uses the original EDT model and LO layout with native GDI drawing
and a Windows caret. `editor-ui.cpp` supplies native commands and property
dialogs; `editor-platform.cpp` supplies the original editor frontend callbacks,
local file saving and cancellable progress. Save calls `EDT_SaveFile`, including
its image/link handling and backup transaction; failed or cancelled saves retain
unsaved changes in the editor. Publishing uses the native workflow described below.

The interface transcribes Gold RT_MENU 101 for the editor and RT_MENU 2 for its
browser. It has three icon-only bars, paragraph and size selectors, a 104px
toolbar area at 96 DPI, and a chosen 1105 x 765 outer reference size constrained
to the monitor work area. Toolbar cells come from the original Gold 3.02 bitmap
resources 2001, 2004 and 2005; see the
[artwork provenance](../cmd/winfe/res/gold/README.md). These measurements describe
the selected reference configuration, not an established historical placement
default.

The native command surface includes text entry and selection, clipboard editing,
undo/redo, basic character and paragraph formatting, lists/alignment, local
Save/Save As and browser preview. Insert/property dialogs expose links, images,
targets, HTML tags, horizontal rules, tables and document/text properties.
Printing, publishing, templates, an offline page wizard, Editor Preferences and
external HTML source editing are now connected to native Windows workflows.

The Gold-specific `windows_gold_editor` fixture is registered when testing and
the editor option are enabled. On 2026-11-04, all eighteen Gold CTests passed in
Win32 Debug and Release with MSVC 19.51; the standard edition retained seventeen
passing tests in both configurations. The editor fixture checks new documents,
keyboard and mouse selection, typing, bold, undo/redo, rich clipboard,
HTML save/reopen and browser display, relative links/images, failed-save dirty
state, final buffered-write errors, close/navigation cancellation and reentrant
close events. It compares every compiled Gold menu item with the independently
decoded original resources and captures the actual GDI window at 96 DPI.
The Save As title regression has an exact caption assertion. Final affected
navigation/menu checks were rerun after the save guards were added.

The installed Release executable passed the same editor fixture from a path
containing spaces. CPack produces a separate
`netscape-legacy-gold-3.02-Windows-x86.zip` so the Gold build does not replace
the standard edition's package. Shared MFC/Visual C++ runtimes are still required.

Advanced property-dialog equivalence, asynchronous image-save cancellation,
high-DPI behavior and non-Latin-1 editing remain unverified. The limited original Gold observations are recorded in
[windows-reference.md](windows-reference.md).

## Gold completion workflows

File > Page Setup, Print and Print Preview use the current LO document and GDI
drawing commands. Preview records vector EMF pages, with Next/Previous and zoom;
the same pages feed the Windows printer dialog and spooler. Paper, orientation
and margins persist atomically in `printing.ini`. The current document width is
fitted to paper; ordinary lines/images move to the next page rather than being
cut, while objects taller than a page can span pages. The live editor's selection,
scroll and dirty state are preserved. Browser, focused frame and Mail/News reader
views share printing; message-composition printing remains separate work.
Preview works without an installed printer. Physical printer-driver output has
not been exercised by these tests.

Options > Editor Preferences uses the original Gold General/Publish dialog
templates. Author, local template, external application paths, autosave interval,
link/image save policy, publishing/browse locations and username persist in
`gold-editor.ini`. Cancel retains the previous values. Save failures leave
settings unchanged. Passwords are not written to preferences. The original EDT
autosave callback saves an existing local document; unsaved documents retain the
engine's filename/save prompt behavior.

File > New Document > From Template opens an independent unsaved editor copy.
Its source URL remains the base for relative assets; ordinary Save asks for a new
filename. Opening the original separately does not activate or overwrite that
copy. From Wizard provides a labeled native offline builder for title, text,
layout and background color; it retains a button for the historical hosted
wizard. Web Page Starter supplies an offline guide. These replace unavailable
hosted content and are not copies of the original online services.

Edit HTML Source saves with EDT, launches the configured executable with the
filename passed as data, and offers to import the edited temporary copy into a
new unsaved editor. The current document is retained. Declining import retains
the temporary file and reports its location. External editor configuration takes
an executable path, not shell commands or argument templates; Windows argument
quoting is covered. External image-editor launching and every third-party editor
interaction remain outside runtime coverage.

Publish first saves the current EDT HTML locally, then offers its associated
sibling assets, optional other files from the same directory, destination,
credentials and progress/cancel controls. The original HTTP PUT/FTP backends
upload assets before the referring HTML. Only HTTP and FTP publishing are
enabled; HTTPS requires the separate security port. Failed/cancelled uploads
retain local work and allow retry. Remote uploads are not atomic: earlier files
may already exist on the server when a later transfer fails or is cancelled.
Passwords are cleared from the dialog, never persisted to settings, and cached
HTTP credentials are wiped at network shutdown.

The new fixtures inspect actual vector text and printed form values/password
masking, preview controls, owner-close cancellation, preferences controls and
persistence, template independence, wizard output and actual EDT autosave.
Loopback HTTP/FTP fixtures verify exact HTML/assets/empty files, native Publish
controls, final server rejection, cancel/retry and reserved-character HTTP
credentials. Native General/Publish/Preview/Publish Document captures were
inspected at 96 DPI. All twenty-one Gold tests and eighteen standard-edition tests
passed in Win32 Debug and Release with MSVC 19.51. After the final Save-path
correction, the affected editor/workflow/publishing tests passed again in both
configurations. The regression verifies that navigating an editor and saving
does not overwrite its previously saved document. The earlier
eighteen/seventeen-test results above describe the initial editor milestone.

The installed Release executable passed printing, authoring-workflow,
publishing and original-editor fixtures from a path containing spaces. The
updated Gold ZIP contains that verified executable and the resource provenance.

## Classic Windows interface

The browser now follows the supplied Navigator reference: raised icon/text
buttons, Location dropdown, directory shortcuts, right-hand Netscape logo,
traditional File/Edit/View/Go/Bookmarks/Options/Directory/Window/Help menus,
and a segmented status bar. Original 3.02 artwork is compiled into the binary.
The about page uses the original wordmark and gray table layout, retaining
version 3.02. Java/security/editor credits are not shown as enabled features.

View toggles control the toolbar, Location, directory buttons and status bar.
Ctrl+L/Alt+D selects Location; Enter navigates; the dropdown holds recent
addresses. Toolbar controls support Tab/Shift+Tab and keyboard activation.
Home defaults to the built-in about page; Options can save the current page as
Home in the profile's `browser.ini`. Directory shortcuts retain historical
Netscape destinations; current availability of those sites is not established.

Find (Ctrl+F) uses the original layout search and selection APIs, with case,
direction and wrap options. It retains the focused document when a toolbar
button takes focus, including nested frames. Copy and Select All use native
edit controls or layout selections as appropriate. Full mouse text-selection
work remains separate. Native document printing is available. Gold page editing is available
in the optional editor configuration above; it remains disabled in the standard
build. Images is disabled because images already load automatically.

Mail/news use command-specific classic toolbars and backend command states.
Composer, bookmarks, history, downloads, account settings and prompts share
compact classic fonts, gray surfaces and raised buttons. The composer body
retains its fixed-width editing font; document colors are not overridden.
Metrics scale from window DPI; visual captures were checked at 96 DPI, with
multi-monitor/high-DPI runtime checks still pending. Title bars, system menus
and system file pickers remain managed by Windows.

All thirteen native CTests passed in Release and Debug after this UI pass.
The Linux browser rebuilt with the Windows-specific about-buffer cleanup
guarded separately. Actual captures were checked for the browser, mail,
composer, bookmarks, account settings and native prompts.

`windows_classic_chrome` verifies real navigation, toolbar states, menus, Find,
bar toggles, narrow-window geometry, actual captured pixels (including disabled
artwork), and the embedded about-page image. The frame fixture additionally
checks focused-child Find. About resource buffers are writable for the legacy
parser and explicitly released; the fixture checks zero outstanding buffers.

For repeatable offline window captures, use an isolated profile:

```powershell
$env:NETSCAPE_PROFILE_DIR = "$PWD/out/ui-preview-profile"
.\out\windows-frontend\Release\netscape.exe --ui-gallery "$PWD/out/ui-preview"
```

This diagnostic command requires the profile override and sets a test mail
identity in that profile. It never sends mail. Toolbar bitmap regeneration is
an optional development step using Python/Pillow:
`python cmake/tools/generate_windows_toolbar.py`; normal builds use checked-in
bitmaps. Source provenance is in `cmd/winfe/res/classic/README.md`.

## Original 3.02 menus and geometry

The standard browser now has nine toolbar buttons. Mail/News use the observed
303px folder width and 203px upper lists at 96 DPI; both message lists have
Sender, separate priority/flag cells, Subject and Date. Subscription boxes and
header markers use the original Windows resources, with provenance beside the
assets. Reference-sized browser and Mail/News windows are 1105 x 765 and
805 x 605; these comparison windows were manually resized during inspection,
so these are chosen reference sizes, not independently established factory
window-placement defaults. Message Composition uses its observed 534 x 953
outer size. All are constrained to the current monitor's work area.

The four menu inventories, command routing, exact historical Help/Directory
URLs and remaining unavailable facilities are recorded in
[windows-reference.md](windows-reference.md). Menu generation is optional for
normal builds because generated RC files are checked in. Run
`python cmake/tools/generate_windows_menus.py --check` to verify them.

Options contains the original bar toggles and Auto Load Images. Document/Frame
Source uses the original source converter; Frame Reload targets the focused
frame. Window History opens visited history, while Go lists current session
history. Bookmark dropdown folders preserve backend order. Window menus list
and activate live browser/composition windows, with singleton Mail/News entries.

Message menus now expose the original backend selection, sorting, read/unread,
flagging, attachment/header modes, finding, navigation, folder and filing
commands. Native multi-selection is reconciled before commands; clearing it
also clears the displayed message and disables Reply. Save As in composition
exports body text; Save Draft retains the full message. Immediate/Deferred
Delivery controls the Send command. Wrap preserves content and selection but
resets the native EDIT undo buffer.

The complete seventeen-test suite passed in Win32 Release and Debug after this
parity pass. Tests compare all four compiled menu trees and exercise actual
Source conversion, Find Again, image request deferral, dynamic Go/Window menus,
bookmark hierarchy and cached URLs, message selection/flags/sorting/filing,
header/attachment modes and composer delivery selection. Captures were reviewed
at 96 DPI; broader high-DPI and every interactive picker remain unverified.

## General Preferences

Options > General Preferences opens the classic seven-tab dialog in browser,
Mail and News windows. Appearance controls toolbar pictures/text, default
Browser/Mail/News startup windows, blank/home location, link underlining and
visited-history expiration. Explicit command-line URLs and `--mail`, `--news`
or `--compose` take precedence over default startup choices. The default home
remains the offline `about:` page.

Fonts selects Latin-1 proportional/fixed fonts and sizes through native pickers.
Colors selects text/link/visited/background colors and document override. Image
color and incremental-display options feed the original IL settings. Language
supports ordered available/accepted lists and custom tags; actual requests use
the saved Accept-Language value. Its validated 492-byte limit fits the original
HTTP engine's fixed header buffer. Palette dithering on indexed displays and
progressive timing need further runtime coverage.

Apps and Helpers show the original rows and supported MIME actions, with
unsupported external-app/helper editing disabled. Background-image preferences
and non-Latin-1 font encodings are also disabled pending integration.

Settings live in the profile's `browser.ini`, `[General]` section. Existing
`[Browser] Home` migrates when the new key is absent and remains synchronized
on save. OK validates, flushes a temporary copy and replaces the file before
changing runtime settings; unrelated INI sections are preserved. Failed saves
keep the dialog open and retain the prior file/settings. Cancel discards edits,
including a pending Expire Now. Font/image settings are captured for each
document load, and supported cached GET pages reflow after preference changes;
POST responses are not silently submitted again.

`windows_preferences` checks actual native tabs and keyboard controls, Cancel,
validation, page retention, locked-file failure/retry, all three toolbar styles,
cross-process persistence and legacy-home migration. It loads a real GIF and
HTML page over loopback, checks the chosen font and captured background pixels,
tests blank-browser startup, and inspects actual Accept-Language requests in two
processes. Store checks cover malformed persisted values, unknown INI sections,
temporary-file cleanup and the 492/493-byte header boundary.

All seventeen native CTests passed in Win32 Release and Debug with MSVC 19.51
on 2026-11-04. The installed Release executable passed the Preferences fixture
from a path containing spaces. Actual dialog captures were inspected at 96 DPI;
broader DPI, non-Latin-1 rendering and all startup combinations remain unverified.

The original Windows release was also run and inspected. Provenance, exact
download hashes, observed UI agreements and remaining differences are recorded
in [windows-reference.md](windows-reference.md). This does not establish parity
for every original menu action or optional engine.

## Mail and news

Use Messages > Open Mail / Open News, or launch `netscape.exe --mail`, `--news`,
or `--compose`. Account settings configure name, email address, organization,
SMTP, POP and NNTP hosts. Settings are stored in the profile's `mailnews.ini`,
under `[MailNews]`, with keys `name`, `email`, `organization`, `smtp`, `pop`, and
`news`. NNTP and SMTP accept `host:port`. Change the News host through
Options > Mail and News Preferences. Empty server settings leave the service
unconfigured.

The folder and message controls call the original MSG backend; the reader uses
its MIME conversion, HTML layout and native GDI text drawing. Local mail uses
mbox files in the profile's `Mail` directory. Inbox, Drafts, Sent, Outbox and
Trash are created only when missing. Message commands include reply, reply all,
forward, mark read, delete mail, fetch mail/more news, and send queued messages.

Mail and news now follow the supplied classic Mail reference: their own menus,
a compact ten-button toolbar and logo, folder/message lists across a short
upper band, and a full-width reader below. Browser Location and directory bars
are hidden. Raised dividers resize the folder column and reader height; list
columns resize with the window. Original folder/envelope/flag artwork, Sender,
status, Subject and Date columns, sorting, Previous/Next, normal/all headers,
selected-message titles and mailbox counts use the original backend state.
Plain text messages use Courier New, and borderless MIME header tables retain
their intended appearance. Reader printing uses the shared native vector workflow.

`windows_mail_layout` seeds two real MIME messages in an isolated profile and
checks native selection, decoded reader text, Previous/Next, reader scrolling,
divider dragging/clamps, narrow geometry, menus and toolbar states. It also
checks actual captured GDI pixels for folder artwork and borderless headers.
The new layout fixture and existing mail/news delivery tests passed in Win32
Release and Debug on 2026-11-04.

News also follows its separate supplied reference. Its twelve-button toolbar
contains To:News, To:Mail, Re:Mail, Re:News, Re:Both, Forward, Previous, Next,
Thread, Group, Print and Stop. Re:Both posts and mails the reply; Thread and
Group mark the selected thread or group read. Previous/Next use unread-article
navigation. Get More remains available from File/F5. Its own menus and keyboard
shortcuts distinguish posting from mail composition.

The News Server list has expandable tree guides, host/group artwork,
subscription marks and unread counts (??? when unknown). Click a subscription
cell or press Space on a selected group to toggle it; use the expansion glyph
or Left/Right on a directory row to fold/unfold it. Options selects subscribed
or all newsgroups. Existing subscriptions use the flat subscribed view; a new
host with no subscriptions initially exposes its full hierarchy. Completed
requests preserve an explicitly chosen view and its article selection.

News starts with a taller upper band and wider server list, plus a full-width
white reader when no article is selected. Loaded articles retain normal MIME
rendering. The article list has Sender, separate priority/flag cells, Subject and Date columns. Both pane
dividers remain draggable. Directory rows do not report message counts, and
double-clicking a leaf group cannot invoke directory expansion.

`windows_news_layout` uses a local NNTP server with the three reference group
names. It checks the real server tree, mouse/keyboard subscription and expansion
gestures, empty/selected command states, post/mail/reply headers, mark-thread
and mark-group commands, view retention after article loading, narrow resizing
and divider clamps. It also captures empty and populated native windows and
checks the white empty-reader pixels and actual yellow subscription checkmarks. No articles are posted by this fixture.
The final sixteen-test suite passed in Win32 Release and Debug with MSVC 19.51
on 2026-11-04. Actual empty and populated News captures were inspected at 96 DPI,
including original toolbar artwork and the Netscape logo.
The refreshed Release installation passed the same News fixture from a path
containing spaces, including actual NNTP discovery and composition routing.

The composer now follows the supplied Message Composition reference: its own
File/Edit/View/Options/Window menus, Send/Quote/Attach/Address/Stop toolbar,
right-hand logo, compact Mail To/Cc/Subject/Attachment rows, a fixed-width body
editor and segmented status bar. View exposes advanced recipient headers and
newsgroups while retaining their backend values. Draft, Send Later, attachment
removal, native editing shortcuts and Close remain available through the menus.
Address is visibly disabled until an address book UI exists.

Quote uses the original message translation path. Stop cancels that separate
translation context, keeps the composition open and permits a retry. Explicit
Quote marks the message as changed. Stop also preserves the editor, recipients
and attachments during active SMTP or NNTP delivery so the same composition
can retry. The additive MSG quotation-state/cancel API exposes the backend's
existing translator lifetime without transferring ownership to the frontend.

The composer supports recipient headers, newsgroups, subject, plain text body,
attachments, Send, Send Later, Save Draft and cancellation. Drafts and queued
messages use the original MIME generator and mailbox writer. Drafts may be
saved before entering recipients. Draft saving does not preserve the temporary
Fcc override as a distinct composition setting. Address book UI, rich text
editing, full subscription management and attachment viewing are not implemented.

Transport uses the original plaintext POP/SMTP/NNTP engines. TLS, modern mail
provider authentication and modern server interoperability are not provided by
this port. Local fixtures verify SMTP and NNTP delivery; POP retrieval and
external servers have not been runtime verified.

The `windows_mailnews` test exercises isolated profiles (including spaces and
literal percent signs), native list selection and reader rendering, MIME header
and body decoding, drafts, queueing, attachment changes, canceled-send state,
and real delivery to local SMTP/NNTP fixtures. It never relays to external servers.
`windows_news_empty` checks that an empty LIST fails once without a retry loop.
All six native CTests passed in Win32 Release and Debug with MSVC 19.51 on
2026-11-04. The Linux browser also rebuilt and reported version 3.02 after the
shared MIME, mailbox-path, composition and NNTP fixes.

`windows_composition_ui` checks the real compact and advanced layouts, header
retention/toggles, menus, command states, narrow geometry and captured editor
pixels. A loopback HTTP fixture verifies active Quote cancellation, dirty state,
preserved fields and successful retry. The SMTP/NNTP fixtures also cancel an
active send with an attachment and retry the same composer, checking one
successful delivery. All fifteen native CTests passed in Win32 Release and
Debug with MSVC 19.51 on 2026-11-04. The Linux browser rebuilt and reported
version 3.02 after the shared quotation API addition. Actual full-window Mail
and Message Composition captures were reviewed at 96 DPI.
The refreshed Release installation also passed both native reference-layout
fixtures, including Quote cancellation/retry, from a path containing spaces.

## Build and test

Install CMake 3.20 or newer and Visual Studio with the C++ desktop workload and
a Windows SDK. Configure for **Win32/x86**, not x64. For Visual Studio 2026:

```powershell
cmake -S . -B out/windows -G "Visual Studio 18 2026" -A Win32
cmake --build out/windows --config Release --parallel
ctest --test-dir out/windows -C Release --output-on-failure
```

For Visual Studio 2022, use `-G "Visual Studio 17 2022"`. That toolchain has not
been tested here. The current platform module accepts MSVC and clang-cl;
clang-cl has not been tested either. MinGW and cross compilation are not supported.

On Windows, `NETSCAPE_BUILD_WINDOWS_CORE` defaults to `ON`; the Linux browser and
legacy-library options default to `OFF`. Java, Mocha and security must stay off
for this initial port. `BUILD_TESTING=OFF` omits the native smoke executables.

The `netscape_windows_core` aggregate builds these independently selectable targets:

| Targets | Sources / role |
| --- | --- |
| `netscape_nspr` | Original Win32 native threads, monitors, I/O, memory, events and runtime support. |
| `netscape_jpeg`, `netscape_dbm`, `netscape_hash` | JPEG decoder, persistent hash database and security hash routines. |
| `netscape_parse`, `netscape_lay`, `netscape_img`, `netscape_i18n` | Parser, layout, image and text conversion backends. |
| `netscape_net`, `netscape_msg`, `netscape_mime` | Network protocols, mail/news backend and MIME. |
| `netscape_misc`, `netscape_plugin`, `netscape_xlate`, `netscape_xp` | Shared browser services, plugin backend, translation and platform helpers. |

Archive definitions are in `cmake/targets/windows/`, with shared configuration
in `cmake/modules/NetscapeWindows.cmake`. Existing per-library source manifests
are reused with explicit Windows source selections. Unix external-process
converters and DNS, fallback plugin loaders and Unix builtin string tables are
excluded; these services belong to the Windows frontend. No placeholder frontend
implementations have been added to make a browser link.

The NSPR CPU configuration header is generated by a native executable. Header
namespaces are staged in the build directory. The parser and MIME tables use the
checked-in generated files for now; native resource regeneration and Windows
string/resource DLLs remain frontend work. The Linux resource pipeline is unchanged.

The initial NSPR target is static (`NSPR_STATIC`), rather than the original DLL.
It uses `HW_THREADS`, the source list from `nspr/src/makefile.win` for
`OS_RELEASE=32`, and SDK Winsock. Modern CRT `stat` and `snprintf` implementations
replace conflicting legacy wrappers. Non-SSL socket macros call Winsock rather
than CRT file descriptor functions.

The x64 guard is intentional: this Win32 NSPR backend accesses x86 `CONTEXT`
registers and assumes 32-bit `long`/pointer relationships. The Linux x64 fixes do
not make those Windows assumptions safe.

## Verification

All 15 archives built with Visual Studio 2026 MSVC 19.51 and Windows SDK
10.0.26100 for Win32 Release and Debug. Both configurations passed the runtime
tests, which exercise:

- NSPR initialization, thread identity, thread creation, monitor wait/notify,
  hash insertion/removal and formatted output.
- DBM binary data written, synced, closed and retrieved after reopening.
- Browser non-SSL socket macros using a local TCP listener, data transfer and
  nonblocking error handling.

These tests do not establish that all backend functions work. Building static
archives alone does not resolve their frontend dependencies. The optional MFC
frontend resolves those interfaces for its supported browser paths; there is
no native Java, historical SSL/S-MIME or Mocha runtime verification. Native
BearSSL browser HTTPS has a separate verification matrix below.
Legacy compiler warnings remain, including code paths requiring a later runtime
audit.

## Recovered 1998 Windows frontend

The official [Mozilla source archive index](https://releases.mozilla.org/pub/mozilla/source/)
contains the [April 8, 1998 source release](https://releases.mozilla.org/pub/mozilla/source/mozilla-19980408.tar.gz).
It includes `ns/cmd/winfe` and `ns/cmd/wincom`. The March 31 Unix archive was also
inspected and contains no `cmd/winfe` tree.

Archive SHA-256, computed locally:

```text
2cfcc52d87834663d4aa0b4959c51f66451df965283b28fbfbad62d46741e013
```

Retrieve and verify the reference with Python 3.9 or newer:

```powershell
python cmake/tools/fetch_windows_reference.py
```

The script downloads the archive into ignored `out/research/`, checks that hash,
and extracts the unmodified WinFE/WinCom trees, original `LICENSE`/`LEGAL`, and
the 1998 `fe_proto.h`/`xp_core.h` headers. It excludes CVS metadata, skips links,
checks extraction paths and records an inventory. The reference stays separate
from the 3.02 sources. Copyright and Netscape Public License notices are retained.

This is later Mozilla/Communicator-era code, **not the original Navigator 3.02
Windows frontend**. Concrete differences already found:

- `cmd/winfe/stdafx.h` uses MFC (including private `afxpriv.h`), OLE and common
  controls, and includes `layers.h`, which this 3.02 tree lacks.
- `cfe.cpp` supplies context callbacks such as `CFE_CreateNewDocWindow` and
  `CFE_GetTextInfo`. Some callback signatures resemble 3.02, but context layouts
  and tables must be matched explicitly.
- The 1998 `fe_proto.h` adds NSPR `PRFileDesc *` polling interfaces and conditional
  layer callbacks; 3.02 uses integer socket/select interfaces and exposes
  `FE_DisplayImage`/`FE_DisplaySubImage` callbacks absent from that later header.
- The Windows makefile traverses preferences, optional editor/mail resource DLLs
  and `xpstrdll`, using the 1998 configuration rules. Those rules and backend
  dependencies cannot simply be linked to the 3.02 archives.

The native frontend above provides the first MFC application shell, text
rendering, event dispatch, asynchronous sockets/DNS, file services and resource
targets. General Preferences now covers supported browser settings; advanced
helper/cache controls and optional engines remain integration work. The recovered source remains the implementation guide
while the 3.02 backend interfaces stay authoritative.

The recovered source guides implementation while the original 3.02 Windows
release now supplies a runtime UI reference. See [windows-reference.md](windows-reference.md)
for its provenance and the limits of the comparison. No Windows binary was
decompiled, and complete behavior parity has not been established.

## Browser mouse selection

Browser and message-reader document views route mouse dragging and Shift-click
to the original LO selection APIs. Double-click uses the original word rule,
including trailing spaces. Captured dragging supports autoscroll, suppresses
link activation, and stops on capture loss or a new document load. Editor views
continue to use EDT selection. The `windows_browser_interaction` fixture checks
exact selected/clipboard text, reverse drag, word/Shift selection, ordinary link
click versus drag, capture cancellation and selection in a scrolled child frame.
The native GDI capture shows the selected characters with system highlight colors.

Browser link interactions use owned URL/target strings from a shared text/image
hit test. Native context menus offer Back/Forward/Reload, link opening in its
target or a new window, URL copying, image viewing/saving and selected-text
Copy. Hover shows the resolved destination in the owning browser's status bar
and uses hand/text cursors. Client-side and server-side image maps share their
coordinate/target resolution with normal clicks. Mail/news reader links open a
browser rather than replacing the message. The `windows_browser_links` fixture
selects real popup items, checks exact clipboard URLs, separate windows and
named child-frame targets, and closes a browser while its popup is active.
Queued messages for destroyed windows are filtered before MFC examines them.

Frame dividers use the original LO edge visibility, movement bounds and
NORESIZE state. A nonactivating drag indicator follows the captured pointer;
releasing commits through `LO_MoveGridEdge`. Escape, capture loss, navigation
and edge destruction cancel the operation. Visible edges use classic beveled
painting. Parent framesets occupy the full pane without scrollbars, matching
the original full-window callback contract and allowing LO to restore resized
grid geometry from same-size history. Child recreation retains saved form/grid
data. When the window dimensions change, LO recalculates the frameset using
its original sizing rules. The `windows_frame_dividers` fixture checks both
axes, clamping, resize cursors, tracking-before-release, cancellation, NORESIZE
(including double-click), form retention, history and window resizing.

Cancellation is retained through the next mouse-up: releasing a canceled drag
over a link cannot activate it or submit an image input. Clipboard URL writes
retry a brief competing lock (up to 200 ms) without pumping a reentrant window
loop, and report a busy clipboard if it remains unavailable. Regressions cover
capture-loss and navigation cancellation plus a real competing clipboard owner.

Browser interaction verification on 2026-11-04: the complete standard edition
suite passed 21/21 in both Win32 Debug and Release; Gold passed 24/24 in both
configurations. These runs include the original mail/news, printing, preferences
and Gold editing/publishing workflows as well as the new selection, link and
frame-divider fixtures. The native selection and frame captures were reviewed.
An independent review found one cancellation issue; its new failing regressions
passed after the fix, followed by all four complete suites.

## Background image preferences

The original Colors page now enables Image File, its path field and Browse.
The image path/mode are saved with General Preferences; Cancel and failed writes
retain the old settings. Local paths with spaces and percent signs are converted
to file URLs for LO's default backdrop. A page's background wins unless Always
Use My Colors is selected. Choosing Default/Custom clears the user backdrop on
subsequent layout. Missing configured files fall back to Default at startup
without rewriting the saved preferences. GIF/JPEG/XBM decoding uses the original
image engine. The background fixture checks actual tiled GDI pixels, persistence
across processes, failed saves, invalid files and removal; Gold additionally
checks that a user backdrop is absent from the editor's document properties.

## Supporting applications

The classic Apps page now saves Telnet, TN3270 and View Source executable paths
and an existing writable temporary directory. Browse selects a Windows .exe;
fields accept executable paths without shell command templates. Terminal clients
receive `[-l username] host [port]` as distinct arguments and must support that
convention. Dotted, dashed and underscored logins are preserved on Windows;
invalid login characters are rejected instead of silently truncating the name.
Rlogin remains unavailable.

A configured source viewer receives a temporary file only after a fresh raw GET
finishes and the checked download stream validates its result. POST responses
are not resubmitted. Blank View Source retains the original engine's separate
named colored-source window. External source retrieval inherits the download
engine's content/transfer-encoding limits. Successful source copies remain in
the selected temporary directory so applications that hand off to another
process can still read them; failed/canceled copies are removed. Browser closure
and later directory changes do not invalidate an active source job.

Cancel and failed preference writes retain active settings. Missing configured
applications fall back to unconfigured/internal behavior at startup. A missing
temporary directory falls back to the profile-local temp folder without changing
the saved INI. Native fixtures launch a controlled child executable and check
actual arguments, source bytes, encoded-invalid logins, 245-character directory
paths, truncation/cancellation, failed launch, POST protection, browser closure,
temp-directory switching and restart persistence/fallback. Third-party terminal
and source-viewer applications have not been exercised.


## Native browser HTTPS (BearSSL)

The native x86 frontend defaults `NETSCAPE_ENABLE_BEARSSL_HTTPS` to `ON`.
It is mutually exclusive with `NETSCAPE_ENABLE_SECURITY` (the historical
SSL/S-MIME backend). Mail POP/SMTP/NNTP remains plaintext. This integration
has been exercised on Windows; no Linux TLS runtime support is established.

Build prerequisites are CMake 3.20 or newer, Visual Studio's desktop C++
workload with matching x86 MFC libraries and a Windows SDK, and Python 3.
Verification used CMake 4.4.4, Visual Studio 18 2026, MSVC 19.51 and Windows
SDK 10.0.26100 on a Windows 11 IoT Enterprise LTSC 10.0.26100 x64 host.
Select a CMake release that supports the installed VS generator.
The native TLS fixtures additionally require Python `ssl`, `cryptography`, and
an OpenSSL CLI capable of real TLS 1.0/1.1 server/client handshakes. The
`tls_engine_prerequisites` test fails visibly if that capability is unavailable;
a skipped/failed legacy fixture is not evidence of compatibility support.
The executed environment was Python 3.13.9, cryptography 46.0.3, Python OpenSSL
3.0.18 and Git for Windows' OpenSSL CLI 3.5.6. Fixture OpenSSL is neither linked
nor installed with Netscape.

```powershell
cmake -S . -B out/bearssl-win32 -G "Visual Studio 18 2026" -A Win32 -DNETSCAPE_BUILD_WINDOWS_FRONTEND=ON -DNETSCAPE_ENABLE_BEARSSL_HTTPS=ON -DNETSCAPE_TLS_FIXTURE_OPENSSL=C:/Users/raynorpat/AppData/Local/Programs/Git/mingw64/bin/openssl.exe
cmake --build out/bearssl-win32 --config Release --parallel 8
ctest --test-dir out/bearssl-win32 -C Release --output-on-failure
cmake --build out/bearssl-win32 --config Debug --parallel 8
ctest --test-dir out/bearssl-win32 -C Debug --output-on-failure
cmake --install out/bearssl-win32 --config Release --prefix "out/bearssl installed 100%"
python cmake/tests/check_windows_tls.py --executable "out/bearssl installed 100%/bin/netscape.exe" --output out/bearssl-win32/tls-installed-manual --mode installed
```

Run native UI tests sequentially; simultaneous suites compete for windows and
clipboard state. `windows_browser_tls` retains its original name and covers real
HTTPS document/image rendering, POSTs, downloads, redirects, CONNECT, reuse,
cancellation and transfer failures. `windows_tls_policy` covers persisted
preferences and live connection policy; `windows_tls_errors` checks native
Document/Frame Info and failure presentation. `windows_tls_installed` installs
the selected configuration under its build directory and checks the Runtime
package from an unrelated working directory. It mutates only relocated copies.
Its checks cover packaged roots/notices, spaces and percent signs, startup CA
selection with omitted/`-` smoke arguments, explicit override replacement,
missing/malformed default and override, no source/CWD fallback, and continued
HTTP with broken trust configuration. Native captures are stored beside reports.
An explicitly disabled frontend instead registers `windows_tls_disabled`, which
uses the existing smoke CLI to prove HTTPS fails before even opening a TCP
connection while HTTP still renders.

General Preferences > Security has an unchecked-by-default obsolete-TLS
compatibility checkbox and a CA bundle field. The profile's `browser.ini` stores
`[General] allow_legacy_tls=1` only for explicit compatibility and `ca_bundle`
as an absolute local filename. Relative, missing, malformed and unsupported CA
files disable HTTPS; there is no certificate bypass or silent fallback. A blank
CA field loads `../share/netscape/ca/cacert.pem` relative to `bin/netscape.exe`,
never the working or source directory. Overrides replace the complete root set.
Paths use the native ANSI/MAX_PATH APIs; Unicode/long-path support is not claimed.
CA file selection/content changes take effect after restarting Netscape. Saving
the protocol preference affects new connections immediately, retires idle reused
connections and lets already active transfers finish under their old policy.
Cancel or failed preference writes retain the active settings.

The default permits TLS 1.2 with ephemeral ECDHE RSA/ECDSA authentication and
ChaCha20-Poly1305, AES-128-GCM or AES-256-GCM. Compatibility permits TLS 1.0
through 1.2 and additionally ECDHE RSA/ECDSA AES-128-CBC-SHA. Neither mode permits
SSL 2/3, RC4, DES, 3DES, NULL, export or static-RSA suites. There is no automatic
downgrade retry, session resumption or renegotiation. TLS 1.3, client certificates,
IP-literal identities and non-ASCII hostnames are unsupported. ASCII DNS names
use SNI and certificate chain, hostname and UTC validity checks. Entropy comes
from Windows BCryptGenRandom; entropy, certificate and TLS failures close the
request. No session keys or production HTTP payloads are logged by this adapter.
Document/Frame Info reports the actual request's negotiated version/cipher;
cache/history content without live evidence does not claim authentication.

Certificate purpose is also checked on every certificate presented by the peer,
including intermediates and any presented root. Absent Extended Key Usage (EKU)
is unrestricted; a present EKU must include serverAuth or anyExtendedKeyUsage.
Mixed server/client use is allowed, while clientAuth-only, codeSigning-only or
other unrelated-only EKU is rejected before application data. Configured trust
anchor records are not reparsed as presented certificates. Purpose rejection is
appended category `NET_TLS_ERROR_CERT_PURPOSE = 16`; values 0 through 15 retain
their numbering. Native Document/Frame Info reports that the certificate purpose
does not permit server authentication.

The adapter uses a fixed 65,536-byte buffer per live TLS context, reused for each
presented certificate. This adds 64 KiB per context and limits each presented
certificate to 65,536 DER bytes. Malformed or oversized certificates fail closed.
The existing BearSSL minimal validator still decides signature, trust, name,
time, Basic Constraints, Key Usage and critical-extension validity; its errors
always take precedence. In particular, critical EKU remains unsupported: even a
valid critical serverAuth EKU is rejected by the underlying validator. The
purpose inspection is an adapter guard, not a replacement PKIX implementation.

No revocation checking (OCSP/CRL), automatic intermediate download, background
CA updater or Windows certificate-store import is provided. The Mozilla-derived
PEM list does not reproduce Firefox's additional root restrictions. HTTPS fetch
success does not establish modern HTML/JavaScript rendering compatibility.

BearSSL is pinned at `7bea48e5e850ab4cafbe68d3765cdaba13a86d6f`, with unmodified
upstream files; its import archive SHA-256 is
`ed397832b1613582737d9c1ce01c1af74ad6a9e9c52a341e24433987cda0f372`.
See [dependency provenance](../third_party/bearssl/README.md).
The shipped curl/Mozilla bundle is dated 2026-10-20, has 118 roots, and SHA-256
`796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe`.
See [CA provenance and parser limits](../security/ca/README.md). To update it,
review a dated CA Extract bundle and checksum, replace the complete PEM, update
its date/count/hash provenance and the installed checker's expected hash/count,
rebuild/reinstall and rerun the trust, browser,
policy, error and installed tests. Restart running browsers to load replacement
roots. For a private replacement, set the absolute CA path and restart instead.
Runtime packages retain BearSSL MIT/provenance, Mozilla MPL/provenance and WinFE
NPL/source provenance notices under `share/doc/netscape`.

The dated [verification report](../docs/superpowers/reports/2026-11-04-bearssl-https-verification.md)
records the final commands and results after the independently reviewed purpose
fix: complete standard Release and Debug suites each passed 118/118; separate
Gold editor/network/HTTPS passed 17/17; explicit TLS-off passed 24/24; installed
runtime checks passed 13/13 plus notices/provenance. Public TLS 1.2 fetches and
native rendering observations are recorded separately, including an origin
denial. All final checks ran on 2026-11-04 EST. Historical verification paragraphs
elsewhere in this guide refer to earlier stages, not the current TLS matrix.
