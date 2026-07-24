#pragma once

#include "content/n3_character.hpp"

#include "raylib.h"

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
    void unload() noexcept;
    void draw(Vector3 worldPosition, float targetHeight, Color tint = WHITE) const;

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] float sourceHeight() const noexcept { return sourceHeight_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::size_t partCount() const noexcept { return models_.size(); }

private:
    std::vector<Model> models_;
    bool ready_ = false;
    float sourceHeight_ = 0.0F;
    std::string error_;
};

} // namespace korework::client
