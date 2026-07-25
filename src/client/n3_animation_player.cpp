#include "client/n3_animation_player.hpp"

#include <algorithm>
#include <cmath>

namespace korework::client {

bool N3AnimationPlayer::configure(const content::N3AnimationSet& animations) noexcept {
    reset();
    const auto* idle = animations.preferredIdle();
    const auto* move = animations.preferredMove();
    const auto* attack = animations.preferredAttack();
    if (idle == nullptr || move == nullptr || attack == nullptr) return false;
    if (idle->frameEnd <= idle->frameStart || move->frameEnd <= move->frameStart || attack->frameEnd <= attack->frameStart) return false;
    if (idle->framesPerSecond <= 0.0F || move->framesPerSecond <= 0.0F || attack->framesPerSecond <= 0.0F) return false;

    idle_ = *idle;
    move_ = *move;
    attack_ = *attack;
    ready_ = true;
    state_ = N3AnimationState::Idle;
    frame_ = idle_.frameStart;
    return true;
}

void N3AnimationPlayer::reset() noexcept {
    idle_ = {};
    move_ = {};
    attack_ = {};
    state_ = N3AnimationState::Idle;
    frame_ = 0.0F;
    ready_ = false;
}

void N3AnimationPlayer::setState(N3AnimationState state, bool restart) noexcept {
    if (!ready_) return;
    if (state_ != state || restart) {
        state_ = state;
        frame_ = selectedClip().frameStart;
    }
}

void N3AnimationPlayer::update(float deltaSeconds) noexcept {
    if (!ready_ || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0F) return;
    const auto& active = selectedClip();
    const float span = active.frameEnd - active.frameStart;
    if (span <= 0.0F || active.framesPerSecond <= 0.0F) {
        frame_ = active.frameStart;
        return;
    }

    frame_ += std::min(deltaSeconds, 0.25F) * active.framesPerSecond;
    if (frame_ > active.frameEnd) {
        frame_ = active.frameStart + std::fmod(frame_ - active.frameStart, span);
    }
    frame_ = std::clamp(frame_, active.frameStart, active.frameEnd);
}

const content::N3AnimationClip* N3AnimationPlayer::clip() const noexcept {
    return ready_ ? &selectedClip() : nullptr;
}

std::string_view N3AnimationPlayer::stateName(N3AnimationState state) noexcept {
    switch (state) {
        case N3AnimationState::Idle: return "idle";
        case N3AnimationState::Move: return "move";
        case N3AnimationState::Attack: return "attack";
    }
    return "idle";
}

const content::N3AnimationClip& N3AnimationPlayer::selectedClip() const noexcept {
    switch (state_) {
        case N3AnimationState::Move: return move_;
        case N3AnimationState::Attack: return attack_;
        case N3AnimationState::Idle: return idle_;
    }
    return idle_;
}

} // namespace korework::client
