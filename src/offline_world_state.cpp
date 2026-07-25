#include "offline_world_state.hpp"

#include "offline_roster.hpp"

#include <algorithm>
#include <fstream>

namespace korework {

OfflineWorldState::OfflineWorldState(std::size_t profileSlot)
    : profileSlot_(std::min<std::size_t>(profileSlot, OfflineRoster::SlotCount - 1U)) {
    (void) load();
}

std::filesystem::path OfflineWorldState::path() const {
    return OfflineRoster::storageRoot() / ("offline_world_" + std::to_string(profileSlot_) + ".koworld");
}

void OfflineWorldState::reset() {
    starterQuest_ = {};
    merchantPurchases_ = 0;
    healerVisits_ = 0;
    (void) save();
}

bool OfflineWorldState::load() {
    std::ifstream input(path());
    if (!input) return false;
    std::string header;
    std::getline(input, header);
    if (header != "KOREWORK_WORLD_V1") return false;
    int accepted = 0;
    int completed = 0;
    int rewardClaimed = 0;
    input >> accepted >> completed >> rewardClaimed >> merchantPurchases_ >> healerVisits_;
    if (!input || accepted < 0 || accepted > 1 || completed < 0 || completed > 1
        || rewardClaimed < 0 || rewardClaimed > 1 || merchantPurchases_ < 0 || healerVisits_ < 0) return false;
    starterQuest_.accepted = accepted != 0;
    starterQuest_.completed = completed != 0;
    starterQuest_.rewardClaimed = rewardClaimed != 0;
    if (starterQuest_.rewardClaimed) starterQuest_.completed = true;
    if (starterQuest_.completed) starterQuest_.accepted = true;
    return true;
}

bool OfflineWorldState::save() const {
    const std::filesystem::path destination = path();
    const std::filesystem::path temporary = destination.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << "KOREWORK_WORLD_V1\n"
           << (starterQuest_.accepted ? 1 : 0) << ' '
           << (starterQuest_.completed ? 1 : 0) << ' '
           << (starterQuest_.rewardClaimed ? 1 : 0) << ' '
           << merchantPurchases_ << ' ' << healerVisits_ << '\n';
    output.close();
    if (!output) return false;
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (!error) return true;
    error.clear();
    std::filesystem::remove(destination, error);
    error.clear();
    std::filesystem::rename(temporary, destination, error);
    return !error;
}

void OfflineWorldState::recordMerchantPurchase() {
    ++merchantPurchases_;
    (void) save();
}

void OfflineWorldState::recordHealerVisit() {
    ++healerVisits_;
    (void) save();
}

} // namespace korework
