# Windows Sun Java AWT and applets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the original source-built Java runtime, native AWT and applet/browser bridges after Mocha and NPAPI qualification.

**Architecture:** Use original Windows VM/AWT makefiles and JRI/class contracts, with build-tree generation and native thread/lifetime adapters. Do not substitute a modern JVM or Unix AWT.

**Tech Stack:** Legacy C, C++14/MFC, Win32 x86/MSVC, CMake/CTest and Python standard-library fixtures; original 3.02 engine APIs.

**Spec:** [Approved Windows parity design](../specs/2026-11-04-windows-winfe-parity-design.md); [master plan](2026-11-04-windows-winfe-completion.md). Read both before execution.

## Global Constraints

- Retain native MFC/Win32 controls, GDI rendering and system colors.
- Target the existing Win32/x86 ABI first; native x64 is a separate follow-up.
- Preserve original engines, legacy source encodings, profiles, mailboxes and recoverable edits.
- Core application and Gold UI parity precede Mocha, NPAPI and Java, in that order.
- Keep shared engine changes narrow and verify affected XFE/Linux paths where available.
- Keep the approved BearSSL HTTPS policy; do not restore SSL 2/3 or bypass certificate validation.
- Run GUI fixtures serially; never use live mail, credentials, public posting or publishing as fixtures.
- One reviewable item per commit, including its behavior tests and parity-ledger update.
- Read the master plan for build commands, native fixture contracts, gates and exception handling.

## Review Focus

- Malformed or truncated external data must fail without replacing valid profile data (core C11/C13/C15/C18/C20/C24/C25; engine parser/stream tasks).
- Closing/navigating during callbacks must not leave stale native handles or engine contexts (C14/C30, M05, N06, J07/J08).
- Two windows, profiles or origins must not inherit each other's mutable state (C08/C10/C12, M06, N05/N07, J09/J10).
- Disabled/failed engines must preserve core application behavior and truthful UI state (C07, M08, N08, J12, R04).
- Installed paths, ABI mismatches and missing resources must yield useful errors without source-tree fallback (C09, N01/N02, J04/J12, R03).

---

## File and interface map

`cmake/targets/windows/java.cmake` owns Windows runtime/tool/AWT selections;
portable generation stays in existing `sun_java_generate.py`/shared manifests.
`cmd/winfe/java-platform.h/.cpp` owns VM startup/availability and native lifecycle.
Java fixtures live in `cmake/tests/java/`; compatible compiled fixture classes are
reproducible build/test assets with compiler/version provenance, not hand-edited
binary magic. J01 records every exact VM/peer/class file selected by the Windows
makefiles; later files below must match that map before implementation.
Use-Stage java enables Mocha, NPAPI, NETSCAPE_ENABLE_JAVA and
NETSCAPE_BUILD_SUN_JAVA with NETSCAPE_JAVA_PROVIDER=SOURCE. Do not enable Unix
NETSCAPE_BUILD_LEGACY to work around current Windows option guards.

## Commit sequence

### J01: Select original Windows VM targets and generate resources

**Depends on:** N08; core C32.

**Files:**

- Modify: `CMakeLists.txt`.
- Create: `cmake/targets/windows/java.cmake`.
- Modify: `cmake/targets/sun-java.cmake`.
- Modify: `cmake/modules/NetscapeJava.cmake`.
- Modify: `cmake/modules/sun_java_generate.py`.
- Create: `cmake/tests/windows_java_probe.c`.
- Create: `cmake/tests/check_winfe_j01.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original Windows makefiles, shared Sun Java manifests and C01 VM/platform audit. Produces Windows netscape_sun_java_headers/config targets and native selected archives without X11 dependencies.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j01` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j01(r):
    assert r["probe_executes"] == "1"
    assert r["generated_resources_match"] == "1"
    assert r["unix_awt_dependency_count"] == "0"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j01'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Translate Win32 source choices into focused Windows CMake while reusing portable manifests/generators. Relax option guards only for supported native combinations; preserve Linux behavior and reject unsupported provider/ABI configurations clearly. Generate opcodes/native resources into the build tree with explicit dependencies and compare to canonical inputs. Prove a linked native probe rather than only archives.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j01'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add native Windows Sun Java target and resource selection"`. Follow the master commit-scope rules.

This and J02-J04 use non-GUI native probes. Windows source selection must not include Unix AWT sources or silently pull in Motif/GNU rescan groups.

### J02: Execute the original VM and essential class library

**Depends on:** J01.

**Files:**

- Modify: `cmake/targets/windows/java.cmake`.
- Modify: `cmake/targets/sun-java/javai.cmake`.
- Modify: `cmake/targets/sun-java/javart.cmake`.
- Modify: `sun-java/runtime/interpreter.c`.
- Create: `cmake/tests/java/ParityMain.java`.
- Modify: `cmake/tests/windows_java_probe.c`.
- Create: `cmake/tests/check_winfe_j02.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes J01 generated headers and original interpreter/class-file interfaces. Produces native Java launcher/probe with original class-format/version diagnostics.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j02` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j02(r):
    assert r["class_program_output"] == "PARITY-JAVA-OK"
    assert r["exception_cases_match"] == "1"
    assert r["bad_class_version_rejected"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j02'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Run arithmetic, objects, arrays, strings, exceptions, class loading and basic I/O against controlled classes. Keep compiler/class version and runtime library provenance explicit; no dependency on a globally installed modern Java. A missing or incompatible class archive is an error, not a no-op successful launch.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j02'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Execute original Java runtime and class library on Win32"`. Follow the master commit-scope rules.

### J03: Qualify VM threads, GC and native stack interaction

**Depends on:** J02.

**Files:**

- Modify: `sun-java/runtime/gc.c`.
- Modify: `sun-java/md/gc_md.c`.
- Modify: `sun-java/md/thread_md.c`.
- Modify: `cmake/targets/windows/java.cmake`.
- Create: `cmake/tests/java/ParityThreads.java`.
- Modify: `cmake/tests/windows_java_probe.c`.
- Create: `cmake/tests/check_winfe_j03.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes Win32 native-thread NSPR, original VM GC/root registration and J02 launcher. Produces original runtime threading/GC APIs with correct Win32 stack/register handling.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j03` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j03(r):
    assert r["gc_live_objects_preserved"] == "1"
    assert r["thread_monitor_cases_match"] == "1"
    assert r["shutdown_with_active_threads_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j03'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Confirm the Win32 conditional branches and register/stack contracts in gc_md.c and thread_md.c against the Windows makefile before editing. Test allocation pressure, exceptions, synchronized waits, thread start/stop, native calls and shutdown. Verify live objects survive GC and stopped threads do not retain contexts. No pointer-to-32-bit truncation workarounds outside the Win32 contract.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j03'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Qualify Win32 Java threading stack roots and GC"`. Follow the master commit-scope rules.

Extend the J01 source-selection map before changing an additional VM source; do not create a guessed substitute runtime implementation.

### J04: Build JRI/native exports and vintage tools

**Depends on:** J03.

**Files:**

- Modify: `cmake/targets/windows/java.cmake`.
- Modify: `cmake/targets/sun-java/jri.cmake`.
- Modify: `cmake/targets/sun-java/javah.cmake`.
- Modify: `cmake/targets/sun-java/javap.cmake`.
- Modify: `cmake/modules/sun_java_generate.py`.
- Create: `cmake/tests/java/ParityNative.java`.
- Create: `cmake/tests/check_winfe_j04.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes include/jri.h and generated _gen stubs; original Windows export conventions. Produces source-built native-interface/tool targets and reproducible export/header/stub artifacts.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j04` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j04(r):
    assert r["native_method_roundtrip"] == "1"
    assert r["exports_and_generated_stubs_match"] == "1"
    assert r["vintage_tools_execute"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j04'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Verify javah/javap and native invocation with an actual controlled native method and exception/array/string exchange. Match original DLL basenames, exported symbols and calling conventions from makefiles rather than newer JNI assumptions. Rebuilding classes requires an explicit compatible compiler command and pinned inputs; retain checked-in compatible bootstrap classes when unavailable.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j04'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Build and verify original Java native interfaces and tools"`. Follow the master commit-scope rules.

### J05: Port native AWT drawing, fonts and image peers

**Depends on:** J04.

**Files:**

- Modify: `cmake/targets/windows/java.cmake`.
- Modify: `sun-java/awt/win/awt_graphics.cpp`.
- Modify: `sun-java/awt/win/awt_font.cpp`.
- Modify: `sun-java/awt/win/awt_image.cpp`.
- Modify: `sun-java/awt/win/awt_toolkit.cpp`.
- Create: `cmake/tests/java/ParityGraphics.java`.
- Create: `cmake/tests/check_winfe_j05.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/j05.inc`.

**Interfaces:** Consumes original awt/win/makefile.win source list and J04 native stubs. Produces source-built original Win32 AWT library and real drawing peer callbacks.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j05` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j05(r):
    assert r["awt_drawn_pixels_match"] == "1"
    assert r["font_metrics_and_clip_match"] == "1"
    assert r["peer_resources_released"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j05'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Wire the actual Windows AWT sources/resources, correct ABI/compiler compatibility narrowly, and compare text metrics, clipping, colors, image decode/transparency and resource ownership in native captures. Keep original DLL names expected by Java class/native lookup. Test failed image loads and repeated peer construction/destruction.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j05'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Port original Win32 AWT drawing font and image peers"`. Follow the master commit-scope rules.

### J06: Complete AWT widgets, focus and native dialogs

**Depends on:** J05.

**Files:**

- Modify: `sun-java/awt/win/awt_component.cpp`.
- Modify: `sun-java/awt/win/awt_window.cpp`.
- Modify: `sun-java/awt/win/awt_textfield.cpp`.
- Modify: `sun-java/awt/win/awt_textarea.cpp`.
- Modify: `sun-java/awt/win/awt_button.cpp`.
- Modify: `sun-java/awt/win/awt_listbox.cpp`.
- Modify: `sun-java/awt/win/awt_dialog.cpp`.
- Create: `cmake/tests/java/ParityWidgets.java`.
- Create: `cmake/tests/check_winfe_j06.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/j06.inc`.

**Interfaces:** Consumes J05 peer infrastructure and original AWT native declarations. Produces working original Windows widget peers; no substitute modern toolkit.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j06` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j06(r):
    assert r["widget_input_focus_match"] == "1"
    assert r["clipboard_dialog_cancel_match"] == "1"
    assert r["missing_peer_families"] == "0"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j06'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Port controls, menus and dialogs identified by the AWT source manifest. Test keyboard/selection/focus, clipboard, scroll/layout and owner close with modal dialogs. C01/J01 inventory assigns any remaining peer families to concrete addendum commits before J12; the listed source examples are not permission to omit others.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j06'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete original Win32 AWT widget and dialog behavior"`. Follow the master commit-scope rules.

### J07: Make AWT event delivery and teardown safe

**Depends on:** J03,J06.

**Files:**

- Modify: `sun-java/awt/win/awt_event.cpp`.
- Modify: `sun-java/awt/win/awt_toolkit.cpp`.
- Modify: `sun-java/awt/win/awt_object.cpp`.
- Create: `cmd/winfe/java-platform.h`.
- Create: `cmd/winfe/java-platform.cpp`.
- Modify: `cmd/winfe/platform.c`.
- Create: `cmake/tests/java/ParityEvents.java`.
- Create: `cmake/tests/check_winfe_j07.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/j07.inc`.

**Interfaces:** Consumes original AWT event dispatch and J03 threads/GC contract. Produces int WFE_JavaInit(void); void WFE_JavaShutdown(void); int WFE_JavaAvailable(void).

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j07` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j07(r):
    assert r["event_thread_affinity"] == "1"
    assert r["reentrant_shutdown_safe"] == "1"
    assert r["callback_after_hwnd_destroy"] == "0"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j07'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Establish correct UI-thread dispatch and VM/native synchronization without blocking the browser pump. Test nested events, waits, dialogs, resize and shutdown from callbacks. Explicitly prove no Java thread touches a destroyed HWND or waits on a dead UI owner.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j07'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Make native Java AWT event delivery and shutdown safe"`. Follow the master commit-scope rules.

### J08: Integrate applet embedding and browser lifecycle

**Depends on:** J07.

**Files:**

- Modify: `cmake/targets/windows/java.cmake`.
- Modify: `cmake/targets/java.cmake`.
- Modify: `cmd/winfe/java-platform.cpp`.
- Modify: `cmd/winfe/frontend.c`.
- Modify: `lib/libjava/lj_init.c`.
- Modify: `lib/libjava/lj_embed.c`.
- Modify: `lib/libjava/lj_run.c`.
- Modify: `lib/layout/layjava.c`.
- Create: `cmake/tests/java/ParityApplet.java`.
- Create: `cmake/tests/check_winfe_j08.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/j08.inc`.

**Interfaces:** Consumes LJ_StartupJava and original FE_GetJavaAppSize/Display/Hide/Free callbacks. Produces void WFE_JavaReleaseContext(MWContext*); original LJ applet init/start/stop/destroy paths.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j08` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j08(r):
    assert r["applet_lifecycle_order"] == "1"
    assert r["parameters_and_resize_match"] == "1"
    assert r["closed_loading_context_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j08'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Create real applet contexts with correct size, parameters, class loading and native AWT children. Test frames, scrolling/resize, hidden/visible transitions, reload/stop/back and browser closure while loading. Destroy applets and release Java roots before context/native-window teardown.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j08'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Embed original Java applets in native browser contexts"`. Follow the master commit-scope rules.

### J09: Verify applet class origin and sandbox enforcement

**Depends on:** J08.

**Files:**

- Modify: `cmd/winfe/java-platform.cpp`.
- Modify: `lib/libjava/lj_run.c`.
- Modify: `lib/libjava/lj_init.c`.
- Modify: `sun-java/classsrc/netscape/applet/AppletSecurity.java`.
- Create: `cmake/tests/java/ParitySandbox.java`.
- Create: `cmake/tests/check_winfe_j09.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/j09.inc`.

**Interfaces:** Consumes original applet security/class-loader implementation and two controlled origins. Produces original permissions enforced by actual runtime, including denial diagnostics.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j09` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j09(r):
    assert r["same_origin_applet_resource_allowed"] == "1"
    assert r["foreign_file_process_access_denied"] == "1"
    assert r["cached_context_privilege_not_inherited"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j09'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Execute allowed same-origin resources and denied foreign-origin, arbitrary file and process access through the real VM. Check redirects, cached classes, context reuse and failed loads do not inherit privilege. Map actual class source locations at J01; if absent, treat restoring enforcement as a blocker, not an allow-all shim.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j09'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Verify native applet class origin and sandbox boundaries"`. Follow the master commit-scope rules.

### J10: Restore original Mocha/Java bridge

**Depends on:** M08,J09.

**Files:**

- Modify: `mocha/src/mo_java.c`.
- Modify: `lib/libmocha/lm_applt.c`.
- Modify: `lib/libmocha/lm_win.c`.
- Modify: `cmake/targets/sun-java/njs.cmake`.
- Modify: `cmd/winfe/mocha-platform.cpp`.
- Modify: `cmd/winfe/java-platform.cpp`.
- Create: `cmake/tests/java/ParityScriptBridge.java`.
- Create: `cmake/tests/check_winfe_j10.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/j10.inc`.

**Interfaces:** Consumes qualified M08/J09 engines; original JRI/JavaScript bridge contracts. Produces original supported Java/script conversions, calls and object ownership.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j10` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j10(r):
    assert r["bridge_values_and_exceptions_match"] == "1"
    assert r["bridge_origin_denials_match"] == "1"
    assert r["bridge_gc_and_navigation_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j10'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Exercise numbers/strings/arrays/objects, exceptions and script-to-applet/applet-to-script paths supported by this source era. Verify origin checks, retained Java/GC references and navigation during calls. Keep unsupported modern LiveConnect/JSObject APIs outside claims unless baseline demonstrates them.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j10'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Restore original Mocha Java bridge with ownership checks"`. Follow the master commit-scope rules.

### J11: Restore original NPAPI Java bridge where supported

**Depends on:** N08,J09,J10.

**Files:**

- Modify: `lib/plugin/npjava.c`.
- Modify: `lib/plugin/javap.c`.
- Modify: `cmd/winfe/plugins.cpp`.
- Modify: `cmd/winfe/java-platform.cpp`.
- Modify: `cmake/targets/windows/java.cmake`.
- Modify: `cmake/tests/plugins/winfe-test-plugin.c`.
- Create: `cmake/tests/check_winfe_j11.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/j11.inc`.

**Interfaces:** Consumes N08 native plugin, J09 runtime and original Java-enabled NPAPI entries. Produces original Java class/environment callbacks backed by real JRI objects.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j11` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j11(r):
    assert r["plugin_java_object_roundtrip"] == "1"
    assert r["invalid_or_dead_java_handle_rejected"] == "1"
    assert r["plugin_java_unload_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j11'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Wire only bridge entries present in the original Windows headers/core. Have the controlled plugin exchange a real Java object and survive GC, navigation and plugin unload. Deny invalid handles/context origin and keep absent Java runtime behavior truthful. A baseline with no supported bridge receives evidence-backed disposition, not fake success objects.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j11'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Connect supported original NPAPI Java bridge callbacks"`. Follow the master commit-scope rules.

### J12: Qualify Java console, settings and installed resources

**Depends on:** J01-J11 and required class/peer addenda.

**Files:**

- Modify: `cmd/winfe/java-platform.cpp`.
- Modify: `cmd/winfe/preferences-ui.cpp`.
- Modify: `cmd/winfe/preferences-settings.cpp`.
- Modify: `cmake/modules/NetscapeInstall.cmake`.
- Modify: `cmake/tests/run_winfe_matrix.py`.
- Modify: `cmake/windows-parity.json`.
- Create: `cmake/tests/check_winfe_j12.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/j12.inc`.

**Interfaces:** Consumes J01-J11 runtime/class/native resources and core command/reference inventory. Produces void WFE_JavaSetEnabled(int); qualified Java-enabled/disabled packages and original console/diagnostics.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_j12` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_j12(r):
    assert r["installed_runtime_and_applet_pass"] == "1"
    assert r["missing_runtime_error_truthful"] == "1"
    assert r["java_disabled_core_pass"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_j12'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Complete original enable/console/status/missing-runtime workflows. Install runtime archive, native libraries and class resources with provenance/licenses; verify launch/console/applet loading without source/build/global-Java fallback. Test missing/mismatched resources and stage enable changes safely during live applets. Run Gate-Engine java and all required J01 peer/library addenda.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_j12'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Qualify native Java UI runtime packaging and disabled builds"`. Follow the master commit-scope rules.

## Handoff and gate

Requires N08. J12 requires all real VM/AWT/class/bridge cases and all inventory
addenda; a class archive or linked runtime alone is not applet parity. Re-run
Mocha/NPAPI origin and teardown, forms/frames, native preferences, TLS and
Java-disabled core checks. Continue to security/qualification after J12.
