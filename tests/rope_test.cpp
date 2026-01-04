// Unit tests for rope data structure

#include "../src/rope.h"
#include <cstdio>
#include <cstring>
#include <cassert>

void test_rope_creation() {
    printf("Test: Rope creation...\n");

    Rope rope;
    rope_init(&rope);
    assert(rope_length(&rope) == 0);

    rope_from_string(&rope, "Hello, World!");
    assert(rope_length(&rope) == 13);

    char* str = rope_to_string(&rope);
    assert(strcmp(str, "Hello, World!") == 0);
    delete[] str;

    rope_free(&rope);
    printf("  PASSED\n");
}

void test_rope_insert() {
    printf("Test: Rope insert...\n");

    Rope rope;
    rope_from_string(&rope, "Hello!");

    rope_insert(&rope, 5, " World", 6);
    assert(rope_length(&rope) == 12);

    char* str = rope_to_string(&rope);
    assert(strcmp(str, "Hello World!") == 0);
    delete[] str;

    // Insert at beginning
    rope_insert(&rope, 0, ">> ", 3);
    str = rope_to_string(&rope);
    assert(strcmp(str, ">> Hello World!") == 0);
    delete[] str;

    // Insert at end
    rope_insert(&rope, rope_length(&rope), " <<", 3);
    str = rope_to_string(&rope);
    assert(strcmp(str, ">> Hello World! <<") == 0);
    delete[] str;

    rope_free(&rope);
    printf("  PASSED\n");
}

void test_rope_delete() {
    printf("Test: Rope delete...\n");

    Rope rope;
    rope_from_string(&rope, "Hello, World!");

    // Delete middle
    rope_delete(&rope, 5, 7);
    assert(rope_length(&rope) == 6);

    char* str = rope_to_string(&rope);
    assert(strcmp(str, "Hello!") == 0);
    delete[] str;

    // Delete from beginning
    rope_delete(&rope, 0, 2);
    str = rope_to_string(&rope);
    assert(strcmp(str, "llo!") == 0);
    delete[] str;

    // Delete from end
    rope_delete(&rope, rope_length(&rope) - 1, 1);
    str = rope_to_string(&rope);
    assert(strcmp(str, "llo") == 0);
    delete[] str;

    rope_free(&rope);
    printf("  PASSED\n");
}

void test_rope_char_at() {
    printf("Test: Rope char_at...\n");

    Rope rope;
    rope_from_string(&rope, "Hello");

    assert(rope_char_at(&rope, 0) == 'H');
    assert(rope_char_at(&rope, 1) == 'e');
    assert(rope_char_at(&rope, 4) == 'o');

    rope_free(&rope);
    printf("  PASSED\n");
}

void test_rope_large() {
    printf("Test: Rope large file...\n");

    Rope rope;
    rope_init(&rope);

    // Insert many lines
    for (int i = 0; i < 1000; i++) {
        char line[128];
        snprintf(line, sizeof(line), "This is line %d\n", i);
        rope_insert(&rope, rope_length(&rope), line, strlen(line));
    }

    assert(rope_length(&rope) > 0);

    // Verify some content
    char buffer[64];
    size_t expected_len = 15;  // "This is line 0\n" is 15 chars
    size_t copied = rope_copy(&rope, 0, buffer, expected_len);
    buffer[copied] = '\0';
    assert(strcmp(buffer, "This is line 0\n") == 0);

    rope_free(&rope);
    printf("  PASSED\n");
}

int main() {
    printf("Running rope tests...\n\n");

    test_rope_creation();
    test_rope_insert();
    test_rope_delete();
    test_rope_char_at();
    test_rope_large();

    printf("\nAll tests passed!\n");
    return 0;
}
