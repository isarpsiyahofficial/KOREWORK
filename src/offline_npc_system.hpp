#pragma once

#include "content/smd_map.hpp"
#include "offline_runtime.hpp"
#include "offline_world_state.hpp"

#include "raylib.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace korework {

class OfflineNpcSystem final {
public:
    enum class NpcKind {
        Captain,
        Merchant,
        Healer
    };

    explicit OfflineNpcSystem(std::size_t profileSlot = 0U);

    void initialize(OfflineRuntime& runtime, const content::SmdMap* map);
    void update(OfflineRuntime& runtime, const content::SmdMap* map);
    void drawWorld(const content::SmdMap* map) const;
    void drawUi(const OfflineRuntime& runtime) const;

    [[nodiscard]] bool modalOpen() const noexcept { return mode_ != Mode::None; }
    [[nodiscard]] bool interactionAvailable() const noexcept { return nearbyNpc_.has_value() || nearbyWarp_.has_value(); }
    [[nodiscard]] Vec3 constrainToMap(const content::SmdMap* map, Vec3 requested, Vec3 fallback) const noexcept;

private:
    enum class Mode {
        None,
        Quest,
        Merchant,
        Healer,
        Warp
    };

    struct Npc {
        NpcKind kind = NpcKind::Captain;
        std::string name;
        Vec3 position;
    };

    struct ShopEntry {
        std::uint32_t itemId = 0;
        std::string name;
        int price = 0;
    };

    [[nodiscard]] static float distanceSquared(const Vec3& left, const Vec3& right) noexcept;
    [[nodiscard]] static Vec3 centeredMapPosition(const content::SmdMap* map, float x, float y, float z) noexcept;
    void createNpcs(const content::SmdMap* map);
    void createShop(const OfflineRuntime& runtime);
    void findInteraction(const OfflineRuntime& runtime, const content::SmdMap* map);
    void openNearby();
    void updateQuest(OfflineRuntime& runtime);
    void updateMerchant(OfflineRuntime& runtime);
    void updateHealer(OfflineRuntime& runtime);
    void updateWarp(OfflineRuntime& runtime, const content::SmdMap* map);
    [[nodiscard]] std::uint32_t questRewardItem(const OfflineRuntime& runtime) const noexcept;

    OfflineWorldState state_;
    std::vector<Npc> npcs_;
    std::vector<ShopEntry> shop_;
    std::optional<std::size_t> nearbyNpc_;
    std::optional<std::size_t> nearbyWarp_;
    Mode mode_ = Mode::None;
    std::size_t selectedShopEntry_ = 0U;
    std::string message_;
};

} // namespace korework
