# BearSSL HTTPS implementation decisions

This record preserves implementation judgments from the approved plan's isolated
worktree. It complements the [design](../specs/2026-11-04-bearssl-https-design.md),
[plan](../plans/2026-11-04-bearssl-https.md) and
[verification report](2026-11-04-bearssl-https-verification.md).

The following decisions are listed in the order made. Costs describe the
maintenance burden or consequence if the judgment proves wrong.

1. Implement from the committed plan base, excluding unrelated edits in the
   original checkout. Cost: subsequent mainline changes may need reconciliation.
2. Initially retain the native managed worktree's detached HEAD. Cost: a branch
   name must be assigned or the managed worktree retained for a durable handoff.
3. Make the dependency smoke initialize and inspect real BearSSL state. Cost:
   modest behavioral-test maintenance instead of a function-address comparison.
4. Use gpt-6.1-sol/high for implementation and task reviews, escalating security
   reviews to gpt-6-astra/high. Cost: review time and model usage; independent
   review and final verification remain necessary.
5. Accept UTF-8 non-control informational CA labels required by the pinned
   Mozilla-derived bundle. Cost: a slightly broader metadata grammar; labels are
   discarded and cannot influence trust or identity checks.
6. Move the original dependency smoke into its own source file, preserving its
   CTest behavior. Cost: one additional test file to maintain.
7. Add NETTLS_CreateWithError while preserving NETTLS_Create. Cost: one additive
   API to maintain; typed initialization failures enable useful diagnostics.
8. Make native socket close occur once per invocation, matching native handle
   semantics; only the TLS engine close is idempotent. Cost: callers must not
   repeatedly close a recycled numeric handle; no unbounded tombstone registry.
9. On secure-registry allocation failure, latch untracked I/O closed until
   shutdown. Cost: rare allocation failure may block otherwise ordinary HTTP and
   require restart; ordinary CA configuration failures still allow HTTP.
10. Add an explicit socket pump and Windows configuration/error/legacy-policy
    helpers. Cost: additive interface maintenance; these preserve active policy
    snapshots and restart-only CA replacement.
11. Replace a hung desktop-inspection backend with bounded native captures and
    local image inspection. Cost: the smoke harness owns capture/dismissal
    behavior; native success, failure and preference dialogs were inspected.
12. Fix the demonstrated upload-stage readiness delay before the final matrix.
    Cost: a narrow poller scheduling change requiring no-spin/handshake and real
    upload checks; those focused checks and independent review passed.
13. Add a bounded certificate-purpose guard around unmodified BearSSL validation
    after a real clientAuth-only certificate authenticated and exchanged data.
    Cost: a small DER policy parser and 64 KiB extra per TLS context; presented
    certificates above 65,536 bytes are rejected and critical EKU remains
    unsupported. Underlying validation errors always take precedence.

14. Retain dedicated View Source/context recreation and same-wrapper EKU reset
    tests as optional follow-up. Independent review inspected ownership and
    reset behavior and found no defect. Cost: less dedicated regression
    protection for these transitions, despite broader existing coverage.
15. Accept the final review's explicit boundaries: pinned upstream cryptography
    was not re-audited from scratch; deferred platforms/protocols/capabilities
    and the rejected Java configuration are not supported claims. The unchanged
    shared HTTP producer's analogous read-error weakness, warning cleanup,
    clean-machine deployment and comprehensive leak auditing remain outside
    this TLS change. Cost: those audits/behaviors remain unverified and the
    inherited non-TLS file-upload weakness persists.

## Final review and handoff

Whole-branch review covered baseline
`ceb0a1a806f5df5e79cfdb96e47ca3ac22cfb1c7` through `079acbc` and found one
Important issue, WB-I1: a failed HTTPS upload file read could be treated as EOF.
Commit `4eb3f9a9ae303740799ad226e686ffb5c4cce269` corrects that path and adds
real-UCRT zero/error, short/error, EOF and data/EOF regressions. Independent
scoped re-review marked WB-I1 addressed with no new breakage. No Critical or
other Important finding remains from these reviews.

The full pre-correction matrix and focused post-correction evidence are
separately recorded in the [verification report](2026-11-04-bearssl-https-verification.md).
The controller inspected current focused test logs, native success/failure
captures and the installed/built executable hashes. No extra full-matrix pass,
Linux runtime, clean-machine deployment or warning-free build is inferred.

Work is preserved on `codex/bearssl-https` in the managed worktree
`C:/Users/raynorpat/.codex/worktrees/bearssl-https/netscape`; it has not been
merged or pushed. The refreshed installed executable is
`out/bearssl installed 100%/bin/netscape.exe`, SHA-256
`ab1c650dde9e1b484d0982514fefced1067f8ad0aae1c5d18572019d79889b58`.
The original checkout's unrelated work was left untouched.
