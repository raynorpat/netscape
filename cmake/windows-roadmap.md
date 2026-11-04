# Windows port backlog

This is a source audit of the native Win32 frontend and CMake targets on
2026-11-04. No configure, build, or runtime tests were run for this audit.
Existing verification recorded in [windows.md](windows.md) is historical
evidence, not a fresh result for the working tree. Native mail/news and composer verification is recorded in `windows.md`.

## Recommended next slice

Implement inline images through the original image library, then native HTML
form controls. These address visible browser gaps without first porting optional
engines. The Windows executable already links the parser, layout, image library,
and JPEG decoder; the missing image work is concentrated in the frontend.

For images, connect `FE_GetImageInfo` to `IL_GetImage`, choose a supported color
format, own the decoded pixels/DIBs, propagate intrinsic dimensions and relayout,
draw partial updates and transparency, and release shared image references at
navigation and window destruction. The [image API](../include/il.h) and original
[XFE image adapter](../cmd/xfe/images.c) provide lifecycle evidence; the X11
pixmap implementation itself is not reusable as native GDI code. Start with GIF,
JPEG, and XBM formats already listed in [img.cmake](targets/img.cmake).

Acceptance should include local and loopback HTTP image requests, intrinsic and
explicit dimensions, a transparent GIF, repeated/shared images, failed loads,
scrolling, reload, and closing during decoding. Check rendered pixels as well
as request counts; the current browser smoke only proves text drawing and
placeholder teardown. Next, exercise form editing, radio/checkbox state, select
controls, reset, keyboard focus, scrolling, and exact GET/POST values against a
local server, using the original layout submission APIs.

## Native frontend work

| Priority | Area | Observed implementation and remaining work |
| --- | --- | --- |
| 1 | Images | `FE_GetImageInfo` supplies 48-by-32 fallback geometry; size/data callbacks do not allocate image storage; display draws an outline. Request, decode, draw, relayout, and teardown need integration. |
| 2 | Forms | Form callbacks provide fixed geometry and outlines. Value extraction, reset, toggle state, widget lifetime, and submission interaction are absent. Layout already includes `layform.c`; [XFE forms](../cmd/xfe/forms.c) demonstrates the backend contracts. |
| 3 | Frames | `FE_MakeGridWindow` declines frames; restructuring/history callbacks are empty. Add child contexts, native view ownership, focus, target routing, sizing, navigation, and cleanup. `laygrid.c` is already in the layout archive. |
| 3 | Saving/downloads | General file selection declines the operation; progress callbacks are empty. Add native save selection, an explicit save converter/stream, cancel/error handling, binary preservation, and completion UI. A configured download directory alone is not a download implementation. |
| 3 | Bookmarks/history | Bookmark clipboard transfers exist, while bookmark list/properties/find callbacks decline work. Browser Back/Forward uses a private URL vector and reloads URLs; SHIST contexts are initialized, and netlib owns global-history initialization/update/save calls. Integrate native UI with those backends rather than replacing their databases. |
| 3 | Charset/text | The frontend defaults to Latin-1 and draws/measures through ANSI GDI. A subset of single-byte conversion tables uses Windows code pages; this does not establish correct multilingual rendering. Verify document charset changes, MIME headers, fonts, conversion failure, selection, and copy. |
| 4 | Plugins | The plugin archive and `NPL_Init` are present, but discovery registers no descriptors, loading returns NULL, and embed streaming fails. Native module loading, NPAPI callbacks, embedded-window lifetime, and a controlled test plugin are separate work. |
| 4 | Other browser UI | Add selection/copy, find, printing, preferences, cache controls, navigation command state, and link context actions. Printing/translation archives being linked does not supply native print UI. Disk cache is explicitly disabled at initialization. |

The direct evidence is in [frontend callbacks](../cmd/winfe/frontend.c),
[services](../cmd/winfe/services.c), [window/navigation code](../cmd/winfe/main.cpp),
and the [layout source manifest](targets/lay.cmake). The native resource menu
is in [netscape.rc](../cmd/winfe/netscape.rc). Update stale about/help statements
and [windows.md](windows.md) as each feature becomes verified.

## Build coverage compared with Linux

[Windows core](targets/windows/core.cmake) reuses the shared backend manifests
and adds native NSPR; [Windows frontend](targets/windows/frontend.cmake) links
the corresponding browser backends. The main differences from
[libraries.cmake](targets/libraries.cmake) and the
[Linux browser link](targets/browser.cmake) are:

| Area | Windows state and next build task |
| --- | --- |
| Security | Only the hash archive is included. `secnav`, `secmod`, `pkcs12`, `ssl`, `pkcs7`, `cert`, `key`, `crypto`, and `secutil` are not instantiated by Windows core. Audit their Windows source choices and resources, then compile/archive/test them before enabling browser security and dialogs. Vintage protocol support and modern interoperability are separate questions. |
| JAR/zlib | `netscape_jar` is not instantiated. The shared legacy path finds ZLIB and links it to JAR; Windows core does neither. Establish a native ZLIB dependency and compile/test the four-file JAR manifest independently before security integration. |
| Mocha | Neither `netscape_mocha` nor `netscape_libmocha` is included; layout script sources and the `MOCHA` platform definition are also absent. Port standalone engine/shell first, then browser bindings, event dispatch, and frontend services. Header staging does not enable JavaScript. |
| Java | Windows excludes both browser integration and source-built Sun Java. Existing CMake stages `awt/x`, links Motif/X11 and GNU link groups, and creates Unix-style tool launchers. Port Windows VM, native interface, AWT and tool source selections before browser applets. Native C++ AWT sources and a [Windows makefile](../sun-java/awt/win/makefile.win) are present under `sun-java/awt/win`, but current CMake does not select them. Imported runtime handling is also currently reached through the Linux-oriented platform path. |
| Resources/generation | Windows uses checked-in parser tables and `mkformat.c`; Linux runs parser/MIME/security/localization generators. Wire portable regeneration with explicit tool dependencies and compare outputs before replacing checked-in tables. Native RC strings/menu/icon exist already; full security/localized resources need separate coverage. |
| XFE-only libraries | `netscape_xml` is the Microline XmL Motif widget library, and `netscape_hpane` is an XFE widget. They are not missing HTML/XML parsing engines to add to the native executable. Native controls replace their UI roles. |

The [root option guard](../CMakeLists.txt) explicitly rejects Java, Mocha,
security, and Linux legacy/browser combinations when Windows core is enabled.
Those options require deliberate target/platform separation, not simply extra
link libraries. [NetscapeWindows.cmake](modules/NetscapeWindows.cmake) currently
sets `NO_SSL`, uses static native-thread NSPR and ANSI/IPv4 interfaces, and
rejects x64. Keep x64 as a separate port with pointer/ABI tests.

Use independent configure/build probes for each newly enabled archive, followed
by runtime tests for its actual behavior. The checked-in Windows CTest setup
covers core, socket, local frontend, HTTP frontend, and mail/news fixtures. No optional
engine, native image/form, plugin, frame, or download test result is established
by this audit.
