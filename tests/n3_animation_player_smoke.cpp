#include "client/n3_animation_player.hpp"
#include "content/n3_animation.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

template <typename T>
void write(std::ofstream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void writeString(std::ofstream& stream, const std::string& value) {
    const std::int32_t length = static_cast<std::int32_t>(value.size());
    write(stream, length);
    stream.write(value.data(), length);
}

void writeClip(std::ofstream& stream, const std::string& name, float start, float end, float fps) {
    write(stream, std::int32_t{0});
    write(stream, start);
    write(stream, end);
    write(stream, fps);
    write(stream, start);
    write(stream, end);
    write(stream, float{0.0F});
    write(stream, float{0.0F});
    write(stream, float{0.2F});
    write(stream, std::int32_t{0});
    write(stream, start + 1.0F);
    write(stream, start + 2.0F);
    writeString(stream, name);
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "korework_animation_player_smoke.n3anim";
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        assert(stream);
        write(stream, std::int32_t{3});
        writeClip(stream, "idle", 0.0F, 30.0F, 30.0F);
        writeClip(stream, "run", 40.0F, 70.0F, 30.0F);
        writeClip(stream, "attack", 80.0F, 95.0F, 30.0F);
    }

    const auto animations = korework::content::N3AnimationSet::load(path);
    korework::client::N3AnimationPlayer player;
    assert(player.configure(animations));
    assert(player.ready());
    assert(player.state() == korework::client::N3AnimationState::Idle);
    assert(player.frame() == 0.0F);

    player.update(0.5F);
    assert(player.frame() > 0.0F && player.frame() <= 30.0F);

    player.setState(korework::client::N3AnimationState::Move);
    assert(player.frame() == 40.0F);
    player.update(0.5F);
    assert(player.frame() > 40.0F && player.frame() <= 70.0F);

    player.setState(korework::client::N3AnimationState::Attack, true);
    assert(player.frame() == 80.0F);
    player.update(1.0F);
    assert(player.frame() >= 80.0F && player.frame() <= 95.0F);

    std::filesystem::remove(path);
    return 0;
}
