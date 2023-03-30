#version 450 core

flat layout(location = 0) in uint pass_colorIndex;
flat layout(location = 1) in float pass_shading;

layout(location = 0) out vec4 fragColor;

//layout(set = 0, binding = 1) uniform ColorPalette{
//    vec3 color[256];
//} u_colorPalette;

void main() {
    fragColor = vec4(vec3(0, 1, 0) * pass_shading, 1.0);
}