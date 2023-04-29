#include "RenderSystem.h"
#include "Vulkan.h"
#include "../Logger.h"
#include "../util/Timer.h"
#include "../util/TypeTraits.h"

namespace vanguard {
    void RenderSystem::init() {
        auto& device = Vulkan::getDevice();

        m_frameData.reserve(FRAMES_IN_FLIGHT);
        for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
            vk::raii::CommandPool commandPool = device.createCommandPool({ .queueFamilyIndex = Vulkan::getQueueFamilyIndex() });
            auto commandBuffers = device.allocateCommandBuffers({ .commandPool = *commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 2 });

            m_frameData.push_back(FrameData{
                .imageAvailableSemaphore = device.createSemaphore({}),
                .commandsFinishedSemaphore = device.createSemaphore({}),
                .blitFinishedSemaphore = device.createSemaphore({}),
                .inFlightFence = device.createFence({ .flags = vk::FenceCreateFlagBits::eSignaled }),
                .commandPool = std::move(commandPool),
                .generalCommandBuffer = std::move(commandBuffers.at(0)),
                .blitCommandBuffer = std::move(commandBuffers.at(1))
            });
        }
    }

    void RenderSystem::bakeCommands(const CommandsInfo& commandsInfo) {
#ifdef VANGUARD_DEBUG
        if(commandsInfo.backbufferImage == UNDEFINED_RESOURCE) {
            throw std::runtime_error("Backbuffer image is undefined");
        }
#endif
        m_commands = commandsInfo;
    }

    void RenderSystem::render(Window& window) {
        auto& device = Vulkan::getDevice();
        auto& frameData = m_frameData[m_currentFrame];

        {
            TIMER("RenderSystem::fenceWaiting");
            auto result = device.waitForFences({*frameData.inFlightFence}, VK_TRUE, UINT64_MAX);
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("Failed to wait for fence");
            }
            device.resetFences({*frameData.inFlightFence});
        }

        uint32_t imageIndex;
        {
            TIMER("RenderSystem::acquireImage");
            auto [imageResult, i] = Vulkan::getSwapchain().acquireNextImage(UINT64_MAX, *frameData.imageAvailableSemaphore);
            if (imageResult == vk::Result::eSuboptimalKHR | imageResult == vk::Result::eErrorOutOfDateKHR) {
                Vulkan::recreateSwapchain(window.getWidth(), window.getHeight());
                //rebake();
                return;
            }
            imageIndex = i;
        }

        // Execute Commands
        auto& frame = m_frameData[m_currentFrame];
        frame.commandPool.reset();

        // General Commands
        {
            TIMER("RenderSystem::generalCommands");
            auto& commandBuffer = *frame.generalCommandBuffer;
            commandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

            // Stager
            m_stager.bakeCommands(commandBuffer);
            m_stager.flush();

            for (auto& command : m_commands.commands) {
                std::visit([&](auto& cmd) {
                    using T = std::decay_t<decltype(cmd)>;
                    if constexpr(std::is_same_v<T, GeneralCommand>) {
                        cmd.execution(commandBuffer);
                    } else if constexpr(std::is_same_v<T, RenderPipelineCommand>) {
                        auto& pipeline = m_resourceManager.getRenderPipeline(cmd.pipeline);

                        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.pipeline);
                        commandBuffer.beginRenderPass(vk::RenderPassBeginInfo{
                                .renderPass = *pipeline.renderPass,
                                .framebuffer = *pipeline.framebuffer,
                                .renderArea = vk::Rect2D{
                                        .offset = vk::Offset2D{0, 0},
                                        .extent = pipeline.info.extent
                                },
                                .clearValueCount = static_cast<uint32_t>(cmd.clearValues.size()),
                                .pClearValues = cmd.clearValues.data()
                        }, vk::SubpassContents::eInline);

                        cmd.execution(commandBuffer);

                        commandBuffer.endRenderPass();
                    } else if constexpr(std::is_same_v<T, ComputePipelineCommand>) {
                        auto& pipeline = m_resourceManager.getComputePipeline(cmd.pipeline);

                        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline.pipeline);
                        cmd.execution(commandBuffer);
                    } else if constexpr(std::is_same_v<T, PipelineBarrierCommand>) {
                        std::vector<vk::ImageMemoryBarrier> imageMemoryBarriers;
                        std::vector<vk::BufferMemoryBarrier> bufferMemoryBarriers;

                        for(const ImageBarrierInfo& barrier: cmd.imageMemoryBarriers) {
                            auto& image = m_resourceManager.getImage(barrier.image);
                            imageMemoryBarriers.push_back(vk::ImageMemoryBarrier{
                                .srcAccessMask = barrier.srcAccessMask,
                                .dstAccessMask = barrier.dstAccessMask,
                                .oldLayout = barrier.oldLayout,
                                .newLayout = barrier.newLayout,
                                .image = *image.image,
                                .subresourceRange = vk::ImageSubresourceRange{
                                    .aspectMask = image.info.aspect,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                }
                            });
                        }
                        for(const BufferBarrierInfo& barrier: cmd.bufferMemoryBarriers) {
                            auto& buffer = m_resourceManager.getBuffer(barrier.buffer);
                            bufferMemoryBarriers.push_back(vk::BufferMemoryBarrier{
                                .srcAccessMask = barrier.srcAccessMask,
                                .dstAccessMask = barrier.dstAccessMask,
                                .buffer = *buffer.buffer,
                                .offset = barrier.offset,
                                .size = barrier.size
                            });
                        }
                        commandBuffer.pipelineBarrier(cmd.srcStage, cmd.dstStage, {}, {}, bufferMemoryBarriers, imageMemoryBarriers);
                    } else {
                        static_assert(always_false<T>, "Non-exhaustive visitor");
                    }
                }, command);
            }
            commandBuffer.end();

            vk::SubmitInfo submitInfo{
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer,
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores = &*frameData.commandsFinishedSemaphore
            };
            Vulkan::getQueue().submit(submitInfo);
        }

        // Blit the backbuffer image to swapchain image and transition the swapchain image to present
        {
            {
                auto& backbufferImage = m_resourceManager.getImage(m_commands.backbufferImage);

                vk::Offset3D zeroOffset{0, 0, 0};
                vk::Offset3D windowSizeOffset{static_cast<int32_t>(window.getWidth()),
                                              static_cast<int32_t>(window.getHeight()), 1};
                vk::Offset3D backbufferSizeOffset{static_cast<int32_t>(backbufferImage.info.width),
                                                  static_cast<int32_t>(backbufferImage.info.height), 1};
                std::array<vk::Offset3D, 2> backbufferOffsets = {zeroOffset, backbufferSizeOffset};
                std::array<vk::Offset3D, 2> swapchainOffsets = {zeroOffset, windowSizeOffset};
                vk::ImageSubresourceLayers subresource{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                };
                vk::ImageBlit blitInfo{
                        .srcSubresource = subresource,
                        .srcOffsets = backbufferOffsets,
                        .dstSubresource = subresource,
                        .dstOffsets = swapchainOffsets,
                };

                frame.blitCommandBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

                vk::ImageSubresourceRange subresourceRange{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                };
                std::vector<vk::ImageMemoryBarrier> barriers = {
                        vk::ImageMemoryBarrier{
                                .srcAccessMask = vk::AccessFlagBits::eNone,
                                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                                .oldLayout = vk::ImageLayout::eUndefined,
                                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                                .image = VkImage(
                                        Vulkan::getSwapchainImages()[imageIndex].image),
                                .subresourceRange = subresourceRange
                        }
                };
                if(m_commands.backbufferImageLayout != vk::ImageLayout::eTransferSrcOptimal || m_commands.backbufferImageAccessMask != vk::AccessFlagBits::eTransferRead) {
                    barriers.push_back(vk::ImageMemoryBarrier{
                            .srcAccessMask = m_commands.backbufferImageAccessMask,
                            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                            .oldLayout = m_commands.backbufferImageLayout,
                            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                            .image = *backbufferImage.image,
                            .subresourceRange = subresourceRange
                    });
                }

                frame.blitCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllGraphics, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barriers);

                frame.blitCommandBuffer.blitImage(*backbufferImage.image,
                                                  vk::ImageLayout::eTransferSrcOptimal,
                                                  Vulkan::getSwapchainImages().at(imageIndex).image,
                                                  vk::ImageLayout::eTransferDstOptimal,
                                                  {blitInfo}, vk::Filter::eNearest);


                frame.blitCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                                        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, {
                                                                vk::ImageMemoryBarrier{
                                                                        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                                                                        .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                                                                        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                                                                        .newLayout = vk::ImageLayout::ePresentSrcKHR,
                                                                        .image = Vulkan::getSwapchainImages()[imageIndex].image,
                                                                        .subresourceRange = subresourceRange
                                                                }
                                                        });

                frame.blitCommandBuffer.end();
            }

            std::vector<vk::PipelineStageFlags> waitFlags = {vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer};
            std::vector<vk::Semaphore> waitSemaphores = {*frameData.imageAvailableSemaphore, *frameData.commandsFinishedSemaphore};
            Vulkan::getQueue().submit(vk::SubmitInfo{
                    .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
                    .pWaitSemaphores = waitSemaphores.data(),
                    .pWaitDstStageMask = waitFlags.data(),
                    .commandBufferCount = 1,
                    .pCommandBuffers = &*frameData.blitCommandBuffer,
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores = &*frameData.blitFinishedSemaphore,
            }, *frameData.inFlightFence);
        }

        // Present the swapchain image
        {
            TIMER("RenderSystem::submitPresentation");
            auto result = Vulkan::getQueue().presentKHR(vk::PresentInfoKHR{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &*frameData.blitFinishedSemaphore,
                    .swapchainCount = 1,
                    .pSwapchains = &*Vulkan::getSwapchain(),
                    .pImageIndices = &imageIndex,
            });
            if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR) {
                Vulkan::recreateSwapchain(window.getWidth(), window.getHeight());
                //rebake();
                return;
            }
        }

        m_currentFrame = (m_currentFrame + 1) % FRAMES_IN_FLIGHT;
        m_frameCount++;
    }
}