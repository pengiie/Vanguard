#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Asset.h"
#include "File.h"
#include "../Logger.h"
#include "glm/vec3.hpp"
#include "../graphics/VertexInput.h"

namespace vanguard {
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    struct Mesh {
        std::vector<Vertex> vertices;
    };

    static VertexInputData getMeshVertexData() {
        VertexInputData data = VertexInputData::createVertexInputData<Vertex>();
        data.setAttribute(0, offsetof(Vertex, position), vk::Format::eR32G32B32Sfloat);
        data.setAttribute(1, offsetof(Vertex, normal), vk::Format::eR32G32B32Sfloat);
        data.setAttribute(2, offsetof(Vertex, uv), vk::Format::eR32G32Sfloat);
        return data;
    }

    static Asset loadObj(const File& file) {
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(file.path(), aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_GenUVCoords | aiProcess_FlipUVs | aiProcess_MakeLeftHanded);

        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
            ERROR("Failed to load model: {}", importer.GetErrorString());

        if(scene->mNumMeshes == 0)
            ERROR("No meshes in model: {}", file.path());

        auto* aiMesh = scene->mMeshes[0];

        Mesh mesh{};
        mesh.vertices.reserve(aiMesh->mNumVertices);

        for(unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
            mesh.vertices.push_back(Vertex{
                .position = {
                    aiMesh->mVertices[i].x,
                    aiMesh->mVertices[i].y,
                    aiMesh->mVertices[i].z
                },
                .normal = {
                    -aiMesh->mNormals[i].x,
                    -aiMesh->mNormals[i].y,
                    -aiMesh->mNormals[i].z
                },
                .uv = {
                    aiMesh->mTextureCoords[0][i].x,
                    aiMesh->mTextureCoords[0][i].y
                }
            });
        }

        return Asset(mesh);
    }
}