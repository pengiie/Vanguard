#version 450 core

layout(location = 0) in vec3 p_normal;
layout(location = 1) in vec2 p_uv;
layout(location = 2) in vec3 p_position;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0, std140) uniform CameraUniform{
    vec3 position;
    vec2 screenSize;

    mat4 view;
    mat4 projection;
    mat4 viewProj;
    mat4 toWorld;
} u_camera;
layout(set = 0, binding = 1) uniform sampler2D u_texture;
layout(set = 0, binding = 2) uniform samplerCube u_skybox;

const vec3 skyColor = vec3(0.52, 0.80, 0.92);
const vec3 sunDir = normalize(vec3(0.0, 1.0, 0.0));

void main() {
    vec3 color = texture(u_texture, p_uv).rgb;
    //color = mix(color, texture(u_skybox, dir).rgb, 1 - blendFactor);
    //dir.y *= -1.0;

    float shading = min(0.0, dot(p_normal, sunDir));
    shading = pow(shading, 2.0);

    color = mix(color, vec3(1.0, 1.0, 0.0), shading) * max(0.2, shading);


    fragColor = vec4(color, 1.0);
}