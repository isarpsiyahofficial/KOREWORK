#include "client/n3_character_model.hpp"

#include "content/n3_texture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

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

bool N3CharacterModel::load(const content::N3Character& character, std::size_t preferredLod) {
    unload();
    error_.clear();

    try {
        Bounds bounds;
        std::size_t selectedParts = 0;
        for (const auto& part : character.parts) {
            const auto* lod = selectLod(part, preferredLod);
            if (lod == nullptr) continue;
            ++selectedParts;
            for (const auto& point : lod->bindPositions) bounds.include(point);
        }
        if (selectedParts == 0 || bounds.maximumY <= bounds.minimumY) {
            throw std::runtime_error("N3 character has no valid bind-pose geometry");
        }

        const float centerX = (bounds.minimumX + bounds.maximumX) * 0.5F;
        const float centerZ = (bounds.minimumZ + bounds.maximumZ) * 0.5F;
        sourceHeight_ = bounds.maximumY - bounds.minimumY;
        models_.reserve(selectedParts);
        textures_.reserve(selectedParts);

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

                    const auto& point = lod->bindPositions[vertexIndex];
                    const auto& normal = lod->normals[vertexIndex];
                    const content::N3Vector2 uv = uvIndex < lod->uvs.size() ? lod->uvs[uvIndex] : content::N3Vector2 {};
                    mesh.vertices[output * 3U + 0U] = -point.x - centerX;
                    mesh.vertices[output * 3U + 1U] = point.y - bounds.minimumY;
                    mesh.vertices[output * 3U + 2U] = point.z - centerZ;
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

            UploadMesh(&mesh, false);
            Model model = LoadModelFromMesh(mesh);
            model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {
                channel(part.diffuse[0]), channel(part.diffuse[1]), channel(part.diffuse[2]), channel(part.diffuse[3])
            };

            Texture2D gpuTexture {};
            if (!part.texturePath.empty()) {
                try {
                    auto textureData = content::N3TextureLoader::load(part.texturePath);
                    Image image {};
                    image.data = textureData.rgba.data();
                    image.width = textureData.width;
                    image.height = textureData.height;
                    image.mipmaps = 1;
                    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    gpuTexture = LoadTextureFromImage(image);
                    if (gpuTexture.id == 0U) throw std::runtime_error("GPU texture creation returned id 0");
                    SetTextureFilter(gpuTexture, TEXTURE_FILTER_BILINEAR);
                    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gpuTexture;
                } catch (...) {
                    UnloadModel(model);
                    throw;
                }
            }

            models_.push_back(model);
            if (gpuTexture.id != 0U) textures_.push_back(gpuTexture);
        }

        if (models_.empty()) throw std::runtime_error("No N3 character parts could be converted to render meshes");
        ready_ = true;
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
        unload();
        return false;
    }
}

void N3CharacterModel::unload() noexcept {
    for (Model& model : models_) UnloadModel(model);
    models_.clear();
    for (Texture2D texture : textures_) if (texture.id != 0U) UnloadTexture(texture);
    textures_.clear();
    ready_ = false;
    sourceHeight_ = 0.0F;
}

void N3CharacterModel::draw(Vector3 worldPosition, float targetHeight, Color tint) const {
    if (!ready_ || sourceHeight_ <= 0.0F) return;
    const float scale = targetHeight / sourceHeight_;
    for (const Model& model : models_) DrawModel(model, worldPosition, scale, tint);
}

} // namespace korework::client
