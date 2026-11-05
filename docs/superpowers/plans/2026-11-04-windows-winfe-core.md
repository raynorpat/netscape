# Windows core applications and Gold UI parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reach verified core application/Gold UI parity before optional engine work.

**Architecture:** Finish workflows through shared native controls, existing LO/IL/SHIST/BM/MSG/EDT services and original profile formats. Engine-backed routes stay explicitly blocked until their later phase.

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

Existing anchors: `main.cpp` owns browser and Mail/News frames; `chrome.cpp`,
`classic.cpp` and `mailui.cpp` own shared surfaces; `services.c`/`frontend.c` own
frontend callbacks. Keep those interfaces and move only new responsibility into
focused helpers/cache/network/encoding/find/subscription/security modules named
below. Read original signatures in `include/fe_proto.h`, `bkmks.h`, `msgcom.h` and
EDT headers rather than copying later Communicator interfaces.

C01 creates the capability ledger/reference metadata; C02 owns the small test-only
dispatch/runner contract in the master. C03-C31 create one native include and one
stdlib driver per item; C32 uses the matrix/package audit drivers, registered in `cmake/targets/windows/parity-tests.cmake`.
For GUI items the native function is `static int WFE_RunCxxParity(const char*,
const char*,const char*)` (report, snapshot, fixture), as defined in the master.
Only this plan owns core completion status; the old backlog links to the ledger.

## Commit sequence

### C01: Inventory the compiled Gold/XFE capability baseline

**Depends on:** clean integration base described in master.

**Files:**

- Create: `cmake/windows-parity.json`.
- Create: `cmake/tests/check_windows_parity_ledger.py`.
- Create: `cmake/tests/fixtures/windows302-dialogs.json`.
- Create: `cmake/targets/windows/parity-tests.cmake`.
- Modify: `cmake/targets/windows/frontend.cmake`.

**Interfaces:** Consumes original MENU fixtures, cmd/xfe/Makefile, cmake/targets/xfe-sources.cmake and conditional backend manifests. Produces schema version 1 ledger rows with id, workflow, apps, routes, gold_reference, xfe_condition, evidence, winfe_files, state, depends, owner, tests, exception.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c01` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c01(r):
    assert int(r["unmapped_routes"]) == 0
    assert int(r["unclassified_conditionals"]) == 0
    assert r["reference_hashes_recorded"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c01'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Audit each original command/dialog and configured XFE callback. Record release hashes and source selections; distinguish working, conditional, unbuilt and stubbed features. Capture original reference metrics without deriving expected values from WinFE. Classify filters/IMAP/new address APIs and Unix-only operations here, not at release time. Produce a disposition/addendum for each confirmed capability not assigned below.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c01'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Record compiled Windows Gold and XFE parity inventory"`. Follow the master commit-scope rules.

This is a pure Python inventory test, not a native smoke. An audit does not mark source-only capabilities complete. Any required addendum gets exact files/APIs/tests before implementation; core gate rejects uncovered rows.

### C02: Add a bounded native parity fixture runner

**Depends on:** C01.

**Files:**

- Create: `cmd/winfe/parity-smoke.inc`.
- Modify: `cmd/winfe/main.cpp`.
- Create: `cmake/tests/winfe_parity_fixture.py`.
- Create: `cmake/tests/test_winfe_parity_fixture.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Modify: `cmake/targets/windows/frontend.cmake`.

**Interfaces:** Consumes existing native smoke/capture patterns and C01 IDs. Produces BUILD_TESTING-only --winfe-parity-smoke CASE REPORT SNAPSHOT [FIXTURE], WFE_RunParitySmoke and run_case contract in master.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c02` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c02(r):
    assert r["timeout_child_reaped"] == "1"
    assert r["stale_report_rejected"] == "1"
    assert r["unknown_case_rejected"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c02'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement only dispatch, owned temporary-profile launch, fresh-report validation and bounded termination of its own child. Unknown cases exit nonzero. Keep native GUI cases serial; retain stdout/stderr/traces on timeout. Add the CTest manifest include without restructuring production windows.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c02'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add bounded native WinFE parity fixtures"`. Follow the master commit-scope rules.

Driver tests use fake child programs and an actual native no-op case; no GUI behavior is replaced by a fake implementation.

### C03: Resolve Welcome reader rendering hangs

**Depends on:** C02.

**Files:**

- Modify: `cmd/winfe/mail-welcome-smoke.inc`.
- Modify: `cmd/winfe/images.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmake/tests/check_windows_mail_welcome.py`.
- Create: `cmd/winfe/parity-tests/c03.inc`.
- Create: `cmake/tests/check_winfe_c03.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original Windows Welcome payload and IL/GDI capture path. Produces unchanged Welcome bytes and bounded real paint/teardown.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c03` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c03(r):
    assert r["paint_complete"] == "1"
    assert r["welcome_payload_unchanged"] == "1"
    assert r["closed_without_hang"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c03'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Reproduce AlphaBlend stalls using owned profiles, capture main-thread stacks and isolate driver/host, fixture and product causes. Correct only a demonstrated cause. Preserve alpha pixels and real paint/capture coverage; if host-only, commit a reproducible diagnostic/preflight and verified environment evidence instead of a speculative renderer rewrite.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c03'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Resolve native Welcome paint and teardown verification"`. Follow the master commit-scope rules.

### C04: Resolve News native tooltip/hit-test hangs

**Depends on:** C02.

**Files:**

- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/mailui.cpp`.
- Modify: `cmake/tests/check_windows_news_ui.py`.
- Create: `cmd/winfe/parity-tests/c04.inc`.
- Create: `cmake/tests/check_winfe_c04.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes news_ui_click, native list notifications and existing subscription checks. Produces bounded real list mouse/keyboard behavior and teardown.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c04` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c04(r):
    assert r["mouse_subscription"] == "1"
    assert r["keyboard_subscription"] == "1"
    assert r["tooltip_lifetime_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c04'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Reproduce WindowFromPoint/tooltip stalls with native event traces. Fix the proven lifetime/event or fixture scheduling defect; prove hit testing, subscription clicks and tooltip behavior rather than disabling tooltips. Keep a host-only finding explicit if no product defect is demonstrated.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c04'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Resolve News list interaction and tooltip verification"`. Follow the master commit-scope rules.

### C05: Verify background startup and missing-file recovery

**Depends on:** C02.

**Files:**

- Modify: `cmd/winfe/preferences-background-smoke.inc`.
- Modify: `cmd/winfe/preferences-settings.cpp`.
- Modify: `cmd/winfe/images.cpp`.
- Modify: `cmake/tests/check_windows_background.py`.
- Create: `cmd/winfe/parity-tests/c05.inc`.
- Create: `cmake/tests/check_winfe_c05.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes persisted background preference and original image override rules. Produces repeatable new-process fallback without rewriting valid preferences.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c05` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c05(r):
    assert r["reopened_pixels_match"] == "1"
    assert r["missing_falls_back"] == "1"
    assert r["ini_preserved"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c05'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Trace background/reopen/missing-file runs independently, including blocked paint or leftover child handles. Fix demonstrated faults and retain original override semantics. Require actual pixels and unchanged INI bytes when a missing image falls back.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c05'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Make background preference recovery reproducible"`. Follow the master commit-scope rules.

### C06: Finish shared Gold chrome and dialog geometry

**Depends on:** C01-C05.

**Files:**

- Modify: `cmd/winfe/classic.cpp`.
- Modify: `cmd/winfe/chrome.cpp`.
- Modify: `cmd/winfe/mailui.cpp`.
- Modify: `cmd/winfe/composer.cpp`.
- Modify: `cmd/winfe/editor-ui.cpp`.
- Modify: `cmd/winfe/netscape.rc`.
- Modify: `cmake/tests/fixtures/windows302-dialogs.json`.
- Create: `cmd/winfe/parity-tests/c06.inc`.
- Create: `cmake/tests/check_winfe_c06.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes C01 original metrics and ClassicUI system-palette services. Produces consistent native sizes, fonts, artwork, borders and focus across window families.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c06` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c06(r):
    assert r["browser_button_96"] == "49x41"
    assert r["companion_button_96"] == "41x41"
    assert r["system_palette_and_focus"] == "1"
    assert r["narrow_and_144dpi_usable"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c06'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Fix ledger differences against independent 3.02 references. Retain known 96-DPI browser 49x41 buttons, companion 41x41 buttons, original group gaps and system-owned borders. Verify supported 96/144-DPI scaling and narrow layouts; document original-only 96-DPI comparison.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c06'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Finish shared Gold chrome and native dialog geometry"`. Follow the master commit-scope rules.

### C07: Complete command, shortcut and context-menu routing

**Depends on:** C06.

**Files:**

- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/composer.cpp`.
- Modify: `cmd/winfe/bookmarks.cpp`.
- Modify: `cmd/winfe/addressbook.cpp`.
- Modify: `cmd/winfe/editor-ui.cpp`.
- Modify: `cmake/tests/fixtures/windows302-menus.json`.
- Modify: `cmake/tools/generate_windows_menus.py`.
- Create: `cmd/winfe/parity-tests/c07.inc`.
- Create: `cmake/tests/check_winfe_c07.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original command IDs; BrowserFrame::OnCommand and OnUpdateMenu; WFE_WindowCommand. Produces truthful execution/enabled/check states for every classified route.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c07` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c07(r):
    assert int(r["unmapped_core_commands"]) == 0
    assert r["all_entrypoints_same_action"] == "1"
    assert r["engine_off_states_truthful"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c07'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Drive original menu, toolbar, accelerator and context routes through existing BM/MSG/EDT services. Include focused edit/frame targets, modal states, busy/idle transitions and engine-off controls. Correct implementation gaps, not reference data.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c07'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete native menu shortcut and context command routing"`. Follow the master commit-scope rules.

### C08: Complete startup, accounts and atomic preference changes

**Depends on:** C07.

**Files:**

- Modify: `cmd/winfe/preferences.h`.
- Modify: `cmd/winfe/preferences-settings.cpp`.
- Modify: `cmd/winfe/preferences-ui.cpp`.
- Modify: `cmd/winfe/mailsettings.c`.
- Modify: `cmd/winfe/main.cpp`.
- Create: `cmd/winfe/parity-tests/c08.inc`.
- Create: `cmake/tests/check_winfe_c08.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes GeneralPreferences; WFE_SaveGeneralPreferences; WFE_MailPreference and WFE_SetMailPreference. Produces existing preference APIs with tested startup/account application and rollback.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c08` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c08(r):
    assert r["startup_routes_match"] == "1"
    assert r["cancel_and_write_failure_preserve"] == "1"
    assert r["two_windows_consistent"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c08'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Reconcile observed startup/window combinations, account dialogs, defaults and restart behavior. Stage modal changes, preserve preferences after Cancel/write failure, and apply supported changes consistently to all windows. Keep TLS extension separate from original tabs.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c08'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete startup accounts and atomic preference workflows"`. Follow the master commit-scope rules.

### C09: Implement Helpers associations and MIME launch choices

**Depends on:** C08.

**Files:**

- Create: `cmd/winfe/helpers.h`.
- Create: `cmd/winfe/helpers.cpp`.
- Modify: `cmd/winfe/applications.cpp`.
- Modify: `cmd/winfe/preferences-ui.cpp`.
- Modify: `cmd/winfe/services.c`.
- Modify: `cmd/winfe/downloads.cpp`.
- Create: `cmd/winfe/parity-tests/c09.inc`.
- Create: `cmake/tests/check_winfe_c09.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes WFE_LaunchApplication(const std::string&,const std::vector<std::string>&,std::string*); content converters. Produces int WFE_HandleHelper(MWContext*, URL_Struct*), returning 1 if handled, 0 declined, -1 error.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c09` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c09(r):
    assert r["helper_argv_exact"] == "1"
    assert r["cancel_did_not_launch"] == "1"
    assert r["saved_binary_sha256_matches"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c09'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Persist explicit MIME/extension/action associations and wire Helpers editing, save/open choices and external protocols. Use full executable paths and argument vectors; preserve quoting for spaces/percent/metacharacters. Launch only the fixture probe in tests; do not alter OS-wide associations.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c09'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add native Helpers associations and MIME actions"`. Follow the master commit-scope rules.

### C10: Implement proxy and connection preference workflows

**Depends on:** C08.

**Files:**

- Create: `cmd/winfe/network-preferences.h`.
- Create: `cmd/winfe/network-preferences.cpp`.
- Modify: `cmd/winfe/preferences-settings.cpp`.
- Modify: `cmd/winfe/platform.c`.
- Modify: `cmd/winfe/main.cpp`.
- Create: `cmd/winfe/parity-tests/c10.inc`.
- Create: `cmake/tests/check_winfe_c10.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes NET_SetProxyServer and NET_SelectProxyStyle from include/net.h; existing CONNECT/TLS adapter. Produces int WFE_ApplyNetworkPreferences(void), 1 applied or 0 with original preferences retained.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c10` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c10(r):
    assert r["direct_manual_bypass"] == "1"
    assert r["connect_identity_retained"] == "1"
    assert r["active_load_policy_consistent"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c10'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement original direct/manual/no-proxy routes, connection limits/timeouts and dialog validation using original netlib. Test HTTP/HTTPS CONNECT, destination changes and proxy failure. Automatic proxy scripts stay an explicit Mocha dependency owned by M07.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c10'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add native proxy and connection preferences"`. Follow the master commit-scope rules.

### C11: Complete cookie prompts, policy and persistence

**Depends on:** C08.

**Files:**

- Create: `cmd/winfe/cookie-ui.h`.
- Create: `cmd/winfe/cookie-ui.cpp`.
- Modify: `cmd/winfe/services.c`.
- Modify: `cmd/winfe/preferences-settings.cpp`.
- Modify: `cmd/winfe/frontend.c`.
- Create: `cmd/winfe/parity-tests/c11.inc`.
- Create: `cmake/tests/check_winfe_c11.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes NET_SetCookiePrefs and original cookie file APIs in include/net.h. Produces native cookie callback implementations with existing signatures; no new public core API.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c11` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c11(r):
    assert r["policy_and_prompt_obeyed"] == "1"
    assert r["cross_origin_not_leaked"] == "1"
    assert r["truncated_store_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c11'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement confirmed original accept/warn/reject/foreign-cookie settings and their prompt states. Exercise two origins, malformed/truncated saved data, expiration and Cancel. Never log real cookie contents; fixtures are synthetic.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c11'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete cookie policy prompts and storage"`. Follow the master commit-scope rules.

### C12: Expose memory cache policy and clear operations

**Depends on:** C08.

**Files:**

- Create: `cmd/winfe/cache-ui.h`.
- Create: `cmd/winfe/cache-ui.cpp`.
- Modify: `cmd/winfe/preferences-settings.cpp`.
- Modify: `cmd/winfe/frontend.c`.
- Create: `cmd/winfe/parity-tests/c12.inc`.
- Create: `cmake/tests/check_winfe_c12.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes NET_SetMemoryCacheSize and cache query APIs; existing reload/history paths. Produces void WFE_ShowCachePreferences(MWContext*), memory portion active first.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c12` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c12(r):
    assert r["memory_limits_applied"] == "1"
    assert r["reload_request_counts_match"] == "1"
    assert r["clear_preserves_live_documents"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c12'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Wire actual memory cache limits, query and clearing into the observed UI. Prove normal reload/force reload and two independent windows; preserve active documents while clearing cached entries. Disk controls remain accurately disabled until C13.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c12'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Expose native memory cache policy and clearing"`. Follow the master commit-scope rules.

### C13: Enable checked disk cache and recovery

**Depends on:** C10-C12.

**Files:**

- Modify: `cmd/winfe/cache-ui.cpp`.
- Modify: `cmd/winfe/platform.c`.
- Modify: `cmd/winfe/frontend.c`.
- Modify: `lib/libnet/mkcache.c`.
- Create: `cmd/winfe/parity-tests/c13.inc`.
- Create: `cmake/tests/check_winfe_c13.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes C12 UI and original FAT/cache APIs in include/net.h and mkcache.h. Produces int WFE_DiskCacheInit(void); void WFE_DiskCacheShutdown(void).

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c13` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c13(r):
    assert r["restart_cache_hit"] == "1"
    assert r["corrupt_entry_refetched"] == "1"
    assert r["write_failure_preserved_profile"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c13'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Enable only after testing exact stored response bytes, FAT write/close/rename failures, corrupt/truncated cache records and restart recovery in owned directories. Clear only cache-owned paths and respect HTTPS non-caching policy. Do not recover by deleting unrelated profile data.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c13'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Enable recoverable native disk cache workflows"`. Follow the master commit-scope rules.

### C14: Close navigation and frame interruption gaps

**Depends on:** C07,C10,C12.

**Files:**

- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/history.cpp`.
- Modify: `cmd/winfe/frame-divider.cpp`.
- Modify: `cmd/winfe/browser-links.cpp`.
- Create: `cmd/winfe/parity-tests/c14.inc`.
- Create: `cmake/tests/check_winfe_c14.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes SHIST/LO navigation and existing target routing. Produces existing navigation/history interfaces with consistent saved state.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c14` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c14(r):
    assert r["history_state_restored"] == "1"
    assert r["cancelled_post_not_replayed"] == "1"
    assert r["closed_frame_callbacks_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c14'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Cover redirect, fragment, targeted-frame, POST reload confirmation, interrupted load, back/forward and window closure. Restore focus, form values, scroll and divider state without reviving dead child contexts.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c14'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete interrupted navigation and frame history behavior"`. Follow the master commit-scope rules.

### C15: Complete multipart uploads and form keyboard behavior

**Depends on:** C14.

**Files:**

- Modify: `cmd/winfe/forms.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `lib/libnet/mkutils.c`.
- Create: `cmd/winfe/parity-tests/c15.inc`.
- Create: `cmake/tests/check_winfe_c15.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original form collection and upload producer; native file control. Produces existing WFE_Form callbacks and original multipart protocol output.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c15` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c15(r):
    assert r["multipart_bytes_exact"] == "1"
    assert r["offscreen_focus_and_reset"] == "1"
    assert r["read_error_no_partial_success"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c15'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Add exact multipart filename/content byte tests, offscreen Tab/focus/Enter, reset and navigation restoration. Test file growth/truncation and real read errors; preserve file ownership and report errors instead of accepting partial reads as EOF. Keep page-supplied file paths rejected.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c15'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete native multipart forms and upload error handling"`. Follow the master commit-scope rules.

### C16: Finish animated and progressive image lifecycle

**Depends on:** C03,C14.

**Files:**

- Modify: `cmd/winfe/images.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `lib/libimg/ilclient.c`.
- Create: `cmd/winfe/parity-tests/c16.inc`.
- Create: `cmake/tests/check_winfe_c16.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes IL frame/progress callbacks and existing GDI surfaces. Produces existing image frontend callbacks with bounded timer/context ownership.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c16` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c16(r):
    assert r["frames_and_progress_match"] == "1"
    assert r["shared_clients_safe"] == "1"
    assert r["no_timer_after_close"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c16'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Match original animated/progressive updates, transparency, image suppression and reload. Use controlled fragmented GIF/JPEG/XBM failures and multiple shared clients; stop timers after navigation/close and preserve alpha pixels.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c16'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Finish progressive and animated image lifetime"`. Follow the master commit-scope rules.

### C17: Complete charset, fonts and clipboard conversion

**Depends on:** C08,C14.

**Files:**

- Create: `cmd/winfe/text-encoding.h`.
- Create: `cmd/winfe/text-encoding.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/preferences-ui.cpp`.
- Modify: `cmd/winfe/services.c`.
- Create: `cmd/winfe/parity-tests/c17.inc`.
- Create: `cmake/tests/check_winfe_c17.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original libi18n tables and font metrics; configured XFE encoding inventory. Produces bool WFE_ConvertClipboardText(MWContext*,const char*,std::string*,std::string*).

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c17` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c17(r):
    assert r["document_and_mime_decode_match"] == "1"
    assert r["copy_roundtrip_supported_charsets"] == "1"
    assert r["conversion_failure_preserves_text"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c17'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Enable only baseline-supported charset choices; verify MIME headers/body, font sizing, document charset changes and Copy using byte/codepoint fixtures. Failed or unrepresentable conversions must give a visible result and preserve existing data; do not claim full Unicode browsing from ANSI GDI.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c17'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete supported charset font and clipboard workflows"`. Follow the master commit-scope rules.

### C18: Finish download disposition and legacy protocol actions

**Depends on:** C09,C10,C14.

**Files:**

- Modify: `cmd/winfe/downloads.cpp`.
- Modify: `cmd/winfe/helpers.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/platform.c`.
- Create: `cmd/winfe/parity-tests/c18.inc`.
- Create: `cmake/tests/check_winfe_c18.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes C09 helper dispatch and original netlib converters/protocol selection. Produces existing download API with final-response disposition and checked output.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c18` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c18(r):
    assert r["final_disposition_honored"] == "1"
    assert r["binary_and_transformed_hashes_match"] == "1"
    assert r["failed_output_not_published"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c18'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Cover renderable attachments, redirected filenames, transformed/chunked responses, concurrent transfers, browser closure, truncation and final close errors. Exercise configured FTP/Gopher/other working baseline routes through local servers; classify any additional protocol commit before core gate.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c18'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete download disposition and legacy protocol workflows"`. Follow the master commit-scope rules.

### C19: Complete printing across browser, readers and composition

**Depends on:** C17.

**Files:**

- Modify: `cmd/winfe/printing.cpp`.
- Modify: `cmd/winfe/composer.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/editor-platform.cpp`.
- Create: `cmd/winfe/parity-tests/c19.inc`.
- Create: `cmake/tests/check_winfe_c19.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes WFE_PrintDocumentSize, WFE_PrintDocumentPage and WFE_PrintCommand signatures in printing.h. Produces existing print adapter plus composition document rendering.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c19` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c19(r):
    assert r["pagination_and_header_text"] == "1"
    assert r["compose_print_preserves_draft"] == "1"
    assert r["cancel_preserves_live_view"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c19'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Match original print/page setup/preview options and selection/frame targets. Add composition printing without losing headers/body/attachments; cancel without mutating live documents. Verify EMF/text/page boundaries automatically and separately record a physical printer-driver run or an explicit verification blocker.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c19'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete document reader and composer printing"`. Follow the master commit-scope rules.

### C20: Implement bookmark and address import/export

**Depends on:** C07,C17.

**Files:**

- Modify: `cmd/winfe/bookmarks.cpp`.
- Modify: `cmd/winfe/addressbook.cpp`.
- Modify: `cmd/winfe/services.c`.
- Modify: `include/bkmks.h`.
- Modify: `lib/libmisc/bkmks.c`.
- Create: `cmd/winfe/parity-tests/c20.inc`.
- Create: `cmake/tests/check_winfe_c20.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original BM import/HTML serialization APIs; existing read-failed guards. Produces existing BM command routes with native import/export dialogs.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c20` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c20(r):
    assert r["roundtrip_contacts_aliases_bookmarks"] == "1"
    assert r["cancel_and_bad_import_preserve"] == "1"
    assert r["failed_export_preserves_target"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c20'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Use original supported formats, stage imports and preserve nickname/list aliases and bookmarks. Test escaped/long names, duplicate contacts, malformed/truncated input, Cancel and failed replacement. Export never overwrites without the native user-selected target.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c20'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add checked bookmark and address import export"`. Follow the master commit-scope rules.

### C21: Add original BM find and change-check workflows

**Depends on:** C20.

**Files:**

- Create: `cmd/winfe/bm-find.h`.
- Create: `cmd/winfe/bm-find.cpp`.
- Modify: `cmd/winfe/services.c`.
- Modify: `cmd/winfe/bookmarks.cpp`.
- Modify: `cmd/winfe/addressbook.cpp`.
- Create: `cmd/winfe/parity-tests/c21.inc`.
- Create: `cmake/tests/check_winfe_c21.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes BM_FindInfo, BM_DoFindBookmark and BM_StartWhatsChanged/BM_CancelWhatsChanged. Produces void* WFE_ShowBMFind(MWContext*,BM_FindInfo*) with original BMFE ownership contract.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c21` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c21(r):
    assert r["find_again_and_no_match"] == "1"
    assert r["change_check_progress_and_cancel"] == "1"
    assert r["parent_close_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c21'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Replace BMFE_OpenFindWindow placeholder with modeless native find/again behavior. Wire confirmed change-check progress and cancellation through original BM callbacks, using local changed/unchanged/unreachable pages. Close parent safely and preserve selection on no match.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c21'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add native BM find and change checking"`. Follow the master commit-scope rules.

### C22: Complete personal-data multi-selection and clipboard

**Depends on:** C20,C21.

**Files:**

- Modify: `cmd/winfe/bookmarks.cpp`.
- Modify: `cmd/winfe/addressbook.cpp`.
- Modify: `cmd/winfe/services.c`.
- Create: `cmd/winfe/parity-tests/c22.inc`.
- Create: `cmake/tests/check_winfe_c22.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes BM_ConvertSelectionsToBlock and BM_InsertBlockAt signatures in bkmks.h. Produces native selection model synchronized with original BM selection and undo.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c22` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c22(r):
    assert r["visible_selection_matches_bm"] == "1"
    assert r["clipboard_roundtrip_and_undo"] == "1"
    assert r["bad_paste_preserves_entries"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c22'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement visible multi-selection and Cut/Copy/Paste across compatible personal-data windows. Keep address aliases/nicknames correct, reject incompatible/corrupt clipboard blocks, and make one user edit one Undo action. Never erase original entries if insertion fails.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c22'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete personal data multi selection and clipboard"`. Follow the master commit-scope rules.

### C23: Add personal-data drag/drop and organization

**Depends on:** C22.

**Files:**

- Modify: `cmd/winfe/bookmarks.cpp`.
- Modify: `cmd/winfe/addressbook.cpp`.
- Modify: `cmd/winfe/classic.cpp`.
- Create: `cmd/winfe/parity-tests/c23.inc`.
- Create: `cmake/tests/check_winfe_c23.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes C22 selection and BM move/alias model. Produces existing tree commands plus native drag target handling.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c23` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c23(r):
    assert r["order_persists_and_undoes"] == "1"
    assert r["cycle_drop_rejected"] == "1"
    assert r["escape_capture_loss_no_mutation"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c23'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement valid reorder/folder moves and modifier-copy actions where baseline supports them. Reject self/descendant drops and incompatible stores; cancel on Escape/capture loss; restore order/selection in one Undo and after restart. Do not enable BM sort commands marked NYI without a working baseline contract.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c23'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add native bookmark and address organization"`. Follow the master commit-scope rules.

### C24: Verify and finish native POP retrieval

**Depends on:** C08,C17.

**Files:**

- Modify: `cmd/winfe/mailsettings.c`.
- Modify: `cmd/winfe/frontend.c`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `lib/libnet/mkpop3.c`.
- Create: `cmd/winfe/parity-tests/c24.inc`.
- Create: `cmake/tests/check_winfe_c24.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original POP engine and existing MSG folder update callbacks. Produces Get Mail workflow with real mailbox delivery and retry.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c24` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c24(r):
    assert r["pop_mailbox_bytes_exact"] == "1"
    assert r["interrupted_fetch_retry_no_duplicate"] == "1"
    assert r["failed_write_keeps_saved_mail"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c24'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Use loopback POP fixtures for authentication, multiline dot-stuffing, duplicate prevention, interrupted fetch, write failure, cancellation and retry. Update folder counts and reader safely without logging credentials or contacting a real account.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c24'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete native POP retrieval and failure recovery"`. Follow the master commit-scope rules.

### C25: Complete mailbox filing and folder maintenance

**Depends on:** C24.

**Files:**

- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/mailui.cpp`.
- Modify: `cmd/winfe/mailsettings.c`.
- Modify: `lib/libmsg/folders.c`.
- Create: `cmd/winfe/parity-tests/c25.inc`.
- Create: `cmake/tests/check_winfe_c25.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes MSG_Command/MSG_CommandStatus and original folder services. Produces existing create/rename/delete/compact/move/copy routes with checked persistence.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c25` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c25(r):
    assert r["filing_counts_and_restart"] == "1"
    assert r["compact_preserves_message_hashes"] == "1"
    assert r["failed_move_keeps_source"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c25'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Cover selection ranges, unread totals, cross-folder move/copy, compact, undo where supported, restart and filesystem failures. Use disposable mailboxes and preserve originals on failed writes. Match original delete/confirmation behavior and live menu state.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c25'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete mailbox filing and folder maintenance"`. Follow the master commit-scope rules.

### C26: Finish reader search, attachments and message views

**Depends on:** C09,C17,C25.

**Files:**

- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/mailui.cpp`.
- Modify: `cmd/winfe/services.c`.
- Modify: `cmd/winfe/helpers.cpp`.
- Create: `cmd/winfe/parity-tests/c26.inc`.
- Create: `cmake/tests/check_winfe_c26.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes MSG search/header/attachment commands and MIME decoder. Produces native search/results and attachment open/save routes using existing core APIs.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c26` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c26(r):
    assert r["search_sort_selection_match"] == "1"
    assert r["decoded_attachment_hash_match"] == "1"
    assert r["deleted_result_and_close_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c26'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Complete original search/navigation/thread/header/attachment views, sorting and selection retention. Save decoded attachments exactly and route external open through C09. Test encoded/non-ASCII names, malformed MIME, deleted result rows and reader closure during load.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c26'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete message search views and attachment actions"`. Follow the master commit-scope rules.

### C27: Complete News discovery and subscription management

**Depends on:** C04,C26.

**Files:**

- Create: `cmd/winfe/news-subscriptions.h`.
- Create: `cmd/winfe/news-subscriptions.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/mailui.cpp`.
- Create: `cmd/winfe/parity-tests/c27.inc`.
- Create: `cmake/tests/check_winfe_c27.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes MSG_ToggleSubscribed, original NNTP discovery and group command APIs. Produces void WFE_ShowNewsSubscriptions(MWContext*).

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c27` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c27(r):
    assert r["subscriptions_persist_per_host"] == "1"
    assert r["group_article_actions_match"] == "1"
    assert r["interrupted_discovery_retry_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c27'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement original host/group subscribe/find/refresh management and persist subscription/unread state. Test interrupted group listing, unknown counts, multiple hosts, group disappearance, article retrieval and post/reply/mark-thread/group commands against local NNTP.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c27'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete News discovery subscription and article workflows"`. Follow the master commit-scope rules.

### C28: Finish composition, drafts, queue and delivery interactions

**Depends on:** C19,C22,C26,C27.

**Files:**

- Modify: `cmd/winfe/composer.cpp`.
- Modify: `cmd/winfe/address-picker.cpp`.
- Modify: `cmd/winfe/mailsettings.c`.
- Create: `cmd/winfe/parity-tests/c28.inc`.
- Create: `cmake/tests/check_winfe_c28.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original MSG/MIME composer and existing quote/send/draft paths. Produces existing composer APIs with observed original header/attachment interactions.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c28` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c28(r):
    assert r["draft_queue_delivered_mime_match"] == "1"
    assert r["stop_retry_keeps_body_attachments"] == "1"
    assert r["cancel_and_owner_close_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c28'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Complete ledger differences in recipient controls, address picker, attachments, quoting/forwarding, edit-again, queue/resend and close prompts. Match original plain-text composition rather than inventing HTML composition. Parse actual draft and delivered SMTP/NNTP MIME; stop/retry retains body and attachment ownership.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c28'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete original composition draft and queue workflows"`. Follow the master commit-scope rules.

### C29: Finish Gold editor formatting and property UI

**Depends on:** C06,C07,C17.

**Files:**

- Modify: `cmd/winfe/editor-ui.cpp`.
- Modify: `cmd/winfe/editor-preferences.cpp`.
- Modify: `cmd/winfe/editor-create.cpp`.
- Modify: `cmd/winfe/editor_resource.h`.
- Create: `cmd/winfe/parity-tests/c29.inc`.
- Create: `cmake/tests/check_winfe_c29.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original EDT interfaces; independent MENU 101/property reference captures. Produces existing EDT command bridge and native property dialogs.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c29` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c29(r):
    assert r["format_properties_roundtrip"] == "1"
    assert r["toolbar_menu_states_match"] == "1"
    assert r["clipboard_preview_undo_match"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c29'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Match remaining formatting, table/image/link/target properties, selection, clipboard, preference and toolbar states. Exercise keyboard and mouse editing, undo/redo and browser preview. Verify semantic saved HTML and original dialog metrics independently.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c29'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete Gold editor formatting and property dialogs"`. Follow the master commit-scope rules.

### C30: Close Gold save, image and publish lifetime gaps

**Depends on:** C18,C29.

**Files:**

- Modify: `cmd/winfe/editor-platform.cpp`.
- Modify: `cmd/winfe/editor-publish.cpp`.
- Modify: `cmd/winfe/editor-stream-smoke.cpp`.
- Modify: `cmd/winfe/editor-ui.cpp`.
- Create: `cmd/winfe/parity-tests/c30.inc`.
- Create: `cmake/tests/check_winfe_c30.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes original EDT save/publish callbacks and local HTTP/FTP fixtures. Produces existing editor save/publish APIs with checked asynchronous completion.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c30` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c30(r):
    assert r["image_save_cancel_preserves_document"] == "1"
    assert r["final_write_failure_keeps_dirty"] == "1"
    assert r["publish_close_reentrancy_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c30'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Test source import, relative resources, image-save cancellation, final buffered-write failure, publishing interruption, navigation and reentrant close. Preserve dirty state and original targets on failure; prove callbacks do not use freed contexts. Do not publish to a real site.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c30'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete Gold save image and publish lifetime handling"`. Follow the master commit-scope rules.

### C31: Complete current HTTPS security information UI

**Depends on:** C07,C10; integrated reviewed BearSSL baseline.

**Files:**

- Create: `cmd/winfe/security-info.h`.
- Create: `cmd/winfe/security-info.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/tls-platform.c`.
- Create: `cmd/winfe/parity-tests/c31.inc`.
- Create: `cmake/tests/check_winfe_c31.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes request-owned adapter-neutral TLS metadata from include/net_tls_types.h. Produces void WFE_ShowConnectionSecurity(MWContext*).

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c31` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c31(r):
    assert r["https_info_matches_request"] == "1"
    assert r["http_and_failure_clear_secure_state"] == "1"
    assert r["legacy_capabilities_not_implied"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c31'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Expose current connection state and supported certificate/protocol/cipher/error information in original-style security entry points. Clear stale state across HTTP redirects/context recreation and failed loads. Do not synthesize legacy certificate/key objects or imply client-certificate/S-MIME support.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c31'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add truthful native HTTPS connection security information"`. Follow the master commit-scope rules.

### C32: Qualify core parity and ship standard/Gold candidates

**Depends on:** C01-C31 and every required C01 addendum.

**Files:**

- Create: `cmake/tests/run_winfe_matrix.py`.
- Create: `cmake/tests/check_winfe_core_gate.py`.
- Create: `cmake/tests/check_winfe_package.py`.
- Modify: `cmake/windows-parity.json`.
- Modify: `cmake/windows-reference.md`.
- Modify: `cmake/windows-roadmap.md`.
- Modify: `cmake/windows.md`.
- Modify: `cmake/modules/NetscapePackaging.cmake`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes all core rows, C01 conditional addenda, per-item evidence and independent Gold references. Produces machine-checkable core gate and separate verified standard/Gold Release candidates.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_c32` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_c32(r):
    assert int(r["unresolved_core_rows"]) == 0
    assert r["all_required_checks_pass"] == "1"
    assert r["installed_packages_verified"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_c32'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Reject unclassified core rows, absent evidence and unexplained timeouts. Run Gate-Core, matched Gold comparisons and installed/package scenarios, preserving profile data. A physical printer or unavailable XFE environment is an explicit unresolved acceptance item unless user-approved exception; never count it as passed.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_c32'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Qualify core Windows apps and Gold UI parity"`. Follow the master commit-scope rules.

C32 is the hard gate before M01. Each additional confirmed baseline feature found in C01 requires its own numbered commit/test before this gate; no placeholder item is treated as implementation.

## Handoff and gate

C01 and C32 are audit cases: use the named Python drivers, not new native includes.

Required existing neighbors: original artwork/chrome, preferences, background,
bookmarks, history, forms/images/frames, downloads, printing, mail/news, Welcome,
address book, composition, News layout, Gold editing/publishing and browser TLS.
Run affected neighbors per item; run the full four-configuration matrix at C32.
Do not start Mocha until C32 passes. Record evidence; do not mark any checkbox
because an earlier build or a linked static archive happened to pass.
