#include "Vulkan.h"
#include "../Config.h"
#include "../Logger.h"
#include "Allocator.h"

#define VULKAN_LOGGER_NAME "Vulkan"
#define VULKAN_MIN_IMAGE_COUNT 2

namespace vanguard {
    static vk::raii::Context s_context;
    static std::optional<vk::raii::Instance> s_instance;
    [[maybe_unused]] static std::optional<vk::raii::DebugUtilsMessengerEXT> s_debugMessenger;
    static std::optional<vk::raii::PhysicalDevice> s_physicalDevice;
    static std::optional<vk::raii::Device> s_device;
    static uint32_t s_queueFamilyIndex = UINT32_MAX;
    static std::optional<vk::raii::Queue> s_queue;
    static std::optional<vk::raii::SurfaceKHR> s_surface;
    static std::optional<vk::raii::SwapchainKHR> s_swapchain;
    static vk::Extent2D s_swapchainExtent;
    static std::vector<SwapchainImage> s_swapchainImages;
    static std::optional<Allocator> s_allocator;
    static std::optional<vk::raii::DescriptorPool> s_descriptorPool;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerFunc( VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
                                                     VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
                                                     VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData,
                                                     void * /*pUserData*/ ) {
        if(!LoggerRegistry::hasLogger(VULKAN_LOGGER_NAME)) {
            LoggerRegistry::createLogger(VULKAN_LOGGER_NAME);
        }
        auto& logger = LoggerRegistry::getLogger(VULKAN_LOGGER_NAME);
        switch(messageSeverity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                logger.trace(pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                logger.info(pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                logger.warn(pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                logger.error(pCallbackData->pMessage);
                break;
            default: break;
        }
        return VK_FALSE;
    }

    void Vulkan::init(const std::vector<std::string>& extensions) {
        vk::ApplicationInfo appInfo{
            .pApplicationName = APPLICATION_NAME,
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = APPLICATION_NAME,
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_2,
        };

        const char* validationLayers[] = {
            "VK_LAYER_KHRONOS_validation"
        };

        std::vector<const char*> ccExtensions(extensions.size());
        ccExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        std::transform(extensions.begin(), extensions.end(), ccExtensions.begin(), [](const std::string& str) { return str.c_str(); });

        s_instance = vk::raii::Instance(s_context, {
                .pApplicationInfo = &appInfo,
                .enabledLayerCount = 1,
                .ppEnabledLayerNames = validationLayers,
                .enabledExtensionCount = static_cast<uint32_t>(ccExtensions.size()),
                .ppEnabledExtensionNames = ccExtensions.data(),
        });

        s_debugMessenger = s_instance->createDebugUtilsMessengerEXT(vk::DebugUtilsMessengerCreateInfoEXT{
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debugMessengerFunc,
        });

        auto physicalDevices = s_instance->enumeratePhysicalDevices();
        for (const auto& item: physicalDevices) {
            auto props = item.getProperties();
            INFO("Found physical device: {}", props.deviceName);
        }

        s_physicalDevice = std::move(physicalDevices[0]);
        INFO("Using physical device: {}", s_physicalDevice->getProperties().deviceName);

        auto queueFamilyProperties = s_physicalDevice->getQueueFamilyProperties();
        for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
            if (queueFamilyProperties[i].queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer)) {
                s_queueFamilyIndex = i;
                break;
            }
        }
        if(s_queueFamilyIndex == UINT32_MAX) {
            ERROR("No suitable queue family found");
        }

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = s_queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };

        s_device = s_physicalDevice->createDevice({
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
        });

        s_queue = s_device->getQueue(s_queueFamilyIndex, 0);

        VmaVulkanFunctions vmaVulkanFunctions{
            .vkGetInstanceProcAddr = s_context.getDispatcher()->vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = s_instance->getDispatcher()->vkGetDeviceProcAddr,
        };
        s_allocator = Allocator(VmaAllocatorCreateInfo{
            .physicalDevice = static_cast<VkPhysicalDevice>(**s_physicalDevice),
            .device = static_cast<VkDevice>(**s_device),
            .pVulkanFunctions = &vmaVulkanFunctions,
            .instance = static_cast<VkInstance>(**s_instance),
        });

        #define POOL_SIZE 11
        vk::DescriptorPoolSize poolSizes[POOL_SIZE] = {
            { vk::DescriptorType::eSampler, 1000 },
            { vk::DescriptorType::eCombinedImageSampler, 1000 },
            { vk::DescriptorType::eSampledImage, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eUniformTexelBuffer, 1000 },
            { vk::DescriptorType::eStorageTexelBuffer, 1000 },
            { vk::DescriptorType::eUniformBuffer, 1000 },
            { vk::DescriptorType::eStorageBuffer, 1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment, 1000 },
        };
        s_descriptorPool = s_device->createDescriptorPool({
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 1000 * POOL_SIZE,
            .poolSizeCount = POOL_SIZE,
            .pPoolSizes = poolSizes,
        });
    }

    static void createSwapchain(uint32_t width, uint32_t height) {
        auto surfaceFormat = vk::Format::eR8G8B8A8Unorm;
        auto presentMode = vk::PresentModeKHR::eFifo;
        uint32_t imageCount = VULKAN_MIN_IMAGE_COUNT + 1;

        s_swapchainExtent = vk::Extent2D{ width, height };
        vk::SwapchainCreateInfoKHR info{
            .surface = **s_surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat,
            .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
            .imageExtent = s_swapchainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = presentMode,
            .clipped = VK_TRUE
        };

        if(s_swapchain.has_value())
            info.oldSwapchain = **s_swapchain;

        s_swapchain = s_device->createSwapchainKHR(info);

        auto images = s_swapchain->getImages();
        s_swapchainImages.clear();
        s_swapchainImages.reserve(images.size());

//        vk::ImageViewCreateInfo viewInfo{
//            .viewType = vk::ImageViewType::e2D,
//            .format = surfaceFormat,
//            .subresourceRange = vk::ImageSubresourceRange{
//                .aspectMask = vk::ImageAspectFlagBits::eColor,
//                .levelCount = 1,
//                .layerCount = 1,
//            },
//        };

        for (auto image: images) {
            //viewInfo.image = image;
            s_swapchainImages.emplace_back(vanguard::SwapchainImage{
                .image = image,
                //.imageView = s_device->createImageView(viewInfo),
            });
        }
    }

    void Vulkan::initWindow(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
        s_surface = vk::raii::SurfaceKHR(*s_instance, surface);

        createSwapchain(width, height);
    }

    void Vulkan::recreateSwapchain(uint32_t width, uint32_t height) {
        createSwapchain(width, height);
    }

    void Vulkan::initImGui(ImGuiWindow& window) {
        auto& wd = window.getImGuiWindowData();

        wd.Surface = window.getSurface(static_cast<VkInstance>(**s_instance));

        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        wd.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(static_cast<VkPhysicalDevice>(**s_physicalDevice), window.getImGuiWindowData().Surface, requestSurfaceImageFormat, (size_t) IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

        const VkPresentModeKHR presentModes[] = { VK_PRESENT_MODE_FIFO_KHR };
        wd.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(static_cast<VkPhysicalDevice>(**s_physicalDevice), window.getImGuiWindowData().Surface, presentModes, (size_t) IM_ARRAYSIZE(presentModes));

        ImGui_ImplVulkanH_CreateOrResizeWindow(static_cast<VkInstance>(**s_instance), static_cast<VkPhysicalDevice>(**s_physicalDevice), static_cast<VkDevice>(**s_device), &window.getImGuiWindowData(), s_queueFamilyIndex, nullptr, static_cast<int>(window.getWidth()), static_cast<int>(window.getHeight()), VULKAN_MIN_IMAGE_COUNT);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplGlfw_InitForVulkan(window.getHandle(), true);

        static VkPipelineCache imGuiPipelineCache = VK_NULL_HANDLE;

        ImGui_ImplVulkan_InitInfo init_info{
            .Instance = static_cast<VkInstance>(**s_instance),
            .PhysicalDevice = static_cast<VkPhysicalDevice>(**s_physicalDevice),
            .Device = static_cast<VkDevice>(**s_device),
            .QueueFamily = s_queueFamilyIndex,
            .Queue = static_cast<VkQueue>(**s_queue),
            .PipelineCache = imGuiPipelineCache,
            .DescriptorPool = static_cast<VkDescriptorPool>(**s_descriptorPool),
            .MinImageCount = VULKAN_MIN_IMAGE_COUNT,
            .ImageCount = wd.ImageCount,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .Allocator = nullptr,
            .CheckVkResultFn = nullptr,
        };
        ImGui_ImplVulkan_Init(&init_info, wd.RenderPass);

        // Upload ImGui Fonts
        VkCommandPool commandPool = wd.Frames[wd.FrameIndex].CommandPool;
        VkCommandBuffer commandBuffer = wd.Frames[wd.FrameIndex].CommandBuffer;

        vkResetCommandPool(static_cast<VkDevice>(**s_device), commandPool, 0);
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

        VkSubmitInfo endInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
        };

        vkEndCommandBuffer(commandBuffer);
        vkQueueSubmit(static_cast<VkQueue>(**s_queue), 1, &endInfo, VK_NULL_HANDLE);

        s_device->waitIdle();
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void Vulkan::beginImGuiFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Vulkan::renderImGuiFrame(ImGuiWindow& window) {
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        const bool isMinimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);
        if (!isMinimized) {
            auto& wd = window.getImGuiWindowData();
            auto imageAcquireSemaphore = wd.FrameSemaphores[wd.SemaphoreIndex].ImageAcquiredSemaphore;
            auto renderCompleteSemaphore = wd.FrameSemaphores[wd.SemaphoreIndex].RenderCompleteSemaphore;

            auto result = vkAcquireNextImageKHR(static_cast<VkDevice>(**s_device), wd.Swapchain, UINT64_MAX, imageAcquireSemaphore, VK_NULL_HANDLE, &wd.FrameIndex);
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                // If the swapchain is out of date (e.g. the window was resized) we need to recreate it.
                ImGui_ImplVulkanH_CreateOrResizeWindow(static_cast<VkInstance>(**s_instance), static_cast<VkPhysicalDevice>(**s_physicalDevice), static_cast<VkDevice>(**s_device), &window.getImGuiWindowData(), s_queueFamilyIndex, nullptr, static_cast<int>(window.getWidth()), static_cast<int>(window.getHeight()), VULKAN_MIN_IMAGE_COUNT);
                wd.FrameIndex = 0;
                return;
            }

            ImGui_ImplVulkanH_Frame* fd = &wd.Frames[wd.FrameIndex];
            vkWaitForFences(static_cast<VkDevice>(**s_device), 1, &fd->Fence, VK_TRUE, UINT64_MAX);
            vkResetFences(static_cast<VkDevice>(**s_device), 1, &fd->Fence);

            vkResetCommandPool(static_cast<VkDevice>(**s_device), fd->CommandPool, 0);

            {
                VkCommandBufferBeginInfo info{};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(fd->CommandBuffer, &info);
            }

            {
                VkRenderPassBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                info.renderPass = wd.RenderPass;
                info.framebuffer = fd->Framebuffer;
                info.renderArea.extent.width = wd.Width;
                info.renderArea.extent.height = wd.Height;
                info.clearValueCount = 1;
                info.pClearValues = &wd.ClearValue;
                vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
            }

            ImGui_ImplVulkan_RenderDrawData(drawData, fd->CommandBuffer);

            vkCmdEndRenderPass(fd->CommandBuffer);
            {
                VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                info.waitSemaphoreCount = 1;
                info.pWaitSemaphores = &imageAcquireSemaphore;
                info.pWaitDstStageMask = &waitStages;
                info.commandBufferCount = 1;
                info.pCommandBuffers = &fd->CommandBuffer;
                info.signalSemaphoreCount = 1;
                info.pSignalSemaphores = &renderCompleteSemaphore;

                vkEndCommandBuffer(fd->CommandBuffer);
                vkQueueSubmit(static_cast<VkQueue>(**s_queue), 1, &info, fd->Fence);
            }

            {
                VkPresentInfoKHR info = {};
                info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                info.waitSemaphoreCount = 1;
                info.pWaitSemaphores = &renderCompleteSemaphore;
                info.swapchainCount = 1;
                info.pSwapchains = &wd.Swapchain;
                info.pImageIndices = &wd.FrameIndex;
                vkQueuePresentKHR(static_cast<VkQueue>(**s_queue), &info);
            }

            wd.SemaphoreIndex = (wd.SemaphoreIndex + 1) % wd.ImageCount;
        }
    }

    void Vulkan::destroyImGui(ImGuiWindow& window) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImGui_ImplVulkanH_DestroyWindow(static_cast<VkInstance>(**s_instance), static_cast<VkDevice>(**s_device), &window.getImGuiWindowData(), nullptr);

        ImGui::DestroyContext();
    }

    vk::raii::Instance& Vulkan::getInstance() {
        return *s_instance;
    }

    vk::raii::PhysicalDevice& Vulkan::getPhysicalDevice() {
        return *s_physicalDevice;
    }

    vk::raii::Device& Vulkan::getDevice() {
        return *s_device;
    }

    vk::raii::Queue& Vulkan::getQueue() {
        return *s_queue;
    }

    uint32_t Vulkan::getQueueFamilyIndex() {
        return s_queueFamilyIndex;
    }

    vk::raii::SwapchainKHR& Vulkan::getSwapchain() {
        return *s_swapchain;
    }

    vk::Extent2D Vulkan::getSwapchainExtent() {
        return s_swapchainExtent;
    }

    std::vector<SwapchainImage>& Vulkan::getSwapchainImages() {
        return s_swapchainImages;
    }

    Allocator& Vulkan::getAllocator() {
        return *s_allocator;
    }

    vk::raii::DescriptorPool& Vulkan::getDescriptorPool() {
        return *s_descriptorPool;
    }
}