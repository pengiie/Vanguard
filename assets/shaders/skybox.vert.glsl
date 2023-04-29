#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 uv;

layout(location = 0) out vec3 p_uv;

layout(set = 0, binding = 0, std140) uniform CameraUniform{
    vec3 position;
    vec2 screenSize;

    mat4 view;
    mat4 projection;
    mat4 viewProj;
    mat4 toWorld;
} u_camera;

void main() {
    mat4 v = u_camera.view;
    v[3][0] = 0;
    v[3][1] = 0;
    v[3][2] = 0;
    gl_Position = u_camera.projection * v * vec4(position, 1.0);
    p_uv = uv;
}