# Classic News reference UI implementation plan

**Goal:** Apply the supplied Netscape News reference to the native 3.02 client.
**Architecture:** Retain MFC, MSG-owned lists and the shared draggable pane
helper. Specialize News chrome, list presentation and initial geometry. Reuse
original source artwork; Windows continues to draw system decorations.
**Execution:** Continue the already authorized UI roadmap in this workspace.
The reference supplies the visual direction; no additional design choice is
required from the user. Implement directly with a final independent review.

## Design and scope

News has File/Edit/View/Message/Go/Options/Window/Help menus and twelve buttons:
To:News, To:Mail, Re:News, Re:Mail, Re:All, Forward, Previous, Next, Thread,
Group, Print, Stop. Post-and-mail reply maps to Re:All. Thread and Group mark
the selected thread/group read through MSG. No browser Location/directory bars.
The server/group list occupies about half the upper band, with tree guides,
expand/collapse glyphs, host/group artwork, subscription state and unread counts
(??? when unknown). Article columns are Sender/status/Subject. The upper band
initially uses about three quarters of available height, with a full-width
gray empty reader below; loaded articles retain their document colors.

Use shared existing panes rather than separate tree widgets: this preserves
backend row identity and column alignment. Custom-draw only the News tree cell;
keep native selection, keyboard focus and column controls. Subscription state
and expansion gestures call the backend. Existing mail presentation stays intact.

## Tasks

- [x] Add a loopback NNTP reference fixture and native observation/capture path;
      confirm the current News toolbar/columns/empty-reader geometry fail.
- [x] Implement News toolbar, menu/accelerator commands and backend states.
- [x] Add News tree cells, subscription controls, unread counts, initial panes
      and empty-reader background. Check native mouse/keyboard gestures.
- [x] Verify an empty/expanded/collapsed server, group selection/article read,
      post versus mail composition, replies, mark-thread/group read, narrow
      resizing and divider clamps against real backend state.
- [x] Run Release and Debug suites, review and inspect captures; update docs,
      refresh the installation/ZIP, verify the installed client and commit.

## Review focus

No article selected: replies and Thread/Group availability follow MSG.
Unknown counts: show ??? without reading uninitialized directory counts.
Collapse/re-expand and subscription toggles preserve backend row identity.
Article load and resize retain a live context and correct scroll-page geometry.
Mail/browser layouts and composer commands retain existing regression coverage.
All network checks use isolated profiles and loopback NNTP, with no posting.

## Implementation evidence

The initial fixture fetched and rendered real NNTP articles, then failed on the
old mail-derived toolbar/columns and short-list/white-reader layout. The new
focused fixture passes those reference checks and real native mouse/keyboard
gestures, subscription changes, post versus mail and all three reply routes,
thread/group marking and narrow/divider geometry.

Subscribed-view coverage reproduced a real regression in the previous startup
fallback: every completed News request forced All Groups and discarded article
selection. Discovery now runs only initially or after a server change, and
preserves explicit view choices. Independent review also found browser Post
News disabling, uninitialized directory counts and leaf expansion; those are
fixed with runtime guard checks. Native list mouse-down requires a queued
release in the fixture, and composer close is asynchronous; the fixture now
uses both native lifecycles correctly. Shared backend files are unchanged.

The final capture check also verifies actual Netscape logo pixels and geometry.
Each fixture invocation now creates its own profile (including spaces and a
literal percent sign), so a Debug run cannot reuse Release's NNTP discovery
cache and silently omit the LIST request.

Final full-suite verification: all 16 CTests passed in Release (83.62 seconds)
and Debug (100.75 seconds). The independent final review confirmed all three
guards fixed and found no remaining actionable P1/P2 issue. Empty and populated
native captures were inspected at 96 DPI. Windows still draws the title bars;
printing remains disabled. Shared backend/Linux sources were not changed.

The refreshed Release installation passed the News fixture, including NNTP
LIST/group/overview/article traffic and all composition/gesture checks. CPack
produced the x86 ZIP; its executable exactly matches the tested Release and
installed files, and the packaged Windows guide matches the source. Shared
Visual C++/MFC runtimes are still required.
