#version 450 core

layout(location = 0) in vec3 vertices;
layout(location = 1) in float shading;

layout(set = 0, binding = 0) uniform CameraUniform{
    mat4 view;
    mat4 projection;
    mat4 viewProj;
} u_camera;

flat layout(location = 0) out uint pass_colorIndex;
flat layout(location = 1) out float pass_shading;

void main() {
    gl_Position = u_camera.viewProj * vec4(vertices, 1.0);

    pass_colorIndex = 1;
    pass_shading = shading;
}