#include "content/n3_shape.hpp"

#include "content/binary_reader.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace korework::content {
namespace {

constexpr std::int32_t kMaximumParts = 16'384;
constexpr std::int32_t kMaximumTextures = 256;
constexpr std::int32_t kMaximumAnimKeys = 2'000'000;

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
    requireFinite(value.x, "shape vector");
    requireFinite(value.y, "shape vector");
    requireFinite(value.z, "shape vector");
    return value;
}

N3Quaternion readQuaternion(BinaryReader& reader) {
    N3Quaternion value {reader.read<float>(), reader.read<float>(), reader.read<float>(), reader.read<float>()};
    requireFinite(value.x, "shape quaternion");
    requireFinite(value.y, "shape quaternion");
    requireFinite(value.z, "shape quaternion");
    requireFinite(value.w, "shape quaternion");
    return value;
}

void skipAnimationKey(BinaryReader& reader) {
    const std::int32_t count = reader.read<std::int32_t>();
    requireCount(count, kMaximumAnimKeys, "shape animation key");
    if (count == 0) return;
    const std::uint32_t type = reader.read<std::uint32_t>();
    const float samplingRate = reader.read<float>();
    requireFinite(samplingRate, "shape animation sampling rate");
    if (samplingRate <= 0.0F || samplingRate > 10'000.0F) {
        throw std::runtime_error("Invalid N3 shape animation sampling rate");
    }
    if (type == 0U) reader.skip(static_cast<std::uint64_t>(count) * 12U);
    else if (type == 1U) reader.skip(static_cast<std::uint64_t>(count) * 16U);
    else throw std::runtime_error("Unsupported N3 shape animation key type: " + std::to_string(type));
}

std::filesystem::path resolveOptional(const KoAssetResolver& resolver,
                                      const std::filesystem::path& baseFile,
                                      const std::string& storedPath,
                                      const char* label) {
    if (storedPath.empty()) return {};
    const auto resolved = resolver.resolve(baseFile, storedPath);
    if (!resolved.has_value()) {
        throw std::runtime_error(std::string("Unable to resolve ") + label + " '" + storedPath
                                 + "' from " + baseFile.string());
    }
    return *resolved;
}

} // namespace

N3Shape N3ShapeLoader::load(const std::filesystem::path& shapePath) const {
    BinaryReader reader(shapePath);
    N3Shape shape;
    shape.sourcePath = shapePath;
    shape.name = reader.readString(256U);
    shape.position = readVector3(reader);
    shape.rotation = readQuaternion(reader);
    shape.scale = readVector3(reader);
    skipAnimationKey(reader);
    skipAnimationKey(reader);
    skipAnimationKey(reader);

    shape.collisionMeshPath = resolveOptional(resolver_, shapePath, reader.readString(), "shape collision mesh");
    shape.climbMeshPath = resolveOptional(resolver_, shapePath, reader.readString(), "shape climb mesh");

    const std::int32_t partCount = reader.read<std::int32_t>();
    requireCount(partCount, kMaximumParts, "shape part");
    shape.parts.reserve(static_cast<std::size_t>(partCount));
    for (std::int32_t partIndex = 0; partIndex < partCount; ++partIndex) {
        N3ShapePart part;
        part.pivot = readVector3(reader);
        part.meshPath = resolveOptional(resolver_, shapePath, reader.readString(), "shape part mesh");
        for (float& channel : part.diffuse) {
            channel = reader.read<float>();
            requireFinite(channel, "shape material diffuse");
        }
        // Remaining D3DMATERIAL9 channels: ambient, specular, emissive and power.
        for (int materialFloat = 0; materialFloat < 13; ++materialFloat) {
            requireFinite(reader.read<float>(), "shape material channel");
        }
        (void) reader.read<std::uint32_t>(); // Color operation.
        (void) reader.read<std::uint32_t>(); // Color argument 1.
        (void) reader.read<std::uint32_t>(); // Color argument 2.
        part.renderFlags = reader.read<std::uint32_t>();
        part.sourceBlend = reader.read<std::uint32_t>();
        part.destinationBlend = reader.read<std::uint32_t>();
        const std::int32_t textureCount = reader.read<std::int32_t>();
        requireCount(textureCount, kMaximumTextures, "shape texture");
        part.textureFps = reader.read<float>();
        requireFinite(part.textureFps, "shape texture FPS");
        part.texturePaths.reserve(static_cast<std::size_t>(textureCount));
        for (std::int32_t textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
            const auto texture = resolveOptional(resolver_, shapePath, reader.readString(), "shape texture");
            if (!texture.empty()) part.texturePaths.push_back(texture);
        }
        if (part.meshPath.empty()) throw std::runtime_error("N3 shape part contains no mesh: " + shapePath.string());
        part.mesh = equipmentLoader_.loadProgressiveMesh(part.meshPath);
        shape.parts.push_back(std::move(part));
    }

    shape.belong = reader.read<std::int32_t>();
    shape.eventId = reader.read<std::int32_t>();
    shape.eventType = reader.read<std::int32_t>();
    shape.npcId = reader.read<std::int32_t>();
    shape.npcStatus = reader.read<std::int32_t>();
    if (reader.remaining() != 0U) {
        throw std::runtime_error("Unexpected trailing N3 shape bytes: " + std::to_string(reader.remaining()));
    }
    if (shape.parts.empty()) throw std::runtime_error("N3 shape contains no renderable parts: " + shapePath.string());
    return shape;
}

} // namespace korework::content
