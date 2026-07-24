#version 450

// Textured-quad vertex shader for the Venus GPU compositor. Emits a fullscreen
// quad (two triangles, 6 vertices) using gl_VertexIndex so no vertex buffer is
// needed, and passes through UVs for sampling the window-surface texture.
layout(location = 0) out vec2 vUV;

void main() {
    vec2 positions[6] = vec2[](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
    );
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = pos * 0.5 + 0.5;
}
