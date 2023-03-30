#pragma once

#include <optional>
#include "../Config.h"
#include "../Window.h"
#include "RenderGraph.h"
#include "ResourceManager.h"
#include <vulkan/vulkan_raii.hpp>

#include <mutex>

namespace vanguard {
    struct FrameData {
        vk::raii::Semaphore imageAvailableSemaphore;
        vk::raii::Semaphore swapchainImageTransitionSemaphore;
        vk::raii::Semaphore renderFinishedSemaphore;
        vk::raii::Semaphore blitFinishedSemaphore;
        vk::raii::Fence inFlightFence;

        vk::raii::CommandPool commandPool;
        vk::raii::CommandBuffer contextCommandBuffer;
        vk::raii::CommandBuffer swapchainImageTransitionCommandBuffer;
        vk::raii::CommandBuffer renderCommandBuffer;
        vk::raii::CommandBuffer blitCommandBuffer;
    };

    class RenderSystem {
    public:
        void init();
        void bake(RenderGraphBuilder&& builder);

        void beginFrame();
        void render(Window& window);

        [[nodiscard]] inline uint32_t getFrameCount() const { return m_frameCount; }
        [[nodiscard]] inline uint32_t getFrameIndex() const { return m_currentFrame; }

        [[nodiscard]] inline ResourceManager& getResourceManager() { return m_resourceManager; }

        inline void rebake() { bake(std::move(m_renderGraphBuilder)); }
    private:
        std::vector<FrameData> m_frameData{};
        uint32_t m_currentFrame = 0;
        uint32_t m_frameCount = 0;

        ResourceManager m_resourceManager;

        // Render graph
        std::unique_ptr<RenderGraph> m_renderGraph;
        RenderGraphBuilder m_renderGraphBuilder;
    };
}