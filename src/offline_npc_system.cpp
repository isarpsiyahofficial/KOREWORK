#include "offline_npc_system.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace korework {
namespace {

Color npcColor(OfflineNpcSystem::NpcKind kind) {
    switch (kind) {
        case OfflineNpcSystem::NpcKind::Captain: return Color{180, 66, 54, 255};
        case OfflineNpcSystem::NpcKind::Merchant: return Color{194, 147, 53, 255};
        case OfflineNpcSystem::NpcKind::Healer: return Color{76, 139, 181, 255};
    }
    return GRAY;
}

} // namespace

OfflineNpcSystem::OfflineNpcSystem(std::size_t profileSlot)
    : state_(profileSlot) {}

float OfflineNpcSystem::distanceSquared(const Vec3& left, const Vec3& right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return x * x + y * y + z * z;
}

Vec3 OfflineNpcSystem::centeredMapPosition(const content::SmdMap* map, float x, float y, float z) noexcept {
    if (map == nullptr || !map->loaded()) return {x, y, z};
    return {x - map->width() * 0.5F, y, z - map->length() * 0.5F};
}

void OfflineNpcSystem::createNpcs(const content::SmdMap* map) {
    Vec3 origin {};
    if (map != nullptr && !map->regeneEvents().empty()) {
        const auto& regene = map->regeneEvents().front();
        origin = centeredMapPosition(map, regene.positionX, regene.positionY, regene.positionZ);
    }
    npcs_ = {
        {NpcKind::Captain, "Captain Ronark", {origin.x + 3.0F, origin.y, origin.z + 2.0F}},
        {NpcKind::Merchant, "Merchant Ares", {origin.x - 4.0F, origin.y, origin.z + 2.5F}},
        {NpcKind::Healer, "Priest Helena", {origin.x, origin.y, origin.z - 4.0F}}
    };
}

void OfflineNpcSystem::createShop(const OfflineRuntime& runtime) {
    shop_.clear();
    for (const auto& item : runtime.gameData().items) {
        if (item.id == 0U || item.name.empty() || item.name.rfind("KO Item #", 0) == 0U) continue;
        if (item.requiredLevel > static_cast<std::uint8_t>(std::min(83, runtime.player().level + 5))) continue;
        const bool useful = item.countable || item.slot < 14U || item.damage > 0 || item.armor > 0;
        if (!useful) continue;
        const int price = static_cast<int>(std::clamp<std::uint32_t>(item.buyPrice == 0U ? 100U : item.buyPrice, 10U, 2'000'000U));
        shop_.push_back({item.id, item.name, price});
        if (shop_.size() >= 24U) break;
    }
    if (selectedShopEntry_ >= shop_.size()) selectedShopEntry_ = 0U;
}

void OfflineNpcSystem::initialize(OfflineRuntime& runtime, const content::SmdMap* map) {
    createNpcs(map);
    createShop(runtime);
    if (runtime.player().position.x == 0.0F && runtime.player().position.z == 0.0F
        && map != nullptr && !map->regeneEvents().empty()) {
        const auto& regene = map->regeneEvents().front();
        runtime.player().position = centeredMapPosition(map, regene.positionX, regene.positionY, regene.positionZ);
        runtime.save();
    }
    findInteraction(runtime, map);
}

Vec3 OfflineNpcSystem::constrainToMap(const content::SmdMap* map, Vec3 requested, Vec3 fallback) const noexcept {
    if (map == nullptr || !map->loaded()) return requested;
    const float mapX = requested.x + map->width() * 0.5F;
    const float mapZ = requested.z + map->length() * 0.5F;
    return map->contains(mapX, mapZ) ? requested : fallback;
}

void OfflineNpcSystem::findInteraction(const OfflineRuntime& runtime, const content::SmdMap* map) {
    nearbyNpc_.reset();
    nearbyWarp_.reset();
    float closest = 16.0F;
    for (std::size_t index = 0; index < npcs_.size(); ++index) {
        const float current = distanceSquared(runtime.player().position, npcs_[index].position);
        if (current < closest) {
            closest = current;
            nearbyNpc_ = index;
        }
    }
    if (map != nullptr) {
        for (std::size_t index = 0; index < map->warps().size(); ++index) {
            const auto& warp = map->warps()[index];
            const Vec3 position = centeredMapPosition(map, warp.x, warp.y, warp.z);
            const float radius = std::max(3.0F, warp.radius);
            const float current = distanceSquared(runtime.player().position, position);
            if (current <= radius * radius && current < closest) {
                closest = current;
                nearbyNpc_.reset();
                nearbyWarp_ = index;
            }
        }
    }
}

void OfflineNpcSystem::openNearby() {
    if (nearbyNpc_.has_value()) {
        switch (npcs_[*nearbyNpc_].kind) {
            case NpcKind::Captain: mode_ = Mode::Quest; break;
            case NpcKind::Merchant: mode_ = Mode::Merchant; break;
            case NpcKind::Healer: mode_ = Mode::Healer; break;
        }
        message_.clear();
    } else if (nearbyWarp_.has_value()) {
        mode_ = Mode::Warp;
        message_.clear();
    }
}

std::uint32_t OfflineNpcSystem::questRewardItem(const OfflineRuntime& runtime) const noexcept {
    for (const auto& item : runtime.gameData().items) {
        if (item.id != 0U && item.requiredLevel <= 2U && (item.countable || item.slot < 14U)) return item.id;
    }
    return 0U;
}

void OfflineNpcSystem::updateQuest(OfflineRuntime& runtime) {
    auto& quest = state_.starterQuest();
    if (IsKeyPressed(KEY_ESCAPE)) {
        mode_ = Mode::None;
        return;
    }
    if (!IsKeyPressed(KEY_ENTER)) return;
    if (!quest.accepted) {
        quest.accepted = true;
        message_ = "Quest accepted: reach level 2.";
        (void) state_.save();
        return;
    }
    if (!quest.completed && runtime.player().level >= 2) {
        quest.completed = true;
        (void) state_.save();
    }
    if (quest.completed && !quest.rewardClaimed) {
        const std::uint32_t rewardItem = questRewardItem(runtime);
        if (runtime.grantQuestReward(rewardItem, rewardItem == 0U ? 0 : 1, 1500)) {
            quest.rewardClaimed = true;
            message_ = "Quest completed. 1500 Noah and an item awarded.";
            (void) state_.save();
        }
    } else if (!quest.completed) {
        message_ = "Reach level 2 before claiming the reward.";
    } else {
        message_ = "This quest is already completed.";
    }
}

void OfflineNpcSystem::updateMerchant(OfflineRuntime& runtime) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        mode_ = Mode::None;
        return;
    }
    if (shop_.empty()) {
        message_ = "No usable shop entries were found in Item_Org.";
        return;
    }
    if (IsKeyPressed(KEY_DOWN)) selectedShopEntry_ = (selectedShopEntry_ + 1U) % shop_.size();
    if (IsKeyPressed(KEY_UP)) selectedShopEntry_ = (selectedShopEntry_ + shop_.size() - 1U) % shop_.size();
    if (IsKeyPressed(KEY_ENTER)) {
        const auto& selected = shop_[selectedShopEntry_];
        if (runtime.purchaseItem(selected.itemId, 1, selected.price)) {
            state_.recordMerchantPurchase();
            message_ = selected.name + " purchased.";
        } else {
            message_ = "Purchase failed. Check Noah and item data.";
        }
    }
}

void OfflineNpcSystem::updateHealer(OfflineRuntime& runtime) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        mode_ = Mode::None;
        return;
    }
    if (IsKeyPressed(KEY_ENTER)) {
        const int cost = std::max(10, runtime.player().level * 20);
        if (runtime.restoreAtHealer(cost)) {
            state_.recordHealerVisit();
            message_ = "HP and MP restored for " + std::to_string(cost) + " Noah.";
        } else {
            message_ = "Healing failed: already full or insufficient Noah.";
        }
    }
}

void OfflineNpcSystem::updateWarp(OfflineRuntime& runtime, const content::SmdMap* map) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        mode_ = Mode::None;
        return;
    }
    if (map == nullptr || map->warps().empty() || !nearbyWarp_.has_value()) {
        message_ = "No usable warp destination.";
        return;
    }
    if (!IsKeyPressed(KEY_ENTER)) return;
    const std::size_t destinationIndex = (*nearbyWarp_ + 1U) % map->warps().size();
    const auto& destination = map->warps()[destinationIndex];
    const int cost = static_cast<int>(std::min<std::uint32_t>(destination.cost, static_cast<std::uint32_t>(std::numeric_limits<int>::max())));
    if (runtime.player().gold < cost) {
        message_ = "Insufficient Noah for warp.";
        return;
    }
    runtime.player().gold -= cost;
    runtime.player().position = centeredMapPosition(map, destination.x, destination.y, destination.z);
    runtime.save();
    message_ = "Warped to " + (destination.name.empty() ? std::string("destination") : destination.name) + ".";
    mode_ = Mode::None;
}

void OfflineNpcSystem::update(OfflineRuntime& runtime, const content::SmdMap* map) {
    if (mode_ == Mode::None) {
        findInteraction(runtime, map);
        if (IsKeyPressed(KEY_F)) openNearby();
        return;
    }
    switch (mode_) {
        case Mode::Quest: updateQuest(runtime); break;
        case Mode::Merchant: updateMerchant(runtime); break;
        case Mode::Healer: updateHealer(runtime); break;
        case Mode::Warp: updateWarp(runtime, map); break;
        case Mode::None: break;
    }
}

void OfflineNpcSystem::drawWorld(const content::SmdMap* map) const {
    for (const auto& npc : npcs_) {
        const float ground = map == nullptr ? npc.position.y : map->heightAt(npc.position.x + map->width() * 0.5F,
                                                                             npc.position.z + map->length() * 0.5F);
        const Vector3 base {npc.position.x, ground, npc.position.z};
        const Color color = npcColor(npc.kind);
        DrawCylinder({base.x, base.y + 0.9F, base.z}, 0.38F, 0.48F, 1.25F, 12, color);
        DrawSphere({base.x, base.y + 1.78F, base.z}, 0.30F, Color{214, 185, 144, 255});
        DrawCircle3D({base.x, base.y + 0.02F, base.z}, 0.65F, {1.0F, 0.0F, 0.0F}, 90.0F, Color{245, 212, 111, 135});
    }
    if (map != nullptr) {
        for (const auto& warp : map->warps()) {
            const Vec3 position = centeredMapPosition(map, warp.x, warp.y, warp.z);
            const float ground = map->heightAt(position.x + map->width() * 0.5F, position.z + map->length() * 0.5F);
            DrawCircle3D({position.x, ground + 0.03F, position.z}, std::max(1.2F, warp.radius),
                         {1.0F, 0.0F, 0.0F}, 90.0F, Color{82, 156, 231, 190});
        }
    }
}

void OfflineNpcSystem::drawUi(const OfflineRuntime& runtime) const {
    if (mode_ == Mode::None) {
        if (nearbyNpc_.has_value()) {
            const std::string prompt = "F: Talk to " + npcs_[*nearbyNpc_].name;
            DrawText(prompt.c_str(), (GetScreenWidth() - MeasureText(prompt.c_str(), 20)) / 2, GetScreenHeight() - 112, 20, YELLOW);
        } else if (nearbyWarp_.has_value()) {
            DrawText("F: Use warp gate", (GetScreenWidth() - MeasureText("F: Use warp gate", 20)) / 2,
                     GetScreenHeight() - 112, 20, SKYBLUE);
        }
        return;
    }

    const Rectangle panel {(GetScreenWidth() - 620.0F) * 0.5F, (GetScreenHeight() - 430.0F) * 0.5F, 620.0F, 430.0F};
    DrawRectangleRounded(panel, 0.04F, 8, Color{7, 10, 15, 246});
    DrawRectangleLinesEx(panel, 2.0F, Color{206, 171, 77, 255});
    const char* title = mode_ == Mode::Quest ? "CAPTAIN - STARTER QUEST"
                       : mode_ == Mode::Merchant ? "MERCHANT - ITEM_ORG SHOP"
                       : mode_ == Mode::Healer ? "HEALER"
                       : "WARP GATE";
    DrawText(title, static_cast<int>(panel.x + 22.0F), static_cast<int>(panel.y + 18.0F), 25, Color{240, 215, 142, 255});

    if (mode_ == Mode::Quest) {
        const auto& quest = state_.starterQuest();
        std::string status = !quest.accepted ? "Reach level 2 and return for your first reward."
                           : !quest.completed && runtime.player().level < 2 ? "Quest active: reach level 2."
                           : !quest.rewardClaimed ? "Objective complete. Press ENTER to claim reward."
                           : "Quest completed and reward claimed.";
        DrawText(status.c_str(), static_cast<int>(panel.x + 26.0F), static_cast<int>(panel.y + 85.0F), 20, RAYWHITE);
        DrawText("ENTER: accept/claim   ESC: close", static_cast<int>(panel.x + 26.0F), static_cast<int>(panel.y + 135.0F), 18, LIGHTGRAY);
    } else if (mode_ == Mode::Merchant) {
        int y = static_cast<int>(panel.y + 70.0F);
        const std::size_t first = selectedShopEntry_ > 5U ? selectedShopEntry_ - 5U : 0U;
        for (std::size_t index = first; index < shop_.size() && index < first + 11U; ++index) {
            const Rectangle row {panel.x + 22.0F, static_cast<float>(y), panel.width - 44.0F, 27.0F};
            DrawRectangleRounded(row, 0.06F, 4, index == selectedShopEntry_ ? Color{75, 64, 38, 255} : Color{28, 32, 40, 255});
            const std::string label = shop_[index].name + "  -  " + std::to_string(shop_[index].price) + " Noah";
            DrawText(label.c_str(), static_cast<int>(row.x + 8.0F), static_cast<int>(row.y + 5.0F), 16, RAYWHITE);
            y += 30;
        }
        DrawText("UP/DOWN: select   ENTER: buy   ESC: close", static_cast<int>(panel.x + 22.0F), static_cast<int>(panel.y + panel.height - 48.0F), 17, LIGHTGRAY);
    } else if (mode_ == Mode::Healer) {
        const int cost = std::max(10, runtime.player().level * 20);
        const std::string line = "Restore all HP and MP for " + std::to_string(cost) + " Noah.";
        DrawText(line.c_str(), static_cast<int>(panel.x + 26.0F), static_cast<int>(panel.y + 90.0F), 21, RAYWHITE);
        DrawText("ENTER: heal   ESC: close", static_cast<int>(panel.x + 26.0F), static_cast<int>(panel.y + 140.0F), 18, LIGHTGRAY);
    } else {
        DrawText("ENTER: travel to the next SMD warp destination", static_cast<int>(panel.x + 26.0F), static_cast<int>(panel.y + 90.0F), 20, RAYWHITE);
        DrawText("ESC: close", static_cast<int>(panel.x + 26.0F), static_cast<int>(panel.y + 140.0F), 18, LIGHTGRAY);
    }
    if (!message_.empty()) DrawText(message_.c_str(), static_cast<int>(panel.x + 24.0F), static_cast<int>(panel.y + panel.height - 82.0F), 17, Color{246, 201, 103, 255});
}

} // namespace korework
