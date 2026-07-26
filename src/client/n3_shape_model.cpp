#include "client/n3_shape_model.hpp"

#include "content/n3_texture.hpp"

#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace korework::client {
namespace {

unsigned char channel(float value) {
    return static_cast<unsigned char>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

content::N3Vector3 rotate(const content::N3Quaternion& input, const content::N3Vector3& point) {
    float length = std::sqrt(input.x * input.x + input.y * input.y + input.z * input.z + input.w * input.w);
    content::N3Quaternion q = input;
    if (length > 0.000001F) {
        q.x /= length; q.y /= length; q.z /= length; q.w /= length;
    } else {
        q = {};
    }
    const content::N3Vector3 u {q.x, q.y, q.z};
    const float dotUV = u.x * point.x + u.y * point.y + u.z * point.z;
    const float dotUU = u.x * u.x + u.y * u.y + u.z * u.z;
    const content::N3Vector3 cross {
        u.y * point.z - u.z * point.y,
        u.z * point.x - u.x * point.z,
        u.x * point.y - u.y * point.x
    };
    return {
        2.0F * dotUV * u.x + (q.w * q.w - dotUU) * point.x + 2.0F * q.w * cross.x,
        2.0F * dotUV * u.y + (q.w * q.w - dotUU) * point.y + 2.0F * q.w * cross.y,
        2.0F * dotUV * u.z + (q.w * q.w - dotUU) * point.z + 2.0F * q.w * cross.z
    };
}

content::N3Vector3 transformPoint(const content::N3Shape& shape,
                                  const content::N3ShapePart& part,
                                  const content::N3Vector3& source) {
    content::N3Vector3 local {source.x + part.pivot.x, source.y + part.pivot.y, source.z + part.pivot.z};
    local.x *= shape.scale.x;
    local.y *= shape.scale.y;
    local.z *= shape.scale.z;
    local = rotate(shape.rotation, local);
    local.x += shape.position.x;
    local.y += shape.position.y;
    local.z += shape.position.z;
    return local;
}

content::N3Vector3 transformNormal(const content::N3Shape& shape, const content::N3Vector3& source) {
    content::N3Vector3 normal = rotate(shape.rotation, source);
    const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length > 0.000001F) {
        normal.x /= length; normal.y /= length; normal.z /= length;
    }
    return normal;
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

N3ShapeModel::~N3ShapeModel() { unload(); }

bool N3ShapeModel::load(const content::N3Shape& shape) {
    unload();
    error_.clear();
    try {
        float minimumY = std::numeric_limits<float>::max();
        float maximumY = -std::numeric_limits<float>::max();
        for (const auto& sourcePart : shape.parts) {
            if (sourcePart.mesh.indices.empty() || sourcePart.mesh.indices.size() % 3U != 0U) {
                throw std::runtime_error("N3 shape part has invalid triangle geometry");
            }
            const std::size_t cornerCount = sourcePart.mesh.indices.size();
            if (cornerCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                throw std::runtime_error("N3 shape part is too large for raylib");
            }

            Mesh mesh {};
            mesh.vertexCount = static_cast<int>(cornerCount);
            mesh.triangleCount = static_cast<int>(cornerCount / 3U);
            mesh.vertices = static_cast<float*>(MemAlloc(cornerCount * 3U * sizeof(float)));
            mesh.normals = static_cast<float*>(MemAlloc(cornerCount * 3U * sizeof(float)));
            mesh.texcoords = static_cast<float*>(MemAlloc(cornerCount * 2U * sizeof(float)));
            if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.texcoords == nullptr) {
                releaseCpuMesh(mesh);
                throw std::runtime_error("Unable to allocate N3 shape GPU mesh");
            }

            const std::array<std::size_t, 3> winding {0U, 2U, 1U};
            std::size_t output = 0U;
            for (std::size_t triangle = 0U; triangle < cornerCount; triangle += 3U) {
                for (const std::size_t corner : winding) {
                    const std::uint16_t vertexIndex = sourcePart.mesh.indices[triangle + corner];
                    if (vertexIndex >= sourcePart.mesh.vertices.size()) {
                        releaseCpuMesh(mesh);
                        throw std::runtime_error("N3 shape index exceeds vertex count");
                    }
                    const auto& source = sourcePart.mesh.vertices[vertexIndex];
                    const auto point = transformPoint(shape, sourcePart, source.position);
                    const auto normal = transformNormal(shape, source.normal);
                    mesh.vertices[output * 3U + 0U] = -point.x;
                    mesh.vertices[output * 3U + 1U] = point.y;
                    mesh.vertices[output * 3U + 2U] = point.z;
                    mesh.normals[output * 3U + 0U] = -normal.x;
                    mesh.normals[output * 3U + 1U] = normal.y;
                    mesh.normals[output * 3U + 2U] = normal.z;
                    mesh.texcoords[output * 2U + 0U] = source.uv.u;
                    mesh.texcoords[output * 2U + 1U] = source.uv.v;
                    minimumY = std::min(minimumY, point.y);
                    maximumY = std::max(maximumY, point.y);
                    ++output;
                }
            }

            UploadMesh(&mesh, false);
            Part part;
            part.renderFlags = sourcePart.renderFlags;
            part.model = LoadModelFromMesh(mesh);
            part.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {
                channel(sourcePart.diffuse[0]), channel(sourcePart.diffuse[1]),
                channel(sourcePart.diffuse[2]), channel(sourcePart.diffuse[3])
            };
            if (!sourcePart.texturePaths.empty()) {
                const auto textureData = content::N3TextureLoader::load(sourcePart.texturePaths.front());
                Image image {};
                image.data = const_cast<std::uint8_t*>(textureData.rgba.data());
                image.width = textureData.width;
                image.height = textureData.height;
                image.mipmaps = 1;
                image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                part.texture = LoadTextureFromImage(image);
                if (part.texture.id == 0U) throw std::runtime_error("Unable to create N3 shape texture");
                SetTextureFilter(part.texture, TEXTURE_FILTER_BILINEAR);
                part.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = part.texture;
            }
            parts_.push_back(part);
        }
        sourceHeight_ = std::max(0.001F, maximumY - minimumY);
        return !parts_.empty();
    } catch (const std::exception& exception) {
        error_ = exception.what();
        unload();
        return false;
    }
}

void N3ShapeModel::unload() noexcept {
    for (auto& part : parts_) {
        if (part.model.meshCount > 0) UnloadModel(part.model);
        if (part.texture.id != 0U) UnloadTexture(part.texture);
    }
    parts_.clear();
    sourceHeight_ = 1.0F;
}

void N3ShapeModel::draw(Vector3 worldPosition, float scale, Color tint) const {
    constexpr std::uint32_t AlphaBlending = 0x1U;
    constexpr std::uint32_t DoubleSided = 0x4U;
    for (const auto& part : parts_) {
        const bool doubleSided = (part.renderFlags & DoubleSided) != 0U;
        const bool alpha = (part.renderFlags & AlphaBlending) != 0U;
        if (doubleSided) rlDisableBackfaceCulling();
        if (alpha) BeginBlendMode(BLEND_ALPHA);
        DrawModel(part.model, worldPosition, scale, tint);
        if (alpha) EndBlendMode();
        if (doubleSided) rlEnableBackfaceCulling();
    }
}

} // namespace korework::client
