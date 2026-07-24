#include "content/n3_animation.hpp"

#include "content/binary_reader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>

namespace korework::content {
namespace {

constexpr std::int32_t kMaximumAnimationClips = 16'384;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void requireFinite(float value, const char* label) {
    if (!std::isfinite(value)) throw std::runtime_error(std::string("Invalid N3 animation ") + label);
}

} // namespace

float N3AnimationClip::durationSeconds() const noexcept {
    return framesPerSecond > 0.0F && frameEnd >= frameStart
        ? (frameEnd - frameStart) / framesPerSecond
        : 0.0F;
}

bool N3AnimationClip::contains(float frame) const noexcept {
    return frame >= frameStart && frame <= frameEnd;
}

N3AnimationSet N3AnimationSet::load(const std::filesystem::path& path) {
    BinaryReader reader(path);
    const std::int32_t count = reader.read<std::int32_t>();
    if (count < 0 || count > kMaximumAnimationClips) {
        throw std::runtime_error("Invalid N3 animation clip count: " + std::to_string(count));
    }

    N3AnimationSet set;
    set.clips_.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 0; index < count; ++index) {
        (void) reader.read<std::int32_t>(); // Reserved legacy field.
        N3AnimationClip clip;
        clip.frameStart = reader.read<float>();
        clip.frameEnd = reader.read<float>();
        clip.framesPerSecond = reader.read<float>();
        clip.plugTraceStart = reader.read<float>();
        clip.plugTraceEnd = reader.read<float>();
        clip.soundFrame0 = reader.read<float>();
        clip.soundFrame1 = reader.read<float>();
        clip.blendTime = reader.read<float>();
        clip.blendFlags = reader.read<std::int32_t>();
        clip.strikeFrame0 = reader.read<float>();
        clip.strikeFrame1 = reader.read<float>();
        clip.name = reader.readString(4096);

        requireFinite(clip.frameStart, "start frame");
        requireFinite(clip.frameEnd, "end frame");
        requireFinite(clip.framesPerSecond, "frame rate");
        requireFinite(clip.plugTraceStart, "plug trace start");
        requireFinite(clip.plugTraceEnd, "plug trace end");
        requireFinite(clip.soundFrame0, "sound frame 0");
        requireFinite(clip.soundFrame1, "sound frame 1");
        requireFinite(clip.blendTime, "blend time");
        requireFinite(clip.strikeFrame0, "strike frame 0");
        requireFinite(clip.strikeFrame1, "strike frame 1");
        if (clip.frameStart < 0.0F || clip.frameEnd < clip.frameStart || clip.frameEnd > 10'000'000.0F) {
            throw std::runtime_error("Invalid N3 animation frame range");
        }
        if (clip.framesPerSecond <= 0.0F || clip.framesPerSecond > 1000.0F) {
            throw std::runtime_error("Invalid N3 animation frames-per-second value");
        }
        if (clip.blendTime < 0.0F || clip.blendTime > 60.0F) {
            throw std::runtime_error("Invalid N3 animation blend time");
        }
        set.clips_.push_back(std::move(clip));
    }

    if (reader.remaining() != 0U) {
        throw std::runtime_error("Unexpected trailing N3 animation bytes: " + std::to_string(reader.remaining()));
    }
    return set;
}

const N3AnimationClip* N3AnimationSet::find(const std::string& name) const noexcept {
    const std::string wanted = lower(name);
    for (const auto& clip : clips_) if (lower(clip.name) == wanted) return &clip;
    return nullptr;
}

const N3AnimationClip* N3AnimationSet::findContaining(const std::vector<std::string>& terms) const noexcept {
    for (const auto& term : terms) {
        const std::string wanted = lower(term);
        for (const auto& clip : clips_) {
            if (lower(clip.name).find(wanted) != std::string::npos && clip.frameEnd > clip.frameStart) return &clip;
        }
    }
    return nullptr;
}

const N3AnimationClip* N3AnimationSet::preferredIdle() const noexcept {
    if (const auto* clip = findContaining({"idle", "wait", "breath", "stand"}); clip != nullptr) return clip;
    for (const auto& clip : clips_) if (clip.frameEnd > clip.frameStart) return &clip;
    return nullptr;
}

const N3AnimationClip* N3AnimationSet::preferredMove() const noexcept {
    if (const auto* clip = findContaining({"run", "walk", "move"}); clip != nullptr) return clip;
    return preferredIdle();
}

const N3AnimationClip* N3AnimationSet::preferredAttack() const noexcept {
    if (const auto* clip = findContaining({"attack", "strike", "skill", "hit"}); clip != nullptr) return clip;
    return preferredIdle();
}

} // namespace korework::content
