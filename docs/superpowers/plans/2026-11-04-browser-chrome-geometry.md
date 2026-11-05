# Gold browser chrome geometry and status implementation plan

**Goal:** Match the original Gold 3.02 browser controls in the user's comparison,
retaining native Windows colors and the original extracted artwork.

**Architecture:** Correct the existing shared MFC chrome rather than replacing
its input controls. Keep the editable native combo/history behavior. Restore
the original Location bitmap and recessed combo appearance. Keep document status
separate from temporary MFC menu/idle messages. Use the original status-pane
proportions, mail indicator and native sizing grip.

**Evidence:** User-provided side-by-side PNG and the running original Gold
window, inspected with the computer-use skill at the same desktop DPI. The PNG
shows 49x41 browser buttons, gaps before Edit/Reload/Stop, variable directory
button widths, the missing page icon, a 50px framed logo and thin status edges.
MFC barstat.cpp confirms WM_SETTEXT writes to the ID_SEPARATOR pane, explaining
why idle/menu updates can erase the displayed document status.

- [x] Add failing native chrome assertions for button bounds, grouping, Location
  icon/combo/dropdown, directory bounds, logo and status pane proportions. Check
  that MFC idle/menu-help updates restore document status, including about:.
- [x] Correct browser-only geometry in chrome.cpp; preserve narrow wrapping,
  visibility toggles, focus/navigation and Mail/News toolbar geometry.
- [x] Extract original bitmap 1239 for Location, retain provenance/hashes and
  regenerate checked-in resources. Restore the complete unknown-mail indicator.
- [x] Correct shared status painting, sizing grip and idle/help behavior; route
  document status writes through the shared API without losing menu help.
- [x] Build/test standard and Gold Debug/Release; review same-sized native
  captures, update documentation/packages and commit this browser roadmap item.

The About page's optional Java/security/plugin content remains a separate engine
integration issue. This change addresses browser chrome, not claims of those
optional engines being enabled.

Regression evidence: the initial native fixture reported Back=(4,3,50,43),
missing Location/logo IDs, a 95px chrome area, pane widths 46/86/34 and failed
idle/menu restoration. The corrected Gold Debug fixture reports Back=(5,3,54,44),
Location=(78,52,844,73), the 17x20 icon and 50px logo, chrome=107 and panes
50/96/50. Captured recessed edges, arrow, pen icon and sizing grip pass pixel
checks, and native F4/Escape and HTBOTTOMRIGHT checks pass (6.99s).

Read-only review found one P2: hover cleanup used the visible menu-help text
rather than stored document status. A new link fixture reproduced leave=0.
Hover now saves/restores the stored document text; temporary menu help remains
visible until the framework clears it. Full verification follows this fix.


Final verification: the reviewed chrome/link regressions passed 2/2 (10.27s),
including the actual WM_SETMESSAGESTRING idle route. The full suites then ran
serially against the final source:

| Edition | Debug | Release |
| --- | --- | --- |
| Standard | 24/24, 107.48s | 24/24, 104.45s |
| Gold | 27/27, 117.68s | 27/27, 121.88s |

The default browser capture was visually compared with the original Gold
window. Native geometry, visible Location icon/recess/arrow, status indicator,
grip, idle/menu restoration, narrow wrapping and toolbar toggles all pass.
The new Location asset is reproducible with the other 97 generated assets;
33 hashed original inputs are retained and 84 compiled bitmap/icon resources
are checked. Both Release ZIPs are refreshed and verified for archive integrity,
executable hashes and current documentation before this item is committed.
