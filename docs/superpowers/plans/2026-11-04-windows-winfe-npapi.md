# Windows NPAPI plugins Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the original supported NPAPI ABI, browser embedding and streams after Mocha qualification.

**Architecture:** Keep descriptors/modules in one native registry, adapt original FE/NPN/NPP contracts and prove behavior with an in-tree controlled Win32 DLL. Do not substitute later NPAPI APIs.

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

Create `cmake/targets/windows/plugins.cmake` without re-declaring the existing
netscape_plugin archive. Add `cmd/winfe/plugins.h/.cpp` for discovery/ABI/module and
instance ownership, and `plugin-stream.cpp` for only netlib stream adaptation.
The controlled DLL, DEF and native ABI host live in `cmake/tests/plugins/` and
`cmake/tests/windows_plugin_abi.c`. Original public types stay in `include/npapi.h`
and `lib/plugin/npupp.h`. Use-Stage npapi enables Mocha and NETSCAPE_ENABLE_NPAPI;
Java remains disabled. N01 is an executable probe; N02-N08 use C02 native cases.

## Commit sequence

### N01: Build a controlled Win32 plugin and negotiate its ABI

**Depends on:** M08; core C32.

**Files:**

- Modify: `CMakeLists.txt`.
- Create: `cmake/targets/windows/plugins.cmake`.
- Create: `cmake/tests/plugins/winfe-test-plugin.c`.
- Create: `cmake/tests/plugins/winfe-test-plugin.def`.
- Create: `cmake/tests/windows_plugin_abi.c`.
- Create: `cmake/tests/check_winfe_n01.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes include/npapi.h and lib/plugin/npupp.h Windows declarations. Produces NETSCAPE_ENABLE_NPAPI option (OFF during port); winfe_test_plugin DLL and native ABI probe.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_n01` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_n01(r):
    assert r["machine_i386"] == "1"
    assert r["real_entrypoints_called"] == "1"
    assert r["bad_abi_rejected_before_callback"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_n01'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Use exact Windows entrypoint exports/calling conventions from the era headers, not XFE dlopen calling conventions. Negotiate structure sizes/versions; reject wrong-machine, missing-export and unsupported-version fixtures before callbacks. Prove actual entrypoint invocations and clean unload.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_n01'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add controlled Win32 NPAPI ABI and plugin fixtures"`. Follow the master commit-scope rules.

N01 uses a native executable ABI probe instead of a browser smoke; include an independent bad-export DLL and a mismatched-header fixture.

### N02: Discover plugins and persist truthful availability

**Depends on:** N01.

**Files:**

- Create: `cmd/winfe/plugins.h`.
- Create: `cmd/winfe/plugins.cpp`.
- Modify: `cmd/winfe/services.c`.
- Modify: `cmd/winfe/preferences-ui.cpp`.
- Modify: `cmd/winfe/preferences-settings.cpp`.
- Modify: `cmake/targets/windows/plugins.cmake`.
- Create: `cmake/tests/check_winfe_n02.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/n02.inc`.

**Interfaces:** Consumes N01 ABI validator, void FE_RegisterPlugins(void), NPPluginFuncs* FE_LoadPlugin(void*,NPNetscapeFuncs*) and void FE_UnloadPlugin(void*). Produces int WFE_PluginsInit(void); void WFE_PluginsShutdown(void); void WFE_PluginsSetEnabled(int).

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_n02` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_n02(r):
    assert r["mime_registry_matches_modules"] == "1"
    assert r["disabled_module_not_loaded"] == "1"
    assert r["duplicate_and_missing_policy_stable"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_n02'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Own descriptors and loaded-module references in one registry. Default controlled lookup roots are executable-relative plugins and profile plugins; add any original Windows lookup locations only with C01 evidence. Load resolved absolute paths, register supported MIME converters and report missing/disabled plugins accurately. Test duplicate MIME priorities, changed/broken modules and paths containing spaces/percent characters.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_n02'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add native plugin discovery MIME registration and preferences"`. Follow the master commit-scope rules.

### N03: Embed plugin instances with native window lifetimes

**Depends on:** N02.

**Files:**

- Modify: `cmd/winfe/plugins.cpp`.
- Modify: `cmd/winfe/frontend.c`.
- Modify: `cmd/winfe/images.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Create: `cmake/tests/check_winfe_n03.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/n03.inc`.

**Interfaces:** Consumes N02 descriptors; original FE_GetEmbedSize/FE_DisplayEmbed/FE_FreeEmbedElement signatures. Produces void WFE_PluginsReleaseContext(MWContext*) and original NPP instance/window callback paths.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_n03` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_n03(r):
    assert r["instance_window_callback_order"] == "1"
    assert r["clip_resize_focus_match"] == "1"
    assert r["instances_destroyed_once"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_n03'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Create/destroy real instances and child windows; deliver size/clip/visibility changes and focus in correct order. Test multiple instances sharing a module, frames, scrolling, resizing and navigation. Do not promise later windowless APIs absent from baseline headers.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_n03'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Embed Win32 NPAPI instances with native lifetime handling"`. Follow the master commit-scope rules.

### N04: Implement plugin stream flow control and file delivery

**Depends on:** N03.

**Files:**

- Create: `cmd/winfe/plugin-stream.cpp`.
- Modify: `cmd/winfe/plugins.h`.
- Modify: `cmd/winfe/services.c`.
- Modify: `lib/plugin/npglue.c`.
- Create: `cmake/tests/check_winfe_n04.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/n04.inc`.

**Interfaces:** Consumes EmbedStream existing signature and original NP stream/write-ready/write callbacks. Produces original NET_StreamClass producer adapter and terminal delivery status.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_n04` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_n04(r):
    assert r["stream_payload_sha256_matches"] == "1"
    assert r["zero_write_no_spin"] == "1"
    assert r["one_terminal_result_and_temp_cleanup"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_n04'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Deliver real response bytes with partial writes/backpressure, seek/file modes only where baseline supports them, and one terminal stream result. Test binary payload hashes, throttled writes, truncation, errors, cancellation and temporary-file ownership. Never busy-spin when the plugin accepts zero bytes.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_n04'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Implement checked NPAPI stream and file delivery"`. Follow the master commit-scope rules.

### N05: Connect browser callbacks and URL notifications

**Depends on:** N04.

**Files:**

- Modify: `cmd/winfe/plugins.cpp`.
- Modify: `cmd/winfe/plugin-stream.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `lib/plugin/npglue.c`.
- Create: `cmake/tests/check_winfe_n05.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/n05.inc`.

**Interfaces:** Consumes original NPNetscapeFuncs table and existing browser navigation/network services. Produces supported browser callback table entries with matching lifetime and origin.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_n05` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_n05(r):
    assert r["url_post_notify_bytes_and_order"] == "1"
    assert r["target_and_origin_preserved"] == "1"
    assert r["unsupported_callbacks_fail_truthfully"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_n05'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement baseline URL/post/notify, status, user agent and allocation callbacks using request-owned data. Verify named-frame destinations, redirects, owned buffers and callback behavior when a request origin changes; unsupported callbacks return documented errors.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_n05'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Connect original NPAPI browser and URL notification callbacks"`. Follow the master commit-scope rules.

### N06: Harden plugin close, failure and module ownership

**Depends on:** N05.

**Files:**

- Modify: `cmd/winfe/plugins.cpp`.
- Modify: `cmd/winfe/plugin-stream.cpp`.
- Modify: `cmd/winfe/services.c`.
- Modify: `cmd/winfe/main.cpp`.
- Create: `cmake/tests/check_winfe_n06.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/n06.inc`.

**Interfaces:** Consumes N03-N05 ownership rules and WFE_BeginSetModuleState/WFE_EndSetModuleState boundary. Produces registry unload only after final instance/stream/callback release.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_n06` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_n06(r):
    assert r["reentrant_close_no_stale_callback"] == "1"
    assert r["module_retained_until_last_release"] == "1"
    assert r["failed_init_resources_released"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_n06'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Drive plugin callbacks that navigate, close their frame, request another URL or fail initialization. Cancel streams before native context release, reject stale callbacks and never unload executing code. Preserve MFC module state at real boundaries where required; do not assume an independent DLL is part of the executable.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_n06'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Make NPAPI callback teardown and module unloading safe"`. Follow the master commit-scope rules.

### N07: Restore original Mocha plugin-object integration

**Depends on:** M08,N06.

**Files:**

- Modify: `cmd/winfe/plugins.cpp`.
- Modify: `lib/libmocha/lm_plgin.c`.
- Modify: `lib/libmocha/lm_embed.c`.
- Modify: `cmd/winfe/mocha-platform.cpp`.
- Create: `cmake/tests/check_winfe_n07.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/n07.inc`.

**Interfaces:** Consumes qualified Mocha and NPAPI instances; original lm_plgin/lm_embed object contracts. Produces original script-visible plugin/embed objects without enabling later NPObject APIs.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_n07` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_n07(r):
    assert r["original_plugin_objects_match"] == "1"
    assert r["destroyed_object_safe"] == "1"
    assert r["cross_origin_bridge_denied"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_n07'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Test enumeration/properties and every bridge operation supported by the actual original API. Use two local origins and destroyed instances; no script may retain privileged access through a recycled plugin context. Defer Java-dependent calls explicitly to J11.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_n07'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Connect original Mocha plugin and embed objects"`. Follow the master commit-scope rules.

### N08: Qualify NPAPI preferences, printing and packaging

**Depends on:** N01-N07.

**Files:**

- Modify: `cmd/winfe/plugins.cpp`.
- Modify: `cmd/winfe/printing.cpp`.
- Modify: `cmd/winfe/preferences-ui.cpp`.
- Modify: `cmake/tests/run_winfe_matrix.py`.
- Modify: `cmake/modules/NetscapeInstall.cmake`.
- Modify: `cmake/windows-parity.json`.
- Create: `cmake/tests/check_winfe_n08.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/n08.inc`.

**Interfaces:** Consumes N01-N07 controlled plugin and original print/info interfaces. Produces qualified plugin-enabled/disabled standard/Gold configurations and developer test-plugin package component.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_n08` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_n08(r):
    assert r["plugin_print_callback_verified"] == "1"
    assert r["enabled_disabled_matrix_pass"] == "1"
    assert r["installed_discovery_verified"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_n08'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Complete baseline plugin information/refresh and print callback behavior, verify plugin-off core operation and installed discovery with no source fallback. Test-plugin binaries belong to a test/developer component, not the user package by accident. Run Gate-Engine npapi with Mocha on and independent plugin-off checks.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_n08'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Qualify native NPAPI UI printing and installed discovery"`. Follow the master commit-scope rules.

## Handoff and gate

Requires M08. Re-run forms/frames, downloads, TLS, print, Mocha origin/lifetime,
core command-state and engine-disabled cases. N08 must pass before Java begins.
Third-party historic plugin trials are optional evidence; the controlled DLL
must deterministically prove every supported callback and stream contract.
