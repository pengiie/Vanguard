#include "Application.h"
#include "Logger.h"
#include "Input.h"
#include "graphics/Vulkan.h"
#include "util/Timer.h"
#include "world/WorldScene.h"

namespace vanguard {
    Application* Application::s_instance = nullptr;

    Application::Application() {
        FTIMER();

        s_instance = this;

        LoggerRegistry::createLogger(APPLICATION_NAME);

        Window::initGLFW();
        m_window.init();
        m_imGuiWindow.init();

        Vulkan::init(Window::getRequiredExtensions());
        Vulkan::initWindow(m_window.getSurface(static_cast<VkInstance>(*Vulkan::getInstance())), m_window.getWidth(), m_window.getHeight());
        Vulkan::initImGui(m_imGuiWindow);

        m_renderSystem.init();

        setScene<WorldScene>();
    }

    void Application::run() {
        m_running = true;
        //m_imGuiWindow.show();
        m_window.show();

        bool open = true;

        while(m_running && !m_window.isCloseRequested()) {
            TIMER("Application::loop");
            Window::pollEvents();

            m_scene->update(Window::getDeltaTime());
            m_scheduler.update();

            // Render ImGui
//            Vulkan::beginImGuiFrame();
//            ImGui::ShowDemoWindow(&open);
//            Vulkan::renderImGuiFrame(m_imGuiWindow);

            m_renderSystem.render(m_window);
        }

        // Wait for the device to finish all operations
        Vulkan::getDevice().waitIdle();
        Vulkan::destroyImGui(m_imGuiWindow);
        Window::terminateGLFW();
    }

    void Application::stop() {
        m_running = false;
    }

    Application& Application::Get() {
        return *s_instance;
    }
}