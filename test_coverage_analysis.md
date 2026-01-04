# Test Coverage Gap Analysis - Zed Editor

## Summary
The automated tests use **mock implementations** that bypass real platform code, leading to untested code paths.

## Critical Gap: X11 Clipboard

### What's Tested
- ✅ Mock clipboard (`g_test_clipboard`) - simple buffer copy/paste
- ✅ Editor clipboard operations (`editor_copy`, `editor_cut`, `editor_paste`)
- ✅ Rope text manipulation during copy/paste

### What's NOT Tested
- ❌ `platform_set_clipboard()` X11 code path
- ❌ `platform_get_clipboard()` X11 code path  
- ❌ **SelectionRequest event handler** (never executed!)
- ❌ X11 selection ownership
- ❌ Clipboard timeout handling
- ❌ Multiple clipboard targets (UTF8_STRING, XA_STRING)

### Why the Bug Wasn't Caught
```cpp
// Tests use this path:
if (!platform || !platform->display) {
    strncpy(g_test_clipboard, text, ...);  // ✅ Tested
    return;
}

// Real editor uses this path:
XSetSelectionOwner(...);  // ❌ Never tested!
XChangeProperty(...);      // ❌ Never tested!
```

The SelectionRequest handler was **completely untested** because tests pass `nullptr` for platform.

## Other Coverage Gaps

### Platform Layer (Estimated 30% coverage)
**Tested:**
- ✅ Basic event structure creation (in test framework)
- ✅ Mock clipboard

**NOT Tested:**
- ❌ `platform_init()` - window creation
- ❌ `platform_shutdown()` - cleanup
- ❌ `platform_poll_event()` - X11 event loop
  - ❌ Mouse wheel events (ButtonPress 4/5)
  - ❌ ConfigureNotify (window resize)
  - ❌ **SelectionRequest** (clipboard)  
  - ❌ MotionNotify edge cases
- ❌ `platform_set_cursor()` - cursor management
- ❌ VSync/swap control functions
- ❌ Error handling (XOpenDisplay failure, etc.)

### Editor Layer (Estimated 75% coverage)
**Tested:**
- ✅ Text insertion/deletion
- ✅ Cursor movement
- ✅ Selection
- ✅ Undo/redo
- ✅ Search (comprehensive)
- ✅ Copy/paste (logic only, not X11)

**NOT Tested:**
- ❌ File operations (`editor_open_file`, `editor_save_file`)
- ❌ Context menu rendering
- ❌ Context menu interaction
- ❌ Mouse-to-position conversion
- ❌ Scrolling beyond viewport
- ❌ Very large files (> 10MB)
- ❌ Unicode/multi-byte characters
- ❌ Line wrapping edge cases
- ❌ Error paths (OOM, file I/O errors)

### Renderer (Estimated 0% coverage)
**NOT Tested:**
- ❌ All renderer functions (requires OpenGL context)
- ❌ Font loading
- ❌ Text rendering
- ❌ Glyph caching
- ❌ Cursor rendering
- ❌ Selection highlighting rendering

### Rope (Estimated 90% coverage)
**Tested:**
- ✅ Basic operations (insert, delete, substr)
- ✅ Large text handling

**NOT Tested:**
- ❌ Edge cases (very long lines, many small chunks)
- ❌ Memory exhaustion scenarios
- ❌ Pathological input (deeply nested rope structure)

## Recommendations

### Priority 1: X11 Integration Tests
Create tests that use real X11 display:
- Set `DISPLAY=:99` and use Xvfb (virtual framebuffer)
- Test actual X11 clipboard protocol
- Test SelectionRequest/SelectionNotify flow
- Verify clipboard works between two editor instances

### Priority 2: File I/O Tests
```cpp
TEST_CASE(test_file_save_load) {
    TestEditor te;
    te.type_text("Hello World");
    editor_save_file(&te.editor, "/tmp/test.txt");
    
    TestEditor te2;
    editor_open_file(&te2.editor, "/tmp/test.txt");
    TEST_ASSERT_STR_EQ("Hello World", te2.get_text().c_str());
}
```

### Priority 3: Context Menu Tests
Test right-click menu functionality:
- Menu opening/closing
- Item selection
- Cut/Copy/Paste/Select All from menu

### Priority 4: Property-Based Testing
Use random input generation to test:
- Random edit sequences
- Random cursor movements
- Undo/redo with random operations

### Priority 5: Coverage Tooling
- Fix gcov version mismatch issues
- Add automated coverage reporting to CI
- Set coverage thresholds (e.g., 80% minimum)
- Track coverage trends over time

## Test Architecture Improvements

### Current Architecture Problem
```
Tests → TestEditor (platform=nullptr) → Mock Everything
                                      → X11 Code Never Runs
```

### Proposed Architecture
```
Tests → Integration Tests → Real Platform (Xvfb)
     → Unit Tests → Mock Platform → Test Editor Logic Only
```

**Unit Tests** (current approach):
- Fast, no dependencies
- Test editor logic in isolation
- Use mocks for platform layer

**Integration Tests** (new):
- Test full stack with real X11
- Slower, requires Xvfb
- Catch platform-specific bugs

## Metrics

| Component | Estimated Coverage | Critical Gaps |
|-----------|-------------------|---------------|
| Editor Logic | 75% | File I/O, Context Menu |
| Search | 95% | - |
| Rope | 90% | Edge cases |
| Platform | 30% | X11 events, clipboard |
| Renderer | 0% | Everything |
| **Overall** | **~55%** | Platform & Rendering |

## Action Items

1. ✅ Add coverage analysis tooling (Makefile targets)
2. ⏳ Fix gcov version mismatch
3. ⏳ Create X11 integration test suite with Xvfb
4. ⏳ Add file I/O tests
5. ⏳ Add context menu tests
6. ⏳ Set up CI with coverage reporting
7. ⏳ Add coverage badge to README

---

**Last Updated:** 2026-01-04  
**Generated by:** `analyze_coverage.py`
