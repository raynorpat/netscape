# Windows remaining security and release qualification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close confirmed security/conditional gaps and qualify the combined frontend and engine release.

**Architecture:** Keep modern HTTPS policy, require a focused approved provider contract for new security capabilities, and reconcile the parity ledger against installed standard/Gold builds.

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

S01 creates a focused security design and executable leaf plan because the
approved HTTPS adapter explicitly excludes client certificates/S-MIME and the
provider choice cannot be guessed. S02-S05 name workflow ownership and native UI
entrypoints; their provider-facing types/files must be filled by that approved
addendum before coding. They remain scheduled required work, not claimed parity.
Native UI modules are certificate-manager, client-identity and message-security;
legacy protocol archives remain separate from the BearSSL transport.
R01 owns portable regeneration, R02 the supported matrix JSON/runner, R03 package
manifest validation and R04 final evidence. Audit/matrix/package cases are Python
or native CLI tests, not pretend GUI probes; only actual UI cases use C02 dispatch.

## Commit sequence

### S01: Resolve security capability and provider contracts

**Depends on:** J12.

**Files:**

- Create: `docs/superpowers/specs/2026-11-04-windows-security-capabilities-design.md`.
- Create: `docs/superpowers/plans/2026-11-04-windows-security-capabilities.md`.
- Modify: `cmake/windows-parity.json`.
- Create: `cmake/tests/check_winfe_s01.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes C01 confirmed security routes, C31 TLS metadata and the existing BearSSL design boundaries. Produces reviewed per-capability provider/vault ABI, crypto policy, file ownership and negative-vector tests; exact implementation addendum before S02-S05.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_s01` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_s01(r):
    assert r["unassigned_security_capabilities"] == "0"
    assert r["provider_contract_and_negative_vectors"] == "1"
    assert r["obsolete_ssl_or_validation_bypass"] == "0"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_s01'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Map confirmed certificate/trust/client-identity/S-MIME/signed-archive services to available implementations. Distinguish original UI from modern provider requirements and conditional/unbuilt XFE code. Specify exact owned types, signatures, algorithms, persistence, migration and task files in a focused design and leaf plan. Obtain required design approval for provider/cryptographic changes; do not choose a new provider silently.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_s01'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Define Windows security capability and provider contracts"`. Follow the master commit-scope rules.

This is a design/audit commit, not implementation. S02-S05 are scheduled workflow commits with native entrypoints below; they are not executable until S01 names the actual provider APIs/files and approval required by the approved spec is recorded. No security feature is marked complete by this design commit.

### S02: Build required archive/security dependencies natively

**Depends on:** S01 approved; only confirmed required dependencies.

**Files:**

- Create: `cmake/targets/windows/security.cmake`.
- Modify: `cmake/targets/jar.cmake`.
- Modify: `cmake/targets/windows/core.cmake`.
- Create: `cmake/tests/windows_archive_probe.c`.
- Create: `cmake/tests/check_winfe_s02.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes S01 approved dependency/source map and C01 signed-archive baseline. Produces only required native archives plus ZLIB::ZLIB imported dependency and a real archive/verification probe.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_s02` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_s02(r):
    assert r["archive_payload_hash_match"] == "1"
    assert r["corrupt_or_bad_signature_rejected"] == "1"
    assert r["old_ssl_transport_enabled"] == "0"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_s02'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Add Windows source selections without enabling the old SSL transport or colliding with BearSSL. Reuse CMake modules/find_package for imported libraries. Exercise actual archive bytes/CRC/truncation and required provider verification, not symbol-address smoke. Split independently reviewable library compatibility fixes into S02.n commits defined by the S01 addendum.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_s02'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Build required native archive and security dependencies"`. Follow the master commit-scope rules.

### S03: Complete certificate and trust management workflow

**Depends on:** S01 approved,S02.

**Files:**

- Create: `cmd/winfe/certificate-manager.h`.
- Create: `cmd/winfe/certificate-manager.cpp`.
- Modify: `cmd/winfe/security-info.cpp`.
- Modify: `cmd/winfe/services.c`.
- Create: `cmake/tests/check_winfe_s03.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/s03.inc`.

**Interfaces:** Consumes S01 approved certificate/trust/vault ABI and original security UI references. Produces void WFE_ShowCertificateManager(MWContext*) plus original FE routes.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_s03` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_s03(r):
    assert r["certificate_actions_match_reference"] == "1"
    assert r["bad_import_and_failed_save_preserve"] == "1"
    assert r["https_validation_not_bypassed"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_s03'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement inspect/import/export/delete/trust actions actually supported by baseline. Stage confirmation and persist changes with checked replacement; test invalid/multiple/truncated certificates, wrong format, nickname collisions and failed writes using synthetic stores. Preserve HTTPS validation and distinguish managed identities from the immutable CA-bundle override.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_s03'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete native certificate and trust management"`. Follow the master commit-scope rules.

### S04: Complete client identity selection and authentication

**Depends on:** S01 approved,S03.

**Files:**

- Create: `cmd/winfe/client-identity.h`.
- Create: `cmd/winfe/client-identity.cpp`.
- Modify: `cmd/winfe/tls-platform.c`.
- Modify: `cmd/winfe/security-info.cpp`.
- Create: `cmake/tests/check_winfe_s04.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/s04.inc`.

**Interfaces:** Consumes S01 approved key/certificate identity ABI; S03 managed identities. Produces void WFE_ShowClientIdentity(MWContext*); provider selection/result callbacks and ownership are pinned by S01.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_s04` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_s04(r):
    assert r["real_client_authentication_pass"] == "1"
    assert r["cancel_wrong_password_no_identity_leak"] == "1"
    assert r["new_origin_does_not_inherit_identity"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_s04'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement original identity/password/selection UI and real authenticated connection behavior through the approved adapter extension. Use controlled mutual-authentication servers, cancellation, wrong-password and missing-key cases. Keep credentials out of logs and clear any selected identity on origin/context changes according to policy.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_s04'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete native client identity and authentication workflows"`. Follow the master commit-scope rules.

### S05: Complete confirmed signed/encrypted messaging workflows

**Depends on:** S01 approved,S03,S04 as required by provider.

**Files:**

- Create: `cmd/winfe/message-security.h`.
- Create: `cmd/winfe/message-security.cpp`.
- Modify: `cmd/winfe/composer.cpp`.
- Modify: `cmd/winfe/main.cpp`.
- Modify: `cmd/winfe/mailui.cpp`.
- Create: `cmake/tests/check_winfe_s05.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.
- Create: `cmd/winfe/parity-tests/s05.inc`.

**Interfaces:** Consumes S01 approved signing/encryption/MIME ABI and S03 identities; original MSG security command contracts. Produces void WFE_ShowMessageSecurity(MWContext*) and existing compose/reader command routes.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_s05` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_s05(r):
    assert r["signed_encrypted_mime_roundtrip"] == "1"
    assert r["bad_signature_wrong_recipient_fail"] == "1"
    assert r["cancel_draft_and_selection_state_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_s05'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Implement only confirmed baseline signing/encryption and reader verification through the approved provider. Verify real MIME signatures/encrypted payloads using synthetic keys, wrong recipients, invalid signatures, unavailable keys, cancellation and draft/queue/send transitions. Local delivery only; never display a verified indicator after failed decoding or a stale message selection.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_s05'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Complete confirmed native message signing and encryption"`. Follow the master commit-scope rules.

If C01 proves this is not a working selected baseline capability, record the evidence and required scope exception instead of fabricating a UI/backend. Full gate still rejects an unapproved omission.

### S06: Close remaining conditional capability rows

**Depends on:** S01-S05 and all required addenda.

**Files:**

- Modify: `cmake/windows-parity.json`.
- Create: `docs/superpowers/reports/windows-conditional-capabilities.md`.
- Modify: `cmake/tests/check_windows_parity_ledger.py`.
- Create: `cmake/tests/check_winfe_s06.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes C01 conditional addenda and all core/engine/security ownership rows. Produces no unassigned protocol/platform/security/library capability and no silent exclusions.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_s06` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_s06(r):
    assert r["unassigned_or_unclassified_rows"] == "0"
    assert r["unapproved_exceptions"] == "0"
    assert r["required_addenda_complete"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_s06'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Verify all original/XFE conditional behaviors, Unix-equivalent decisions, Java library/peer addenda, filters/IMAP dispositions and security outcomes. Implement every confirmed required row via its numbered addendum commits before marking it complete; produce user-reviewed exceptions only where explicitly justified.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_s06'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Reconcile all conditional Windows parity capabilities"`. Follow the master commit-scope rules.

This is an audit/closure test, not a placeholder feature commit. It cannot close a missing capability merely because its implementation is difficult.

### R01: Regenerate native parser and localized resources reproducibly

**Depends on:** S06; generator capability inventory from C01.

**Files:**

- Modify: `cmake/modules/NetscapeGenerated.cmake`.
- Modify: `cmake/modules/NetscapeResources.cmake`.
- Modify: `cmake/modules/RunGenerator.cmake`.
- Modify: `cmake/targets/windows/core.cmake`.
- Create: `cmake/tests/check_windows_regeneration.py`.
- Create: `cmake/tests/check_winfe_r01.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes canonical parser/security/localization inputs and existing Python/native generation rules. Produces portable build-tree generator targets with explicit tool/input dependencies.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_r01` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_r01(r):
    assert r["canonical_outputs_match"] == "1"
    assert r["incremental_dependencies_correct"] == "1"
    assert r["source_tree_unmodified"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_r01'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Build host tools natively and generate tables/resources into the build tree without modifying checkout sources. Compare semantic/byte output to canonical checked-in artifacts, accounting only for documented nondeterministic metadata. Add rebuild-on-input-change and no-rebuild-when-unchanged checks. Keep native RC menus/artwork provenance independent.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_r01'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Add reproducible native resource and table regeneration"`. Follow the master commit-scope rules.

### R02: Qualify supported core and combined-engine configurations

**Depends on:** R01,M08,N08,J12,S06.

**Files:**

- Modify: `cmake/tests/run_winfe_matrix.py`.
- Create: `cmake/tests/data/windows-supported-configurations.json`.
- Modify: `cmake/windows-parity.json`.
- Modify: `CMakeLists.txt`.
- Create: `cmake/tests/check_winfe_r02.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes core/Mocha/NPAPI/Java gates and original runtime state contracts. Produces explicit supported configuration matrix and per-run JUnit/source/hash evidence.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_r02` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_r02(r):
    assert r["supported_matrix_pass"] == "1"
    assert r["unsupported_configuration_rejected"] == "1"
    assert r["combined_lifetime_origin_cases_pass"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_r02'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Run all required standard/Gold Debug/Release disabled/minimum/combined configurations, plus targeted independent-engine and bridge cases. Unsupported combinations fail configuration with an actionable message. Probe combined workloads for callbacks crossing editor/mail/frame/plugin/applet lifetimes; verify all origin/taint/sandbox denials. No zero-test regex or skipped dependency counts as a pass.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_r02'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Qualify supported Windows engine combinations"`. Follow the master commit-scope rules.

### R03: Verify installation, profile preservation and package contents

**Depends on:** R02.

**Files:**

- Modify: `cmake/modules/NetscapeInstall.cmake`.
- Modify: `cmake/modules/NetscapePackaging.cmake`.
- Modify: `cmake/tests/check_winfe_package.py`.
- Create: `cmake/tests/data/windows-package-manifest.json`.
- Modify: `cmake/windows.md`.
- Create: `cmake/tests/check_winfe_r03.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes R02 tested binaries and source/runtime/dependency/license manifests. Produces separate standard/Gold ZIP/install candidates with exact tested executable/resource hashes.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_r03` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_r03(r):
    assert r["tested_exe_resource_hashes_match"] == "1"
    assert r["off_build_clean_install_pass"] == "1"
    assert r["old_profile_and_missing_asset_safe"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_r03'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Verify runtime/class/native/plugin discovery in clean-machine or isolated clean deployment, paths containing spaces and percent characters, a renamed/absent source directory and old fixture profiles. Ship only licensed required data/DLLs, default engine choices from verified original preferences and documented MFC/runtime requirements. Test malformed/missing assets and read-only profiles without rewriting valid user data.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_r03'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Verify complete Windows installation and release packages"`. Follow the master commit-scope rules.

### R04: Review final Gold/XFE parity and publish release evidence

**Depends on:** R03 and every required addendum.

**Files:**

- Modify: `cmake/windows-parity.json`.
- Modify: `cmake/windows-reference.md`.
- Modify: `cmake/windows-roadmap.md`.
- Modify: `cmake/windows.md`.
- Create: `docs/superpowers/reports/windows-winfe-parity-verification.md`.
- Create: `cmake/tests/check_winfe_r04.py`.
- Modify: `cmake/targets/windows/parity-tests.cmake`.

**Interfaces:** Consumes all workflow evidence, approved exceptions and R03 candidates. Produces final reconciled parity report and repeatable regression entrypoint; no automatic push/publication.

- [ ] **RED test:** Add the appropriate native or audit probe and stdlib driver assertions below; register `windows_parity_r04` in the appropriate Windows CMake test manifest. For behavior gaps, the probe must reach the existing operation before the assertion fails; an unknown smoke flag is not proof of the defect.

```python
def test_r04(r):
    assert r["required_rows_incomplete"] == "0"
    assert r["unexplained_timeouts"] == "0"
    assert r["final_packages_match_verified_source"] == "1"
```

- [ ] **Run RED:** `Check-Item 'windows_parity_r04'`; retain the failing behavioral assertion/trace (baseline-working items follow the master policy).
- [ ] **Implement:** Review every ledger row against independent Gold references and working XFE evidence; compare matched-size screenshots/dialog behavior. Run final failure/cancel/startup/modal/narrow/DPI workloads and record original-reference gaps explicitly. Complete a fresh whole-change review and fix findings with scoped tests/commits. Package only binaries matching final verification; publication/push remains separately authorized.
- [ ] **Run GREEN:** `Check-Item 'windows_parity_r04'` plus affected neighbors from the handoff; require exit 0, exact assertions and reviewed captures for UI changes.
- [ ] **Commit:** Stage only the listed files plus the matching ledger/evidence row; review the staged diff; run `git commit -m "Record final Windows Gold UI and XFE feature parity qualification"`. Follow the master commit-scope rules.

## Handoff and gate

Requires the user's approved core-first, Mocha, NPAPI, Java sequence. Some
independent resource/dependency groundwork may be prepared earlier, but it cannot
replace any earlier gate. S01 may require an additional provider-design decision;
do not expand cryptographic policy silently. Full release requires S06 and R04,
not just a passing core build or a compiled collection of optional archives.
