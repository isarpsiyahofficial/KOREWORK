#include "offline_runtime.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace {

void setHome(const std::filesystem::path& path) {
#ifdef _WIN32
    _putenv_s("APPDATA", path.string().c_str());
#else
    setenv("HOME", path.string().c_str(), 1);
#endif
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "korework_runtime_spawn_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    setHome(root);

    korework::OfflineRuntime runtime;
    runtime.configureProfile(0U, "SpawnTester", korework::PlayerClass::Warrior);
    runtime.initialize();
    assert(!runtime.monsterTemplates().empty());
    const std::uint32_t npcId = runtime.monsterTemplates().front().sid;

    std::vector<korework::MonsterSpawnPlacement> placements {
        {npcId, {-20.0F, 0.0F, -10.0F}, {-5.0F, 0.0F, 12.0F}, 5U},
        {999999U, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 1.0F}, 4U}
    };
    const std::size_t count = runtime.replaceWorldSpawns(placements, 32U);
    assert(count == 5U);
    assert(runtime.monsters().size() == 5U);
    for (const auto& monster : runtime.monsters()) {
        assert(monster.templateIndex == 0U);
        assert(monster.position.x >= -20.0F && monster.position.x <= -5.0F);
        assert(monster.position.z >= -10.0F && monster.position.z <= 12.0F);
        assert(monster.spawnPosition.x == monster.position.x);
        assert(monster.spawnPosition.z == monster.position.z);
        assert(monster.hp == runtime.monsterTemplates().front().maxHp);
    }

    std::filesystem::remove_all(root);
    return 0;
}
