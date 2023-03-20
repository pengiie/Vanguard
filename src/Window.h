#pragma once

#include <vector>
#include <string>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Input.h"

#include <vulkan/vulkan.h>

namespace vanguard {
    class Window {
    public:
        ~Window();

        static void initGLFW();
        static void terminateGLFW();

        void init();
        void show();
        void toggleCursor();
        [[nodiscard]] bool isCloseRequested() const;

        inline static void pollEvents() {
            Input::clearInputs();
            glfwPollEvents();
        }
        static float getDeltaTime();

        [[nodiscard]] VkSurfaceKHR getSurface(VkInstance instance) const;

        [[nodiscard]] inline GLFWwindow* getHandle() const { return m_handle; }
        [[nodiscard]] inline uint32_t getWidth() const { return m_width; }
        [[nodiscard]] inline uint32_t getHeight() const { return m_height; }
        [[nodiscard]] static std::vector<std::string> getRequiredExtensions() ;
    private:
        GLFWwindow* m_handle = nullptr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };
}