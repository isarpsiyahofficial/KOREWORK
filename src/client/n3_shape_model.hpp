#pragma once

#include "content/n3_shape.hpp"

#include "raylib.h"

#include <cstdint>
#include <string>
#include <vector>

namespace korework::client {

class N3ShapeModel final {
public:
    N3ShapeModel() = default;
    ~N3ShapeModel();
    N3ShapeModel(const N3ShapeModel&) = delete;
    N3ShapeModel& operator=(const N3ShapeModel&) = delete;

    bool load(const content::N3Shape& shape);
    void unload() noexcept;
    void draw(Vector3 worldPosition = {}, float scale = 1.0F, Color tint = WHITE) const;

    [[nodiscard]] bool ready() const noexcept { return !parts_.empty(); }
    [[nodiscard]] float sourceHeight() const noexcept { return sourceHeight_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    struct Part {
        Model model {};
        Texture2D texture {};
        std::uint32_t renderFlags = 0U;
    };

    std::vector<Part> parts_;
    float sourceHeight_ = 1.0F;
    std::string error_;
};

} // namespace korework::client
