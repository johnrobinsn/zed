// File I/O Tests - Test file operations
// Tests: open, save, error handling

#include "test_framework.h"
#include <unistd.h>

// Save and load file
TEST_CASE(test_file_save_load) {
    TestEditor te;

    te.type_text("Hello, World!\nThis is a test file.");

    // Save to temp file
    const char* temp_file = "/tmp/zed_test_file.txt";
    bool save_result = editor_save_file(&te.editor, temp_file);
    TEST_ASSERT(save_result, "File should save successfully");

    // Load in new editor
    TestEditor te2;
    bool load_result = editor_open_file(&te2.editor, temp_file);
    TEST_ASSERT(load_result, "File should load successfully");

    // Verify content
    std::string loaded = te2.get_text();
    TEST_ASSERT_STR_EQ("Hello, World!\nThis is a test file.", loaded.c_str(), "Loaded content should match");

    // Cleanup
    unlink(temp_file);
}

// Save empty file
TEST_CASE(test_file_save_empty) {
    TestEditor te;

    const char* temp_file = "/tmp/zed_test_empty.txt";
    bool save_result = editor_save_file(&te.editor, temp_file);
    TEST_ASSERT(save_result, "Empty file should save successfully");

    // Load and verify
    TestEditor te2;
    editor_open_file(&te2.editor, temp_file);
    TEST_ASSERT_EQ(0, te2.get_text_length(), "Empty file should load as empty");

    unlink(temp_file);
}

// Load non-existent file
TEST_CASE(test_file_open_nonexistent) {
    TestEditor te;

    bool result = editor_open_file(&te.editor, "/tmp/nonexistent_file_12345.txt");
    TEST_ASSERT(!result, "Opening non-existent file should fail");

    // Editor should remain empty
    TEST_ASSERT_EQ(0, te.get_text_length(), "Editor should be empty after failed load");
}

// Save with special characters
TEST_CASE(test_file_special_characters) {
    TestEditor te;

    te.type_text("Special: !@#$%^&*()");

    const char* temp_file = "/tmp/zed_test_special.txt";
    editor_save_file(&te.editor, temp_file);

    TestEditor te2;
    editor_open_file(&te2.editor, temp_file);

    std::string loaded = te2.get_text();
    printf("[DEBUG] Loaded text length: %zu, content: '%s'\n", loaded.length(), loaded.c_str());
    TEST_ASSERT_STR_EQ("Special: !@#$%^&*()", loaded.c_str(), "Special chars preserved");

    unlink(temp_file);
}

// Save multi-line file
TEST_CASE(test_file_multiline) {
    TestEditor te;

    te.type_text("Line 1");
    te.press_enter();
    te.type_text("Line 2");
    te.press_enter();
    te.type_text("Line 3");

    const char* temp_file = "/tmp/zed_test_multiline.txt";
    editor_save_file(&te.editor, temp_file);

    TestEditor te2;
    editor_open_file(&te2.editor, temp_file);

    std::string loaded = te2.get_text();
    printf("[DEBUG] Multi-line loaded length: %zu, content: '%s'\n", loaded.length(), loaded.c_str());
    TEST_ASSERT_STR_EQ("Line 1\nLine 2\nLine 3", loaded.c_str(), "Multi-line content preserved");

    unlink(temp_file);
}

// Save and load large file
TEST_CASE(test_file_large) {
    TestEditor te;

    // Create large content (10,000 characters)
    for (int i = 0; i < 1000; i++) {
        te.type_text("0123456789");
    }

    const char* temp_file = "/tmp/zed_test_large.txt";
    editor_save_file(&te.editor, temp_file);

    TestEditor te2;
    editor_open_file(&te2.editor, temp_file);

    TEST_ASSERT_EQ(10000, te2.get_text_length(), "Large file length preserved");

    unlink(temp_file);
}

// Main function
int main() {
    return run_all_tests();
}
