#include "Skybox.h"

namespace vanguard {
    static const std::vector<SkyboxMeshVertex> cubeVertices = {
            // +Z
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, 1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(-1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, 1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, 1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, 1.0f, 1.0f)
            },

            // -Z
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, -1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, -1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, -1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, -1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, -1.0f)
            },

            // +X
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, -1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, 1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, 1.0f, 1.0f)
            },

            // -X
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, -1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, -1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(-1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(-1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, 1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, 1.0f)
            },

            // +Y
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, 1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, 1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, 1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, 1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, 1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, 1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, 1.0f, -1.0f),
                    .uv = glm::vec3(1.0f, 1.0f, -1.0f)
            },

            // -Y
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, -1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, -1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(-1.0f, -1.0f, 1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(-1.0f, -1.0f, -1.0f),
                    .uv = glm::vec3(-1.0f, -1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, -1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, -1.0f)
            },
            SkyboxMeshVertex{
                    .position = glm::vec3(1.0f, -1.0f, 1.0f),
                    .uv = glm::vec3(1.0f, -1.0f, 1.0f)
            },
    };

    void Skybox::init() {
        auto& top = ASSETS.get<TextureData>("skybox/top.jpg");
        auto& bottom = ASSETS.get<TextureData>("skybox/bottom.jpg");
        auto& left = ASSETS.get<TextureData>("skybox/left.jpg");
        auto& right = ASSETS.get<TextureData>("skybox/right.jpg");
        auto& front = ASSETS.get<TextureData>("skybox/front.jpg");
        auto& back = ASSETS.get<TextureData>("skybox/back.jpg");

        if(top.data.size() != bottom.data.size() || top.data.size() != left.data.size() || top.data.size() != right.data.size() || top.data.size() != front.data.size() || top.data.size() != back.data.size()) {
            throw std::runtime_error("Skybox textures are not the same size!");
        }

        m_cubeMapTexture.create(CubeMapTextureInfo{
                .right = right,
                .left = left,
                .top = top,
                .bottom = bottom,
                .front = front,
                .back = back,
                .width = top.width,
                .height = top.height,
                .channels = top.channels,
        });
        m_vb.create(cubeVertices);
    }

    void Skybox::addSkyboxPass(vanguard::FrameGraphBuilder& builder, const vanguard::FGBResourceRef& image, const FGBResourceRef& cameraUniform) {
        FGBResourceRef skyboxTexture = builder.addUniformSampledImage(0, 2, &m_cubeMapTexture, SamplerInfo{
            .addressModeU = vk::SamplerAddressMode::eClampToEdge,
            .addressModeV = vk::SamplerAddressMode::eClampToEdge,
            .addressModeW = vk::SamplerAddressMode::eClampToEdge,
        });

        builder.addRenderPass(FGBRenderPassInfo{
            .vertexShaderPath = "shaders/skybox.vert.glsl",
            .fragmentShaderPath = "shaders/skybox.frag.glsl",
            .inputs = { cameraUniform, skyboxTexture },
            .outputs = { image },
            .callback = [&](vk::CommandBuffer cmd, ResourceRef pipeline, std::unordered_map<uint32_t, FrameGraph::DescriptorSet> sets) {
                sets.at(0).bindGraphics(pipeline, cmd);
                m_vb.bind(cmd);
                cmd.draw(cubeVertices.size(), 1, 0, 0);
            },
            .vertexInputData = getVertexInputData(),
        });
    }

    VertexInputData Skybox::getVertexInputData() {
        VertexInputData data = VertexInputData::createVertexInputData<SkyboxMeshVertex>();
        data.setAttribute(0, offsetof(SkyboxMeshVertex, position), vk::Format::eR32G32B32Sfloat);
        data.setAttribute(1, offsetof(SkyboxMeshVertex, uv), vk::Format::eR32G32B32Sfloat);
        return data;
    }
}