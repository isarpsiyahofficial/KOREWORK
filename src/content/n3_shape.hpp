#pragma once

#include "content/ko_asset_resolver.hpp"
#include "content/n3_character.hpp"
#include "content/n3_equipment.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace korework::content {

struct N3ShapePart {
    N3Vector3 pivot;
    std::array<float, 4> diffuse {1.0F, 1.0F, 1.0F, 1.0F};
    float textureFps = 10.0F;
    std::uint32_t renderFlags = 0U;
    std::uint32_t sourceBlend = 0U;
    std::uint32_t destinationBlend = 0U;
    std::filesystem::path meshPath;
    std::vector<std::filesystem::path> texturePaths;
    N3ProgressiveMesh mesh;
};

struct N3Shape {
    std::string name;
    std::filesystem::path sourcePath;
    N3Vector3 position;
    N3Quaternion rotation;
    N3Vector3 scale {1.0F, 1.0F, 1.0F};
    std::filesystem::path collisionMeshPath;
    std::filesystem::path climbMeshPath;
    std::vector<N3ShapePart> parts;
    std::int32_t belong = 0;
    std::int32_t eventId = 0;
    std::int32_t eventType = 0;
    std::int32_t npcId = 0;
    std::int32_t npcStatus = 0;
};

class N3ShapeLoader final {
public:
    explicit N3ShapeLoader(const KoAssetResolver& resolver) : resolver_(resolver), equipmentLoader_(resolver) {}
    [[nodiscard]] N3Shape load(const std::filesystem::path& shapePath) const;

private:
    const KoAssetResolver& resolver_;
    N3EquipmentLoader equipmentLoader_;
};

} // namespace korework::content
