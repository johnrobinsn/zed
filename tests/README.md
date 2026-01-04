# Zed Editor Test Suite

Comprehensive testing infrastructure with unit tests, integration tests, and visual regression testing.

## Test Suites

### Unit Tests (Fast, No Dependencies)

```bash
make                    # Run all unit tests
make editor_test        # Text editing, cursor, selection, undo/redo
make search_test        # Search functionality
make integration_test   # Multi-step workflows
make file_test          # File I/O operations
```

**Total:** 44 unit tests covering ~65% of codebase

### Integration Tests (Requires Xvfb)

```bash
make integration        # Run Xvfb integration tests
```

**Features:**
- Real X11 protocol testing (headless via Xvfb)
- Actual clipboard testing with SelectionRequest/SelectionNotify
- Screenshot capture for visual regression
- State snapshots for debugging

**Dependencies:**
```bash
sudo apt-get install xvfb scrot imagemagick
```

## Advanced Features

### 1. Editor State Snapshots

Capture complete editor state for debugging and assertions:

```cpp
#include "test_utilities.h"

TestEditor te;
te.type_text("Hello");

// Capture state
EditorSnapshot snap = capture_snapshot(&te.editor);

// Inspect state
printf("Text: %s\n", snap.text.c_str());
printf("Cursor: %zu\n", snap.cursor_pos);
printf("Selection: %s\n", snap.has_selection ? "yes" : "no");

// Save for debugging
snap.save("/tmp/debug_state.txt");

// Compare states
EditorSnapshot snap2 = capture_snapshot(&te.editor);
assert(snap != snap2);
```

**Captured State:**
- Text content
- Cursor position
- Selection state (start, end)
- Search state (query, matches, case-sensitive)
- Rope version
- Undo/redo stack sizes
- File path

### 2. Xvfb Integration Tests

Test with real X11 without GUI:

```cpp
#include "test_utilities.h"

TEST_CASE(test_with_real_x11) {
    // Start virtual X server
    XvfbSession xvfb(99);
    if (!xvfb.start()) return;

    // Create editor with real platform
    IntegrationTestEditor editor(&xvfb);
    if (!editor.is_ready()) return;

    // Test real clipboard protocol
    editor.type("Hello World");
    editor.send_key('a', PLATFORM_MOD_CTRL);  // Select all
    editor.copy();  // Real X11 clipboard

    // Paste in same editor
    editor.paste();

    // Verify
    EditorSnapshot snap = editor.snapshot();
    assert(snap.text == "Hello World");
}
```

### 3. Screenshot Capture

Visual regression testing with screenshots:

```cpp
IntegrationTestEditor editor(&xvfb);
editor.type("Test text");

// Capture screenshot
editor.screenshot("/tmp/test_screenshot.png");

// Compare with baseline
bool match = compare_screenshots(
    "/tmp/test_screenshot.png",
    "golden/expected.png",
    "/tmp/diff.png"  // Visual diff
);
```

**Screenshot Tools:**
- **scrot** - Fast screenshot capture
- **ImageMagick** - Screenshot comparison
- Pixel-perfect comparison with tolerance

### 4. State Diff Debugging

When tests fail, save snapshots for debugging:

```cpp
EditorSnapshot before = capture_snapshot(&editor);

// Perform operation
editor.press_ctrl('z');  // Undo

EditorSnapshot after = capture_snapshot(&editor);

if (before == after) {
    // Save both states for debugging
    before.save("/tmp/before.txt");
    after.save("/tmp/after.txt");

    printf("States are identical! Check:\n");
    printf("  /tmp/before.txt\n");
    printf("  /tmp/after.txt\n");
}
```

## Test Coverage

### Current Coverage: ~65%

```bash
make coverage           # Generate coverage report (requires gcov)
```

| Component | Coverage | Gap |
|-----------|----------|-----|
| Editor Logic | 80% | Context menu, error handling |
| Search | 95% | - |
| Rope | 95% | Edge cases |
| **Platform** | **40%** | **X11 events (now partially tested)** |
| Renderer | 0% | Requires OpenGL context |

### Coverage Gaps

See `test_coverage_analysis.md` for detailed gap analysis.

**Priority gaps:**
1. ❌ Context menu (rendering, interaction)
2. ❌ Renderer (requires OpenGL - possible with Xvfb)
3. ❌ Error handling paths
4. ❌ Large file stress tests (>1MB)

## Test Architecture

### Unit Tests (Mock Platform)
```
Test → TestEditor → Editor Logic
                  → Mock Clipboard (g_test_clipboard)
                  → No Platform/Renderer
```

**Pros:** Fast, no dependencies
**Cons:** Doesn't test platform layer

### Integration Tests (Real Platform)
```
Test → IntegrationTestEditor → Real Platform (Xvfb)
                              → Real X11 Clipboard
                              → Real OpenGL Renderer
```

**Pros:** Tests full stack, catches platform bugs
**Cons:** Slower, requires dependencies

## Best Practices

### When to Use Unit Tests
- Testing editor logic in isolation
- Fast iteration during development
- CI/CD pipelines (fast feedback)

### When to Use Integration Tests
- Testing clipboard protocol
- Visual regression testing
- End-to-end workflows
- Bug reproduction with real platform

### When to Use Snapshots
- Debugging test failures
- Comparing editor state before/after
- Verifying complex state transitions

### When to Use Screenshots
- Visual regression testing
- UI layout changes
- Rendering bugs
- Font rendering issues

## Examples

### Example 1: Test Clipboard Between Apps

```cpp
TEST_CASE(test_clipboard_between_instances) {
    XvfbSession xvfb(99);
    xvfb.start();

    // Editor 1: Copy
    {
        IntegrationTestEditor editor1(&xvfb);
        editor1.type("Shared text");
        editor1.send_key('a', PLATFORM_MOD_CTRL);
        editor1.copy();
    }

    // Editor 2: Paste
    {
        IntegrationTestEditor editor2(&xvfb);
        editor2.paste();

        EditorSnapshot snap = editor2.snapshot();
        assert(snap.text == "Shared text");
    }
}
```

### Example 2: Visual Regression Test

```cpp
TEST_CASE(test_visual_rendering) {
    XvfbSession xvfb(99);
    xvfb.start();

    IntegrationTestEditor editor(&xvfb);
    editor.type("Line 1\nLine 2\nLine 3");

    // Capture screenshot
    editor.screenshot("/tmp/actual.png");

    // Compare with golden image
    bool match = compare_screenshots(
        "/tmp/actual.png",
        "golden/multiline.png",
        "/tmp/diff.png"
    );

    if (!match) {
        printf("Visual regression! Check /tmp/diff.png\n");
    }

    assert(match);
}
```

### Example 3: State Snapshot Debugging

```cpp
TEST_CASE(test_undo_redo_sequence) {
    TestEditor te;

    te.type_text("A");
    EditorSnapshot s1 = capture_snapshot(&te.editor);

    te.type_text("B");
    EditorSnapshot s2 = capture_snapshot(&te.editor);

    te.press_ctrl('z');  // Undo
    EditorSnapshot s3 = capture_snapshot(&te.editor);

    // Verify undo restored to s1
    assert(s3.text == s1.text);
    assert(s3.cursor_pos == s1.cursor_pos);

    // If test fails, save states
    if (s3 != s1) {
        s1.save("/tmp/expected.txt");
        s3.save("/tmp/actual.txt");
        printf("Undo failed! Compare:\n");
        printf("  Expected: /tmp/expected.txt\n");
        printf("  Actual: /tmp/actual.txt\n");
    }
}
```

## Troubleshooting

### Xvfb Not Found
```bash
sudo apt-get install xvfb
```

### Screenshot Tools Not Found
```bash
sudo apt-get install scrot imagemagick
```

### Integration Tests Fail
```bash
# Check if Xvfb is running
ps aux | grep Xvfb

# Kill stale Xvfb processes
pkill -f 'Xvfb :99'

# Run tests with verbose output
DISPLAY=:99 ./integration_xvfb_test
```

### Platform Initialization Fails
- Check X11 libraries: `ldconfig -p | grep libX11`
- Check OpenGL: `ldconfig -p | grep libGL`
- Verify Xvfb display: `DISPLAY=:99 xdpyinfo`

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v2

    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y xvfb scrot imagemagick
        sudo apt-get install -y libx11-dev libgl-dev libglew-dev libfreetype-dev

    - name: Build
      run: make

    - name: Run unit tests
      run: cd tests && make

    - name: Run integration tests
      run: cd tests && make integration

    - name: Upload screenshots
      if: failure()
      uses: actions/upload-artifact@v2
      with:
        name: test-screenshots
        path: /tmp/*.png
```

## Future Improvements

- [ ] Code coverage automation (fix gcov version)
- [ ] Benchmark tests (performance regression)
- [ ] Fuzzing (AFL, libFuzzer)
- [ ] Memory leak detection (Valgrind)
- [ ] Thread sanitizer (TSAN)
- [ ] Address sanitizer (ASAN)
- [ ] Property-based testing
- [ ] Mutation testing

---

**Last Updated:** 2026-01-04
**Test Count:** 44 unit + 6 integration = 50 total
**Coverage:** ~65% overall
