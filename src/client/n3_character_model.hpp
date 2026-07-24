#pragma once

#include "content/n3_character.hpp"
#include "content/n3_skeleton.hpp"

#include "raylib.h"

#include <cstdint>
#include <string>
#include <vector>

namespace korework::client {

class N3CharacterModel final {
public:
    N3CharacterModel() = default;
    ~N3CharacterModel();

    N3CharacterModel(const N3CharacterModel&) = delete;
    N3CharacterModel& operator=(const N3CharacterModel&) = delete;

    bool load(const content::N3Character& character, std::size_t preferredLod = 0);
    bool updateAnimation(float frame) noexcept;
    void unload() noexcept;
    void draw(Vector3 worldPosition, float targetHeight, Color tint = WHITE);

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] bool animated() const noexcept { return animated_; }
    [[nodiscard]] float sourceHeight() const noexcept { return sourceHeight_; }
    [[nodiscard]] float maximumFrame() const noexcept { return skeleton_.maximumFrame(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::size_t partCount() const noexcept { return parts_.size(); }
    [[nodiscard]] std::size_t textureCount() const noexcept;

private:
    struct PartRuntime {
        Model model {};
        Texture2D texture {};
        std::vector<float> gpuPositions;
        std::vector<std::uint16_t> cornerSourceIndices;
        std::vector<content::N3Vector3> bindPositions;
        std::vector<content::N3SkinInfluence> influences;
    };

    [[nodiscard]] content::N3Vector3 skinVertex(const PartRuntime& part,
                                                std::size_t sourceVertex,
                                                const std::vector<content::N3Matrix4>& skinMatrices) const;

    std::vector<PartRuntime> parts_;
    content::N3Skeleton skeleton_;
    bool ready_ = false;
    bool animated_ = false;
    float sourceHeight_ = 0.0F;
    float centerX_ = 0.0F;
    float centerZ_ = 0.0F;
    float minimumY_ = 0.0F;
    float lastAnimationFrame_ = -1.0F;
    std::string error_;
};

} // namespace korework::client
