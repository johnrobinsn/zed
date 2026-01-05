# Zed Editor - TODO & Backlog

This file tracks future improvements, bug fixes, and feature ideas for the Zed text editor.

---

## High Priority

### Bugs from Code Review
- [ ] **Search case-insensitivity broken for non-ASCII** (MEDIUM priority from code review)
  - Location: `src/editor.h` - `editor_find_matches()`
  - Issue: Uses ASCII `tolower()` which doesn't handle UTF-8 properly
  - Fix: Implement proper Unicode case folding or use ICU library
  - Impact: Search for "Café" won't match "café" in case-insensitive mode

- [ ] **Undo grouping improvements**
  - Currently: Each character typed creates separate undo entry
  - Desired: Group consecutive typing into single undo operation
  - Impact: Better user experience (undo word at a time, not character at a time)

### Performance
- [ ] **Replace hardcoded layout approximations with real font metrics** (LOW priority from code review)
  - Location: `src/editor.h` - fallback paths using `8.4f` magic number
  - Issue: Inaccurate cursor/selection positioning when layout cache invalid
  - Fix: Always use FreeType metrics, remove approximations
  - Impact: Pixel-perfect positioning even without cache

---

## Medium Priority

### Features
- [ ] **Syntax highlighting**
  - Basic regex-based highlighting for common languages
  - Start with: C/C++, Python, JavaScript, Markdown

- [ ] **Line numbers in gutter**
  - Display line numbers on left side
  - Make configurable (show/hide)

- [ ] **Multiple cursors**
  - Ctrl+Click to add cursor
  - Alt+Shift+Down/Up to add cursor above/below

- [ ] **Find and Replace**
  - Extend current search to support replacement
  - Ctrl+H to open find/replace dialog

- [ ] **Auto-indent**
  - Detect indentation from file (tabs vs spaces, width)
  - Auto-indent new lines based on previous line

### Quality of Life
- [ ] **Proper clipboard handling for large text**
  - Current implementation may have issues with very large clipboard data
  - Add size limits and error handling

- [ ] **Recent files menu**
  - Track recently opened files
  - Quick access via keyboard shortcut

- [ ] **Status bar**
  - Show: line:column, file encoding, file type, selection info
  - Maybe: word count, git status

---

## Low Priority / Nice to Have

### UI/UX Enhancements
- [ ] **Theme system**
  - Load color schemes from config
  - Built-in themes: light, dark, solarized, etc.

- [ ] **Font selection**
  - Allow configuring font family and size in config
  - Hot-reload when config changes

- [ ] **Minimap** (like VSCode/Sublime)
  - Small overview of entire file on right side
  - Click to jump to location

- [ ] **Split panes**
  - Vertical/horizontal splits
  - View same file or different files side-by-side

### Advanced Features
- [ ] **LSP (Language Server Protocol) support**
  - Code completion, go-to-definition, diagnostics
  - Requires significant architecture work

- [ ] **Plugin system**
  - Lua or WASM plugins
  - Allow extending editor functionality

- [ ] **Git integration**
  - Show git diff in gutter
  - Stage/unstage hunks
  - Commit from editor

---

## Testing & Quality

### Test Coverage
- [ ] **Automated clipboard tests with real X11**
  - Current tests use mock clipboard
  - Add integration tests with real X11 clipboard protocol

- [ ] **Fuzzing for rope operations**
  - Fuzz test rope insert/delete for crashes
  - Ensure UTF-8 invariants always hold

- [ ] **Performance benchmarks**
  - Track performance over time
  - Benchmark: large file loading, search, rendering

- [ ] **Visual regression tests**
  - Expand Xvfb integration tests
  - Screenshot comparison for UI changes

### Code Quality
- [ ] **Memory leak detection**
  - Run tests under valgrind
  - Fix any leaks

- [ ] **Static analysis**
  - Run clang-tidy or cppcheck
  - Fix warnings

- [ ] **Code coverage reporting**
  - Integrate gcov output into CI
  - Track coverage percentage over time

---

## Technical Debt

### Refactoring
- [ ] **Layout cache invalidation**
  - Current approach: increment `rope_version` everywhere
  - Better: Event system or dirty flags

- [ ] **Platform abstraction cleanup**
  - Separate X11-specific code more clearly
  - Easier to add Wayland/Windows support later

- [ ] **Renderer batching optimization**
  - Current: Flush rectangles, then flush text
  - Better: Interleave primitives with Z-ordering

### Documentation
- [ ] **Code documentation**
  - Add docstrings to public functions
  - Document struct fields

- [ ] **User manual**
  - Keyboard shortcuts reference
  - Configuration guide
  - Building from source guide

- [ ] **Architecture documentation**
  - Explain rope data structure
  - Explain layout cache system
  - Explain search implementation

---

## Platform Support

- [ ] **Wayland support**
  - Add Wayland backend alongside X11
  - Share common code

- [ ] **Windows support**
  - Add Win32 backend
  - Handle different font rendering

- [ ] **macOS support**
  - Add Cocoa backend
  - Handle macOS-specific keyboard shortcuts (Cmd vs Ctrl)

---

## Configuration

- [ ] **More config options**
  - Tab width
  - Word wrap on/off
  - Show whitespace characters
  - Auto-save interval

- [ ] **Config hot-reload**
  - Watch config file for changes
  - Reload without restarting editor

---

## Ideas / Future Exploration

- [ ] **Modal editing** (Vim-like modes)
  - Normal/Insert/Visual modes
  - Vim keybindings

- [ ] **Tree-sitter integration**
  - Better syntax highlighting
  - Structural editing

- [ ] **Collaborative editing**
  - OT (Operational Transform) or CRDT
  - Multiple users editing same file

- [ ] **Terminal emulator integration**
  - Embedded terminal pane
  - Run build commands without leaving editor

---

## Completed ✓

- [x] UTF-8 support (arrow keys, backspace/delete, selection, rendering)
- [x] Search functionality with live preview
- [x] Undo/redo system
- [x] Copy/paste with X11 clipboard
- [x] Mouse click positioning (including beyond line end)
- [x] Large file loading (fixed rope insertion bug)
- [x] Basic test infrastructure
- [x] Git repository setup

---

**Last Updated:** 2026-01-04
