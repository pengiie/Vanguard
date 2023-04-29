#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include "../ImGuiWindow.h"
#include "../Window.h"

#include <mutex>

namespace vanguard {
    struct SwapchainImage {
        VkImage image;
        //vk::raii::ImageView imageView;
    };

    class Allocator;
    class Vulkan {
    public:
        static void init(const std::vector<std::string>& extensions);
        static void initWindow(VkSurfaceKHR surface, uint32_t width, uint32_t height);
        static void recreateSwapchain(uint32_t width, uint32_t height);

        static void initImGui(ImGuiWindow& window);
        static void beginImGuiFrame();
        static void renderImGuiFrame(ImGuiWindow& window);
        static void destroyImGui(ImGuiWindow& window);

        static vk::raii::Instance& getInstance();
        static vk::raii::PhysicalDevice& getPhysicalDevice();
        static vk::raii::Device& getDevice();
        static vk::raii::Queue& getQueue();
        static uint32_t getQueueFamilyIndex();
        static vk::raii::SwapchainKHR& getSwapchain();
        static vk::Extent2D getSwapchainExtent();
        static std::vector<SwapchainImage>& getSwapchainImages();
        static Allocator& getAllocator();
        static vk::raii::DescriptorPool& getDescriptorPool();
        static std::mutex& getVmaMutex();

        static vk::Format getDepthFormat();
        static uint32_t padUniformBufferSize(uint32_t originalSize);
    };
}