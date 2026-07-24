#include "content/n3_texture.hpp"

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

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "korework_ntf_smoke.dxt";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        assert(output);
        const std::string name = "Test";
        writeValue(output, static_cast<std::int32_t>(name.size()));
        output.write(name.data(), static_cast<std::streamsize>(name.size()));
        const std::array<char, 4> identifier {'N', 'T', 'F', 3};
        output.write(identifier.data(), static_cast<std::streamsize>(identifier.size()));
        writeValue(output, static_cast<std::int32_t>(4));
        writeValue(output, static_cast<std::int32_t>(4));
        writeValue(output, static_cast<std::uint32_t>(0x31545844U));
        writeValue(output, static_cast<std::int32_t>(0));

        // One DXT1 block. color0=red, color1=green and every selector chooses color0.
        writeValue(output, static_cast<std::uint16_t>(0xF800U));
        writeValue(output, static_cast<std::uint16_t>(0x07E0U));
        writeValue(output, static_cast<std::uint32_t>(0U));
    }

    const auto texture = korework::content::N3TextureLoader::load(path);
    assert(texture.name == "Test");
    assert(texture.width == 4);
    assert(texture.height == 4);
    assert(texture.rgba.size() == 64U);
    for (std::size_t pixel = 0; pixel < 16U; ++pixel) {
        assert(texture.rgba[pixel * 4U + 0U] == 255U);
        assert(texture.rgba[pixel * 4U + 1U] == 0U);
        assert(texture.rgba[pixel * 4U + 2U] == 0U);
        assert(texture.rgba[pixel * 4U + 3U] == 255U);
    }

    std::filesystem::remove(path);
    return 0;
}
