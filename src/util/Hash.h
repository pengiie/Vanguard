#pragma once

#include <unordered_set>
#include <glm/ext.hpp>

template <>
struct std::hash<glm::ivec3> {
    std::size_t operator()(const glm::ivec3& k) const {
        return ((std::hash<int>()(k.x)
            ^ (std::hash<int>()(k.y) << 1)) >> 1)
            ^ (std::hash<int>()(k.z) << 1);
    }
};