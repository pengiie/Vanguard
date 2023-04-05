#include "Camera.h"

#include "../Application.h"

#include "glm/gtx/transform.hpp"
#include "glm/gtx/vector_angle.hpp"

namespace vanguard {
    void Camera::init() {
        auto& window = Application::Get().getWindow();
        m_perspectiveData = {
                .fov = 45.0f,
                .aspectRatio = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight()),
                .nearPlane = 0.1f,
                .farPlane = 1000.0f
        };

        m_cameraBuffer.create(sizeof(CameraData));
        m_data = {
                .view = glm::mat4(1.0f),
                .projection = createPerspective()
        };
        m_position = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    void Camera::update(float deltaTime) {
        // The following block is movement logic subject to deletion at any time.
        {
            INFO("Position: ({}, {}, {})", m_position.x, m_position.y, m_position.z);
            INFO("Chunk Position: ({}, {}, {})", floor(m_position.x / 16.0f), floor(m_position.y / 16.0f),
                 floor(m_position.z / 16.0f));

            auto direction = glm::vec3(0.0f);
            if (Input::isKeyDown(Key::W)) {
                direction += glm::vec3(0.0f, 0.0f, 1.0f);
            }
            if (Input::isKeyDown(Key::S)) {
                direction += glm::vec3(0.0f, 0.0f, -1.0f);
            }
            if (Input::isKeyDown(Key::A)) {
                direction += glm::vec3(-1.0f, 0.0f, 0.0f);
            }
            if (Input::isKeyDown(Key::D)) {
                direction += glm::vec3(1.0f, 0.0f, 0.0f);
            }
            if (Input::isKeyDown(Key::Space)) {
                direction += glm::vec3(0.0f, 1.0f, 0.0f);
            }
            if (Input::isKeyDown(Key::LeftShift)) {
                direction += glm::vec3(0.0f, -1.0f, 0.0f);
            }
            float speed = 35.0f;
            if (Input::isKeyDown(Key::LeftControl)) {
                speed *= 3.0f;
            }
            if (glm::length(direction) > 0.0f) {
                direction = glm::normalize(direction);
                direction = glm::rotate(direction, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                m_position += direction * speed * deltaTime;
            }

            auto mousePos = Input::getMousePosition();
            if (Input::isCursorDelta()) {
                m_rotation.x += mousePos.y * 0.1f;
                m_rotation.y += mousePos.x * 0.1f;
            }
        }

        m_data.position = glm::vec4(m_position, 0.0f);
        m_data.screenSize = glm::vec4(Application::Get().getWindow().getWidth(), Application::Get().getWindow().getHeight(), 0.0f, 0.0f);
        INFO("Screen Size: ({}, {})", m_data.screenSize.x, m_data.screenSize.y);
        m_data.view = glm::lookAt(m_position, m_position + forward(), glm::vec3(0.0f, -1.0f, 0.0f));
        m_data.projView = m_data.projection * m_data.view;
        m_data.screenToWorld = createToWorld();

        m_cameraBuffer.update(&m_data, sizeof(CameraData));

        m_frustum = createFrustum();
    }

    glm::mat4 Camera::createPerspective() const {
        return glm::perspective(glm::radians(m_perspectiveData.fov), m_perspectiveData.aspectRatio, m_perspectiveData.nearPlane, m_perspectiveData.farPlane);
    }

    glm::mat4 Camera::createToWorld() const {
        float nearWidth1_2 = m_perspectiveData.nearPlane * tan(m_perspectiveData.fov / 2);
        float nearHeight1_2 = nearWidth1_2 / m_perspectiveData.aspectRatio;

        glm::mat4 mat = glm::mat4(1.0f);
        mat = glm::translate(mat, m_position + forward() * m_perspectiveData.nearPlane);
        mat = glm::scale(mat, glm::vec3(nearWidth1_2, nearHeight1_2, 1.0f));
        return mat;
    }

    Frustum Camera::createFrustum() const {
        float farWidth1_2 = m_perspectiveData.farPlane * tan(m_perspectiveData.fov / 2);
        float farHeight1_2 = farWidth1_2 / m_perspectiveData.aspectRatio;
        float nearWidth1_2 = m_perspectiveData.nearPlane * (farWidth1_2 / m_perspectiveData.farPlane);
        float nearHeight1_2 = nearWidth1_2 / m_perspectiveData.aspectRatio;

        glm::vec3 farCenter = m_position + forward() * m_perspectiveData.farPlane;
        glm::vec3 nearCenter = m_position + forward() * m_perspectiveData.nearPlane;

        glm::vec3 farRight = farCenter + (right() * farWidth1_2);
        glm::vec3 farLeft = farCenter - (right() * farWidth1_2);
        glm::vec3 farTop = farCenter + (up() * farHeight1_2);
        glm::vec3 farBottom = farCenter - (up() * farHeight1_2);

        glm::vec3 nearNormal = forward();
        glm::vec3 farNormal = -forward();
        glm::vec3 rightNormal = -glm::cross(up(), glm::normalize(farRight - m_position));
        glm::vec3 leftNormal = glm::cross(up(), glm::normalize(farLeft - m_position));
        glm::vec3 topNormal = glm::cross(right(), glm::normalize(farTop - m_position));
        glm::vec3 bottomNormal = -glm::cross(right(), glm::normalize(farBottom - m_position));

        glm::vec3 farTopRight = farTop + (right() * farWidth1_2);
        glm::vec3 farTopLeft = farTop - (right() * farWidth1_2);
        glm::vec3 nearTopRight = farTopRight - (forward() * m_perspectiveData.farPlane);
        AABB boundingBox(farTopRight, farTopLeft, nearTopRight);

        Frustum frustum;
        frustum.near = { .normal = nearNormal, .point = nearCenter };
        frustum.far = { .normal = farNormal, .point = farCenter };
        frustum.right = { .normal = rightNormal, .point = farRight };
        frustum.left = { .normal = leftNormal, .point = farLeft };
        frustum.top = { .normal = topNormal, .point = farTop };
        frustum.bottom = { .normal = bottomNormal, .point = farBottom };
        frustum.boundingBox = boundingBox;
        return frustum;
    }

    glm::vec3 Camera::forward() const {
        auto forward = glm::vec3(0.0f, 0.0f, 1.0f);
        forward = glm::rotate(forward, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        forward = glm::rotate(forward, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        return forward;
    }

    glm::vec3 Camera::right() const {
        auto right = glm::vec3(1.0f, 0.0f, 0.0f);
        right = glm::rotate(right, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        right = glm::rotate(right, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        return right;
    }

    glm::vec3 Camera::up() const {
        auto up = glm::vec3(0.0f, 1.0f, 0.0f);
        up = glm::rotate(up, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        up = glm::rotate(up, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        return up;
    }
}