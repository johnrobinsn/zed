// FreeType font loading and glyph atlas management
// Dynamic atlas with LRU eviction, subpixel AA

#ifndef ZED_FONT_H
#define ZED_FONT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H

#include <GL/gl.h>
#include <cstdio>
#include <cstring>
#include <unordered_map>

// Atlas configuration
constexpr int ATLAS_WIDTH = 2048;
constexpr int ATLAS_HEIGHT = 2048;
constexpr int GLYPH_PADDING = 2;  // Padding between glyphs

// Glyph info
struct GlyphInfo {
    // Atlas texture coordinates (normalized 0-1)
    float u0, v0, u1, v1;

    // Glyph metrics
    float advance_x;
    float bearing_x;
    float bearing_y;
    float width;
    float height;

    // LRU tracking
    uint32_t last_used_frame;
    bool in_atlas;
};

// Glyph atlas
struct GlyphAtlas {
    GLuint texture;
    unsigned char* buffer;  // RGB buffer for subpixel AA
    int current_x;
    int current_y;
    int current_row_height;
    uint32_t frame_counter;

    // Glyph cache: codepoint -> GlyphInfo
    std::unordered_map<uint32_t, GlyphInfo> glyphs;
};

// Font system
struct FontSystem {
    FT_Library ft_library;
    FT_Face face;
    GlyphAtlas atlas;
    int font_size;
    float line_height;
};

// Initialize FreeType library
inline bool font_system_init(FontSystem* font_sys) {
    if (FT_Init_FreeType(&font_sys->ft_library)) {
        fprintf(stderr, "Failed to initialize FreeType\n");
        return false;
    }

    // Enable LCD filter for subpixel AA
    FT_Library_SetLcdFilter(font_sys->ft_library, FT_LCD_FILTER_DEFAULT);

    printf("FreeType initialized\n");
    return true;
}

// Load font from file
inline bool font_system_load_font(FontSystem* font_sys, const char* font_path, int font_size) {
    if (FT_New_Face(font_sys->ft_library, font_path, 0, &font_sys->face)) {
        fprintf(stderr, "Failed to load font: %s\n", font_path);
        return false;
    }

    // Set font size (in 1/64th of points)
    FT_Set_Pixel_Sizes(font_sys->face, 0, font_size);

    font_sys->font_size = font_size;
    font_sys->line_height = font_sys->face->size->metrics.height / 64.0f;

    printf("Loaded font: %s (size: %d, line height: %.1f)\n",
           font_path, font_size, font_sys->line_height);

    return true;
}

// Initialize glyph atlas
inline void glyph_atlas_init(GlyphAtlas* atlas) {
    atlas->current_x = GLYPH_PADDING;
    atlas->current_y = GLYPH_PADDING;
    atlas->current_row_height = 0;
    atlas->frame_counter = 0;

    // Allocate single-channel buffer for grayscale (simpler for debugging)
    atlas->buffer = new unsigned char[ATLAS_WIDTH * ATLAS_HEIGHT];
    memset(atlas->buffer, 0, ATLAS_WIDTH * ATLAS_HEIGHT);

    // Create OpenGL texture
    glGenTextures(1, &atlas->texture);
    glBindTexture(GL_TEXTURE_2D, atlas->texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload empty texture (RED = single channel)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT,
                 0, GL_RED, GL_UNSIGNED_BYTE, atlas->buffer);

    glBindTexture(GL_TEXTURE_2D, 0);

    printf("Glyph atlas created (%dx%d grayscale)\n", ATLAS_WIDTH, ATLAS_HEIGHT);
}

// Add glyph to atlas
inline bool glyph_atlas_add_glyph(GlyphAtlas* atlas, FT_Face face, uint32_t codepoint) {
    // Load glyph (grayscale for now)
    FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
        fprintf(stderr, "Failed to load glyph: U+%04X\n", codepoint);
        return false;
    }

    FT_GlyphSlot slot = face->glyph;
    FT_Bitmap* bitmap = &slot->bitmap;

    // Check if glyph fits in current row
    int glyph_width = bitmap->width;
    int glyph_height = bitmap->rows;

    if (atlas->current_x + glyph_width + GLYPH_PADDING > ATLAS_WIDTH) {
        // Move to next row
        atlas->current_x = GLYPH_PADDING;
        atlas->current_y += atlas->current_row_height + GLYPH_PADDING;
        atlas->current_row_height = 0;
    }

    if (atlas->current_y + glyph_height + GLYPH_PADDING > ATLAS_HEIGHT) {
        fprintf(stderr, "Atlas full! Need to implement LRU eviction.\n");
        return false;
    }

    // Copy glyph to atlas buffer (grayscale)
    int non_zero_pixels = 0;
    for (int y = 0; y < glyph_height; y++) {
        for (int x = 0; x < glyph_width; x++) {
            int atlas_x = atlas->current_x + x;
            int atlas_y = atlas->current_y + y;
            int atlas_idx = atlas_y * ATLAS_WIDTH + atlas_x;
            int bitmap_idx = y * bitmap->pitch + x;

            // Copy grayscale value
            if (bitmap_idx < (int)(bitmap->rows * bitmap->pitch)) {
                unsigned char pixel = bitmap->buffer[bitmap_idx];
                atlas->buffer[atlas_idx] = pixel;
                if (pixel > 0) non_zero_pixels++;
            }
        }
    }

    static int glyph_count = 0;
    if (glyph_count < 5) {
        printf("Glyph '%c': copied %dx%d, %d non-zero pixels, first pixel=%d\n",
               (char)codepoint, glyph_width, glyph_height, non_zero_pixels,
               glyph_height > 0 && glyph_width > 0 ? bitmap->buffer[0] : 0);
        glyph_count++;
    }

    // Update entire texture
    glBindTexture(GL_TEXTURE_2D, atlas->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT,
                 0, GL_RED, GL_UNSIGNED_BYTE, atlas->buffer);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Store glyph info
    GlyphInfo info;
    info.u0 = (float)atlas->current_x / ATLAS_WIDTH;
    info.v0 = (float)atlas->current_y / ATLAS_HEIGHT;
    info.u1 = (float)(atlas->current_x + glyph_width) / ATLAS_WIDTH;
    info.v1 = (float)(atlas->current_y + glyph_height) / ATLAS_HEIGHT;

    info.advance_x = slot->advance.x / 64.0f;
    info.bearing_x = slot->bitmap_left;
    info.bearing_y = slot->bitmap_top;
    info.width = glyph_width;
    info.height = glyph_height;

    info.last_used_frame = atlas->frame_counter;
    info.in_atlas = true;

    atlas->glyphs[codepoint] = info;

    // Update atlas state
    atlas->current_x += glyph_width + GLYPH_PADDING;
    if (glyph_height > atlas->current_row_height) {
        atlas->current_row_height = glyph_height;
    }

    return true;
}

// Get glyph info (loads if not in atlas)
inline GlyphInfo* font_system_get_glyph(FontSystem* font_sys, uint32_t codepoint) {
    GlyphAtlas* atlas = &font_sys->atlas;

    // Check if already in atlas
    auto it = atlas->glyphs.find(codepoint);
    if (it != atlas->glyphs.end()) {
        it->second.last_used_frame = atlas->frame_counter;
        return &it->second;
    }

    // Not in atlas, add it
    if (glyph_atlas_add_glyph(atlas, font_sys->face, codepoint)) {
        return &atlas->glyphs[codepoint];
    }

    return nullptr;
}

// Begin frame (increment frame counter for LRU)
inline void font_system_begin_frame(FontSystem* font_sys) {
    font_sys->atlas.frame_counter++;
}

// Shutdown font system
inline void font_system_shutdown(FontSystem* font_sys) {
    if (font_sys->atlas.buffer) {
        delete[] font_sys->atlas.buffer;
        font_sys->atlas.buffer = nullptr;
    }

    if (font_sys->atlas.texture) {
        glDeleteTextures(1, &font_sys->atlas.texture);
        font_sys->atlas.texture = 0;
    }

    if (font_sys->face) {
        FT_Done_Face(font_sys->face);
        font_sys->face = nullptr;
    }

    if (font_sys->ft_library) {
        FT_Done_FreeType(font_sys->ft_library);
        font_sys->ft_library = nullptr;
    }

    printf("Font system shutdown\n");
}

#endif // ZED_FONT_H
