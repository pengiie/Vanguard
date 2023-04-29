#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 p_normal;
layout(location = 1) out vec2 p_uv;
layout(location = 2) out vec3 p_position;

layout(set = 0, binding = 0, std140) uniform CameraUniform{
    vec3 position;
    vec2 screenSize;

    mat4 view;
    mat4 projection;
    mat4 viewProj;
    mat4 toWorld;
} u_camera;

const mat3 transform = mat3(100.0, 0.0,  0.0,
0.0,  100.0, 0.0,
0.0,  0.0,  100.0);

void main() {
    p_position = transform * position;
    gl_Position = u_camera.viewProj * vec4(p_position, 1.0);
    p_normal = normal;
    p_uv = uv;
}