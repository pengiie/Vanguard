#version 450 core

layout(location = 0) in vec3 p_uv;

layout(location = 0) out vec4 o_color;

layout(set = 0, binding = 2) uniform samplerCube u_texture;

const vec3 skyColor = vec3(0.52, 0.80, 0.92);

void main() {
    float y = p_uv.y * 0.5 + 0.5;

    float shading = y;

    o_color = vec4(skyColor * shading, 1.0);
}