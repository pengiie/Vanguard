#include "Window.h"
#include "Config.h"
#include "Logger.h"

namespace vanguard {
    Window::~Window() {
        glfwDestroyWindow(m_handle);
    }

    void Window::initGLFW() {
        if(!glfwInit()) {
            ERROR("Failed to initialize GLFW!");
            return;
        }
        if(!glfwVulkanSupported()) {
            ERROR("Vulkan is not supported!");
            return;
        }
    }

    void Window::terminateGLFW() {
        glfwTerminate();
    }

    void Window::init() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

        m_handle = glfwCreateWindow(1280, 720, WINDOW_NAME, nullptr, nullptr);
        glfwSetWindowUserPointer(m_handle, this);

        glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if(glfwRawMouseMotionSupported())
            glfwSetInputMode(m_handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

        glfwSetWindowPos(m_handle, 320, 180);

        glfwSetKeyCallback(m_handle, Input::keyCallback);
        glfwGetFramebufferSize(m_handle, (int*) &m_width, (int*) &m_height);

        glfwSetWindowSizeCallback(m_handle, [](GLFWwindow* window, int width, int height) {
            auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
            self->m_width = width;
            self->m_height = height;
        });
    }

    void Window::show() {
        glfwShowWindow(m_handle);
    }

    bool Window::isCloseRequested() const {
        return glfwWindowShouldClose(m_handle);
    }

    void Window::toggleCursor() {
        if(glfwGetInputMode(m_handle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
            glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    std::vector<std::string> Window::getRequiredExtensions() {
        uint32_t extensionCount;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        std::vector<std::string> result;
        for(uint32_t i = 0; i < extensionCount; i++) {
            result.emplace_back(extensions[i]);
        }
        return result;
    }

    VkSurfaceKHR Window::getSurface(VkInstance instance) const {
        VkSurfaceKHR surface;
        if(glfwCreateWindowSurface(instance, m_handle, nullptr, &surface) != VK_SUCCESS) {
            ERROR("Failed to create window surface!");
            return VK_NULL_HANDLE;
        }
        return surface;
    }

    float Window::getDeltaTime() {
        static double lastTime = glfwGetTime();
        double currentTime = glfwGetTime();
        if(currentTime - lastTime > 0.0f) {
            auto deltaTime = (float) (currentTime - lastTime);
            lastTime = currentTime;
            return deltaTime;
        }
        return 0;
    }
}