#include "content/n3_animation.hpp"

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
    const auto path = std::filesystem::temp_directory_path() / "korework_anim_smoke.n3anim";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        assert(output);
        writeValue(output, static_cast<std::int32_t>(2));

        writeValue(output, static_cast<std::int32_t>(0));
        for (float value : {0.0F, 60.0F, 30.0F, 0.0F, 0.0F, 12.0F, 40.0F, 0.2F}) writeValue(output, value);
        writeValue(output, static_cast<std::int32_t>(0));
        writeValue(output, 0.0F);
        writeValue(output, 0.0F);
        writeString(output, "Idle_Breath");

        writeValue(output, static_cast<std::int32_t>(0));
        for (float value : {61.0F, 91.0F, 30.0F, 64.0F, 85.0F, 0.0F, 0.0F, 0.15F}) writeValue(output, value);
        writeValue(output, static_cast<std::int32_t>(1));
        writeValue(output, 72.0F);
        writeValue(output, 76.0F);
        writeString(output, "Attack_Strike");
    }

    const auto animations = korework::content::N3AnimationSet::load(path);
    assert(animations.clips().size() == 2U);
    assert(animations.find("idle_breath") != nullptr);
    assert(animations.preferredIdle() != nullptr);
    assert(animations.preferredIdle()->name == "Idle_Breath");
    assert(animations.preferredAttack() != nullptr);
    assert(animations.preferredAttack()->name == "Attack_Strike");
    assert(near(animations.preferredIdle()->durationSeconds(), 2.0F));
    assert(animations.preferredAttack()->contains(72.0F));

    std::filesystem::remove(path);
    return 0;
}
