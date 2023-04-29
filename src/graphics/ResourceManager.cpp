#include "ResourceManager.h"

#include "../Config.h"
#include "../Application.h"

namespace vanguard {

    ResourceRef ImagePool::create(const ImageInfo& info) {
        auto& device = Vulkan::getDevice();

        vk::raii::Image image = device.createImage(vk::ImageCreateInfo{
                .flags = info.type == ImageType::Cube ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags{},
                .imageType = vk::ImageType::e2D,
                .format = info.format,
                .extent = vk::Extent3D{ info.width, info.height, 1 },
                .mipLevels = 1,
                .arrayLayers = info.arrayLayers,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = info.usage,
                .sharingMode = vk::SharingMode::eExclusive,
                .initialLayout = info.initialLayout,
        });

        Allocation allocation;
        VmaAllocationCreateInfo allocInfo{
               // .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };
        vmaAllocateMemoryForImage(*Vulkan::getAllocator(), static_cast<VkImage>(*image), &allocInfo, &allocation.allocation, &allocation.allocationInfo);
        vmaBindImageMemory(*Vulkan::getAllocator(), allocation.allocation, static_cast<VkImage>(*image));

        vk::raii::ImageView view = device.createImageView(vk::ImageViewCreateInfo{
                .image = *image,
                .viewType = info.type == ImageType::Cube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D,
                .format = info.format,
                .subresourceRange = vk::ImageSubresourceRange{
                        .aspectMask = info.aspect,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = info.arrayLayers
                }
        });

        return allocate(Image{
                .info = info,
                .image = std::move(image),
                .view = std::move(view),
                .allocation = std::move(allocation)
        });
    }

    ResourceRef BufferPool::create(const BufferInfo& info) {
        auto& device = Vulkan::getDevice();

        vk::raii::Buffer buffer = device.createBuffer(vk::BufferCreateInfo{
                .size = info.size,
                .usage = info.usage,
                .sharingMode = vk::SharingMode::eExclusive
        });

        Allocation allocation;
        VmaAllocationCreateInfo allocInfo{
                .flags = info.memoryFlags,
              //  .usage = info.memoryUsage,
                .requiredFlags = static_cast<VkMemoryPropertyFlags>(info.memoryProperties),
        };
        vmaAllocateMemoryForBuffer(*Vulkan::getAllocator(), static_cast<VkBuffer>(*buffer), &allocInfo, &allocation.allocation, &allocation.allocationInfo);
        vmaBindBufferMemory(*Vulkan::getAllocator(), allocation.allocation, static_cast<VkBuffer>(*buffer));

        return allocate(Buffer{
                .info = info,
                .buffer = std::move(buffer),
                .allocation = std::move(allocation)
        });
    }

    ResourceRef SamplerPool::create(const SamplerInfo& info) {
        vk::raii::Sampler sampler = Vulkan::getDevice().createSampler(vk::SamplerCreateInfo{
                .magFilter = info.magFilter,
                .minFilter = info.minFilter,
                .mipmapMode = info.mipmapMode,
                .addressModeU = info.addressModeU,
                .addressModeV = info.addressModeV,
                .addressModeW = info.addressModeW,
        });

        return allocate(Sampler{
                .info = info,
                .sampler = std::move(sampler)
        });
    }

    ResourceRef DescriptorSetLayoutPool::create(const DescriptorSetLayoutInfo& info) {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        bindings.reserve(info.bindings.size());
        for (const auto& binding : info.bindings) {
            bindings.push_back(vk::DescriptorSetLayoutBinding{
                    .binding = binding.binding,
                    .descriptorType = binding.type,
                    .descriptorCount = binding.count,
                    .stageFlags = binding.stages,
                    .pImmutableSamplers = nullptr
            });
        }

        vk::raii::DescriptorSetLayout layout = Vulkan::getDevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data()
        });

        return allocate(DescriptorSetLayout{
                .info = info,
                .layout = std::move(layout)
        });
    }

    ResourceRef DescriptorSetPool::create(const vanguard::DescriptorSetInfo& info) {
        auto& device = Vulkan::getDevice();
        auto& pool = *Vulkan::getDescriptorPool();

        vk::DescriptorSetLayout layout = *RENDER_SYSTEM.getResourceManager().getDescriptorSetLayout(info.layout).layout;

        auto sets = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                .descriptorPool = pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout
        });

        return allocate(DescriptorSet{
                .info = info,
                .set = std::move(sets[0])
        });
    }

    void DescriptorSetPool::update(ResourceRef ref, const std::vector<DescriptorSetWrite>& writes) const {
        auto& set = get(ref);

        std::vector<vk::WriteDescriptorSet> vkWrites;
        std::vector<std::optional<vk::DescriptorImageInfo>> imageInfos(writes.size());
        std::vector<std::optional<vk::DescriptorBufferInfo>> bufferInfos(writes.size());
        vkWrites.reserve(writes.size());
        for (int i = 0; i < writes.size(); ++i) {
            auto& write = writes[i];
            if(!write.image.has_value() && !write.buffer.has_value()) {
                throw std::runtime_error("DescriptorSetWrite must have either image or buffer");
            }

            if(write.image.has_value()) {
                auto& image = RENDER_SYSTEM.getResourceManager().getImage(write.image->image);
                imageInfos[i] = vk::DescriptorImageInfo{
                        .imageView = *image.view,
                        .imageLayout = write.image->imageLayout
                };
                if(write.image->sampler != UNDEFINED_RESOURCE) {
                    auto& sampler = RENDER_SYSTEM.getResourceManager().getSampler(write.image->sampler);
                    imageInfos[i]->sampler = *sampler.sampler;
                }
            }
            if(write.buffer.has_value()) {
                auto& buffer = RENDER_SYSTEM.getResourceManager().getBuffer(write.buffer->buffer);
                bufferInfos[i] = vk::DescriptorBufferInfo{
                        .buffer = *buffer.buffer,
                        .offset = write.buffer->offset,
                        .range = write.buffer->size
                };
            }

            vkWrites.push_back(vk::WriteDescriptorSet{
                    .dstSet = *set.set,
                    .dstBinding = write.binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = write.type,
                    .pImageInfo = imageInfos[i].has_value() ? &imageInfos[i].value() : nullptr,
                    .pBufferInfo = bufferInfos[i].has_value() ? &bufferInfos[i].value() : nullptr,
                    .pTexelBufferView = nullptr
            });
        }
        Vulkan::getDevice().updateDescriptorSets(vkWrites, {});
    }

    void DescriptorSetPool::cmdUpdate(vk::CommandBuffer& cmd, ResourceRef ref, const std::vector<DescriptorSetWrite>& writes) const {
        // TODO
    }

    static vk::raii::ShaderModule createShaderModule(const std::vector<uint32_t>& code) {
        return Vulkan::getDevice().createShaderModule(vk::ShaderModuleCreateInfo{
                .codeSize = code.size() * sizeof(uint32_t),
                .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        });
    }

    ResourceRef RenderPipelinePool::create(const RenderPipelineInfo& info) {
        std::vector<vk::AttachmentDescription> attachments;
        std::vector<vk::ImageView> attachmentViews;
        std::vector<vk::AttachmentReference> inputReferences;
        std::vector<vk::AttachmentReference> colorReferences;
        std::optional<vk::AttachmentReference> depthStencilReference;

        for (const RenderPipelineImageInfo& imageInfo: info.inputAttachments) {
            auto& image = RENDER_SYSTEM.getResourceManager().getImage(imageInfo.image);
            attachments.push_back(vk::AttachmentDescription{
                    .format = image.info.format,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = imageInfo.loadOp,
                    .storeOp = imageInfo.storeOp,
                    .stencilLoadOp = imageInfo.loadOp,
                    .stencilStoreOp = imageInfo.storeOp,
                    .initialLayout = imageInfo.initialLayout,
                    .finalLayout = imageInfo.finalLayout
            });
            attachmentViews.push_back(*image.view);
            inputReferences.push_back(vk::AttachmentReference{
                    .attachment = static_cast<uint32_t>(attachments.size() - 1),
                    .layout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
        }
        for (const RenderPipelineImageInfo& imageInfo: info.colorAttachments) {
            auto& image = RENDER_SYSTEM.getResourceManager().getImage(imageInfo.image);
            attachments.push_back(vk::AttachmentDescription{
                    .format = image.info.format,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = imageInfo.loadOp,
                    .storeOp = imageInfo.storeOp,
                    .stencilLoadOp = imageInfo.loadOp,
                    .stencilStoreOp = imageInfo.storeOp,
                    .initialLayout = imageInfo.initialLayout,
                    .finalLayout = imageInfo.finalLayout
            });
            attachmentViews.push_back(*image.view);
            colorReferences.push_back(vk::AttachmentReference{
                    .attachment = static_cast<uint32_t>(attachments.size() - 1),
                    .layout = vk::ImageLayout::eColorAttachmentOptimal
            });
        }
        if(info.depthStencilAttachment.image != UNDEFINED_RESOURCE) {
            auto& image = RENDER_SYSTEM.getResourceManager().getImage(info.depthStencilAttachment.image);
            attachments.push_back(vk::AttachmentDescription{
                    .format = image.info.format,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = info.depthStencilAttachment.loadOp,
                    .storeOp = info.depthStencilAttachment.storeOp,
                    .stencilLoadOp = info.depthStencilAttachment.loadOp,
                    .stencilStoreOp = info.depthStencilAttachment.storeOp,
                    .initialLayout = info.depthStencilAttachment.initialLayout,
                    .finalLayout = info.depthStencilAttachment.finalLayout
            });
            attachmentViews.push_back(*image.view);
            depthStencilReference = vk::AttachmentReference{
                    .attachment = static_cast<uint32_t>(attachments.size() - 1),
                    .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
            };
        }

        vk::SubpassDescription subpass{
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .inputAttachmentCount = static_cast<uint32_t>(inputReferences.size()),
                .pInputAttachments = inputReferences.data(),
                .colorAttachmentCount = static_cast<uint32_t>(colorReferences.size()),
                .pColorAttachments = colorReferences.data(),
                .pDepthStencilAttachment = depthStencilReference.has_value() ? &depthStencilReference.value() : nullptr
        };

        std::vector<vk::SubpassDependency> subpassDependencies;
        subpassDependencies.push_back(vk::SubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead,
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        });
        if(depthStencilReference.has_value()) {
            subpassDependencies.push_back(vk::SubpassDependency{
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead,
                    .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            });
        }

        vk::raii::RenderPass renderPass = Vulkan::getDevice().createRenderPass(vk::RenderPassCreateInfo{
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
                .pDependencies = subpassDependencies.data()
        });
        vk::raii::Framebuffer framebuffer = Vulkan::getDevice().createFramebuffer(vk::FramebufferCreateInfo{
                .renderPass = *renderPass,
                .attachmentCount = static_cast<uint32_t>(attachmentViews.size()),
                .pAttachments = attachmentViews.data(),
                .width = info.extent.width,
                .height = info.extent.height,
                .layers = 1
        });

        vk::raii::ShaderModule vertexShader = createShaderModule(ASSETS.get<SpirVShaderCode>(info.vertexShaderPath));
        vk::raii::ShaderModule fragmentShader = createShaderModule(ASSETS.get<SpirVShaderCode>(info.fragmentShaderPath));

        vk::PipelineShaderStageCreateInfo shaderStageInfo[] = {
                vk::PipelineShaderStageCreateInfo{
                        .stage = vk::ShaderStageFlagBits::eVertex,
                        .module = *vertexShader,
                        .pName = "main"
                },
                vk::PipelineShaderStageCreateInfo{
                        .stage = vk::ShaderStageFlagBits::eFragment,
                        .module = *fragmentShader,
                        .pName = "main"
                }
        };

        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        vk::VertexInputBindingDescription bindingDescription{};
        if(info.vertexInputData.has_value()) {
            bindingDescription = {
                    .binding = 0,
                    .stride = info.vertexInputData->getStride() ,
                    .inputRate = vk::VertexInputRate::eVertex
            };
            attributeDescriptions.reserve(info.vertexInputData->getAttributes().size());
            for(const auto& attribute : info.vertexInputData->getAttributes()) {
                attributeDescriptions.push_back(vk::VertexInputAttributeDescription{
                        .location = attribute.location,
                        .binding = 0,
                        .format = attribute.format,
                        .offset = attribute.offset
                });
            }
        }
        auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &bindingDescription,
                .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
                .pVertexAttributeDescriptions = attributeDescriptions.data()
        };

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
                .topology = vk::PrimitiveTopology::eTriangleList,
                .primitiveRestartEnable = VK_FALSE
        };

        vk::Viewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(info.extent.width),
                .height = static_cast<float>(info.extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
        };
        vk::Rect2D scissor{
                .offset = vk::Offset2D{0, 0},
                .extent = info.extent
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
        if(info.depthStencilAttachment.image != UNDEFINED_RESOURCE) {
            depthStencilStateInfo.depthTestEnable = info.depthTest ? VK_TRUE : VK_FALSE;
            depthStencilStateInfo.depthWriteEnable = info.depthWrite ? VK_TRUE : VK_FALSE;
            depthStencilStateInfo.depthCompareOp = vk::CompareOp::eLess;
            depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
            depthStencilStateInfo.minDepthBounds = 0.0f;
            depthStencilStateInfo.maxDepthBounds = 1.0f;
            depthStencilStateInfo.stencilTestEnable = VK_FALSE;
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
        descriptorSetLayouts.reserve(info.descriptorSetLayouts.size());
        for(ResourceRef reference : info.descriptorSetLayouts) {
            auto& layout = RENDER_SYSTEM.getResourceManager().getDescriptorSetLayout(reference);
            descriptorSetLayouts.push_back(*layout.layout);
        }

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
                .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
                .pSetLayouts = descriptorSetLayouts.data(),
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr
        };
        vk::raii::PipelineLayout pipelineLayout = Vulkan::getDevice().createPipelineLayout(pipelineLayoutInfo);

        vk::raii::Pipeline pipeline = Vulkan::getDevice().createGraphicsPipeline(nullptr, vk::GraphicsPipelineCreateInfo{
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

        return allocate(RenderPipeline{
            .info = info,
            .renderPass = std::move(renderPass),
            .framebuffer = std::move(framebuffer),
            .pipelineLayout = std::move(pipelineLayout),
            .pipeline = std::move(pipeline)
        });
    }

    ResourceRef ComputePipelinePool::create(const ComputePipelineInfo& info) {
        vk::raii::ShaderModule computeShader = createShaderModule(ASSETS.get<SpirVShaderCode>(info.computeShaderPath));

        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
        descriptorSetLayouts.reserve(info.descriptorSetLayouts.size());
        for(ResourceRef reference : info.descriptorSetLayouts) {
            auto& layout = RENDER_SYSTEM.getResourceManager().getDescriptorSetLayout(reference);
            descriptorSetLayouts.push_back(*layout.layout);
        }

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
                .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
                .pSetLayouts = descriptorSetLayouts.data(),
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr
        };
        vk::raii::PipelineLayout pipelineLayout = Vulkan::getDevice().createPipelineLayout(pipelineLayoutInfo);

        vk::raii::Pipeline pipeline = Vulkan::getDevice().createComputePipeline(nullptr, vk::ComputePipelineCreateInfo{
                .stage = vk::PipelineShaderStageCreateInfo{
                        .stage = vk::ShaderStageFlagBits::eCompute,
                        .module = *computeShader,
                        .pName = "main"
                },
                .layout = *pipelineLayout
        });
        return allocate(ComputePipeline{
            .info = info,
            .pipelineLayout = std::move(pipelineLayout),
            .pipeline = std::move(pipeline)
        });
    }
}