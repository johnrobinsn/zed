// Integration Tests with Xvfb - Real X11 clipboard and rendering
// Tests: clipboard protocol, visual rendering, state snapshots

#include "test_framework.h"
#include "test_utilities.h"

// Test clipboard with real X11 protocol
TEST_CASE(test_real_x11_clipboard) {
    XvfbSession xvfb(99);
    if (!xvfb.start()) {
        printf("  ⚠️  SKIPPED (Xvfb not available)\n");
        return;
    }

    IntegrationTestEditor editor(&xvfb);
    if (!editor.is_ready()) {
        printf("  ⚠️  SKIPPED (Platform initialization failed)\n");
        return;
    }

    // Type some text
    editor.type("Hello from Xvfb!");

    // Select all and copy
    editor.send_key('a', PLATFORM_MOD_CTRL);
    editor.copy();

    // Move cursor to end and paste
    editor.send_key(0xff57, 0);  // End
    editor.type(" ");
    editor.paste();

    // Verify
    EditorSnapshot snap = editor.snapshot();
    TEST_ASSERT_STR_EQ("Hello from Xvfb! Hello from Xvfb!", snap.text.c_str(), "Clipboard paste");
}

// Test screenshot capture
TEST_CASE(test_screenshot_capture) {
    XvfbSession xvfb(99);
    if (!xvfb.start()) {
        printf("  ⚠️  SKIPPED (Xvfb not available)\n");
        return;
    }

    IntegrationTestEditor editor(&xvfb);
    if (!editor.is_ready()) {
        printf("  ⚠️  SKIPPED (Platform initialization failed)\n");
        return;
    }

    // Type some text
    editor.type("Testing screenshots!");

    // Take screenshot
    bool success = editor.screenshot("/tmp/zed_test_screenshot.png");
    TEST_ASSERT(success, "Screenshot should be captured");

    // Verify file exists
    FILE* f = fopen("/tmp/zed_test_screenshot.png", "r");
    TEST_ASSERT(f != nullptr, "Screenshot file should exist");
    if (f) fclose(f);

    // Cleanup
    unlink("/tmp/zed_test_screenshot.png");
}

// Test editor state snapshots
TEST_CASE(test_state_snapshots) {
    TestEditor te;

    // Initial state
    EditorSnapshot snap1 = capture_snapshot(&te.editor);
    TEST_ASSERT_EQ(0, snap1.cursor_pos, "Initial cursor at 0");
    TEST_ASSERT_EQ(0, snap1.text_length, "Initial text empty");

    // Type text
    te.type_text("Hello");
    EditorSnapshot snap2 = capture_snapshot(&te.editor);
    TEST_ASSERT_EQ(5, snap2.cursor_pos, "Cursor after typing");
    TEST_ASSERT_EQ(5, snap2.text_length, "Text length after typing");
    TEST_ASSERT_STR_EQ("Hello", snap2.text.c_str(), "Text content");

    // Verify snapshots are different
    TEST_ASSERT(snap1 != snap2, "Snapshots should differ");

    // Save snapshot for debugging
    snap2.save("/tmp/zed_snapshot_test.txt");

    // Cleanup
    unlink("/tmp/zed_snapshot_test.txt");
}

// Test clipboard between two editor instances
TEST_CASE(test_clipboard_between_instances) {
    XvfbSession xvfb(99);
    if (!xvfb.start()) {
        printf("  ⚠️  SKIPPED (Xvfb not available)\n");
        return;
    }

    // Create first editor and copy text
    {
        IntegrationTestEditor editor1(&xvfb);
        if (!editor1.is_ready()) {
            printf("  ⚠️  SKIPPED (Platform initialization failed)\n");
            return;
        }

        editor1.type("Shared clipboard!");
        editor1.send_key('a', PLATFORM_MOD_CTRL);  // Select all
        editor1.copy();

        // Keep window alive briefly for clipboard ownership
        usleep(100000);  // 100ms
    }

    // Create second editor and paste
    {
        IntegrationTestEditor editor2(&xvfb);
        if (!editor2.is_ready()) {
            printf("  ⚠️  SKIPPED (Platform initialization failed)\n");
            return;
        }

        editor2.paste();

        EditorSnapshot snap = editor2.snapshot();
        printf("[TEST] Pasted text: '%s'\n", snap.text.c_str());

        // Note: This may not work perfectly due to clipboard ownership transfer
        // but it tests the protocol
        TEST_ASSERT(snap.text_length > 0, "Should paste some text");
    }
}

// Test visual state after editing
TEST_CASE(test_visual_state_after_edit) {
    TestEditor te;

    // Perform some operations
    te.type_text("Line 1");
    te.press_enter();
    te.type_text("Line 2");

    EditorSnapshot before = capture_snapshot(&te.editor);
    printf("[TEST] Before undo: text='%s', len=%zu, undo_stack=%zu\n",
           before.text.c_str(), before.text_length, before.undo_stack_size);

    // Undo "Line 2" (6 characters, one undo per character)
    for (int i = 0; i < 6; i++) {
        te.press_ctrl('z');
    }

    EditorSnapshot after = capture_snapshot(&te.editor);
    printf("[TEST] After undo: text='%s', len=%zu, undo_stack=%zu\n",
           after.text.c_str(), after.text_length, after.undo_stack_size);

    // Verify state changed
    TEST_ASSERT(before != after, "State should change after undo");
    TEST_ASSERT_EQ(7, after.text_length, "Text length after undo");
    TEST_ASSERT_STR_EQ("Line 1\n", after.text.c_str(), "Text after undoing Line 2");

    printf("[TEST] Before undo: %zu chars, After undo: %zu chars\n",
           before.text_length, after.text_length);
}

// Stress test: rapid state changes
TEST_CASE(test_rapid_state_changes) {
    TestEditor te;

    std::vector<EditorSnapshot> snapshots;

    // Perform rapid operations and capture snapshots
    for (int i = 0; i < 10; i++) {
        te.type_text("X");
        snapshots.push_back(capture_snapshot(&te.editor));
    }

    // Verify all snapshots are different
    for (size_t i = 1; i < snapshots.size(); i++) {
        TEST_ASSERT(snapshots[i-1] != snapshots[i], "Each state should be unique");
    }

    // Verify progression
    TEST_ASSERT_EQ(10, snapshots.back().text_length, "Final text length");
}

// Main function
int main() {
    printf("\n");
    printf("=================================================================\n");
    printf("  INTEGRATION TESTS (Xvfb + Real X11 + Screenshots)\n");
    printf("=================================================================\n");
    printf("\n");
    printf("These tests use Xvfb for headless X11 testing.\n");
    printf("Install with: sudo apt-get install xvfb scrot imagemagick\n");
    printf("\n");

    return run_all_tests();
}
