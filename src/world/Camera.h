#pragma once

#include <glm/glm.hpp>

#include "../graphics/RenderSystem.h"

namespace vanguard {
    struct CameraData {
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 projView;
    };

    class Camera {
    public:
        void init();
        void update(float deltaTime);

        [[nodiscard]] inline BufferRef getCameraBuffer() const { return m_cameraBuffer; }
        [[nodiscard]] inline const glm::vec3& getPosition() const { return m_position; }
        [[nodiscard]] inline const glm::vec3& getRotation() const { return m_rotation; }
    private:
        BufferRef m_cameraBuffer;
        CameraData m_data;

        glm::vec3 m_position;
        glm::vec3 m_rotation;
    };
}