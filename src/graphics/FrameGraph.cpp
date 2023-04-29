#include "FrameGraph.h"
#include "../Application.h"
#include "../util/TypeTraits.h"

namespace vanguard {
    FrameGraph::~FrameGraph() {
        for (auto& image : m_images) {
            RENDER_SYSTEM.getResourceManager().destroyImage(image);
        }
        for (auto& pipeline : m_pipelines) {
            RENDER_SYSTEM.getResourceManager().destroyRenderPipeline(pipeline);
        }
        if(!m_descriptorSets.empty()) {
            for (auto& [location, descriptorSet]: m_descriptorSets) {
                descriptorSet.destroy();
            }
        }
    }

    FrameGraph::FrameGraph(FrameGraph&& other) noexcept {
        m_commands = std::move(other.m_commands);
        m_images = std::move(other.m_images);
        other.m_images.clear();
        m_pipelines = std::move(other.m_pipelines);
        other.m_pipelines.clear();
        m_descriptorSets = std::move(other.m_descriptorSets);
        other.m_descriptorSets.clear();
    }

    FrameGraph& FrameGraph::operator=(FrameGraph&& other) noexcept {
        m_commands = std::move(other.m_commands);
        m_images = std::move(other.m_images);
        other.m_images.clear();
        m_pipelines = std::move(other.m_pipelines);
        other.m_pipelines.clear();
        m_descriptorSets = std::move(other.m_descriptorSets);
        other.m_descriptorSets.clear();
        return *this;
    }

    FGBResourceRef FrameGraphBuilder::createImage(const vanguard::FGBImageInfo& info) {
        m_images.push_back(info);
        return {
            FGBResourceType::Image,
            static_cast<uint32_t>(m_images.size() - 1)
        };
    }

    FGBResourceRef FrameGraphBuilder::createDepthStencil(const vanguard::FGBDepthStencilInfo& info) {
        m_depthStencils.push_back(info);
        return {
            FGBResourceType::DepthStencil,
            static_cast<uint32_t>(m_depthStencils.size() - 1)
        };
    }

    FGBResourceRef FrameGraphBuilder::addRenderPass(const vanguard::FGBRenderPassInfo& info) {
        m_passes.emplace_back(info);
        return {
            FGBResourceType::RenderPass,
            static_cast<uint32_t>(m_passes.size() - 1)
        };
    }

    FGBResourceRef FrameGraphBuilder::addComputePass(const vanguard::FGBComputePassInfo& info) {
        m_passes.emplace_back(info);
        return {
            FGBResourceType::ComputePass,
            static_cast<uint32_t>(m_passes.size() - 1)
        };
    }

    FGBResourceRef FrameGraphBuilder::addUniformBuffer(uint32_t location, uint32_t binding, const UniformBuffer* buffer) {
        m_uniforms.emplace_back(FGBUniformBufferInfo{ location, binding, buffer });
        return {
            FGBResourceType::UniformBuffer,
            static_cast<uint32_t>(m_uniforms.size() - 1)
        };
    }

    FGBResourceRef FrameGraphBuilder::addUniformSampledImage(uint32_t location, uint32_t binding, FGBResourceRef image, const SamplerInfo& samplerInfo) {
        m_uniforms.emplace_back(FGBUniformSampledImageInfo{
            .location = location,
            .binding = binding,
            .image = image,
            .samplerInfo = samplerInfo
        });
        return {
            FGBResourceType::UniformSampledImage,
            static_cast<uint32_t>(m_uniforms.size() - 1)
        };
    }

    FGBResourceRef FrameGraphBuilder::addUniformSampledImage(uint32_t location, uint32_t binding,
                                                             const vanguard::Texture* texture,
                                                             const vanguard::SamplerInfo& samplerInfo) {
        m_uniforms.emplace_back(FGBUniformSampledImageInfo{
            .location = location,
            .binding = binding,
            .texture = texture,
            .samplerInfo = samplerInfo
        });
        return {
            FGBResourceType::UniformSampledImage,
            static_cast<uint32_t>(m_uniforms.size() - 1)
        };
    }

    FGBResourceRef FrameGraphBuilder::addUniformStorageImage(uint32_t location, uint32_t binding, FGBResourceRef image) {
        m_uniforms.emplace_back(FGBUniformStorageImageInfo{ location, binding, image });
        return {
            FGBResourceType::UniformStorageImage,
            static_cast<uint32_t>(m_uniforms.size() - 1)
        };
    }

    void FrameGraphBuilder::setBackbuffer(FGBResourceRef image) {
        m_backbuffer = image;
    }

    // Frame Graph Baking
    static uint32_t toActualWidth(uint32_t width) {
        return width == FGB_SWAPCHAIN_EXTENT ? Vulkan::getSwapchainExtent().width : width;
    }

    static uint32_t toActualHeight(uint32_t height) {
        return height == FGB_SWAPCHAIN_EXTENT ? Vulkan::getSwapchainExtent().height : height;
    }

    static std::pair<std::vector<FGBResourceRef>, std::vector<FGBResourceRef>> getPassInputsAndOutputs(const FGBPassInfo& pass) {
        return std::visit([&](const auto& pass) {
            using T = std::decay_t<decltype(pass)>;
            if constexpr (std::is_same_v<T, FGBRenderPassInfo>) {
                return std::make_pair(pass.inputs, pass.outputs);
            }
            else if constexpr (std::is_same_v<T, FGBComputePassInfo>) {
                return std::make_pair(pass.inputs, pass.outputs);
            }
        }, pass);
    }

    FrameGraph FrameGraphBuilder::bake() {
        if(m_backbuffer.location == FGB_UNDEFINED_RESOURCE)
            throw std::runtime_error("Backbuffer not set");

        // Calculate image usages
        std::unordered_map<uint32_t, vk::ImageUsageFlags> imageUsages;
        std::unordered_map<uint32_t, vk::ImageUsageFlags> depthStencilUsages;

        for (const auto& pass: m_passes) {
            auto [inputs, outputs] = getPassInputsAndOutputs(pass);
            for (const auto& input: inputs) {
                switch(input.type) {
                    case FGBResourceType::Image:
                        imageUsages[input.location] |= vk::ImageUsageFlagBits::eSampled;
                        break;
                    case FGBResourceType::DepthStencil:
                        depthStencilUsages[input.location] |= vk::ImageUsageFlagBits::eSampled;
                        break;
                    case FGBResourceType::UniformSampledImage: {
                        auto& uniform = std::get<FGBUniformSampledImageInfo>(m_uniforms[input.location]);
                        if(uniform.image.has_value())
                            imageUsages[uniform.image->location] |= vk::ImageUsageFlagBits::eSampled;
                        break;
                    }
                    case FGBResourceType::UniformStorageImage: {
                        auto& uniform = std::get<FGBUniformStorageImageInfo>(m_uniforms[input.location]);
                        imageUsages[uniform.image.location] |= vk::ImageUsageFlagBits::eStorage;
                        break;
                    }
                    default:
                        break;
                }
            }
            for (const auto& output: outputs) {
                if(output.type == FGBResourceType::Image)
                    imageUsages[output.location] |= vk::ImageUsageFlagBits::eColorAttachment;
                else if(output.type == FGBResourceType::DepthStencil)
                    depthStencilUsages[output.location] |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                else if(output.type == FGBResourceType::UniformStorageImage) {
                    auto& uniform = std::get<FGBUniformStorageImageInfo>(m_uniforms[output.location]);
                    imageUsages[uniform.image.location] |= vk::ImageUsageFlagBits::eStorage;
                }
            }
        }
        imageUsages[m_backbuffer.location] |= vk::ImageUsageFlagBits::eTransferSrc;

        FrameGraph graph;

        // Create images
        std::unordered_map<FGBResourceRef, ResourceRef> imageLocations;
        for (int i = 0; i < m_images.size(); ++i) {
            if(imageUsages.find(i) == imageUsages.end())
                continue;

            const auto& info = m_images[i];
            graph.m_images.push_back(RENDER_SYSTEM.getResourceManager().createImage(ImageInfo{
                    .format = info.format,
                    .usage = imageUsages[i],
                    .aspect = vk::ImageAspectFlagBits::eColor,
                    .width = toActualWidth(info.extent.width),
                    .height = toActualHeight(info.extent.height)
            }));
            imageLocations.emplace(FGBResourceRef{ FGBResourceType::Image, static_cast<uint32_t>(i) }, graph.m_images.back());
        }
        for (int i = 0; i < m_depthStencils.size(); ++i) {
            if(depthStencilUsages.find(i) == depthStencilUsages.end())
                continue;

            const auto& info = m_depthStencils[i];
            graph.m_images.push_back(RENDER_SYSTEM.getResourceManager().createImage(ImageInfo{
                    .format = info.format,
                    .usage = depthStencilUsages[i],
                    .aspect = vk::ImageAspectFlagBits::eDepth,
                    .width = toActualWidth(info.extent.width),
                    .height = toActualHeight(info.extent.height)
            }));
            imageLocations.emplace(FGBResourceRef{ FGBResourceType::DepthStencil, static_cast<uint32_t>(i) }, graph.m_images.back());
        }

        std::vector<ResourceRef> samplers;

        std::unordered_map<uint32_t, std::vector<DescriptorSetBinding>> descriptorBindings;
        std::unordered_map<uint32_t, uint32_t> uniformLocations;
        std::unordered_map<uint32_t, ResourceRef> uniformDescriptorMap;
        std::unordered_map<uint32_t, std::vector<std::vector<DescriptorSetWrite>>> descriptorWrites;
        for (int i = 0; i < m_uniforms.size(); i++) {
            const auto& reference = m_uniforms[i];
            auto [location, binding] = std::visit([&](const auto& uniform) {
                using T = std::decay_t<decltype(uniform)>;
                if constexpr (std::is_same_v<T, FGBUniformBufferInfo>) {
                    descriptorWrites[uniform.location].resize(FRAMES_IN_FLIGHT);
                    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
                        descriptorWrites[uniform.location][i].push_back(DescriptorSetWrite{
                                .binding = uniform.binding,
                                .type = vk::DescriptorType::eUniformBuffer,
                                .buffer = DescriptorBufferInfo{
                                        .buffer = uniform.buffer->getBuffer(),
                                        .offset = uniform.buffer->getStride() * i,
                                        .size = uniform.buffer->getStride()
                                }
                        });
                    return std::pair<uint32_t, DescriptorSetBinding>{uniform.location, DescriptorSetBinding{
                        .binding = uniform.binding,
                        .type = vk::DescriptorType::eUniformBuffer,
                        .count = 1
                    }};
                }
                else if constexpr (std::is_same_v<T, FGBUniformSampledImageInfo>) {
                    ResourceRef image = uniform.image.has_value() ? imageLocations[*uniform.image] : (*uniform.texture)->getImage();
                    descriptorWrites[uniform.location].resize(FRAMES_IN_FLIGHT);

                    auto sampler = RENDER_SYSTEM.getResourceManager().createSampler(uniform.samplerInfo);
                    samplers.push_back(sampler);

                    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
                        descriptorWrites[uniform.location][i].push_back(DescriptorSetWrite{
                                .binding = uniform.binding,
                                .type = vk::DescriptorType::eCombinedImageSampler,
                                .image = DescriptorImageInfo{
                                        .image = image,
                                        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                        .sampler = sampler
                                }
                        });
                    return std::pair<uint32_t, DescriptorSetBinding>{uniform.location, DescriptorSetBinding{
                            .binding = uniform.binding,
                            .type = vk::DescriptorType::eCombinedImageSampler,
                            .count = 1
                    }};
                }
                else if constexpr (std::is_same_v<T, FGBUniformStorageImageInfo>) {
                    ResourceRef image = imageLocations[uniform.image];
                    descriptorWrites[uniform.location].resize(FRAMES_IN_FLIGHT);

                    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
                        descriptorWrites[uniform.location][i].push_back(DescriptorSetWrite{
                                .binding = uniform.binding,
                                .type = vk::DescriptorType::eStorageImage,
                                .image = DescriptorImageInfo{
                                        .image = image,
                                        .imageLayout = vk::ImageLayout::eGeneral,
                                }
                        });
                    return std::pair<uint32_t, DescriptorSetBinding>{uniform.location, DescriptorSetBinding{
                            .binding = uniform.binding,
                            .type = vk::DescriptorType::eStorageImage,
                            .count = 1
                    }};
                }
            }, reference);
            descriptorBindings[location].push_back(binding);
            uniformLocations[i] = location;
        }
        std::unordered_map<uint32_t, ResourceRef> descriptorLayouts;
        for (const auto& [location, bindings]: descriptorBindings) {
            descriptorLayouts[location] = RENDER_SYSTEM.getResourceManager().createDescriptorSetLayout({ .bindings = bindings });
        }
        for (int i = 0; i < m_uniforms.size(); i++) {
            uniformDescriptorMap.emplace(i, descriptorLayouts[uniformLocations[i]]);
        }
        std::unordered_map<uint32_t, FrameGraph::DescriptorSet> descriptorSets;
        for (const auto& [location, layout]: descriptorLayouts) {
            std::vector<ResourceRef> sets;
            const auto& writes = descriptorWrites[location];
            for(int i = 0; i < FRAMES_IN_FLIGHT; i++) {
                sets.push_back(RENDER_SYSTEM.getResourceManager().createDescriptorSet({ .layout = layout }));
                RENDER_SYSTEM.getResourceManager().updateDescriptorSet(sets.back(), writes[i]);
            }
            descriptorSets.emplace(location, FrameGraph::DescriptorSet(location, layout, sets));
        }
        graph.m_descriptorSets = std::move(descriptorSets);

        std::vector<ResourceRef> renderPasses;
        std::vector<ResourceRef> computePasses;
        std::unordered_set<FGBResourceRef> imagesUsed;

        std::unordered_map<ResourceRef, vk::ImageLayout> imageLayouts;
        std::unordered_map<ResourceRef, vk::AccessFlags> imageAccesses;
        std::unordered_set<ResourceRef> nativeImages;
        for (int i = 0; i < m_images.size(); ++i) {
            imageLayouts.emplace(imageLocations[FGBResourceRef{ FGBResourceType::Image, static_cast<uint32_t>(i) }], vk::ImageLayout::eUndefined);
            imageAccesses.emplace(imageLocations[FGBResourceRef{ FGBResourceType::Image, static_cast<uint32_t>(i) }], vk::AccessFlagBits::eNoneKHR);
        }
        for (int i = 0; i < m_depthStencils.size(); ++i) {
            imageLayouts.emplace(imageLocations[FGBResourceRef{ FGBResourceType::DepthStencil, static_cast<uint32_t>(i) }], vk::ImageLayout::eUndefined);
            imageAccesses.emplace(imageLocations[FGBResourceRef{ FGBResourceType::DepthStencil, static_cast<uint32_t>(i) }], vk::AccessFlagBits::eNoneKHR);
        }
        for (const auto& uniform: m_uniforms) {
            std::visit([&](const auto& uniform) {
                using T = std::decay_t<decltype(uniform)>;
                if constexpr (std::is_same_v<T, FGBUniformSampledImageInfo>) {
                    if(uniform.texture.has_value()) {
                        auto imageRef = (*uniform.texture)->getImage();
                        imageLayouts.emplace(imageRef, vk::ImageLayout::eShaderReadOnlyOptimal);
                        imageAccesses.emplace(imageRef, vk::AccessFlagBits::eShaderRead);
                    }
                }
            }, uniform);
        }

        std::vector<std::pair<PipelineBarrierCommand, uint32_t>> pipelineBarriers;

        std::vector<Command> commands;
        for (int i = 0; i < m_passes.size(); ++i) {
            const auto& passInfo = m_passes[i];
            std::optional<PipelineBarrierCommand> barrier;
            Command command = std::visit([&](const auto& pass) {
                using T = std::decay_t<decltype(pass)>;
                if constexpr (std::is_same_v<T, FGBRenderPassInfo>) {
                    std::unordered_set<ResourceRef> descriptorSetsUsed;
                    std::vector<RenderPipelineImageInfo> inputAttachments;
                    std::vector<RenderPipelineImageInfo> colorAttachments;
                    RenderPipelineImageInfo depthStencilAttachment{};

                    std::vector<std::pair<ResourceRef, vk::ImageLayout>> imageLayoutsToTransition;
                    std::vector<std::pair<ResourceRef, vk::AccessFlags>> imageAccessesToTransition;

                    for(const auto& input: pass.inputs) {
                        switch(input.type) {
                            case FGBResourceType::Image:
                                if(imageLayouts[imageLocations[input]] != vk::ImageLayout::eShaderReadOnlyOptimal) {
                                    imageLayoutsToTransition.emplace_back(imageLocations[input], vk::ImageLayout::eShaderReadOnlyOptimal);
                                }
                                if(imageAccesses[imageLocations[input]] != vk::AccessFlagBits::eShaderRead) {
                                    imageAccessesToTransition.emplace_back(imageLocations[input], vk::AccessFlagBits::eShaderRead);
                                }
                                inputAttachments.push_back(RenderPipelineImageInfo{
                                        .image = imageLocations[input],
                                        .initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                        .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                        .loadOp = vk::AttachmentLoadOp::eLoad,
                                        .storeOp = vk::AttachmentStoreOp::eDontCare,
                                });
                                break;
                            case FGBResourceType::DepthStencil:
                                if(imageLayouts[imageLocations[input]] != vk::ImageLayout::eDepthStencilReadOnlyOptimal) {
                                    imageLayoutsToTransition.emplace_back(imageLocations[input], vk::ImageLayout::eDepthStencilReadOnlyOptimal);
                                }
                                if(imageAccesses[imageLocations[input]] != vk::AccessFlagBits::eShaderRead) {
                                    imageAccessesToTransition.emplace_back(imageLocations[input], vk::AccessFlagBits::eShaderRead);
                                }
                                inputAttachments.push_back(RenderPipelineImageInfo{
                                        .image = imageLocations[input],
                                        .initialLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
                                        .finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
                                        .loadOp = vk::AttachmentLoadOp::eLoad,
                                        .storeOp = vk::AttachmentStoreOp::eDontCare,
                                });
                                break;
                            case FGBResourceType::UniformSampledImage: {
                                const auto& uniform = std::get<FGBUniformSampledImageInfo>(m_uniforms[input.location]);
                                ResourceRef imageRef = uniform.image.has_value() ? imageLocations[*uniform.image] : (*uniform.texture)->getImage();
                                if(imageLayouts[imageRef] != vk::ImageLayout::eShaderReadOnlyOptimal) {
                                    imageLayoutsToTransition.emplace_back(imageRef, vk::ImageLayout::eShaderReadOnlyOptimal);
                                }
                                if(imageAccesses[imageRef] != vk::AccessFlagBits::eShaderRead) {
                                    imageAccessesToTransition.emplace_back(imageRef, vk::AccessFlagBits::eShaderRead);
                                }
                                descriptorSetsUsed.insert(uniformDescriptorMap[input.location]);
                                break;
                            }
                            case FGBResourceType::UniformStorageImage: {
                                const auto& imageRef = std::get<FGBUniformStorageImageInfo>(m_uniforms[input.location]).image;
                                if(imageLayouts[imageLocations[imageRef]] != vk::ImageLayout::eGeneral) {
                                    imageLayoutsToTransition.emplace_back(imageLocations[imageRef], vk::ImageLayout::eGeneral);
                                }
                                if(imageAccesses[imageLocations[imageRef]] != vk::AccessFlagBits::eShaderRead) {
                                    imageAccessesToTransition.emplace_back(imageLocations[input], vk::AccessFlagBits::eShaderRead);
                                }
                                descriptorSetsUsed.insert(uniformDescriptorMap[input.location]);
                                break;
                            }
                            case FGBResourceType::UniformBuffer: {
                                descriptorSetsUsed.insert(uniformDescriptorMap[input.location]);
                                break;
                            }
                            default:
                                throw std::runtime_error("Invalid input type");
                        }
                    }
                    for(const auto& output: pass.outputs) {
                        switch(output.type) {
                            case FGBResourceType::Image: {
                                auto [_, shouldClear] = imagesUsed.insert(output);
                                colorAttachments.push_back(RenderPipelineImageInfo{
                                        .image = imageLocations[output],
                                        .initialLayout = imageLayouts[imageLocations[output]],
                                        .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                        .loadOp = shouldClear ? vk::AttachmentLoadOp::eClear
                                                              : vk::AttachmentLoadOp::eLoad,
                                        .storeOp = vk::AttachmentStoreOp::eStore,
                                });
                                imageLayouts[imageLocations[output]] = vk::ImageLayout::eColorAttachmentOptimal;
                                imageAccesses[imageLocations[output]] = vk::AccessFlagBits::eColorAttachmentWrite;
                                break;
                            }
                            case FGBResourceType::DepthStencil: {
                                auto [_, shouldClear] = imagesUsed.insert(output);
                                depthStencilAttachment = RenderPipelineImageInfo{
                                        .image = imageLocations[output],
                                        .initialLayout = imageLayouts[imageLocations[output]],
                                        .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                        .loadOp = shouldClear ? vk::AttachmentLoadOp::eClear
                                                              : vk::AttachmentLoadOp::eLoad,
                                        .storeOp = vk::AttachmentStoreOp::eStore,
                                };
                                imageLayouts[imageLocations[output]] = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                                imageAccesses[imageLocations[output]] = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                                break;
                            }
                            case FGBResourceType::UniformStorageImage: {
                                const auto& imageRef = std::get<FGBUniformStorageImageInfo>(m_uniforms[output.location]).image;
                                if(imageLayouts[imageLocations[imageRef]] != vk::ImageLayout::eGeneral) {
                                    imageLayoutsToTransition.emplace_back(imageLocations[imageRef], vk::ImageLayout::eGeneral);
                                }
                                if(imageAccesses[imageLocations[imageRef]] != vk::AccessFlagBits::eShaderRead) {
                                    imageAccessesToTransition.emplace_back(imageLocations[output], vk::AccessFlagBits::eShaderWrite);
                                }
                                descriptorSetsUsed.insert(uniformDescriptorMap[output.location]);
                                break;
                            }
                            default:
                                throw std::runtime_error("Invalid output type");
                        }
                    }

                    if(!imageLayoutsToTransition.empty()) {
                        std::vector<ImageBarrierInfo> imageBarriers;
                        for(const auto& [image, layout]: imageLayoutsToTransition) {
                            vk::AccessFlags newAccess;
                            for (const auto& item: imageAccessesToTransition) {
                                if(item.first == image) {
                                    newAccess = item.second;
                                    break;
                                }
                            }
                            imageBarriers.push_back(ImageBarrierInfo{
                                    .image = image,
                                    .oldLayout = imageLayouts[image],
                                    .newLayout = layout,
                                    .srcAccessMask = imageAccesses[image],
                                    .dstAccessMask = newAccess,
                            });
                        }

                        barrier = PipelineBarrierCommand{
                            .srcStage = vk::PipelineStageFlagBits::eAllCommands,
                            .dstStage = vk::PipelineStageFlagBits::eAllGraphics,
                            .imageMemoryBarriers = imageBarriers,
                            .bufferMemoryBarriers = {},
                        };
                    }

                    std::vector<vk::ClearValue> clearValues;
                    for(const auto& colorAttachment: colorAttachments) {
                        clearValues.emplace_back(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));
                    }
                    if(depthStencilAttachment.image != UNDEFINED_RESOURCE) {
                        auto dv = vk::ClearDepthStencilValue();
                        dv.setDepth(1.0f);
                        clearValues.emplace_back(dv);
                    }

                    std::vector<ResourceRef> descriptorSetLayouts;
                    for(const auto& descriptorSet: descriptorSetsUsed) {
                        descriptorSetLayouts.push_back(uniformDescriptorMap[descriptorSet]);
                    }

                    ResourceRef pipeline = RENDER_SYSTEM.getResourceManager().createRenderPipeline(RenderPipelineInfo{
                            .descriptorSetLayouts = descriptorSetLayouts,
                            .inputAttachments = inputAttachments,
                            .colorAttachments = colorAttachments,
                            .depthStencilAttachment = depthStencilAttachment,
                            .depthTest = pass.depthTest,
                            .depthWrite = pass.depthWrite,
                            .extent = vk::Extent2D{toActualWidth(pass.extent.width),
                                                   toActualHeight(pass.extent.height)},
                            .vertexShaderPath = pass.vertexShaderPath,
                            .fragmentShaderPath = pass.fragmentShaderPath,
                            .vertexInputData = pass.vertexInputData,
                    });
                    renderPasses.push_back(pipeline);
                    return Command(RenderPipelineCommand{
                        .pipeline = pipeline,
                        .clearValues = clearValues,
                        .execution = [callback = pass.callback, pipeline, descriptorSets = graph.m_descriptorSets](vk::CommandBuffer commandBuffer) {
                            callback(commandBuffer, pipeline, descriptorSets);
                        },
                    });
                } else if constexpr (std::is_same_v<T, FGBComputePassInfo>) {
                    std::unordered_set<ResourceRef> descriptorSetsUsed;
                    std::vector<std::pair<ResourceRef, vk::ImageLayout>> imageLayoutsToTransition;
                    std::vector<std::pair<ResourceRef, vk::AccessFlags>> imageAccessesToTransition;

                    for(const auto& input: pass.inputs) {
                        switch(input.type) {
                            case FGBResourceType::UniformSampledImage: {
                                const auto& uniform = std::get<FGBUniformSampledImageInfo>(m_uniforms[input.location]);
                                ResourceRef imageRef = uniform.image.has_value() ? imageLocations[*uniform.image] : (*uniform.texture)->getImage();
                                if(imageLayouts[imageRef] != vk::ImageLayout::eShaderReadOnlyOptimal) {
                                    imageLayoutsToTransition.emplace_back(imageRef, vk::ImageLayout::eShaderReadOnlyOptimal);
                                }
                                if(imageAccesses[imageRef] != vk::AccessFlagBits::eShaderRead) {
                                    imageAccessesToTransition.emplace_back(imageRef, vk::AccessFlagBits::eShaderRead);
                                }
                                descriptorSetsUsed.insert(uniformDescriptorMap[input.location]);
                                break;
                            }
                            case FGBResourceType::UniformStorageImage: {
                                const auto& imageRef = std::get<FGBUniformStorageImageInfo>(m_uniforms[input.location]).image;
                                if(imageLayouts[imageLocations[imageRef]] != vk::ImageLayout::eGeneral) {
                                    imageLayoutsToTransition.emplace_back(imageLocations[imageRef], vk::ImageLayout::eGeneral);
                                }
                                if(imageAccesses[imageLocations[imageRef]] != vk::AccessFlagBits::eShaderRead) {
                                    imageAccessesToTransition.emplace_back(imageLocations[input], vk::AccessFlagBits::eShaderRead);
                                }
                                descriptorSetsUsed.insert(uniformDescriptorMap[input.location]);
                                break;
                            }
                            case FGBResourceType::UniformBuffer: {
                                descriptorSetsUsed.insert(uniformDescriptorMap[input.location]);
                                break;
                            }
                            default:
                                throw std::runtime_error("Invalid input type");
                        }
                    }
                    for(const auto& output: pass.outputs) {
                        switch(output.type) {
                            case FGBResourceType::UniformStorageImage: {
                                const auto& imageRef = std::get<FGBUniformStorageImageInfo>(m_uniforms[output.location]).image;
                                if(imageLayouts[imageLocations[imageRef]] != vk::ImageLayout::eGeneral) {
                                    imageLayoutsToTransition.emplace_back(imageLocations[imageRef], vk::ImageLayout::eGeneral);
                                }
                                if(imageAccesses[imageLocations[imageRef]] != vk::AccessFlagBits::eShaderRead) {
                                    imageAccessesToTransition.emplace_back(imageLocations[output], vk::AccessFlagBits::eShaderWrite);
                                }
                                descriptorSetsUsed.insert(uniformDescriptorMap[output.location]);
                                break;
                            }
                            default:
                                throw std::runtime_error("Invalid output type");
                        }
                    }

                    if(!imageLayoutsToTransition.empty()) {
                        std::vector<ImageBarrierInfo> imageBarriers;
                        for(const auto& [image, layout]: imageLayoutsToTransition) {
                            vk::AccessFlags newAccess;
                            for (const auto& item: imageAccessesToTransition) {
                                if(item.first == image) {
                                    newAccess = item.second;
                                    break;
                                }
                            }
                            imageBarriers.push_back(ImageBarrierInfo{
                                    .image = image,
                                    .oldLayout = imageLayouts[image],
                                    .newLayout = layout,
                                    .srcAccessMask = imageAccesses[image],
                                    .dstAccessMask = newAccess,
                            });
                        }

                        barrier = PipelineBarrierCommand{
                                .srcStage = vk::PipelineStageFlagBits::eAllCommands,
                                .dstStage = vk::PipelineStageFlagBits::eAllGraphics,
                                .imageMemoryBarriers = imageBarriers,
                                .bufferMemoryBarriers = {},
                        };
                    }

                    std::vector<ResourceRef> descriptorSetLayouts;
                    for(const auto& descriptorSet: descriptorSetsUsed) {
                        descriptorSetLayouts.push_back(uniformDescriptorMap[descriptorSet]);
                    }

                    ResourceRef pipeline = RENDER_SYSTEM.getResourceManager().createComputePipeline(ComputePipelineInfo{
                        .descriptorSetLayouts = descriptorSetLayouts,
                        .computeShaderPath = pass.computeShaderPath,
                    });

                    return Command(ComputePipelineCommand{
                        .pipeline = pipeline,
                        .execution = [callback = pass.callback, pipeline, descriptorSets = graph.m_descriptorSets](vk::CommandBuffer commandBuffer) {
                            callback(commandBuffer, pipeline, descriptorSets);
                        },
                    });
                } else {
                    static_assert(always_false<T>, "non-exhaustive visitor!");
                }
            }, passInfo);
            if(barrier.has_value()) {
                commands.emplace_back(*barrier);
            }
            commands.emplace_back(command);
        }

        std::unordered_map<uint32_t, PipelineBarrierCommand> pipelineBarrierMap;
        for(const auto& [barrier, index]: pipelineBarriers) {
            pipelineBarrierMap[index] = barrier;
        }

        CommandsInfo commandsInfo;
        commandsInfo.backbufferImage = imageLocations[m_backbuffer];
        commandsInfo.backbufferImageAccessMask = imageAccesses[imageLocations[m_backbuffer]];
        commandsInfo.backbufferImageLayout = imageLayouts[imageLocations[m_backbuffer]];
        commandsInfo.commands = commands;

        graph.m_samplers = samplers;
        graph.m_pipelines = renderPasses;
        graph.m_commands = commandsInfo;

        return graph;
    }
}