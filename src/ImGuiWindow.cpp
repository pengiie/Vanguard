#include "ImGuiWindow.h"
#include "Logger.h"

namespace vanguard {
    ImGuiWindow::~ImGuiWindow() {
        glfwDestroyWindow(m_handle);
    }

    void ImGuiWindow::init() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);

        m_handle = glfwCreateWindow(800, 400, "Vanguard - Debug", nullptr, nullptr);
        glfwSetWindowUserPointer(m_handle, this);

        glfwGetFramebufferSize(m_handle, (int*) &m_width, (int*) &m_height);

        glfwSetWindowCloseCallback(m_handle, [](GLFWwindow* window) {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
        });
        glfwSetWindowSizeCallback(m_handle, [](GLFWwindow* window, int width, int height) {
            auto* self = reinterpret_cast<ImGuiWindow*>(glfwGetWindowUserPointer(window));
            self->m_width = width;
            self->m_height = height;
        });
    }

    void ImGuiWindow::show() {
        glfwShowWindow(m_handle);
    }

    VkSurfaceKHR ImGuiWindow::getSurface(VkInstance instance) const {
        VkSurfaceKHR surface;
        if(glfwCreateWindowSurface(instance, m_handle, nullptr, &surface) != VK_SUCCESS) {
            ERROR("Failed to create imgui window surface!");
            return VK_NULL_HANDLE;
        }
        return surface;
    }
}