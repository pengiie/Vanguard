#version 450 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 1, binding = 0, rgba8) readonly uniform image2D i_image;
layout (set = 1, binding = 1, rgba8) writeonly uniform image2D o_image;

void main() {
    ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);
    vec4 pixel = imageLoad(i_image, storePos);
    imageStore(o_image, storePos, pixel);
}