/**
 * The entry point of the application.
 *
 * Application standards:
 * - Every init() method will always be called after all the main application components have been initialize (eg. RenderSystem, Assets, Logger, etc.).
 */
#pragma once

#include "Logger.h"
#include "assets/Assets.h"
#include "Window.h"
#include "util/Singleton.h"
#include "ImGuiWindow.h"
#include "graphics/RenderSystem.h"
#include "Scene.h"
#include "Scheduler.h"

namespace vanguard {
    class Application : public Singleton {
    public:
        Application();

        void run();
        void stop();

        template<typename T>
        void setScene() {
            m_scene = std::make_unique<T>();
            m_scene->init();

            RenderGraphBuilder builder;
            m_scene->buildRenderGraph(builder);
            m_renderSystem.bake(std::move(builder));
        }

        [[nodiscard]] inline Assets& getAssets() { return m_assets; }
        [[nodiscard]] inline Window& getWindow() { return m_window; }
        [[nodiscard]] inline RenderSystem& getRenderSystem() { return m_renderSystem;}
        [[nodiscard]] inline Scheduler& getScheduler() { return m_scheduler; }

        static Application& Get();
    private:
        static Application* s_instance;

        std::unique_ptr<Scene> m_scene;
        bool m_running = false;

        Assets m_assets{};
        Window m_window{};
        ImGuiWindow m_imGuiWindow{};
        RenderSystem m_renderSystem{};
        Scheduler m_scheduler{};
    };
}
