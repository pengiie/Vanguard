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

    void RenderSystem::bake(RenderGraphBuilder&& builder) {
        // Bakes the render graph into sequential render passes with automatic layout transitions
        if(builder.m_backBuffer == UNDEFINED_REFERENCE) {
            ERROR("RenderGraphBuilder has no back buffer set");
            return;
        }

        // Create render graph (Deletes any existing render graph)
        m_renderGraph = std::make_unique<RenderGraph>();
        // Save the builder to rebuild in case of swapchain recreation
        m_renderGraphBuilder = std::move(builder);

        ReferenceMap imageReferenceMap;
        ReferenceMap renderPassReferenceMap;

        std::unordered_map<ResourceRef, vk::ImageUsageFlags> imageUsages;
        for (const auto& pass: m_renderGraphBuilder.m_renderPasses) {
            for (const auto& item: pass.inputs) {
                auto type = m_renderGraphBuilder.m_resources[item]->getType();
                if(type == ResourceType::Image || type == ResourceType::Stencil) {
                    imageUsages[item] |= vk::ImageUsageFlagBits::eInputAttachment;
                }
            }
            for (const auto& item: pass.outputs) {
                auto type = m_renderGraphBuilder.m_resources[item]->getType();
                if(type == ResourceType::Image) {
                    imageUsages[item] |= vk::ImageUsageFlagBits::eColorAttachment;
                } else if(type == ResourceType::Stencil) {
                    imageUsages[item] |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                }
            }
        }
        imageUsages[m_renderGraphBuilder.m_backBuffer] |= vk::ImageUsageFlagBits::eTransferSrc;

        // Index by UniformFrequency
        std::vector<std::vector<vk::DescriptorSetLayoutBinding>> setBindings((uint8_t) UniformFrequency::SIZE);

        std::vector<ResourceRef> uniformResources;
        std::vector<ResourceRef> stencilResources;

        // Create resources
        for(size_t i = 0; i < m_renderGraphBuilder.m_resources.size(); i++) {
            auto& resource = m_renderGraphBuilder.m_resources[i];

            if(resource->getType() == ResourceType::Image) {
                auto* imageResource = reinterpret_cast<ImageResource*>(resource.get());
                if(imageResource->info.format == vk::Format::eUndefined) {
                    ERROR("Could not create image resource {}: Format is undefined", imageResource->name);
                }
                auto ref = m_renderGraph->createImage(*imageResource, imageUsages.at(i), false);

                // If resource is the back buffer, set it as the back buffer
                if(i == m_renderGraphBuilder.m_backBuffer) {
                    m_renderGraph->m_backBuffer = ref;
                }

                imageReferenceMap[i] = ref;
            } else if(resource->getType() == ResourceType::Stencil) {
                auto* stencilResource = reinterpret_cast<StencilResource*>(resource.get());
                if(stencilResource->info.format == vk::Format::eUndefined) {
                    ERROR("Could not create stencil resource {}: Format is undefined", stencilResource->name);
                }
                ImageResource imageResource{};
                imageResource.name = stencilResource->name;
                imageResource.info = stencilResource->info;

                auto ref = m_renderGraph->createImage(imageResource, imageUsages.at(i), true);

                // If resource is the back buffer, set it as the back buffer
                if(i == m_renderGraphBuilder.m_backBuffer) {
                    m_renderGraph->m_backBuffer = ref;
                }

                imageReferenceMap[i] = ref;
                stencilResources.push_back(i);
            } else if(resource->getType() == ResourceType::Uniform) {
                auto* uniformResource = reinterpret_cast<UniformResource*>(resource.get());
                if(uniformResource->info.frequency == UniformFrequency::SIZE) {
                    ERROR("Could not create uniform resource {}: Frequency is undefined", uniformResource->name);
                }

                setBindings[(uint8_t) uniformResource->info.frequency].emplace_back(vk::DescriptorSetLayoutBinding{
                        .binding = uniformResource->info.binding,
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .descriptorCount = 1,
                        .stageFlags = vk::ShaderStageFlagBits::eAll,
                });
                uniformResources.push_back(i);
            }
        }

        // Create descriptor sets
        for (uint8_t i = 0; i < static_cast<uint8_t>(setBindings.size()); i++) {
            auto frequency = static_cast<UniformFrequency>(i);
            auto& bindings = setBindings[i];
            if(bindings.empty()) continue;

            m_renderGraph->createDescriptorSetLayout(frequency, bindings);
            m_renderGraph->createDescriptorSet(frequency);

            for(uint32_t j = 0; j < FRAMES_IN_FLIGHT; j++) {
                std::vector<std::pair<uint32_t, vk::DescriptorBufferInfo>> bufferInfos;
                for (const auto& resource: uniformResources) {
                    UniformInfo uniformInfo = reinterpret_cast<UniformResource*>(m_renderGraphBuilder.m_resources[resource].get())->info;
                    if(uniformInfo.frequency == frequency) {
                        const AllocatedBuffer& buffer = m_resourceManager.getBuffer(uniformInfo.buffer.getForFrame(j));
                        bufferInfos.emplace_back(uniformInfo.binding, vk::DescriptorBufferInfo{
                                .buffer = *buffer.buffer,
                                .offset = 0,
                                .range = buffer.bufferSize
                        });
                    }
                }

                m_renderGraph->updateDescriptorBindings(frequency, j, bufferInfos);
            }
        }

        // Create render passes
        for(size_t i = 0; i < m_renderGraphBuilder.m_renderPasses.size(); i++) {
            auto& renderPassInfo = m_renderGraphBuilder.m_renderPasses[i];
            ResourceRef stencilRef = UNDEFINED_REFERENCE;
            for(auto& outputRef: renderPassInfo.outputs) {
                if(std::find(stencilResources.begin(), stencilResources.end(), outputRef) != stencilResources.end()) {
                    if(stencilRef != UNDEFINED_REFERENCE)
                        ERROR("Render pass {} has multiple stencil outputs", renderPassInfo.name);
                    stencilRef = outputRef;
                }
            }
            auto ref = m_renderGraph->addRenderPass(imageReferenceMap, stencilRef, renderPassInfo);
            renderPassReferenceMap[i] = ref;
        }

        // Create image layouts (using RenderGraphBuilder ResourceRef)
        std::unordered_map<ResourceRef, vk::ImageLayout> imageLayouts;
        std::unordered_map<ResourceRef, vk::AccessFlags> imageAccess;
        for(auto& [imageRef, _]: imageReferenceMap) {
            imageLayouts[imageRef] = vk::ImageLayout::eUndefined;
            imageAccess[imageRef] = vk::AccessFlags();
        }

        std::vector<ResourceRef> shaderReadOnlyTransitions;

        for(size_t i = 0; i < m_renderGraphBuilder.m_renderPasses.size(); i++) {
            auto& renderPassInfo = m_renderGraphBuilder.m_renderPasses[i];

            // Add images to shader read only transition queue
            bool requiresTransition = false;
            for(auto& input : renderPassInfo.inputs) {
                // Only add image resources to the transition queue
                if(m_renderGraphBuilder.m_resources[input]->getType() == ResourceType::Image) {
                    if(imageLayouts[input] != vk::ImageLayout::eShaderReadOnlyOptimal) {
                        shaderReadOnlyTransitions.push_back(input);
                        requiresTransition = true;
                    }
                }
            }

            // Submit a transition command if needed
            if(requiresTransition) {
                ImageBarrierCommand command;
                for (const auto& resourceRef: shaderReadOnlyTransitions) {
                    command.transitions.push_back(ImageTransition{
                        .image = imageReferenceMap.at(resourceRef),
                        .oldLayout = imageLayouts.at(resourceRef),
                        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                        .oldAccess = imageAccess.at(resourceRef),
                        .newAccess = vk::AccessFlagBits::eShaderRead
                    });
                }
                m_renderGraph->addCommand(command);
            }

            // Submit render pass command
            RenderPassCommand command;
            command.pass = renderPassReferenceMap.at(i);
            m_renderGraph->addCommand(command);

            // Update output image layouts
            for(auto& output : renderPassInfo.outputs) {
                imageLayouts[output] = vk::ImageLayout::eColorAttachmentOptimal;
                imageAccess[output] = vk::AccessFlagBits::eColorAttachmentWrite;
            }
        }

        // Add a final transition command for the back buffer
        ImageBarrierCommand command;
        command.transitions.push_back(ImageTransition{
            .image = m_renderGraph->m_backBuffer,
            .oldLayout = imageLayouts.at(m_renderGraphBuilder.m_backBuffer),
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .oldAccess = imageAccess.at(m_renderGraphBuilder.m_backBuffer),
            .newAccess = vk::AccessFlagBits::eTransferRead
        });
        m_renderGraph->addCommand(command);
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
                rebake();
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
                m_renderGraph->execute(frame.renderCommandBuffer, m_currentFrame);
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
                frame.blitCommandBuffer.blitImage(*m_renderGraph->getBackBufferImage(m_currentFrame).image,
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
                rebake();
                return;
            }
        }

        m_currentFrame = (m_currentFrame + 1) % FRAMES_IN_FLIGHT;
        m_frameCount++;
    }
}