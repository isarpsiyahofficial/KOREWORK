#pragma once

#include <cstddef>
#include <filesystem>

namespace korework {

struct OfflineQuestProgress {
    bool accepted = false;
    bool completed = false;
    bool rewardClaimed = false;
};

class OfflineWorldState final {
public:
    explicit OfflineWorldState(std::size_t profileSlot = 0U);

    bool load();
    bool save() const;
    void reset();

    [[nodiscard]] std::size_t profileSlot() const noexcept { return profileSlot_; }
    [[nodiscard]] const OfflineQuestProgress& starterQuest() const noexcept { return starterQuest_; }
    [[nodiscard]] OfflineQuestProgress& starterQuest() noexcept { return starterQuest_; }
    [[nodiscard]] int merchantPurchases() const noexcept { return merchantPurchases_; }
    [[nodiscard]] int healerVisits() const noexcept { return healerVisits_; }

    void recordMerchantPurchase();
    void recordHealerVisit();

private:
    [[nodiscard]] std::filesystem::path path() const;

    std::size_t profileSlot_ = 0U;
    OfflineQuestProgress starterQuest_;
    int merchantPurchases_ = 0;
    int healerVisits_ = 0;
};

} // namespace korework
