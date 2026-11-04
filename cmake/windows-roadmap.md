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
The next priorities are native saving/downloads and completing
history/bookmark/navigation UI. Optional engines remain separate porting tasks.

## Native frontend work

| Priority | Area | Observed implementation and remaining work |
| --- | --- | --- |
| Implemented | Images | Native IL/GDI adapter decodes, paints, scales, tiles backgrounds and releases shared images. Progressive/animated timing and broader format/error cases need further coverage. |
| Implemented | Forms | Native controls collect/reset values and submit through LO. File picker exists; multipart uploads, offscreen keyboard traversal and full history restoration need runtime coverage. |
| Implemented | Frames | Native child contexts use the original grid/history engine. Fixtures cover nested views, named/parent/top targets, streamed server-directed target changes, child forms/images, resize and cleanup. Draggable divider UI and broader interrupted-load cases need further coverage. |
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
covers core, socket, local frontend, HTTP frontend, and mail/news fixtures. No optional engine, plugin, frame, or download runtime result is established
by this audit. Native image/form checks are now part of the frontend suite.
