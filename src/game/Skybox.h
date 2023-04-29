#pragma once

#include "../graphics/Texture.h"
#include "glm/vec3.hpp"
#include "../graphics/Buffer.h"
#include "../graphics/FrameGraph.h"

namespace vanguard {
    struct SkyboxMeshVertex {
        glm::vec3 position;
        glm::vec3 uv;
    };

    class Skybox {
    public:
        Skybox() = default;
        Skybox(const Skybox&) = delete;
        Skybox& operator=(const Skybox&) = delete;

        void init();
        void addSkyboxPass(FrameGraphBuilder& builder, const FGBResourceRef& image, const FGBResourceRef& cameraUniform);
        static VertexInputData getVertexInputData();

        [[nodiscard]] const CubeMapTexture& getCubeMapTexture() const { return m_cubeMapTexture; }
        [[nodiscard]] const VertexBuffer& getVertexBuffer() const { return m_vb; }
    private:
        CubeMapTexture m_cubeMapTexture{};
        VertexBuffer m_vb{};
    };
}