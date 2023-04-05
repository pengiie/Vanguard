#include "FrameGraph.h"
#include "../Application.h"

namespace vanguard {

    void FrameGraph::init(FrameGraphBuilder&& builder) {
        m_sampler = Vulkan::getDevice().createSampler(vk::SamplerCreateInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
        });

        bake(std::move(builder));
    }

    void FrameGraph::bake(FrameGraphBuilder&& builder) {
        m_builder = std::move(builder);

        const std::string& backBufferName = m_builder.getResource(m_builder.backBuffer)->name;

        auto transferPasses = m_builder.getTransferPasses();
        auto computePasses = m_builder.getComputePasses();
        auto renderPasses = m_builder.getRenderPasses();

        // Determine image usage flags
        std::unordered_map<std::string, vk::ImageUsageFlags> usageFlags;
        for (const auto& pass: transferPasses) {
            for (const auto& input: pass.inputs) {
                const Resource* resource = m_builder.getResource(input);
                if (resource->getType() == ResourceType::Image || resource->getType() == ResourceType::Stencil) {
                    usageFlags[resource->name] |= vk::ImageUsageFlagBits::eTransferSrc;
                }
            }
            for (const auto& output: pass.outputs) {
                const Resource* resource = m_builder.getResource(output);
                if (resource->getType() == ResourceType::Image || resource->getType() == ResourceType::Stencil) {
                    usageFlags[resource->name] |= vk::ImageUsageFlagBits::eTransferDst;
                }
            }
        }
        for (const auto& pass: computePasses) {
            for (const auto& input: pass.inputs) {
                const Resource* resource = m_builder.getResource(input);
                if (resource->getType() == ResourceType::Image || resource->getType() == ResourceType::Stencil) {
                    usageFlags[resource->name] |= vk::ImageUsageFlagBits::eStorage;
                }
            }
            for (const auto& output: pass.outputs) {
                const Resource* resource = m_builder.getResource(output);
                if (resource->getType() == ResourceType::Image || resource->getType() == ResourceType::Stencil) {
                    usageFlags[resource->name] |= vk::ImageUsageFlagBits::eStorage;
                }
            }
        }
        for (const auto& pass: renderPasses) {
            for (const auto& input: pass.inputs) {
                const Resource* resource = m_builder.getResource(input);
                if (resource->getType() == ResourceType::Image || resource->getType() == ResourceType::Stencil) {
                    usageFlags[resource->name] |= vk::ImageUsageFlagBits::eSampled;
                }
            }
            for (const auto& output: pass.outputs) {
                const Resource* resource = m_builder.getResource(output);
                if (resource->getType() == ResourceType::Image) {
                    usageFlags[resource->name] |= vk::ImageUsageFlagBits::eColorAttachment;
                }
                if (resource->getType() == ResourceType::Stencil) {
                    usageFlags[resource->name] |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                }
            }
        }

#ifdef VANGUARD_DEBUG
        if(usageFlags.find(backBufferName) == usageFlags.end()) {
            throw std::runtime_error("Back buffer is not used by any pass");
        }
#endif

        usageFlags[backBufferName] |= vk::ImageUsageFlagBits::eTransferSrc;

        // Define images and stencils
        for (const auto& resource: m_builder.getImageResources()) {
            createImage(resource.name, resource.info, usageFlags.at(resource.name), vk::ImageAspectFlagBits::eColor);
        }
        for (const auto& resource: m_builder.getStencilResources()) {
            createImage(resource.name, resource.info, usageFlags.at(resource.name), vk::ImageAspectFlagBits::eDepth);
        }

        // Define descriptor set info
        std::unordered_map<UniformFrequency, DescriptorSetInfo> descriptorSetInfos;
        for (const auto& resource: m_builder.getUniformBufferResources()) {
            const auto& info = resource.info;
            descriptorSetInfos[info.frequency].uniformBuffers[resource.name] = info;
        }
        for (const auto& resource: m_builder.getUniformImageResources()) {
            const auto& info = resource.info;
            descriptorSetInfos[info.frequency].uniformImages[resource.name] = info;
        }

        // Create descriptor sets
        for (const auto& [frequency, info]: descriptorSetInfos) {
            createDescriptorSet(frequency, info);
        }

        // Set to track which images have been used to determine which render pass has the image first.
        std::unordered_set<std::string> usedImages;

        // Create passes
        for (const auto& pass: transferPasses) {
            createTransferPass(pass.name, pass);
        }
        for (const auto& pass: renderPasses) {
            RenderPassCreateInfo info{};
            info.info = pass;
            for (const auto& item: pass.inputs) {
                const Resource* resource = m_builder.getResource(item);
                if (resource->getType() == ResourceType::Image || resource->getType() == ResourceType::Stencil) {
                    auto& name = resource->name;
                    info.inputAttachments.emplace_back(name, !usedImages.contains(name));
                    usedImages.insert(name);
                }
            }
            for (const auto& item: pass.outputs) {
                const Resource* resource = m_builder.getResource(item);
                if (resource->getType() == ResourceType::Image) {
                    auto& name = resource->name;
                    info.colorAttachments.emplace_back(name, !usedImages.contains(name));
                    usedImages.insert(name);
                }
                if (resource->getType() == ResourceType::Stencil) {
#ifdef VANGUARD_DEBUG
                    if (info.depthAttachment.has_value()) {
                        throw std::runtime_error("Render pass cannot have more than one depth stencil attachment!");
                    }
#endif
                    auto& name = resource->name;
                    info.depthAttachment = { name, !usedImages.contains(name) };
                    usedImages.insert(name);
                }
            }

            createRenderPass(pass.name, info);
        }
        for (const auto& pass: computePasses) {
            createComputePass(pass.name, pass);
        }

        // Determine pipeline barriers and command execution order.
        std::unordered_map<std::string, vk::ImageLayout> imageLayouts;
        for (const auto& [name, _]: m_images) {
            imageLayouts.emplace(name, vk::ImageLayout::eUndefined);
        }

        std::unordered_map<std::string, vk::AccessFlags> imageAccesses;
        for (const auto& [name, _]: m_images) {
            imageAccesses.emplace(name, vk::AccessFlagBits::eNone);
        }

        vk::PipelineStageFlags currentStage = vk::PipelineStageFlagBits::eTopOfPipe;

        for (const auto& passInfo: m_builder.getPasses()) {
            std::unique_ptr<Command> command;
            std::vector<ImageMemoryBarrier> imageBarriers;
            vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eTopOfPipe;

            // Transfer passes
            if(passInfo->getType() == PassType::Transfer) {
                dstStage = vk::PipelineStageFlagBits::eTransfer;
                command = std::make_unique<TransferPassCommand>(passInfo->name);

                for (const auto& resourceRef: passInfo->inputs) {
                    const Resource* resource = m_builder.getResource(resourceRef);
                    auto name = resource->name;
#ifdef VANGUARD_DEBUG
                    if (resource->getType() != ResourceType::Image) {
                        throw std::runtime_error("Transfer pass input only supports images!");
                    }
#endif
                    auto currentLayout = imageLayouts.at(name);
                    auto currentAccess = imageAccesses.at(name);
                    if(currentLayout != vk::ImageLayout::eTransferSrcOptimal) {
                        imageBarriers.push_back(ImageMemoryBarrier{
                            .image = name,
                            .oldLayout = currentLayout,
                            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                            .srcAccessMask = currentAccess,
                            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                            .aspectMask = vk::ImageAspectFlagBits::eColor
                        });
                        imageLayouts.at(name) = vk::ImageLayout::eTransferSrcOptimal;
                        imageAccesses.at(name) = vk::AccessFlagBits::eTransferRead;
                    }
                }
                for (const auto& resourceRef: passInfo->outputs) {
                    const Resource* resource = m_builder.getResource(resourceRef);
                    auto name = resource->name;
#ifdef VANGUARD_DEBUG
                    if (resource->getType() != ResourceType::Image) {
                        throw std::runtime_error("Transfer pass output only supports images!");
                    }
#endif
                    auto currentLayout = imageLayouts.at(name);
                    auto currentAccess = imageAccesses.at(name);
                    if(currentLayout != vk::ImageLayout::eTransferDstOptimal) {
                        imageBarriers.push_back(ImageMemoryBarrier{
                                .image = name,
                                .oldLayout = currentLayout,
                                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                                .srcAccessMask = currentAccess,
                                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                                .aspectMask = vk::ImageAspectFlagBits::eColor
                        });
                        imageLayouts.at(name) = vk::ImageLayout::eTransferDstOptimal;
                        imageAccesses.at(name) = vk::AccessFlagBits::eTransferWrite;
                    }
                }
            }

            // Render passes
            if(passInfo->getType() == PassType::Render) {
                dstStage = vk::PipelineStageFlagBits::eAllGraphics;
                command = std::make_unique<RenderPassCommand>(passInfo->name );

                for (const auto& resourceRef: passInfo->inputs) {
                    const Resource* resource = m_builder.getResource(resourceRef);
                    auto name = resource->name;

                    if(resource->getType() == ResourceType::Image) {
                        auto currentLayout = imageLayouts.at(name);
                        auto currentAccess = imageAccesses.at(name);
                        if(currentLayout != vk::ImageLayout::eShaderReadOnlyOptimal) {
                            imageBarriers.push_back(ImageMemoryBarrier{
                                .image = name,
                                .oldLayout = currentLayout,
                                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                .srcAccessMask = currentAccess,
                                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                            });
                            imageLayouts.at(name) = vk::ImageLayout::eShaderReadOnlyOptimal;
                            imageAccesses.at(name) = vk::AccessFlagBits::eShaderRead;
                        }
                    }
                }
                for (const auto& resourceRef: passInfo->outputs) {
                    const Resource* resource = m_builder.getResource(resourceRef);
                    auto name = resource->name;

                    if(resource->getType() == ResourceType::Image) {
                        auto currentLayout = imageLayouts.at(name);
                        auto currentAccess = imageAccesses.at(name);
                        if(currentLayout != vk::ImageLayout::eColorAttachmentOptimal) {
                            imageBarriers.push_back(ImageMemoryBarrier{
                                .image = name,
                                .oldLayout = currentLayout,
                                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                .srcAccessMask = currentAccess,
                                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                            });
                            imageLayouts.at(name) = vk::ImageLayout::eColorAttachmentOptimal;
                            imageAccesses.at(name) = vk::AccessFlagBits::eColorAttachmentWrite;
                        }
                    }
                    if(resource->getType() == ResourceType::Stencil) {
                        auto currentLayout = imageLayouts.at(name);
                        auto currentAccess = imageAccesses.at(name);
                        if(currentLayout != vk::ImageLayout::eDepthStencilAttachmentOptimal) {
                            imageBarriers.push_back(ImageMemoryBarrier{
                                .image = name,
                                .oldLayout = currentLayout,
                                .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                .srcAccessMask = currentAccess,
                                .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                                .aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                            });
                            imageLayouts.at(name) = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                            imageAccesses.at(name) = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                        }
                    }
                }
            }

            if(passInfo->getType() == PassType::Compute) {
                dstStage = vk::PipelineStageFlagBits::eComputeShader;
                command = std::make_unique<ComputePassCommand>(passInfo->name);

                for (const auto& resourceRef: passInfo->inputs) {
                    const Resource* resource = m_builder.getResource(resourceRef);
                    auto name = resource->name;

                    if(resource->getType() == ResourceType::Image) {
                        auto currentLayout = imageLayouts.at(name);
                        auto currentAccess = imageAccesses.at(name);
                        if(currentLayout != vk::ImageLayout::eGeneral) {
                            imageBarriers.push_back(ImageMemoryBarrier{
                                .image = name,
                                .oldLayout = currentLayout,
                                .newLayout = vk::ImageLayout::eGeneral,
                                .srcAccessMask = currentAccess,
                                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                            });
                            imageLayouts.at(name) = vk::ImageLayout::eGeneral;
                            imageAccesses.at(name) = vk::AccessFlagBits::eShaderRead;
                        }
                    }
                }
                for(const auto& resourceRef: passInfo->outputs) {
                    const Resource* resource = m_builder.getResource(resourceRef);
                    auto name = resource->name;

                    if(resource->getType() == ResourceType::Image) {
                        auto currentLayout = imageLayouts.at(name);
                        auto currentAccess = imageAccesses.at(name);
                        if(currentLayout != vk::ImageLayout::eGeneral) {
                            imageBarriers.push_back(ImageMemoryBarrier{
                                .image = name,
                                .oldLayout = currentLayout,
                                .newLayout = vk::ImageLayout::eGeneral,
                                .srcAccessMask = currentAccess,
                                .dstAccessMask = vk::AccessFlagBits::eShaderWrite,
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                            });
                            imageLayouts.at(name) = vk::ImageLayout::eGeneral;
                            imageAccesses.at(name) = vk::AccessFlagBits::eShaderWrite;
                        }
                    }
                }
            }

#ifdef VANGUARD_DEBUG
            if(!command) {
                throw std::runtime_error("Pass type is unknown!");
            }
#endif

            if(!imageBarriers.empty()) {
                m_commands.push_back(std::make_unique<PipelineBarrierCommand>(PipelineBarrierInfo{
                    .srcStageMask = currentStage,
                    .dstStageMask = dstStage,
                    .imageMemoryBarriers = imageBarriers
                }));
                currentStage = dstStage;
            }
            m_commands.emplace_back(std::move(command));
        }

        // Transition backbuffer image
        m_commands.emplace_back(std::make_unique<PipelineBarrierCommand>((PipelineBarrierInfo{
                .srcStageMask = currentStage,
                .dstStageMask = vk::PipelineStageFlagBits::eTransfer,
                .imageMemoryBarriers = {
                    ImageMemoryBarrier{
                        .image = backBufferName,
                        .oldLayout = imageLayouts.at(backBufferName),
                        .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                        .srcAccessMask = imageAccesses.at(backBufferName),
                        .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                    }
                }
        })));

        // Debug info
#ifdef VANGUARD_DEBUG
        {
            for (const auto& [frequency, info]: descriptorSetInfos) {
                auto frequencyName = "Unknown";
                switch (frequency) {
                    case UniformFrequency::PerFrame:
                        frequencyName = "PerFrame";
                        break;
                    case UniformFrequency::PerPass:
                        frequencyName = "PerPass";
                        break;
                }
                INFO("Descriptor Set {} - {}:", (int) frequency, frequencyName);
                std::vector<std::string> bindings(info.uniformBuffers.size() + info.uniformImages.size());
                for (const auto& [name, bindingInfo]: info.uniformBuffers) {
                    bindings[bindingInfo.binding] = name;
                }
                for (const auto& [name, bindingInfo]: info.uniformImages) {
                    bindings[bindingInfo.binding] = name;
                }
                for (int i = 0; i < bindings.size(); ++i) {
                    INFO("\tBinding {} - {}", i, bindings[i]);
                }
                INFO("");
            }
        }
#endif
    }

    void FrameGraph::execute(const vk::CommandBuffer& cmd) {
        auto frameIndex = Application::Get().getRenderSystem().getFrameIndex();
        for (const auto& command: m_commands) {
            auto type = command->getType();
            if(type == CommandType::PipelineBarrier) {
                auto& barrier = *reinterpret_cast<PipelineBarrierCommand*>(command.get());
                auto& info = barrier.info;

                std::vector<vk::ImageMemoryBarrier> imageBarriers;
                for (const auto& imageBarrier: info.imageMemoryBarriers) {
                    imageBarriers.push_back(vk::ImageMemoryBarrier{
                        .srcAccessMask = imageBarrier.srcAccessMask,
                        .dstAccessMask = imageBarrier.dstAccessMask,
                        .oldLayout = imageBarrier.oldLayout,
                        .newLayout = imageBarrier.newLayout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = *m_images.at(imageBarrier.image).image,
                        .subresourceRange = vk::ImageSubresourceRange{
                            .aspectMask = imageBarrier.aspectMask,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1
                        }
                    });
                }
                cmd.pipelineBarrier(info.srcStageMask, info.dstStageMask, {}, {}, {}, imageBarriers);
            }
            if(type == CommandType::TransferPass) {
                auto& transferPassCommand = *reinterpret_cast<TransferPassCommand*>(command.get());
                m_transferPasses.at(transferPassCommand.passName).info.execution(cmd);
            }

            // Pipeline Passes
            std::vector<vk::DescriptorSet> descriptorSets;
            if(m_descriptorSets.contains(UniformFrequency::PerFrame)) {
                descriptorSets.push_back(*m_descriptorSets.at(UniformFrequency::PerFrame).sets.at(frameIndex));
            }
            if(m_descriptorSets.contains(UniformFrequency::PerPass)) {
                descriptorSets.push_back(*m_descriptorSets.at(UniformFrequency::PerPass).sets.at(frameIndex));
            }

            if(type == CommandType::RenderPass) {
                auto& renderPassCommand = *reinterpret_cast<RenderPassCommand*>(command.get());
                auto& renderPass = m_renderPasses.at(renderPassCommand.passName);

                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *renderPass.pipeline);

                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderPass.pipelineLayout, 0, descriptorSets, {});

                cmd.beginRenderPass(vk::RenderPassBeginInfo{
                    .renderPass = *renderPass.renderPass,
                    .framebuffer = *renderPass.framebuffer,
                    .renderArea = vk::Rect2D{
                        .offset = vk::Offset2D{ 0, 0 },
                        .extent = Vulkan::getSwapchainExtent()
                    },
                    .clearValueCount = (uint32_t) renderPass.clearValues.size(),
                    .pClearValues = renderPass.clearValues.data()
                }, vk::SubpassContents::eInline);
                renderPass.info.execution(cmd);
                cmd.endRenderPass();
            }
            if(type == CommandType::ComputePass) {
                auto& computePassCommand = *reinterpret_cast<ComputePassCommand*>(command.get());
                auto& computePass = m_computePasses.at(computePassCommand.passName);

                cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePass.pipeline);
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePass.pipelineLayout, 0, descriptorSets, {});
                computePass.info.execution(cmd);
            }
        }
    }

    void FrameGraph::createImage(std::string_view name, const ImageInfo& info, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect) {
        auto& device = Vulkan::getDevice();
        auto swapchainExtent = Vulkan::getSwapchainExtent();

        vk::raii::Image image = device.createImage(vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = info.format,
            .extent = vk::Extent3D{ swapchainExtent.width, swapchainExtent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined
        });

        Allocation allocation;
        VmaAllocationCreateInfo allocInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };
        vmaAllocateMemoryForImage(*Vulkan::getAllocator(), static_cast<VkImage>(*image), &allocInfo, &allocation.allocation, &allocation.allocationInfo);
        vmaBindImageMemory(*Vulkan::getAllocator(), allocation.allocation, static_cast<VkImage>(*image));

        vk::raii::ImageView view = device.createImageView(vk::ImageViewCreateInfo{
            .image = *image,
            .viewType = vk::ImageViewType::e2D,
            .format = info.format,
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        });

        m_images.emplace(name, Image{
            .info = info,
            .image = std::move(image),
            .view = std::move(view),
            .allocation = std::move(allocation)
        });
    }

    static vk::DescriptorType toVkDescriptorType(UniformImageType type) {
        switch(type) {
            case UniformImageType::CombinedSampler: return vk::DescriptorType::eCombinedImageSampler;
            case UniformImageType::Storage: return vk::DescriptorType::eStorageImage;
        }
        throw std::runtime_error("Invalid UniformImageType");
    }

    void FrameGraph::createDescriptorSet(UniformFrequency frequency, const DescriptorSetInfo& info) {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        for (const auto& [name, bufferInfo]: info.uniformBuffers) {
            bindings.push_back(vk::DescriptorSetLayoutBinding{
                    .binding = bufferInfo.binding,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eAll
            });
        }
        for (const auto& [name, imageInfo]: info.uniformImages) {
            bindings.push_back(vk::DescriptorSetLayoutBinding{
                    .binding = imageInfo.binding,
                    .descriptorType = toVkDescriptorType(imageInfo.type),
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eAll
            });
        }

        auto& device = Vulkan::getDevice();
        vk::raii::DescriptorSetLayout layout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data()
        });

        std::vector<vk::DescriptorSetLayout> layouts(FRAMES_IN_FLIGHT, *layout);
        std::vector<vk::raii::DescriptorSet> sets = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                .descriptorPool = *Vulkan::getDescriptorPool(),
                .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
                .pSetLayouts = layouts.data()
        });

        // Write descriptor sets
        for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
            auto& set = sets[i];
            std::vector<vk::WriteDescriptorSet> writes;
            for (const auto& [name, bufferInfo]: info.uniformBuffers) {
                auto& buffer = Application::Get().getRenderSystem().getResourceManager().getBuffer(bufferInfo.buffer.getForFrame(i));
                vk::DescriptorBufferInfo descriptorBufferInfo{
                    .buffer = *buffer.buffer,
                    .offset = 0,
                    .range = buffer.bufferSize
                };
                writes.push_back(vk::WriteDescriptorSet{
                        .dstSet = *set,
                        .dstBinding = bufferInfo.binding,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo = &descriptorBufferInfo
                });
            }
            for (const auto& [name, imageInfo]: info.uniformImages) {
                auto& image = m_images.at(m_builder.getResource(imageInfo.image)->name);
                vk::DescriptorImageInfo descriptorImageInfo{
                    .sampler = **m_sampler,
                    .imageView = *image.view,
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                };

                if(imageInfo.type == UniformImageType::Storage) {
                    descriptorImageInfo.imageLayout = vk::ImageLayout::eGeneral;
                    descriptorImageInfo.sampler = nullptr;
                }

                writes.push_back(vk::WriteDescriptorSet{
                        .dstSet = *set,
                        .dstBinding = imageInfo.binding,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = toVkDescriptorType(imageInfo.type),
                        .pImageInfo = &descriptorImageInfo
                });
            }

            device.updateDescriptorSets(writes, {});
        }

        m_descriptorSets.emplace(frequency, DescriptorSet{
            .info = info,
            .layout = std::move(layout),
            .sets = std::move(sets)
        });
    }

    void FrameGraph::createTransferPass(std::string_view name, const TransferPassInfo& info) {
        m_transferPasses.emplace(name, TransferPass{
            .info = info
        });
    }

    static vk::raii::ShaderModule createShaderModule(const std::vector<uint32_t>& code) {
        return Vulkan::getDevice().createShaderModule(vk::ShaderModuleCreateInfo{
                .codeSize = code.size() * sizeof(uint32_t),
                .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        });
    }

    void FrameGraph::createRenderPass(std::string_view name, const RenderPassCreateInfo& info) {
        // Attachments and attachment references
        std::vector<vk::AttachmentDescription> attachments;
        std::vector<vk::ImageView> imageViews;

        std::vector<vk::AttachmentReference> inputAttachmentRefs;
        std::vector<vk::AttachmentReference> colorAttachmentRefs;
        std::optional<vk::AttachmentReference> depthAttachmentRef;

        for (const auto& [imageName, shouldClear]: info.inputAttachments) {
            auto& image = m_images.at(imageName);
            attachments.push_back(vk::AttachmentDescription{
                    .format = image.info.format,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = shouldClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
            inputAttachmentRefs.push_back(vk::AttachmentReference{
                    .attachment = static_cast<uint32_t>(attachments.size() - 1),
                    .layout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
            imageViews.push_back(*image.view);
        }
        for (const auto& [imageName, shouldClear]: info.colorAttachments) {
            auto& image = m_images.at(imageName);
            attachments.push_back(vk::AttachmentDescription{
                    .format = image.info.format,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = shouldClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .finalLayout = vk::ImageLayout::eColorAttachmentOptimal
            });
            colorAttachmentRefs.push_back(vk::AttachmentReference{
                    .attachment = static_cast<uint32_t>(attachments.size() - 1),
                    .layout = vk::ImageLayout::eColorAttachmentOptimal
            });
            imageViews.push_back(*image.view);
        }
        if(info.depthAttachment) {
            auto [imageName, shouldClear] = *info.depthAttachment;
            auto& image = m_images.at(imageName);
            attachments.push_back(vk::AttachmentDescription{
                    .format = image.info.format,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = shouldClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    .stencilLoadOp = shouldClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
                    .stencilStoreOp = vk::AttachmentStoreOp::eStore,
                    .initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
            });
            depthAttachmentRef = vk::AttachmentReference{
                    .attachment = static_cast<uint32_t>(attachments.size() - 1),
                    .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
            };
            imageViews.push_back(*image.view);
        }

        // Define subpass and subpass dependencies.
        auto swapchainExtent = Vulkan::getSwapchainExtent();
        vk::SubpassDescription subpass{
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .inputAttachmentCount = static_cast<uint32_t>(inputAttachmentRefs.size()),
                .pInputAttachments = inputAttachmentRefs.data(),
                .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size()),
                .pColorAttachments = colorAttachmentRefs.data(),
                .pDepthStencilAttachment = depthAttachmentRef ? &depthAttachmentRef.value() : nullptr
        };

        std::vector<vk::SubpassDependency> dependencies = {
                vk::SubpassDependency{
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        .srcAccessMask = {},
                        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite
                }
        };
        if(depthAttachmentRef)
            dependencies.push_back(vk::SubpassDependency{
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .srcAccessMask = {},
                    .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite
            });

        auto& device = Vulkan::getDevice();
        vk::raii::RenderPass renderPass = device.createRenderPass(vk::RenderPassCreateInfo{
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = static_cast<uint32_t>(dependencies.size()),
                .pDependencies = dependencies.data()
        });
        vk::raii::Framebuffer framebuffer = device.createFramebuffer(vk::FramebufferCreateInfo{
                .renderPass = *renderPass,
                .attachmentCount = static_cast<uint32_t>(imageViews.size()),
                .pAttachments = imageViews.data(),
                .width = swapchainExtent.width,
                .height = swapchainExtent.height,
                .layers = 1
        });

        // Pipeline and pipeline layout creation
        // Create shaderModules
        vk::raii::ShaderModule vertexModule = createShaderModule(info.info.vertexShader);
        vk::raii::ShaderModule fragmentModule = createShaderModule(info.info.fragmentShader);

        vk::PipelineShaderStageCreateInfo shaderStageInfo[] = {
                vk::PipelineShaderStageCreateInfo{
                        .stage = vk::ShaderStageFlagBits::eVertex,
                        .module = *vertexModule,
                        .pName = "main"
                },
                vk::PipelineShaderStageCreateInfo{
                        .stage = vk::ShaderStageFlagBits::eFragment,
                        .module = *fragmentModule,
                        .pName = "main"
                }
        };

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
                .vertexBindingDescriptionCount = 0,
                .pVertexBindingDescriptions = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions = nullptr
        };

        vk::VertexInputBindingDescription bindingDescription{
                .binding = 0,
                .stride = info.info.vertexInput.getStride() ,
                .inputRate = vk::VertexInputRate::eVertex
        };
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        attributeDescriptions.reserve(info.info.vertexInput.getAttributes().size());
        for(const auto& attribute : info.info.vertexInput.getAttributes()) {
            attributeDescriptions.push_back(vk::VertexInputAttributeDescription{
                    .location = attribute.location,
                    .binding = 0,
                    .format = attribute.format,
                    .offset = attribute.offset
            });
        }
        if(info.info.vertexInput.getStride() != 0) {
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        }

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
                .topology = vk::PrimitiveTopology::eTriangleList,
                .primitiveRestartEnable = VK_FALSE
        };

        vk::Viewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(swapchainExtent.width),
                .height = static_cast<float>(swapchainExtent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
        };
        vk::Rect2D scissor{
                .offset = vk::Offset2D{0, 0},
                .extent = swapchainExtent
        };
        vk::PipelineViewportStateCreateInfo viewportStateInfo{
                .viewportCount = 1,
                .pViewports = &viewport,
                .scissorCount = 1,
                .pScissors = &scissor
        };

        vk::PipelineRasterizationStateCreateInfo rasterizationStateInfo{
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = vk::PolygonMode::eFill,
                .cullMode = vk::CullModeFlagBits::eBack,
                .frontFace = vk::FrontFace::eCounterClockwise,
                .depthBiasEnable = VK_FALSE,
                .depthBiasConstantFactor = 0.0f,
                .depthBiasClamp = 0.0f,
                .depthBiasSlopeFactor = 0.0f,
                .lineWidth = 1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisampleStateInfo{
                .rasterizationSamples = vk::SampleCountFlagBits::e1,
                .sampleShadingEnable = VK_FALSE,
                .minSampleShading = 1.0f,
                .pSampleMask = nullptr,
                .alphaToCoverageEnable = VK_FALSE,
                .alphaToOneEnable = VK_FALSE
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencilStateInfo{
                .depthTestEnable = VK_FALSE,
                .depthWriteEnable = VK_FALSE,
        };
        if(depthAttachmentRef) {
            depthStencilStateInfo.depthTestEnable = VK_TRUE;
            depthStencilStateInfo.depthWriteEnable = VK_TRUE;
            depthStencilStateInfo.depthCompareOp = vk::CompareOp::eLess;
        }

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = vk::BlendFactor::eOne,
                .dstColorBlendFactor = vk::BlendFactor::eZero,
                .colorBlendOp = vk::BlendOp::eAdd,
                .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                .alphaBlendOp = vk::BlendOp::eAdd,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };
        std::array<float, 4> blendConstants = {0.0f, 0.0f, 0.0f, 0.0f};
        vk::PipelineColorBlendStateCreateInfo colorBlendStateInfo{
                .logicOpEnable = VK_FALSE,
                .logicOp = vk::LogicOp::eCopy,
                .attachmentCount = 1,
                .pAttachments = &colorBlendAttachment,
                .blendConstants = blendConstants
        };

        vk::PipelineDynamicStateCreateInfo dynamicStateInfo{
                .dynamicStateCount = 0,
                .pDynamicStates = nullptr
        };

        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
        descriptorSetLayouts.reserve(m_descriptorSets.size());
        for(const auto& [_, descriptorSet] : m_descriptorSets) {
            descriptorSetLayouts.push_back(*descriptorSet.layout);
        }

        vk::raii::PipelineLayout pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        });

        vk::raii::Pipeline pipeline = device.createGraphicsPipeline(nullptr, vk::GraphicsPipelineCreateInfo{
            .stageCount = 2,
            .pStages = shaderStageInfo,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportStateInfo,
            .pRasterizationState = &rasterizationStateInfo,
            .pMultisampleState = &multisampleStateInfo,
            .pDepthStencilState = &depthStencilStateInfo,
            .pColorBlendState = &colorBlendStateInfo,
            .pDynamicState = &dynamicStateInfo,
            .layout = *pipelineLayout,
            .renderPass = *renderPass,
            .subpass = 0,
            .basePipelineHandle = nullptr,
            .basePipelineIndex = -1
        });

        std::vector<vk::ClearValue> clearValues;
        clearValues.reserve(info.colorAttachments.size() + (depthAttachmentRef ? 1 : 0));
        for(const auto& [imageName, _] : info.colorAttachments) {
            auto& color = m_images.at(imageName).info.clearColor;

            vk::ClearColorValue clearColorValue;
            clearColorValue.setFloat32({ color.x, color.y, color.z, 1.0f });
            vk::ClearValue clearValue;
            clearValue.setColor(clearColorValue);
            clearValues.push_back(clearValue);
        }
        if(info.depthAttachment) {
            auto& depth = m_images.at(info.depthAttachment.value().first).info.clearDepth;

            vk::ClearDepthStencilValue clearDepthStencilValue;
            clearDepthStencilValue.setDepth(depth);
            vk::ClearValue clearValue;
            clearValue.setDepthStencil(clearDepthStencilValue);
            clearValues.push_back(clearValue);
        }

        m_renderPasses.emplace(name, RenderPass{
            .info = info.info,
            .pipeline = std::move(pipeline),
            .pipelineLayout = std::move(pipelineLayout),
            .renderPass = std::move(renderPass),
            .framebuffer = std::move(framebuffer),
            .clearValues = clearValues
        });
    }

    void FrameGraph::createComputePass(std::string_view name, const ComputePassInfo& info) {
        auto& device = Vulkan::getDevice();

        auto shaderModule = createShaderModule(info.computeShader);

        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
        descriptorSetLayouts.reserve(m_descriptorSets.size());
        for(const auto& [_, descriptorSet] : m_descriptorSets) {
            descriptorSetLayouts.push_back(*descriptorSet.layout);
        }

        vk::raii::PipelineLayout pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
                .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
                .pSetLayouts = descriptorSetLayouts.data(),
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr
        });

        vk::raii::Pipeline pipeline = device.createComputePipeline(nullptr, vk::ComputePipelineCreateInfo{
                .stage = vk::PipelineShaderStageCreateInfo{
                        .stage = vk::ShaderStageFlagBits::eCompute,
                        .module = *shaderModule,
                        .pName = "main"
                },
                .layout = *pipelineLayout,
                .basePipelineHandle = nullptr,
                .basePipelineIndex = -1
        });

        m_computePasses.emplace(name, ComputePass{
            .info = info,
            .pipeline = std::move(pipeline),
            .pipelineLayout = std::move(pipelineLayout)
        });
    }
}