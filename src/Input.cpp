#include "Input.h"
#include "Application.h"

namespace vanguard {
    bool Input::isKeyPressed(KeyCode key) {
        return get().m_pressed.contains(key);
    }

    bool Input::isKeyDown(KeyCode key) {
        return glfwGetKey(Application::Get().getWindow().getHandle(), (int) key) == GLFW_PRESS;
    }

    bool Input::isKeyReleased(KeyCode key) {
        return get().m_released.contains(key);
    }

    glm::vec2 Input::getMousePosition() {
        double x, y;
        glfwGetCursorPos(Application::Get().getWindow().getHandle(), &x, &y);
        return { x, y };
    }

    bool Input::isCursorDelta() {
        return glfwGetInputMode(Application::Get().getWindow().getHandle(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    }

    void Input::clearInputs() {
        get().m_pressed.clear();
        get().m_released.clear();
        if(Input::isCursorDelta())
            glfwSetCursorPos(Application::Get().getWindow().getHandle(), 0, 0);
    }

    void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if(action == GLFW_PRESS) {
            get().m_pressed.insert((KeyCode) key);
        } else if(action == GLFW_RELEASE) {
            get().m_released.insert((KeyCode) key);
        }
    }

    Input& Input::get() {
        static Input instance;
        return instance;
    }

}