#include "client/offline_skill_vfx.hpp"

#include <algorithm>
#include <cmath>

namespace korework::client {
namespace {

Color classEffectColor(PlayerClass playerClass) {
    switch (playerClass) {
        case PlayerClass::Warrior: return Color{238, 94, 45, 230};
        case PlayerClass::Rogue: return Color{95, 221, 104, 230};
        case PlayerClass::Mage: return Color{83, 144, 255, 235};
        case PlayerClass::Priest: return Color{255, 218, 98, 235};
    }
    return WHITE;
}

Vec3 interpolate(const Vec3& left, const Vec3& right, float amount) {
    return {left.x + (right.x - left.x) * amount,
            left.y + (right.y - left.y) * amount,
            left.z + (right.z - left.z) * amount};
}

} // namespace

void OfflineSkillVfx::spawn(const SkillDefinition& skill, PlayerClass playerClass, Vec3 source, Vec3 target) {
    Effect effect;
    effect.source = source;
    effect.target = target;
    effect.source.y += 1.1F;
    effect.target.y += skill.heal > 0.0F ? 1.0F : 0.8F;
    effect.color = classEffectColor(playerClass);
    effect.healing = skill.heal > 0.0F;
    const float dx = effect.target.x - effect.source.x;
    const float dz = effect.target.z - effect.source.z;
    effect.projectile = !effect.healing && dx * dx + dz * dz > 10.0F;
    effect.duration = effect.healing ? 0.80F : (effect.projectile ? 0.55F : 0.32F);
    effects_.push_back(effect);
    if (effects_.size() > 64U) effects_.erase(effects_.begin(), effects_.begin() + static_cast<std::ptrdiff_t>(effects_.size() - 64U));
}

void OfflineSkillVfx::update(float deltaSeconds) {
    for (auto& effect : effects_) effect.age += std::max(0.0F, deltaSeconds);
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [](const Effect& effect) {
        return effect.age >= effect.duration;
    }), effects_.end());
}

void OfflineSkillVfx::draw() const {
    for (const auto& effect : effects_) {
        const float progress = std::clamp(effect.age / std::max(0.001F, effect.duration), 0.0F, 1.0F);
        const float fade = 1.0F - progress;
        const Color color = Fade(effect.color, fade);
        if (effect.healing) {
            const Vector3 center {effect.target.x, effect.target.y + progress * 1.5F, effect.target.z};
            DrawCircle3D(center, 0.65F + progress * 0.45F, {1.0F, 0.0F, 0.0F}, 90.0F, color);
            DrawSphere(center, 0.14F + fade * 0.12F, color);
        } else if (effect.projectile) {
            const Vec3 current = interpolate(effect.source, effect.target, progress);
            DrawSphere({current.x, current.y, current.z}, 0.16F + fade * 0.10F, color);
            DrawLine3D({effect.source.x, effect.source.y, effect.source.z},
                       {current.x, current.y, current.z}, Fade(effect.color, fade * 0.55F));
        } else {
            const float radius = 0.4F + progress * 1.35F;
            DrawCircle3D({effect.target.x, effect.target.y - 0.75F, effect.target.z}, radius,
                         {1.0F, 0.0F, 0.0F}, 90.0F, color);
        }
    }
}

} // namespace korework::client
