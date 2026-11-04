# Windows browser interaction implementation plan

**Goal:** Complete the next browser UI roadmap items in three independently
verified commits while retaining the classic 3.02 frontend.

**Architecture:** Extend the existing MFC BrowserView using original LO
selection, hit-testing and frame-edge interfaces. Keep editor gestures routed
through EDT, native form controls in charge of their own input, and child frame
gestures scoped to their own document. No optional engine port is part of this
batch. The user requested execution and a commit for each item.

**Approach:** Reuse original layout behavior instead of inventing another text
selection or frame sizing model. Add a focused native smoke include and Python
loopback fixture; assertions inspect selection text, clipboard, geometry and
requests, rather than accepting only success flags.

## Task 1: Mouse selection

- [x] Add failing native gesture fixtures for forward/backward drag, Shift
  extension, double-click word selection, clipboard, and link drag suppression.
- [x] Implement capture, drag threshold, selection extension, timer autoscroll,
  capture cancellation and double-click using LO, preserving EDT dispatch.
- [x] Run new fixture, existing chrome/forms/frames/editor regressions, update
  the roadmap and commit.

## Task 2: Link interaction

- [ ] Add failing hover and context action tests, including text/image maps,
  clipboard exact URL, focused-frame navigation and opening a separate window.
- [ ] Share link hit resolution among click, hover and context menu; add classic
  Open/Open New Window/Copy Link Location and View/Copy Image Location actions.
- [ ] Run interaction and relevant existing tests, document and commit.

## Task 3: Frame dividers

- [ ] Add failing divider gestures for vertical/horizontal edges, clamping,
  NORESIZE, capture loss and resize/history preservation.
- [ ] Draw visible original LO edges, provide resize cursors and capture with
  bounded movement, invoke LO_MoveGridEdge on commit, invalidate safely on
  document navigation and closure.
- [ ] Run standard and Gold Debug/Release suites, review actual captures,
  document the verified limits and commit.

## Review focus

- Dragging a link selects text without navigating; clicks still follow links.
- Captured input must stop safely on navigation, closure and capture loss.
- Scroll offsets and child frames must not change selection coordinates.
- Image maps and targets must resolve consistently across mouse actions.
- NORESIZE and edge bounds must survive attempted out-of-range dragging.

Tests run serially because they create real desktop windows. Installed package
refresh is optional for this batch; report only the builds and tests executed.

Task 1 evidence: standard Debug 19/19; Gold Debug browser gestures, classic
chrome, forms, frames and original editor regressions passed. Native selection
capture reviewed. Release coverage will follow after the final browser item.
