#include "content/smd_map.hpp"

#include <algorithm>
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

void writeFixedString(std::ofstream& stream, const std::string& value, std::size_t width) {
    std::string buffer(width, '\0');
    value.copy(buffer.data(), std::min(width, value.size()));
    stream.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "korework_smd_smoke.smd";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        assert(output);

        const std::int32_t mapSize = 3;
        const float unitDistance = 4.0F;
        writeValue(output, mapSize);
        writeValue(output, unitDistance);
        const std::array<float, 9> heights {0.0F, 1.0F, 2.0F, 1.0F, 2.0F, 3.0F, 2.0F, 3.0F, 4.0F};
        output.write(reinterpret_cast<const char*>(heights.data()), static_cast<std::streamsize>(sizeof(heights)));

        const float width = 8.0F;
        const float length = 8.0F;
        const std::int32_t collisionFaces = 0;
        writeValue(output, width);
        writeValue(output, length);
        writeValue(output, collisionFaces);

        const std::uint32_t cellExists = 0;
        writeValue(output, cellExists);

        const std::int32_t objectEvents = 0;
        writeValue(output, objectEvents);
        const std::array<std::int16_t, 9> eventGrid {};
        output.write(reinterpret_cast<const char*>(eventGrid.data()), static_cast<std::streamsize>(sizeof(eventGrid)));

        const std::int32_t regeneCount = 1;
        writeValue(output, regeneCount);
        for (const float value : std::array<float, 5> {4.0F, 0.0F, 4.0F, 2.0F, 2.0F}) {
            writeValue(output, value);
        }

        const std::int32_t warpCount = 1;
        writeValue(output, warpCount);
        writeValue(output, static_cast<std::int16_t>(11));
        writeFixedString(output, "Test Warp", 32);
        writeFixedString(output, "Synthetic warp for parser validation", 256);
        writeValue(output, static_cast<std::uint16_t>(0));
        writeValue(output, static_cast<std::uint32_t>(100));
        writeValue(output, static_cast<std::int16_t>(21));
        writeValue(output, static_cast<std::uint16_t>(0));
        for (const float value : std::array<float, 4> {6.0F, 0.0F, 6.0F, 1.5F}) {
            writeValue(output, value);
        }
        writeValue(output, static_cast<std::int16_t>(0));
        writeValue(output, static_cast<std::uint16_t>(0));
    }

    korework::content::SmdMap map;
    assert(map.load(path));
    assert(map.loaded());
    assert(map.mapSize() == 3);
    assert(map.heights().size() == 9);
    assert(map.collisionTriangles().empty());
    assert(map.collisionCells().size() == 1);
    assert(map.regeneEvents().size() == 1);
    assert(map.regeneEvents().front().point == 0);
    assert(map.warps().size() == 1);
    assert(map.warps().front().warpId == 11);
    assert(map.warps().front().name == "Test Warp");
    assert(map.contains(4.0F, 4.0F));
    assert(map.heightAt(4.0F, 4.0F) == 2.0F);

    std::filesystem::remove(path);
    return 0;
}
