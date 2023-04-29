#pragma once

#include <stb_image.h>

#include "Asset.h"
#include "File.h"

namespace vanguard {
    struct TextureData {
        uint32_t width;
        uint32_t height;
        uint32_t channels;
        std::vector<uint8_t> data;
    };

    static Asset loadTexture(const File& file) {
        int width, height, channels;
        stbi_info(file.path().c_str(), &width, &height, &channels);
        auto* data = stbi_load(file.path().c_str(), &width, &height, &channels, channels);
        TextureData textureData{};

        textureData.width = width;
        textureData.height = height;
        textureData.channels = channels;
        textureData.data = std::vector<uint8_t>(data, data + width * height * channels);

        stbi_image_free(data);

        return Asset(textureData);
    }
}