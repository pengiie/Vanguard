#pragma once

#include <vulkan/vulkan.h>
#include <backends/imgui_impl_vulkan.h>
#include "GLFW/glfw3.h"

namespace vanguard {
    class ImGuiWindow {
    public:
        ~ImGuiWindow();

        void init();
        void show();

        [[nodiscard]] VkSurfaceKHR getSurface(VkInstance instance) const;

        [[nodiscard]] inline GLFWwindow* getHandle() const { return m_handle; }
        [[nodiscard]] inline ImGui_ImplVulkanH_Window& getImGuiWindowData() { return m_imGuiWindowData; }
        [[nodiscard]] inline uint32_t getWidth() const { return m_width; }
        [[nodiscard]] inline uint32_t getHeight() const { return m_height; }
    private:
        GLFWwindow* m_handle = nullptr;
        ImGui_ImplVulkanH_Window m_imGuiWindowData;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };
}