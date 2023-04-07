#version 450 core

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0, std140) uniform CameraUniform{
    vec3 position;
    vec2 screenSize;

    mat4 view;
    mat4 projection;
    mat4 viewProj;
    mat4 toWorld;
} u_camera;

layout (set = 0, binding = 1, rgba8) uniform writeonly image2D u_image;

const vec3 lightPos = vec3(5.0, 3.0, 0.0);
const vec3 spherePos = vec3(2.0, 0.0, 7.0);
const float radius = 2.0f;

struct Ray {
    vec3 origin;
    vec3 dir;
};

float signedDistance(Ray ray) {
    return length(ray.origin - spherePos) - radius;
}

vec4 rayMarch(Ray ray) {
    float distance = 0.0f;
    while(distance < 100.0f) {
        distance = signedDistance(ray);
        if(distance < 0.001f) {
            vec3 normal = normalize(ray.origin - spherePos);
            vec3 lightDir = normalize(lightPos - ray.origin);
            float diffuse = max(dot(normal, lightDir), 0.0f);

            float shading = max(diffuse, 0.1f);
            return vec4(1.0f, 1.0f, 1.0f, 1.0f) * shading;
        }
        ray.origin += ray.dir * distance;
    }
    return vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(pos) / u_camera.screenSize;
    if(uv.x > 1.0f || uv.y > 1.0f) return;

    vec2 screenPos = uv * 2.0f - 1.0f;
    vec3 worldPos = vec3(u_camera.toWorld * vec4(screenPos, 0.0f, 1.0f));
    vec3 rayDir = normalize(worldPos);

    Ray ray = Ray(u_camera.position, rayDir);
    vec4 color = rayMarch(ray);

    imageStore(u_image, pos, color);
}