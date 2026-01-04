// Search Tests - Test search functionality thoroughly
// Tests: open/close, find matches, navigation, case sensitivity, bug fixes

#include "test_framework.h"

// Basic search open/close
TEST_CASE(test_search_open_close) {
    TestEditor te;

    te.type_text("Hello World");
    te.open_search();

    TEST_ASSERT(te.search_is_active(), "Search should be active");

    te.close_search();
    TEST_ASSERT(!te.search_is_active(), "Search should be closed");
}

// Find single match
TEST_CASE(test_search_single_match) {
    TestEditor te;

    te.type_text("Hello World");
    te.open_search();
    te.type_text("World");

    TEST_ASSERT_EQ(1, te.get_search_matches(), "Should find 1 match");
    TEST_ASSERT_STR_EQ("World", te.get_search_query().c_str(), "Query stored");
    TEST_ASSERT_EQ(6, te.get_cursor(), "Cursor moved to match");
}

// Find multiple matches (case-insensitive)
TEST_CASE(test_search_multiple_matches) {
    TestEditor te;

    te.type_text("Test test TEST tEsT");
    te.open_search();
    te.type_text("test");

    TEST_ASSERT_EQ(4, te.get_search_matches(), "Should find 4 matches (case-insensitive)");
    TEST_ASSERT_STR_EQ("test", te.get_search_query().c_str(), "Query stored");
}

// No matches found
TEST_CASE(test_search_no_matches) {
    TestEditor te;

    te.type_text("Hello World");
    te.open_search();
    te.type_text("xyz");

    TEST_ASSERT_EQ(0, te.get_search_matches(), "Should find 0 matches");
}

// Navigate to next match
TEST_CASE(test_search_navigation_next) {
    TestEditor te;

    te.type_text("foo bar foo baz foo");
    te.open_search();
    te.type_text("foo");

    TEST_ASSERT_EQ(3, te.get_search_matches(), "Should find 3 matches");
    TEST_ASSERT_EQ(0, te.get_cursor(), "First match at position 0");

    te.press_enter();  // Next match
    TEST_ASSERT_EQ(8, te.get_cursor(), "Second match at position 8");

    te.press_enter();  // Next match
    TEST_ASSERT_EQ(16, te.get_cursor(), "Third match at position 16");

    te.press_enter();  // Wrap around
    TEST_ASSERT_EQ(0, te.get_cursor(), "Wrapped to first match");
}

// Navigate to previous match
TEST_CASE(test_search_navigation_prev) {
    TestEditor te;

    te.type_text("foo bar foo baz foo");
    te.open_search();
    te.type_text("foo");

    TEST_ASSERT_EQ(0, te.get_cursor(), "Start at first match");

    te.press_shift(0xff0d);  // Shift+Enter - previous match
    TEST_ASSERT_EQ(16, te.get_cursor(), "Wrapped to last match");

    te.press_shift(0xff0d);  // Shift+Enter - previous match
    TEST_ASSERT_EQ(8, te.get_cursor(), "Second match");
}

// Ctrl+G navigation
TEST_CASE(test_search_ctrl_g) {
    TestEditor te;

    te.type_text("test test test");
    te.open_search();
    te.type_text("test");

    te.press_ctrl('g');  // Find next
    TEST_ASSERT_EQ(5, te.get_cursor(), "Second match");

    te.press_ctrl('g');  // Find next
    TEST_ASSERT_EQ(10, te.get_cursor(), "Third match");
}

// Case-sensitive toggle (Ctrl+Alt+C)
TEST_CASE(test_search_case_sensitive) {
    TestEditor te;

    te.type_text("Test test TEST");
    te.open_search();
    te.type_text("test");

    TEST_ASSERT_EQ(3, te.get_search_matches(), "3 matches (case-insensitive)");
    TEST_ASSERT(!te.get_search_case_sensitive(), "Case-insensitive by default");

    // Toggle case-sensitive
    te.press_key('c', PLATFORM_MOD_CTRL | PLATFORM_MOD_ALT);

    TEST_ASSERT(te.get_search_case_sensitive(), "Case-sensitive after toggle");
    TEST_ASSERT_EQ(1, te.get_search_matches(), "1 match (case-sensitive)");
}

// Query editing with backspace
TEST_CASE(test_search_query_editing) {
    TestEditor te;

    te.type_text("Hello World");
    te.open_search();
    te.type_text("Wor");

    TEST_ASSERT_EQ(1, te.get_search_matches(), "Partial match");

    te.type_text("l");
    TEST_ASSERT_STR_EQ("Worl", te.get_search_query().c_str(), "Query updated");
    TEST_ASSERT_EQ(1, te.get_search_matches(), "Still 1 match");

    te.press_backspace();
    TEST_ASSERT_STR_EQ("Wor", te.get_search_query().c_str(), "Backspace removed char");
}

// BUG FIX: Backspace on empty query should not crash or add character
TEST_CASE(test_search_backspace_on_empty) {
    TestEditor te;

    te.type_text("Hello World");
    te.open_search();

    // Backspace on empty query
    te.press_backspace();

    TEST_ASSERT_EQ(0, te.get_search_query().length(), "Query should be empty");
    TEST_ASSERT_STR_EQ("Hello World", te.get_text().c_str(), "Document unchanged");
}

// BUG FIX: Query longer than text should not underflow
TEST_CASE(test_search_query_longer_than_text) {
    TestEditor te;

    te.type_text("Hi");
    te.open_search();
    te.type_text("Hello");

    // Should not crash or underflow - query is longer than document
    TEST_ASSERT_EQ(0, te.get_search_matches(), "Should find 0 matches");
    // Query should be stored (even though we can't search for it)
    TEST_ASSERT(te.get_search_query().length() == 5, "Query length should be 5");
}

// Live update: search results update when document changes
TEST_CASE(test_search_live_update) {
    TestEditor te;

    te.type_text("test test");
    te.open_search();
    te.type_text("test");

    TEST_ASSERT_EQ(2, te.get_search_matches(), "2 matches initially");

    // Close search and add more text
    te.close_search();
    te.type_text(" test");

    // Reopen search - query should be preserved
    te.open_search();

    // Update should happen automatically
    te.update();  // Force update
    TEST_ASSERT_EQ(3, te.get_search_matches(), "3 matches after adding text");
}

// Empty document search
TEST_CASE(test_search_empty_document) {
    TestEditor te;

    te.open_search();
    te.type_text("test");

    TEST_ASSERT_EQ(0, te.get_search_matches(), "No matches in empty document");
}

// Single character search
TEST_CASE(test_search_single_char) {
    TestEditor te;

    te.type_text("a b c a d a");
    te.open_search();
    te.type_text("a");

    TEST_ASSERT_EQ(3, te.get_search_matches(), "Should find 3 'a' characters");
}

// Search with special characters
TEST_CASE(test_search_special_chars) {
    TestEditor te;

    te.type_text("Hello, World! Test: 123");
    te.open_search();
    te.type_text("!");

    TEST_ASSERT_EQ(1, te.get_search_matches(), "Should find exclamation mark");
}

// Search preserves query when reopened
TEST_CASE(test_search_preserves_query) {
    TestEditor te;

    te.type_text("Hello World");
    te.open_search();
    te.type_text("World");

    te.close_search();

    te.open_search();
    TEST_ASSERT_STR_EQ("World", te.get_search_query().c_str(), "Query preserved");
}

// Main function
int main() {
    return run_all_tests();
}
