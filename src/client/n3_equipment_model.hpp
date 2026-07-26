#pragma once

#include "content/n3_equipment.hpp"

#include "raylib.h"

#include <cstdint>
#include <string>
#include <vector>

namespace korework::client {

class N3EquipmentModel final {
public:
    N3EquipmentModel() = default;
    ~N3EquipmentModel();

    N3EquipmentModel(const N3EquipmentModel&) = delete;
    N3EquipmentModel& operator=(const N3EquipmentModel&) = delete;

    bool load(const content::N3EquipmentPlug& plug);
    bool update(const content::N3Matrix4& jointWorld,
                float characterCenterX,
                float characterCenterZ,
                float characterMinimumY) noexcept;
    void draw(Vector3 worldPosition,
              float targetCharacterHeight,
              float sourceCharacterHeight,
              Color tint = WHITE,
              float yawDegrees = 0.0F) const;
    void unload() noexcept;

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] std::int32_t jointIndex() const noexcept { return jointIndex_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    [[nodiscard]] content::N3Vector3 transformVertex(const content::N3Vector3& source,
                                                     const content::N3Matrix4& jointWorld) const noexcept;

    Model model_ {};
    Texture2D texture_ {};
    std::vector<content::N3Vector3> sourcePositions_;
    std::vector<std::uint16_t> cornerSourceIndices_;
    std::vector<float> gpuPositions_;
    content::N3Vector3 plugPosition_;
    content::N3Vector3 plugScale_ {1.0F, 1.0F, 1.0F};
    content::N3Matrix4 plugRotation_;
    std::int32_t jointIndex_ = -1;
    bool ready_ = false;
    std::string error_;
};

} // namespace korework::client
