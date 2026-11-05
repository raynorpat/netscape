# Windows WinFE completion and parity design

Date: 2026-11-04. Status: approved by the user on 2026-11-04 after builder review.

## Intent and agreed decisions

Finish the native Windows frontend so its applications look and behave like
Netscape Navigator Gold 3.02 on Windows and cover the working capabilities of
this repository's XFE frontend. Preserve the original browser, layout, messaging,
editor and optional language engines rather than embedding a replacement browser.

The user explicitly chose:

- Original Windows Gold as the UI reference, with XFE as the feature reference.
- Core application and Gold UI parity before the remaining engine ports.
- Include JavaScript/Mocha, Java applets and NPAPI plugins in the completion roadmap.
- Complete workflows across applications rather than finish one application at a time.
- A separate commit for each completed item.

This is a project spanning several implementation phases. This document defines
the target and phase boundaries. After approval, a separate implementation plan
will break the phases into executable tasks, file changes and verification commands.
Approval of this document is not a claim that parity has already been established.

## Approaches

1. **Complete workflows across applications (selected).** Finish a user operation
   through its commands, UI, shared backend, persistence, cancellation and errors
   wherever it appears. Shared chrome, preferences, document services and MSG/BM
   behavior stay consistent. Individual applications reach final parity together
   rather than one application being declared complete prematurely.
2. **Complete applications in sequence.** Finish the browser, Mail/News, composer,
   address book and Gold editor separately. This yields a finished application
   earlier but revisits shared controls and services during later application work.

The user selected the first approach. The original native MFC/Win32 frontend and
shared 3.02 engines remain the architecture; replacing them is not an alternative
under this design.

## Baselines and what parity means

### Visual and interaction baseline

Use the preserved English Windows **3.02 Gold** executable/resources and the
user's original-release captures. Later 3.04 captures provide context where no
3.02 observation exists, but cannot override measured 3.02 behavior. Provenance,
existing captures and original menu transcriptions are recorded in
[windows-reference.md](../../../cmake/windows-reference.md).

Match window client layouts, original artwork, toolbar groups/captions, menus,
accelerators, enabled/checked states, default sizes, pane headers, selection,
status text, dialogs, keyboard focus and interactions. Compare the same content,
client size, DPI and system palette on the same host. Windows owns nonclient
decorations, native dialogs, system colors and sizing borders; do not recreate
Windows 95 title bars or reintroduce fixed gray surfaces. Document rendering is
evaluated separately from application chrome.

Retain the original resources already recovered where their provenance permits
use. Do not substitute newly drawn approximations for available original icons.
Missing reference observations are research tasks, not permission to declare a
plausible-looking dialog equivalent. A modern extension such as the staged TLS
preferences must be identified as an extension, not relabeled as original UI.

### Functional baseline

Use the actual configured XFE build and corresponding shared engine behavior,
not every file under `cmd/xfe`. Inventory the original makefiles, current CMake
manifests, feature defines, commands, callbacks and backend implementations.
Record whether behavior is reproduced at runtime, demonstrated by a test,
source-only, conditional, stubbed, unbuilt or unavailable.

For example, `cmd/xfe/mailfilter.c` and `addrbk.c` exist but were not found in
the inspected primary XFE makefile's source list. Their presence alone does not
establish working 3.02 filter or newer address-database functionality. IMAP source
also exists in netlib; inspect actual selection and functioning backend before
promising an IMAP frontend. Conversely, working original Windows Gold features
remain required even when XFE exposes them differently or lacks an equivalent.

Represent Unix-specific operations by the corresponding useful native Windows
workflow where possible. Motif widgets, X resources, X remote-control transport,
Unix spool locking and process launch mechanisms are implementation details,
not UI requirements to copy literally. Any feature with no useful Windows
equivalent gets an explicit proposed exception with evidence and user approval.

### Parity ledger

Create one authoritative, checked-in capability ledger during phase 0. Each
row has a stable ID and records:

- User workflow, applications and entry points: menus, toolbar, shortcuts,
  context menus, command line, links or backend callbacks.
- Gold reference version, resource/capture/observation and known differences.
- XFE source/build condition, shared backend and evidence of working behavior.
- WinFE implementation, current state and dependencies.
- UI, state/lifetime, persistence, error/cancel and verification requirements.
- Owning phase, commit-sized items, test/capture evidence and completion status.
- Classification of an intentional extension or approved exception, if any.

States distinguish unassessed, missing, partial, blocked by an engine, behavior
verified, UI verified and complete. A linked archive, enabled menu, or successful
compile does not establish runtime completion. Stub searches help inventory;
they do not automatically turn valid empty callbacks into defects.

## Current implementation and integration baseline

The frontend parity slice is `9f549f1`, following address-book commit `43608a4`.
The subsequent integrated project baseline is merge commit `505156b` (BearSSL HTTPS).
WinFE already has native browser chrome, text/image/layout rendering, forms,
frames, downloads, history, bookmarks, Mail/News panes, plain-text composition,
address book/recipient selection, preferences, printing and optional Gold EDT
editing/publishing. Most areas have explicit limitations, listed in
[windows-roadmap.md](../../../cmake/windows-roadmap.md) and
[windows.md](../../../cmake/windows.md); "implemented" is not a parity verdict.

The last frontend slice built standard and Gold Win32 Debug/Release. Its new
address-book/picker checks passed all four and both installed Release editions.
Full suites were 24/26 standard and 26/29 Gold, with Welcome/News timeouts and
additional Gold background-preference timeouts. Native stack samples blocked
in GDI AlphaBlend and tooltip WindowFromPoint. The prior packaged executable
also reproduced the Welcome timeout. These observations do not establish the
root cause or excuse the failures; phase 0 must restore a trustworthy baseline.

BearSSL HTTPS integration has its own [design](2026-11-04-bearssl-https-design.md),
[decisions](../reports/2026-11-04-bearssl-https-decisions.md) and
[verification](../reports/2026-11-04-bearssl-https-verification.md).
It was staged in an active merge when this design was drafted; the integration
owner completed that merge as `505156b` while the approved implementation plans
were being authored. Those reports describe specific tested source revisions;
no fresh merged-source matrix is inferred. Preserve that integration, record
the actual execution base, and refresh the ledger/build baseline before work.
This documentation task did not modify, duplicate or revert its product code.

Windows CMake currently rejects the original Mocha, Java and legacy security
combinations. NPAPI frontend loading/streaming and Java element callbacks remain
stubs. The legacy security archives are separate from the new HTTPS adapter;
HTTPS alone does not provide S/MIME, client certificates or secure mail transport.

## Architecture and constraints

- Retain native MFC/Win32 controls, GDI rendering and system colors. Preserve
  keyboard operation, focus, resizing, scroll behavior and classic artwork.
- Reuse original LO/IL, SHIST, BM, MSG/MIME, EDT, Mocha/libmocha, NPAPI and
  Sun Java/JRI components where applicable. Adapt at native frontend boundaries.
- Reuse compatible recovered 1998 MFC components/resources as references or
  adapters; do not introduce later Communicator API assumptions into 3.02 core.
- Keep shared engine changes narrow, preserve legacy source encodings and style,
  and verify affected XFE/Linux configurations when shared code changes.
- Continue modular CMake: the root assembles targets from `cmake/targets/`;
  Windows-specific selection belongs under `cmake/targets/windows/`, reusable
  imported modules under `cmake/modules/`. Do not enable incompatible option
  combinations by merely deleting configuration guards.
- Target the existing Win32/x86 ABI first. Native x64 is a separate proposed
  follow-up, not a substitute for finishing this port. Do not mix Win32 plugins
  or historical Java components into a native x64 process.
- Protect existing profiles, address/bookmark HTML, mailboxes, drafts and
  publishing destinations. Stage edits until acceptance; preserve recoverable
  state on failed writes, cancellation and reentrant close/navigation.
- Every visible command must work, be correctly disabled for the current state,
  or be marked as an engine-dependent item in the ledger. Do not hide a gap by
  changing the reference fixture to agree with the port.
- Use original-era compatibility as the language/layout target. Modern Web
  standards, a modern JavaScript engine and contemporary provider authentication
  are not implied by XFE parity. Newly requested extensions require explicit scope.

## Phase 0: Inventory, integration and reliable verification

1. Establish a clean implementation base that includes the intended HTTPS
   integration without disturbing unrelated changes. Record exact source,
   configuration, toolchain and package provenance.
2. Build the parity ledger from original resources, current WinFE, actual XFE
   source selections and shared callbacks. Include every menu/shortcut/context
   route, preference page, dialog family and feature conditional.
3. Reproduce and diagnose the current GUI hangs with bounded native traces and
   minimal fixtures. Separate host interference, fixture defects and product
   defects; correct demonstrated causes. Do not increase timeouts or bypass
   rendering merely to obtain green results.
4. Establish reproducible Gold reference profiles and local content/messages,
   with client size/DPI/theme recorded. Obtain XFE runtime evidence where a
   compatible build is available; explicitly retain source-only uncertainties.

**Exit:** a reviewed ledger, integration baseline and reliable core verification
matrix, with any unresolved blocker explicitly identified before parity claims.
This phase does not require implementing all missing product features.

## Phase 1: Shared application shell and Gold UI

Complete chrome, menus, accelerators, context menus, prompts, tooltips, focus,
window activation/default sizes, pane layout, status/help/progress and native
palette behavior across browser, Mail/News, message composition, address book,
bookmarks/history, downloads, preferences and Gold editor.

Audit every original command's execution, update/check state and ownership.
Complete shared preferences/persistence, startup/window routing and account
configuration before adding dependent workflows. Include proxy/connection,
cookie, helper, cache, charset and security surfaces in the inventory; each
becomes active with its corresponding backend workflow. Existing menu extensions
remain separately documented. Unsupported engine controls may remain accurately
disabled until their engine phase; they cannot count as full parity yet.

**Exit:** all available core shell operations behave correctly at reference DPI
and narrow sizes; remaining engine-dependent routes are explicit ledger entries.

## Phase 2: Browsing, files, network settings and document workflows

Finish complete workflows through shared dialogs, backend behavior and storage:

- Navigation, redirects/reloads, history, links, focused frames, selection and
  clipboard, find, image loading, page/source/info and local-file handling.
- Helper associations and safe native launching, MIME/content disposition,
  external protocols and download actions. Audit supported FTP and other legacy
  protocol paths from the configured baseline, including cancellation/errors.
- Memory/disk cache controls, location/history persistence, cookies, proxy and
  connection settings. Add crash/write-failure coverage before enabling disk cache.
- Form keyboard traversal, multipart/file uploads, reset and history restoration;
  interrupted loads, frames, image animation/progressive updates and print flows.
- Fonts, document charset changes, conversion, MIME text decoding and clipboard
  behavior for encodings actually supported by the original baseline.

Audit existing implementations before replacing them. Complete physical printer
and driver-dependent acceptance separately from captured preview/vector tests.
Preserve the staged HTTPS adapter's protocol and certificate validation policy.

**Exit:** browsing/file/settings workflows have real local/loopback evidence,
matching command states, persistent settings and verified failure/cancel behavior.

## Phase 3: Personal data, Mail/News and composition

- Bookmarks/address book: import/export where present in the baseline, search,
  multiple selection, clipboard, ordering/organization and drag/drop; exact
  property/picker behavior, list membership, nickname expansion and persistence.
- Mail: account/POP retrieval, local folders, create/rename/delete/compact,
  move/copy, search, sorting/threading/selection, reader headers, attachments,
  save/open/print and offline/queued work exposed by the baseline.
- News: host/group discovery, subscribe/unsubscribe management, group/article
  views, unread/thread actions, navigation, search, reader attachments and posting.
- Composition: original recipient/attachment interactions, quoting/forwarding,
  drafts/queue/edit-again, send/stop/retry, account/header settings, print and
  closure/cancellation. Do not assume Mail composition was a rich HTML editor;
  reproduce demonstrated Gold behavior.
- Filters, IMAP or newer address-database behavior: inspect compile selection,
  backend completeness and release relevance in phase 0. Implement them if they
  belong to a working baseline; otherwise classify the evidence and seek approval
  for any scope exception rather than inventing absent later APIs.

Use original BM/MSG/MIME storage and command services. Keep filesystem operations
checked, recoverable and independently tested. Loopback POP/SMTP/NNTP and local
mailboxes establish behavior without sending user mail or posting public news.

**Exit:** supported Mail/News/personal-data workflows are complete through actual
storage and protocol results, not just visible panes or enabled menus.

## Phase 4: Gold editing and core application parity gate

Finish original Gold editor formatting, selection/clipboard, insertion/property
dialogs, tables/images/links, toolbar/menu state, preferences, file/source editing,
save/reopen, publish, printing and cross-window browser preview. Audit EDT image
save and asynchronous publish/save lifetime, reentrant close, cancellation,
failures and relative URL handling. Retain existing native extensions distinctly.

Reconcile every remaining core ledger row across all applications. Review error,
startup, modal, narrow-window, keyboard and DPI behavior as well as happy paths.
Produce matched Gold comparisons and a release candidate for standard and Gold.

**Core gate:** all non-engine capabilities from the agreed baselines are complete
or have explicit approved exceptions. Relevant standard/Gold Debug/Release and
installed/package checks pass, and no unexplained timeout remains. A core parity
release may precede engine phases, with engine gaps accurately disclosed.

## Phase 5: JavaScript/Mocha

Port the original engine first as an independently built/tested native target,
using `mocha/makefile.win`, the shared manifests and original tests as references.
Then integrate libmocha and layout/browser bindings: script execution, window/
document/location/history, forms, links/images, frames, events, timers, dialogs,
preferences, cancellation, navigation and context destruction.

Honor original language behavior and origin/taint/security checks. Exercise
event reentrancy and shutdown, not only expression evaluation. Keep Java/plugin
bridges explicitly disabled until their dependent engine phases work; the
standalone script/browser integration must not require a fake Java runtime.

**Exit:** native engine tests plus browser behavior/negative-origin/lifetime
fixtures pass; disabled-engine builds remain functional. No modern ECMAScript
conformance is implied.

## Phase 6: NPAPI plugins

Implement native plugin discovery/descriptors, supported MIME registration,
Windows exports/calling conventions and ABI negotiation, module lifetime,
browser callback tables, instance creation/destruction, embedding/window changes,
network stream delivery, notifications and supported printing/focus behavior.

XFE's dynamic loader is a behavioral reference, not Windows binary ABI glue.
Use an in-tree controlled Win32 test plugin to prove actual callbacks and bytes,
including malformed/version-mismatched modules, failed loads, multiple instances,
stream failure/cancellation and unload after navigation/window closure.
Inventory windowless/other optional NPAPI capabilities against the actual era's
headers and baseline; do not promise later NPAPI versions.

Historic third-party plugin binary smoke tests are optional compatibility evidence
and need not block deterministic coverage using the controlled plugin. Native
plugins remain in-process, as required by the existing ABI; isolation in a broker
would be a separately designed extension. Retain an explicit enable/disable control.

**Exit:** supported NPAPI workflows run through a real DLL and browser contexts,
with deterministic lifetime tests and working plugin-disabled builds.

## Phase 7: Sun Java runtime, AWT and applets

Port the source-built runtime/tool/JRI components in dependency order, then the
existing Windows AWT peers under `sun-java/awt/win`, then libjava browser contexts
and applet embedding/lifecycle. Reuse the Windows makefile's selections as
evidence, not the current Unix AWT target or a modern JVM substituted by default.

Include class resources/generation/packaging, native calling conventions,
threading/GC interaction with Win32 NSPR, AWT events/focus/painting, console,
preferences and missing/mismatched-runtime diagnostics. Verify applet init/start/
stop/destroy during reload, frame changes, resize, interrupted loads and shutdown.
Preserve original applet origin/sandbox enforcement with negative tests.

Restore script/Java and any plugin/Java bridges only after the participating
engines pass independently. Cover conversion, exceptions, origin checks and
ownership at those boundaries. A feasibility blocker becomes a concrete report
with required changes; it does not silently turn Java into an abandoned goal.

**Exit:** source-built runtime, native AWT and controlled applets work in browser
contexts; lifecycle and sandbox checks pass; runtime-disabled builds work and
the package has verified class/native-resource paths.

## Phase 8: Remaining security and conditional XFE capabilities

Keep this phase explicit so HTTPS is not mistaken for complete security parity.
Inventory and complete user-facing certificate/security information, trust and
certificate management, client identity and signing/encryption workflows that
belong to a working baseline. This includes S/MIME or security/archive services
if confirmed by the chosen XFE configuration. Resolve JAR/zlib and required
security-library dependencies independently before enabling their frontend routes.

Preserve the approved modern BearSSL HTTPS policy. Do not re-enable SSL 2/3,
disable certificate validation, or port obsolete cryptography merely to match a
historical toggle. A parity capability needing a new cryptographic provider or
unsupported adapter API requires its own reviewed design before implementation.
Secure mail transport and modern provider authentication are extensions unless
established as part of the selected baseline; record their status explicitly.

Also close out conditional protocols, platform integrations and features found
by phase 0 that do not fit earlier workflows. No source-only uncertainty can be
silently discarded at this phase.

**Exit:** every confirmed baseline capability has an implementation or an
explicitly approved exception, including its required backend and native UI.

## Phase 9: Combined-engine qualification and release

Validate standard/Gold engine-disabled and supported engine-enabled builds,
including the combined Mocha/NPAPI/Java configuration. Generate a supported option
matrix instead of assuming every permutation works. Cross-engine bridges have
dedicated tests after independent engine phases, followed by combined workloads.

Complete portable native resource regeneration, dependency/license packaging,
runtime/class/plugin lookup, clean-machine installation, profile upgrade/data
preservation and release ZIP/install checks. Test installed binaries from paths
containing spaces and percent characters, without relying on the source/build
directory. Keep standard and Gold packages distinct and identify required MFC/
Visual C++ runtimes. Verify executable/resource hashes and package contents.

**Full gate:** the ledger is reconciled, all supported configurations pass their
required tests, Gold UI comparisons are reviewed, installed behavior is verified,
and every exception/extension is documented with user approval where required.
Native x64 and modern-Web compatibility remain separate proposed projects.

## Verification and reference policy

- Keep original UI reference data independent of WinFE implementation. Record
  observations and expected values before changing the product where possible.
- Use synthetic local HTML/images/forms, MIME messages, mailboxes, plugin DLLs,
  applets and loopback protocol servers for deterministic behavior tests.
- Native captures and control bounds verify appearance; backend/disk/protocol
  assertions verify results. Neither alone establishes the other.
- Each workflow tests the important failure, cancel, retry and lifetime cases;
  avoid tests that merely reproduce its implementation or assert an archive exists.
- Run GUI fixtures serially. Build in independent directories and preserve test
  source/configuration provenance. Build or GUI-tool hangs are explicit failures
  to diagnose, not automatic product regressions or automatic environment excuses.
- Baseline comparisons may use an isolated VM if the original release is unstable
  on the host. Do not install or alter system associations just to inspect UI.
- Synthetic mail/news stays local. Use explicit temporary profiles; never use a
  user's live mailbox, credentials, publishing site or personal plugin collection
  as an automated acceptance fixture.
- For shared core changes, build/test the affected Linux/XFE path where available.
  Missing environments remain recorded verification gaps, not fabricated passes.

## Commit and planning contract

One item is a reviewable user operation or a necessary engine/build foundation,
not an entire phase. A workflow item normally includes its native UI/commands,
backend wiring, persisted state, errors/cancellation, relevant tests and ledger
update in one commit. Split a large item into prerequisite foundation commits
followed by behavior commits; do not label the foundation as working parity.

The implementation plan will assign stable IDs, exact touchpoints, dependencies,
acceptance cases and verification commands to those commits. Phase 0 may refine
their count as evidence resolves unknowns. Do not provide calendar estimates
before the inventory and engine feasibility work; effort/risk can be ranked.
Before each commit, review the diff and verify the relevant checks. Integrate
only authorized files and preserve other staged/worktree changes. Pushes and
unrelated branch merges are not implied by this design.

## Builder decisions and remaining uncertainties

The following are proposed technical calls, not additional user statements:

- Maintain Win32/x86 as the parity target and separate native x64 work.
- Sequence Mocha, NPAPI, then Java, with combined bridges afterward. The builder
  may reorder independent foundations when real dependencies require it, while
  preserving the user's core-first order and visible engine phase boundaries.
- Use the existing MFC/core architecture, original language/runtime implementations,
  controlled fixtures and current Windows palette. Do not replace the browser/VM.
- Keep the approved HTTPS adapter and its policy; design additional security
  capabilities separately where the provider/backend cannot supply them today.
- Make the compiled baseline inventory authoritative over filenames, with explicit
  user review for real omissions rather than silently excluding difficult features.

Unknowns to resolve through planned audit/feasibility tasks include exhaustive
command coverage, source-only XFE facilities, original dialogs not yet exercised,
the Win32 graphics/tooltips hangs, AWT/runtime platform completeness and optional
security-provider coverage. They have owners/phases above and do not require
guessing that an unverified feature already works.

## Approval and next artifact

The independent builder check returned "nothing important left" on 2026-11-04.
No additional user decision was required by that review. The proposed technical
calls above remain visible for the user's design review.

After the builder check and user approval of this design, invoke the
`superpowers:writing-plans` skill to create the executable implementation roadmap.
The first detailed implementation tranche is phase 0 followed by shared/core
workflows; later engine phases retain explicit feasibility and integration gates.
No product implementation is authorized by an unapproved design document.
