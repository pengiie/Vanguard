#pragma once

#include "glm/vec3.hpp"
#include <algorithm>

namespace vanguard {
    struct AABB {
        AABB() = default;
        AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}
        AABB(const glm::vec3& pointA, const glm::vec3& pointB, const glm::vec3& pointC) {
            min = glm::vec3(
                std::min(pointA.x, std::min(pointB.x, pointC.x)),
                std::min(pointA.y, std::min(pointB.y, pointC.y)),
                std::min(pointA.z, std::min(pointB.z, pointC.z))
            );

            max = glm::vec3(
                std::max(pointA.x, std::max(pointB.x, pointC.x)),
                std::max(pointA.y, std::max(pointB.y, pointC.y)),
                std::max(pointA.z, std::max(pointB.z, pointC.z))
            );
        }

        [[nodiscard]] bool intersects(const AABB& other) const {
            return (min.x <= other.max.x && max.x >= other.min.x) &&
                (min.y <= other.max.y && max.y >= other.min.y) &&
                (min.z <= other.max.z && max.z >= other.min.z);
        }

        glm::vec3 min{};
        glm::vec3 max{};
    };
}