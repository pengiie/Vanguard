#version 450 core

flat layout(location = 0) in uint pass_colorIndex;
flat layout(location = 1) in uint pass_normal;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform ColorPalette{
    vec3 color[256];
} u_colorPalette;

void main() {
    float shading = 1.0;
    if(pass_normal > 1 && pass_normal <= 3)
        shading = 0.75;
    if(pass_normal > 3 && pass_normal <= 5)
        shading = 0.4;
    fragColor = vec4(u_colorPalette.color[pass_colorIndex] * shading, 1.0);
}