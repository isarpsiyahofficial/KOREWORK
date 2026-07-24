#include "client/smd_terrain.hpp"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace korework::client {
namespace {

std::vector<std::int32_t> makeSamples(std::int32_t mapSize, std::size_t maximumSamples) {
    maximumSamples = std::clamp<std::size_t>(maximumSamples, 2, 255);
    const auto intervals = static_cast<std::size_t>(mapSize - 1);
    const auto stride = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(
        static_cast<double>(intervals) / static_cast<double>(maximumSamples - 1))));

    std::vector<std::int32_t> result;
    for (std::size_t index = 0; index < intervals; index += stride) {
        result.push_back(static_cast<std::int32_t>(index));
    }
    if (result.empty() || result.back() != mapSize - 1) {
        result.push_back(mapSize - 1);
    }
    return result;
}

Vector3 terrainNormal(const content::SmdMap& map, float x, float z) {
    const float step = map.unitDistance();
    const float left = map.heightAt(std::max(0.0F, x - step), z);
    const float right = map.heightAt(std::min(map.width(), x + step), z);
    const float down = map.heightAt(x, std::max(0.0F, z - step));
    const float up = map.heightAt(x, std::min(map.length(), z + step));
    return Vector3Normalize({left - right, step * 2.0F, down - up});
}

} // namespace

SmdTerrainModel::~SmdTerrainModel() {
    unload();
}

bool SmdTerrainModel::load(const content::SmdMap& map, std::size_t maximumSamplesPerAxis) {
    unload();
    error_.clear();

    if (!map.loaded() || map.mapSize() < 2) {
        error_ = "SMD map is not loaded";
        return false;
    }

    try {
        const auto sampleX = makeSamples(map.mapSize(), maximumSamplesPerAxis);
        const auto sampleZ = makeSamples(map.mapSize(), maximumSamplesPerAxis);
        const std::size_t vertexCount = sampleX.size() * sampleZ.size();
        if (vertexCount == 0 || vertexCount > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("SMD terrain sample count exceeds raylib index limits");
        }

        const std::size_t cellCount = (sampleX.size() - 1) * (sampleZ.size() - 1);
        const std::size_t indexCount = cellCount * 6U;

        Mesh mesh {};
        mesh.vertexCount = static_cast<int>(vertexCount);
        mesh.triangleCount = static_cast<int>(cellCount * 2U);
        mesh.vertices = static_cast<float*>(MemAlloc(vertexCount * 3U * sizeof(float)));
        mesh.normals = static_cast<float*>(MemAlloc(vertexCount * 3U * sizeof(float)));
        mesh.texcoords = static_cast<float*>(MemAlloc(vertexCount * 2U * sizeof(float)));
        mesh.indices = static_cast<unsigned short*>(MemAlloc(indexCount * sizeof(unsigned short)));
        if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.texcoords == nullptr || mesh.indices == nullptr) {
            if (mesh.vertices != nullptr) MemFree(mesh.vertices);
            if (mesh.normals != nullptr) MemFree(mesh.normals);
            if (mesh.texcoords != nullptr) MemFree(mesh.texcoords);
            if (mesh.indices != nullptr) MemFree(mesh.indices);
            throw std::runtime_error("Unable to allocate SMD terrain mesh");
        }

        const float halfWidth = map.width() * 0.5F;
        const float halfLength = map.length() * 0.5F;
        std::size_t vertex = 0;
        for (std::size_t zIndex = 0; zIndex < sampleZ.size(); ++zIndex) {
            const float mapZ = static_cast<float>(sampleZ[zIndex]) * map.unitDistance();
            for (std::size_t xIndex = 0; xIndex < sampleX.size(); ++xIndex) {
                const float mapX = static_cast<float>(sampleX[xIndex]) * map.unitDistance();
                const Vector3 normal = terrainNormal(map, mapX, mapZ);
                mesh.vertices[vertex * 3U + 0U] = mapX - halfWidth;
                mesh.vertices[vertex * 3U + 1U] = map.heightAt(mapX, mapZ);
                mesh.vertices[vertex * 3U + 2U] = mapZ - halfLength;
                mesh.normals[vertex * 3U + 0U] = normal.x;
                mesh.normals[vertex * 3U + 1U] = normal.y;
                mesh.normals[vertex * 3U + 2U] = normal.z;
                mesh.texcoords[vertex * 2U + 0U] = static_cast<float>(xIndex) / static_cast<float>(sampleX.size() - 1);
                mesh.texcoords[vertex * 2U + 1U] = static_cast<float>(zIndex) / static_cast<float>(sampleZ.size() - 1);
                ++vertex;
            }
        }

        std::size_t outputIndex = 0;
        for (std::size_t z = 0; z + 1 < sampleZ.size(); ++z) {
            for (std::size_t x = 0; x + 1 < sampleX.size(); ++x) {
                const auto topLeft = static_cast<unsigned short>(z * sampleX.size() + x);
                const auto topRight = static_cast<unsigned short>(topLeft + 1U);
                const auto bottomLeft = static_cast<unsigned short>((z + 1U) * sampleX.size() + x);
                const auto bottomRight = static_cast<unsigned short>(bottomLeft + 1U);
                mesh.indices[outputIndex++] = topLeft;
                mesh.indices[outputIndex++] = bottomLeft;
                mesh.indices[outputIndex++] = topRight;
                mesh.indices[outputIndex++] = topRight;
                mesh.indices[outputIndex++] = bottomLeft;
                mesh.indices[outputIndex++] = bottomRight;
            }
        }

        UploadMesh(&mesh, false);
        model_ = LoadModelFromMesh(mesh);
        model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = Color{128, 145, 92, 255};
        samplesX_ = sampleX.size();
        samplesZ_ = sampleZ.size();
        ready_ = true;
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
        unload();
        return false;
    }
}

void SmdTerrainModel::unload() noexcept {
    if (ready_) {
        UnloadModel(model_);
    }
    model_ = {};
    ready_ = false;
    samplesX_ = 0;
    samplesZ_ = 0;
}

void SmdTerrainModel::draw() const {
    if (!ready_) {
        return;
    }
    DrawModel(model_, {0.0F, 0.0F, 0.0F}, 1.0F, WHITE);
}

} // namespace korework::client
