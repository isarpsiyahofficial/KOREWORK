#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace korework::content {

struct N3TextureData {
    std::string name;
    std::filesystem::path sourcePath;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint32_t format = 0;
    bool hasMipMaps = false;
    std::vector<std::uint8_t> rgba;
};

class N3TextureLoader final {
public:
    [[nodiscard]] static N3TextureData load(const std::filesystem::path& path);
};

} // namespace korework::content
