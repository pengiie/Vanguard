#pragma once

#include <vector>
#include <functional>
#include "Vulkan.h"
#include "ResourceManager.h"
#include "glm/vec3.hpp"

namespace vanguard {
    typedef uint32_t ResourceRef;
    #define UNDEFINED_RESOURCE UINT32_MAX

    typedef std::function<void(const vk::CommandBuffer&)> ExecutionFunction;

    // Resources
    enum class ResourceType {
        Image,
        Stencil,
        UniformBuffer,
        UniformImage,
    };

    struct Resource {
        std::string name;
        Resource() = default;
        virtual ~Resource() = default;
        [[nodiscard]] virtual ResourceType getType() const = 0;
    };

    // Image resource and info
    struct ImageInfo {
        vk::Format format = vk::Format::eUndefined;
        glm::vec3 clearColor = glm::vec3(0.0f);
        float clearDepth = 1.0f;
    };
    struct ImageResource : Resource {
        ImageInfo info{};
        ImageResource() = default;
        [[nodiscard]] ResourceType getType() const override { return ResourceType::Image; }
    };

    // Stencil resource and info
    typedef ImageInfo StencilInfo;
    struct StencilResource : Resource {
        StencilInfo info{};
        [[nodiscard]] ResourceType getType() const override { return ResourceType::Stencil; }
    };

    // Uniform type resources
    enum class UniformFrequency: uint8_t {
        PerFrame = 0,
        PerPass = 1
    };

    struct UniformBufferInfo {
        UniformBuffer buffer;
        UniformFrequency frequency = UniformFrequency::PerFrame;
        uint32_t binding = 0;
    };
    struct UniformBufferResource : Resource {
        UniformBufferInfo info{};
        [[nodiscard]] ResourceType getType() const override { return ResourceType::UniformBuffer; }
    };

    enum class UniformImageType {
        CombinedSampler,
        Storage
    };
    struct UniformImageInfo {
        ResourceRef image = UNDEFINED_RESOURCE;
        UniformImageType type = UniformImageType::CombinedSampler;
        UniformFrequency frequency = UniformFrequency::PerFrame;
        uint32_t binding = 0;
    };
    struct UniformImageResource : Resource {
        UniformImageInfo info{};
        [[nodiscard]] ResourceType getType() const override { return ResourceType::UniformImage; }
    };

    // Vertex input data
    struct VertexAttribute {
        uint32_t location;
        uint32_t offset;
        vk::Format format;
    };
    class VertexInputData {
    public:
        VertexInputData() = default;
        explicit VertexInputData(uint32_t size) : m_stride(size) {}

        void setAttribute(uint32_t location, uint32_t offset, vk::Format format) {
            m_attributes.push_back({
                                           location,
                                           offset,
                                           format
                                   });
        }

        template <typename T>
        [[nodiscard]] static VertexInputData createVertexInputData() {
            return VertexInputData(sizeof(T));
        }

        [[nodiscard]] const std::vector<VertexAttribute>& getAttributes() const { return m_attributes; }
        [[nodiscard]] uint32_t getStride() const { return m_stride; }
    private:
        std::vector<VertexAttribute> m_attributes;
        uint32_t m_stride = 0;

        friend class RenderGraph;
    };

    // Passes
    enum class PassType {
        Unknown,
        Transfer,
        Compute,
        Render
    };

    struct PassInfo {
        std::string name;
        std::vector<ResourceRef> inputs;
        std::vector<ResourceRef> outputs;
        ExecutionFunction execution;

        virtual PassType getType() { return PassType::Unknown; }
    };

    struct TransferPassInfo : public PassInfo {
        PassType getType() override { return PassType::Transfer; }
    };

    struct ComputePassInfo : public PassInfo {
        std::vector<uint32_t> computeShader;

        PassType getType() override { return PassType::Compute; }
    };

    // Render pass info
    struct RenderPassInfo : public PassInfo {
        VertexInputData vertexInput;
        std::vector<uint32_t> vertexShader;
        std::vector<uint32_t> fragmentShader;

        PassType getType() override { return PassType::Render; }
    };

    // A strict definition of a frame graph.
    struct FrameGraphBuilder {
        std::vector<std::unique_ptr<Resource>> resources;
        std::vector<std::unique_ptr<PassInfo>> passes;

        ResourceRef backBuffer = UNDEFINED_RESOURCE;

        // "Helper methods"
        ResourceRef createImage(const std::string& name, const ImageInfo& info);
        ResourceRef createStencil(const std::string& name, const StencilInfo& info);
        ResourceRef createUniformBuffer(const std::string& name, const UniformBufferInfo& info);
        ResourceRef createUniformImage(const std::string& name, const UniformImageInfo& info);

        void addExecutionPass(const PassInfo& info);
        void addComputePass(const ComputePassInfo& info);
        void addRenderPass(const RenderPassInfo& info);

        void setBackBuffer(ResourceRef resource);

        [[nodiscard]] const Resource* getResource(ResourceRef ref) const;
        [[nodiscard]] std::vector<ImageResource> getImageResources();
        [[nodiscard]] std::vector<StencilResource> getStencilResources();
        [[nodiscard]] std::vector<UniformBufferResource> getUniformBufferResources();
        [[nodiscard]] std::vector<UniformImageResource> getUniformImageResources();

        [[nodiscard]] std::vector<PassInfo*> getPasses() const;
        [[nodiscard]] std::vector<TransferPassInfo> getTransferPasses() const;
        [[nodiscard]] std::vector<ComputePassInfo> getComputePasses() const;
        [[nodiscard]] std::vector<RenderPassInfo> getRenderPasses() const;
    };
}