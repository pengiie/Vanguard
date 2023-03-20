#include "File.h"

#include <fstream>
#include "../Logger.h"

namespace vanguard {

    File::File(const std::string& filePath) {
        m_filePath = filePath;
        while(m_filePath.find('/') != std::string::npos)
            m_filePath = m_filePath.replace(m_filePath.find('/'), 1, "\\");

        auto offset = m_filePath.find_last_of('\\') + 1;
        m_fileName = m_filePath.substr(offset, m_filePath.find_last_of('.') - offset);
        m_fileExtension = m_filePath.substr(m_filePath.find_last_of('.') + 1);
    }

    std::vector<char> File::load(bool binary) const {
        auto mode = std::ios::ate | std::ios::in;
        if (binary)
            mode |= std::ios::binary;
        std::ifstream stream(m_filePath, mode);

        if (stream.is_open()) {
            std::streamsize size = stream.tellg();
            std::vector<char> buffer(size);
            stream.seekg(0);
            stream.read(buffer.data(), size);

            // Remove null terminator padding
            if(!binary) {
                while(buffer.back() == '\0')
                    buffer.pop_back();
            }
            return buffer;
        }
        ERROR("Failed to open file: {}", m_filePath);
        throw std::exception();
    }
}