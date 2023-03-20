#version 450 core

flat layout(location = 0) in uint pass_colorIndex;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform ColorPalette{
    vec3 color[256];
} u_colorPalette;

void main() {
    fragColor = vec4(u_colorPalette.color[pass_colorIndex], 1.0);
}