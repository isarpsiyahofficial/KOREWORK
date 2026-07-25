#pragma once

#include "offline_roster.hpp"
#include "offline_runtime.hpp"

#include "raylib.h"

#include <vector>

namespace korework::client {

class OfflineSkillVfx final {
public:
    void spawn(const SkillDefinition& skill, PlayerClass playerClass, Vec3 source, Vec3 target);
    void update(float deltaSeconds);
    void draw() const;
    void clear() noexcept { effects_.clear(); }

private:
    struct Effect {
        Vec3 source;
        Vec3 target;
        Color color = WHITE;
        float age = 0.0F;
        float duration = 0.45F;
        bool healing = false;
        bool projectile = false;
    };

    std::vector<Effect> effects_;
};

} // namespace korework::client
