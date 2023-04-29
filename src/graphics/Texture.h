#pragma once

#include "ResourceManager.h"
#include "../Application.h"
#include "../assets/TextureData.h"

namespace vanguard {
    class Texture {
    public:
        [[nodiscard]] virtual ResourceRef getImage() const = 0;
    };

    class Texture2D : public Texture {
    public:
        Texture2D() = default;
        Texture2D(const Texture2D&) = delete;
        Texture2D& operator=(const Texture2D&) = delete;

        Texture2D(Texture2D&& other) noexcept {
            m_image = other.m_image;
            other.m_image = UNDEFINED_RESOURCE;
        }

        Texture2D& operator=(Texture2D&& other) noexcept {
            m_image = other.m_image;
            other.m_image = UNDEFINED_RESOURCE;
            return *this;
        }

        ~Texture2D() {
            if(m_image != UNDEFINED_RESOURCE) {
                RENDER_SYSTEM.getResourceManager().destroyImage(m_image);
            }
        }

        void create(const TextureData& data) {
            auto format = vk::Format::eUndefined;
            switch(data.channels) {
                case 1: format = vk::Format::eR8Unorm; break;
                case 2: format = vk::Format::eR8G8Unorm; break;
                case 3: format = vk::Format::eR8G8B8Unorm; break;
                case 4: format = vk::Format::eR8G8B8A8Unorm; break;
            }

            m_image = RENDER_SYSTEM.getResourceManager().createImage(ImageInfo{
                .format = format,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                .aspect = vk::ImageAspectFlagBits::eColor,
                .width = data.width,
                .height = data.height,
            });
            RENDER_SYSTEM.getStager().updateImage(m_image, vk::ImageLayout::eUndefined, data.data.size(), data.data.data());
        }

        [[nodiscard]] ResourceRef getImage() const override { return m_image; }
    private:
        ResourceRef m_image = UNDEFINED_RESOURCE;
    };

    struct CubeMapTextureInfo {
        TextureData right;
        TextureData left;
        TextureData top;
        TextureData bottom;
        TextureData front;
        TextureData back;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 0;
    };
    class CubeMapTexture : public Texture {
    public:
        CubeMapTexture() = default;
        CubeMapTexture(const CubeMapTexture&) = delete;
        CubeMapTexture& operator=(const CubeMapTexture&) = delete;

        CubeMapTexture(CubeMapTexture&& other) noexcept {
            m_image = other.m_image;
            other.m_image = UNDEFINED_RESOURCE;
        }

        CubeMapTexture& operator=(CubeMapTexture&& other) noexcept {
            m_image = other.m_image;
            other.m_image = UNDEFINED_RESOURCE;
            return *this;
        }

        ~CubeMapTexture() {
            if(m_image != UNDEFINED_RESOURCE) {
                RENDER_SYSTEM.getResourceManager().destroyImage(m_image);
            }
        }

        void create(const CubeMapTextureInfo& data) {
            auto format = vk::Format::eUndefined;
            switch(data.channels) {
                case 1: format = vk::Format::eR8Unorm; break;
                case 2: format = vk::Format::eR8G8Unorm; break;
                case 3: format = vk::Format::eR8G8B8Unorm; break;
                case 4: format = vk::Format::eR8G8B8A8Unorm; break;
            }

            m_image = RENDER_SYSTEM.getResourceManager().createImage(ImageInfo{
                .format = format,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                .aspect = vk::ImageAspectFlagBits::eColor,
                .width = data.width,
                .height = data.height,
                .arrayLayers = 6,
                .type = ImageType::Cube,
            });
            auto faces = {&data.right, &data.left, &data.top, &data.bottom, &data.front, &data.back};
            auto vFaces = std::vector(faces.begin(), faces.end());
            for(int i = 0; i < 6; i++) {
                RENDER_SYSTEM.getStager().updateImage(m_image, vk::ImageLayout::eUndefined, vFaces[i]->data.size(), vFaces[i]->data.data(), i);
            }
        }

        [[nodiscard]] ResourceRef getImage() const override { return m_image; }
    private:
        ResourceRef m_image = UNDEFINED_RESOURCE;
    };
}