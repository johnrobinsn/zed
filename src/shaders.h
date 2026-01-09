// GLSL shaders for text rendering
// Embedded as string literals

#ifndef ZED_SHADERS_H
#define ZED_SHADERS_H

// Vertex shader for instanced glyph rendering
// Per-instance attributes: position (x, y), atlas texcoords (u, v, s, t), color (rgba)
const char* TEXT_VERTEX_SHADER = R"(
#version 330 core

// Vertex attributes (quad corners)
layout(location = 0) in vec2 vertex_pos;    // Quad vertex position (0-1)
layout(location = 1) in vec2 vertex_uv;     // Quad texture coords (0-1)

// Instance attributes
layout(location = 2) in vec2 glyph_pos;     // Screen position
layout(location = 3) in vec2 glyph_size;    // Glyph size in pixels
layout(location = 4) in vec4 atlas_rect;    // Atlas UV rect (u, v, w, h)
layout(location = 5) in vec4 glyph_color;   // Glyph color

// Outputs to fragment shader
out vec2 frag_uv;
out vec4 frag_color;

// Uniforms
uniform mat4 projection;

void main() {
    // Calculate final position
    vec2 pos = glyph_pos + vertex_pos * glyph_size;
    gl_Position = projection * vec4(pos, 0.0, 1.0);

    // Calculate texture coordinates
    // atlas_rect is (u0, v0, u1, v1), interpolate between them
    vec2 uv_min = atlas_rect.xy;
    vec2 uv_max = atlas_rect.zw;
    frag_uv = mix(uv_min, uv_max, vertex_uv);

    // Pass color
    frag_color = glyph_color;
}
)";

// Fragment shader for grayscale anti-aliased text
const char* TEXT_FRAGMENT_SHADER = R"(
#version 330 core

in vec2 frag_uv;
in vec4 frag_color;

out vec4 out_color;

uniform sampler2D atlas_texture;

void main() {
    // Sample atlas (grayscale alpha)
    float alpha = texture(atlas_texture, frag_uv).r;

    // Output text color with alpha for proper blending
    // OpenGL's blending will handle compositing with what's already drawn
    out_color = vec4(frag_color.rgb, alpha);
}
)";

// Simple vertex shader for selection rectangles
const char* RECT_VERTEX_SHADER = R"(
#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;

out vec4 frag_color;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(position, 0.0, 1.0);
    frag_color = color;
}
)";

// Simple fragment shader for solid colors
const char* RECT_FRAGMENT_SHADER = R"(
#version 330 core

in vec4 frag_color;
out vec4 out_color;

void main() {
    out_color = frag_color;
}
)";

#endif // ZED_SHADERS_H
