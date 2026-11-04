# Classic General Preferences and release oracle implementation plan

**Goal:** Reproduce the supplied Preferences window using actual Navigator 3.02
Win32 behavior as the UI reference, within the supported native frontend.
**Architecture:** A shared typed settings contract, a profile-local transactional
store, and seven native tab pages. Root integrates chrome, startup and rendering.
**Reference:** The preserved original 3.02 executable is running locally; actual
captures for all seven pages are under ignored out/research/navigator302/captures.
Windows owns system decorations. Do not add original binaries to the repository.

## Contract and file ownership

`cmd/winfe/preferences.h` fixes the contract shared by the independent store and
UI work. `preferences-settings.cpp` loads/migrates browser.ini Home and saves
all general preferences atomically before applying core defaults. Preserve
unknown INI sections and retain the old file/settings if saving fails.
`preferences-ui.cpp`, `preferences_resource.h` and `res/preferences.rc` own the
native tabs and dialog commands. Root owns main/chrome/classic/frontend/images/
services integration, CMake, the Python fixture and documentation.

## Reference behavior and supported scope

Appearance: Pictures/Text/Both; startup Browser/Mail/News; blank/home URL;
underlined links; never-expire/expire-after days; Expire Now. The fresh original
uses 9 days, both captions/icons, Browser enabled, home selected and underlined
links. Retain about: as this port's offline-safe default home.
Fonts: Latin1 and native proportional/fixed font pickers. Colors: custom text,
link, visited and background swatches with document override. Background-image
selection stays visibly disabled pending separate integration. Images: original
automatic/dither/closest and while/after loading controls, through IL preferences.
Apps: original supporting-application rows, disabled where unsupported. Helpers:
read-only supported MIME actions with unavailable editing/launch controls disabled.
Language: available/accepted lists, add/remove and custom language tag entry;
validate tokens before sending Accept-Language.

## Tasks

- [x] Record original browser/mail/news/composer menus and all Preferences pages;
      document verified version/hashes and measured differences.
- [x] Implement and independently test atomic settings persistence, validation,
      migration and failed-save retention (store worker).
- [x] Implement native reference tab pages and real dialog/cancel/validation/
      persistence/capture checks (UI worker).
- [x] Integrate menu entry, global toolbar presentation, explicit-versus-default
      startup, document font/underline snapshots, LO colors/history and IL options.
- [x] Verify real UI effects, cross-process persistence and loopback language
      headers; review, run full Release/Debug checks, install/package and commit.

The continuing UI roadmap authorizes execution. Use parallel workers only for
the independent UI and store files with the fixed header above. Root continues
the original-binary comparison and owns shared-file integration. No workers may
run concurrent GUI tests or builds in the shared CMake tree without coordination.

## Review focus

Cancel preserves runtime and disk values. Invalid startup/days/fonts/language
values are rejected without closing. Save failure preserves file and memory.
Toolbar style must not erase bitmaps/captions or change composer/utility buttons.
Explicit CLI URLs/modes take precedence over startup settings. Changing document
preferences must not silently resend POST. Original application data remains
separate from the port's NETSCAPE_PROFILE_DIR.

## Verification record

All 17 CTests passed in Win32 Release and Debug. The installed Release binary
passed the Preferences fixture from a path containing spaces. CPack ZIP
integrity, x86 PE machine type, current documentation and exact equality of
built/installed/packaged executables were checked. Native dialog captures were
inspected at 96 DPI. Independent review's HTTP language-buffer finding was fixed
with 492-byte acceptance, 493-byte rejection and persisted-value fallback checks.
Original-release inspection and remaining gaps are in cmake/windows-reference.md.
