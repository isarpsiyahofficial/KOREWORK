#pragma once

#include "content/smd_map.hpp"

#include "raylib.h"

#include <cstddef>
#include <string>

namespace korework::client {

class SmdTerrainModel final {
public:
    SmdTerrainModel() = default;
    ~SmdTerrainModel();

    SmdTerrainModel(const SmdTerrainModel&) = delete;
    SmdTerrainModel& operator=(const SmdTerrainModel&) = delete;

    bool load(const content::SmdMap& map, std::size_t maximumSamplesPerAxis = 241);
    void unload() noexcept;
    void draw() const;

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::size_t samplesX() const noexcept { return samplesX_; }
    [[nodiscard]] std::size_t samplesZ() const noexcept { return samplesZ_; }

private:
    Model model_ {};
    bool ready_ = false;
    std::string error_;
    std::size_t samplesX_ = 0;
    std::size_t samplesZ_ = 0;
};

} // namespace korework::client
