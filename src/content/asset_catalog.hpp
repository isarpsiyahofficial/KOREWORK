#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace korework::content {

struct KoAssetCatalog {
    std::filesystem::path root;
    std::uint64_t totalFiles = 0;
    std::uint64_t totalBytes = 0;
    std::map<std::string, std::uint64_t> extensionCounts;
    std::vector<std::filesystem::path> serverMaps;
    std::vector<std::filesystem::path> terrainFiles;
    std::vector<std::filesystem::path> objectPlacementFiles;
    std::vector<std::filesystem::path> characterFiles;
    std::vector<std::filesystem::path> characterPartFiles;
    std::vector<std::filesystem::path> equipmentPlugFiles;
    std::vector<std::filesystem::path> jointFiles;
    std::vector<std::filesystem::path> animationFiles;
    std::vector<std::filesystem::path> shapeFiles;
    std::vector<std::filesystem::path> effectFiles;
    std::vector<std::filesystem::path> textureFiles;
    std::vector<std::filesystem::path> uiFiles;

    [[nodiscard]] static KoAssetCatalog scan(const std::filesystem::path& rootPath);
    [[nodiscard]] bool hasRuntimeMinimum() const noexcept;
    [[nodiscard]] std::string summary() const;
};

} // namespace korework::content
