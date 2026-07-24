#include "content/n3_skeleton.hpp"

#include "content/binary_reader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace korework::content {
namespace {

constexpr std::int32_t kMaximumKeyCount = 2'000'000;
constexpr std::int32_t kMaximumJointCount = 8192;
constexpr std::int32_t kMaximumChildren = 1024;
constexpr std::int32_t kMaximumDepth = 256;

void requireCount(std::int32_t count, std::int32_t maximum, const char* label) {
    if (count < 0 || count > maximum) {
        throw std::runtime_error(std::string("Invalid ") + label + " count: " + std::to_string(count));
    }
}

void requireFinite(float value, const char* label) {
    if (!std::isfinite(value)) throw std::runtime_error(std::string("Invalid ") + label + " value");
}

N3Vector3 readVector3(BinaryReader& reader) {
    N3Vector3 value {reader.read<float>(), reader.read<float>(), reader.read<float>()};
    requireFinite(value.x, "vector");
    requireFinite(value.y, "vector");
    requireFinite(value.z, "vector");
    return value;
}

N3Quaternion readQuaternion(BinaryReader& reader) {
    N3Quaternion value {reader.read<float>(), reader.read<float>(), reader.read<float>(), reader.read<float>()};
    requireFinite(value.x, "quaternion");
    requireFinite(value.y, "quaternion");
    requireFinite(value.z, "quaternion");
    requireFinite(value.w, "quaternion");
    return value;
}

N3Vector3 lerp(const N3Vector3& left, const N3Vector3& right, float amount) noexcept {
    return {
        left.x + (right.x - left.x) * amount,
        left.y + (right.y - left.y) * amount,
        left.z + (right.z - left.z) * amount
    };
}

N3Quaternion normalize(N3Quaternion value) noexcept {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
    if (length <= 0.000001F) return {};
    value.x /= length;
    value.y /= length;
    value.z /= length;
    value.w /= length;
    return value;
}

N3Quaternion multiplyQuaternion(const N3Quaternion& left, const N3Quaternion& right) noexcept {
    return normalize({
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z
    });
}

N3Quaternion slerp(N3Quaternion left, N3Quaternion right, float amount) noexcept {
    left = normalize(left);
    right = normalize(right);
    float dot = left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w;
    dot = std::clamp(dot, -1.0F, 1.0F);
    if (dot < 0.0F) {
        right = {-right.x, -right.y, -right.z, -right.w};
        dot = -dot;
    }
    if (dot > 0.9995F) {
        return normalize({
            left.x + amount * (right.x - left.x),
            left.y + amount * (right.y - left.y),
            left.z + amount * (right.z - left.z),
            left.w + amount * (right.w - left.w)
        });
    }
    const float theta0 = std::acos(dot);
    const float sinTheta0 = std::sin(theta0);
    if (std::fabs(sinTheta0) < 0.000001F) return left;
    const float theta = theta0 * amount;
    const float first = std::cos(theta) - dot * std::sin(theta) / sinTheta0;
    const float second = std::sin(theta) / sinTheta0;
    return normalize({
        first * left.x + second * right.x,
        first * left.y + second * right.y,
        first * left.z + second * right.z,
        first * left.w + second * right.w
    });
}

N3VectorKey readVectorKey(BinaryReader& reader) {
    N3VectorKey key;
    const std::int32_t count = reader.read<std::int32_t>();
    requireCount(count, kMaximumKeyCount, "vector key");
    if (count == 0) return key;
    const std::uint32_t type = reader.read<std::uint32_t>();
    if (type != 0U) throw std::runtime_error("Expected VECTOR3 animation key");
    key.samplingRate = reader.read<float>();
    requireFinite(key.samplingRate, "sampling rate");
    if (key.samplingRate <= 0.0F || key.samplingRate > 10'000.0F) throw std::runtime_error("Invalid vector key sampling rate");
    key.values.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 0; index < count; ++index) key.values.push_back(readVector3(reader));
    return key;
}

N3QuaternionKey readQuaternionKey(BinaryReader& reader) {
    N3QuaternionKey key;
    const std::int32_t count = reader.read<std::int32_t>();
    requireCount(count, kMaximumKeyCount, "quaternion key");
    if (count == 0) return key;
    const std::uint32_t type = reader.read<std::uint32_t>();
    if (type != 1U) throw std::runtime_error("Expected QUATERNION animation key");
    key.samplingRate = reader.read<float>();
    requireFinite(key.samplingRate, "sampling rate");
    if (key.samplingRate <= 0.0F || key.samplingRate > 10'000.0F) throw std::runtime_error("Invalid quaternion key sampling rate");
    key.values.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 0; index < count; ++index) key.values.push_back(normalize(readQuaternion(reader)));
    return key;
}

N3Matrix4 localMatrix(const N3Joint& joint, float frame) {
    const N3Vector3 position = joint.positionKeys.sample(frame, joint.position);
    const N3Quaternion rotation = joint.rotationKeys.sample(frame, joint.rotation);
    const N3Vector3 scale = joint.scaleKeys.sample(frame, joint.scale);
    const N3Quaternion orientation = joint.orientationKeys.sample(frame, joint.orientation);
    const N3Quaternion combined = multiplyQuaternion(rotation, orientation);

    const float x = combined.x;
    const float y = combined.y;
    const float z = combined.z;
    const float w = combined.w;
    N3Matrix4 matrix;
    matrix.value = {
        (1.0F - 2.0F * (y * y + z * z)) * scale.x,
        (2.0F * (x * y - w * z)) * scale.y,
        (2.0F * (x * z + w * y)) * scale.z,
        position.x,
        (2.0F * (x * y + w * z)) * scale.x,
        (1.0F - 2.0F * (x * x + z * z)) * scale.y,
        (2.0F * (y * z - w * x)) * scale.z,
        position.y,
        (2.0F * (x * z - w * y)) * scale.x,
        (2.0F * (y * z + w * x)) * scale.y,
        (1.0F - 2.0F * (x * x + y * y)) * scale.z,
        position.z,
        0.0F, 0.0F, 0.0F, 1.0F
    };
    return matrix;
}

void readJoint(BinaryReader& reader, std::vector<N3Joint>& joints, std::int32_t parentIndex, std::int32_t depth) {
    if (depth > kMaximumDepth || joints.size() >= static_cast<std::size_t>(kMaximumJointCount)) {
        throw std::runtime_error("N3 joint hierarchy exceeds safety limits");
    }

    const std::int32_t currentIndex = static_cast<std::int32_t>(joints.size());
    joints.emplace_back();
    N3Joint joint;
    joint.name = reader.readString();
    joint.parentIndex = parentIndex;
    joint.position = readVector3(reader);
    joint.rotation = normalize(readQuaternion(reader));
    joint.scale = readVector3(reader);
    joint.positionKeys = readVectorKey(reader);
    joint.rotationKeys = readQuaternionKey(reader);
    joint.scaleKeys = readVectorKey(reader);
    joint.orientationKeys = readQuaternionKey(reader);

    const std::int32_t childCount = reader.read<std::int32_t>();
    requireCount(childCount, kMaximumChildren, "joint child");
    joints[static_cast<std::size_t>(currentIndex)] = std::move(joint);
    for (std::int32_t child = 0; child < childCount; ++child) readJoint(reader, joints, currentIndex, depth + 1);
}

} // namespace

N3Matrix4 N3Matrix4::multiply(const N3Matrix4& left, const N3Matrix4& right) noexcept {
    N3Matrix4 output;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            float value = 0.0F;
            for (std::size_t inner = 0; inner < 4; ++inner) value += left.value[row * 4U + inner] * right.value[inner * 4U + column];
            output.value[row * 4U + column] = value;
        }
    }
    return output;
}

N3Matrix4 N3Matrix4::inverseAffine(const N3Matrix4& matrix) {
    const float a00 = matrix.value[0], a01 = matrix.value[1], a02 = matrix.value[2];
    const float a10 = matrix.value[4], a11 = matrix.value[5], a12 = matrix.value[6];
    const float a20 = matrix.value[8], a21 = matrix.value[9], a22 = matrix.value[10];
    const float determinant = a00 * (a11 * a22 - a12 * a21)
        - a01 * (a10 * a22 - a12 * a20)
        + a02 * (a10 * a21 - a11 * a20);
    if (std::fabs(determinant) < 0.0000001F) throw std::runtime_error("Non-invertible N3 bind matrix");
    const float inverseDeterminant = 1.0F / determinant;

    N3Matrix4 inverse;
    inverse.value[0] = (a11 * a22 - a12 * a21) * inverseDeterminant;
    inverse.value[1] = (a02 * a21 - a01 * a22) * inverseDeterminant;
    inverse.value[2] = (a01 * a12 - a02 * a11) * inverseDeterminant;
    inverse.value[4] = (a12 * a20 - a10 * a22) * inverseDeterminant;
    inverse.value[5] = (a00 * a22 - a02 * a20) * inverseDeterminant;
    inverse.value[6] = (a02 * a10 - a00 * a12) * inverseDeterminant;
    inverse.value[8] = (a10 * a21 - a11 * a20) * inverseDeterminant;
    inverse.value[9] = (a01 * a20 - a00 * a21) * inverseDeterminant;
    inverse.value[10] = (a00 * a11 - a01 * a10) * inverseDeterminant;

    const float tx = matrix.value[3], ty = matrix.value[7], tz = matrix.value[11];
    inverse.value[3] = -(inverse.value[0] * tx + inverse.value[1] * ty + inverse.value[2] * tz);
    inverse.value[7] = -(inverse.value[4] * tx + inverse.value[5] * ty + inverse.value[6] * tz);
    inverse.value[11] = -(inverse.value[8] * tx + inverse.value[9] * ty + inverse.value[10] * tz);
    inverse.value[12] = 0.0F;
    inverse.value[13] = 0.0F;
    inverse.value[14] = 0.0F;
    inverse.value[15] = 1.0F;
    return inverse;
}

N3Vector3 N3Matrix4::transformPoint(const N3Vector3& point) const noexcept {
    return {
        value[0] * point.x + value[1] * point.y + value[2] * point.z + value[3],
        value[4] * point.x + value[5] * point.y + value[6] * point.z + value[7],
        value[8] * point.x + value[9] * point.y + value[10] * point.z + value[11]
    };
}

N3Vector3 N3VectorKey::sample(float frame, const N3Vector3& fallback) const noexcept {
    if (values.empty() || samplingRate <= 0.0F || frame < 0.0F) return fallback;
    const float keyPosition = frame * (samplingRate / 30.0F);
    const std::size_t index = static_cast<std::size_t>(std::floor(keyPosition));
    if (index >= values.size() - 1U) return values.back();
    return lerp(values[index], values[index + 1U], keyPosition - static_cast<float>(index));
}

N3Quaternion N3QuaternionKey::sample(float frame, const N3Quaternion& fallback) const noexcept {
    if (values.empty() || samplingRate <= 0.0F || frame < 0.0F) return fallback;
    const float keyPosition = frame * (samplingRate / 30.0F);
    const std::size_t index = static_cast<std::size_t>(std::floor(keyPosition));
    if (index >= values.size() - 1U) return values.back();
    return slerp(values[index], values[index + 1U], keyPosition - static_cast<float>(index));
}

N3Skeleton N3Skeleton::load(const std::filesystem::path& path) {
    BinaryReader reader(path);
    N3Skeleton skeleton;
    readJoint(reader, skeleton.joints_, -1, 0);
    if (reader.remaining() != 0U) throw std::runtime_error("Unexpected trailing N3 joint bytes: " + std::to_string(reader.remaining()));
    skeleton.bindWorld_ = skeleton.worldMatrices(0.0F);
    skeleton.inverseBindWorld_.reserve(skeleton.bindWorld_.size());
    for (const auto& matrix : skeleton.bindWorld_) skeleton.inverseBindWorld_.push_back(N3Matrix4::inverseAffine(matrix));
    return skeleton;
}

std::vector<N3Matrix4> N3Skeleton::worldMatrices(float frame) const {
    std::vector<N3Matrix4> matrices;
    matrices.reserve(joints_.size());
    for (std::size_t index = 0; index < joints_.size(); ++index) {
        const N3Matrix4 local = localMatrix(joints_[index], frame);
        if (joints_[index].parentIndex < 0) matrices.push_back(local);
        else {
            const auto parent = static_cast<std::size_t>(joints_[index].parentIndex);
            if (parent >= index) throw std::runtime_error("Invalid N3 joint parent order");
            matrices.push_back(N3Matrix4::multiply(matrices[parent], local));
        }
    }
    return matrices;
}

std::vector<N3Matrix4> N3Skeleton::skinMatrices(float frame) const {
    const auto world = worldMatrices(frame);
    if (world.size() != inverseBindWorld_.size()) throw std::runtime_error("N3 skeleton bind matrix count mismatch");
    std::vector<N3Matrix4> skin;
    skin.reserve(world.size());
    for (std::size_t index = 0; index < world.size(); ++index) skin.push_back(N3Matrix4::multiply(world[index], inverseBindWorld_[index]));
    return skin;
}

} // namespace korework::content
