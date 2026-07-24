#include "content/smd_map.hpp"

#include "content/binary_reader.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace korework::content {
namespace {

constexpr float kMainCellSize = 16.0F;
constexpr std::int32_t kMaximumMapSize = 4096;
constexpr std::int32_t kMaximumCollisionFaces = 8'000'000;
constexpr std::int32_t kMaximumObjectEvents = 1'000'000;
constexpr std::int32_t kMaximumRegeneEvents = 1'000'000;
constexpr std::int32_t kMaximumWarps = 100'000;

void requireCount(std::int32_t count, std::int32_t maximum, const char* label) {
    if (count < 0 || count > maximum) {
        throw std::runtime_error(std::string("Invalid ") + label + " count: " + std::to_string(count));
    }
}

std::size_t checkedSquare(std::int32_t value) {
    if (value <= 0 || value > kMaximumMapSize) {
        throw std::runtime_error("Invalid SMD map size: " + std::to_string(value));
    }
    const auto converted = static_cast<std::size_t>(value);
    return converted * converted;
}

} // namespace

bool SmdMap::load(const std::filesystem::path& path) {
    clear();
    sourcePath_ = path;

    try {
        BinaryReader reader(path);

        mapSize_ = reader.read<std::int32_t>();
        unitDistance_ = reader.read<float>();
        const std::size_t heightCount = checkedSquare(mapSize_);
        if (!std::isfinite(unitDistance_) || unitDistance_ <= 0.0F || unitDistance_ > 128.0F) {
            throw std::runtime_error("Invalid SMD unit distance");
        }
        heights_ = reader.readVector<float>(heightCount);

        collisionWidth_ = reader.read<float>();
        collisionLength_ = reader.read<float>();
        if (!std::isfinite(collisionWidth_) || !std::isfinite(collisionLength_)
            || collisionWidth_ <= 0.0F || collisionLength_ <= 0.0F
            || collisionWidth_ > 65'536.0F || collisionLength_ > 65'536.0F) {
            throw std::runtime_error("Invalid SMD collision dimensions");
        }

        const std::int32_t faceCount = reader.read<std::int32_t>();
        requireCount(faceCount, kMaximumCollisionFaces, "collision face");
        collisionTriangles_.resize(static_cast<std::size_t>(faceCount));
        for (SmdCollisionTriangle& triangle : collisionTriangles_) {
            for (SmdVector3& vertex : triangle.vertices) {
                vertex.x = reader.read<float>();
                vertex.y = reader.read<float>();
                vertex.z = reader.read<float>();
            }
        }

        const auto cellsX = static_cast<std::size_t>(std::ceil(collisionWidth_ / kMainCellSize));
        const auto cellsZ = static_cast<std::size_t>(std::ceil(collisionLength_ / kMainCellSize));
        if (cellsX == 0 || cellsZ == 0 || cellsX > 4096 || cellsZ > 4096
            || cellsX > std::numeric_limits<std::size_t>::max() / cellsZ) {
            throw std::runtime_error("Invalid SMD collision cell dimensions");
        }
        collisionCells_.reserve(cellsX * cellsZ);

        for (std::size_t z = 0; z < cellsZ; ++z) {
            for (std::size_t x = 0; x < cellsX; ++x) {
                const std::uint32_t exists = reader.read<std::uint32_t>();
                if (exists == 0) {
                    collisionCells_.emplace_back();
                    continue;
                }
                if (exists != 1) {
                    throw std::runtime_error("Invalid SMD collision cell presence flag");
                }

                SmdCollisionMainCell cell;
                const std::int32_t shapeCount = reader.read<std::int32_t>();
                requireCount(shapeCount, 1'000'000, "shape index");
                cell.shapeIndices = reader.readVector<std::uint16_t>(static_cast<std::size_t>(shapeCount));

                for (SmdCollisionSubCell& subCell : cell.subCells) {
                    const std::int32_t polygonCount = reader.read<std::int32_t>();
                    requireCount(polygonCount, kMaximumCollisionFaces, "sub-cell polygon");
                    const auto indexCount = static_cast<std::size_t>(polygonCount) * 3U;
                    subCell.triangleIndices = reader.readVector<std::uint32_t>(indexCount);
                    for (const std::uint32_t index : subCell.triangleIndices) {
                        if (index >= collisionTriangles_.size() * 3U && !collisionTriangles_.empty()) {
                            throw std::runtime_error("SMD collision vertex index is outside the collision buffer");
                        }
                    }
                }
                collisionCells_.push_back(std::move(cell));
            }
        }

        const std::int32_t objectEventCount = reader.read<std::int32_t>();
        requireCount(objectEventCount, kMaximumObjectEvents, "object event");
        objectEvents_.reserve(static_cast<std::size_t>(objectEventCount));
        for (std::int32_t index = 0; index < objectEventCount; ++index) {
            const auto bytes = reader.readBytes(24);
            std::array<std::byte, 24> event {};
            std::copy(bytes.begin(), bytes.end(), event.begin());
            objectEvents_.push_back(event);
        }

        eventGrid_ = reader.readVector<std::int16_t>(heightCount);

        if (reader.remaining() >= sizeof(std::int32_t)) {
            const std::int32_t regeneCount = reader.read<std::int32_t>();
            requireCount(regeneCount, kMaximumRegeneEvents, "regene event");
            regeneEvents_.reserve(static_cast<std::size_t>(regeneCount));
            for (std::int32_t index = 0; index < regeneCount; ++index) {
                SmdRegeneEvent event;
                event.positionX = reader.read<float>();
                event.positionY = reader.read<float>();
                event.positionZ = reader.read<float>();
                event.areaZ = reader.read<float>();
                event.areaX = reader.read<float>();
                event.point = index;
                regeneEvents_.push_back(event);
            }
        }

        if (reader.remaining() >= sizeof(std::int32_t)) {
            const std::int32_t warpCount = reader.read<std::int32_t>();
            requireCount(warpCount, kMaximumWarps, "warp");
            warps_.reserve(static_cast<std::size_t>(warpCount));
            for (std::int32_t index = 0; index < warpCount; ++index) {
                SmdWarpInfo warp;
                warp.warpId = reader.read<std::int16_t>();
                warp.name = reader.readFixedString(32);
                warp.announcement = reader.readFixedString(256);
                warp.unknown0 = reader.read<std::uint16_t>();
                warp.cost = reader.read<std::uint32_t>();
                warp.zone = reader.read<std::int16_t>();
                warp.unknown1 = reader.read<std::uint16_t>();
                warp.x = reader.read<float>();
                warp.y = reader.read<float>();
                warp.z = reader.read<float>();
                warp.radius = reader.read<float>();
                warp.nation = reader.read<std::int16_t>();
                warp.unknown2 = reader.read<std::uint16_t>();
                warps_.push_back(std::move(warp));
            }
        }

        if (reader.remaining() != 0) {
            throw std::runtime_error("Unexpected trailing SMD bytes: " + std::to_string(reader.remaining()));
        }

        const float expectedWidth = static_cast<float>(mapSize_ - 1) * unitDistance_;
        if (std::fabs(expectedWidth - collisionWidth_) > 0.05F
            || std::fabs(expectedWidth - collisionLength_) > 0.05F) {
            throw std::runtime_error("SMD terrain and collision dimensions do not match");
        }

        loaded_ = true;
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
        loaded_ = false;
        return false;
    }
}

float SmdMap::heightAt(float x, float z) const noexcept {
    if (!loaded_ || !contains(x, z) || mapSize_ < 2 || unitDistance_ <= 0.0F) {
        return 0.0F;
    }

    const float gridX = x / unitDistance_;
    const float gridZ = z / unitDistance_;
    const auto x0 = static_cast<std::int32_t>(std::floor(gridX));
    const auto z0 = static_cast<std::int32_t>(std::floor(gridZ));
    const auto x1 = std::min(x0 + 1, mapSize_ - 1);
    const auto z1 = std::min(z0 + 1, mapSize_ - 1);
    const float tx = gridX - static_cast<float>(x0);
    const float tz = gridZ - static_cast<float>(z0);

    const auto sample = [this](std::int32_t sampleX, std::int32_t sampleZ) {
        const auto index = static_cast<std::size_t>(sampleX) * static_cast<std::size_t>(mapSize_)
            + static_cast<std::size_t>(sampleZ);
        return heights_[index];
    };

    const float h00 = sample(x0, z0);
    const float h10 = sample(x1, z0);
    const float h01 = sample(x0, z1);
    const float h11 = sample(x1, z1);
    const float top = h00 + (h10 - h00) * tx;
    const float bottom = h01 + (h11 - h01) * tx;
    return top + (bottom - top) * tz;
}

bool SmdMap::contains(float x, float z) const noexcept {
    return loaded_ && x >= 0.0F && z >= 0.0F && x <= collisionWidth_ && z <= collisionLength_;
}

void SmdMap::clear() noexcept {
    loaded_ = false;
    error_.clear();
    sourcePath_.clear();
    mapSize_ = 0;
    unitDistance_ = 0.0F;
    collisionWidth_ = 0.0F;
    collisionLength_ = 0.0F;
    heights_.clear();
    collisionTriangles_.clear();
    collisionCells_.clear();
    objectEvents_.clear();
    eventGrid_.clear();
    regeneEvents_.clear();
    warps_.clear();
}

} // namespace korework::content
