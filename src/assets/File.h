#pragma once

#include <string>
#include <vector>

namespace vanguard {
    class File {
    public:
        explicit File(const std::string& filePath);

        [[nodiscard]] std::vector<char> load(bool binary) const;

        [[nodiscard]] inline const std::string& path() const {
            return m_filePath;
        }

        [[nodiscard]] inline const std::string& name() const {
            return m_fileName;
        }

        [[nodiscard]] inline const std::string& extension() const {
            return m_fileExtension;
        }
    private:
        std::string m_filePath;
        std::string m_fileName;
        std::string m_fileExtension;
    };
}
