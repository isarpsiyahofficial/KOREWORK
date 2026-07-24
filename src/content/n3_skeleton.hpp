#pragma once

#include "content/n3_character.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace korework::content {

struct N3Matrix4 {
    std::array<float, 16> value {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F
    };

    [[nodiscard]] static N3Matrix4 multiply(const N3Matrix4& left, const N3Matrix4& right) noexcept;
    [[nodiscard]] static N3Matrix4 inverseAffine(const N3Matrix4& matrix);
    [[nodiscard]] N3Vector3 transformPoint(const N3Vector3& point) const noexcept;
};

struct N3VectorKey {
    float samplingRate = 30.0F;
    std::vector<N3Vector3> values;
    [[nodiscard]] N3Vector3 sample(float frame, const N3Vector3& fallback) const noexcept;
    [[nodiscard]] float lastFrame() const noexcept;
};

struct N3QuaternionKey {
    float samplingRate = 30.0F;
    std::vector<N3Quaternion> values;
    [[nodiscard]] N3Quaternion sample(float frame, const N3Quaternion& fallback) const noexcept;
    [[nodiscard]] float lastFrame() const noexcept;
};

struct N3Joint {
    std::string name;
    std::int32_t parentIndex = -1;
    N3Vector3 position;
    N3Quaternion rotation;
    N3Vector3 scale {1.0F, 1.0F, 1.0F};
    N3Quaternion orientation;
    N3VectorKey positionKeys;
    N3QuaternionKey rotationKeys;
    N3VectorKey scaleKeys;
    N3QuaternionKey orientationKeys;
};

class N3Skeleton final {
public:
    [[nodiscard]] static N3Skeleton load(const std::filesystem::path& path);
    [[nodiscard]] const std::vector<N3Joint>& joints() const noexcept { return joints_; }
    [[nodiscard]] const std::vector<N3Matrix4>& bindWorldMatrices() const noexcept { return bindWorld_; }
    [[nodiscard]] float maximumFrame() const noexcept { return maximumFrame_; }
    [[nodiscard]] std::vector<N3Matrix4> worldMatrices(float frame) const;
    [[nodiscard]] std::vector<N3Matrix4> skinMatrices(float frame) const;

private:
    std::vector<N3Joint> joints_;
    std::vector<N3Matrix4> bindWorld_;
    std::vector<N3Matrix4> inverseBindWorld_;
    float maximumFrame_ = 0.0F;
};

} // namespace korework::content
