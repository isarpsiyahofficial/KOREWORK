#include "client/n3_equipment_model.hpp"

#include "content/n3_texture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace korework::client {
namespace {

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

} // namespace

N3EquipmentModel::~N3EquipmentModel() { unload(); }

content::N3Vector3 N3EquipmentModel::transformVertex(
    const content::N3Vector3& source,
    const content::N3Matrix4& jointWorld) const noexcept {
    content::N3Vector3 scaled {
        source.x * plugScale_.x,
        source.y * plugScale_.y,
        source.z * plugScale_.z
    };
    content::N3Vector3 local = plugRotation_.transformPoint(scaled);
    local.x += plugPosition_.x;
    local.y += plugPosition_.y;
    local.z += plugPosition_.z;
    return jointWorld.transformPoint(local);
}

bool N3EquipmentModel::load(const content::N3EquipmentPlug& plug) {
    unload();
    error_.clear();

    try {
        if (plug.jointIndex < 0) throw std::runtime_error("N3 equipment has no attachment joint");
        if (plug.mesh.vertices.empty() || plug.mesh.indices.empty() || plug.mesh.indices.size() % 3U != 0U) {
            throw std::runtime_error("N3 equipment has no valid triangle geometry");
        }
        if (plug.mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("N3 equipment mesh is too large for raylib");
        }

        jointIndex_ = plug.jointIndex;
        plugPosition_ = plug.position;
        plugScale_ = plug.scale;
        plugRotation_ = plug.rotation;
        sourcePositions_.reserve(plug.mesh.vertices.size());
        for (const auto& vertex : plug.mesh.vertices) sourcePositions_.push_back(vertex.position);

        const std::size_t cornerCount = plug.mesh.indices.size();
        cornerSourceIndices_.reserve(cornerCount);
        gpuPositions_.resize(cornerCount * 3U);

        Mesh mesh {};
        mesh.vertexCount = static_cast<int>(cornerCount);
        mesh.triangleCount = static_cast<int>(cornerCount / 3U);
        mesh.vertices = static_cast<float*>(MemAlloc(cornerCount * 3U * sizeof(float)));
        mesh.normals = static_cast<float*>(MemAlloc(cornerCount * 3U * sizeof(float)));
        mesh.texcoords = static_cast<float*>(MemAlloc(cornerCount * 2U * sizeof(float)));
        if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.texcoords == nullptr) {
            releaseCpuMesh(mesh);
            throw std::runtime_error("Unable to allocate N3 equipment mesh");
        }

        const content::N3Matrix4 identity;
        const std::array<std::size_t, 3> winding {0U, 2U, 1U};
        std::size_t output = 0;
        for (std::size_t triangle = 0; triangle < cornerCount; triangle += 3U) {
            for (const std::size_t corner : winding) {
                const std::uint16_t vertexIndex = plug.mesh.indices[triangle + corner];
                if (vertexIndex >= plug.mesh.vertices.size()) {
                    releaseCpuMesh(mesh);
                    throw std::runtime_error("N3 equipment index exceeds vertex count");
                }
                cornerSourceIndices_.push_back(vertexIndex);
                const auto& source = plug.mesh.vertices[vertexIndex];
                const auto point = transformVertex(source.position, identity);
                gpuPositions_[output * 3U + 0U] = -point.x;
                gpuPositions_[output * 3U + 1U] = point.y;
                gpuPositions_[output * 3U + 2U] = point.z;
                mesh.vertices[output * 3U + 0U] = gpuPositions_[output * 3U + 0U];
                mesh.vertices[output * 3U + 1U] = gpuPositions_[output * 3U + 1U];
                mesh.vertices[output * 3U + 2U] = gpuPositions_[output * 3U + 2U];
                mesh.normals[output * 3U + 0U] = -source.normal.x;
                mesh.normals[output * 3U + 1U] = source.normal.y;
                mesh.normals[output * 3U + 2U] = source.normal.z;
                mesh.texcoords[output * 2U + 0U] = source.uv.u;
                mesh.texcoords[output * 2U + 1U] = source.uv.v;
                ++output;
            }
        }

        UploadMesh(&mesh, true);
        model_ = LoadModelFromMesh(mesh);
        model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {
            channel(plug.diffuse[0]), channel(plug.diffuse[1]), channel(plug.diffuse[2]), channel(plug.diffuse[3])
        };

        if (!plug.texturePath.empty()) {
            auto textureData = content::N3TextureLoader::load(plug.texturePath);
            Image image {};
            image.data = textureData.rgba.data();
            image.width = textureData.width;
            image.height = textureData.height;
            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            texture_ = LoadTextureFromImage(image);
            if (texture_.id == 0U) throw std::runtime_error("GPU equipment texture creation returned id 0");
            SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
            model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture_;
        }

        ready_ = true;
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
        unload();
        return false;
    }
}

bool N3EquipmentModel::update(const content::N3Matrix4& jointWorld,
                              float characterCenterX,
                              float characterCenterZ,
                              float characterMinimumY) noexcept {
    if (!ready_) return false;
    try {
        if (cornerSourceIndices_.size() * 3U != gpuPositions_.size()) {
            throw std::runtime_error("N3 equipment dynamic corner mapping is inconsistent");
        }
        for (std::size_t corner = 0; corner < cornerSourceIndices_.size(); ++corner) {
            const std::size_t sourceIndex = cornerSourceIndices_[corner];
            if (sourceIndex >= sourcePositions_.size()) throw std::runtime_error("N3 equipment source index is invalid");
            const auto point = transformVertex(sourcePositions_[sourceIndex], jointWorld);
            gpuPositions_[corner * 3U + 0U] = -point.x - characterCenterX;
            gpuPositions_[corner * 3U + 1U] = point.y - characterMinimumY;
            gpuPositions_[corner * 3U + 2U] = point.z - characterCenterZ;
        }
        UpdateMeshBuffer(model_.meshes[0], 0, gpuPositions_.data(),
                         static_cast<int>(gpuPositions_.size() * sizeof(float)), 0);
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
        return false;
    }
}

void N3EquipmentModel::draw(Vector3 worldPosition,
                            float targetCharacterHeight,
                            float sourceCharacterHeight,
                            Color tint,
                            float yawDegrees) const {
    if (!ready_ || sourceCharacterHeight <= 0.0F) return;
    const float scale = targetCharacterHeight / sourceCharacterHeight;
    DrawModelEx(model_, worldPosition, {0.0F, 1.0F, 0.0F}, yawDegrees,
                {scale, scale, scale}, tint);
}

void N3EquipmentModel::unload() noexcept {
    if (model_.meshCount > 0) UnloadModel(model_);
    if (texture_.id != 0U) UnloadTexture(texture_);
    model_ = {};
    texture_ = {};
    sourcePositions_.clear();
    cornerSourceIndices_.clear();
    gpuPositions_.clear();
    plugPosition_ = {};
    plugScale_ = {1.0F, 1.0F, 1.0F};
    plugRotation_ = {};
    jointIndex_ = -1;
    ready_ = false;
}

} // namespace korework::client
