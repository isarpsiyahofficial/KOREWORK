#pragma once

#include "content/ko_asset_resolver.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace korework::content {

struct N3Vector2 { float u = 0.0F; float v = 0.0F; };
struct N3Vector3 { float x = 0.0F; float y = 0.0F; float z = 0.0F; };
struct N3Quaternion { float x = 0.0F; float y = 0.0F; float z = 0.0F; float w = 1.0F; };

struct N3SkinLod {
    std::string name;
    std::vector<N3Vector3> positions;
    std::vector<N3Vector3> normals;
    std::vector<std::uint16_t> faceIndices;
    std::vector<N3Vector2> uvs;
    std::vector<std::uint16_t> uvIndices;
};

struct N3CharacterPart {
    std::string name;
    std::filesystem::path sourcePath;
    std::filesystem::path texturePath;
    std::filesystem::path skinsPath;
    std::array<float, 4> diffuse {1.0F, 1.0F, 1.0F, 1.0F};
    std::array<N3SkinLod, 4> lods;
};

struct N3Character {
    std::string name;
    std::filesystem::path sourcePath;
    N3Vector3 position;
    N3Quaternion rotation;
    N3Vector3 scale {1.0F, 1.0F, 1.0F};
    std::filesystem::path jointPath;
    std::filesystem::path animationPath;
    std::vector<N3CharacterPart> parts;
    std::vector<std::filesystem::path> plugs;
};

class N3CharacterLoader final {
public:
    explicit N3CharacterLoader(const KoAssetResolver& resolver) : resolver_(resolver) {}
    [[nodiscard]] N3Character load(const std::filesystem::path& characterPath) const;

private:
    [[nodiscard]] N3CharacterPart loadPart(const std::filesystem::path& partPath) const;
    void loadSkins(const std::filesystem::path& skinsPath, std::array<N3SkinLod, 4>& lods) const;
    const KoAssetResolver& resolver_;
};

} // namespace korework::content
