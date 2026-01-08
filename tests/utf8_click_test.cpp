// UTF-8 Click Positioning Tests
// Tests that layout cache is properly indexed by byte position (not codepoint)
// and validates the UTF-8 handling in layout calculations

#include "../src/editor.h"
#include "../src/config.h"
#include "test_framework.h"
#include "test_utilities.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>

// Test helper: Simulate layout cache building with the same logic as editor_calculate_layout
// but without needing FreeType (uses fixed glyph width)
struct MockLayoutCache {
    std::vector<float> char_positions;

    void build(const char* text, float glyph_width = 8.0f, float line_height = 16.0f) {
        char_positions.clear();
        size_t text_len = strlen(text);
        char_positions.reserve(text_len + 1);

        float x = 0.0f;
        const char* p = text;
        size_t byte_pos = 0;

        while (byte_pos < text_len && *p) {
            // Decode UTF-8 character to determine byte length
            const char* prev_p = p;
            uint32_t codepoint = utf8_decode(&p);
            if (codepoint == 0) break;

            size_t char_bytes = p - prev_p;

            // CRITICAL: Store position for EACH BYTE (matching the fix)
            for (size_t i = 0; i < char_bytes; i++) {
                char_positions.push_back(x);
            }

            byte_pos += char_bytes;

            if (codepoint == '\n') {
                x = 0.0f;
            } else {
                x += glyph_width;
            }
        }

        // Store final position
        char_positions.push_back(x);
    }
};

// Test: Layout cache size matches byte length (not codepoint count)
void test_layout_cache_byte_indexing() {
    printf("TEST: Layout cache size matches byte length\n");

    MockLayoutCache cache;

    // Test with mixed UTF-8: "x≤y" where ≤ is 3 bytes (U+2264)
    // Total: 1 (x) + 3 (≤) + 1 (y) = 5 bytes, 3 codepoints
    const char* test_text = "x≤y";
    size_t byte_length = strlen(test_text);  // Should be 5

    cache.build(test_text);

    // Cache should have entries for each byte position plus one for end
    size_t expected_cache_size = byte_length + 1;
    size_t actual_cache_size = cache.char_positions.size();

    printf("  Text: \"%s\"\n", test_text);
    printf("  Byte length: %zu\n", byte_length);
    printf("  Expected cache size: %zu\n", expected_cache_size);
    printf("  Actual cache size: %zu\n", actual_cache_size);

    assert(byte_length == 5);
    assert(actual_cache_size == expected_cache_size);
    printf("  ✓ Cache size equals byte length + 1\n");

    // Verify multi-byte character bytes have same position
    // Bytes 1, 2, 3 (indices 1, 2, 3) should all have the same X position (after 'x')
    float pos_byte1 = cache.char_positions[1];
    float pos_byte2 = cache.char_positions[2];
    float pos_byte3 = cache.char_positions[3];

    printf("  Position at byte 1 (start of ≤): %.2f\n", pos_byte1);
    printf("  Position at byte 2 (middle of ≤): %.2f\n", pos_byte2);
    printf("  Position at byte 3 (end of ≤): %.2f\n", pos_byte3);

    assert(pos_byte1 == pos_byte2);
    assert(pos_byte2 == pos_byte3);
    printf("  ✓ Multi-byte character bytes share same position\n");

    // Verify 'y' has a different position (after the ≤)
    float pos_y = cache.char_positions[4];
    printf("  Position at byte 4 ('y'): %.2f\n", pos_y);
    assert(pos_y > pos_byte1);
    printf("  ✓ Position after multi-byte char is advanced\n");
}

// Test: Layout cache handles 4-byte UTF-8 (emoji)
void test_layout_cache_4byte_utf8() {
    printf("TEST: Layout cache with 4-byte UTF-8 (emoji)\n");

    MockLayoutCache cache;

    // Test: "a🌍b" where 🌍 is 4 bytes (U+1F30D)
    // Total: 1 (a) + 4 (🌍) + 1 (b) = 6 bytes
    const char* test_text = "a🌍b";
    size_t byte_length = strlen(test_text);

    cache.build(test_text);

    size_t expected_cache_size = byte_length + 1;
    size_t actual_cache_size = cache.char_positions.size();

    printf("  Text: \"a🌍b\"\n");
    printf("  Byte length: %zu\n", byte_length);
    printf("  Cache size: %zu\n", actual_cache_size);

    assert(byte_length == 6);
    assert(actual_cache_size == expected_cache_size);
    printf("  ✓ Cache size correct for 4-byte emoji\n");

    // Verify all 4 bytes of emoji have same position
    float pos_emoji_byte0 = cache.char_positions[1];
    float pos_emoji_byte1 = cache.char_positions[2];
    float pos_emoji_byte2 = cache.char_positions[3];
    float pos_emoji_byte3 = cache.char_positions[4];

    assert(pos_emoji_byte0 == pos_emoji_byte1);
    assert(pos_emoji_byte1 == pos_emoji_byte2);
    assert(pos_emoji_byte2 == pos_emoji_byte3);
    printf("  ✓ All 4 bytes of emoji share same position\n");

    // Verify 'b' is after emoji
    float pos_b = cache.char_positions[5];
    assert(pos_b > pos_emoji_byte0);
    printf("  ✓ Position after emoji is advanced\n");
}

// Test: 2-byte UTF-8 characters (accented letters)
void test_layout_cache_2byte_utf8() {
    printf("TEST: Layout cache with 2-byte UTF-8 (accented)\n");

    MockLayoutCache cache;

    // Test: "café" where é is 2 bytes (U+00E9)
    // Total: 1 (c) + 1 (a) + 1 (f) + 2 (é) = 5 bytes
    const char* test_text = "café";
    size_t byte_length = strlen(test_text);

    cache.build(test_text);

    printf("  Text: \"café\"\n");
    printf("  Byte length: %zu\n", byte_length);
    printf("  Cache size: %zu\n", cache.char_positions.size());

    assert(byte_length == 5);
    assert(cache.char_positions.size() == 6);  // byte_length + 1
    printf("  ✓ Cache size correct for 2-byte accented char\n");

    // Bytes 3 and 4 should have same position (both part of é)
    assert(cache.char_positions[3] == cache.char_positions[4]);
    printf("  ✓ Both bytes of é share same position\n");
}

// Test: Multiline with UTF-8 - cache positions reset at newlines
void test_layout_cache_multiline_utf8() {
    printf("TEST: Multiline layout cache with UTF-8\n");

    MockLayoutCache cache;

    // Test: "世界\nHello"
    // Line 1: 世(3) + 界(3) + \n(1) = 7 bytes
    // Line 2: H(1) + e(1) + l(1) + l(1) + o(1) = 5 bytes
    // Total = 12 bytes
    const char* test_text = "世界\nHello";
    size_t byte_length = strlen(test_text);

    cache.build(test_text);

    printf("  Text: \"世界\\nHello\"\n");
    printf("  Byte length: %zu\n", byte_length);
    printf("  Cache size: %zu\n", cache.char_positions.size());

    assert(byte_length == 12);
    assert(cache.char_positions.size() == 13);
    printf("  ✓ Cache size correct for multiline UTF-8\n");

    // Check newline position (byte 6) - should be at x = 2*8 = 16 (after 2 chars)
    float newline_x = cache.char_positions[6];
    printf("  Newline (byte 6) at x=%.1f\n", newline_x);
    assert(newline_x == 16.0f);  // 2 characters * 8.0 glyph_width

    // After newline (byte 7 = 'H'), x should reset to 0
    float h_x = cache.char_positions[7];
    printf("  'H' (byte 7) at x=%.1f\n", h_x);
    assert(h_x == 0.0f);
    printf("  ✓ X position resets after newline\n");
}

// Test: Large file with many UTF-8 characters (like SPEC.md)
void test_large_utf8_content() {
    printf("TEST: Large content with scattered UTF-8\n");

    MockLayoutCache cache;

    // Simulate SPEC.md-like content with UTF-8 scattered throughout
    char text[4000] = "";
    int utf8_chars = 0;

    for (int i = 0; i < 40; i++) {
        char line[80];
        if (i % 10 == 0) {
            // Lines with UTF-8: "Line NN: value ≤ 100\n"
            snprintf(line, sizeof(line), "Line %02d: value ≤ 100\n", i);
            utf8_chars++;
        } else {
            snprintf(line, sizeof(line), "Line %02d: ASCII only text\n", i);
        }
        strcat(text, line);
    }

    size_t byte_length = strlen(text);
    cache.build(text);

    printf("  Lines: 40, UTF-8 chars: %d\n", utf8_chars);
    printf("  Byte length: %zu\n", byte_length);
    printf("  Cache size: %zu\n", cache.char_positions.size());

    // Cache must cover all bytes
    assert(cache.char_positions.size() == byte_length + 1);
    printf("  ✓ Cache covers all bytes in large file\n");

    // Verify we can index any byte position without going out of bounds
    for (size_t i = 0; i < byte_length; i++) {
        // This should not crash or go out of bounds
        float x = cache.char_positions[i];
        (void)x;
    }
    printf("  ✓ All byte positions are indexable\n");
}

// Test: Verify the BUG scenario - byte positions after line 27 in UTF-8 content
void test_line27_bug_scenario() {
    printf("TEST: Line 27+ bug scenario (byte indexing after UTF-8)\n");

    MockLayoutCache cache;

    // Create content where UTF-8 appears before line 27
    // This simulates the bug where cache was short due to UTF-8
    char text[4000] = "";

    for (int i = 0; i < 35; i++) {
        char line[80];
        if (i < 25) {
            // Early lines with some UTF-8
            if (i == 10 || i == 20) {
                snprintf(line, sizeof(line), "Line %02d: has ≤ and ≥ symbols\n", i);
            } else {
                snprintf(line, sizeof(line), "Line %02d: normal ASCII text\n", i);
            }
        } else {
            // Lines 25+ are all ASCII
            snprintf(line, sizeof(line), "Line %02d: text after UTF-8 area\n", i);
        }
        strcat(text, line);
    }

    size_t byte_length = strlen(text);
    cache.build(text);

    printf("  Total bytes: %zu\n", byte_length);
    printf("  Cache entries: %zu\n", cache.char_positions.size());

    // The OLD bug: cache would be short because it had one entry per codepoint
    // The FIX: cache has one entry per byte
    assert(cache.char_positions.size() == byte_length + 1);
    printf("  ✓ Cache size matches byte count (not codepoint count)\n");

    // Find line 27 start position
    size_t line = 0;
    size_t line27_start = 0;
    for (size_t i = 0; i < byte_length && line < 27; i++) {
        if (text[i] == '\n') {
            line++;
            if (line == 27) {
                line27_start = i + 1;
            }
        }
    }

    printf("  Line 27 starts at byte %zu\n", line27_start);

    // This was the bug: accessing char_positions[line27_start] would be wrong
    // because the cache was indexed by codepoint, not byte
    assert(line27_start < cache.char_positions.size());
    float x_at_line27 = cache.char_positions[line27_start];
    printf("  X position at line 27 start: %.1f\n", x_at_line27);

    // Line starts should always have x = 0
    assert(x_at_line27 == 0.0f);
    printf("  ✓ Line 27 correctly has x=0 at start\n");

    // Check a position in the middle of line 27
    size_t mid_line27 = line27_start + 10;
    if (mid_line27 < byte_length) {
        float x_mid = cache.char_positions[mid_line27];
        printf("  X position 10 chars into line 27: %.1f\n", x_mid);
        assert(x_mid > 0.0f);  // Should not be 0!
        printf("  ✓ Mid-line position is non-zero (the bug was it would be 0)\n");
    }
}

// Test: Verify utf8_decode handles all byte lengths correctly
void test_utf8_decode_byte_lengths() {
    printf("TEST: utf8_decode returns correct byte lengths\n");

    // 1-byte: ASCII 'A' (U+0041)
    const char* ascii = "A";
    const char* p = ascii;
    uint32_t cp = utf8_decode(&p);
    assert(cp == 0x41);
    assert(p - ascii == 1);
    printf("  ✓ 1-byte ASCII: 'A' = U+%04X, %ld bytes\n", cp, p - ascii);

    // 2-byte: 'é' (U+00E9)
    const char* two_byte = "é";
    p = two_byte;
    cp = utf8_decode(&p);
    assert(cp == 0xE9);
    assert(p - two_byte == 2);
    printf("  ✓ 2-byte char: 'é' = U+%04X, %ld bytes\n", cp, p - two_byte);

    // 3-byte: '≤' (U+2264)
    const char* three_byte = "≤";
    p = three_byte;
    cp = utf8_decode(&p);
    assert(cp == 0x2264);
    assert(p - three_byte == 3);
    printf("  ✓ 3-byte char: '≤' = U+%04X, %ld bytes\n", cp, p - three_byte);

    // 4-byte: '🌍' (U+1F30D)
    const char* four_byte = "🌍";
    p = four_byte;
    cp = utf8_decode(&p);
    assert(cp == 0x1F30D);
    assert(p - four_byte == 4);
    printf("  ✓ 4-byte emoji: '🌍' = U+%04X, %ld bytes\n", cp, p - four_byte);
}

int main() {
    printf("\n=== UTF-8 Click Positioning Test Suite ===\n\n");

    test_utf8_decode_byte_lengths();
    printf("\n");

    test_layout_cache_byte_indexing();
    printf("\n");

    test_layout_cache_2byte_utf8();
    printf("\n");

    test_layout_cache_4byte_utf8();
    printf("\n");

    test_layout_cache_multiline_utf8();
    printf("\n");

    test_large_utf8_content();
    printf("\n");

    test_line27_bug_scenario();
    printf("\n");

    printf("=== All UTF-8 click positioning tests passed! ===\n\n");
    return 0;
}
