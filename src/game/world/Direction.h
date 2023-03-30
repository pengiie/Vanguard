#pragma once

#include "glm/ext.hpp"

namespace vanguard {
    class Direction {
    public:
        explicit Direction(const glm::ivec3& position) {
            m_up = position + glm::ivec3(0, 1, 0);
            m_down = position + glm::ivec3(0, -1, 0);
            m_left = position + glm::ivec3(-1, 0, 0);
            m_right = position + glm::ivec3(1, 0, 0);
            m_front = position + glm::ivec3(0, 0, 1);
            m_back = position + glm::ivec3(0, 0, -1);
        };

        [[nodiscard]] inline const glm::ivec3& up() const { return m_up; };
        [[nodiscard]] inline const glm::ivec3& down() const { return m_down; };
        [[nodiscard]] inline const glm::ivec3& left() const { return m_left; };
        [[nodiscard]] inline const glm::ivec3& right() const { return m_right; };
        [[nodiscard]] inline const glm::ivec3& front() const { return m_front; };
        [[nodiscard]] inline const glm::ivec3& back() const { return m_back; };

        struct Iterator {
            using iterator_category = std::input_iterator_tag;
            using difference_type = uint32_t;
            using value_type = const glm::ivec3;
            using pointer = const glm::ivec3*;
            using reference = const glm::ivec3&;

            Iterator(const Direction* direction, uint32_t index = 0) : m_direction(direction), m_index(index) {};

            reference operator*() const {
                switch (m_index) {
                    case 0: return m_direction->up();
                    case 1: return m_direction->down();
                    case 2: return m_direction->left();
                    case 3: return m_direction->right();
                    case 4: return m_direction->front();
                    case 5: return m_direction->back();
                    default: throw std::runtime_error("Invalid index");
                }
            }

            pointer operator->() const {
                return &operator*();
            }

            Iterator& operator++() {
                m_index++;
                return *this;
            }

            Iterator operator++(int) {
                Iterator tmp(*this);
                operator++();
                return tmp;
            }

            friend bool operator==(const Iterator& a, const Iterator& b) {
                return a.m_index == b.m_index;
            }
            friend bool operator!=(const Iterator& a, const Iterator& b) {
                return a.m_index != b.m_index;
            }
        private:
            const Direction* m_direction;
            uint32_t m_index = 0;
        };

        [[nodiscard]] inline Iterator begin() const { return { this }; }
        [[nodiscard]] inline Iterator end() const { return { this, 6 }; }

    private:
        glm::ivec3 m_up{};
        glm::ivec3 m_down{};
        glm::ivec3 m_left{};
        glm::ivec3 m_right{};
        glm::ivec3 m_front{};
        glm::ivec3 m_back{};
    };
}