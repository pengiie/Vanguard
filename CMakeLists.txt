cmake_minimum_required(VERSION 3.23)
cmake_policy(VERSION 3.23)
project(vanguard)

set(CMAKE_CXX_STANDARD 20)
set(BUILD_ARCH "x64")

set(VANGUARD_FILES
        src/assets/Asset.h
        src/assets/Assets.cpp
        src/assets/Assets.h
        src/Application.cpp
        src/Application.h
        src/Logger.cpp
        src/Logger.h
        src/assets/File.cpp
        src/assets/File.h
        src/Config.h
        src/Window.cpp
        src/Window.h
        src/Input.cpp
        src/Input.h
        src/graphics/Vulkan.cpp
        src/graphics/Vulkan.h
        src/util/Singleton.h
        src/util/Timer.h
        src/graphics/Allocator.h
        src/ImGuiWindow.cpp
        src/ImGuiWindow.h
        src/graphics/RenderGraph.h
        src/graphics/RenderGraph.cpp
        src/graphics/RenderSystem.cpp
        src/graphics/RenderSystem.h
        src/assets/SpirVShader.h
        src/Scene.h
        src/world/WorldScene.cpp
        src/world/WorldScene.h
        src/world/Camera.cpp
        src/world/Camera.h)

set(IMGUI_DIR ext/imgui)
set(IMGUI_FILES
        ${IMGUI_DIR}/imgui.h
        ${IMGUI_DIR}/imconfig.h
        ${IMGUI_DIR}/imgui_internal.h
        ${IMGUI_DIR}/imstb_rectpack.h
        ${IMGUI_DIR}/imstb_textedit.h
        ${IMGUI_DIR}/imstb_truetype.h
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp src/world/terrain/Terrain.cpp src/world/terrain/Terrain.h src/world/terrain/Chunk.cpp src/world/terrain/Chunk.h src/world/terrain/Voxel.h src/world/terrain/Voxel.cpp src/world/terrain/TerrainConstants.h src/world/terrain/TerrainGenerator.cpp src/world/terrain/TerrainGenerator.h)
set(ETC_FILES
        ext/perlin/PerlinNoise.hpp)

add_executable(vanguard src/main.cpp ${IMGUI_FILES} ${VANGUARD_FILES})

# After each build copy the assets folder to the build directory
add_custom_command(TARGET vanguard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:vanguard>/assets)

# Disable VMA's static Vulkan functions since we're using dynamic ones
set(VMA_STATIC_VULKAN_FUNCTIONS OFF)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON)

# Add subdirectories for external libraries
add_subdirectory(ext/spdlog)
add_subdirectory(ext/glfw)
add_subdirectory(ext/vma)
add_subdirectory(ext/glm)
add_subdirectory(ext/entt)

# Load Vulkan SDK from system environment variable
set(VULKAN_SDK $ENV{VULKAN_SDK})
if(NOT VULKAN_SDK)
    message(FATAL_ERROR "VULKAN_SDK environment variable not set")
endif()
set(VULKAN_SDK_INCLUDE ${VULKAN_SDK}/include)
set(VULKAN_LIB ${VULKAN_SDK}/lib/vulkan-1.lib)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(SHADERC_LIB ${VULKAN_SDK}/lib/shaderc_combinedd.lib)
else()
    set(SHADERC_LIB ${VULKAN_SDK}/lib/shaderc_combined.lib)
endif()

# Target Vulkan SDK for VMA since it's required for building
target_include_directories(VulkanMemoryAllocator PRIVATE ${VULKAN_SDK_INCLUDE})
target_link_libraries(VulkanMemoryAllocator ${VULKAN_LIB})

# Include imgui
target_include_directories(vanguard PRIVATE ext/imgui)

# Include etc...
target_include_directories(vanguard PRIVATE ext/perlin)

# Link external libraries to vanguard
target_include_directories(vanguard PRIVATE ${VULKAN_SDK_INCLUDE})
target_link_libraries(vanguard PRIVATE spdlog::spdlog glfw ${VULKAN_LIB} ${SHADERC_LIB} VulkanMemoryAllocator EnTT::EnTT glm::glm)