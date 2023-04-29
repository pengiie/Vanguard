#pragma once

#include "Vulkan.h"

namespace vanguard {
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
    };
}