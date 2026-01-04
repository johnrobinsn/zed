// Test Utilities - Enhanced testing capabilities
// Provides state dumps, screenshots, and integration test helpers

#ifndef TEST_UTILITIES_H
#define TEST_UTILITIES_H

#include <string>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

// Editor state snapshot for debugging and assertions
struct EditorSnapshot {
    std::string text;
    size_t cursor_pos;
    bool has_selection;
    size_t selection_start;
    size_t selection_end;
    size_t rope_version;
    size_t text_length;

    // Search state
    bool search_active;
    std::string search_query;
    size_t search_matches;
    bool search_case_sensitive;

    // Additional state
    std::string file_path;
    size_t undo_stack_size;
    size_t redo_stack_size;

    // Comparison operators
    bool operator==(const EditorSnapshot& other) const {
        return text == other.text &&
               cursor_pos == other.cursor_pos &&
               has_selection == other.has_selection &&
               selection_start == other.selection_start &&
               selection_end == other.selection_end;
    }

    bool operator!=(const EditorSnapshot& other) const {
        return !(*this == other);
    }

    // Serialize to string for debugging
    std::string to_string() const {
        std::ostringstream oss;
        oss << "EditorSnapshot {\n";
        oss << "  text: \"" << text.substr(0, 50);
        if (text.length() > 50) oss << "...";
        oss << "\" (len=" << text_length << ")\n";
        oss << "  cursor: " << cursor_pos << "\n";
        oss << "  selection: " << (has_selection ? "yes" : "no");
        if (has_selection) {
            oss << " [" << selection_start << ", " << selection_end << "]";
        }
        oss << "\n";
        oss << "  search: " << (search_active ? "active" : "inactive");
        if (search_active) {
            oss << " query=\"" << search_query << "\" matches=" << search_matches;
        }
        oss << "\n";
        oss << "  rope_version: " << rope_version << "\n";
        oss << "  undo/redo: " << undo_stack_size << "/" << redo_stack_size << "\n";
        oss << "}";
        return oss.str();
    }

    // Save to file
    void save(const char* filename) const {
        std::ofstream f(filename);
        f << to_string();
        f.close();
    }
};

// Capture editor state snapshot
inline EditorSnapshot capture_snapshot(Editor* editor) {
    EditorSnapshot snap;

    // Text content
    char* text = rope_to_string(&editor->rope);
    snap.text = text ? text : "";
    delete[] text;

    // Basic state
    snap.cursor_pos = editor->cursor_pos;
    snap.has_selection = editor->has_selection;
    snap.selection_start = editor->selection_start;
    snap.selection_end = editor->selection_end;
    snap.rope_version = editor->rope_version;
    snap.text_length = rope_length(&editor->rope);

    // Search state
    snap.search_active = editor->search_state->active;
    snap.search_query = editor->search_state->query;
    snap.search_matches = editor->search_state->match_count;
    snap.search_case_sensitive = editor->search_state->case_sensitive;

    // File path
    snap.file_path = editor->file_path ? editor->file_path : "";

    // Undo/redo stack sizes
    snap.undo_stack_size = editor->undo_stack.size();
    snap.redo_stack_size = editor->redo_stack.size();

    return snap;
}

// Xvfb helper for integration tests
class XvfbSession {
private:
    int display_num;
    pid_t xvfb_pid;
    bool running;

public:
    XvfbSession(int display = 99) : display_num(display), xvfb_pid(-1), running(false) {}

    bool start() {
        // Check if Xvfb is available
        if (system("which Xvfb > /dev/null 2>&1") != 0) {
            fprintf(stderr, "Xvfb not found. Install with: sudo apt-get install xvfb\n");
            return false;
        }

        // Start Xvfb
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "Xvfb :%d -screen 0 1024x768x24 > /dev/null 2>&1 &", display_num);
        system(cmd);

        // Wait for Xvfb to start
        usleep(500000);  // 500ms

        // Set DISPLAY environment variable
        char display_env[32];
        snprintf(display_env, sizeof(display_env), ":%d", display_num);
        setenv("DISPLAY", display_env, 1);

        printf("[XVFB] Started on display :%d\n", display_num);
        running = true;
        return true;
    }

    void stop() {
        if (!running) return;

        // Kill Xvfb
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "pkill -f 'Xvfb :%d'", display_num);
        system(cmd);

        printf("[XVFB] Stopped display :%d\n", display_num);
        running = false;
    }

    ~XvfbSession() {
        stop();
    }

    // Take screenshot using scrot or import
    bool screenshot(const char* filename) {
        if (!running) return false;

        char cmd[512];

        // Try scrot first
        if (system("which scrot > /dev/null 2>&1") == 0) {
            snprintf(cmd, sizeof(cmd), "DISPLAY=:%d scrot '%s' 2>/dev/null", display_num, filename);
            return system(cmd) == 0;
        }

        // Try import (ImageMagick)
        if (system("which import > /dev/null 2>&1") == 0) {
            snprintf(cmd, sizeof(cmd), "DISPLAY=:%d import -window root '%s' 2>/dev/null",
                    display_num, filename);
            return system(cmd) == 0;
        }

        fprintf(stderr, "No screenshot tool found (scrot or ImageMagick)\n");
        return false;
    }
};

// Integration test helper - run editor with real platform
class IntegrationTestEditor {
private:
    XvfbSession* xvfb;
    Editor editor;
    Config config;
    Platform platform;
    Renderer renderer;
    bool platform_initialized;
    bool renderer_initialized;

public:
    IntegrationTestEditor(XvfbSession* xvfb_session)
        : xvfb(xvfb_session), platform_initialized(false), renderer_initialized(false) {

        config_set_defaults(&config);
        editor_init(&editor, &config);

        // Initialize platform with real X11
        platform_initialized = platform_init(&platform, &config);
        if (platform_initialized) {
            // Initialize renderer with real OpenGL
            renderer_initialized = renderer_init(&renderer, &config);
            printf("[INTEGRATION] Platform and renderer initialized\n");
        }
    }

    ~IntegrationTestEditor() {
        if (renderer_initialized) {
            renderer_shutdown(&renderer);
        }
        if (platform_initialized) {
            platform_shutdown(&platform);
        }
        editor_shutdown(&editor);
    }

    bool is_ready() const {
        return platform_initialized && renderer_initialized;
    }

    // Send synthetic event
    void send_key(int key, int mods = 0, const char* text = "") {
        if (!platform_initialized) return;

        PlatformEvent event;
        event.type = PLATFORM_EVENT_KEY_PRESS;
        event.key.key = key;
        event.key.mods = mods;
        strncpy(event.key.text, text, sizeof(event.key.text) - 1);

        editor_handle_event(&editor, &event, &renderer, &platform);
    }

    // Render frame
    void render() {
        if (!renderer_initialized) return;

        renderer_begin_frame(&renderer);
        editor_render(&editor, &renderer);
        renderer_end_frame(&renderer);
        platform_swap_buffers(&platform);
    }

    // Take screenshot
    bool screenshot(const char* filename) {
        if (!xvfb) return false;
        render();  // Render current state
        return xvfb->screenshot(filename);
    }

    // Get snapshot
    EditorSnapshot snapshot() {
        return capture_snapshot(&editor);
    }

    // Type text
    void type(const char* text) {
        for (size_t i = 0; text[i]; i++) {
            char ch[2] = {text[i], '\0'};
            send_key(0, 0, ch);
        }
    }

    // Copy to clipboard (real X11 clipboard)
    void copy() {
        send_key('c', PLATFORM_MOD_CTRL);
    }

    // Paste from clipboard (real X11 clipboard)
    void paste() {
        send_key('v', PLATFORM_MOD_CTRL);
    }

    // Get editor for direct access
    Editor* get_editor() { return &editor; }
    Platform* get_platform() { return &platform; }
    Renderer* get_renderer() { return &renderer; }
};

// Visual regression test - compare screenshots
inline bool compare_screenshots(const char* actual, const char* expected, const char* diff_output = nullptr) {
    // Use ImageMagick compare if available
    if (system("which compare > /dev/null 2>&1") == 0) {
        char cmd[1024];
        if (diff_output) {
            snprintf(cmd, sizeof(cmd), "compare -metric AE '%s' '%s' '%s' 2>&1",
                    actual, expected, diff_output);
        } else {
            snprintf(cmd, sizeof(cmd), "compare -metric AE '%s' '%s' null: 2>&1",
                    actual, expected);
        }

        FILE* pipe = popen(cmd, "r");
        if (!pipe) return false;

        char result[128];
        fgets(result, sizeof(result), pipe);
        int diff = atoi(result);
        pclose(pipe);

        printf("[VISUAL] Screenshot diff: %d pixels\n", diff);

        // Allow up to 100 pixels difference (for anti-aliasing, etc.)
        return diff <= 100;
    }

    fprintf(stderr, "ImageMagick 'compare' not found\n");
    return false;
}

#endif // TEST_UTILITIES_H
