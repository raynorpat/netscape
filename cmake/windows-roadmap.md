# Windows port backlog

This tracks the native Win32 frontend and CMake target gaps as of 2026-11-04.
Verified builds and runtime checks are recorded in [windows.md](windows.md).
Remaining items below come from source inspection and are not implied to work
merely because their backend archive builds.

## Completed browser slice and next work

Native inline GIF/JPEG/XBM images and HTML form controls now use the original
3.02 engines. Image fixtures verify actual GDI pixels, transparency, scaling,
shared clients, failed loads/backgrounds, local paths and HTTP. Form fixtures
verify native reset/scrolling/cleanup, Enter, exact GET/POST values and image
submit coordinates. See [windows.md](windows.md) for verification and limits.

Native frame contexts now support nested cells, named and special targets,
child forms/images, independent scrolling, cache/history-based resize and cleanup.
Native saving/downloads now use independent transfer windows and checked binary
streams. Native history traversal and its dialog now use SHIST, including frame history,
forms/scroll state and persisted visited-page dates. A native bookmark manager
uses the original tree and HTML persistence. Original menu trees, live Go/Bookmarks/Window entries and supported MSG command
routes now have native coverage. Remaining browser UI and optional engines are
separate porting tasks.

## Native frontend work

| Priority | Area | Observed implementation and remaining work |
| --- | --- | --- |
| Implemented | Images | Native IL/GDI adapter decodes, paints, scales, tiles backgrounds and releases shared images. Progressive/animated timing and broader format/error cases need further coverage. |
| Implemented | Forms | Native controls collect/reset values and submit through LO. File picker exists; multipart uploads, offscreen keyboard traversal and full history restoration need runtime coverage. |
| Implemented | Frames | Native child contexts use the original grid/history engine. Fixtures cover nested views, named/parent/top targets, streamed server-directed target changes, child forms/images, resize and cleanup. Original LO edges now draw classic bevels and support captured vertical/horizontal dragging with a nonactivating tracker, bounds, NORESIZE, Escape and capture-loss cancellation. Same-size history restores divider positions; form values survive history and window resizing. Broader interrupted-load cases need further coverage. |
| Implemented | Saving/downloads | Save Page and link/image context actions use native pickers and independent progress/cancel windows. Fixtures cover exact bytes, final-response headers, truncation, cancellation, concurrent transfers, browser closure and filesystem failures. Known renderable attachment enforcement, transformed/chunked responses and large-file limits need further work. |
| Implemented | History | Back/Forward and Reload use SHIST; a native history list and visited-pages view expose the core data. Fixtures cover form/scroll restoration, frame traversal, forward truncation, POST cancellation, persisted dates and malformed/expired records. |
| Implemented | Bookmarks | Add page and a native tree manager support folders, editing, opening and deletion through the original BM APIs. Fixtures check escaped and long names/URLs, ordering, cross-process persistence and failed-save retry. Import/export, search, drag/drop and richer organization remain future work. |
| Implemented | Classic mail/news UI | Own menus and ten-button toolbar, folder/message lists above a full-width reader, draggable dividers, original icons, sorting, Previous/Next, header modes, selected-message caption and mailbox status. Populated MIME captures, scrolling and narrow layouts are covered. Reader printing uses the shared native vector workflow. |
| Implemented | Classic News UI | Separate twelve-button post/mail/reply toolbar and menus; expandable server/group tree with subscription controls, unread counts and original artwork; original pane dimensions and white empty reader. Native gestures, composition routing, mark-thread/group and article-load view retention use loopback coverage. |
| Implemented | Classic composer UI | Reference menus, five-button toolbar/logo, compact recipient/attachment rows, fixed-width editor and segmented status. Advanced headers, drafts, Send Later and attachments are retained. Quote and active SMTP/NNTP Stop/retry are covered; Address remains disabled. |
| Implemented | General Preferences | Seven original tabs, atomic profile persistence, toolbar style/startup/home, fonts, colors/override, history, IL options and ordered HTTP languages. Real UI, persistence and rendered-page checks exist. Background-image files now support classic Colors controls, original page/override rules and missing-file startup fallback. Apps now supports explicit executable paths for source viewing and Telnet/TN3270, plus a validated temporary directory. Other encodings and editable helper associations remain disabled. Original release observations and gaps are in [windows-reference.md](windows-reference.md). |
| Implemented | Gold HTML editing | Optional `NETSCAPE_BUILD_WINDOWS_EDITOR=ON` configuration builds the original EDT editor with native GDI document views, MENU 101 editor/MENU 2 browser resources, three icon-only bars, formatting, insertion/property dialogs and local save/progress callbacks. Use a separate `out/windows-gold` build and `--edit [path]`. Native printing/preview, original-template Editor Preferences, independent templates, offline wizard, external HTML source import and HTTP/FTP publishing are implemented. All twenty-seven Gold tests passed in Win32 Debug and Release, including real EDT editing/save/reopen, menu/resource checks and save/cancel failure cases. Advanced dialog parity, asynchronous image-save cancellation and broader DPI/charset behavior remain unverified. |
| 3 | Charset/text | The frontend defaults to Latin-1 and draws/measures through ANSI GDI. A subset of single-byte conversion tables uses Windows code pages; this does not establish correct multilingual rendering. Verify document charset changes, MIME headers, fonts, conversion failure, selection, and copy. |
| 4 | Plugins | The plugin archive and `NPL_Init` are present, but discovery registers no descriptors, loading returns NULL, and embed streaming fails. Native module loading, NPAPI callbacks, embedded-window lifetime, and a controlled test plugin are separate work. |
| Implemented | Classic chrome | Browser toolbar, location dropdown, shortcuts/logo, traditional menus and segmented status retain the Gold-era layout and artwork. Browser, Mail/News, message composer, Gold and utility/preferences surfaces now use the Windows system palette, native controls and system bevels rather than fixed gray. Document colors and artwork transparency remain independent of the UI palette. Native Find supports selection, case/direction/wrap and focused frames. View toggles and keyboard location entry are covered. |
| 4 | Other browser UI | Mouse selection now supports capture, forward/backward drag, Shift extension, original word selection, autoscroll and focused child-frame Copy. Link hover, Open/Open New Window, Copy Link/Image Location, View Image and image-map targets now share one hit test. Background-image preferences are implemented. Supporting applications are implemented. Finish helper associations, cache controls and remaining navigation actions. Copy/Select All, message finding, source/info, image loading and native Find exist; Gold editing is optional as recorded above, and native printing/preview is available for document views; physical printer-driver output and composition printing remain unverified. Disk cache is explicitly disabled at initialization. |

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
| Gold editor | `NETSCAPE_BUILD_WINDOWS_EDITOR=ON` adds the original editor manifest and applies `EDITOR`/`GOLD` consistently to the native build. It requires Windows core and frontend; keep standard and Gold build directories separate. See [windows.md](windows.md) for commands and current runtime verification limits. |
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
by runtime tests for its actual behavior. The checked-in Windows CTest setup covers core, socket, local/HTTP frontend,
mail/news, images, forms, frames, downloads, history, bookmarks, classic chrome and
mail/news pane layouts, composition and General Preferences. These fixtures
do not establish optional engine or plugin runtime support.
