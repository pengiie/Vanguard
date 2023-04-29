#pragma once

#include <optional>
#include "../Config.h"
#include "../Window.h"
#include "ResourceManager.h"
#include "Stager.h"
#include <vulkan/vulkan_raii.hpp>

#include <mutex>
#include <variant>

namespace vanguard {
    struct FrameData {
        vk::raii::Semaphore imageAvailableSemaphore;
        vk::raii::Semaphore commandsFinishedSemaphore;
        vk::raii::Semaphore blitFinishedSemaphore;
        vk::raii::Fence inFlightFence;

        vk::raii::CommandPool commandPool;
        vk::raii::CommandBuffer generalCommandBuffer;
        vk::raii::CommandBuffer blitCommandBuffer;
    };

    struct RenderPipelineCommand {
        ResourceRef pipeline = UNDEFINED_RESOURCE;
        std::vector<vk::ClearValue> clearValues;
        std::function<void(vk::CommandBuffer)> execution;
    };

    struct ComputePipelineCommand {
        ResourceRef pipeline = UNDEFINED_RESOURCE;
        std::function<void(vk::CommandBuffer)> execution;
    };

    struct ImageBarrierInfo {
        ResourceRef image;
        vk::ImageLayout oldLayout;
        vk::ImageLayout newLayout;
        vk::AccessFlags srcAccessMask;
        vk::AccessFlags dstAccessMask;
    };
    struct BufferBarrierInfo {
        ResourceRef buffer;
        vk::AccessFlags srcAccessMask;
        vk::AccessFlags dstAccessMask;
        vk::DeviceSize offset = 0;
        vk::DeviceSize size = VK_WHOLE_SIZE;
    };
    struct PipelineBarrierCommand {
        vk::PipelineStageFlags srcStage;
        vk::PipelineStageFlags dstStage;
        std::vector<ImageBarrierInfo> imageMemoryBarriers;
        std::vector<BufferBarrierInfo> bufferMemoryBarriers;
    };

    struct GeneralCommand {
        std::function<void(vk::CommandBuffer)> execution;
    };
    typedef std::variant<RenderPipelineCommand, ComputePipelineCommand, PipelineBarrierCommand, GeneralCommand> Command;

    struct CommandsInfo {
        std::vector<Command> commands;
        ResourceRef backbufferImage = UNDEFINED_RESOURCE;
        vk::ImageLayout backbufferImageLayout = vk::ImageLayout::eUndefined;
        vk::AccessFlags backbufferImageAccessMask = vk::AccessFlagBits::eNone;
    };

    class RenderSystem {
    public:
        void init();
        void bakeCommands(const CommandsInfo& commandsInfo);

        void render(Window& window);

        [[nodiscard]] inline uint32_t getFrameCount() const { return m_frameCount; }
        [[nodiscard]] inline uint32_t getFrameIndex() const { return m_currentFrame; }

        [[nodiscard]] inline ResourceManager& getResourceManager() { return m_resourceManager; }
        [[nodiscard]] inline Stager& getStager() { return m_stager; }
    private:
        std::vector<FrameData> m_frameData{};
        uint32_t m_currentFrame = 0;
        uint32_t m_frameCount = 0;

        ResourceManager m_resourceManager;
        Stager m_stager;
        CommandsInfo m_commands;
    };
}