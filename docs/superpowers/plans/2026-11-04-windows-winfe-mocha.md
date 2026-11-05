# Windows Mocha engine and browser integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable the original JavaScript engine and its working browser bindings after core/UI parity.

**Architecture:** Reuse netscape_mocha and netscape_libmocha with native lifecycle/event adapters. Java and plugin bridges stay blocked until their respective engines pass.

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

Create `cmake/targets/windows/mocha.cmake` for Windows source/define/link selection,
`cmd/winfe/mocha-platform.h/.cpp` for native lifecycle/event/dialog adapters, and
`cmake/tests/windows_mocha_engine.c` for a real standalone interpreter probe.
Consume the original public interfaces in `include/libmocha.h` and
`mocha/include/mochaapi.h`; do not duplicate or change their types casually.
C02 owns native fixture dispatch; M02-M08 add test includes to that dispatcher.
Use-Stage mocha selects NETSCAPE_ENABLE_MOCHA=ON and no Java/NPAPI until later.

## Commit sequence

### M01: Build and execute standalone original Mocha

**Depends on:** core C32 passed.

**Files:**

- Modify: `CMakeLists.txt`.
- Create: `cmake/targets/windows/mocha.cmake`.
- Modify: `cmake/targets/windows/core.cmake`.
- Modify: `cmake/targets/mocha.cmake`.
- Create: `cmake/tests/windows_mocha_engine.c`.
- Create: `cmake/tests/check_winfe_m01.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes MOCHA_NewContext, MOCHA_EvaluateBuffer and MOCHA_DestroyContext from mocha/include/mochaapi.h; mocha/makefile.win. Produces netscape_mocha and netscape_windows_mocha_engine native targets; supported Windows NETSCAPE_ENABLE_MOCHA option.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_m01` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_m01(r):
    assert r["arithmetic_result"] == "3"
    assert r["language_cases_and_errors"] == "1"
    assert r["context_gc_cycles_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_m01'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Select original Windows-compatible sources/defines without enabling Unix legacy libraries. Keep Java hooks disabled explicitly. Run interpreter, arrays/objects/functions/strings/math/date and syntax-error/GC checks in a small C host; reuse original language tests after classifying expected era behavior. Permit the shell option only when its real file interface builds.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_m01'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Build and test original Mocha natively on Win32"`. Follow the master commit-scope rules.

This is an executable engine probe, not a GUI smoke. Add direct CTest names under mocha_engine_ for small deterministic cases and include them in Check-Item/Gate-Engine.

### M02: Attach libmocha to native document contexts

**Depends on:** M01.

**Files:**

- Modify: `cmake/targets/windows/mocha.cmake`.
- Modify: `cmake/targets/libmocha.cmake`.
- Create: `cmd/winfe/mocha-platform.h`.
- Create: `cmd/winfe/mocha-platform.cpp`.
- Modify: `cmd/winfe/frontend.c`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `lib/layout/laymocha.c`.
- Create: `cmake/tests/check_winfe_m02.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/m02.inc`.

**Interfaces:** Consumes LM_InitMocha, LM_GetMochaDecoder and LM_RemoveWindowContext from include/libmocha.h. Produces int WFE_MochaInit(void); void WFE_MochaShutdown(void); void WFE_MochaReleaseContext(MWContext*).

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_m02` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_m02(r):
    assert r["inline_external_script_rendered"] == "1"
    assert r["syntax_error_reported"] == "1"
    assert r["decoder_contexts_released"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_m02'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Wire original script parsing/stream execution and document decoders. Initialize/shutdown once, implement required native FE services and error display, and release decoders before native contexts disappear. Keep Java/plugin bindings explicitly unavailable, with no success stubs.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_m02'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Integrate libmocha with native document contexts"`. Follow the master commit-scope rules.

### M03: Complete script document, navigation and frame bindings

**Depends on:** M02.

**Files:**

- Modify: `cmd/winfe/mocha-platform.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `lib/libmocha/lm_doc.c`.
- Modify: `lib/libmocha/lm_win.c`.
- Modify: `lib/libmocha/lm_hist.c`.
- Modify: `lib/libmocha/lm_href.c`.
- Create: `cmake/tests/check_winfe_m03.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/m03.inc`.

**Interfaces:** Consumes M02 context lifecycle and existing SHIST/frame targets. Produces original window/document/location/history bindings with native window routing.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_m03` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_m03(r):
    assert r["document_window_navigation_match"] == "1"
    assert r["named_frame_targets_match"] == "1"
    assert r["closed_window_reference_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_m03'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Drive document.write, properties, open/close, location/history, named frames and image/link access using original object models. Verify queued navigation and callback reentrancy without inventing modern DOM semantics.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_m03'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete native Mocha document window and frame bindings"`. Follow the master commit-scope rules.

### M04: Connect scripted form and input events

**Depends on:** M03.

**Files:**

- Modify: `cmd/winfe/forms.cpp`.
- Modify: `cmd/winfe/mocha-platform.cpp`.
- Modify: `lib/libmocha/lm_form.c`.
- Modify: `lib/libmocha/lm_input.c`.
- Create: `cmake/tests/check_winfe_m04.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/m04.inc`.

**Interfaces:** Consumes original LM form/input APIs and native WFE_Form callbacks. Produces original click/change/focus/blur/reset/submit dispatch with truthful return handling.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_m04` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_m04(r):
    assert r["event_order_matches_reference"] == "1"
    assert r["cancelled_submit_requests"] == "0"
    assert r["detached_input_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_m04'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Run existing lib/libmocha/tests form fixtures and controlled native gestures. Verify canceled onSubmit sends nothing, programmatic changes update controls, reset restores values and detached elements cannot trigger stale handlers.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_m04'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Connect native forms to original Mocha events"`. Follow the master commit-scope rules.

### M05: Make script timers and dialogs safe through reentrancy

**Depends on:** M03,M04.

**Files:**

- Modify: `cmd/winfe/mocha-platform.cpp`.
- Modify: `cmd/winfe/platform.c`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `lib/libmocha/lm_win.c`.
- Create: `cmake/tests/check_winfe_m05.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/m05.inc`.

**Interfaces:** Consumes original timer/event/dialog callbacks and M02 context ownership. Produces native timer/dialog implementation of existing FE signatures; no timer retains a dead context.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_m05` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_m05(r):
    assert r["timer_order_and_cancel"] == "1"
    assert r["dialog_navigation_reentrancy_safe"] == "1"
    assert r["callbacks_after_context_release"] == "0"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_m05'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Cover timer cancellation, modal alert/confirm/prompt, unload, reload and closing from callbacks. Drive original event ordering; do not block the native network pump indefinitely or dispatch after destruction.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_m05'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Make native Mocha timers and dialogs lifetime safe"`. Follow the master commit-scope rules.

### M06: Verify original script origin and taint enforcement

**Depends on:** M03-M05.

**Files:**

- Modify: `lib/libmocha/lm_taint.c`.
- Modify: `lib/libmocha/lm_url.c`.
- Modify: `cmd/winfe/mocha-platform.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Create: `cmake/tests/check_winfe_m06.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/m06.inc`.

**Interfaces:** Consumes LM_CheckGetURL and original taint/origin rules; two local origins. Produces existing policy checks preserved across redirects, frames and reused contexts.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_m06` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_m06(r):
    assert r["same_origin_allowed"] == "1"
    assert r["cross_origin_secret_leaked"] == "0"
    assert r["context_reuse_keeps_policy"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_m06'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Use two distinct loopback origins and synthetic secrets. Assert allowed same-origin behavior and original-policy denial of cross-origin frame/form/cookie reads; verify no privilege inheritance after redirect, history or window reuse. Missing native policy adapters fail closed; do not rewrite rules as a modern security model without design approval.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_m06'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Verify Mocha origin and taint boundaries on Windows"`. Follow the master commit-scope rules.

### M07: Enable baseline automatic proxy scripts and script preferences

**Depends on:** M05,M06,C10.

**Files:**

- Modify: `cmd/winfe/network-preferences.cpp`.
- Modify: `cmd/winfe/preferences-settings.cpp`.
- Modify: `cmd/winfe/preferences-ui.cpp`.
- Modify: `cmd/winfe/mocha-platform.cpp`.
- Modify: `lib/libnet/mkautocf.c`.
- Create: `cmake/tests/check_winfe_m07.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/m07.inc`.

**Interfaces:** Consumes C10 manual proxy settings and original NET_ProxyAutoConfig producer. Produces void WFE_MochaSetEnabled(int); automatic-proxy controls active only with a functioning supported interpreter.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_m07` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_m07(r):
    assert r["pac_routes_requests_correctly"] == "1"
    assert r["bad_pac_action_matches_baseline"] == "1"
    assert r["engine_off_preferences_truthful"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_m07'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement confirmed original script enable/error-console behaviors and automatic proxy configuration using original APIs. Cover direct/proxy decisions, malformed PAC, interrupted fetch, unavailable engine and changes during active requests. Preserve current TLS destination identity and no silent policy bypass.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_m07'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Enable original proxy scripts and native script preferences"`. Follow the master commit-scope rules.

Only implement PAC if C01 confirms the shipped/configured interpreter route; otherwise retain an evidence-backed explicit disposition, never an inert enabled control.

### M08: Qualify Mocha standard/Gold and disabled builds

**Depends on:** M01-M07.

**Files:**

- Modify: `cmake/tests/run_winfe_matrix.py`.
- Modify: `cmake/windows-parity.json`.
- Modify: `cmake/windows.md`.
- Modify: `cmake/modules/NetscapeInstall.cmake`.
- Create: `cmake/tests/check_winfe_m08.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/m08.inc`.

**Interfaces:** Consumes M01-M07 engine/browser cases plus core gate artifacts. Produces qualified Mocha-enabled packages and Mocha-disabled core matrix.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_m08` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_m08(r):
    assert r["mocha_matrix_pass"] == "1"
    assert r["disabled_core_pass"] == "1"
    assert r["installed_script_execution"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_m08'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Run Gate-Engine mocha, original language/browser fixtures and installed execution without source-tree dependence. Verify disabled builds omit runtime initialization and have correct command states. Keep Java/NPAPI bridge rows blocked for later owners, not falsely complete.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_m08'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Qualify native Mocha enabled and disabled configurations"`. Follow the master commit-scope rules.

## Handoff and gate

Requires core C32. Required neighbor checks: browser forms, frames/history,
printing, image lifetimes, TLS, core command states and startup preferences.
M08 must pass before the NPAPI implementation phase. Test individual bridge
absence explicitly; full-engine parity is not claimed at this gate.
