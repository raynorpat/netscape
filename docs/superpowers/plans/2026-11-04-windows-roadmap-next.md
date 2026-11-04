# Windows roadmap continuation plan

> **For agentic workers:** Execute each roadmap item through its own fixture, implementation, review, verification and commit. Use parallel agents only for independent fixture/research work.

**Goal:** Continue the native Windows frontend with a separate reviewed commit for each completed roadmap item.

**Architecture:** Reuse the original 3.02 layout/network/history engines and the existing MFC window, image and form adapters. Native frame cells get their own contexts and document views; downloads get explicit streams and file ownership; browser history/bookmark UI uses the core databases.

**Tech stack:** Win32/x86, MFC, original C backends, CMake and isolated Python fixtures.

**Spec:** [Windows roadmap](../../../cmake/windows-roadmap.md), continuing the user's selected 3.02-compatible native frontend design.

## Constraints and review focus

- Preserve legacy source encodings, other platforms and current mail/news behavior.
- Commit one completed roadmap item at a time; update its status with actual verification.
- Keep optional engines disabled until their native targets and runtime behavior are verified.
- Test nested context destruction, late callbacks, named targets, resize and shared image/form ownership.
- Downloads must preserve bytes, honor cancellation, report failures and avoid leaving an apparently complete partial file.
- History traversal must preserve the intended destination and avoid silently resending POST data.

## 1. Frame contexts

Files: `cmd/winfe/main.cpp`, `frontend.c`, `frontend.h`; `cmake/tests/check_windows_frames.py`; native frontend CMake target and Windows docs.

- [x] Establish a failing frameset fixture with real missing child requests.
- [x] Create native child views/contexts with the original grid metadata, history and cleanup contracts.
- [x] Route named and special link/form targets; support nested cells, focus, independent scrolling and resizing.
- [x] Verify text/image/form rendering, target navigation, nested layout, resize and cleanup in Release/Debug; review and commit.

## 2. Saving and downloads

Files: focused `cmd/winfe/downloads.cpp`/`.h`, native menu/callback glue, independent binary/error/cancel fixtures and CMake tests.

- [x] Trace original save-stream/picker contracts and establish a failing binary fixture.
- [x] Implement native destination selection, explicit binary stream ownership, progress, cancellation and error handling.
- [x] Verify successful bytes, failure/partial cleanup and cancellation; review and commit.

## 3. History navigation and UI

- [x] Replace private URL traversal with SHIST, preserving form/scroll/grid data.
- [x] Add command state, native history selection and visited-page display.
- [x] Reproduce and fix fixed-width date reads and expired POST confirmation.
- [x] Complete Release/Debug regression checks, review and commit.

## 4. Bookmark manager

- [x] Integrate the original bookmark tree with native add/folder/edit/open/delete controls.
- [x] Verify safe backend lifetime, persistence and special-character round trips.
- [x] Review, run regressions and commit independently.

## Execution record

Starting commit: `b8e0a42`. Existing eight native CTests passed in Release and Debug; native ZIP/install and Linux build checks are recorded in `cmake/windows.md`.

Frame contexts: complete. Nine CTests passed in both Win32 Release and Debug, including split-response target transfer after the original child is destroyed. The Linux browser rebuilt with the descriptor-query API. Review findings on URL ownership, per-request watches, resize scheduling and retained select state were addressed.

Saving/downloads: complete. Ten CTests passed in Release and Debug; Linux rebuilt after current-response header metadata and identity encoding handling. Actual identity truncation and redirect failures were reproduced before fixes. Destination creation/final rename errors, cancellation, browser closure and exact output bytes passed independent fixture checks. Mid-write disk exhaustion remains an inspected limit.

History: complete. Eleven CTests passed in Release and Debug; focused browser checks passed after the final position-saving changes. The Linux browser rebuilt with fixed-width global-history dates and the expired-POST guard. Independent review found no remaining P1/P2 findings.

Bookmarks: complete. Twelve CTests passed in Release and Debug; Linux rebuilt.
Actual raw HTML title loss and fixed-line long-URL loss were reproduced before
fixes. Fixtures check Add Page, native edit/delete/open/manager reopen, quoted
and literal-entity text, long escaped URLs, failed rename preservation/retry,
legacy wrapped titles and boundary descriptions. Core review findings on buffer
lifetime, append slack and failed-read guards were addressed; allocation failure
and import recovery were inspected rather than fault-injected.
