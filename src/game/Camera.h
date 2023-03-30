#pragma once

#include "glm/glm.hpp"

#include "../graphics/RenderSystem.h"
#include "../util/Frustum.h"

namespace vanguard {
    struct CameraData {
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 projView;
    };

    struct PerspectiveData {
        float fov;
        float aspectRatio;
        float nearPlane;
        float farPlane;
    };

    class Camera {
    public:
        void init();
        void update(float deltaTime);

        [[nodiscard]] inline const UniformBuffer& getCameraBuffer() const { return m_cameraBuffer; }
        [[nodiscard]] inline const glm::vec3& getPosition() const { return m_position; }
        [[nodiscard]] inline const glm::vec3& getRotation() const { return m_rotation; }
        [[nodiscard]] inline const Frustum& getFrustum() const { return m_frustum; }
    private:
        [[nodiscard]] glm::mat4 createPerspective() const;
        [[nodiscard]] Frustum createFrustum() const;
        [[nodiscard]] glm::vec3 forward() const;
        [[nodiscard]] glm::vec3 right() const;
        [[nodiscard]] glm::vec3 up() const;
    private:
        UniformBuffer m_cameraBuffer;
        CameraData m_data;

        PerspectiveData m_perspectiveData;
        Frustum m_frustum;

        glm::vec3 m_position;
        glm::vec3 m_rotation;
    };
}