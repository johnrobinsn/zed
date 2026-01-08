// Editor state and logic

#ifndef ZED_EDITOR_H
#define ZED_EDITOR_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include "config.h"
#include "platform.h"
#include "renderer.h"
#include "rope.h"
#include "font.h"

#include <vector>

// Debug logging control - set to 1 to enable verbose mouse/click/layout logging
#define EDITOR_DEBUG_MOUSE 0
#define EDITOR_DEBUG_LAYOUT 0

// Command types for undo/redo
enum CommandType {
    CMD_INSERT,
    CMD_DELETE
};

// Command for undo/redo
struct Command {
    CommandType type;
    size_t pos;
    char* content;
    size_t length;
};

// Text layout cache for accurate cursor positioning
struct TextLayout {
    std::vector<float> char_positions;  // X position for each character
    size_t text_length;
    bool valid;
};

// Editor state
struct Editor {
    Config* config;

    // Text buffer
    Rope rope;
    char* file_path;  // Currently open file path (NULL if new file)

    // Cursor state
    size_t cursor_pos;
    float cursor_blink_time;
    bool cursor_visible;
    size_t cursor_preferred_col;  // Preferred column for up/down navigation

    // Selection state
    bool has_selection;
    size_t selection_start;
    size_t selection_end;
    bool mouse_dragging;  // Track if we're dragging to select

    // Undo/redo system
    std::vector<Command> undo_stack;
    std::vector<Command> redo_stack;
    static constexpr size_t MAX_UNDO_STACK = 1000;

    // Viewport/scrolling
    float scroll_y;           // Vertical scroll offset in pixels
    float line_height;        // Height of one line in pixels
    int viewport_height;      // Height of viewport in pixels

    // Layout cache for accurate cursor positioning
    TextLayout layout_cache;

    // Text caching to avoid rope_to_string() every frame
    char* cached_text;
    size_t rope_version;        // Incremented on rope modifications
    size_t cached_text_version; // Version of cached_text

    // Search state
    struct SearchState* search_state;

    // Context menu
    struct ContextMenu* context_menu;
};

// Search functionality
#define SEARCH_QUERY_MAX_LEN 256

struct SearchState {
    bool active;                        // Is search box visible?
    char query[SEARCH_QUERY_MAX_LEN];  // Current search query
    size_t query_len;                   // Length of query string

    // Match tracking
    size_t* match_positions;            // Array of match positions in rope
    size_t match_count;                 // Number of matches found
    size_t match_capacity;              // Allocated capacity
    size_t current_match_index;         // Which match is selected (0-based)

    // Options
    bool case_sensitive;                // Match case exactly

    // Version tracking
    size_t rope_version_at_search;      // Invalidate matches when rope changes
};

// Context menu
struct ContextMenu {
    bool active;
    int x, y;  // Position where menu was opened
    int selected_item;  // -1 = none, 0+ = menu item index
};

// Forward declarations for search functions
inline void editor_search_open(Editor* editor);
inline void editor_search_close(Editor* editor);
inline void editor_search_update_matches(Editor* editor);
inline void editor_search_next_match(Editor* editor);
inline void editor_search_prev_match(Editor* editor);

// Initialize editor
inline void editor_init(Editor* editor, Config* config) {
    editor->config = config;
    rope_init(&editor->rope);
    editor->file_path = nullptr;
    editor->cursor_pos = 0;
    editor->cursor_blink_time = 0.0f;
    editor->cursor_visible = true;
    editor->cursor_preferred_col = 0;
    editor->has_selection = false;
    editor->selection_start = 0;
    editor->selection_end = 0;
    editor->mouse_dragging = false;

    // Initialize viewport
    editor->scroll_y = 0.0f;
    editor->line_height = 16.0f;  // Will match renderer line height
    editor->viewport_height = 720; // Initial, will be updated on resize

    // Initialize layout cache
    editor->layout_cache.valid = false;
    editor->layout_cache.text_length = 0;

    // Initialize text cache
    editor->cached_text = nullptr;
    editor->rope_version = 0;
    editor->cached_text_version = 0;

    // Initialize search state
    editor->search_state = new SearchState();
    editor->search_state->active = false;
    editor->search_state->query[0] = '\0';
    editor->search_state->query_len = 0;
    editor->search_state->match_positions = nullptr;
    editor->search_state->match_count = 0;
    editor->search_state->match_capacity = 0;
    editor->search_state->current_match_index = 0;
    editor->search_state->case_sensitive = false;
    editor->search_state->rope_version_at_search = 0;

    // Initialize context menu
    editor->context_menu = new ContextMenu();
    editor->context_menu->active = false;
    editor->context_menu->x = 0;
    editor->context_menu->y = 0;
    editor->context_menu->selected_item = -1;

    // Start with empty buffer (welcome text removed for automated testing)
    rope_from_string(&editor->rope, "");
}

// Synchronize font metrics from renderer (call after zoom changes)
inline void editor_sync_font_metrics(Editor* editor, Renderer* renderer) {
    float old_line_height = editor->line_height;
    editor->line_height = renderer->font_sys.line_height;

    // Invalidate layout cache when line height changes
    if (old_line_height != editor->line_height) {
        editor->layout_cache.valid = false;
        printf("Editor line height: %.1f → %.1f\n", old_line_height, editor->line_height);
    }
}

// ============================================================================
// COORDINATE TRANSFORMATION
// ============================================================================
// Unified transformation between document space and screen space
// Handles zoom scaling and scroll offset consistently
//
// Document space: Character positions in pixels at current font size
//                 (e.g., char 10 at 14px font ≈ 84px, at 21px font ≈ 126px)
// Screen space: Scaled, scrolled viewport coordinates (window pixels)
//
// Transform: screen = (doc - scroll) * zoom + margin * zoom
//            Equivalently: screen = zoom * (doc - scroll + margin)

// Transform document X coordinate to screen space
inline float editor_doc_to_screen_x(Editor* editor, Renderer* renderer, float doc_x) {
    float zoom_scale = renderer->font_sys.font_size / (float)renderer->base_font_size;
    float margin_x = 20.0f;  // Left margin
    return (doc_x - 0) * zoom_scale + margin_x * zoom_scale;
}

// Transform document Y coordinate to screen space
inline float editor_doc_to_screen_y(Editor* editor, Renderer* renderer, float doc_y) {
    float zoom_scale = renderer->font_sys.font_size / (float)renderer->base_font_size;
    float margin_y = 40.0f;  // Top margin
    return (doc_y - editor->scroll_y) * zoom_scale + margin_y * zoom_scale;
}

// Inverse transform: screen X coordinate to document space (current font pixels)
// Layout cache is in "current font pixel space", so we just subtract scaled margin
inline float editor_screen_to_doc_x(Editor* editor, Renderer* renderer, float screen_x) {
    float zoom_scale = renderer->font_sys.font_size / (float)renderer->base_font_size;
    float margin_x = 20.0f;
    return screen_x - (margin_x * zoom_scale);
}

// Inverse transform: screen Y coordinate to document space (current font pixels)
inline float editor_screen_to_doc_y(Editor* editor, Renderer* renderer, float screen_y) {
    float zoom_scale = renderer->font_sys.font_size / (float)renderer->base_font_size;
    float margin_y = 40.0f;
    return (screen_y - margin_y * zoom_scale) + editor->scroll_y;
}

// Convenience: Get base text position in screen space (for rendering)
inline void editor_get_text_origin_screen(Editor* editor, Renderer* renderer,
                                         float* out_x, float* out_y) {
    *out_x = editor_doc_to_screen_x(editor, renderer, 0);
    *out_y = editor_doc_to_screen_y(editor, renderer, 0);
}

// Forward declarations
inline bool editor_save_file(Editor* editor, const char* path = nullptr);
inline size_t editor_mouse_to_pos(Editor* editor, const char* text, float mouse_x, float mouse_y,
                                   float start_x, float start_y, float line_height);
inline void editor_push_command(Editor* editor, CommandType type, size_t pos, const char* content, size_t length);

// Copy selected text to clipboard
inline void editor_copy(Editor* editor, Platform* platform) {
    if (!editor->has_selection) {
        printf("No selection to copy\n");
        return;
    }

    size_t start = editor->selection_start < editor->selection_end ? editor->selection_start : editor->selection_end;
    size_t end = editor->selection_start < editor->selection_end ? editor->selection_end : editor->selection_start;
    size_t length = end - start;

    char* selected_text = new char[length + 1];
    rope_substr(&editor->rope, start, length, selected_text);
    selected_text[length] = '\0';  // Ensure null termination

    printf("[COPY DEBUG] Selected text: '%s' (start=%zu, end=%zu, len=%zu)\n", selected_text, start, end, length);
    platform_set_clipboard(platform, selected_text);
    delete[] selected_text;

    printf("Copied %zu characters\n", length);
}

// Cut selected text to clipboard
inline void editor_cut(Editor* editor, Platform* platform) {
    if (!editor->has_selection) {
        printf("No selection to cut\n");
        return;
    }

    // First copy
    editor_copy(editor, platform);

    // Then delete
    size_t start = editor->selection_start < editor->selection_end ? editor->selection_start : editor->selection_end;
    size_t end = editor->selection_start < editor->selection_end ? editor->selection_end : editor->selection_start;
    size_t length = end - start;

    // Get text for undo
    char* deleted_text = new char[length + 1];
    rope_substr(&editor->rope, start, length, deleted_text);

    // Record command
    editor_push_command(editor, CMD_DELETE, start, deleted_text, length);

    // Delete
    rope_delete(&editor->rope, start, length);
    editor->cursor_pos = start;
    editor->has_selection = false;
    editor->rope_version++;  // Invalidate cache

    delete[] deleted_text;
}

// Paste from clipboard
inline void editor_paste(Editor* editor, Platform* platform) {
    char* clipboard_text = platform_get_clipboard(platform);
    if (!clipboard_text) {
        printf("No clipboard content available\n");
        return;
    }

    size_t paste_len = strlen(clipboard_text);
    printf("[PASTE DEBUG] Clipboard text: '%s' (len=%zu)\n", clipboard_text, paste_len);
    if (paste_len == 0) {
        delete[] clipboard_text;
        return;
    }

    // If there's a selection, delete it first
    if (editor->has_selection) {
        size_t start = editor->selection_start < editor->selection_end ? editor->selection_start : editor->selection_end;
        size_t end = editor->selection_start < editor->selection_end ? editor->selection_end : editor->selection_start;
        size_t length = end - start;

        char* deleted_text = new char[length + 1];
        rope_substr(&editor->rope, start, length, deleted_text);
        editor_push_command(editor, CMD_DELETE, start, deleted_text, length);
        rope_delete(&editor->rope, start, length);
        editor->cursor_pos = start;
        editor->has_selection = false;
        editor->rope_version++;  // Invalidate cache
        delete[] deleted_text;
    }

    // Insert clipboard content
    printf("[PASTE DEBUG] Inserting at cursor_pos=%zu\n", editor->cursor_pos);
    editor_push_command(editor, CMD_INSERT, editor->cursor_pos, clipboard_text, paste_len);
    rope_insert(&editor->rope, editor->cursor_pos, clipboard_text, paste_len);
    editor->cursor_pos += paste_len;
    editor->rope_version++;  // Invalidate cache

    // Debug: Check rope length after paste
    size_t new_len = rope_length(&editor->rope);
    printf("[PASTE DEBUG] Rope length after paste: %zu, cursor now at: %zu\n", new_len, editor->cursor_pos);

    printf("Pasted %zu characters\n", paste_len);
    delete[] clipboard_text;
}

// Select all text
inline void editor_select_all(Editor* editor) {
    editor->has_selection = true;
    editor->selection_start = 0;
    editor->selection_end = rope_length(&editor->rope);
    editor->cursor_pos = editor->selection_end;
    printf("Selected all text\n");
}

// Calculate text layout with accurate glyph metrics
inline void editor_calculate_layout(Editor* editor, Renderer* renderer, const char* text) {
    if (!text) return;

    size_t text_len = strlen(text);
    editor->layout_cache.char_positions.clear();
    editor->layout_cache.char_positions.reserve(text_len + 1);

#if EDITOR_DEBUG_LAYOUT
    printf("[LAYOUT] Calculating layout for %zu chars, font_size=%d, line_height=%.1f\n",
           text_len, renderer->font_sys.font_size, editor->line_height);
#endif

    float x = 0.0f;
    float y = 0.0f;
    float line_height = editor->line_height;

    // Store position for each character (UTF-8 aware)
    const char* p = text;
    size_t byte_pos = 0;

    while (byte_pos < text_len && *p) {
        // Decode UTF-8 character first to know byte length
        const char* prev_p = p;
        uint32_t codepoint = utf8_decode(&p);
        if (codepoint == 0) break;

        size_t char_bytes = p - prev_p;

        // CRITICAL: Store position for EACH BYTE of the character
        // This ensures char_positions[byte_index] always works correctly
        // For multi-byte UTF-8, all bytes of the same char get the same X position
        for (size_t i = 0; i < char_bytes; i++) {
            editor->layout_cache.char_positions.push_back(x);
        }

        byte_pos += char_bytes;

        if (codepoint == '\n') {
            // Newline: reset X, advance Y
            x = 0.0f;
            y += line_height;
        } else {
            // Regular character: get actual glyph metrics
            GlyphInfo* glyph = font_system_get_glyph(&renderer->font_sys, codepoint);

            float advance = 0;
            if (glyph) {
                // Use actual advance_x from FreeType, not 8.4px approximation
                advance = glyph->advance_x;
                x += advance;
            } else {
                // Fallback for missing glyphs
                advance = 8.4f;
                x += advance;
            }
        }
    }

    // Store final position (end of text)
    editor->layout_cache.char_positions.push_back(x);

    editor->layout_cache.text_length = text_len;
    editor->layout_cache.valid = true;

#if EDITOR_DEBUG_LAYOUT
    printf("[LAYOUT] Built cache with %zu positions, first 5: ", editor->layout_cache.char_positions.size());
    for (size_t i = 0; i < 5 && i < editor->layout_cache.char_positions.size(); i++) {
        printf("%.2f ", editor->layout_cache.char_positions[i]);
    }
    printf("\n");

    // Show character positions for a line in the middle (to verify lower half)
    size_t middle_line = 20;  // Check line 20
    size_t line = 0;
    size_t pos = 0;
    while (pos < text_len && text[pos] && line < middle_line) {
        if (text[pos] == '\n') line++;
        pos++;
    }
    if (line == middle_line && pos < editor->layout_cache.char_positions.size()) {
        printf("[LAYOUT] Line %zu (pos %zu) first 10 positions: ", middle_line, pos);
        for (size_t i = 0; i < 10 && (pos + i) < editor->layout_cache.char_positions.size() && text[pos + i] != '\n'; i++) {
            printf("%.2f ", editor->layout_cache.char_positions[pos + i]);
        }
        printf("\n");
    }
#endif
}

// Calculate maximum scroll position (don't scroll past end of document)
inline float editor_get_max_scroll(Editor* editor) {
    // Count total lines in document
    size_t total_lines = 1;  // At least 1 line
    size_t rope_len = rope_length(&editor->rope);
    for (size_t i = 0; i < rope_len; i++) {
        if (rope_char_at(&editor->rope, i) == '\n') {
            total_lines++;
        }
    }

    // Calculate total document height
    float doc_height = total_lines * editor->line_height;

    // Scroll margin to keep cursor comfortable from edges (same as in editor_ensure_cursor_visible)
    float scroll_margin = editor->line_height * 2.0f;

    // Max scroll = document height - viewport height + margin
    // This allows the last line to have comfortable spacing
    float max_scroll = doc_height - editor->viewport_height + scroll_margin;
    if (max_scroll < 0.0f) max_scroll = 0.0f;

    return max_scroll;
}

// Clamp scroll to valid range
inline void editor_clamp_scroll(Editor* editor) {
    if (editor->scroll_y < 0.0f) {
        editor->scroll_y = 0.0f;
    }

    float max_scroll = editor_get_max_scroll(editor);
    if (editor->scroll_y > max_scroll) {
        editor->scroll_y = max_scroll;
    }
}

// Scroll editor view
inline void editor_scroll(Editor* editor, float delta_y) {
    editor->scroll_y += delta_y;
    editor_clamp_scroll(editor);
}

// Ensure cursor is visible in viewport
inline void editor_ensure_cursor_visible(Editor* editor) {
    // Calculate cursor line
    size_t line = 0;
    size_t pos = 0;
    size_t rope_len = rope_length(&editor->rope);

    while (pos < editor->cursor_pos && pos < rope_len) {
        if (rope_char_at(&editor->rope, pos) == '\n') {
            line++;
        }
        pos++;
    }

    float cursor_y = line * editor->line_height;

    // The line extends from cursor_y to cursor_y + line_height
    float line_top = cursor_y;
    float line_bottom = cursor_y + editor->line_height;

    // Scroll margin: keep cursor 2 lines away from edges for comfortable viewing
    float scroll_margin = editor->line_height * 2.0f;

    // Check if line is above viewport (top of line not visible)
    // Keep a comfortable margin from the top edge
    float comfortable_viewport_top = editor->scroll_y + scroll_margin;

    if (line_top < comfortable_viewport_top) {
        editor->scroll_y = line_top - scroll_margin;
        if (editor->scroll_y < 0.0f) editor->scroll_y = 0.0f;
    }

    // Check if line is below viewport (bottom of line not visible)
    // Keep a comfortable margin (2 lines) from the bottom edge
    float comfortable_viewport_bottom = editor->scroll_y + editor->viewport_height - scroll_margin;

    if (line_bottom > comfortable_viewport_bottom) {
        // Scroll so the line has margin from the bottom edge
        editor->scroll_y = line_bottom - editor->viewport_height + scroll_margin;
    }

    // Clamp to valid scroll range
    editor_clamp_scroll(editor);
}

// Helper: Find start of line containing position
inline size_t editor_line_start(Rope* rope, size_t pos) {
    if (pos == 0) return 0;

    // Search backwards for newline
    size_t line_start = pos;
    while (line_start > 0) {
        char c = rope_char_at(rope, line_start - 1);
        if (c == '\n') break;
        line_start--;
    }
    return line_start;
}

// Helper: Find end of line containing position
inline size_t editor_line_end(Rope* rope, size_t pos) {
    size_t rope_len = rope_length(rope);
    size_t line_end = pos;

    while (line_end < rope_len) {
        char c = rope_char_at(rope, line_end);
        if (c == '\n') break;
        line_end++;
    }
    return line_end;
}

// Helper: Get column position within current line
inline size_t editor_get_column(Rope* rope, size_t pos) {
    size_t line_start = editor_line_start(rope, pos);
    return pos - line_start;
}

// Helper: Get line number (1-based) for position
inline size_t editor_get_line_number(Rope* rope, size_t pos) {
    size_t line = 1;
    for (size_t i = 0; i < pos && i < rope_length(rope); i++) {
        if (rope_char_at(rope, i) == '\n') {
            line++;
        }
    }
    return line;
}

// Helper: Move cursor up one line
inline void editor_move_up(Editor* editor) {
    size_t line_start = editor_line_start(&editor->rope, editor->cursor_pos);
    if (line_start == 0) return; // Already on first line

    // Find previous line
    size_t prev_line_end = line_start - 1; // The newline character
    size_t prev_line_start = editor_line_start(&editor->rope, prev_line_end);
    size_t prev_line_len = prev_line_end - prev_line_start;

    // Position cursor at preferred column, or end of line if shorter
    size_t target_col = editor->cursor_preferred_col;
    if (target_col > prev_line_len) target_col = prev_line_len;

    editor->cursor_pos = prev_line_start + target_col;
}

// Helper: Move cursor down one line
inline void editor_move_down(Editor* editor) {
    size_t line_end = editor_line_end(&editor->rope, editor->cursor_pos);
    size_t rope_len = rope_length(&editor->rope);

    if (line_end >= rope_len) return; // Already on last line

    // Find next line
    size_t next_line_start = line_end + 1; // Skip the newline
    size_t next_line_end = editor_line_end(&editor->rope, next_line_start);
    size_t next_line_len = next_line_end - next_line_start;

    // Position cursor at preferred column, or end of line if shorter
    size_t target_col = editor->cursor_preferred_col;
    if (target_col > next_line_len) target_col = next_line_len;

    editor->cursor_pos = next_line_start + target_col;
}

// Helper: Move cursor to start of line (Home)
inline void editor_move_home(Editor* editor) {
    editor->cursor_pos = editor_line_start(&editor->rope, editor->cursor_pos);
    editor->cursor_preferred_col = 0;
}

// Helper: Move cursor to end of line (End)
inline void editor_move_end(Editor* editor) {
    editor->cursor_pos = editor_line_end(&editor->rope, editor->cursor_pos);
    editor->cursor_preferred_col = editor_get_column(&editor->rope, editor->cursor_pos);
}

// Helper: Move cursor one page up
inline void editor_page_up(Editor* editor) {
    int lines_per_page = (editor->viewport_height / (int)editor->line_height) - 1;
    for (int i = 0; i < lines_per_page; i++) {
        editor_move_up(editor);
    }
}

// Helper: Move cursor one page down
inline void editor_page_down(Editor* editor) {
    int lines_per_page = (editor->viewport_height / (int)editor->line_height) - 1;
    for (int i = 0; i < lines_per_page; i++) {
        editor_move_down(editor);
    }
}

// Push command to undo stack
inline void editor_push_command(Editor* editor, CommandType type, size_t pos, const char* content, size_t length) {
    // Clear redo stack when new edit is made
    for (auto& cmd : editor->redo_stack) {
        delete[] cmd.content;
    }
    editor->redo_stack.clear();

    // Create command
    Command cmd;
    cmd.type = type;
    cmd.pos = pos;
    cmd.length = length;
    cmd.content = new char[length + 1];
    memcpy(cmd.content, content, length);
    cmd.content[length] = '\0';

    // Add to undo stack
    editor->undo_stack.push_back(cmd);

    // Limit stack size
    while (editor->undo_stack.size() > Editor::MAX_UNDO_STACK) {
        delete[] editor->undo_stack[0].content;
        editor->undo_stack.erase(editor->undo_stack.begin());
    }
}

// Undo last command
inline void editor_undo(Editor* editor) {
    if (editor->undo_stack.empty()) {
        printf("Nothing to undo\n");
        return;
    }

    Command cmd = editor->undo_stack.back();
    editor->undo_stack.pop_back();

    if (cmd.type == CMD_INSERT) {
        // Undo insert by deleting
        rope_delete(&editor->rope, cmd.pos, cmd.length);
        editor->cursor_pos = cmd.pos;
    } else if (cmd.type == CMD_DELETE) {
        // Undo delete by inserting
        rope_insert(&editor->rope, cmd.pos, cmd.content, cmd.length);
        editor->cursor_pos = cmd.pos + cmd.length;
    }

    editor->rope_version++;  // Invalidate cache

    // Move command to redo stack
    editor->redo_stack.push_back(cmd);
}

// Redo last undone command
inline void editor_redo(Editor* editor) {
    if (editor->redo_stack.empty()) {
        printf("Nothing to redo\n");
        return;
    }

    Command cmd = editor->redo_stack.back();
    editor->redo_stack.pop_back();

    if (cmd.type == CMD_INSERT) {
        // Redo insert
        rope_insert(&editor->rope, cmd.pos, cmd.content, cmd.length);
        editor->cursor_pos = cmd.pos + cmd.length;
    } else if (cmd.type == CMD_DELETE) {
        // Redo delete
        rope_delete(&editor->rope, cmd.pos, cmd.length);
        editor->cursor_pos = cmd.pos;
    }

    editor->rope_version++;  // Invalidate cache

    // Move command back to undo stack
    editor->undo_stack.push_back(cmd);
}

// Handle platform event
inline void editor_handle_event(Editor* editor, PlatformEvent* event, Renderer* renderer, Platform* platform) {
    switch (event->type) {
        case PLATFORM_EVENT_KEY_PRESS: {
            // Reset cursor blink on any key
            editor->cursor_blink_time = 0.0f;
            editor->cursor_visible = true;

            int key = event->key.key;
            bool ctrl = event->key.mods & PLATFORM_MOD_CTRL;
            bool shift = event->key.mods & PLATFORM_MOD_SHIFT;
            bool alt = event->key.mods & PLATFORM_MOD_ALT;

            // Debug: Log Ctrl key combos
            if (ctrl && key >= 'a' && key <= 'z') {
                printf("[DEBUG] Ctrl+%c (key=%d, text='%s')\n", (char)key, key, event->key.text);
            }

            // If search is active, route text input to search query
            if (editor->search_state->active) {
                SearchState* search = editor->search_state;

                // Escape closes search
                if (key == 0xff1b) {  // Escape
                    editor_search_close(editor);
                    break;
                }

                // Enter/Return navigates to next match
                if (key == 0xff0d) {  // Return/Enter
                    if (shift) {
                        editor_search_prev_match(editor);
                    } else {
                        editor_search_next_match(editor);
                    }
                    break;
                }

                // Backspace removes character from query
                if (key == 0xff08) {  // Backspace
                    if (search->query_len > 0) {
                        search->query[--search->query_len] = '\0';
                        editor_search_update_matches(editor);
                    }
                    break;  // Always break, even if query is empty
                }

                // Ctrl+G for next/previous match (even when search active)
                if (ctrl && (key == 'g' || key == 'G')) {
                    if (shift) {
                        editor_search_prev_match(editor);
                    } else {
                        editor_search_next_match(editor);
                    }
                    break;
                }

                // Toggle case sensitivity (Ctrl+Alt+C)
                if (ctrl && alt && (key == 'c' || key == 'C')) {
                    search->case_sensitive = !search->case_sensitive;
                    editor_search_update_matches(editor);
                    printf("Search case sensitivity: %s\n",
                           search->case_sensitive ? "ON" : "OFF");
                    break;
                }

                // Regular text input adds to query
                if (event->key.text[0] != '\0' &&
                    search->query_len < SEARCH_QUERY_MAX_LEN - 1) {

                    // Add character to query
                    const char* text = event->key.text;
                    size_t text_len = strlen(text);

                    if (search->query_len + text_len < SEARCH_QUERY_MAX_LEN - 1) {
                        memcpy(search->query + search->query_len, text, text_len);
                        search->query_len += text_len;
                        search->query[search->query_len] = '\0';

                        printf("[Search] Input: '%s' (len=%zu)\n", text, text_len);
                        editor_search_update_matches(editor);
                    }
                    break;
                }

                // If we got here and search is active, prevent normal editing
                break;
            }

            // Ctrl+F: Open search
            if (ctrl && (key == 'f' || key == 'F')) {
                editor_search_open(editor);
                break;
            }

            // Ctrl+G: Find next (when search not active but has query)
            if (ctrl && (key == 'g' || key == 'G')) {
                if (editor->search_state->match_count > 0) {
                    if (shift) {
                        editor_search_prev_match(editor);
                    } else {
                        editor_search_next_match(editor);
                    }
                }
                break;
            }

            // Ctrl+S: Save file
            if (ctrl && (key == 's' || key == 'S')) {
                editor_save_file(editor);
            }
            // Ctrl+C: Copy
            else if (ctrl && (key == 'c' || key == 'C')) {
                editor_copy(editor, platform);
            }
            // Ctrl+V: Paste
            else if (ctrl && (key == 'v' || key == 'V')) {
                printf("Ctrl+V detected (key=%d)\n", key);
                editor_paste(editor, platform);
            }
            // Ctrl+X: Cut
            else if (ctrl && (key == 'x' || key == 'X')) {
                editor_cut(editor, platform);
            }
            // Ctrl+A: Select all
            else if (ctrl && (key == 'a' || key == 'A')) {
                editor_select_all(editor);
            }
            // Ctrl+Z: Undo
            else if (ctrl && !shift && (key == 'z' || key == 'Z')) {
                editor_undo(editor);
            }
            // Ctrl+Y or Ctrl+Shift+Z: Redo
            else if (ctrl && ((key == 'y' || key == 'Y') || (shift && (key == 'z' || key == 'Z')))) {
                editor_redo(editor);
            }
            // Zoom shortcuts
            else if (ctrl && (key == '+' || key == '=' || key == 0x002b)) {
                // Ctrl+Plus: Zoom in (accept +, =, and keypad +)
                renderer_zoom_in(renderer);
                editor_sync_font_metrics(editor, renderer);
                editor_ensure_cursor_visible(editor);
            }
            else if (ctrl && (key == '-' || key == 0x002d)) {
                // Ctrl+Minus: Zoom out
                renderer_zoom_out(renderer);
                editor_sync_font_metrics(editor, renderer);
                editor_ensure_cursor_visible(editor);
            }
            else if (ctrl && (key == '0' || key == 0x0030)) {
                // Ctrl+0: Reset zoom
                renderer_zoom_reset(renderer);
                editor_sync_font_metrics(editor, renderer);
                editor_ensure_cursor_visible(editor);
            }
            // Arrow keys (XK_Left = 0xff51, XK_Right = 0xff53, etc.)
            else if (key == 0xff51) { // Left
                if (shift) {
                    // Start or extend selection
                    if (!editor->has_selection) {
                        editor->has_selection = true;
                        editor->selection_start = editor->cursor_pos;
                    }
                    // UTF-8 aware: move to previous character boundary
                    if (editor->cursor_pos > 0) {
                        char* text = rope_to_string(&editor->rope);
                        editor->cursor_pos = utf8_prev_char_boundary(text, editor->cursor_pos);
                        delete[] text;
                    }
                    editor->selection_end = editor->cursor_pos;
                } else {
                    // Clear selection and move
                    editor->has_selection = false;
                    // UTF-8 aware: move to previous character boundary
                    if (editor->cursor_pos > 0) {
                        char* text = rope_to_string(&editor->rope);
                        editor->cursor_pos = utf8_prev_char_boundary(text, editor->cursor_pos);
                        delete[] text;
                    }
                }
                // Update preferred column for up/down
                editor->cursor_preferred_col = editor_get_column(&editor->rope, editor->cursor_pos);
            } else if (key == 0xff53) { // Right
                if (shift) {
                    // Start or extend selection
                    if (!editor->has_selection) {
                        editor->has_selection = true;
                        editor->selection_start = editor->cursor_pos;
                    }
                    // UTF-8 aware: move to next character boundary
                    if (editor->cursor_pos < rope_length(&editor->rope)) {
                        char* text = rope_to_string(&editor->rope);
                        size_t len = rope_length(&editor->rope);
                        editor->cursor_pos = utf8_next_char_boundary(text, editor->cursor_pos, len);
                        delete[] text;
                    }
                    editor->selection_end = editor->cursor_pos;
                } else {
                    // Clear selection and move
                    editor->has_selection = false;
                    // UTF-8 aware: move to next character boundary
                    if (editor->cursor_pos < rope_length(&editor->rope)) {
                        char* text = rope_to_string(&editor->rope);
                        size_t len = rope_length(&editor->rope);
                        editor->cursor_pos = utf8_next_char_boundary(text, editor->cursor_pos, len);
                        delete[] text;
                    }
                }
                // Update preferred column for up/down
                editor->cursor_preferred_col = editor_get_column(&editor->rope, editor->cursor_pos);
            } else if (key == 0xff52) { // Up
                if (shift) {
                    // Start or extend selection
                    if (!editor->has_selection) {
                        editor->has_selection = true;
                        editor->selection_start = editor->cursor_pos;
                    }
                    editor_move_up(editor);
                    editor->selection_end = editor->cursor_pos;
                } else {
                    // Clear selection and move
                    editor->has_selection = false;
                    editor_move_up(editor);
                }
                // Preferred column is maintained by move_up
                editor_ensure_cursor_visible(editor);
            } else if (key == 0xff54) { // Down
                if (shift) {
                    // Start or extend selection
                    if (!editor->has_selection) {
                        editor->has_selection = true;
                        editor->selection_start = editor->cursor_pos;
                    }
                    editor_move_down(editor);
                    editor->selection_end = editor->cursor_pos;
                } else {
                    // Clear selection and move
                    editor->has_selection = false;
                    editor_move_down(editor);
                }
                // Preferred column is maintained by move_down
                editor_ensure_cursor_visible(editor);
            }
            // Home key
            else if (key == 0xff50) {
                if (shift) {
                    // Start or extend selection
                    if (!editor->has_selection) {
                        editor->has_selection = true;
                        editor->selection_start = editor->cursor_pos;
                    }
                    editor_move_home(editor);
                    editor->selection_end = editor->cursor_pos;
                } else {
                    editor->has_selection = false;
                    editor_move_home(editor);
                }
                editor_ensure_cursor_visible(editor);
            }
            // End key
            else if (key == 0xff57) {
                if (shift) {
                    // Start or extend selection
                    if (!editor->has_selection) {
                        editor->has_selection = true;
                        editor->selection_start = editor->cursor_pos;
                    }
                    editor_move_end(editor);
                    editor->selection_end = editor->cursor_pos;
                } else {
                    editor->has_selection = false;
                    editor_move_end(editor);
                }
                editor_ensure_cursor_visible(editor);
            }
            // Page Up key
            else if (key == 0xff55) {
                if (shift) {
                    // Start or extend selection
                    if (!editor->has_selection) {
                        editor->has_selection = true;
                        editor->selection_start = editor->cursor_pos;
                    }
                    editor_page_up(editor);
                    editor->selection_end = editor->cursor_pos;
                } else {
                    editor->has_selection = false;
                    editor_page_up(editor);
                }
                editor_ensure_cursor_visible(editor);
            }
            // Page Down key
            else if (key == 0xff56) {
                if (shift) {
                    // Start or extend selection
                    if (!editor->has_selection) {
                        editor->has_selection = true;
                        editor->selection_start = editor->cursor_pos;
                    }
                    editor_page_down(editor);
                    editor->selection_end = editor->cursor_pos;
                } else {
                    editor->has_selection = false;
                    editor_page_down(editor);
                }
                editor_ensure_cursor_visible(editor);
            }
            else if (key == 0xff08 || key == 0xff7f) { // Backspace or Delete
                // Clear selection
                editor->has_selection = false;

                if (key == 0xff08 && editor->cursor_pos > 0) { // Backspace
                    // UTF-8 aware: find the start of the previous character
                    char* text = rope_to_string(&editor->rope);
                    size_t prev_pos = utf8_prev_char_boundary(text, editor->cursor_pos);
                    size_t char_len = editor->cursor_pos - prev_pos;

                    // Get character to delete for undo (up to 4 bytes for UTF-8)
                    char deleted_char[5];
                    rope_substr(&editor->rope, prev_pos, char_len, deleted_char);
                    deleted_char[char_len] = '\0';

                    // Record command
                    editor_push_command(editor, CMD_DELETE, prev_pos, deleted_char, char_len);

                    rope_delete(&editor->rope, prev_pos, char_len);
                    editor->cursor_pos = prev_pos;
                    editor->rope_version++;  // Invalidate cache
                    delete[] text;
                } else if (key == 0xff7f && editor->cursor_pos < rope_length(&editor->rope)) { // Delete
                    // UTF-8 aware: find the length of the character at cursor
                    char* text = rope_to_string(&editor->rope);
                    size_t char_len = utf8_char_length(text, editor->cursor_pos);

                    // Get character to delete for undo (up to 4 bytes for UTF-8)
                    char deleted_char[5];
                    rope_substr(&editor->rope, editor->cursor_pos, char_len, deleted_char);
                    deleted_char[char_len] = '\0';

                    // Record command
                    editor_push_command(editor, CMD_DELETE, editor->cursor_pos, deleted_char, char_len);

                    rope_delete(&editor->rope, editor->cursor_pos, char_len);
                    editor->rope_version++;  // Invalidate cache
                    delete[] text;
                }
            } else if (key == 0xff0d) { // Return/Enter
                // Clear selection
                editor->has_selection = false;

                // Record command
                editor_push_command(editor, CMD_INSERT, editor->cursor_pos, "\n", 1);

                rope_insert(&editor->rope, editor->cursor_pos, "\n", 1);
                editor->cursor_pos++;
                editor->rope_version++;  // Invalidate cache
            } else if (event->key.text[0] && !ctrl) {
                // Delete selection if active
                if (editor->has_selection) {
                    size_t start = editor->selection_start < editor->selection_end ?
                                   editor->selection_start : editor->selection_end;
                    size_t end = editor->selection_start < editor->selection_end ?
                                 editor->selection_end : editor->selection_start;
                    size_t length = end - start;

                    char* deleted_text = new char[length + 1];
                    rope_copy(&editor->rope, start, deleted_text, length);
                    deleted_text[length] = '\0';

                    editor_push_command(editor, CMD_DELETE, start, deleted_text, length);
                    rope_delete(&editor->rope, start, length);
                    editor->cursor_pos = start;
                    editor->has_selection = false;
                    editor->rope_version++;
                    delete[] deleted_text;
                }

                // Printable characters
                size_t text_len = strlen(event->key.text);

                // Record command
                editor_push_command(editor, CMD_INSERT, editor->cursor_pos, event->key.text, text_len);

                rope_insert(&editor->rope, editor->cursor_pos, event->key.text, text_len);
                editor->cursor_pos += text_len;
                editor->rope_version++;  // Invalidate cache
            }

            // Ensure cursor is visible after any key press
            editor_ensure_cursor_visible(editor);
            break;
        }

        case PLATFORM_EVENT_RESIZE:
            printf("Resize: %dx%d\n", event->resize.width, event->resize.height);
            renderer_resize(renderer, event->resize.width, event->resize.height);
            editor->viewport_height = event->resize.height;
            break;

        case PLATFORM_EVENT_MOUSE_BUTTON: {
            // Handle scroll wheel (buttons 4 and 5)
            if (event->mouse_button.button == 4 && event->mouse_button.pressed) {
                // Scroll up
                editor_scroll(editor, -editor->line_height * 3);
                break;
            } else if (event->mouse_button.button == 5 && event->mouse_button.pressed) {
                // Scroll down
                editor_scroll(editor, editor->line_height * 3);
                break;
            }

            // Convert rope to string to calculate positions
            char* text = rope_to_string(&editor->rope);

            // CRITICAL: Ensure layout cache is valid before processing click
            // The cache might be invalid after zoom/text changes, and we need
            // accurate glyph positions for correct click-to-position mapping.
            // Without this, the fallback path uses 8.4f approximation which
            // doesn't match actual rendered glyph widths.
            if (renderer && (!editor->layout_cache.valid || editor->layout_cache.char_positions.size() == 0)) {
#if EDITOR_DEBUG_MOUSE
                printf("[CLICK] Rebuilding layout cache before click processing\n");
#endif
                editor_calculate_layout(editor, renderer, text);
            }

            float mouse_doc_x, mouse_doc_y;
            float text_x, text_y;
            float line_height = editor->line_height;

            if (renderer) {
                // Transform mouse coordinates from screen space to document space
                mouse_doc_x = editor_screen_to_doc_x(editor, renderer, event->mouse_button.x);
                mouse_doc_y = editor_screen_to_doc_y(editor, renderer, event->mouse_button.y);

                // Document space origin (0, 0 in document = character position 0)
                text_x = 0.0f;
                text_y = 0.0f;

#if EDITOR_DEBUG_MOUSE
                // DEBUG: Full transformation details
                float zoom = renderer->font_sys.font_size / (float)renderer->base_font_size;
                printf("[CLICK] Screen=(%d, %d) -> Doc=(%.1f, %.1f) | Zoom=%.2fx Font=%d/%d Scroll=%.1f\n",
                       event->mouse_button.x, event->mouse_button.y,
                       mouse_doc_x, mouse_doc_y, zoom,
                       renderer->font_sys.font_size, renderer->base_font_size,
                       editor->scroll_y);
#endif
            } else {
                // Fallback for tests: apply margins but no zoom
                float margin_x = 20.0f;
                float margin_y = 40.0f;
                mouse_doc_x = event->mouse_button.x - margin_x;
                mouse_doc_y = event->mouse_button.y - margin_y;
                text_x = 0.0f;
                text_y = 0.0f;
            }

            // Use document-space coordinates (mouse and text origin both in document space)
            size_t clicked_pos = editor_mouse_to_pos(editor, text,
                                                     mouse_doc_x, mouse_doc_y,
                                                     text_x, text_y, line_height);

#if EDITOR_DEBUG_MOUSE
            // DEBUG: Click result with context
            if (clicked_pos < strlen(text)) {
                // Find line number for this position
                size_t line = 0;
                for (size_t i = 0; i < clicked_pos; i++) {
                    if (text[i] == '\n') line++;
                }
                // Find line start
                size_t line_start = 0;
                size_t current_line = 0;
                for (size_t i = 0; i < clicked_pos; i++) {
                    if (text[i] == '\n') {
                        current_line++;
                        line_start = i + 1;
                    }
                }
                size_t line_offset = clicked_pos - line_start;

                printf("[CLICK] Result: pos=%zu line=%zu offset=%zu char='%c' (0x%02x)\n",
                       clicked_pos, line, line_offset,
                       isprint(text[clicked_pos]) ? text[clicked_pos] : '?',
                       (unsigned char)text[clicked_pos]);
            } else {
                printf("[CLICK] Result: pos=%zu (END OF FILE)\n", clicked_pos);
            }
#endif
            delete[] text;

            if (event->mouse_button.pressed && event->mouse_button.button == 1) { // Left click
                // Check if clicking on context menu
                if (editor->context_menu->active && editor->context_menu->selected_item >= 0) {
                    // Execute menu action
                    switch (editor->context_menu->selected_item) {
                        case 0:  // Cut
                            if (editor->has_selection && platform) {
                                editor_cut(editor, platform);
                            }
                            break;
                        case 1:  // Copy
                            if (editor->has_selection && platform) {
                                editor_copy(editor, platform);
                            }
                            break;
                        case 2:  // Paste
                            if (platform) {
                                editor_paste(editor, platform);
                            }
                            break;
                        case 3:  // Select All
                            editor_select_all(editor);
                            break;
                    }
                    editor->context_menu->active = false;
                } else {
                    // Close context menu
                    editor->context_menu->active = false;

                    // Start selection
                    editor->cursor_pos = clicked_pos;
                    editor->selection_start = clicked_pos;
                    editor->selection_end = clicked_pos;
                    editor->has_selection = true;
                    editor->mouse_dragging = true;
                }
            } else if (!event->mouse_button.pressed && event->mouse_button.button == 1) { // Left release
                editor->mouse_dragging = false;
                // Clear selection if start == end
                if (editor->selection_start == editor->selection_end) {
                    editor->has_selection = false;
                }
            } else if (event->mouse_button.pressed && event->mouse_button.button == 3) { // Right click
                // Open context menu
                editor->context_menu->active = true;
                editor->context_menu->x = event->mouse_button.x;
                editor->context_menu->y = event->mouse_button.y;
                editor->context_menu->selected_item = -1;
            }
            break;
        }

        case PLATFORM_EVENT_MOUSE_MOVE: {
            // Update context menu selection if menu is active
            if (editor->context_menu->active) {
                ContextMenu* menu = editor->context_menu;
                float menu_width = 180.0f;
                float item_height = 30.0f;
                int item_count = 4;

                // Check if mouse is over menu
                if (event->mouse_move.x >= menu->x &&
                    event->mouse_move.x <= menu->x + menu_width &&
                    event->mouse_move.y >= menu->y &&
                    event->mouse_move.y <= menu->y + (item_height * item_count)) {

                    // Calculate which item is hovered
                    int item_index = (event->mouse_move.y - menu->y) / item_height;
                    menu->selected_item = item_index;
                } else {
                    menu->selected_item = -1;
                }
            }

            // Update cursor shape based on mouse position
            if (platform) {
                // Check if mouse is over text area (simple check: x > 10 and y > 30)
                bool over_text = (event->mouse_move.x > 10 && event->mouse_move.y > 30);
                platform_set_cursor(platform, over_text);
            }

            if (editor->mouse_dragging) {
                // Auto-scroll when dragging outside viewport
                float mouse_y = event->mouse_move.y;
                float top_margin = 40.0f;  // Should match rendering margin
                float auto_scroll_zone = 30.0f;  // Pixels from edge to trigger auto-scroll
                float scroll_speed = 2.0f;  // Pixels per frame

                if (mouse_y < top_margin + auto_scroll_zone) {
                    // Dragging near top edge - scroll up
                    float distance_from_edge = (top_margin + auto_scroll_zone) - mouse_y;
                    float scroll_amount = scroll_speed * (distance_from_edge / auto_scroll_zone);
                    editor->scroll_y -= scroll_amount;
                    editor_clamp_scroll(editor);
                } else if (mouse_y > editor->viewport_height - auto_scroll_zone) {
                    // Dragging near bottom edge - scroll down
                    float distance_from_edge = mouse_y - (editor->viewport_height - auto_scroll_zone);
                    float scroll_amount = scroll_speed * (distance_from_edge / auto_scroll_zone);
                    editor->scroll_y += scroll_amount;
                    editor_clamp_scroll(editor);
                }

                // Convert rope to string to calculate positions
                char* text = rope_to_string(&editor->rope);

                // CRITICAL: Ensure layout cache is valid before processing drag
                if (renderer && (!editor->layout_cache.valid || editor->layout_cache.char_positions.size() == 0)) {
                    editor_calculate_layout(editor, renderer, text);
                }

                float mouse_doc_x, mouse_doc_y;
                float text_x, text_y;
                float line_height = editor->line_height;

                if (renderer) {
                    // Transform mouse coordinates from screen space to document space
                    mouse_doc_x = editor_screen_to_doc_x(editor, renderer, event->mouse_move.x);
                    mouse_doc_y = editor_screen_to_doc_y(editor, renderer, event->mouse_move.y);

                    // Document space origin (0, 0 in document = character position 0)
                    text_x = 0.0f;
                    text_y = 0.0f;

#if EDITOR_DEBUG_MOUSE
                    // DEBUG: Full drag coordinate details
                    float zoom = renderer->font_sys.font_size / (float)renderer->base_font_size;
                    printf("[DRAG] Screen=(%d, %d) -> Doc=(%.1f, %.1f) | Zoom=%.2fx | text_x=%.1f text_y=%.1f\n",
                           event->mouse_move.x, event->mouse_move.y,
                           mouse_doc_x, mouse_doc_y, zoom,
                           text_x, text_y);
#endif
                } else {
                    // Fallback for tests: apply margins but no zoom
                    float margin_x = 20.0f;
                    float margin_y = 40.0f;
                    mouse_doc_x = event->mouse_move.x - margin_x;
                    mouse_doc_y = event->mouse_move.y - margin_y;
                    text_x = 0.0f;
                    text_y = 0.0f;
                }

                // Use document-space coordinates (mouse and text origin both in document space)
                size_t mouse_pos = editor_mouse_to_pos(editor, text,
                                                       mouse_doc_x, mouse_doc_y,
                                                       text_x, text_y, line_height);

#if EDITOR_DEBUG_MOUSE
                printf("[DRAG] Result: drag_pos=%zu\n", mouse_pos);
#endif
                delete[] text;

                // Update selection end and cursor
                editor->selection_end = mouse_pos;
                editor->cursor_pos = mouse_pos;
                editor->has_selection = (editor->selection_start != editor->selection_end);
            }
            break;
        }

        case PLATFORM_EVENT_MOUSE_WHEEL: {
            bool ctrl = event->mouse_wheel.ctrl_pressed;

            if (ctrl) {
                // Ctrl+Mousewheel: Zoom in/out
                if (event->mouse_wheel.delta > 0) {
                    // Scroll up = zoom in
                    renderer_zoom_in(renderer);
                } else if (event->mouse_wheel.delta < 0) {
                    // Scroll down = zoom out
                    renderer_zoom_out(renderer);
                }
                editor_sync_font_metrics(editor, renderer);
                editor_ensure_cursor_visible(editor);
            } else {
                // Normal scroll: 3 lines per wheel click
                float scroll_amount = event->mouse_wheel.delta * 3.0f * editor->line_height;
                editor->scroll_y -= scroll_amount;

                // Clamp scroll to valid range
                editor_clamp_scroll(editor);
                // Note: Don't call editor_ensure_cursor_visible() here - user is manually scrolling
            }
            break;
        }

        default:
            break;
    }
}

// Update editor state
inline void editor_update(Editor* editor, float delta_time) {
    // Update cursor blink (0.5s on, 0.5s off)
    editor->cursor_blink_time += delta_time;
    if (editor->cursor_blink_time >= 1.0f) {
        editor->cursor_blink_time -= 1.0f;
    }
    editor->cursor_visible = editor->cursor_blink_time < 0.5f;

    // Re-run search if rope changed and search is active
    if (editor->search_state->active &&
        editor->search_state->rope_version_at_search != editor->rope_version &&
        editor->search_state->query_len > 0) {
        editor_search_update_matches(editor);
    }
}

// Helper: Calculate cursor screen position
inline void editor_get_cursor_pos(Editor* editor, const char* text, float start_x, float start_y,
                                   float line_height, float* out_x, float* out_y) {
    // Use layout cache for accurate positioning
    if (editor->layout_cache.valid && editor->cursor_pos < editor->layout_cache.char_positions.size()) {
        // Fast path: use cached positions
        float y = start_y;

        // Count newlines to calculate Y position
        for (size_t i = 0; i < editor->cursor_pos && text[i]; i++) {
            if (text[i] == '\n') {
                y += line_height;
            }
        }

        // Get X position from cache (already line-relative)
        float x = start_x + editor->layout_cache.char_positions[editor->cursor_pos];

        *out_x = x;
        *out_y = y;
    } else {
        // Fallback: calculate manually if cache is invalid (UTF-8 aware)
        float x = start_x;
        float y = start_y;
        size_t pos = 0;
        size_t len = strlen(text);

        while (pos < editor->cursor_pos && pos < len && text[pos]) {
            if (text[pos] == '\n') {
                x = start_x;
                y += line_height;
                pos++;
            } else {
                // Fallback to approximation per character
                x += 8.4f;
                // UTF-8 aware: skip to next character boundary
                pos = utf8_next_char_boundary(text, pos, len);
            }
        }

        *out_x = x;
        *out_y = y;
    }
}

// Helper: Convert mouse position to text position
inline size_t editor_mouse_to_pos(Editor* editor, const char* text, float mouse_x, float mouse_y,
                                   float start_x, float start_y, float line_height) {
#if EDITOR_DEBUG_MOUSE
    printf("[MOUSE_TO_POS] mouse=(%.1f, %.1f) start=(%.1f, %.1f) line_height=%.1f cache_valid=%d\n",
           mouse_x, mouse_y, start_x, start_y, line_height, editor->layout_cache.valid);
#endif

    // Use layout cache if available
    if (editor->layout_cache.valid && editor->layout_cache.char_positions.size() > 0) {
        float y = start_y;
        size_t pos = 0;
        size_t line_start = 0;
        size_t line_num = 0;

#if EDITOR_DEBUG_MOUSE
        printf("[LINE_SEARCH] Starting at y=%.1f, looking for mouse_y=%.1f\n", y, mouse_y);
#endif

        // First, find which line was clicked based on Y coordinate
        while (text[pos] && pos < editor->layout_cache.char_positions.size()) {
            // Y represents the top of the line in document space
            // Check if mouse is within this line's full height
            float line_top = y;
            float line_bottom = y + line_height;

#if EDITOR_DEBUG_MOUSE
            // DEBUG: Show line boundaries
            if (line_num % 5 == 0 || (mouse_y >= line_top && mouse_y < line_bottom)) {
                printf("[LINE %zu] y_range=[%.1f, %.1f) pos_range=[%zu-%zu) %s\n",
                       line_num, line_top, line_bottom, line_start, pos,
                       (mouse_y >= line_top && mouse_y < line_bottom) ? "<<< MATCH" : "");
            }
#endif

            // Check if mouse Y is on this line
            if (mouse_y >= line_top && mouse_y < line_bottom) {
#if EDITOR_DEBUG_MOUSE
                printf("[LINE_FOUND] Line %zu at pos %zu, searching for X position\n", line_num, line_start);
#endif

                // Found the line! Now find best X position within this line
                size_t best_pos = line_start;
                float best_distance = 1e9f;
                size_t line_pos = line_start;
                float actual_line_end_x = start_x;  // Track the true line end position

                // Search within this line only
                while (text[line_pos] && line_pos < editor->layout_cache.char_positions.size()) {
                    if (text[line_pos] == '\n') {
                        // End of line - check if click is beyond line end
                        // Use the tracked actual line end, NOT the stored newline position
#if EDITOR_DEBUG_MOUSE
                        printf("[LINE_END] pos=%zu actual_end_x=%.1f (mouse_x=%.1f) %s\n",
                               line_pos, actual_line_end_x, mouse_x,
                               mouse_x >= actual_line_end_x ? "BEYOND" : "WITHIN");
#endif

                        if (mouse_x >= actual_line_end_x) {
                            // Clicked beyond line end - position at newline
                            return line_pos;
                        }
                        break;
                    }

                    float x = start_x + editor->layout_cache.char_positions[line_pos];
                    actual_line_end_x = x;  // Update the actual line end as we go
                    float dx = mouse_x - x;
                    float distance = dx * dx;

#if EDITOR_DEBUG_MOUSE
                    // DEBUG: Show character positions every 5 chars or when finding best match
                    if ((line_pos - line_start) % 5 == 0 || distance < best_distance) {
                        printf("[CHAR] pos=%zu offset=%zu x=%.1f dx=%.1f dist=%.1f char='%c' %s\n",
                               line_pos, line_pos - line_start, x, dx, distance,
                               isprint(text[line_pos]) ? text[line_pos] : '?',
                               distance < best_distance ? "<<< NEW BEST" : "");
                    }
#endif

                    if (distance < best_distance) {
                        best_distance = distance;
                        best_pos = line_pos;
                    }

                    line_pos++;
                }

                // If we're at end of file (no newline), check if click is beyond
                if (!text[line_pos] && mouse_x >= start_x + editor->layout_cache.char_positions[line_pos]) {
#if EDITOR_DEBUG_MOUSE
                    printf("[EOF] pos=%zu x=%.1f (beyond)\n",
                           line_pos, start_x + editor->layout_cache.char_positions[line_pos]);
#endif
                    return line_pos;
                }

#if EDITOR_DEBUG_MOUSE
                printf("[BEST_MATCH] pos=%zu offset=%zu distance=%.1f\n",
                       best_pos, best_pos - line_start, best_distance);
#endif
                return best_pos;
            }

            // Move to next line
            if (text[pos] == '\n') {
                y += line_height;
                line_start = pos + 1;
                line_num++;
            }
            pos++;
        }

        // Click was beyond all lines - return end of text
#if EDITOR_DEBUG_MOUSE
        printf("[BEYOND_ALL_LINES] Returning pos=%zu\n", pos);
#endif
        return pos;
    } else {
        // Fallback: use approximation if cache is invalid (UTF-8 aware)
        float x = start_x;
        float y = start_y;
        size_t pos = 0;
        size_t line_start = 0;
        size_t len = strlen(text);

        // First, find which line was clicked based on Y coordinate
        size_t line_num = 0;
        while (pos < len && text[pos]) {
            float line_top = y;
            float line_bottom = y + line_height;

            // Check if mouse Y is on this line
            if (mouse_y >= line_top && mouse_y < line_bottom) {
                // Found the line! Now find best X position within this line
                size_t best_pos = line_start;
                float best_distance = 1e9f;
                size_t line_pos = line_start;
                float line_x = start_x;

                // Search within this line only (UTF-8 aware)
                while (line_pos < len && text[line_pos]) {
                    if (text[line_pos] == '\n') {
                        // End of line - check if click is beyond line end
                        if (mouse_x >= line_x) {
                            // Clicked beyond line end - position at newline
                            return line_pos;
                        }
                        break;
                    }

                    float dx = mouse_x - line_x;
                    float distance = dx * dx;

                    if (distance < best_distance) {
                        best_distance = distance;
                        best_pos = line_pos;
                    }

                    line_x += 8.4f;  // Fallback approximation per character
                    // UTF-8 aware: skip to next character boundary
                    line_pos = utf8_next_char_boundary(text, line_pos, len);
                }

                // If we're at end of file (no newline), check if click is beyond
                if (line_pos >= len || !text[line_pos]) {
                    if (mouse_x >= line_x) {
                        return line_pos;
                    }
                }

                return best_pos;
            }

            // Move to next line (UTF-8 aware)
            if (text[pos] == '\n') {
                y += line_height;
                line_start = pos + 1;
                x = start_x;
                line_num++;
                pos++;
            } else {
                x += 8.4f;
                // UTF-8 aware: skip to next character boundary
                pos = utf8_next_char_boundary(text, pos, len);
            }
        }

        // Click was beyond all lines - return end of text
        return pos;
    }
}

// Render editor
inline void editor_render(Editor* editor, Renderer* renderer) {
    // Use cached text string to avoid rope_to_string() every frame
    if (editor->rope_version != editor->cached_text_version || !editor->cached_text) {
        // Cache is invalid, regenerate text
        printf("[RENDER DEBUG] Regenerating cached text (rope_version=%zu, cached_version=%zu)\n",
               editor->rope_version, editor->cached_text_version);
        if (editor->cached_text) {
            delete[] editor->cached_text;
        }
        editor->cached_text = rope_to_string(&editor->rope);
        editor->cached_text_version = editor->rope_version;
        printf("[RENDER DEBUG] New cached text length: %zu\n", strlen(editor->cached_text ? editor->cached_text : ""));

        // Also recalculate layout when text changes
        editor_calculate_layout(editor, renderer, editor->cached_text);

        static bool first_render = true;
        if (first_render) {
            printf("Rendering %zu characters: '%.50s...'\n", rope_length(&editor->rope), editor->cached_text);
            first_render = false;
        }
    }
    // CRITICAL: Also rebuild layout cache when zoom changes (even if text doesn't change)
    else if (!editor->layout_cache.valid && editor->cached_text) {
        printf("[RENDER DEBUG] Rebuilding layout cache (zoom/font changed)\n");
        editor_calculate_layout(editor, renderer, editor->cached_text);
    }

    char* text = editor->cached_text;

    // Use unified transformation to get text origin in screen space
    float text_x, text_y;
    editor_get_text_origin_screen(editor, renderer, &text_x, &text_y);

    // Render selection if active
    if (editor->has_selection) {
        size_t sel_start = editor->selection_start < editor->selection_end ? editor->selection_start : editor->selection_end;
        size_t sel_end = editor->selection_start < editor->selection_end ? editor->selection_end : editor->selection_start;

        // Calculate selection rectangles
        float sel_start_x = text_x;
        float sel_start_y = text_y;
        float sel_end_x = text_x;
        float sel_end_y = text_y;
        float line_height = editor->line_height;

        // Use layout cache if available
        if (editor->layout_cache.valid && editor->layout_cache.char_positions.size() > 0) {
            // Fast path: use cached positions
            float y = text_y;

            // Find selection start position
            for (size_t i = 0; i < sel_start && text[i]; i++) {
                if (text[i] == '\n') {
                    y += line_height;
                }
            }
            if (sel_start < editor->layout_cache.char_positions.size()) {
                sel_start_x = text_x + editor->layout_cache.char_positions[sel_start];
                sel_start_y = y;
            }

            // Find selection end position
            y = text_y;
            for (size_t i = 0; i < sel_end && text[i]; i++) {
                if (text[i] == '\n') {
                    y += line_height;
                }
            }
            if (sel_end < editor->layout_cache.char_positions.size()) {
                sel_end_x = text_x + editor->layout_cache.char_positions[sel_end];
                sel_end_y = y;
            }
        } else {
            // Fallback: calculate manually (UTF-8 aware)
            float x = text_x;
            float y = text_y;
            size_t pos = 0;
            size_t len = strlen(text);

            while (pos <= sel_end && pos < len && text[pos]) {
                if (pos == sel_start) {
                    sel_start_x = x;
                    sel_start_y = y;
                }
                if (pos == sel_end) {
                    sel_end_x = x;
                    sel_end_y = y;
                    break;
                }

                if (text[pos] == '\n') {
                    x = text_x;
                    y += line_height;
                    pos++;
                } else {
                    x += 8.4f;  // Fallback approximation per character
                    // UTF-8 aware: skip to next character boundary
                    pos = utf8_next_char_boundary(text, pos, len);
                }
            }
        }

        // Render selection rectangles
        Color sel_color = {0.3f, 0.5f, 0.8f, 0.3f}; // Semi-transparent blue
        float sel_y_offset = 0.0f;  // No offset needed - text_y is already top-of-line

        if (sel_start_y == sel_end_y) {
            // Single line selection
            renderer_add_rect(renderer, sel_start_x, sel_start_y - sel_y_offset,
                            sel_end_x - sel_start_x, line_height, sel_color);
        } else {
            // Multiline selection
            // First line: from selection start to end of line
            float viewport_width = (float)renderer->viewport_width;
            renderer_add_rect(renderer, sel_start_x, sel_start_y - sel_y_offset,
                            viewport_width - sel_start_x, line_height, sel_color);

            // Middle lines: full width
            float current_y = sel_start_y + line_height;
            while (current_y < sel_end_y - 0.1f) {
                renderer_add_rect(renderer, text_x, current_y - sel_y_offset,
                                viewport_width - text_x, line_height, sel_color);
                current_y += line_height;
            }

            // Last line: from start of line to selection end
            renderer_add_rect(renderer, text_x, sel_end_y - sel_y_offset,
                            sel_end_x - text_x, line_height, sel_color);
        }
    }

    // Render text
    renderer_add_text(renderer, text, text_x, text_y, editor->config->foreground);

    // Render search match highlights
    if (editor->search_state->active && editor->search_state->match_count > 0) {
        SearchState* search = editor->search_state;
        size_t query_len = search->query_len;
        size_t text_len = strlen(text);
        float line_height = editor->line_height;

        for (size_t i = 0; i < search->match_count; i++) {
            size_t match_pos = search->match_positions[i];
            if (match_pos >= text_len) continue;  // Safety check

            bool is_current = (i == search->current_match_index);
            Color highlight_color = is_current ?
                editor->config->search_current_match_bg : editor->config->search_match_bg;

            // Calculate match start position using layout cache
            float match_x = text_x;
            float match_y = text_y;

            if (editor->layout_cache.valid && match_pos < editor->layout_cache.char_positions.size()) {
                // Use layout cache for accurate positioning
                size_t line_num = 0;
                for (size_t j = 0; j < match_pos; j++) {
                    if (text[j] == '\n') {
                        line_num++;
                    }
                }
                match_y = text_y + line_num * line_height;
                match_x = text_x + editor->layout_cache.char_positions[match_pos];
            } else {
                // Fallback: approximate positioning
                size_t line_num = 0;
                size_t pos = 0;
                for (size_t j = 0; j < match_pos; j++) {
                    if (text[j] == '\n') {
                        line_num++;
                        pos = j + 1;
                    }
                }
                match_y = text_y + line_num * line_height;
                size_t col = match_pos - pos;
                match_x = text_x + col * 8.4f;  // Fallback approximation
            }

            // Calculate width from match_pos to match_pos + query_len
            float match_width = 0.0f;
            size_t match_end = match_pos + query_len;
            if (editor->layout_cache.valid && match_end < editor->layout_cache.char_positions.size()) {
                // Use layout cache for accurate width
                match_width = editor->layout_cache.char_positions[match_end] -
                             editor->layout_cache.char_positions[match_pos];
            } else {
                // Fallback
                match_width = query_len * 8.4f;
            }

            // Draw highlight rectangle - no Y offset needed
            renderer_add_rect(renderer, match_x, match_y, match_width, line_height, highlight_color);
        }
    }

    // Render cursor if visible
    if (editor->cursor_visible) {
        float cursor_x, cursor_y;
        editor_get_cursor_pos(editor, text, text_x, text_y, editor->line_height, &cursor_x, &cursor_y);

        // Draw cursor rectangle (2px wide, line height tall)
        // No Y offset needed - cursor_y is already top-of-line
        renderer_add_rect(renderer, cursor_x, cursor_y, 2.0f, editor->line_height, editor->config->cursor);
    }

    // Flush document rendering before overlays (ensures search box appears on top)
    renderer_flush(renderer);

    // Render search box overlay
    if (editor->search_state->active) {
        SearchState* search = editor->search_state;

        // Search box dimensions
        float box_x = 10.0f;
        float box_y = 10.0f;
        float box_width = 400.0f;
        float box_height = 35.0f;
        float padding = 8.0f;

        // Background
        renderer_add_rect(renderer, box_x, box_y, box_width, box_height,
                         editor->config->search_box_bg);

        // "Find:" label
        Color label_color = {0.8f, 0.8f, 0.8f, 1.0f};
        renderer_add_text(renderer, "Find: ", box_x + padding,
                         box_y + padding + 2.0f, label_color);

        // Query text
        float query_x = box_x + padding + 50.0f;
        if (search->query_len > 0) {
            renderer_add_text(renderer, search->query, query_x,
                             box_y + padding + 2.0f, editor->config->foreground);
        }

        // Cursor in search box (blinking)
        if (editor->cursor_blink_time < 0.5f) {
            // Calculate cursor X position after query text using actual glyph metrics (UTF-8 aware)
            float text_width = 0.0f;
            const char* p = search->query;
            while (*p && (size_t)(p - search->query) < search->query_len) {
                uint32_t codepoint = utf8_decode(&p);
                if (codepoint == 0) break;

                GlyphInfo* glyph = font_system_get_glyph(&renderer->font_sys, codepoint);
                if (glyph) {
                    text_width += glyph->advance_x;
                } else {
                    text_width += editor->config->font_size * 0.6f;  // Fallback
                }
            }
            float cursor_x_offset = query_x + text_width;
            // Align cursor with text baseline (text is at box_y + padding + 2.0f)
            float cursor_y = box_y + padding + 2.0f - 12.0f;  // Baseline minus font ascent
            renderer_add_rect(renderer, cursor_x_offset, cursor_y,
                             2.0f, 16.0f, editor->config->cursor);
        }

        // Match counter
        if (search->query_len > 0) {
            char match_info[64];
            if (search->match_count > 0) {
                snprintf(match_info, sizeof(match_info), "%zu of %zu",
                        search->current_match_index + 1, search->match_count);
            } else {
                snprintf(match_info, sizeof(match_info), "No matches");
            }

            Color counter_color = {0.6f, 0.6f, 0.6f, 1.0f};
            renderer_add_text(renderer, match_info,
                             box_x + box_width - 100.0f,
                             box_y + padding + 2.0f, counter_color);
        }
    }

    // Render context menu
    if (editor->context_menu->active) {
        ContextMenu* menu = editor->context_menu;

        // Menu dimensions
        float menu_width = 180.0f;
        float item_height = 30.0f;
        const char* items[] = {"Cut", "Copy", "Paste", "Select All"};
        int item_count = 4;
        float menu_height = item_height * item_count;

        // Menu background
        Color menu_bg = {0.25f, 0.25f, 0.25f, 0.95f};
        renderer_add_rect(renderer, menu->x, menu->y, menu_width, menu_height, menu_bg);

        // Menu border
        Color border = {0.4f, 0.4f, 0.4f, 1.0f};
        renderer_add_rect(renderer, menu->x, menu->y, menu_width, 2.0f, border);  // Top
        renderer_add_rect(renderer, menu->x, menu->y + menu_height - 2.0f, menu_width, 2.0f, border);  // Bottom
        renderer_add_rect(renderer, menu->x, menu->y, 2.0f, menu_height, border);  // Left
        renderer_add_rect(renderer, menu->x + menu_width - 2.0f, menu->y, 2.0f, menu_height, border);  // Right

        // Menu items
        for (int i = 0; i < item_count; i++) {
            float item_y = menu->y + (i * item_height);

            // Highlight if hovered
            if (menu->selected_item == i) {
                Color highlight = {0.35f, 0.35f, 0.45f, 1.0f};
                renderer_add_rect(renderer, menu->x + 2.0f, item_y + 2.0f,
                                 menu_width - 4.0f, item_height - 2.0f, highlight);
            }

            // Item text - center vertically in the item
            Color text_color = {0.9f, 0.9f, 0.9f, 1.0f};
            float text_y = item_y + (item_height / 2.0f) - (editor->config->font_size / 2.0f) + 2.0f;
            renderer_add_text(renderer, items[i],
                             menu->x + 10.0f,
                             text_y,
                             text_color);
        }
    }

    // Note: Don't delete text here - it's cached in editor->cached_text
}

// Open file
inline bool editor_open_file(Editor* editor, const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file
    char* buffer = new char[file_size + 1];
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    fclose(file);

    // Replace rope content
    rope_free(&editor->rope);
    rope_from_string(&editor->rope, buffer);
    editor->cursor_pos = 0;
    editor->rope_version++;  // Invalidate cache

    // Store file path
    if (editor->file_path) {
        delete[] editor->file_path;
    }
    editor->file_path = new char[strlen(path) + 1];
    strcpy(editor->file_path, path);

    delete[] buffer;

    printf("Opened file: %s (%zu bytes)\n", path, rope_length(&editor->rope));
    return true;
}

// Save file
inline bool editor_save_file(Editor* editor, const char* path) {
    // Use provided path or current file_path
    const char* save_path = path ? path : editor->file_path;

    if (!save_path) {
        fprintf(stderr, "Error: No file path specified for save\n");
        return false;
    }

    // Convert rope to string
    char* content = rope_to_string(&editor->rope);
    size_t content_length = rope_length(&editor->rope);

    // Write to file
    FILE* file = fopen(save_path, "wb");
    if (!file) {
        fprintf(stderr, "Failed to save file: %s\n", save_path);
        delete[] content;
        return false;
    }

    size_t written = fwrite(content, 1, content_length, file);
    fclose(file);
    delete[] content;

    if (written != content_length) {
        fprintf(stderr, "Error: Only wrote %zu of %zu bytes\n", written, content_length);
        return false;
    }

    // Update file_path if we used a new path
    if (path && (!editor->file_path || strcmp(editor->file_path, path) != 0)) {
        if (editor->file_path) {
            delete[] editor->file_path;
        }
        editor->file_path = new char[strlen(path) + 1];
        strcpy(editor->file_path, path);
    }

    printf("Saved file: %s (%zu bytes)\n", save_path, content_length);
    return true;
}

// Shutdown editor
inline void editor_shutdown(Editor* editor) {
    rope_free(&editor->rope);
    if (editor->file_path) {
        delete[] editor->file_path;
        editor->file_path = nullptr;
    }

    // Clean up undo/redo stacks
    for (auto& cmd : editor->undo_stack) {
        delete[] cmd.content;
    }
    editor->undo_stack.clear();

    for (auto& cmd : editor->redo_stack) {
        delete[] cmd.content;
    }
    editor->redo_stack.clear();

    // Clean up cached text
    if (editor->cached_text) {
        delete[] editor->cached_text;
        editor->cached_text = nullptr;
    }

    // Clean up search state
    if (editor->search_state) {
        if (editor->search_state->match_positions) {
            delete[] editor->search_state->match_positions;
        }
        delete editor->search_state;
        editor->search_state = nullptr;
    }

    // Clean up context menu
    if (editor->context_menu) {
        delete editor->context_menu;
        editor->context_menu = nullptr;
    }

    printf("Editor shutdown\n");
}

// Search functions

// Open search box
inline void editor_search_open(Editor* editor) {
    editor->search_state->active = true;
    printf("[Search] Opened - query: \"%s\"\n", editor->search_state->query);
    // Keep existing query if any
}

// Close search box and clear highlights
inline void editor_search_close(Editor* editor) {
    editor->search_state->active = false;
    editor->search_state->match_count = 0;
}

// Find all matches in rope
inline void editor_search_update_matches(Editor* editor) {
    SearchState* search = editor->search_state;

    // Check if query is empty
    if (search->query_len == 0) {
        search->match_count = 0;
        return;
    }

    // Note: We don't skip if rope hasn't changed because the query itself
    // may have changed. The search must update whenever the query changes.

    // Convert rope to string for searching
    char* text = rope_to_string(&editor->rope);
    size_t text_len = rope_length(&editor->rope);

    // Reset match count
    search->match_count = 0;

    // Simple naive search (good for MVP)
    const char* query = search->query;
    size_t query_len = search->query_len;

    // Check if query is longer than text (prevent underflow)
    if (text_len < query_len) {
        delete[] text;
        return;
    }

    for (size_t i = 0; i <= text_len - query_len; i++) {
        bool match = true;

        // Case-insensitive comparison by default
        for (size_t j = 0; j < query_len; j++) {
            char text_ch = text[i + j];
            char query_ch = query[j];

            if (!search->case_sensitive) {
                text_ch = tolower(text_ch);
                query_ch = tolower(query_ch);
            }

            if (text_ch != query_ch) {
                match = false;
                break;
            }
        }

        if (match) {
            // Grow array if needed
            if (search->match_count >= search->match_capacity) {
                search->match_capacity = search->match_capacity == 0 ?
                    16 : search->match_capacity * 2;
                size_t* new_positions = new size_t[search->match_capacity];
                if (search->match_positions) {
                    memcpy(new_positions, search->match_positions,
                           search->match_count * sizeof(size_t));
                    delete[] search->match_positions;
                }
                search->match_positions = new_positions;
            }

            search->match_positions[search->match_count++] = i;
        }
    }

    delete[] text;

    // Update version and reset index
    search->rope_version_at_search = editor->rope_version;
    search->current_match_index = 0;

    printf("[Search] Query: \"%s\" - Found %zu matches (case_sensitive=%d)\n",
           search->query, search->match_count, search->case_sensitive);

    // Move cursor to first match if any
    if (search->match_count > 0) {
        editor->cursor_pos = search->match_positions[0];
        editor_ensure_cursor_visible(editor);
    }
}

// Navigate to next match
inline void editor_search_next_match(Editor* editor) {
    SearchState* search = editor->search_state;
    if (search->match_count == 0) return;

    search->current_match_index = (search->current_match_index + 1) % search->match_count;
    editor->cursor_pos = search->match_positions[search->current_match_index];
    editor_ensure_cursor_visible(editor);
}

// Navigate to previous match
inline void editor_search_prev_match(Editor* editor) {
    SearchState* search = editor->search_state;
    if (search->match_count == 0) return;

    if (search->current_match_index == 0) {
        search->current_match_index = search->match_count - 1;
    } else {
        search->current_match_index--;
    }

    editor->cursor_pos = search->match_positions[search->current_match_index];
    editor_ensure_cursor_visible(editor);
}

#endif // ZED_EDITOR_H
