# Native Gold editor implementation plan

**Goal:** Enable the original 3.02 EDT engine and implement a native Windows
WYSIWYG editing window guided by the original Gold resources and running UI.
**Architecture:** Optional native Gold configuration adds the original editor
sources and consistent EDITOR/GOLD definitions across core and frontend. Reuse
the LO/GDI document surface. Separate native editing controls from the mail
composer; EDT owns editable HTML, selection, formatting, undo and serialization.

- [x] Establish an editor smoke fixture, then compile the seven original
  BUILD_EDITOR engine sources with MSVC; resolve actual compiler/link failures.
- [x] Implement caret, document lifecycle, progress/save and file callbacks.
  Use EDT_SaveFile and preserve dirty state on failed/cancelled saves.
- [x] Reproduce Gold editor menus/toolbars and wire supported editing,
  formatting, insertion and property actions to EDT. Keep unavailable optional
  services visibly disabled; do not replace the engine with a text widget.
- [x] Integrate editor windows, parser routing, mouse/keyboard input, selection
  highlighting, scroll/caret and dirty-close handling into the native renderer.
- [x] Exercise real edits, keyboard selection, clipboard, undo/redo, formatting,
  local HTML saving/reopening and native captures. Run standard/Gold regression
  suites in Release/Debug, review, install/package, document and commit.

Contracts are in cmd/winfe/editor.h. Engine worker owns CMake/engine portability;
platform worker owns editor-platform.cpp; UI worker owns editor-ui.cpp and new
Gold resource files; root owns main/frontend integration and test orchestration.
Worker builds use separate directories. Root owns GUI tests and shared builds.

Review focus: EDITOR changes parser/context layouts, so definitions must agree
across libraries; unsaved edits require a real choice before close/navigation;
document selection/caret offsets must use LO positions; saved HTML and relative
links must come from EDT; failed saves must remain dirty; ordinary browser/mail
paths must still work when the editor is compiled in. Optional Java, scripting,
security and publishing do not become enabled merely by building the editor.
