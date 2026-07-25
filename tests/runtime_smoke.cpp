#include "offline_runtime.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

namespace {

bool externalDataPackRequired() {
    const char* value = std::getenv("KOREWORK_DATA_PACK");
    return value != nullptr && *value != '\0';
}

void validateRuntime(const korework::OfflineRuntime& runtime, bool requireExternalData) {
    assert(!runtime.monsters().empty());
    assert(runtime.skills().size() == 10U);
    assert(runtime.player().maxHp > 0.0F);

    if (requireExternalData) {
        assert(runtime.usingGameData());
        assert(runtime.gameData().monsters.size() >= 50U);
        assert(runtime.gameData().dropTables.size() >= 20U);
        assert(runtime.gameData().skills.size() >= 20U);
        assert(runtime.gameData().classes.size() >= 4U);
        assert(runtime.monsterTemplates().size() == runtime.gameData().monsters.size());
    }
}

} // namespace

int main() {
    const bool requireExternalData = externalDataPackRequired();

    korework::OfflineRuntime runtime;
    runtime.initialize();
    validateRuntime(runtime, requireExternalData);

    runtime.player().position = runtime.monsters().front().position;
    const auto target = runtime.nearestAliveMonster(3.0F);
    assert(target.has_value());

    const bool attacked = runtime.useSkill(0, target);
    assert(attacked);
    runtime.update(0.1F);
    runtime.save();

    korework::OfflineRuntime reloaded;
    reloaded.initialize();
    validateRuntime(reloaded, requireExternalData);

    std::cout << "KOREWORK offline runtime smoke test passed"
              << (requireExternalData ? " with generated OpenKO KOPACK\n" : " with fallback data\n");
    return 0;
}
