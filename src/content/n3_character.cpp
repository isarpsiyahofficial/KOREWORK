#include "content/n3_character.hpp"

#include "content/binary_reader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace korework::content {
namespace {

constexpr std::int32_t kMaximumParts = 128;
constexpr std::int32_t kMaximumPlugs = 128;
constexpr std::int32_t kMaximumAnimKeys = 2'000'000;
constexpr std::int32_t kMaximumFaces = 4'000'000;
constexpr std::int32_t kMaximumVertices = 4'000'000;
constexpr std::int32_t kMaximumUvs = 8'000'000;
constexpr std::int32_t kMaximumBoneInfluences = 64;

void requireCount(std::int32_t count, std::int32_t maximum, const char* label) {
    if (count < 0 || count > maximum) throw std::runtime_error(std::string("Invalid ") + label + " count: " + std::to_string(count));
}

N3Vector3 readVector3(BinaryReader& reader) { return {reader.read<float>(), reader.read<float>(), reader.read<float>()}; }
N3Quaternion readQuaternion(BinaryReader& reader) { return {reader.read<float>(), reader.read<float>(), reader.read<float>(), reader.read<float>()}; }

void readAnimKey(BinaryReader& reader) {
    const std::int32_t count = reader.read<std::int32_t>();
    requireCount(count, kMaximumAnimKeys, "animation key");
    if (count == 0) return;
    const std::uint32_t type = reader.read<std::uint32_t>();
    const float samplingRate = reader.read<float>();
    if (!std::isfinite(samplingRate) || samplingRate < 0.0F || samplingRate > 10'000.0F) throw std::runtime_error("Invalid N3 animation sampling rate");
    if (type == 0U) reader.skip(static_cast<std::uint64_t>(count) * 12U);
    else if (type == 1U) reader.skip(static_cast<std::uint64_t>(count) * 16U);
    else throw std::runtime_error("Unsupported N3 animation key type: " + std::to_string(type));
}

void readTransform(BinaryReader& reader, N3Vector3& position, N3Quaternion& rotation, N3Vector3& scale) {
    position = readVector3(reader);
    rotation = readQuaternion(reader);
    scale = readVector3(reader);
    readAnimKey(reader);
    readAnimKey(reader);
    readAnimKey(reader);
}

std::filesystem::path resolveRequired(const KoAssetResolver& resolver, const std::filesystem::path& baseFile,
                                      const std::string& storedPath, const char* label) {
    if (storedPath.empty()) throw std::runtime_error(std::string("Missing ") + label + " path in " + baseFile.string());
    const auto resolved = resolver.resolve(baseFile, storedPath);
    if (!resolved.has_value()) throw std::runtime_error(std::string("Unable to resolve ") + label + " '" + storedPath + "' from " + baseFile.string());
    return *resolved;
}

} // namespace

N3Character N3CharacterLoader::load(const std::filesystem::path& characterPath) const {
    BinaryReader reader(characterPath);
    N3Character character;
    character.sourcePath = characterPath;
    character.name = reader.readString();
    readTransform(reader, character.position, character.rotation, character.scale);
    (void) reader.readString();
    (void) reader.readString();

    const std::string jointReference = reader.readString();
    if (!jointReference.empty()) character.jointPath = resolveRequired(resolver_, characterPath, jointReference, "joint");

    const std::int32_t partCount = reader.read<std::int32_t>();
    requireCount(partCount, kMaximumParts, "character part");
    character.parts.reserve(static_cast<std::size_t>(partCount));
    for (std::int32_t index = 0; index < partCount; ++index) {
        character.parts.push_back(loadPart(resolveRequired(resolver_, characterPath, reader.readString(), "character part")));
    }

    const std::int32_t plugCount = reader.read<std::int32_t>();
    requireCount(plugCount, kMaximumPlugs, "equipment plug");
    character.plugs.reserve(static_cast<std::size_t>(plugCount));
    for (std::int32_t index = 0; index < plugCount; ++index) {
        character.plugs.push_back(resolveRequired(resolver_, characterPath, reader.readString(), "equipment plug"));
    }

    const std::string animationReference = reader.readString();
    if (!animationReference.empty()) character.animationPath = resolveRequired(resolver_, characterPath, animationReference, "animation");
    for (int index = 0; index < 4; ++index) (void) reader.read<std::int32_t>();

    if (reader.remaining() >= sizeof(std::int32_t)) {
        const std::string fxReference = reader.readString();
        if (!fxReference.empty()) (void) resolver_.resolve(characterPath, fxReference);
    }
    if (reader.remaining() >= sizeof(std::int32_t)) {
        const std::string collisionSkinReference = reader.readString();
        if (!collisionSkinReference.empty()) (void) resolver_.resolve(characterPath, collisionSkinReference);
    }
    if (reader.remaining() != 0U) throw std::runtime_error("Unexpected trailing N3 character bytes: " + std::to_string(reader.remaining()));
    if (character.parts.empty()) throw std::runtime_error("N3 character contains no renderable parts: " + characterPath.string());
    return character;
}

N3CharacterPart N3CharacterLoader::loadPart(const std::filesystem::path& partPath) const {
    BinaryReader reader(partPath);
    N3CharacterPart part;
    part.sourcePath = partPath;
    part.name = reader.readString();
    const std::int32_t version = reader.read<std::int32_t>();
    if (version < 0 || version > 1) throw std::runtime_error("Unsupported N3 character part version: " + std::to_string(version));

    for (float& channel : part.diffuse) {
        channel = reader.read<float>();
        if (!std::isfinite(channel)) throw std::runtime_error("Invalid N3 material diffuse value");
    }
    reader.skip(76U);

    const std::string textureReference = reader.readString();
    if (!textureReference.empty()) part.texturePath = resolveRequired(resolver_, partPath, textureReference, "texture");
    if (version == 1) {
        const std::string diffuseTextureReference = reader.readString();
        if (!diffuseTextureReference.empty()) (void) resolveRequired(resolver_, partPath, diffuseTextureReference, "diffuse texture");
    }

    part.skinsPath = resolveRequired(resolver_, partPath, reader.readString(), "character skins");
    if (reader.remaining() != 0U) throw std::runtime_error("Unexpected trailing N3 character part bytes: " + std::to_string(reader.remaining()));
    loadSkins(part.skinsPath, part.lods);
    return part;
}

void N3CharacterLoader::loadSkins(const std::filesystem::path& skinsPath, std::array<N3SkinLod, 4>& lods) const {
    BinaryReader reader(skinsPath);
    (void) reader.readString();

    for (N3SkinLod& lod : lods) {
        lod.name = reader.readString();
        const std::int32_t faceCount = reader.read<std::int32_t>();
        const std::int32_t vertexCount = reader.read<std::int32_t>();
        const std::int32_t uvCount = reader.read<std::int32_t>();
        requireCount(faceCount, kMaximumFaces, "skin face");
        requireCount(vertexCount, kMaximumVertices, "skin vertex");
        requireCount(uvCount, kMaximumUvs, "skin UV");
        if ((faceCount == 0) != (vertexCount == 0)) throw std::runtime_error("N3 skin has inconsistent face/vertex counts");

        lod.positions.reserve(static_cast<std::size_t>(vertexCount));
        lod.bindPositions.reserve(static_cast<std::size_t>(vertexCount));
        lod.normals.reserve(static_cast<std::size_t>(vertexCount));
        lod.influences.reserve(static_cast<std::size_t>(vertexCount));
        for (std::int32_t vertex = 0; vertex < vertexCount; ++vertex) {
            lod.positions.push_back(readVector3(reader));
            lod.normals.push_back(readVector3(reader));
        }

        const std::size_t indexCount = static_cast<std::size_t>(faceCount) * 3U;
        lod.faceIndices = reader.readVector<std::uint16_t>(indexCount);
        for (const std::uint16_t index : lod.faceIndices) if (index >= lod.positions.size()) throw std::runtime_error("N3 skin face index exceeds vertex count");

        lod.uvs.reserve(static_cast<std::size_t>(uvCount));
        for (std::int32_t uv = 0; uv < uvCount; ++uv) {
            const float u = reader.read<float>();
            const float rawV = reader.read<float>();
            if (!std::isfinite(u) || !std::isfinite(rawV)) throw std::runtime_error("Invalid N3 skin UV value");
            lod.uvs.push_back({u, 1.0F - rawV});
        }
        lod.uvIndices = reader.readVector<std::uint16_t>(indexCount);
        for (const std::uint16_t index : lod.uvIndices) if (index >= lod.uvs.size() && !lod.uvs.empty()) throw std::runtime_error("N3 skin UV index exceeds UV count");

        for (std::int32_t vertex = 0; vertex < vertexCount; ++vertex) {
            lod.bindPositions.push_back(readVector3(reader));
            const std::int32_t influenceCount = reader.read<std::int32_t>();
            requireCount(influenceCount, kMaximumBoneInfluences, "bone influence");
            (void) reader.read<std::int32_t>();
            (void) reader.read<std::int32_t>();

            N3SkinInfluence influence;
            if (influenceCount > 1) {
                influence.jointIndices = reader.readVector<std::int32_t>(static_cast<std::size_t>(influenceCount));
                influence.weights = reader.readVector<float>(static_cast<std::size_t>(influenceCount));
                float totalWeight = 0.0F;
                for (const float weight : influence.weights) {
                    if (!std::isfinite(weight) || weight < -0.001F || weight > 1.001F) throw std::runtime_error("Invalid N3 skin bone weight");
                    totalWeight += weight;
                }
                if (!influence.weights.empty() && (totalWeight < 0.5F || totalWeight > 1.5F)) throw std::runtime_error("Invalid N3 skin total bone weight");
            } else if (influenceCount == 1) {
                influence.jointIndices.push_back(reader.read<std::int32_t>());
                influence.weights.push_back(1.0F);
            }
            lod.influences.push_back(std::move(influence));
        }
        if (lod.bindPositions.size() != lod.positions.size() || lod.influences.size() != lod.positions.size()) {
            throw std::runtime_error("N3 skin bind-pose/influence count mismatch");
        }
    }

    if (reader.remaining() != 0U) throw std::runtime_error("Unexpected trailing N3 skin bytes: " + std::to_string(reader.remaining()));
}

} // namespace korework::content
