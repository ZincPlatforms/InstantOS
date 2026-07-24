#version 450

// Textured-quad fragment shader: samples the bound window-surface texture.
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D uTexture;

void main() {
    outColor = texture(uTexture, vUV);
}
