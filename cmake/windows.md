# Native Windows build

This port builds the native Win32 core libraries and an optional MFC browser
frontend adapted to the 3.02 backend interfaces. The initial frontend supports
HTML layout, GIF/JPEG/XBM images, native HTML forms, local files, HTTP navigation,
frames, history, bookmarks, saving/downloads, scrolling and links, plus native mail/news folder lists, message lists, a reader, and composer windows. Remaining
browser features are tracked in [windows-roadmap.md](windows-roadmap.md).

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

Plugins, printing, Java, Mocha and SSL/security integration
remain incomplete. Text and password prompts use native dialogs.

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
work remains separate. Print and the unavailable Gold page editor are visibly
disabled; Images is disabled because images already load automatically.

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

## Mail and news

Use Messages > Open Mail / Open News, or launch `netscape.exe --mail`, `--news`,
or `--compose`. Account settings configure name, email address, organization,
SMTP, POP and NNTP hosts. Settings are stored in the profile's `mailnews.ini`,
under `[MailNews]`, with keys `name`, `email`, `organization`, `smtp`, `pop`, and
`news`. NNTP and SMTP accept `host:port`. The News location field can switch
hosts with Connect. Empty server settings leave the service unconfigured.

The folder and message controls call the original MSG backend; the reader uses
its MIME conversion, HTML layout and native GDI text drawing. Local mail uses
mbox files in the profile's `Mail` directory. Inbox, Drafts, Sent, Outbox and
Trash are created only when missing. Message commands include reply, reply all,
forward, mark read, delete mail, fetch mail/more news, and send queued messages.

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
no native Java, SSL/S-MIME or Mocha runtime verification.
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
targets. Richer preferences and optional engines remain integration work. The recovered source remains the implementation guide
while the 3.02 backend interfaces stay authoritative.

Because usable Windows frontend source was recovered, binary reconstruction is
not needed at this stage. A verified 3.02 Windows executable can later serve as
a behavior oracle for differences the source cannot explain; no Windows binary
has been decompiled or its behavior claimed to match in this port.
