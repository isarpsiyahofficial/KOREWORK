#include "client/n3_character_model.hpp"

#include "content/n3_texture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace korework::client {
namespace {

const content::N3SkinLod* selectLod(const content::N3CharacterPart& part, std::size_t preferred) {
    if (preferred < part.lods.size() && !part.lods[preferred].bindPositions.empty()) return &part.lods[preferred];
    for (const auto& lod : part.lods) if (!lod.bindPositions.empty()) return &lod;
    return nullptr;
}

unsigned char channel(float value) {
    return static_cast<unsigned char>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

void releaseCpuMesh(Mesh& mesh) noexcept {
    if (mesh.vertices != nullptr) MemFree(mesh.vertices);
    if (mesh.normals != nullptr) MemFree(mesh.normals);
    if (mesh.texcoords != nullptr) MemFree(mesh.texcoords);
    mesh.vertices = nullptr;
    mesh.normals = nullptr;
    mesh.texcoords = nullptr;
}

struct Bounds {
    float minimumX = std::numeric_limits<float>::max();
    float minimumY = std::numeric_limits<float>::max();
    float minimumZ = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    float maximumY = std::numeric_limits<float>::lowest();
    float maximumZ = std::numeric_limits<float>::lowest();

    void include(const content::N3Vector3& point) {
        minimumX = std::min(minimumX, -point.x);
        minimumY = std::min(minimumY, point.y);
        minimumZ = std::min(minimumZ, point.z);
        maximumX = std::max(maximumX, -point.x);
        maximumY = std::max(maximumY, point.y);
        maximumZ = std::max(maximumZ, point.z);
    }
};

} // namespace

N3CharacterModel::~N3CharacterModel() { unload(); }

content::N3Vector3 N3CharacterModel::skinVertex(
    const PartRuntime& part,
    std::size_t sourceVertex,
    const std::vector<content::N3Matrix4>& skinMatrices) const {
    if (sourceVertex >= part.bindPositions.size() || sourceVertex >= part.influences.size()) {
        throw std::runtime_error("N3 dynamic skin source vertex is outside the part data");
    }

    const auto& bindPosition = part.bindPositions[sourceVertex];
    const auto& influence = part.influences[sourceVertex];
    if (influence.jointIndices.empty()) return bindPosition;
    if (influence.jointIndices.size() != influence.weights.size()) {
        throw std::runtime_error("N3 dynamic skin influence arrays are inconsistent");
    }

    content::N3Vector3 result {};
    float totalWeight = 0.0F;
    for (std::size_t index = 0; index < influence.jointIndices.size(); ++index) {
        const std::int32_t jointIndex = influence.jointIndices[index];
        const float weight = influence.weights[index];
        if (jointIndex < 0 || static_cast<std::size_t>(jointIndex) >= skinMatrices.size()) {
            throw std::runtime_error("N3 dynamic skin joint index is outside the skeleton");
        }
        if (!std::isfinite(weight) || weight < 0.0F) {
            throw std::runtime_error("N3 dynamic skin contains an invalid weight");
        }
        const auto transformed = skinMatrices[static_cast<std::size_t>(jointIndex)].transformPoint(bindPosition);
        result.x += transformed.x * weight;
        result.y += transformed.y * weight;
        result.z += transformed.z * weight;
        totalWeight += weight;
    }

    if (totalWeight <= 0.00001F) return bindPosition;
    if (std::fabs(totalWeight - 1.0F) > 0.0001F) {
        result.x /= totalWeight;
        result.y /= totalWeight;
        result.z /= totalWeight;
    }
    return result;
}

bool N3CharacterModel::load(const content::N3Character& character, std::size_t preferredLod) {
    unload();
    error_.clear();

    try {
        if (character.jointPath.empty()) {
            throw std::runtime_error("N3 character is missing its skeleton reference");
        }
        skeleton_ = content::N3Skeleton::load(character.jointPath);
        if (skeleton_.joints().empty()) {
            throw std::runtime_error("N3 character skeleton contains no joints");
        }

        Bounds bounds;
        std::size_t selectedParts = 0;
        for (const auto& part : character.parts) {
            const auto* lod = selectLod(part, preferredLod);
            if (lod == nullptr) continue;
            if (lod->influences.size() != lod->bindPositions.size()) {
                throw std::runtime_error("N3 skin influence count does not match bind-pose vertices");
            }
            ++selectedParts;
            for (const auto& point : lod->bindPositions) bounds.include(point);
            for (const auto& influence : lod->influences) {
                if (influence.jointIndices.size() != influence.weights.size()) {
                    throw std::runtime_error("N3 skin joint and weight arrays do not match");
                }
                for (const std::int32_t jointIndex : influence.jointIndices) {
                    if (jointIndex < 0 || static_cast<std::size_t>(jointIndex) >= skeleton_.joints().size()) {
                        throw std::runtime_error("N3 skin references a joint outside the skeleton");
                    }
                }
            }
        }
        if (selectedParts == 0 || bounds.maximumY <= bounds.minimumY) {
            throw std::runtime_error("N3 character has no valid bind-pose geometry");
        }

        centerX_ = (bounds.minimumX + bounds.maximumX) * 0.5F;
        centerZ_ = (bounds.minimumZ + bounds.maximumZ) * 0.5F;
        minimumY_ = bounds.minimumY;
        sourceHeight_ = bounds.maximumY - bounds.minimumY;
        parts_.reserve(selectedParts);
        const auto initialSkinMatrices = skeleton_.skinMatrices(0.0F);

        for (const auto& part : character.parts) {
            const auto* lod = selectLod(part, preferredLod);
            if (lod == nullptr || lod->faceIndices.empty()) continue;
            if (lod->faceIndices.size() % 3U != 0U || lod->uvIndices.size() != lod->faceIndices.size()) {
                throw std::runtime_error("N3 skin triangle and UV index buffers are inconsistent");
            }

            const std::size_t cornerCount = lod->faceIndices.size();
            if (cornerCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                throw std::runtime_error("N3 character mesh is too large for raylib");
            }

            PartRuntime runtimePart;
            runtimePart.bindPositions = lod->bindPositions;
            runtimePart.influences = lod->influences;
            runtimePart.cornerSourceIndices.reserve(cornerCount);
            runtimePart.gpuPositions.resize(cornerCount * 3U);

            Mesh mesh {};
            mesh.vertexCount = static_cast<int>(cornerCount);
            mesh.triangleCount = static_cast<int>(cornerCount / 3U);
            mesh.vertices = static_cast<float*>(MemAlloc(cornerCount * 3U * sizeof(float)));
            mesh.normals = static_cast<float*>(MemAlloc(cornerCount * 3U * sizeof(float)));
            mesh.texcoords = static_cast<float*>(MemAlloc(cornerCount * 2U * sizeof(float)));
            if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.texcoords == nullptr) {
                releaseCpuMesh(mesh);
                throw std::runtime_error("Unable to allocate N3 character mesh");
            }

            const std::array<std::size_t, 3> winding {0U, 2U, 1U};
            std::size_t output = 0;
            bool valid = true;
            for (std::size_t triangle = 0; triangle < cornerCount && valid; triangle += 3U) {
                for (const std::size_t corner : winding) {
                    const std::size_t sourceCorner = triangle + corner;
                    const std::uint16_t vertexIndex = lod->faceIndices[sourceCorner];
                    const std::uint16_t uvIndex = lod->uvIndices[sourceCorner];
                    if (vertexIndex >= lod->bindPositions.size() || vertexIndex >= lod->normals.size()) {
                        valid = false;
                        break;
                    }

                    runtimePart.cornerSourceIndices.push_back(vertexIndex);
                    const auto point = skinVertex(runtimePart, vertexIndex, initialSkinMatrices);
                    const auto& normal = lod->normals[vertexIndex];
                    const content::N3Vector2 uv = uvIndex < lod->uvs.size() ? lod->uvs[uvIndex] : content::N3Vector2 {};
                    runtimePart.gpuPositions[output * 3U + 0U] = -point.x - centerX_;
                    runtimePart.gpuPositions[output * 3U + 1U] = point.y - minimumY_;
                    runtimePart.gpuPositions[output * 3U + 2U] = point.z - centerZ_;
                    mesh.vertices[output * 3U + 0U] = runtimePart.gpuPositions[output * 3U + 0U];
                    mesh.vertices[output * 3U + 1U] = runtimePart.gpuPositions[output * 3U + 1U];
                    mesh.vertices[output * 3U + 2U] = runtimePart.gpuPositions[output * 3U + 2U];
                    mesh.normals[output * 3U + 0U] = -normal.x;
                    mesh.normals[output * 3U + 1U] = normal.y;
                    mesh.normals[output * 3U + 2U] = normal.z;
                    mesh.texcoords[output * 2U + 0U] = uv.u;
                    mesh.texcoords[output * 2U + 1U] = uv.v;
                    ++output;
                }
            }
            if (!valid) {
                releaseCpuMesh(mesh);
                throw std::runtime_error("N3 character face index exceeds bind-pose data");
            }

            UploadMesh(&mesh, true);
            runtimePart.model = LoadModelFromMesh(mesh);
            runtimePart.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {
                channel(part.diffuse[0]), channel(part.diffuse[1]), channel(part.diffuse[2]), channel(part.diffuse[3])
            };

            if (!part.texturePath.empty()) {
                try {
                    auto textureData = content::N3TextureLoader::load(part.texturePath);
                    Image image {};
                    image.data = textureData.rgba.data();
                    image.width = textureData.width;
                    image.height = textureData.height;
                    image.mipmaps = 1;
                    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    runtimePart.texture = LoadTextureFromImage(image);
                    if (runtimePart.texture.id == 0U) throw std::runtime_error("GPU texture creation returned id 0");
                    SetTextureFilter(runtimePart.texture, TEXTURE_FILTER_BILINEAR);
                    runtimePart.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = runtimePart.texture;
                } catch (...) {
                    UnloadModel(runtimePart.model);
                    if (runtimePart.texture.id != 0U) UnloadTexture(runtimePart.texture);
                    throw;
                }
            }

            parts_.push_back(std::move(runtimePart));
        }

        if (parts_.empty()) throw std::runtime_error("No N3 character parts could be converted to render meshes");
        animated_ = skeleton_.maximumFrame() > 0.0F;
        ready_ = true;
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
        unload();
        return false;
    }
}

bool N3CharacterModel::updateAnimation(float frame) noexcept {
    if (!ready_ || !animated_) return ready_;
    try {
        const auto skinMatrices = skeleton_.skinMatrices(frame);
        for (PartRuntime& part : parts_) {
            if (part.cornerSourceIndices.size() * 3U != part.gpuPositions.size()) {
                throw std::runtime_error("N3 dynamic mesh corner mapping is inconsistent");
            }
            for (std::size_t corner = 0; corner < part.cornerSourceIndices.size(); ++corner) {
                const auto point = skinVertex(part, part.cornerSourceIndices[corner], skinMatrices);
                part.gpuPositions[corner * 3U + 0U] = -point.x - centerX_;
                part.gpuPositions[corner * 3U + 1U] = point.y - minimumY_;
                part.gpuPositions[corner * 3U + 2U] = point.z - centerZ_;
            }
            UpdateMeshBuffer(part.model.meshes[0], 0,
                             part.gpuPositions.data(),
                             static_cast<int>(part.gpuPositions.size() * sizeof(float)), 0);
        }
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
        animated_ = false;
        return false;
    }
}

std::size_t N3CharacterModel::textureCount() const noexcept {
    std::size_t count = 0;
    for (const PartRuntime& part : parts_) if (part.texture.id != 0U) ++count;
    return count;
}

void N3CharacterModel::unload() noexcept {
    for (PartRuntime& part : parts_) {
        UnloadModel(part.model);
        if (part.texture.id != 0U) UnloadTexture(part.texture);
    }
    parts_.clear();
    skeleton_ = {};
    ready_ = false;
    animated_ = false;
    sourceHeight_ = 0.0F;
    centerX_ = 0.0F;
    centerZ_ = 0.0F;
    minimumY_ = 0.0F;
}

void N3CharacterModel::draw(Vector3 worldPosition, float targetHeight, Color tint) const {
    if (!ready_ || sourceHeight_ <= 0.0F) return;
    const float scale = targetHeight / sourceHeight_;
    for (const PartRuntime& part : parts_) DrawModel(part.model, worldPosition, scale, tint);
}

} // namespace korework::client
