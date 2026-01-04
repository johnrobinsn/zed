// Editor Tests - Test core editor functionality
// Tests: text insertion/deletion, cursor movement, selection, undo/redo

#include "test_framework.h"

// Basic text insertion
TEST_CASE(test_basic_text_insertion) {
    TestEditor te;

    te.type_text("Hello, World!");

    TEST_ASSERT_EQ(13, te.get_text_length(), "Text length should be 13");
    TEST_ASSERT_STR_EQ("Hello, World!", te.get_text().c_str(), "Text content");
    TEST_ASSERT_EQ(13, te.get_cursor(), "Cursor should be at end");
}

// Empty editor
TEST_CASE(test_empty_editor) {
    TestEditor te;

    TEST_ASSERT_EQ(0, te.get_text_length(), "Empty editor should have length 0");
    TEST_ASSERT_STR_EQ("", te.get_text().c_str(), "Empty editor should have empty text");
    TEST_ASSERT_EQ(0, te.get_cursor(), "Cursor should be at position 0");
}

// Backspace deletion
TEST_CASE(test_backspace) {
    TestEditor te;

    te.type_text("Hello");
    te.press_backspace();

    TEST_ASSERT_EQ(4, te.get_text_length(), "Length after backspace");
    TEST_ASSERT_STR_EQ("Hell", te.get_text().c_str(), "Text after backspace");
    TEST_ASSERT_EQ(4, te.get_cursor(), "Cursor after backspace");
}

// Multiple backspaces
TEST_CASE(test_multiple_backspaces) {
    TestEditor te;

    te.type_text("Test");
    te.press_backspace();
    te.press_backspace();

    TEST_ASSERT_STR_EQ("Te", te.get_text().c_str(), "Text after 2 backspaces");
    TEST_ASSERT_EQ(2, te.get_cursor(), "Cursor position");
}

// Backspace on empty (should not crash)
TEST_CASE(test_backspace_empty) {
    TestEditor te;

    te.press_backspace();

    TEST_ASSERT_EQ(0, te.get_text_length(), "Empty after backspace on empty");
    TEST_ASSERT_EQ(0, te.get_cursor(), "Cursor at 0");
}

// Newline insertion
TEST_CASE(test_newline) {
    TestEditor te;

    te.type_text("Line 1");
    te.press_enter();
    te.type_text("Line 2");

    TEST_ASSERT_STR_EQ("Line 1\nLine 2", te.get_text().c_str(), "Multi-line text");
    TEST_ASSERT_EQ(13, te.get_cursor(), "Cursor at end of line 2");
}

// Undo single operation
TEST_CASE(test_undo_single) {
    TestEditor te;

    te.type_text("A");
    te.press_ctrl('z');  // Undo

    TEST_ASSERT_STR_EQ("", te.get_text().c_str(), "Text after undo");
    TEST_ASSERT_EQ(0, te.get_cursor(), "Cursor after undo");
}

// Undo/redo cycle
TEST_CASE(test_undo_redo) {
    TestEditor te;

    te.type_text("A");
    te.press_ctrl('z');  // Undo
    TEST_ASSERT_STR_EQ("", te.get_text().c_str(), "After undo");

    te.press_ctrl('y');  // Redo
    TEST_ASSERT_STR_EQ("A", te.get_text().c_str(), "After redo");
    TEST_ASSERT_EQ(1, te.get_cursor(), "Cursor after redo");
}

// Multiple undos
TEST_CASE(test_multiple_undos) {
    TestEditor te;

    te.type_text("A");
    te.type_text("B");
    te.type_text("C");

    te.press_ctrl('z');  // Undo C
    TEST_ASSERT_STR_EQ("AB", te.get_text().c_str(), "After 1 undo");

    te.press_ctrl('z');  // Undo B
    TEST_ASSERT_STR_EQ("A", te.get_text().c_str(), "After 2 undos");

    te.press_ctrl('z');  // Undo A
    TEST_ASSERT_STR_EQ("", te.get_text().c_str(), "After 3 undos");
}

// Cursor left/right
TEST_CASE(test_cursor_movement_arrows) {
    TestEditor te;

    te.type_text("Hello");
    TEST_ASSERT_EQ(5, te.get_cursor(), "Cursor at end initially");

    te.press_key(0xff51, 0);  // Left arrow
    TEST_ASSERT_EQ(4, te.get_cursor(), "Cursor moved left");

    te.press_key(0xff51, 0);  // Left arrow again
    TEST_ASSERT_EQ(3, te.get_cursor(), "Cursor moved left again");

    te.press_key(0xff53, 0);  // Right arrow
    TEST_ASSERT_EQ(4, te.get_cursor(), "Cursor moved right");
}

// Home/End keys
TEST_CASE(test_cursor_home_end) {
    TestEditor te;

    te.type_text("Hello World");
    TEST_ASSERT_EQ(11, te.get_cursor(), "Cursor at end");

    te.press_key(0xff50, 0);  // Home
    TEST_ASSERT_EQ(0, te.get_cursor(), "Cursor at start after Home");

    te.press_key(0xff57, 0);  // End
    TEST_ASSERT_EQ(11, te.get_cursor(), "Cursor at end after End");
}

// Selection with shift+arrows
TEST_CASE(test_selection_shift_arrows) {
    TestEditor te;

    te.type_text("Hello");
    te.press_key(0xff50, 0);  // Home - cursor to start

    te.press_shift(0xff53);  // Shift+Right
    TEST_ASSERT(te.has_selection(), "Should have selection");
    TEST_ASSERT_EQ(0, te.get_selection_start(), "Selection start");
    TEST_ASSERT_EQ(1, te.get_selection_end(), "Selection end");

    te.press_shift(0xff53);  // Shift+Right again
    TEST_ASSERT_EQ(2, te.get_selection_end(), "Selection extended");
}

// Select all
TEST_CASE(test_select_all) {
    TestEditor te;

    te.type_text("Hello World");
    te.press_ctrl('a');  // Select all

    TEST_ASSERT(te.has_selection(), "Should have selection");
    TEST_ASSERT_EQ(0, te.get_selection_start(), "Selection starts at 0");
    TEST_ASSERT_EQ(11, te.get_selection_end(), "Selection ends at text length");
}

// Typing replaces selection
TEST_CASE(test_type_replaces_selection) {
    TestEditor te;

    te.type_text("Hello World");
    te.press_ctrl('a');  // Select all
    te.type_text("X");

    TEST_ASSERT_STR_EQ("X", te.get_text().c_str(), "Selection replaced with X");
    TEST_ASSERT_EQ(1, te.get_cursor(), "Cursor after selection");
    TEST_ASSERT(!te.has_selection(), "Selection cleared");
}

// Copy and paste
TEST_CASE(test_copy_paste) {
    TestEditor te;

    te.type_text("Hello World");

    // Select "Hello"
    te.press_key(0xff50, 0);  // Home
    for (int i = 0; i < 5; i++) {
        te.press_shift(0xff53);  // Shift+Right
    }

    TEST_ASSERT(te.has_selection(), "Should have selection");

    // Copy with Ctrl+C
    te.press_ctrl('c');

    // Move to end
    te.press_key(0xff57, 0);  // End
    te.type_text(" ");

    // Paste with Ctrl+V
    te.press_ctrl('v');

    std::string result = te.get_text();
    printf("[TEST] Result text: '%s' (len=%zu)\n", result.c_str(), result.length());
    TEST_ASSERT_STR_EQ("Hello World Hello", result.c_str(), "Pasted text");
}

// Paste rope version update
TEST_CASE(test_paste_invalidates_cache) {
    TestEditor te;

    te.type_text("Hello World");

    // Select "Hello"
    te.press_key(0xff50, 0);  // Home
    for (int i = 0; i < 5; i++) {
        te.press_shift(0xff53);  // Shift+Right
    }

    // Copy
    te.press_ctrl('c');

    // Move to end
    te.press_key(0xff57, 0);  // End
    te.type_text(" ");

    // Get version before paste
    size_t version_before = te.get_rope_version();
    printf("[TEST] Rope version before paste: %zu\n", version_before);

    // Paste - should increment rope_version
    te.press_ctrl('v');

    size_t version_after = te.get_rope_version();
    printf("[TEST] Rope version after paste: %zu\n", version_after);
    printf("[TEST] Cache is stale: %s\n", te.cache_is_stale() ? "YES" : "NO");

    TEST_ASSERT(version_after > version_before, "Rope version should increment after paste");
    TEST_ASSERT(te.cache_is_stale(), "Cache should be stale after paste");

    // Verify text is correct
    std::string result = te.get_text();
    TEST_ASSERT_STR_EQ("Hello World Hello", result.c_str(), "Pasted text");
}

// Mouse click beyond line end
TEST_CASE(test_mouse_click_beyond_line_end) {
    TestEditor te;

    // Create multi-line text with different lengths
    te.type_text("Short");
    te.press_enter();
    te.type_text("This is a longer line");
    te.press_enter();
    te.type_text("Mid");

    // Simulate clicking far to the right of the first line "Short"
    // Line 0 is at y=40, line height is 16
    // "Short" is 5 chars, with fallback width of 8.4px each = 42px
    // Line 0 ends at x=20+42=62, we'll click at x=200
    PlatformEvent click_event;
    click_event.type = PLATFORM_EVENT_MOUSE_BUTTON;
    click_event.mouse_button.button = 1;
    click_event.mouse_button.pressed = true;
    click_event.mouse_button.x = 200;  // Far beyond "Short" (ends ~62px)
    click_event.mouse_button.y = 45;   // On first line (40 to 56)

    editor_handle_event(&te.editor, &click_event, nullptr, nullptr);

    // Cursor should be at position 5 (at the newline after "Short")
    // Text positions: S(0)h(1)o(2)r(3)t(4)\n(5)T(6)h(7)i(8)s...
    TEST_ASSERT_EQ(5, te.get_cursor(), "Cursor should be at end of first line");

    // Now click far to the right of the third line "Mid"
    // Line 2 is at y=40+32=72 (line_height=16)
    click_event.mouse_button.x = 300;  // Far beyond "Mid"
    click_event.mouse_button.y = 77;   // On third line (72 to 88)

    editor_handle_event(&te.editor, &click_event, nullptr, nullptr);

    // Should be at end of "Mid" which is position: 6("Short\n") + 23("This is a longer line\n") + 3("Mid") = 32
    // Actually: "Short\n" = 6, "This is a longer line\n" = 23, "Mid" = 3, total = 32
    size_t cursor = te.get_cursor();
    std::string text = te.get_text();
    printf("[TEST] Text: '%s' (len=%zu)\n", text.c_str(), text.length());
    printf("[TEST] Cursor after click beyond line 3: %zu\n", cursor);

    // The cursor should be at the end of the file (32 chars total)
    TEST_ASSERT_EQ(text.length(), cursor, "Cursor should be at end of third line");

    printf("[TEST] Mouse click beyond line end test passed\n");
}

// Main function
int main() {
    return run_all_tests();
}
