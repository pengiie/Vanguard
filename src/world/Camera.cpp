#include "Camera.h"

#include "../Application.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace vanguard {
    static glm::mat4 createPerspective() {
        auto& window = Application::Get().getWindow();
        float aspectRatio = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());

        return glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    }

    void Camera::init() {
        auto& renderSystem = Application::Get().getRenderSystem();

        m_cameraBuffer = renderSystem.createUniformBuffer(sizeof(CameraData));
        m_data = {
                .view = glm::mat4(1.0f),
                .projection = createPerspective()
        };
        m_position = glm::vec3(0.0f, 0.0f, -1.0f);
    }

    void Camera::update(float deltaTime) {
        INFO("Position: ({}, {}, {})", m_position.x, m_position.y, m_position.z);

        auto direction = glm::vec3(0.0f);
        if(Input::isKeyDown(Key::W)) {
            direction += glm::vec3(0.0f, 0.0f, 1.0f);
        }
        if(Input::isKeyDown(Key::S)) {
            direction += glm::vec3(0.0f, 0.0f, -1.0f);
        }
        if(Input::isKeyDown(Key::A)) {
            direction += glm::vec3(-1.0f, 0.0f, 0.0f);
        }
        if(Input::isKeyDown(Key::D)) {
            direction += glm::vec3(1.0f, 0.0f, 0.0f);
        }
        if(Input::isKeyDown(Key::Space)) {
            direction += glm::vec3(0.0f, 1.0f, 0.0f);
        }
        if(Input::isKeyDown(Key::LeftShift)) {
            direction += glm::vec3(0.0f, -1.0f, 0.0f);
        }
        float speed = 5.0f;
        if(Input::isKeyDown(Key::LeftControl)) {
            speed *= 2.5f;
        }
        if(glm::length(direction) > 0.0f) {
            direction = glm::normalize(direction);
            direction = glm::rotate(direction, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            m_position += direction * speed * deltaTime;
        }

        auto mousePos = Input::getMousePosition();
        if(Input::isCursorDelta()) {
            m_rotation.x += mousePos.y * 0.1f;
            m_rotation.y += mousePos.x * 0.1f;
        }

        auto front = glm::vec3(0.0f, 0.0f, 1.0f);
        front = glm::rotate(front, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        front = glm::rotate(front, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));

        m_data.view = glm::lookAt(m_position, m_position + front, glm::vec3(0.0f, -1.0f, 0.0f));
        m_data.projView = m_data.projection * m_data.view;

        auto& renderSystem = Application::Get().getRenderSystem();
        renderSystem.updateBuffer(m_cameraBuffer, m_data, true);
    }
}