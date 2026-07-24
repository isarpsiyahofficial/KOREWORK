#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace korework::content {

struct N3AnimationClip {
    std::string name;
    float frameStart = 0.0F;
    float frameEnd = 0.0F;
    float framesPerSecond = 30.0F;
    float plugTraceStart = 0.0F;
    float plugTraceEnd = 0.0F;
    float soundFrame0 = 0.0F;
    float soundFrame1 = 0.0F;
    float blendTime = 0.25F;
    std::int32_t blendFlags = 0;
    float strikeFrame0 = 0.0F;
    float strikeFrame1 = 0.0F;

    [[nodiscard]] float durationSeconds() const noexcept;
    [[nodiscard]] bool contains(float frame) const noexcept;
};

class N3AnimationSet final {
public:
    [[nodiscard]] static N3AnimationSet load(const std::filesystem::path& path);
    [[nodiscard]] const std::vector<N3AnimationClip>& clips() const noexcept { return clips_; }
    [[nodiscard]] const N3AnimationClip* find(const std::string& name) const noexcept;
    [[nodiscard]] const N3AnimationClip* preferredIdle() const noexcept;
    [[nodiscard]] const N3AnimationClip* preferredMove() const noexcept;
    [[nodiscard]] const N3AnimationClip* preferredAttack() const noexcept;

private:
    [[nodiscard]] const N3AnimationClip* findContaining(const std::vector<std::string>& terms) const noexcept;
    std::vector<N3AnimationClip> clips_;
};

} // namespace korework::content
