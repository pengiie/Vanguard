#pragma once

#include <utility>
#include <vector>
#include <unordered_map>
#include <string>
#include <future>
#include <memory>

#include "File.h"
#include "../Logger.h"

#include "SpirVShader.h"
#include "Asset.h"

namespace vanguard {
    class Assets {
    public:
        Assets();

        void addLoader(const std::string& extension, const std::function<Asset(const File&)>& loader);

        void load(const std::string& path);
        void finishLoading();

        template<typename T>
        [[nodiscard]] const T& get(const std::string& path) {
            File file(path);

            if(m_assets.find(file.path()) == m_assets.end())
                ERROR("Asset not loaded: {}", file.path());
            return m_assets.at(file.path()).get<T>();
        }
    private:
        std::unordered_map<std::string, std::function<Asset(const File&)>> m_loaders;

        std::unordered_map<std::string, std::future<void>> m_tasks;
        std::unordered_map<std::string, Asset> m_assets;

        std::mutex m_mutex;
    };
}