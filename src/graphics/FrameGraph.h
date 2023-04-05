#pragma once

#include <utility>

#include "FrameGraphBuilder.h"
#include "Vulkan.h"

namespace vanguard {
    struct Image {
        ImageInfo info;
        vk::raii::Image image;
        vk::raii::ImageView view;
        Allocation allocation;
    };

    struct DescriptorSetInfo {
        std::unordered_map<std::string, UniformBufferInfo> uniformBuffers;
        std::unordered_map<std::string, UniformImageInfo> uniformImages;
    };

    struct DescriptorSet {
        DescriptorSetInfo info;
        vk::raii::DescriptorSetLayout layout;
        std::vector<vk::raii::DescriptorSet> sets;
    };

    struct TransferPass {
        TransferPassInfo info;
    };

    // <name, shouldClear>
    typedef std::pair<std::string, bool> ImageAttachment;
    struct RenderPassCreateInfo {
        RenderPassInfo info;
        std::vector<ImageAttachment> inputAttachments;
        std::vector<ImageAttachment> colorAttachments;
        std::optional<ImageAttachment> depthAttachment;
    };

    struct RenderPass {
        RenderPassInfo info;
        vk::raii::Pipeline pipeline;
        vk::raii::PipelineLayout pipelineLayout;
        vk::raii::RenderPass renderPass;
        vk::raii::Framebuffer framebuffer;
        std::vector<vk::ClearValue> clearValues;
    };

    struct ComputePass {
        ComputePassInfo info;
        vk::raii::Pipeline pipeline;
        vk::raii::PipelineLayout pipelineLayout;
    };

    enum class CommandType {
        TransferPass,
        RenderPass,
        ComputePass,
        PipelineBarrier
    };

    struct Command {
        virtual ~Command() = default;
        [[nodiscard]] virtual CommandType getType() const = 0;
    };

    struct TransferPassCommand : public Command {
        std::string passName;
        explicit TransferPassCommand(std::string_view passName) : passName(passName) { }
        [[nodiscard]] CommandType getType() const override { return CommandType::TransferPass; }
    };

    struct RenderPassCommand : public Command {
        std::string passName;
        explicit RenderPassCommand(std::string_view passName) : passName(passName) { }
        [[nodiscard]] CommandType getType() const override { return CommandType::RenderPass; }
    };

    struct ComputePassCommand : public Command {
        std::string passName;
        explicit ComputePassCommand(std::string_view passName) : passName(passName) { }
        [[nodiscard]] CommandType getType() const override { return CommandType::ComputePass; }
    };

    struct ImageMemoryBarrier {
        std::string image;
        vk::ImageLayout oldLayout;
        vk::ImageLayout newLayout;
        vk::AccessFlags srcAccessMask;
        vk::AccessFlags dstAccessMask;
        vk::ImageAspectFlags aspectMask;
    };
    struct PipelineBarrierInfo {
        vk::PipelineStageFlags srcStageMask;
        vk::PipelineStageFlags dstStageMask;
        std::vector<ImageMemoryBarrier> imageMemoryBarriers;
    };
    struct PipelineBarrierCommand : public Command {
        PipelineBarrierInfo info;
        explicit PipelineBarrierCommand(PipelineBarrierInfo info) : info(std::move(info)) { }
        [[nodiscard]] CommandType getType() const override { return CommandType::PipelineBarrier; }
    };

    class FrameGraph {
    public:
        void init(FrameGraphBuilder&& builder);
        void bake(FrameGraphBuilder&& builder);

        void execute(const vk::CommandBuffer& cmd);
        [[nodiscard]] const Image& getBackBufferImage() const { return m_images.at(m_builder.getResource(m_builder.backBuffer)->name); };
    private:
        void createImage(std::string_view name, const ImageInfo& info, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect);

        void createDescriptorSet(UniformFrequency frequency, const DescriptorSetInfo& info);

        void createTransferPass(std::string_view name, const TransferPassInfo& info);
        void createRenderPass(std::string_view name, const RenderPassCreateInfo& info);
        void createComputePass(std::string_view name, const ComputePassInfo& info);
    private:
        FrameGraphBuilder m_builder;

        std::unordered_map<std::string, Image> m_images;
        std::unordered_map<UniformFrequency, DescriptorSet> m_descriptorSets;
        std::optional<vk::raii::Sampler> m_sampler;

        std::unordered_map<std::string, TransferPass> m_transferPasses;
        std::unordered_map<std::string, RenderPass> m_renderPasses;
        std::unordered_map<std::string, ComputePass> m_computePasses;

        std::vector<std::unique_ptr<Command>> m_commands;
    };
}