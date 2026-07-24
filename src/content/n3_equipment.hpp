#pragma once

#include "content/ko_asset_resolver.hpp"
#include "content/n3_character.hpp"
#include "content/n3_skeleton.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace korework::content {

struct N3MeshVertex {
    N3Vector3 position;
    N3Vector3 normal;
    N3Vector2 uv;
};

struct N3ProgressiveMesh {
    std::string name;
    std::filesystem::path sourcePath;
    std::vector<N3MeshVertex> vertices;
    std::vector<std::uint16_t> indices;
    std::int32_t minimumVertices = 0;
    std::int32_t minimumIndices = 0;
};

struct N3EquipmentPlug {
    std::string name;
    std::filesystem::path sourcePath;
    std::int32_t type = 0;
    std::int32_t jointIndex = -1;
    N3Vector3 position;
    N3Matrix4 rotation;
    N3Vector3 scale {1.0F, 1.0F, 1.0F};
    std::array<float, 4> diffuse {1.0F, 1.0F, 1.0F, 1.0F};
    std::filesystem::path meshPath;
    std::filesystem::path texturePath;
    std::int32_t traceStep = 0;
    N3ProgressiveMesh mesh;
};

class N3EquipmentLoader final {
public:
    explicit N3EquipmentLoader(const KoAssetResolver& resolver) : resolver_(resolver) {}
    [[nodiscard]] N3EquipmentPlug load(const std::filesystem::path& plugPath) const;
    [[nodiscard]] N3ProgressiveMesh loadProgressiveMesh(const std::filesystem::path& meshPath) const;

private:
    const KoAssetResolver& resolver_;
};

} // namespace korework::content
