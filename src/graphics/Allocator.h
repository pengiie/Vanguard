#pragma once

#include "Vulkan.h"
#include <vk_mem_alloc.h>

namespace vanguard {
    class Allocator {
    public:
        explicit Allocator(const VmaAllocatorCreateInfo& createInfo) {
            vmaCreateAllocator(&createInfo, &m_allocator);
        }

        ~Allocator() {
            if(m_allocator) {
                vmaDestroyAllocator(m_allocator);
            }
        }

        Allocator(const Allocator&) = delete;
        Allocator& operator=(const Allocator&) = delete;

        Allocator(Allocator&& other) noexcept {
            m_allocator = other.m_allocator;
            other.m_allocator = nullptr;
        }

        Allocator& operator=(Allocator&& other) noexcept {
            m_allocator = other.m_allocator;
            other.m_allocator = nullptr;
            return *this;
        }

        [[nodiscard]] const VmaAllocator& operator *() const { return m_allocator; }
    private:
        VmaAllocator m_allocator = nullptr;
    };

    struct Allocation {
        Allocation() = default;
        ~Allocation() {
            vmaFreeMemory(*Vulkan::getAllocator(), allocation);
        }

        Allocation(const Allocation&) = delete;
        Allocation& operator=(const Allocation&) = delete;

        Allocation(Allocation&& other) noexcept {
            allocation = other.allocation;
            allocationInfo = other.allocationInfo;
            other.allocation = nullptr;
        }

        Allocation& operator=(Allocation&& other) noexcept {
            allocation = other.allocation;
            allocationInfo = other.allocationInfo;
            other.allocation = nullptr;
            return *this;
        }

        VmaAllocation allocation = nullptr;
        VmaAllocationInfo allocationInfo = {};
    };
}