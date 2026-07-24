#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace korework::content {

struct SmdVector3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct SmdCollisionTriangle {
    std::array<SmdVector3, 3> vertices {};
};

struct SmdCollisionSubCell {
    std::vector<std::uint32_t> triangleIndices;
};

struct SmdCollisionMainCell {
    std::vector<std::uint16_t> shapeIndices;
    std::array<SmdCollisionSubCell, 16> subCells;
};

struct SmdRegeneEvent {
    float positionX = 0.0F;
    float positionY = 0.0F;
    float positionZ = 0.0F;
    float areaZ = 0.0F;
    float areaX = 0.0F;
    std::int32_t point = 0;
};

struct SmdWarpInfo {
    std::int16_t warpId = 0;
    std::string name;
    std::string announcement;
    std::uint16_t unknown0 = 0;
    std::uint32_t cost = 0;
    std::int16_t zone = 0;
    std::uint16_t unknown1 = 0;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float radius = 0.0F;
    std::int16_t nation = 0;
    std::uint16_t unknown2 = 0;
};

class SmdMap final {
public:
    bool load(const std::filesystem::path& path);

    [[nodiscard]] bool loaded() const noexcept { return loaded_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] const std::filesystem::path& sourcePath() const noexcept { return sourcePath_; }
    [[nodiscard]] std::int32_t mapSize() const noexcept { return mapSize_; }
    [[nodiscard]] float unitDistance() const noexcept { return unitDistance_; }
    [[nodiscard]] float width() const noexcept { return collisionWidth_; }
    [[nodiscard]] float length() const noexcept { return collisionLength_; }
    [[nodiscard]] const std::vector<float>& heights() const noexcept { return heights_; }
    [[nodiscard]] const std::vector<SmdCollisionTriangle>& collisionTriangles() const noexcept { return collisionTriangles_; }
    [[nodiscard]] const std::vector<SmdCollisionMainCell>& collisionCells() const noexcept { return collisionCells_; }
    [[nodiscard]] const std::vector<std::array<std::byte, 24>>& objectEvents() const noexcept { return objectEvents_; }
    [[nodiscard]] const std::vector<std::int16_t>& eventGrid() const noexcept { return eventGrid_; }
    [[nodiscard]] const std::vector<SmdRegeneEvent>& regeneEvents() const noexcept { return regeneEvents_; }
    [[nodiscard]] const std::vector<SmdWarpInfo>& warps() const noexcept { return warps_; }
    [[nodiscard]] float heightAt(float x, float z) const noexcept;
    [[nodiscard]] bool contains(float x, float z) const noexcept;

private:
    void clear() noexcept;

    bool loaded_ = false;
    std::string error_;
    std::filesystem::path sourcePath_;
    std::int32_t mapSize_ = 0;
    float unitDistance_ = 0.0F;
    float collisionWidth_ = 0.0F;
    float collisionLength_ = 0.0F;
    std::vector<float> heights_;
    std::vector<SmdCollisionTriangle> collisionTriangles_;
    std::vector<SmdCollisionMainCell> collisionCells_;
    std::vector<std::array<std::byte, 24>> objectEvents_;
    std::vector<std::int16_t> eventGrid_;
    std::vector<SmdRegeneEvent> regeneEvents_;
    std::vector<SmdWarpInfo> warps_;
};

} // namespace korework::content
