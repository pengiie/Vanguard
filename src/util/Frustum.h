#pragma once

#include "glm/vec3.hpp"
#include "glm/gtx/norm.hpp"
#include "AABB.h"

namespace vanguard {
    struct Plane {
        glm::vec3 normal{};
        glm::vec3 point{};
    };

    struct Frustum {
        Plane top;
        Plane bottom;

        Plane right;
        Plane left;

        Plane far;
        Plane near;
        AABB boundingBox;

        [[nodiscard]] bool isBounded(const AABB& bounds) const {
            if (bounds.intersects(boundingBox))
                return true;
            if (isPointBounded(bounds.min) ||
                isPointBounded(bounds.max) ||
                isPointBounded(glm::vec3(bounds.min.x, bounds.min.y, bounds.max.z)) ||
                isPointBounded(glm::vec3(bounds.min.x, bounds.max.y, bounds.min.z)) ||
                isPointBounded(glm::vec3(bounds.min.x, bounds.max.y, bounds.max.z)) ||
                isPointBounded(glm::vec3(bounds.max.x, bounds.min.y, bounds.min.z)) ||
                isPointBounded(glm::vec3(bounds.max.x, bounds.min.y, bounds.max.z)) ||
                isPointBounded(glm::vec3(bounds.max.x, bounds.max.y, bounds.min.z)))
                return true;
            return false;
        }
    private:
        [[nodiscard]] bool isPointBounded(const glm::vec3& point) const {
            return glm::dot(top.normal, point - top.point) >= 0.0f &&
                glm::dot(bottom.normal, point - bottom.point) >= 0.0f &&
                glm::dot(right.normal, point - right.point) >= 0.0f &&
                glm::dot(left.normal, point - left.point) >= 0.0f &&
                glm::dot(far.normal, point - far.point) >= 0.0f &&
                glm::dot(near.normal, point - near.point) >= 0.0f;
        }
    };
}