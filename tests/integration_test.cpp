// Integration Tests - End-to-end workflows
// Tests: complex multi-step operations, stress tests

#include "test_framework.h"

// Complete editing workflow
TEST_CASE(test_complete_editing_workflow) {
    TestEditor te;

    // Type initial text
    te.type_text("Line 1");
    te.press_enter();
    te.type_text("Line 2");

    TEST_ASSERT_STR_EQ("Line 1\nLine 2", te.get_text().c_str(), "Multi-line text");

    // Select all and replace
    te.press_ctrl('a');
    te.type_text("X");

    TEST_ASSERT_STR_EQ("X", te.get_text().c_str(), "Replaced all");
}

// Search + Navigate workflow
TEST_CASE(test_search_modify_workflow) {
    TestEditor te;

    // Create content
    te.type_text("foo bar foo baz foo");

    // Search for "foo"
    te.open_search();
    te.type_text("foo");
    TEST_ASSERT_EQ(3, te.get_search_matches(), "3 foo matches");

    // Navigate to second match
    te.press_enter();
    TEST_ASSERT_EQ(8, te.get_cursor(), "Second match");

    // Navigate to third match
    te.press_enter();
    TEST_ASSERT_EQ(16, te.get_cursor(), "Third match");
}

// Multiple undo/redo operations
TEST_CASE(test_complex_undo_redo) {
    TestEditor te;

    te.type_text("A");
    te.type_text("B");
    te.type_text("C");
    te.type_text("D");

    // Undo all
    te.press_ctrl('z');
    te.press_ctrl('z');
    te.press_ctrl('z');
    te.press_ctrl('z');

    TEST_ASSERT_STR_EQ("", te.get_text().c_str(), "All undone");

    // Redo some
    te.press_ctrl('y');
    te.press_ctrl('y');

    TEST_ASSERT_STR_EQ("AB", te.get_text().c_str(), "Partial redo");

    // Add new content (should clear redo stack)
    te.type_text("X");

    TEST_ASSERT_STR_EQ("ABX", te.get_text().c_str(), "New content added");
}

// Stress test: large text
TEST_CASE(test_large_text) {
    TestEditor te;

    // Add 100 lines
    for (int i = 0; i < 100; i++) {
        te.type_text("Line ");
        // Type line number (simple conversion)
        char num[10];
        snprintf(num, sizeof(num), "%d", i);
        te.type_text(num);
        if (i < 99) {
            te.press_enter();
        }
    }

    size_t length = te.get_text_length();
    TEST_ASSERT(length > 500, "Large text created");

    // Search should work
    te.open_search();
    te.type_text("Line");

    TEST_ASSERT_EQ(100, te.get_search_matches(), "Found all lines");
}

// Stress test: many operations
TEST_CASE(test_many_operations) {
    TestEditor te;

    // Perform 100 insertions and deletions
    for (int i = 0; i < 100; i++) {
        te.type_text("X");
        te.press_backspace();
    }

    TEST_ASSERT_STR_EQ("", te.get_text().c_str(), "All ops canceled out");

    // Now add content
    te.type_text("Final");
    TEST_ASSERT_STR_EQ("Final", te.get_text().c_str(), "Final content");
}

// Selection workflow
TEST_CASE(test_selection_delete_workflow) {
    TestEditor te;

    te.type_text("Hello World");

    // Select all and replace
    te.press_ctrl('a');
    TEST_ASSERT(te.has_selection(), "Should have selection");

    te.type_text("X");
    TEST_ASSERT_STR_EQ("X", te.get_text().c_str(), "Selection replaced");
}

// Main function
int main() {
    return run_all_tests();
}
