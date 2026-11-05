# Windows WinFE Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish original Windows Gold 3.02 UI and working XFE feature parity, core applications first, then original Mocha, NPAPI and Java engines, with one commit per reviewable item.

**Architecture:** Complete user workflows through the existing native MFC/Win32 UI and original shared engines. Use a capability ledger and explicit core/engine/release gates; adapt at native boundaries and keep independent subsystem plans linked below. This plan authorizes no unrelated merge, push or product implementation before execution-method review.

**Tech Stack:** Legacy C, C++14/MFC, Win32 x86/MSVC, CMake/CTest, Python standard-library fixture drivers; LO/IL, SHIST, BM, MSG/MIME, EDT, Mocha/libmocha, original NPAPI and Sun Java/JRI/AWT.

**Spec:** [Approved Windows WinFE parity design](../specs/2026-11-04-windows-winfe-parity-design.md), approved by the user on 2026-11-04. Read both the design and the selected subsystem plan before execution.

## Global Constraints

- Retain native MFC/Win32 controls, GDI rendering and system colors.
- Use English Windows **3.02 Gold** as the UI baseline and the actual configured XFE build as the feature baseline.
- Target the existing Win32/x86 ABI first; native x64 is a separate follow-up.
- Keep the original browser/language/runtime engines; do not embed a replacement browser or substitute a modern JVM/JavaScript engine.
- Core application and Gold UI parity precede Mocha, NPAPI and Java, in that order.
- Keep shared engine changes narrow, preserve legacy source encodings and style, and verify affected XFE/Linux configurations where available.
- Protect existing profiles, address/bookmark HTML, mailboxes, drafts and publishing destinations; preserve recoverable state on failures and cancellation.
- Keep the approved modern BearSSL HTTPS policy; no SSL 2/3, disabled certificate validation or success-shaped security stubs.
- Every visible command must work, be correctly disabled for current state, or remain an explicit engine-dependent ledger item.
- Run GUI fixtures serially. Synthetic mail/news/publishing stays on local fixtures; use explicit temporary profiles.
- Keep standard and Gold packages distinct. Verify installed paths containing spaces and percent characters without source/build fallback.
- One reviewable item per commit, including its tests and ledger evidence. Do not restore CVS metadata or commit unrelated staged work.

## Review Focus

1. Malformed/truncated input must not replace valid user data: core C11/C13/C15/C18/C20/C24/C25 and Java/plugin parser/stream tests.
2. Reentrant close/navigation must release callbacks, timers, HWNDs and contexts safely: C14/C30, M05, N06, J07/J08 and R02.
3. Independent windows/profiles/origins must not inherit mutable state or privilege: C08/C10/C12, M06, N05/N07, J09/J10 and S04.
4. Missing or disabled engines must preserve core behavior and truthful menu/preference state: C07, M08, N08, J12 and R02.
5. Wrong ABI, missing runtime assets and unusual installed paths must fail usefully without source fallback: C09, N01/N02, J04/J12 and R03.

---

## Linked plans and commit order

| Order | Plan | Initial items | Gate / outcome |
| --- | --- | ---: | --- |
| 1 | [Core apps and Gold UI](2026-11-04-windows-winfe-core.md) | C01-C32 | **C32 core parity gate** before any optional engine implementation |
| 2 | [Mocha](2026-11-04-windows-winfe-mocha.md) | M01-M08 | Original interpreter, browser bindings and enabled/disabled qualification |
| 3 | [NPAPI](2026-11-04-windows-winfe-npapi.md) | N01-N08 | Real Win32 DLL ABI, embedding, streams and supported script bridge |
| 4 | [Java](2026-11-04-windows-winfe-java.md) | J01-J12 | Original source VM, Windows AWT, applets, sandbox and cross-engine bridges |
| 5 | [Remaining security and release](2026-11-04-windows-winfe-qualification.md) | S01-S06, R01-R04 | Approved provider-dependent capabilities, generation, combined matrix and installed release |

There are **70 initial commit items**. This is not an assertion that 70 commits
will exhaust every unknown capability. C01 must assign every confirmed working
baseline feature. Newly confirmed features or independently reviewable source/
peer compatibility work get numbered addenda (for example C25.1 or J06.1) with
exact files, interfaces, RED/GREEN tests and commits before their owner gate.
No gate can pass by omitting those addenda or relabeling a gap an extension.
Avoid empty commits for already-verified behavior: record evidence in the ledger
and retain the item ID; commit only meaningful missing coverage, fixes or evidence.

The plans are separate because core UI, interpreter, plugin ABI and VM/AWT have
different dependency and test boundaries. They remain one ordered project; the
split does not authorize engines before the user's core-first gate.

## Current checkout and execution preconditions

Authoring began at frontend commit `9f549f1` during a staged BearSSL merge.
The integration owner completed that merge as `505156b147778d2cfd61ac8b70b2833db1a83fce`
(`Merge BearSSL HTTPS support`) while these plans were being written. Use that
integrated source or a reviewed descendant as the implementation base, together
with the commit containing this approved design and these six plans. This
planning task did not modify the integration, push it, or validate its resulting
product builds/tests. Refer to the TLS report's exact tested revisions rather
than inferring a merged-source pass.

- [ ] Before implementation, inspect `git status`, branch, merge state and applicable AGENTS.md files. Record the actual current integration/source revision and verification provenance.
- [ ] If another merge or unrelated changes are present, obtain an authorized committed base without concluding/aborting a merge, stashing/resetting work or staging somebody else's files. Independent reading/document work may continue if implementation is blocked.
- [ ] Confirm the chosen execution base contains the approved design and all six plans. Documentation commits stage only those paths; never use `git add .` or `git add -A`.
- [ ] At execution time use `superpowers:using-git-worktrees` to create an isolated checkout from the committed integrated base, normally on `codex/winfe-parity`. Preserve the original checkout; no chat creation or copying unrelated work.
- [ ] Confirm the new worktree is clean and contains the approved documents and actual intended HTTPS implementation. Record path/base, toolchain and dependency versions.
- [ ] Use new build directories below; do not reuse the current checkout's old CMake caches or call its old test results a baseline.

The last frontend tests were not fully green: 24/26 standard and 26/29 Gold.
C03-C05 own the actual Welcome/News/background hangs. Earlier stacks and the old
package reproduction are evidence, not a diagnosis. Restore reliable baseline
verification before parity completion, using a controlled VM/desktop if necessary.
Do not arbitrarily relax timeouts, remove paint coverage or disable tooltips.

## File and interface ownership

- `cmake/windows-parity.json` (C01): authoritative machine-readable capability ledger, schema version 1; each row has stable ownership, source/reference provenance, routes, status, tests and exceptions.
- `cmake/tests/fixtures/windows302-menus.json` (existing) and `windows302-dialogs.json` (C01): independent original command/geometry observations. Never generate expected geometry from WinFE.
- `cmake/targets/windows/parity-tests.cmake` (C01/C02): explicit test registration, included by the existing Windows frontend manifest; no source glob pretending to discover supported features.
- `cmd/winfe/parity-smoke.inc`, `cmd/winfe/parity-tests/<id>.inc` (C02 and item owners): BUILD_TESTING-only native fixture dispatcher and per-item cases, following existing smoke include conventions.
- `cmake/tests/winfe_parity_fixture.py` (C02): bounded subprocess/profile/report handling, not a new UI automation platform.
- Focused native feature modules live beside existing WinFE files: helpers, network preferences, cache UI, cookie UI, text encoding, BM find, News subscriptions and security information. Native implementation uses original FE/core signatures and current system colors.
- `cmake/targets/windows/mocha.cmake`, `plugins.cmake`, `java.cmake`, `security.cmake`: platform selections only; reuse shared manifests without redeclaring existing archives. Imported CMake modules belong under `cmake/modules/`.
- `mocha-platform`, `plugins`/`plugin-stream`, `java-platform`: distinct engine/native ownership boundaries. Public signatures are in their item Interfaces blocks; original public types are not replaced.
- `run_winfe_matrix.py`, `check_winfe_core_gate.py`, `check_winfe_package.py` and supported/package manifest JSON: evidence and release qualification, not product feature switches.

## Native test contract

C02 creates the test-only CLI:

```text
netscape.exe --winfe-parity-smoke CASE REPORT SNAPSHOT [FIXTURE]
```

`CASE` is the exact stable item ID, such as C09 or M03. REPORT is a new UTF-8
key=value report; SNAPSHOT is a native capture destination; FIXTURE is an optional
owned data path or loopback endpoint interpreted by that case. Arguments are
passed as subprocess argument vectors, not shell-built strings. Exit 0 means the
case completed; unknown cases exit nonzero. A report cannot override exit status.

The native dispatcher signature in `parity-smoke.inc` is:

```cpp
static int WFE_RunParitySmoke(const char *case_id, const char *report,
                             const char *snapshot, const char *fixture);
```

Each GUI item implements `static int WFE_RunC09Parity(const char *report,
const char *snapshot, const char *fixture)` with its own actual ID substituted,
registered explicitly in that dispatcher. Addendum IDs such as C25.1 stay literal
CLI/ledger strings and map explicitly to `WFE_RunC25_1Parity` and
`parity-tests/c25_1.inc`; never parse the ID as a floating-point number.
Cases drive real native controls/core
operations, create their own contexts and release every owned window/resource.
No production feature is implemented in a fixture. BUILD_TESTING=OFF omits the
new dispatcher and fixture-only code paths.

The Python helper contract is:

```python
def run_case(executable: Path, case: str, profile: Path, output: Path,
             fixture: str = "", timeout: float = 30.0) -> dict[str, str]: ...
```

It sets `NETSCAPE_PROFILE_DIR`, rejects stale/missing/duplicate-key reports,
requires exit 0 and kills/reaps only its own timed-out child, preserving failure
evidence. Drivers use Python's standard library, not an undeclared pytest
fixture. Each `test_c09(r)` assertion function in a plan is called by its driver
with the freshly parsed report `r`. Include independent disk bytes/protocol
hashes/assertions in that driver; a native self-reported boolean alone is not
proof for data-integrity/security assertions.

C01/C32/S01/S06 and matrix/resource/package audits may use pure Python reports;
M01/N01/J01-J04 use real native executable probes. They do not need a pretend
browser case. Register their named CTests explicitly and run the report assertions
against actual audit/probe output. Remove unused native includes for such tasks.
RED for a product gap must reach the real operation and expose wrong behavior;
adding an unknown CLI flag and watching it fail is not sufficient. Successful
baseline items need focused uncovered tests only, not a deliberate regression.

## Build and verification commands

These commands are for execution in the isolated worktree. The installed toolchain
last observed was Visual Studio 18 2026 with Win32 MFC; verify availability before
using it. Preserve CMake's 3.20 minimum unless a task explicitly justifies changing
it. Use separate standard/Gold and engine stage directories.

Load the following PowerShell helpers for the task commands in linked plans:

```powershell
function Use-Stage {
    param([ValidateSet('core','mocha','npapi','java')][string]$Name='core',
          [ValidateSet('standard','gold')][string]$Edition='gold',
          [ValidateSet('Debug','Release')][string]$Configuration='Debug')
    $script:WinFEStage = $Name
    $script:WinFEBuild = "out/winfe-parity-$Name-$Edition"
    $script:WinFEConfig = $Configuration
    $mocha = if ($Name -eq 'core') {'OFF'} else {'ON'}
    $java = if ($Name -eq 'java') {'ON'} else {'OFF'}
    $editor = if ($Edition -eq 'gold') {'ON'} else {'OFF'}
    $flags = @('-DNETSCAPE_BUILD_WINDOWS_CORE=ON',
               '-DNETSCAPE_BUILD_WINDOWS_FRONTEND=ON',
               '-DNETSCAPE_BUILD_BROWSER=OFF','-DNETSCAPE_BUILD_LEGACY=OFF',
               '-DNETSCAPE_ENABLE_SECURITY=OFF','-DNETSCAPE_ENABLE_BEARSSL_HTTPS=ON',
               "-DNETSCAPE_BUILD_WINDOWS_EDITOR=$editor", "-DNETSCAPE_ENABLE_MOCHA=$mocha",
               "-DNETSCAPE_ENABLE_JAVA=$java", "-DNETSCAPE_BUILD_SUN_JAVA=$java",
               '-DNETSCAPE_BUILD_MOCHA_SHELL=OFF','-DBUILD_TESTING=ON')
    if (Test-Path cmake/targets/windows/plugins.cmake) {
        $plugins = if ($Name -in @('npapi','java')) {'ON'} else {'OFF'}
        $flags += "-DNETSCAPE_ENABLE_NPAPI=$plugins"
    }
    if ($Name -eq 'java') { $flags += '-DNETSCAPE_JAVA_PROVIDER=SOURCE' }
    cmake -S . -B $script:WinFEBuild -G 'Visual Studio 18 2026' -A Win32 @flags
    if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
}
function Check-Item {
    param([Parameter(Mandatory=$true)][string]$Regex)
    cmake --build $script:WinFEBuild --config $script:WinFEConfig --parallel 1 -- /nr:false
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    ctest --test-dir $script:WinFEBuild -C $script:WinFEConfig --parallel 1 `
        --no-tests=error --output-on-failure -R $Regex
    if ($LASTEXITCODE -ne 0) { throw 'Requested checks failed' }
}
Use-Stage core gold Debug
```

Until a stage's target/guard task exists, enabling that stage is expected to fail
configuration; its RED evidence is then supplemented by the real standalone probe.
Do not skip directly to its browser gate. While executing engine plans select
`Use-Stage mocha`, `Use-Stage npapi`, then `Use-Stage java` respectively. Disable
unsupported bridges explicitly until their owners land. Setup/partial targets
must not enable global layout/binding macros until their required adapters link.

For each GREEN item, use its `Check-Item` filter plus the existing neighbor filters
listed in that subplan. Run standard Release too when shared core/WinFE code
changes. Full matrices run at gates rather than blindly repeating every suite
after every tiny change. A new failure, API change or unresolved concern warrants
expanded verification. Perform a fresh scoped review before each commit; do not
commit because only an older configuration passed.

### Gate audit scope

A gate checks prerequisite capabilities owned by its stage and earlier stages,
not unfinished later-engine rows or its own not-yet-recorded completion row.
Record that audit row complete only after its tests pass. Future blocked rows
remain visible and required by their later/full gate. This avoids circular
requirements such as C32 demanding that C32 already be complete before its audit.
Approved exceptions must still carry explicit evidence and user approval.

### Gate-Core: four core configurations and ledger closure

C32 creates `cmake/tests/run_winfe_matrix.py` and `check_winfe_core_gate.py`.
The driver must implement the following CLI and preserve one JUnit/log/source
fingerprint set per configuration:

```powershell
python cmake/tests/run_winfe_matrix.py --stage core --root out/winfe-parity `
    --config Debug --config Release --edition standard --edition gold `
    --exclude '^windows_parity_(c32|m08|n08|j12|s06|r02|r03|r04)$' --report out/winfe-parity/core-matrix.json
python cmake/tests/check_winfe_core_gate.py --ledger cmake/windows-parity.json `
    --matrix out/winfe-parity/core-matrix.json --output out/winfe-parity/core-gate.json
```

The matrix driver reproduces Use-Stage configuration flags, builds, then runs
`ctest --parallel 1 --no-tests=error --output-on-failure --output-junit <unique>`.
It must propagate nonzero build/test/timeout status and record missing prerequisites.
Gate/closure audits C32/M08/N08/J12/S06/R02/R03/R04 are excluded from the
matrix's inner CTest pass by default; the explicit --exclude above documents
that rule. Their item drivers run afterward against the freshly written matrix,
package and ledger reports. Do not count excluded audits as passed product tests
or recursively call the same matrix from its own CTest.
For each Release edition the matrix driver also performs cmake --install, CPack
and check_winfe_package.py into an edition/stage-specific path containing spaces
and a percent character before writing the final successful report. Package
failures propagate as failures. The core gate audit runs only after those reports
exist, so installed_packages_verified is not a guessed native boolean.
C01 initial audit registration uses its own stdlib driver before C02 exists.
C32 is an audit/package gate; it has no unused GUI smoke include.

### Gate-Engine and final supported matrix

```powershell
python cmake/tests/run_winfe_matrix.py --stage mocha --root out/winfe-parity `
    --config Debug --config Release --edition standard --edition gold `
    --include-disabled-core --report out/winfe-parity/mocha-matrix.json
```

Repeat with `--stage npapi` at N08 and `--stage java` at J12. After each matrix
completes, run its item audit with Check-Item windows_parity_m08/n08/j12 as
appropriate; the driver reads the current stage matrix report and performs its
remaining native UI/probe assertions. It must reject a missing or stale report. Each run includes
all stage-relevant original/native/core neighbor tests, installed smoke cases
and appropriate original interpreter/runtime probes; do not run just one new
feature test and call the stage qualified. R02 adds explicit independent-engine
combinations from `cmake/tests/data/windows-supported-configurations.json` using a `--matrix-config` argument,
rejecting unsupported combinations at configure time with actionable errors.
`--include-disabled-core` can reuse current source-equivalent core artifacts only
when input fingerprints/options match; otherwise rebuild/rerun them.

### Installed packages and external evidence

```powershell
cmake --install out/winfe-parity-core-gold --config Release --prefix 'out/parity installed 100% gold'
cpack --config out/winfe-parity-core-gold/CPackConfig.cmake -G ZIP -C Release -B out/parity-packages
python cmake/tests/check_winfe_package.py --stage core --edition gold `
    --build out/winfe-parity-core-gold --prefix 'out/parity installed 100% gold' `
    --packages out/parity-packages --output out/winfe-parity/package-gold.json
```

Repeat for standard and the final qualified stage. C32 introduces the package
checker for core assets; R03 extends it for final runtime/class/plugin data and
clean-machine/off-build execution. Gate results include exact tested executable,
resource and package hashes, source-content fingerprints (including new task
files), original reference provenance, relevant captures and approved exceptions.
A runtime test without a source/asset provenance match does not qualify a package.

Use a native Linux/XFE environment for affected shared C/core changes, recording
actual CMake feature flags and source selection there. Original Gold runtime
inspection follows the existing computer-use workflow or bounded native fixture
capture; do not automate live user accounts or globally install the old release.
Physical printer/clean-machine/original-reference acceptance that cannot be run
is an explicit missing verification item requiring completion or an approved
exception, not a pass inferred from source review.

## Commit discipline and handoff

- [ ] Execute exactly one ready item at a time; keep its RED/GREEN evidence, files, ABI changes and ledger ownership together.
- [ ] Stage only named task files and its ledger/evidence updates; review staged scope and run the task checks immediately before committing.
- [ ] Use the proposed commit messages as concrete intent, updating them to actual final scope if the item splits. No catch-all phase commits and no empty progress commits.
- [ ] For new cryptographic/provider work, S01 must produce a reviewed approved design with exact signatures and a leaf implementation plan before S02-S05 become executable. This is the approved spec's explicit design boundary, not a silent omission from this roadmap.
- [ ] At every gate reconcile required conditional/addendum rows; new scope must have an owner, acceptance test and commit path. Report facts, exceptions and remaining blockers plainly.

For S01-S06 and final qualification start from Use-Stage java. Any additional
security/provider flags come from the separately approved S01 leaf plan and
R02's explicit configuration manifest; do not guess new cryptographic options.

This authoring task writes plans only: no product code, dependency installs,
engine enabling, merge completion or successful new build/test claim is implied.
Before implementation, review these plans and select native inline execution or
subagent-driven execution using the writing-plans handoff. Native keeps one
implementer and a final fresh reviewer; subagent-driven uses fresh task implementer/
reviewer gates. The user has chosen workflow order, not yet an execution method.

## Author self-review

- Spec coverage: phases 0-4 map to C01-C32; phase 5 to M01-M08; phase 6 to
  N01-N08; phase 7 to J01-J12; phase 8 to S01-S06; phase 9 to R01-R04.
- Every initial item names source/test paths, consumed/produced interfaces,
  behavioral assertions, RED/GREEN commands and a proposed scoped commit.
- Cross-plan public native lifecycle signatures live with their owning engine;
  fixture functions follow one exact C02 contract. Existing legacy types/signatures
  stay authoritative rather than copied from a later API.
- Review-focus failure modes each map to explicit task assertions; metadata-only
  success never substitutes for disk/protocol/security output verification.
- Audit/provider/addendum gates keep remaining uncertainties visible. No guessed
  XFE feature, VM source path or absent provider ABI is a completed implementation.
