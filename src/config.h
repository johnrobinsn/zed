// Configuration system
// Loads JSON config files for themes, keybindings, and settings

#ifndef ZED_CONFIG_H
#define ZED_CONFIG_H

#include <cstdio>
#include <cstring>
#include <cstdlib>

// Color structure (RGBA)
struct Color {
    float r, g, b, a;
};

// Configuration structure
struct Config {
    // Font settings
    char font_path[512];
    int font_size;

    // Theme colors
    Color background;
    Color foreground;
    Color cursor;
    Color selection;
    Color search_match_bg;
    Color search_current_match_bg;
    Color search_box_bg;

    // Editor settings
    int tab_width;
    bool use_spaces;
    bool line_wrap;

    // Performance settings
    bool adaptive_vsync;           // Enable adaptive VSync
    bool force_vsync_off;          // Override: always disable VSync
    bool force_vsync_on;           // Override: always enable VSync
    int vsync_hysteresis_frames;   // Frames before switching (default 5)

    // TODO: Keybindings map
};

// Helper: Parse hex color #RRGGBB
inline Color parse_color(const char* hex) {
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};

    if (hex && hex[0] == '#' && strlen(hex) == 7) {
        unsigned int r, g, b;
        sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
        color.r = r / 255.0f;
        color.g = g / 255.0f;
        color.b = b / 255.0f;
    }

    return color;
}

// Set default configuration
inline void config_set_defaults(Config* config) {
    strcpy(config->font_path, "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
    config->font_size = 14;

    config->background = parse_color("#1e1e1e");
    config->foreground = parse_color("#d4d4d4");
    config->cursor = parse_color("#00ff00");
    config->selection = parse_color("#264f78");
    config->search_match_bg = {1.0f, 1.0f, 0.0f, 0.2f};         // Yellow, 20% alpha
    config->search_current_match_bg = {1.0f, 0.5f, 0.0f, 0.4f}; // Orange, 40% alpha
    config->search_box_bg = {0.18f, 0.18f, 0.19f, 0.95f};       // Dark gray

    config->tab_width = 4;
    config->use_spaces = true;
    config->line_wrap = false;

    // Performance defaults
    config->adaptive_vsync = true;
    config->force_vsync_off = false;
    config->force_vsync_on = false;
    config->vsync_hysteresis_frames = 5;
}

// Load configuration from JSON file
// Note: For MVP, using simple manual parsing. Could use a JSON library later.
inline bool config_load(Config* config, const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        return false;
    }

    // For MVP: just set defaults
    // TODO: Implement proper JSON parsing
    config_set_defaults(config);

    fclose(file);
    return true;
}

// Free configuration resources
inline void config_free(Config* config) {
    // Nothing to free for now
    (void)config;
}

#endif // ZED_CONFIG_H
