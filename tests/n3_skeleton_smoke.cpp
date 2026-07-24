#include "content/n3_skeleton.hpp"

#include <array>
#include <cassert>
#include <cmath>
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

bool near(float left, float right) {
    return std::fabs(left - right) < 0.0001F;
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "korework_joint_smoke.n3joint";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        assert(output);
        writeString(output, "Root");
        for (float value : std::array<float, 3> {1.0F, 2.0F, 3.0F}) writeValue(output, value);
        for (float value : std::array<float, 4> {0.0F, 0.0F, 0.0F, 1.0F}) writeValue(output, value);
        for (float value : std::array<float, 3> {1.0F, 1.0F, 1.0F}) writeValue(output, value);
        // Position, rotation, scale and orientation animation channels: no keys.
        for (int channel = 0; channel < 4; ++channel) writeValue(output, static_cast<std::int32_t>(0));
        writeValue(output, static_cast<std::int32_t>(0)); // child count
    }

    const auto skeleton = korework::content::N3Skeleton::load(path);
    assert(skeleton.joints().size() == 1U);
    assert(skeleton.joints().front().name == "Root");
    assert(skeleton.joints().front().parentIndex == -1);

    const auto world = skeleton.worldMatrices(0.0F);
    assert(world.size() == 1U);
    const auto transformed = world.front().transformPoint({0.0F, 0.0F, 0.0F});
    assert(near(transformed.x, 1.0F));
    assert(near(transformed.y, 2.0F));
    assert(near(transformed.z, 3.0F));

    const auto skin = skeleton.skinMatrices(0.0F);
    assert(skin.size() == 1U);
    const auto bindPoint = skin.front().transformPoint({4.0F, 5.0F, 6.0F});
    assert(near(bindPoint.x, 4.0F));
    assert(near(bindPoint.y, 5.0F));
    assert(near(bindPoint.z, 6.0F));

    std::filesystem::remove(path);
    return 0;
}
