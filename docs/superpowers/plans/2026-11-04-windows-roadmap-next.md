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

- [ ] Trace original save-stream/picker contracts and establish a failing binary fixture.
- [ ] Implement native destination selection, explicit binary stream ownership, progress, cancellation and error handling.
- [ ] Verify successful bytes, failure/partial cleanup and cancellation; review and commit.

## 3. History and bookmark UI

- [ ] Inspect SHIST/global-history/bookmark APIs and add behavior fixtures before implementation.
- [ ] Connect traversal and command state, then native bookmark/history browsing and persistence.
- [ ] Verify traversal, named frame behavior, stored values and persistence; review and commit separately where the behavior forms independent roadmap items.

## Execution record

Starting commit: `b8e0a42`. Existing eight native CTests passed in Release and Debug; native ZIP/install and Linux build checks are recorded in `cmake/windows.md`.

Frame contexts: complete. Nine CTests passed in both Win32 Release and Debug, including split-response target transfer after the original child is destroyed. The Linux browser rebuilt with the descriptor-query API. Review findings on URL ownership, per-request watches, resize scheduling and retained select state were addressed.
