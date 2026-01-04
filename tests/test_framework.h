// Test Framework for Zed Editor
// Provides assertions, test registration, and helper utilities for headless testing

#ifndef ZED_TEST_FRAMEWORK_H
#define ZED_TEST_FRAMEWORK_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

// Include editor headers
#include "../src/editor.h"
#include "../src/config.h"
#include "../src/platform.h"

// ANSI color codes for output
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"

// Global test state
static int g_test_count = 0;
static int g_test_passed = 0;
static int g_test_failed = 0;
static const char* g_current_test = nullptr;

// Assertion macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  " COLOR_RED "✗ FAILED" COLOR_RESET ": %s\n", message); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
            printf("    condition: %s\n", #condition); \
            g_test_failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do { \
        auto exp_val = (expected); \
        auto act_val = (actual); \
        if (exp_val != act_val) { \
            printf("  " COLOR_RED "✗ FAILED" COLOR_RESET ": %s\n", message); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
            printf("    expected: %zu\n", (size_t)exp_val); \
            printf("    actual:   %zu\n", (size_t)act_val); \
            g_test_failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_STR_EQ(expected, actual, message) \
    do { \
        const char* exp_str = (expected); \
        const char* act_str = (actual); \
        if (strcmp(exp_str, act_str) != 0) { \
            printf("  " COLOR_RED "✗ FAILED" COLOR_RESET ": %s\n", message); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
            printf("    expected: \"%s\"\n", exp_str); \
            printf("    actual:   \"%s\"\n", act_str); \
            g_test_failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_NEQ(not_expected, actual, message) \
    do { \
        auto nexp_val = (not_expected); \
        auto act_val = (actual); \
        if (nexp_val == act_val) { \
            printf("  " COLOR_RED "✗ FAILED" COLOR_RESET ": %s\n", message); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
            printf("    value should not be: %zu\n", (size_t)nexp_val); \
            g_test_failed++; \
            return; \
        } \
    } while(0)

// Test case registration
struct TestCase {
    const char* name;
    std::function<void()> func;
};

static std::vector<TestCase>& get_test_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

#define TEST_CASE(test_name) \
    static void test_name(); \
    static struct TestRegistrar_##test_name { \
        TestRegistrar_##test_name() { \
            get_test_registry().push_back({#test_name, test_name}); \
        } \
    } registrar_##test_name; \
    static void test_name()

// Event builders
inline PlatformEvent make_key_event(int key, int mods, const char* text) {
    PlatformEvent event;
    memset(&event, 0, sizeof(event));
    event.type = PLATFORM_EVENT_KEY_PRESS;
    event.key.key = key;
    event.key.mods = mods;
    if (text) {
        strncpy(event.key.text, text, sizeof(event.key.text) - 1);
        event.key.text[sizeof(event.key.text) - 1] = '\0';
    }
    return event;
}

inline PlatformEvent make_text_event(const char* text) {
    return make_key_event(0, 0, text);
}

inline PlatformEvent make_mouse_event(int x, int y, int button) {
    PlatformEvent event;
    memset(&event, 0, sizeof(event));
    event.type = PLATFORM_EVENT_MOUSE_BUTTON;
    event.mouse_button.x = x;
    event.mouse_button.y = y;
    event.mouse_button.button = button;
    event.mouse_button.pressed = true;
    return event;
}

// Mock clipboard for testing (X11 clipboard won't work headless)
static char test_clipboard[4096] = "";

inline void test_set_clipboard(const char* text) {
    strncpy(test_clipboard, text, sizeof(test_clipboard) - 1);
    test_clipboard[sizeof(test_clipboard) - 1] = '\0';
}

inline const char* test_get_clipboard() {
    return test_clipboard;
}

// TestEditor helper class
struct TestEditor {
    Editor editor;
    Config config;

    // Constructor: initialize editor without platform/renderer
    TestEditor() {
        config_set_defaults(&config);
        editor_init(&editor, &config);
    }

    // Destructor: cleanup
    ~TestEditor() {
        editor_shutdown(&editor);
    }

    // Convenience methods for common operations
    void type_text(const char* text) {
        for (size_t i = 0; text[i]; i++) {
            char ch[2] = {text[i], '\0'};
            PlatformEvent event = make_text_event(ch);
            editor_handle_event(&editor, &event, nullptr, nullptr);
        }
    }

    void press_key(int key, int mods = 0) {
        PlatformEvent event = make_key_event(key, mods, "");
        editor_handle_event(&editor, &event, nullptr, nullptr);
    }

    void press_ctrl(char key) {
        press_key(key, PLATFORM_MOD_CTRL);
    }

    void press_shift(int key) {
        press_key(key, PLATFORM_MOD_SHIFT);
    }

    void press_ctrl_shift(char key) {
        press_key(key, PLATFORM_MOD_CTRL | PLATFORM_MOD_SHIFT);
    }

    void open_search() {
        press_ctrl('f');
    }

    void close_search() {
        press_key(0xff1b, 0);  // Escape
    }

    void press_enter() {
        press_key(0xff0d, 0);  // Return
    }

    void press_backspace() {
        press_key(0xff08, 0);
    }

    // State query methods
    std::string get_text() {
        char* text = rope_to_string(&editor.rope);
        std::string result(text ? text : "");
        delete[] text;
        return result;
    }

    size_t get_cursor() {
        return editor.cursor_pos;
    }

    bool has_selection() {
        return editor.has_selection;
    }

    size_t get_selection_start() {
        return editor.selection_start;
    }

    size_t get_selection_end() {
        return editor.selection_end;
    }

    size_t get_text_length() {
        return rope_length(&editor.rope);
    }

    bool search_is_active() {
        return editor.search_state->active;
    }

    size_t get_search_matches() {
        return editor.search_state->match_count;
    }

    std::string get_search_query() {
        return std::string(editor.search_state->query);
    }

    bool get_search_case_sensitive() {
        return editor.search_state->case_sensitive;
    }

    size_t get_search_current_match() {
        return editor.search_state->current_match_index;
    }

    size_t get_rope_version() {
        return editor.rope_version;
    }

    size_t get_cached_text_version() {
        return editor.cached_text_version;
    }

    bool cache_is_stale() {
        return editor.rope_version != editor.cached_text_version || !editor.cached_text;
    }

    // Simulate frame update (for features that update over time)
    void update(float delta_time = 0.016f) {
        editor_update(&editor, delta_time);
    }
};

// Test runner
inline int run_all_tests() {
    printf("\n" COLOR_CYAN "Running Zed Editor Tests" COLOR_RESET "\n");
    printf("========================\n\n");

    auto& tests = get_test_registry();

    for (const auto& test : tests) {
        g_current_test = test.name;
        g_test_count++;

        printf(COLOR_YELLOW "%s" COLOR_RESET "...\n", test.name);

        int failed_before = g_test_failed;
        test.func();

        if (g_test_failed == failed_before) {
            printf("  " COLOR_GREEN "✓ PASSED" COLOR_RESET "\n");
            g_test_passed++;
        }
    }

    printf("\n========================\n");
    printf("Tests run: %d\n", g_test_count);
    printf(COLOR_GREEN "Passed: %d" COLOR_RESET "\n", g_test_passed);

    if (g_test_failed > 0) {
        printf(COLOR_RED "Failed: %d" COLOR_RESET "\n", g_test_failed);
        return 1;
    } else {
        printf("\n" COLOR_GREEN "All tests passed!" COLOR_RESET "\n\n");
        return 0;
    }
}

#endif // ZED_TEST_FRAMEWORK_H
