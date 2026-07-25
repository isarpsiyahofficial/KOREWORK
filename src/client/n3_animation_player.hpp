#pragma once

#include "content/n3_animation.hpp"

#include <string_view>

namespace korework::client {

enum class N3AnimationState {
    Idle,
    Move,
    Attack
};

class N3AnimationPlayer final {
public:
    bool configure(const content::N3AnimationSet& animations) noexcept;
    void reset() noexcept;
    void setState(N3AnimationState state, bool restart = false) noexcept;
    void update(float deltaSeconds) noexcept;

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] N3AnimationState state() const noexcept { return state_; }
    [[nodiscard]] float frame() const noexcept { return frame_; }
    [[nodiscard]] const content::N3AnimationClip* clip() const noexcept;
    [[nodiscard]] static std::string_view stateName(N3AnimationState state) noexcept;

private:
    [[nodiscard]] const content::N3AnimationClip& selectedClip() const noexcept;

    content::N3AnimationClip idle_;
    content::N3AnimationClip move_;
    content::N3AnimationClip attack_;
    N3AnimationState state_ = N3AnimationState::Idle;
    float frame_ = 0.0F;
    bool ready_ = false;
};

} // namespace korework::client
