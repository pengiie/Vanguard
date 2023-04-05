#include "FrameGraphBuilder.h"

namespace vanguard {
    ResourceRef FrameGraphBuilder::createImage(const std::string& name, const ImageInfo& info) {
        ResourceRef ref = resources.size();

        auto resource = std::make_unique<ImageResource>();
        resource->name = name;
        resource->info = info;

        resources.push_back(std::move(resource));
        return ref;
    }

    ResourceRef FrameGraphBuilder::createStencil(const std::string& name, const StencilInfo& info) {
        ResourceRef ref = resources.size();

        auto resource = std::make_unique<StencilResource>();
        resource->name = name;
        resource->info = info;

        resources.push_back(std::move(resource));
        return ref;
    }

    ResourceRef FrameGraphBuilder::createUniformBuffer(const std::string& name, const UniformBufferInfo& info) {
        ResourceRef ref = resources.size();

        auto resource = std::make_unique<UniformBufferResource>();
        resource->name = name;
        resource->info = info;

        resources.push_back(std::move(resource));
        return ref;
    }

    ResourceRef FrameGraphBuilder::createUniformImage(const std::string& name, const UniformImageInfo& info) {
        ResourceRef ref = resources.size();

        auto resource = std::make_unique<UniformImageResource>();
        resource->name = name;
        resource->info = info;

        resources.push_back(std::move(resource));
        return ref;
    }

    void FrameGraphBuilder::addExecutionPass(const PassInfo& info) {
        passes.push_back(std::make_unique<PassInfo>(info));
    }

    void FrameGraphBuilder::addComputePass(const ComputePassInfo& info) {
        passes.push_back(std::make_unique<ComputePassInfo>(info));
    }

    void FrameGraphBuilder::addRenderPass(const RenderPassInfo& info) {
        passes.push_back(std::make_unique<RenderPassInfo>(info));
    }

    void FrameGraphBuilder::setBackBuffer(ResourceRef resource) {
        backBuffer = resource;
    }

    const Resource* FrameGraphBuilder::getResource(ResourceRef ref) const {
        return resources[ref].get();
    }

    std::vector<ImageResource> FrameGraphBuilder::getImageResources() {
        std::vector<ImageResource> imageResources;
        for (auto& resource : resources) {
            if (resource->getType() == ResourceType::Image) {
                imageResources.push_back(*reinterpret_cast<const std::unique_ptr<ImageResource>&>(resource));
            }
        }
        return imageResources;
    }

    std::vector<StencilResource> FrameGraphBuilder::getStencilResources() {
        std::vector<StencilResource> stencilResources;
        for (auto& resource : resources) {
            if (resource->getType() == ResourceType::Stencil) {
                stencilResources.push_back(*reinterpret_cast<const std::unique_ptr<StencilResource>&>(resource));
            }
        }
        return stencilResources;
    }

    std::vector<UniformBufferResource> FrameGraphBuilder::getUniformBufferResources() {
        std::vector<UniformBufferResource> uniformBufferResources;
        for (auto& resource : resources) {
            if (resource->getType() == ResourceType::UniformBuffer) {
                uniformBufferResources.push_back(*reinterpret_cast<const std::unique_ptr<UniformBufferResource>&>(resource));
            }
        }
        return uniformBufferResources;
    }

    std::vector<UniformImageResource> FrameGraphBuilder::getUniformImageResources() {
        std::vector<UniformImageResource> uniformImageResources;
        for (auto& resource : resources) {
            if (resource->getType() == ResourceType::UniformImage) {
                uniformImageResources.push_back(*reinterpret_cast<const std::unique_ptr<UniformImageResource>&>(resource));
            }
        }
        return uniformImageResources;
    }

    std::vector<PassInfo*> FrameGraphBuilder::getPasses() const {
        std::vector<PassInfo*> passInfos;
        passInfos.reserve(passes.size());
        for (auto& pass : passes) {
            passInfos.push_back(pass.get());
        }
        return passInfos;
    }

    std::vector<TransferPassInfo> FrameGraphBuilder::getTransferPasses() const {
        std::vector<TransferPassInfo> transferPasses;
        for (const auto& pass : passes) {
            if (pass->getType() == PassType::Transfer) {
                transferPasses.push_back(*reinterpret_cast<const std::unique_ptr<TransferPassInfo>&>(pass));
            }
        }
        return transferPasses;
    }

    std::vector<ComputePassInfo> FrameGraphBuilder::getComputePasses() const {
        std::vector<ComputePassInfo> computePasses;
        for (const auto& pass : passes) {
            if (pass->getType() == PassType::Compute) {
                computePasses.push_back(*reinterpret_cast<const std::unique_ptr<ComputePassInfo>&>(pass));
            }
        }
        return computePasses;
    }

    std::vector<RenderPassInfo> FrameGraphBuilder::getRenderPasses() const {
        std::vector<RenderPassInfo> renderPasses;
        for (const auto& pass : passes) {
            if (pass->getType() == PassType::Render) {
                renderPasses.push_back(*reinterpret_cast<const std::unique_ptr<RenderPassInfo>&>(pass));
            }
        }
        return renderPasses;
    }

}