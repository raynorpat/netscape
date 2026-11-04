# Classic Windows UI Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development for independent companion-window styling and fixtures; keep shared chrome/main integration with the coordinator.

**Goal:** Match the supplied Navigator screenshot throughout the native Windows frontend.

**Architecture:** A reusable classic native chrome module owns painted toolbar
buttons, DPI-scaled metrics, artwork and fonts. Existing window/controller
objects retain backend ownership and route commands. Original GIF assets are
mechanically converted to compiled resource bitmaps with provenance.

**Tech stack:** Win32/x86, MFC, GDI, RC, CMake and isolated Python fixtures.

**Spec:** [Classic UI design](../specs/2026-11-04-classic-windows-ui-design.md).

## Constraints and review focus

- Preserve source encodings, child-frame layout and current mail/news behavior.
- Keep the actual application version; the reference's Gold editor is not present.
- Use native keyboard focus and real command state; no enabled dead buttons.
- Check narrow/large windows, hidden bars and DPI-scaled geometry.
- Retain existing stream/context lifetimes when changing window chrome.

## 1. Browser and messaging chrome

- [x] Establish a failing native UI fixture for toolbar/location/menu/status structure.
- [x] Reuse original artwork; add classic button/font/chrome helpers and provenance.
- [x] Add reference toolbar, Location row/dropdown, shortcuts/logo and status panes.
- [x] Restore traditional menu organization, View toggles and available command routing.
- [x] Give mail/news their own command toolbar using the shared chrome.
- [x] Check captures, behavior and Release/Debug regressions; review and commit.

## 2. Companion windows

- [x] Apply shared classic treatment to composer and utility windows.
- [x] Verify existing mail/news/composer/bookmark/history/download fixtures.
- [x] Review and commit independently; refresh install and ZIP.

## Execution notes

Starting commit: `2b6a592`. The screenshot supplies the visual design; existing
authorization to continue the native port covers these reversible UI choices.
Ruling: native MFC chrome and original artwork are the single compatible approach
for the requested appearance; retain OS-managed nonclient decorations.

Actual RED: the initial chrome CLI returned status 2. The disabled-artwork pixel
check also failed against the first native capture, before the gray bitmap fix.
Review identified focused-frame lookup, messaging command states, resource-dialog
styling and about-buffer release contracts; all were addressed. The read-only
resource parser crash was reproduced in Release/Debug and traced with CDB to
pa_CreateMDLTag writing input. Writable request buffers plus source-scoped
cleanup fix it; the about fixture checks zero outstanding allocations.

Visual checks use native PrintWindow captures, including browser about, mail,
composer, bookmarks, account settings and prompts. No mockup replaces a runtime
capture. High-DPI/multi-monitor behavior remains unverified beyond scaled metrics.

Integrated verification: all thirteen CTests passed in Release and Debug. The
Linux browser rebuilt and reported version 3.02 with source-scoped Windows
about-buffer ownership. Fresh review found no remaining P1/P2 findings.

Install and ZIP refreshed. The installed executable passed the classic-chrome
fixture; the ZIP contains the identical tested Win32 binary. Companion-window
styles are committed independently from shared browser/messaging chrome.
