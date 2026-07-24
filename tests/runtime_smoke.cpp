#include "offline_runtime.hpp"

#include <cassert>
#include <iostream>

int main() {
    korework::OfflineRuntime runtime;
    runtime.initialize();

    assert(!runtime.monsters().empty());
    assert(runtime.skills().size() == 10);
    assert(runtime.player().maxHp > 0.0F);

    runtime.player().position = runtime.monsters().front().position;
    const auto target = runtime.nearestAliveMonster(3.0F);
    assert(target.has_value());

    const bool attacked = runtime.useSkill(0, target);
    assert(attacked);
    runtime.update(0.1F);
    runtime.save();

    korework::OfflineRuntime reloaded;
    reloaded.initialize();
    assert(reloaded.player().maxHp > 0.0F);
    assert(reloaded.skills().size() == 10);

    std::cout << "KOREWORK offline runtime smoke test passed\n";
    return 0;
}
