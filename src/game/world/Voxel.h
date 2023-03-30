#pragma once

#include "glm/vec3.hpp"
#include "../../graphics/RenderGraph.h"

#define VOXEL_SIZE 1.0f

namespace vanguard {
    typedef bool Voxel;

    struct VoxelVertex {
        glm::vec3 position;
        float shading;

        static VertexInputData getVertexInputData() {
            VertexInputData data = VertexInputData::createVertexInputData<VoxelVertex>();
            data.setAttribute(0, offsetof(VoxelVertex, VoxelVertex::position), vk::Format::eR32G32B32Sfloat);
            data.setAttribute(1, offsetof(VoxelVertex, VoxelVertex::shading), vk::Format::eR32Sfloat);
            return data;
        }
    };
}