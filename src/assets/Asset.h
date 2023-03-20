#pragma once

#include <string>
#include <memory>

namespace vanguard {
    struct Asset {
        template<typename T>
        explicit Asset(const T& data) : type(typeid(T).name()), data((void*) new T(std::move(data)), [](void* ptr) {
            delete static_cast<T*>(ptr);
        }) {}

        std::string type;
        std::unique_ptr<void, void (*)(void*)> data;

        template<typename T>
        [[nodiscard]] const T& get() {
            return *static_cast<T*>(data.get());
        }
    };
}