#version 450

// Composites a window/layer texture at its screen rectangle. The destination
// rect and target size are passed as INTEGERS in a storage buffer (binding 1)
// so the guest kernel needs no floating-point math; the GPU converts to NDC.
// Uses gl_VertexIndex (no vertex buffer); sampler is at binding 0 (quad.frag).
layout(location = 0) out vec2 vUV;

layout(std430, binding = 1) readonly buffer QuadRect {
    uint dstX;
    uint dstY;
    uint dstW;
    uint dstH;
    uint targetW;
    uint targetH;
} r;

void main() {
    vec2 corners[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
        vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
    );
    vec2 t = corners[gl_VertexIndex];
    vec2 px = vec2(float(r.dstX), float(r.dstY)) + t * vec2(float(r.dstW), float(r.dstH));
    vec2 ndc = px / vec2(float(r.targetW), float(r.targetH)) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = t;
}
