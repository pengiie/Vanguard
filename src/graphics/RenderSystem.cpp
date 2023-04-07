#include "RenderSystem.h"
#include "Vulkan.h"
#include "../Logger.h"
#include "../util/Timer.h"

namespace vanguard {
    void RenderSystem::init() {
        auto& device = Vulkan::getDevice();

        m_frameData.reserve(FRAMES_IN_FLIGHT);
        for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
            vk::raii::CommandPool commandPool = device.createCommandPool({ .queueFamilyIndex = Vulkan::getQueueFamilyIndex() });
            auto commandBuffers = device.allocateCommandBuffers({ .commandPool = *commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 4 });

            m_frameData.push_back(FrameData{
                .imageAvailableSemaphore = device.createSemaphore({}),
                .swapchainImageTransitionSemaphore = device.createSemaphore({}),
                .renderFinishedSemaphore = device.createSemaphore({}),
                .blitFinishedSemaphore = device.createSemaphore({}),
                .inFlightFence = device.createFence({ .flags = vk::FenceCreateFlagBits::eSignaled }),
                .commandPool = std::move(commandPool),
                .contextCommandBuffer = std::move(commandBuffers.at(0)),
                .swapchainImageTransitionCommandBuffer = std::move(commandBuffers.at(1)),
                .renderCommandBuffer = std::move(commandBuffers.at(2)),
                .blitCommandBuffer = std::move(commandBuffers.at(3))
            });
        }

        m_resourceManager.init();
    }

    void RenderSystem::bake(FrameGraphBuilder&& builder) {
        Vulkan::getDevice().waitIdle();
        m_frameGraph = std::make_unique<FrameGraph>();
        m_frameGraph->init(std::move(builder));
    }

    void RenderSystem::beginFrame() {
        m_resourceManager.begin();
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

        TIMER("RenderSystem::commandBufferRecording");

        // Do Rendering
        auto& frame = m_frameData[m_currentFrame];
        frame.commandPool.reset();

        // Transition swapchain image to transfer destination
        {
            frame.swapchainImageTransitionCommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
            frame.swapchainImageTransitionCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, std::vector<vk::ImageMemoryBarrier>{
                    vk::ImageMemoryBarrier{
                            .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                            .dstAccessMask = vk::AccessFlagBits::eMemoryWrite,
                            .oldLayout = vk::ImageLayout::eUndefined,
                            .newLayout = vk::ImageLayout::eTransferDstOptimal,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .image = VkImage(Vulkan::getSwapchainImages()[imageIndex].image),
                            .subresourceRange = vk::ImageSubresourceRange{
                                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                            }
                    }
            });
            frame.swapchainImageTransitionCommandBuffer.end();
        }

        // Execute the render graph
        {
            {
                frame.renderCommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
                m_frameGraph->execute(*frame.renderCommandBuffer);
                frame.renderCommandBuffer.end();
            }

            vk::CommandBuffer commandBuffers[] = {
                    m_resourceManager.bakeStagingCommandBuffer(),
                    *frame.swapchainImageTransitionCommandBuffer,
                    *frame.renderCommandBuffer,
            };
            vk::PipelineStageFlags waitFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            Vulkan::getQueue().submit({vk::SubmitInfo{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &*frameData.imageAvailableSemaphore,
                    .pWaitDstStageMask = &waitFlags,
                    .commandBufferCount = 3,
                    .pCommandBuffers = commandBuffers,
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores = &*frameData.renderFinishedSemaphore,
            }}, nullptr);
        }

        // Blit the backbuffer image to swapchain image and transition the swapchain image to present
        {
            {
                vk::Offset3D zeroOffset{0, 0, 0};
                vk::Offset3D windowSizeOffset{static_cast<int32_t>(window.getWidth()),
                                              static_cast<int32_t>(window.getHeight()), 1};
                std::array<vk::Offset3D, 2> offsets = {zeroOffset, windowSizeOffset};
                vk::ImageSubresourceLayers subresource{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                };
                vk::ImageBlit blitInfo{
                        .srcSubresource = subresource,
                        .srcOffsets = offsets,
                        .dstSubresource = subresource,
                        .dstOffsets = offsets,
                };

                frame.blitCommandBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
                frame.blitCommandBuffer.blitImage(*m_frameGraph->getBackBufferImage().image,
                                                  vk::ImageLayout::eTransferSrcOptimal,
                                                  vk::Image(Vulkan::getSwapchainImages().at(imageIndex).image),
                                                  vk::ImageLayout::eTransferDstOptimal,
                                                  {blitInfo}, vk::Filter::eNearest);


                frame.blitCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                                        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, {
                                                                vk::ImageMemoryBarrier{
                                                                        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                                                                        .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                                                                        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                                                                        .newLayout = vk::ImageLayout::ePresentSrcKHR,
                                                                        .image = VkImage(
                                                                                Vulkan::getSwapchainImages()[imageIndex].image),
                                                                        .subresourceRange = vk::ImageSubresourceRange{
                                                                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                                                .baseMipLevel = 0,
                                                                                .levelCount = 1,
                                                                                .baseArrayLayer = 0,
                                                                                .layerCount = 1
                                                                        }
                                                                }
                                                        });

                frame.blitCommandBuffer.end();
            }

            vk::PipelineStageFlags waitFlags = vk::PipelineStageFlagBits::eTransfer;
            Vulkan::getQueue().submit({vk::SubmitInfo{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &*frameData.renderFinishedSemaphore,
                    .pWaitDstStageMask = &waitFlags,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &*frameData.blitCommandBuffer,
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores = &*frameData.blitFinishedSemaphore,
            }}, *frameData.inFlightFence);
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