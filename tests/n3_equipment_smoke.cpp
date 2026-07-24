#include "content/ko_asset_resolver.hpp"
#include "content/n3_equipment.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

template <typename T>
void writeValue(std::ofstream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void writeString(std::ofstream& stream, const std::string& value) {
    writeValue(stream, static_cast<std::int32_t>(value.size()));
    stream.write(value.data(), static_cast<std::streamsize>(value.size()));
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "korework_equipment_smoke";
    std::filesystem::create_directories(root);
    const auto meshPath = root / "test.n3pmesh";
    const auto plugPath = root / "test.n3cplug";

    {
        std::ofstream output(meshPath, std::ios::binary | std::ios::trunc);
        assert(output);
        writeString(output, "TriangleMesh");
        writeValue(output, static_cast<std::int32_t>(0)); // collapses
        writeValue(output, static_cast<std::int32_t>(0)); // index changes
        writeValue(output, static_cast<std::int32_t>(3)); // max vertices
        writeValue(output, static_cast<std::int32_t>(3)); // max indices
        writeValue(output, static_cast<std::int32_t>(3)); // min vertices
        writeValue(output, static_cast<std::int32_t>(3)); // min indices
        const std::array<std::array<float, 8>, 3> vertices {{
            {{0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F}},
            {{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F}},
            {{0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}}
        }};
        for (const auto& vertex : vertices) for (float value : vertex) writeValue(output, value);
        for (std::uint16_t index : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{2}}) writeValue(output, index);
        writeValue(output, static_cast<std::int32_t>(1));
        writeValue(output, 10.0F);
        writeValue(output, static_cast<std::int32_t>(3));
    }

    {
        std::ofstream output(plugPath, std::ios::binary | std::ios::trunc);
        assert(output);
        writeString(output, "TestWeapon");
        writeValue(output, static_cast<std::int32_t>(0));
        writeValue(output, static_cast<std::int32_t>(0));
        for (float value : std::array<float, 3> {0.0F, 0.0F, 0.0F}) writeValue(output, value);
        const std::array<float, 16> identity {
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F
        };
        for (float value : identity) writeValue(output, value);
        for (float value : std::array<float, 3> {1.0F, 1.0F, 1.0F}) writeValue(output, value);
        for (float value : std::array<float, 4> {1.0F, 1.0F, 1.0F, 1.0F}) writeValue(output, value);
        const std::array<char, 76> materialTail {};
        output.write(materialTail.data(), static_cast<std::streamsize>(materialTail.size()));
        writeString(output, "test.n3pmesh");
        writeString(output, "");
        writeValue(output, static_cast<std::int32_t>(0));
        writeValue(output, static_cast<std::int32_t>(0));
    }

    const korework::content::KoAssetResolver resolver(root);
    const korework::content::N3EquipmentLoader loader(resolver);
    const auto plug = loader.load(plugPath);
    assert(plug.name == "TestWeapon");
    assert(plug.jointIndex == 0);
    assert(plug.mesh.vertices.size() == 3U);
    assert(plug.mesh.indices.size() == 3U);
    assert(plug.mesh.indices[2] == 2U);

    std::filesystem::remove_all(root);
    return 0;
}
