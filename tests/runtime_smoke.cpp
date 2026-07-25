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
        assert(runtime.gameData().items.size() >= 100U);
        assert(runtime.monsterTemplates().size() == runtime.gameData().monsters.size());

        std::size_t namedItems = 0;
        std::size_t equipmentItems = 0;
        for (const auto& item : runtime.gameData().items) {
            if (!item.name.empty() && item.name.rfind("KO Item #", 0) != 0U) ++namedItems;
            if (item.appearanceId != 0U && item.slot < 15U) ++equipmentItems;
        }
        assert(namedItems >= 100U);
        assert(equipmentItems > 0U);
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
              << (requireExternalData ? " with generated OpenKO KOPACK and real item catalog\n" : " with fallback data\n");
    return 0;
}
