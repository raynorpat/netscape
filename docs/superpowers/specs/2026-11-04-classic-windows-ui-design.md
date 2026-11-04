# Classic Windows UI

The user's supplied Navigator 3.04 Gold screenshot defines the visual reference
for this 3.02 port: compact gray chrome, raised icon/text toolbar buttons, the
Location row, six directory shortcuts, a right-hand Netscape logo, traditional
menus and a segmented bottom status bar. Keep the actual version at 3.02.

Use native MFC windows and existing 3.02 GIF artwork. A modern flat toolbar or
a web-rendered imitation would conflict with the requested appearance and the
existing native frontend. Owner-drawn native buttons provide the classic edges
and spacing while retaining keyboard focus, tooltips and command routing.

Apply the same font, gray surfaces, borders and spacing to mail/news, composer,
bookmarks, history, downloads and account/prompt dialogs. Browser and messaging
toolbars use commands appropriate to each window. Preserve the existing MSG,
SHIST, network and rendering paths. Commands without a backend remain visibly
disabled; do not disguise unavailable editing/security features as working UI.

The first implementation uses DPI-scaled metrics based on the reference at
96 DPI, wraps toolbar rows at narrow widths, keeps document space usable, and
leaves the operating system responsible for the title bar and system menu.
View menu switches expose toolbars/location/directory/status visibility.
Home opens the built-in about page; historical directory destinations retain
their original URLs. No startup request to an external site is needed.

Verify command behavior, disabled states, resizing, keyboard location entry,
visibility toggles and native rendering using isolated fixtures. Inspect real
window captures alongside the supplied reference, then run the existing suite.
Commit browser chrome and companion-window styling separately, as requested by
the standing roadmap workflow.
