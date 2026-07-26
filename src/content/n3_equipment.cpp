#include "content/n3_equipment.hpp"

#include "content/binary_reader.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace korework::content {
namespace {

constexpr std::int32_t kMaximumVertices = 4'000'000;
constexpr std::int32_t kMaximumIndices = 12'000'000;
constexpr std::int32_t kMaximumCollapses = 4'000'000;
constexpr std::int32_t kMaximumIndexChanges = 12'000'000;
constexpr std::int32_t kMaximumLodControls = 65'536;

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

std::filesystem::path resolveRequired(const KoAssetResolver& resolver,
                                      const std::filesystem::path& baseFile,
                                      const std::string& storedPath,
                                      const char* label) {
    if (storedPath.empty()) throw std::runtime_error(std::string("Missing ") + label + " path in " + baseFile.string());
    const auto path = resolver.resolve(baseFile, storedPath);
    if (!path.has_value()) {
        throw std::runtime_error(std::string("Unable to resolve ") + label + " '" + storedPath + "' from " + baseFile.string());
    }
    return *path;
}

} // namespace

N3EquipmentPlug N3EquipmentLoader::load(const std::filesystem::path& plugPath) const {
    BinaryReader reader(plugPath);
    N3EquipmentPlug plug;
    plug.sourcePath = plugPath;
    plug.name = reader.readString();
    plug.type = reader.read<std::int32_t>();
    plug.jointIndex = reader.read<std::int32_t>();
    plug.position = readVector3(reader);
    for (float& value : plug.rotation.value) {
        value = reader.read<float>();
        requireFinite(value, "plug rotation");
    }
    plug.scale = readVector3(reader);
    for (float& channel : plug.diffuse) {
        channel = reader.read<float>();
        requireFinite(channel, "plug material");
    }
    reader.skip(76U); // Remaining __Material fields.

    const std::string meshReference = reader.readString();
    const std::string textureReference = reader.readString();
    plug.meshPath = resolveRequired(resolver_, plugPath, meshReference, "plug mesh");
    if (!textureReference.empty()) plug.texturePath = resolveRequired(resolver_, plugPath, textureReference, "plug texture");

    if (reader.remaining() >= sizeof(std::int32_t)) {
        plug.traceStep = reader.read<std::int32_t>();
        if (plug.traceStep < 0 || plug.traceStep > 1'000'000) throw std::runtime_error("Invalid N3 equipment trace step");
        if (plug.traceStep > 0) reader.skip(12U);
    }
    if (reader.remaining() >= sizeof(std::int32_t)) {
        const std::int32_t useVirtualMesh = reader.read<std::int32_t>();
        if (useVirtualMesh != 0) throw std::runtime_error("N3 equipment VirtualMesh is not supported safely");
    }
    if (reader.remaining() != 0U) {
        throw std::runtime_error("Unexpected trailing N3 equipment bytes: " + std::to_string(reader.remaining()));
    }

    plug.mesh = loadProgressiveMesh(plug.meshPath);
    return plug;
}

N3ProgressiveMesh N3EquipmentLoader::loadProgressiveMesh(const std::filesystem::path& meshPath) const {
    BinaryReader reader(meshPath);
    N3ProgressiveMesh mesh;
    mesh.sourcePath = meshPath;
    mesh.name = reader.readString();

    const std::int32_t collapseCount = reader.read<std::int32_t>();
    const std::int32_t indexChangeCount = reader.read<std::int32_t>();
    const std::int32_t vertexCount = reader.read<std::int32_t>();
    const std::int32_t indexCount = reader.read<std::int32_t>();
    mesh.minimumVertices = reader.read<std::int32_t>();
    mesh.minimumIndices = reader.read<std::int32_t>();
    requireCount(collapseCount, kMaximumCollapses, "progressive collapse");
    requireCount(indexChangeCount, kMaximumIndexChanges, "progressive index change");
    requireCount(vertexCount, kMaximumVertices, "progressive vertex");
    requireCount(indexCount, kMaximumIndices, "progressive index");
    if (indexCount % 3 != 0 || mesh.minimumVertices < 0 || mesh.minimumVertices > vertexCount
        || mesh.minimumIndices < 0 || mesh.minimumIndices > indexCount) {
        throw std::runtime_error("Invalid N3 progressive mesh bounds");
    }

    mesh.vertices.reserve(static_cast<std::size_t>(vertexCount));
    for (std::int32_t index = 0; index < vertexCount; ++index) {
        N3MeshVertex vertex;
        vertex.position = readVector3(reader);
        vertex.normal = readVector3(reader);
        vertex.uv.u = reader.read<float>();
        vertex.uv.v = reader.read<float>();
        requireFinite(vertex.uv.u, "progressive UV");
        requireFinite(vertex.uv.v, "progressive UV");
        mesh.vertices.push_back(vertex);
    }

    mesh.indices = reader.readVector<std::uint16_t>(static_cast<std::size_t>(indexCount));
    for (const std::uint16_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) throw std::runtime_error("N3 progressive mesh index exceeds vertex count");
    }

    reader.skip(static_cast<std::uint64_t>(collapseCount) * 24U);
    reader.skip(static_cast<std::uint64_t>(indexChangeCount) * 4U);

    const std::int32_t lodControlCount = reader.read<std::int32_t>();
    requireCount(lodControlCount, kMaximumLodControls, "LOD control");
    for (std::int32_t index = 0; index < lodControlCount; ++index) {
        const float distance = reader.read<float>();
        const std::int32_t vertices = reader.read<std::int32_t>();
        requireFinite(distance, "LOD distance");
        if (distance < 0.0F || vertices < 0 || vertices > vertexCount) throw std::runtime_error("Invalid N3 LOD control value");
    }

    if (reader.remaining() != 0U) {
        throw std::runtime_error("Unexpected trailing N3 progressive mesh bytes: " + std::to_string(reader.remaining()));
    }
    if (mesh.vertices.empty() || mesh.indices.empty()) throw std::runtime_error("N3 progressive mesh contains no geometry");
    return mesh;
}

} // namespace korework::content
