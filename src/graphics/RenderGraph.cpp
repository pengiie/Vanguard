#include "RenderGraph.h"

#include "Vulkan.h"
#include "vk_mem_alloc.h"
#include "Allocator.h"
#include "../Logger.h"


namespace vanguard {

    ResourceRef RenderGraphBuilder::createImage(const std::string& name, const ImageInfo& info) {
        ResourceRef ref = m_resources.size();

        std::unique_ptr<ImageResource> resource = std::make_unique<ImageResource>();
        resource->name = name;
        resource->info = info;
        m_resources.push_back(std::move(resource));

        return ref;
    }

    BufferRef RenderGraphBuilder::createUniform(const std::string& name, const vanguard::UniformInfo& info) {
        ResourceRef ref = m_resources.size();

        std::unique_ptr<UniformResource> resource = std::make_unique<UniformResource>();
        resource->name = name;
        resource->info = info;
        m_resources.push_back(std::move(resource));

        return ref;
    }

    void RenderGraphBuilder::addRenderPass(const RenderPassInfo& info) {
        m_renderPasses.push_back(info);
    }

    void RenderGraphBuilder::setBackBuffer(ResourceRef resource) {
        m_backBuffer = resource;
    }

    ResourceRef RenderGraph::createImage(const ImageResource& resource, vk::ImageUsageFlags usageFlags) {
        ResourceRef ref = m_images.size();

        auto& device = Vulkan::getDevice();
        auto swapchainExtent = Vulkan::getSwapchainExtent();

        vk::raii::Image image = device.createImage(vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = resource.info.format,
            .extent = vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usageFlags,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined
        });

        Allocation allocation;
        VmaAllocationCreateInfo allocCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };
        vmaAllocateMemoryForImage(*Vulkan::getAllocator(), static_cast<VkImage>(*image), &allocCreateInfo, &allocation.allocation, &allocation.allocationInfo);
        image.bindMemory(allocation.allocationInfo.deviceMemory, allocation.allocationInfo.offset);

        vk::raii::ImageView imageView = device.createImageView(vk::ImageViewCreateInfo{
                .image = *image,
                .viewType = vk::ImageViewType::e2D,
                .format = resource.info.format,
                .subresourceRange = vk::ImageSubresourceRange{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                }
        });

        m_images.push_back(vanguard::Image{
                .name = resource.name,
                .info = resource.info,
                .imageData = vanguard::ImageData{
                        .image = std::move(image),
                        .imageView = std::move(imageView),
                        .allocation = std::move(allocation)
                }
        });

        return ref;
    }

    void RenderGraph::createDescriptorSetLayout(UniformFrequency frequency, const std::vector<vk::DescriptorSetLayoutBinding>& bindings) {
        m_descriptorSet.layouts[(uint8_t) frequency] = std::make_unique<vk::raii::DescriptorSetLayout>(Vulkan::getDevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        }));
    }

    void RenderGraph::createDescriptorSet(vanguard::UniformFrequency frequency) {
        const vk::DescriptorSetLayout layout = **m_descriptorSet.layouts[(uint8_t) frequency];
        std::vector<vk::raii::DescriptorSet> sets;
        sets.reserve(FRAMES_IN_FLIGHT);
        for(int i = 0; i < FRAMES_IN_FLIGHT; i++) {
            auto allocatedSets = Vulkan::getDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                .descriptorPool = *Vulkan::getDescriptorPool(),
                .descriptorSetCount = 1,
                .pSetLayouts = &layout
            });
            sets.push_back(std::move(allocatedSets[0]));
        }

        m_descriptorSet.sets[(uint8_t) frequency] = std::move(sets);
    }

    void RenderGraph::updateDescriptorBindings(vanguard::UniformFrequency frequency, uint32_t currentFrame,
                                               const std::vector<std::pair<uint32_t, vk::DescriptorBufferInfo>>& bufferBindings) {

        std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
        writeDescriptorSets.reserve(bufferBindings.size());
        for(const auto& [binding, bufferInfo] : bufferBindings) {
            writeDescriptorSets.push_back(vk::WriteDescriptorSet{
                    .dstSet = *m_descriptorSet.sets[(uint8_t) frequency][currentFrame],
                    .dstBinding = binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &bufferInfo
            });
        }
        Vulkan::getDevice().updateDescriptorSets(writeDescriptorSets, {});
    }

    static vk::raii::ShaderModule createShaderModule(const std::vector<uint32_t>& code) {
        return Vulkan::getDevice().createShaderModule(vk::ShaderModuleCreateInfo{
            .codeSize = code.size() * sizeof(uint32_t),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        });
    }

    struct RenderPassDebugInfo {
        std::string name;
        std::vector<std::pair<std::string, uint32_t>> inputs; /* name, index */
        std::vector<std::pair<std::string, uint32_t>> outputs; /* name, index */
    };
    RenderPassRef RenderGraph::addRenderPass(vanguard::ReferenceMap& imageReferenceMap,
                                             const vanguard::RenderPassInfo& info) {
        RenderPassRef ref = m_renderPasses.size();

        auto& device = Vulkan::getDevice();

        RenderPassDebugInfo debugInfo;
        debugInfo.name = info.name;

        std::vector<Image*> inputs;
        inputs.reserve(info.inputs.size());
        for(ResourceRef input : info.inputs) {
            // Sort out other input types
            if(imageReferenceMap.find(input) != imageReferenceMap.end()) {
                inputs.push_back(&m_images[imageReferenceMap.at(input)]);
            }
        }
        std::vector<Image*> outputs;
        outputs.reserve(info.outputs.size());
        for(ResourceRef output : info.outputs) {
            outputs.push_back(&m_images[imageReferenceMap.at(output)]);
        }

        std::vector<vk::AttachmentDescription> attachments;
        attachments.reserve(inputs.size() + outputs.size());
        for(Image* input : inputs) {
            attachments.push_back(vk::AttachmentDescription{
                .format = input->info.format,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eLoad,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
            debugInfo.inputs.emplace_back(input->name, static_cast<uint32_t>(attachments.size() - 1));
        }
        for(Image* output : outputs) {
            attachments.push_back(vk::AttachmentDescription{
                .format = output->info.format,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eColorAttachmentOptimal
            });
            debugInfo.outputs.emplace_back(output->name, static_cast<uint32_t>(attachments.size() - 1));
        }

        std::vector<vk::AttachmentReference> inputAttachments;
        inputAttachments.reserve(inputs.size());
        for(size_t i = 0; i < inputs.size(); i++) {
            inputAttachments.push_back(vk::AttachmentReference{
                .attachment = static_cast<uint32_t>(i),
                .layout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
        }

        std::vector<vk::AttachmentReference> outputAttachments;
        outputAttachments.reserve(outputs.size());
        for(size_t i = 0; i < outputs.size(); i++) {
            outputAttachments.push_back(vk::AttachmentReference{
                .attachment = static_cast<uint32_t>(i + inputs.size()),
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            });
        }

        vk::Extent2D swapchainExtent = Vulkan::getSwapchainExtent();

        vk::SubpassDescription subpass{
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .inputAttachmentCount = static_cast<uint32_t>(inputAttachments.size()),
                .pInputAttachments = inputAttachments.data(),
                .colorAttachmentCount = static_cast<uint32_t>(outputAttachments.size()),
                .pColorAttachments = outputAttachments.data(),
        };
        std::vector<vk::SubpassDependency> dependencies = {
                vk::SubpassDependency{
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    .srcAccessMask = {},
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                },
                vk::SubpassDependency{
                    .srcSubpass = 0,
                    .dstSubpass = VK_SUBPASS_EXTERNAL,
                    .srcStageMask = vk::PipelineStageFlagBits::eVertexInput,
                    .dstStageMask = vk::PipelineStageFlagBits::eVertexInput,
                    .srcAccessMask = {},
                    .dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead,
                }
        };
        vk::raii::RenderPass renderPass = device.createRenderPass(vk::RenderPassCreateInfo{
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = static_cast<uint32_t>(dependencies.size()),
                .pDependencies = dependencies.data(),
        });

        std::vector<vk::ImageView> imageViews;
        imageViews.reserve(inputs.size() + outputs.size());

        for(Image* input : inputs) {
            imageViews.push_back(*(input->imageData.imageView));
        }
        for(Image* output : outputs) {
            imageViews.push_back(*(output->imageData.imageView));
        }

        vk::raii::Framebuffer frameBuffer = device.createFramebuffer(vk::FramebufferCreateInfo{
            .renderPass = *renderPass,
            .attachmentCount = static_cast<uint32_t>(imageViews.size()),
            .pAttachments = imageViews.data(),
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
            .layers = 1,
        });

        vk::raii::ShaderModule vertexModule = createShaderModule(info.vertexShader);
        vk::raii::ShaderModule fragmentModule = createShaderModule(info.fragmentShader);

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
                .stride = info.vertexInput.m_stride,
                .inputRate = vk::VertexInputRate::eVertex
        };
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        attributeDescriptions.reserve(info.vertexInput.m_attributes.size());
        for(auto& attribute : info.vertexInput.m_attributes) {
            attributeDescriptions.push_back(vk::VertexInputAttributeDescription{
                    .location = attribute.location,
                    .binding = 0,
                    .format = attribute.format,
                    .offset = attribute.offset
            });
        }
        if(info.vertexInput.m_stride != 0) {
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

        // TODO: Add depth stencil support
        vk::PipelineDepthStencilStateCreateInfo depthStencilStateInfo{
                .depthTestEnable = VK_FALSE,
                .stencilTestEnable = VK_FALSE
        };

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
        descriptorSetLayouts.reserve(static_cast<uint8_t>(UniformFrequency::SIZE));
        for(auto& layout : m_descriptorSet.layouts) {
            if(!layout) continue;
            descriptorSetLayouts.push_back(**layout);
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
            .subpass = 0
        });

        std::vector<vk::ClearValue> clearValues;
        clearValues.reserve(info.outputs.size());
        vk::ClearColorValue clearColorValue;
        clearColorValue.setFloat32(info.clearColor);
        vk::ClearValue clearValue;
        clearValue.setColor(clearColorValue);
        for (auto& output : info.outputs) {
            clearValues.push_back(clearValue);
        }

        m_renderPasses.push_back(vanguard::RenderPass{
            .info = info,
            .clearValues = clearValues,
            .pipelineLayout = std::move(pipelineLayout),
            .pipeline = std::move(pipeline),
            .renderPass = std::move(renderPass),
            .framebuffer = std::move(frameBuffer)
        });

        {
            std::vector<std::string> debugAttachmentInputs;
            debugAttachmentInputs.reserve(debugInfo.inputs.size());
            for(auto& input : debugInfo.inputs) {
                debugAttachmentInputs.push_back(fmt::format("\t{1} - {0}", input.first, input.second));
            }
            std::vector<std::string> debugAttachmentsOutputs;
            debugAttachmentsOutputs.reserve(debugInfo.outputs.size());
            for(auto& output : debugInfo.outputs) {
                debugAttachmentsOutputs.push_back(fmt::format("\t{1} - {0}", output.first, output.second));
            }
            INFO("Render Pass Info - {}:\n"
                 "\tInputs:\n"
                 "{}\n"
                 "\tOutputs:\n"
                 "{}\n", debugInfo.name, fmt::join(debugAttachmentInputs, "\n"), fmt::join(debugAttachmentsOutputs, "\n"));
        }

        return ref;
    }

    void RenderGraph::execute(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex) {
        auto swapchainExtent = Vulkan::getSwapchainExtent();

        vk::DescriptorSet perFrameDescriptorSet = *m_descriptorSet.sets.at(static_cast<uint8_t>(UniformFrequency::PerFrame))[frameIndex];

        for (const auto& command: m_commands) {
            if(command->getType() == RenderGraphCommandType::RenderPass) {
                auto* renderPassCommand = reinterpret_cast<RenderPassCommand*>(command.get());
                auto& renderPass = m_renderPasses.at(renderPassCommand->pass);

                commandBuffer.beginRenderPass(vk::RenderPassBeginInfo{
                        .renderPass = *renderPass.renderPass,
                        .framebuffer = *renderPass.framebuffer,
                        .renderArea = vk::Rect2D{
                                .offset = vk::Offset2D{0, 0},
                                .extent = swapchainExtent
                        },
                        .clearValueCount = static_cast<uint32_t>(renderPass.clearValues.size()),
                        .pClearValues = renderPass.clearValues.data()
                }, vk::SubpassContents::eInline);
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *renderPass.pipeline);
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderPass.pipelineLayout, 0, perFrameDescriptorSet, {});

                renderPass.info.execution(*commandBuffer, frameIndex);

                commandBuffer.endRenderPass();
            } else if(command->getType() == RenderGraphCommandType::ImageBarrier) {
                auto* imageBarrierCommand = reinterpret_cast<ImageBarrierCommand*>(command.get());

                std::vector<vk::ImageMemoryBarrier> imageBarriers;
                imageBarriers.reserve(imageBarrierCommand->transitions.size());
                for (const auto& transition : imageBarrierCommand->transitions) {
                    auto& image = m_images.at(transition.image);
                    vk::ImageMemoryBarrier imageBarrier{
                            .srcAccessMask = transition.oldAccess,
                            .dstAccessMask = transition.newAccess,
                            .oldLayout = transition.oldLayout,
                            .newLayout = transition.newLayout,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .image = *m_images.at(transition.image).imageData.image,
                            .subresourceRange = vk::ImageSubresourceRange{
                                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                            }
                    };
                    imageBarriers.push_back(imageBarrier);
                }

                commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eAllGraphics,
                        vk::PipelineStageFlagBits::eAllGraphics,
                        {}, {}, {}, imageBarriers
                );
            }
        }
    }
}