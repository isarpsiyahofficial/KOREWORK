#include "offline_world_state.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>

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
    const auto root = std::filesystem::temp_directory_path() / "korework_world_state_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    setHome(root);

    korework::OfflineWorldState state(1U);
    state.reset();
    assert(!state.starterQuest().accepted);
    state.starterQuest().accepted = true;
    state.starterQuest().completed = true;
    state.starterQuest().rewardClaimed = true;
    state.recordMerchantPurchase();
    state.recordMerchantPurchase();
    state.recordHealerVisit();
    assert(state.save());

    korework::OfflineWorldState loaded(1U);
    assert(loaded.starterQuest().accepted);
    assert(loaded.starterQuest().completed);
    assert(loaded.starterQuest().rewardClaimed);
    assert(loaded.merchantPurchases() == 2);
    assert(loaded.healerVisits() == 1);

    korework::OfflineWorldState separate(2U);
    separate.reset();
    assert(!separate.starterQuest().accepted);
    assert(separate.merchantPurchases() == 0);
    assert(separate.healerVisits() == 0);

    std::filesystem::remove_all(root);
    return 0;
}
