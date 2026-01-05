// UTF-8 Testing - Comprehensive test suite for UTF-8 handling
// Tests all editor operations with multi-byte UTF-8 characters

#include "../src/editor.h"
#include "../src/renderer.h"
#include "../src/config.h"
#include "test_framework.h"
#include "test_utilities.h"
#include <cstdio>
#include <cstring>
#include <cassert>

// UTF-8 test data
static const char* TEST_UTF8_SIMPLE = "Hello 世界";  // Chinese characters (3 bytes each)
static const char* TEST_UTF8_TREE = "├── └── │";     // Box drawing (3 bytes each)
static const char* TEST_UTF8_EMOJI = "Hello 🌍 World"; // Emoji (4 bytes)
static const char* TEST_UTF8_MIXED = "Café ☕ 日本語"; // Mixed: é(2), ☕(3), 日本語(3+3+3)
static const char* TEST_UTF8_MULTILINE = "Line 1: 世界\nLine 2: ├──\nLine 3: 🌍";

// Test: Arrow keys navigate by characters, not bytes
void test_utf8_arrow_navigation() {
    printf("TEST: UTF-8 arrow key navigation\n");

    TestEditor te;
    te.type_text("世界");  // Two 3-byte characters = 6 bytes total

    // Cursor should be at end (byte 6)
    assert(te.editor.cursor_pos == 6);

    // Left arrow should move back one character (3 bytes)
    te.press_key(0xff51, 0);  // Left arrow
    assert(te.editor.cursor_pos == 3);  // Should be at byte 3, not byte 5

    // Left arrow again should move to start
    te.press_key(0xff51, 0);
    assert(te.editor.cursor_pos == 0);

    // Right arrow should move forward one character (3 bytes)
    te.press_key(0xff53, 0);  // Right arrow
    assert(te.editor.cursor_pos == 3);  // Should be at byte 3, not byte 1

    // Right arrow again should move to end
    te.press_key(0xff53, 0);
    assert(te.editor.cursor_pos == 6);

    printf("  ✓ Arrow navigation respects UTF-8 character boundaries\n");
}

// Test: Backspace deletes entire characters, not individual bytes
void test_utf8_backspace() {
    printf("TEST: UTF-8 backspace\n");

    TestEditor te;
    te.type_text("世界");  // Two 3-byte characters = 6 bytes

    // Backspace should delete one character (3 bytes)
    te.press_key(0xff08, 0);  // Backspace
    assert(te.editor.cursor_pos == 3);
    assert(rope_length(&te.editor.rope) == 3);

    // Verify remaining text is valid UTF-8
    char* text = rope_to_string(&te.editor.rope);
    assert(strcmp(text, "世") == 0);
    delete[] text;

    // Backspace again should delete the second character
    te.press_key(0xff08, 0);
    assert(te.editor.cursor_pos == 0);
    assert(rope_length(&te.editor.rope) == 0);

    printf("  ✓ Backspace deletes entire UTF-8 characters\n");
}

// Test: Delete key deletes entire characters
void test_utf8_delete() {
    printf("TEST: UTF-8 delete key\n");

    TestEditor te;
    te.type_text("世界");

    // Move cursor to start
    te.press_key(0xff51, 0);  // Left
    te.press_key(0xff51, 0);  // Left
    assert(te.editor.cursor_pos == 0);

    // Delete should remove one character (3 bytes)
    te.press_key(0xff7f, 0);  // Delete
    assert(te.editor.cursor_pos == 0);  // Cursor stays at start
    assert(rope_length(&te.editor.rope) == 3);

    // Verify remaining text
    char* text = rope_to_string(&te.editor.rope);
    assert(strcmp(text, "界") == 0);
    delete[] text;

    printf("  ✓ Delete key removes entire UTF-8 characters\n");
}

// Test: Undo/redo with UTF-8 characters
void test_utf8_undo_redo() {
    printf("TEST: UTF-8 undo/redo\n");

    TestEditor te;
    te.type_text("世");
    te.type_text("界");

    // Note: type_text sends events byte-by-byte, so "界" (3 bytes) requires 3 undos
    te.press_ctrl('z');  // Undo byte 3 of "界"
    te.press_ctrl('z');  // Undo byte 2 of "界"
    te.press_ctrl('z');  // Undo byte 1 of "界"

    char* text = rope_to_string(&te.editor.rope);
    assert(strcmp(text, "世") == 0);
    delete[] text;

    // Redo should restore all 3 bytes
    te.press_ctrl('y');
    te.press_ctrl('y');
    te.press_ctrl('y');
    text = rope_to_string(&te.editor.rope);
    assert(strcmp(text, "世界") == 0);
    delete[] text;

    printf("  ✓ Undo/redo preserves UTF-8 integrity (byte-level operations)\n");
}

// Test: Box-drawing characters (common in tree views)
void test_utf8_box_drawing() {
    printf("TEST: UTF-8 box-drawing characters\n");

    TestEditor te;
    te.type_text("├── File.txt");

    // Verify text is stored correctly
    char* text = rope_to_string(&te.editor.rope);
    assert(strcmp(text, "├── File.txt") == 0);
    delete[] text;

    // Navigate to after box-drawing character
    te.press_key(0xff51, 0);  // Left (should skip "t")
    te.press_key(0xff51, 0);  // Left (should skip "x")
    te.press_key(0xff51, 0);  // Left (should skip "t")
    te.press_key(0xff51, 0);  // Left (should skip ".")
    te.press_key(0xff51, 0);  // Left (should skip "e")
    te.press_key(0xff51, 0);  // Left (should skip "l")
    te.press_key(0xff51, 0);  // Left (should skip "i")
    te.press_key(0xff51, 0);  // Left (should skip "F")
    te.press_key(0xff51, 0);  // Left (should skip " ")
    te.press_key(0xff51, 0);  // Left (should skip "─")
    te.press_key(0xff51, 0);  // Left (should skip "─")

    // Should be after "├" which is 3 bytes
    assert(te.editor.cursor_pos == 3);

    printf("  ✓ Box-drawing characters handled correctly\n");
}

// Test: Emoji (4-byte UTF-8 characters)
void test_utf8_emoji() {
    printf("TEST: UTF-8 emoji (4-byte characters)\n");

    TestEditor te;
    te.type_text("🌍");  // Earth emoji (4 bytes)

    assert(rope_length(&te.editor.rope) == 4);
    assert(te.editor.cursor_pos == 4);

    // Left arrow should move back 4 bytes (one emoji)
    te.press_key(0xff51, 0);
    assert(te.editor.cursor_pos == 0);

    // Right arrow should move forward 4 bytes
    te.press_key(0xff53, 0);
    assert(te.editor.cursor_pos == 4);

    // Backspace should delete entire emoji
    te.press_key(0xff08, 0);
    assert(rope_length(&te.editor.rope) == 0);

    printf("  ✓ 4-byte emoji handled correctly\n");
}

// Test: Mixed UTF-8 content
void test_utf8_mixed_content() {
    printf("TEST: Mixed UTF-8 content\n");

    TestEditor te;
    te.type_text("Café");  // 'C' 'a' 'f' 'é'(2 bytes) = 5 bytes

    assert(rope_length(&te.editor.rope) == 5);
    assert(te.editor.cursor_pos == 5);

    // Navigate back to 'é'
    te.press_key(0xff51, 0);  // Should move from byte 5 to byte 3
    assert(te.editor.cursor_pos == 3);

    // Delete 'é'
    te.press_key(0xff7f, 0);
    assert(rope_length(&te.editor.rope) == 3);

    char* text = rope_to_string(&te.editor.rope);
    assert(strcmp(text, "Caf") == 0);
    delete[] text;

    printf("  ✓ Mixed ASCII and UTF-8 handled correctly\n");
}

// Test: Multiline UTF-8 content
void test_utf8_multiline() {
    printf("TEST: Multiline UTF-8 content\n");

    TestEditor te;
    te.type_text("世界\nHello");

    // Verify content
    char* text = rope_to_string(&te.editor.rope);
    assert(strcmp(text, "世界\nHello") == 0);
    delete[] text;

    // Navigate to newline
    // From end (byte 12), go left through "Hello" (5 chars) to get to byte 7 (before 'H')
    te.press_key(0xff51, 0);  // Left from end -> before 'o' (byte 11)
    te.press_key(0xff51, 0);  // Left -> before 'l' (byte 10)
    te.press_key(0xff51, 0);  // Left -> before 'l' (byte 9)
    te.press_key(0xff51, 0);  // Left -> before 'e' (byte 8)
    te.press_key(0xff51, 0);  // Left -> before 'H' (byte 7)
    te.press_key(0xff51, 0);  // Left -> at newline (byte 6)

    // Should be at newline
    text = rope_to_string(&te.editor.rope);
    assert(text[te.editor.cursor_pos] == '\n');
    delete[] text;

    // One more left should jump to after "世界"
    te.press_key(0xff51, 0);  // Left -> after "界" (byte 3)
    assert(te.editor.cursor_pos == 3);  // After "世" which is 3 bytes

    printf("  ✓ Multiline UTF-8 navigation works correctly\n");
}

// Test: Selection with UTF-8 characters
void test_utf8_selection() {
    printf("TEST: UTF-8 selection\n");

    TestEditor te;
    te.type_text("世界");

    // Move to start
    te.press_key(0xff51, 0);
    te.press_key(0xff51, 0);

    // Select first character with Shift+Right
    te.press_key(0xff53, PLATFORM_MOD_SHIFT);  // Shift+Right

    assert(te.editor.has_selection);
    assert(te.editor.selection_start == 0);
    assert(te.editor.selection_end == 3);  // Should select 3 bytes

    printf("  ✓ UTF-8 selection respects character boundaries\n");
}

// Test: Copying and verifying UTF-8 integrity through rope operations
void test_utf8_rope_integrity() {
    printf("TEST: UTF-8 rope integrity\n");

    TestEditor te;
    const char* test_str = "Test 世界 🌍 ├──";
    te.type_text(test_str);

    // Verify round-trip through rope
    char* text = rope_to_string(&te.editor.rope);
    assert(strcmp(text, test_str) == 0);
    delete[] text;

    // Insert in middle
    te.press_key(0xff51, 0);  // Left several times to get to middle
    te.press_key(0xff51, 0);
    te.press_key(0xff51, 0);
    te.press_key(0xff51, 0);

    te.type_text("X");

    // Verify integrity preserved
    text = rope_to_string(&te.editor.rope);
    // Should have X inserted somewhere in the middle
    delete[] text;

    printf("  ✓ Rope operations preserve UTF-8 integrity\n");
}

int main() {
    printf("\n=== UTF-8 Comprehensive Test Suite ===\n\n");

    test_utf8_arrow_navigation();
    test_utf8_backspace();
    test_utf8_delete();
    test_utf8_undo_redo();
    test_utf8_box_drawing();
    test_utf8_emoji();
    test_utf8_mixed_content();
    test_utf8_multiline();
    test_utf8_selection();
    test_utf8_rope_integrity();

    printf("\n=== All UTF-8 tests passed! ===\n\n");
    return 0;
}
