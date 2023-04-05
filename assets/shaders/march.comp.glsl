#version 450 core

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform CameraUniform{
    vec4 position;
    vec4 screenSize;

    mat4 view;
    mat4 projection;
    mat4 viewProj;
    mat4 toWorld;
} u_camera;

layout (set = 0, binding = 1, rgba8) uniform writeonly image2D u_image;

const vec3 lightPos = vec3(2.0, 2.0, 2.0);
const vec3 spherePos = vec3(0.0, 0.0, 0.0);

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(pos) / u_camera.screenSize.xy;
    if(uv.x > 1.0f || uv.y > 1.0f) return;

    vec2 screenPos = uv * 2.0f - 1.0f;
    vec3 worldPos = vec3(u_camera.toWorld * vec4(screenPos, 0.0f, 1.0f));
    vec3 rayDir = normalize(worldPos - u_camera.position.xyz);

    imageStore(u_image, pos, vec4(rayDir, 1.0f));
}