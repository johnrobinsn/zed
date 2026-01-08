// OpenGL renderer for text with instanced geometry

#ifndef ZED_RENDERER_H
#define ZED_RENDERER_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

#include "config.h"
#include "font.h"
#include "shaders.h"

// UTF-8 helper: Find start of previous character (move backward to char boundary)
inline size_t utf8_prev_char_boundary(const char* text, size_t pos) {
    if (pos == 0) return 0;

    // Move back at least one byte
    pos--;

    // Keep moving back while we're on a continuation byte (10xxxxxx)
    while (pos > 0 && (text[pos] & 0xC0) == 0x80) {
        pos--;
    }

    return pos;
}

// UTF-8 helper: Find start of next character (move forward to char boundary)
inline size_t utf8_next_char_boundary(const char* text, size_t pos, size_t max_len) {
    if (pos >= max_len) return max_len;

    // Move forward at least one byte
    pos++;

    // Keep moving forward while we're on a continuation byte (10xxxxxx)
    while (pos < max_len && (text[pos] & 0xC0) == 0x80) {
        pos++;
    }

    return pos;
}

// UTF-8 helper: Get byte length of character at position
inline size_t utf8_char_length(const char* text, size_t pos) {
    unsigned char c = text[pos];

    if ((c & 0x80) == 0) return 1;       // 0xxxxxxx - 1 byte
    if ((c & 0xE0) == 0xC0) return 2;    // 110xxxxx - 2 bytes
    if ((c & 0xF0) == 0xE0) return 3;    // 1110xxxx - 3 bytes
    if ((c & 0xF8) == 0xF0) return 4;    // 11110xxx - 4 bytes

    // Invalid UTF-8 start byte, treat as 1 byte
    return 1;
}

// UTF-8 decoder - converts UTF-8 byte sequence to Unicode codepoint
// Returns codepoint and advances pointer past the character
inline uint32_t utf8_decode(const char** p) {
    const unsigned char* s = (const unsigned char*)*p;
    uint32_t codepoint = 0;
    int bytes = 0;

    if (s[0] == 0) {
        return 0;  // End of string
    } else if ((s[0] & 0x80) == 0) {
        // 1-byte ASCII: 0xxxxxxx
        codepoint = s[0];
        bytes = 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        // 2-byte: 110xxxxx 10xxxxxx
        if ((s[1] & 0xC0) == 0x80) {
            codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
            bytes = 2;
        } else {
            // Invalid UTF-8, return replacement character
            codepoint = 0xFFFD;
            bytes = 1;
        }
    } else if ((s[0] & 0xF0) == 0xE0) {
        // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
        if ((s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
            codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
            bytes = 3;
        } else {
            // Invalid UTF-8
            codepoint = 0xFFFD;
            bytes = 1;
        }
    } else if ((s[0] & 0xF8) == 0xF0) {
        // 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
            codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                       ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
            bytes = 4;
        } else {
            // Invalid UTF-8
            codepoint = 0xFFFD;
            bytes = 1;
        }
    } else {
        // Invalid UTF-8 start byte
        codepoint = 0xFFFD;
        bytes = 1;
    }

    *p += bytes;
    return codepoint;
}

// Maximum glyphs per frame
constexpr int MAX_GLYPHS = 100000;

// Zoom constants
constexpr int MIN_FONT_SIZE = 6;     // Minimum readable size
constexpr int MAX_FONT_SIZE = 96;    // Maximum presentation size
constexpr float ZOOM_FACTOR = 1.1f;  // 10% per step

// Glyph instance data (sent to GPU)
struct GlyphInstance {
    float x, y;              // Screen position
    float width, height;     // Glyph size
    float u0, v0, u1, v1;    // Atlas texture rect
    float r, g, b, a;        // Color
};

// Shader program
struct ShaderProgram {
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;

    // Uniform locations
    GLint projection_loc;
    GLint atlas_texture_loc;
};

// Rectangle vertex
struct RectVertex {
    float x, y;
    float r, g, b, a;
};

// Renderer state
struct Renderer {
    int viewport_width;
    int viewport_height;
    Config* config;

    // Font system
    FontSystem font_sys;

    // Zoom state
    int base_font_size;      // Original font size (post-DPI scaling)
    int current_zoom_level;  // Zoom steps: negative = zoom out, positive = zoom in

    // Shaders
    ShaderProgram text_shader;
    ShaderProgram rect_shader;

    // Geometry
    GLuint quad_vao;
    GLuint quad_vbo;
    GLuint instance_vao;
    GLuint instance_vbo;

    // Rectangle rendering
    GLuint rect_vao;
    GLuint rect_vbo;
    std::vector<RectVertex> rect_vertices;

    // Instance data
    std::vector<GlyphInstance> glyph_instances;

    // Projection matrix (orthographic)
    float projection[16];
};

// Compile shader
inline GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, nullptr, info_log);
        fprintf(stderr, "Shader compilation failed:\n%s\n", info_log);
        return 0;
    }

    return shader;
}

// Link shader program
inline GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, nullptr, info_log);
        fprintf(stderr, "Shader linking failed:\n%s\n", info_log);
        return 0;
    }

    return program;
}

// Initialize shader program
inline bool init_shader_program(ShaderProgram* shader, const char* vs_source, const char* fs_source) {
    shader->vertex_shader = compile_shader(GL_VERTEX_SHADER, vs_source);
    if (!shader->vertex_shader) return false;

    shader->fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fs_source);
    if (!shader->fragment_shader) return false;

    shader->program = link_program(shader->vertex_shader, shader->fragment_shader);
    if (!shader->program) return false;

    // Get uniform locations
    shader->projection_loc = glGetUniformLocation(shader->program, "projection");
    shader->atlas_texture_loc = glGetUniformLocation(shader->program, "atlas_texture");

    return true;
}

// Create orthographic projection matrix
inline void create_ortho_matrix(float* matrix, float left, float right, float bottom, float top) {
    memset(matrix, 0, 16 * sizeof(float));

    matrix[0] = 2.0f / (right - left);
    matrix[5] = 2.0f / (top - bottom);
    matrix[10] = -1.0f;
    matrix[12] = -(right + left) / (right - left);
    matrix[13] = -(top + bottom) / (top - bottom);
    matrix[15] = 1.0f;
}

// Initialize renderer
inline bool renderer_init(Renderer* renderer, Config* config) {
    renderer->config = config;
    renderer->viewport_width = 1280;
    renderer->viewport_height = 720;

    // Set up OpenGL state
    glClearColor(
        config->background.r,
        config->background.g,
        config->background.b,
        config->background.a
    );

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initialize font system
    if (!font_system_init(&renderer->font_sys)) {
        return false;
    }

    if (!font_system_load_font(&renderer->font_sys, config->font_path, config->font_size)) {
        fprintf(stderr, "Failed to load font: %s\n", config->font_path);
        return false;
    }

    glyph_atlas_init(&renderer->font_sys.atlas);

    // Initialize zoom state (config->font_size already has DPI scaling applied)
    renderer->base_font_size = config->font_size;
    renderer->current_zoom_level = 0;

    // Initialize shaders
    if (!init_shader_program(&renderer->text_shader, TEXT_VERTEX_SHADER, TEXT_FRAGMENT_SHADER)) {
        fprintf(stderr, "Failed to initialize text shader\n");
        return false;
    }

    if (!init_shader_program(&renderer->rect_shader, RECT_VERTEX_SHADER, RECT_FRAGMENT_SHADER)) {
        fprintf(stderr, "Failed to initialize rect shader\n");
        return false;
    }

    // Create quad geometry (2 triangles for glyph quad)
    float quad_vertices[] = {
        // pos      // uv
        0.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 0.0f,  1.0f, 0.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        0.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &renderer->quad_vao);
    glGenBuffers(1, &renderer->quad_vbo);

    glBindVertexArray(renderer->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Vertex attributes (position + uv)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Create instance buffer
    glGenBuffers(1, &renderer->instance_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_GLYPHS * sizeof(GlyphInstance), nullptr, GL_DYNAMIC_DRAW);

    // Instance attributes (vec2 pos, vec2 size, vec4 atlas_rect, vec4 color)
    size_t offset = 0;

    // glyph_pos (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GlyphInstance), (void*)offset);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    offset += 2 * sizeof(float);

    // glyph_size (location 3)
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(GlyphInstance), (void*)offset);
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);
    offset += 2 * sizeof(float);

    // atlas_rect (location 4)
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(GlyphInstance), (void*)offset);
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);
    offset += 4 * sizeof(float);

    // glyph_color (location 5)
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(GlyphInstance), (void*)offset);
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);

    // Create rectangle VAO/VBO
    glGenVertexArrays(1, &renderer->rect_vao);
    glGenBuffers(1, &renderer->rect_vbo);

    glBindVertexArray(renderer->rect_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, 10000 * sizeof(RectVertex), nullptr, GL_DYNAMIC_DRAW);

    // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RectVertex), (void*)0);
    glEnableVertexAttribArray(0);
    // color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(RectVertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Create projection matrix
    create_ortho_matrix(renderer->projection, 0, renderer->viewport_width, renderer->viewport_height, 0);

    printf("Renderer initialized\n");
    return true;
}

// Resize viewport
inline void renderer_resize(Renderer* renderer, int width, int height) {
    renderer->viewport_width = width;
    renderer->viewport_height = height;
    glViewport(0, 0, width, height);
    create_ortho_matrix(renderer->projection, 0, width, height, 0);
}

// Set zoom level (updates font size and clears atlas)
inline bool renderer_set_zoom(Renderer* renderer, int zoom_level) {
    // Calculate new font size with percentage scaling
    float scale = powf(ZOOM_FACTOR, (float)zoom_level);
    int new_font_size = (int)(renderer->base_font_size * scale + 0.5f);

    // Clamp to limits
    if (new_font_size < MIN_FONT_SIZE) {
        new_font_size = MIN_FONT_SIZE;
        zoom_level = (int)(logf((float)MIN_FONT_SIZE / renderer->base_font_size) / logf(ZOOM_FACTOR));
    }
    if (new_font_size > MAX_FONT_SIZE) {
        new_font_size = MAX_FONT_SIZE;
        zoom_level = (int)(logf((float)MAX_FONT_SIZE / renderer->base_font_size) / logf(ZOOM_FACTOR));
    }

    // No change needed
    if (renderer->current_zoom_level == zoom_level) {
        return true;
    }

    // Resize font system
    if (!font_system_resize(&renderer->font_sys, new_font_size)) {
        return false;
    }

    // Clear glyph atlas (forces re-rasterization)
    glyph_atlas_clear(&renderer->font_sys.atlas);

    renderer->current_zoom_level = zoom_level;
    printf("Zoom: %+d levels (%.0f%%, %dpx)\n", zoom_level, scale * 100.0f, new_font_size);
    return true;
}

// Zoom in
inline void renderer_zoom_in(Renderer* renderer) {
    renderer_set_zoom(renderer, renderer->current_zoom_level + 1);
}

// Zoom out
inline void renderer_zoom_out(Renderer* renderer) {
    renderer_set_zoom(renderer, renderer->current_zoom_level - 1);
}

// Reset zoom to default
inline void renderer_zoom_reset(Renderer* renderer) {
    renderer_set_zoom(renderer, 0);
}

// Begin frame
inline void renderer_begin_frame(Renderer* renderer) {
    glClear(GL_COLOR_BUFFER_BIT);
    renderer->glyph_instances.clear();
    renderer->rect_vertices.clear();
    font_system_begin_frame(&renderer->font_sys);
}

// Add rectangle to render queue
inline void renderer_add_rect(Renderer* renderer, float x, float y, float w, float h, Color color) {
    // Two triangles for rectangle
    RectVertex v1 = {x, y, color.r, color.g, color.b, color.a};
    RectVertex v2 = {x + w, y, color.r, color.g, color.b, color.a};
    RectVertex v3 = {x + w, y + h, color.r, color.g, color.b, color.a};
    RectVertex v4 = {x, y + h, color.r, color.g, color.b, color.a};

    renderer->rect_vertices.push_back(v1);
    renderer->rect_vertices.push_back(v2);
    renderer->rect_vertices.push_back(v3);
    renderer->rect_vertices.push_back(v1);
    renderer->rect_vertices.push_back(v3);
    renderer->rect_vertices.push_back(v4);
}

// Flush rectangles
inline void renderer_flush_rects(Renderer* renderer) {
    if (renderer->rect_vertices.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, renderer->rect_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    renderer->rect_vertices.size() * sizeof(RectVertex),
                    renderer->rect_vertices.data());

    glUseProgram(renderer->rect_shader.program);
    glUniformMatrix4fv(renderer->rect_shader.projection_loc, 1, GL_FALSE, renderer->projection);

    glBindVertexArray(renderer->rect_vao);
    glDrawArrays(GL_TRIANGLES, 0, renderer->rect_vertices.size());
    glBindVertexArray(0);

    renderer->rect_vertices.clear();
}

// Add text to render queue
// Y coordinate is the TOP of the line (not baseline)
// This matches how click detection treats Y coordinates
inline void renderer_add_text(Renderer* renderer, const char* text, float x, float y, Color color) {
    float cursor_x = x;
    // Convert Y from top-of-line to baseline by adding ascent
    float cursor_y = y + renderer->font_sys.ascent;

    static bool first_call = true;
    int char_count = 0;

    const char* p = text;
    while (*p) {
        uint32_t codepoint = utf8_decode(&p);  // Decode UTF-8
        if (codepoint == 0) break;  // End of string
        char_count++;

        // Skip newlines (handle separately)
        if (codepoint == '\n') {
            cursor_x = x;
            cursor_y += renderer->font_sys.line_height;
            continue;
        }

        // Get glyph info
        GlyphInfo* glyph = font_system_get_glyph(&renderer->font_sys, codepoint);
        if (!glyph) {
            if (first_call) {
                printf("Failed to get glyph for '%c' (U+%04X)\n", (char)codepoint, codepoint);
            }
            continue;
        }

        if (first_call && char_count <= 5) {
            printf("Got glyph for '%c': %fx%f, UVs: (%.4f,%.4f)-(%.4f,%.4f)\n",
                   (char)codepoint, glyph->width, glyph->height,
                   glyph->u0, glyph->v0, glyph->u1, glyph->v1);
        }

        // Create instance
        GlyphInstance inst;
        inst.x = cursor_x + glyph->bearing_x;
        inst.y = cursor_y - glyph->bearing_y;
        inst.width = glyph->width;
        inst.height = glyph->height;
        inst.u0 = glyph->u0;
        inst.v0 = glyph->v0;
        inst.u1 = glyph->u1;
        inst.v1 = glyph->v1;
        inst.r = color.r;
        inst.g = color.g;
        inst.b = color.b;
        inst.a = color.a;

        renderer->glyph_instances.push_back(inst);

        cursor_x += glyph->advance_x;
    }

    if (first_call) {
        printf("Added %d glyph instances\n", (int)renderer->glyph_instances.size());
        first_call = false;
    }
}

// Render all queued text
inline void renderer_flush_text(Renderer* renderer) {
    if (renderer->glyph_instances.empty()) return;

    static bool first_flush = true;
    if (first_flush) {
        printf("Flushing %zu glyph instances\n", renderer->glyph_instances.size());
    }

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    renderer->glyph_instances.size() * sizeof(GlyphInstance),
                    renderer->glyph_instances.data());

    // Bind shader and uniforms
    glUseProgram(renderer->text_shader.program);
    glUniformMatrix4fv(renderer->text_shader.projection_loc, 1, GL_FALSE, renderer->projection);

    // Bind atlas texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->font_sys.atlas.texture);
    glUniform1i(renderer->text_shader.atlas_texture_loc, 0);

    // Draw instanced
    glBindVertexArray(renderer->quad_vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, renderer->glyph_instances.size());
    glBindVertexArray(0);

    // Check for OpenGL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("OpenGL error after drawing: 0x%X\n", err);
    }

    if (first_flush) {
        printf("Drew %zu instances\n", renderer->glyph_instances.size());
        first_flush = false;
    }

    renderer->glyph_instances.clear();
}

// Flush both rectangles and text (for layering control)
inline void renderer_flush(Renderer* renderer) {
    renderer_flush_rects(renderer);
    renderer_flush_text(renderer);
}

// End frame
inline void renderer_end_frame(Renderer* renderer) {
    renderer_flush(renderer);  // Flush any remaining geometry
}

// Shutdown renderer
inline void renderer_shutdown(Renderer* renderer) {
    font_system_shutdown(&renderer->font_sys);

    if (renderer->quad_vbo) {
        glDeleteBuffers(1, &renderer->quad_vbo);
        renderer->quad_vbo = 0;
    }

    if (renderer->quad_vao) {
        glDeleteVertexArrays(1, &renderer->quad_vao);
        renderer->quad_vao = 0;
    }

    if (renderer->instance_vbo) {
        glDeleteBuffers(1, &renderer->instance_vbo);
        renderer->instance_vbo = 0;
    }

    if (renderer->text_shader.program) {
        glDeleteProgram(renderer->text_shader.program);
        renderer->text_shader.program = 0;
    }

    printf("Renderer shutdown\n");
}

#endif // ZED_RENDERER_H
