#pragma once

#include "client/n3_animation_player.hpp"
#include "client/n3_character_model.hpp"
#include "client/n3_equipment_model.hpp"
#include "content/ko_asset_resolver.hpp"
#include "content/npc_looks_table.hpp"

#include "raylib.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace korework::client {

class KoMonsterVisualBank final {
public:
    KoMonsterVisualBank() = default;
    ~KoMonsterVisualBank() = default;

    KoMonsterVisualBank(const KoMonsterVisualBank&) = delete;
    KoMonsterVisualBank& operator=(const KoMonsterVisualBank&) = delete;

    bool initialize(const std::filesystem::path& assetRoot);
    void unload() noexcept;
    void beginFrame();
    bool update(std::uint32_t modelId, N3AnimationState state, float deltaSeconds);
    bool draw(std::uint32_t modelId, Vector3 worldPosition, float targetHeight, Color tint = WHITE);
    void preload(const std::vector<std::uint32_t>& modelIds, std::size_t maximumModels = 32U);

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] std::size_t loadedCount() const noexcept { return visuals_.size(); }
    [[nodiscard]] std::size_t mappedCount() const noexcept { return looks_.records().size(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    struct Visual final {
        N3CharacterModel character;
        N3EquipmentModel equipment;
        N3AnimationPlayer animation;
        bool equipmentReady = false;
        std::string error;
    };

    [[nodiscard]] Visual* ensure(std::uint32_t modelId);
    [[nodiscard]] std::filesystem::path locateLooksTable(const std::filesystem::path& assetRoot) const;
    [[nodiscard]] bool attachFirstEquipment(Visual& visual,
                                            const content::N3Character& character,
                                            float frame);

    std::filesystem::path assetRoot_;
    std::filesystem::path looksTablePath_;
    std::unique_ptr<content::KoAssetResolver> resolver_;
    content::NpcLooksTable looks_;
    std::unordered_map<std::uint32_t, std::unique_ptr<Visual>> visuals_;
    std::unordered_set<std::uint32_t> failed_;
    std::unordered_set<std::uint32_t> updatedThisFrame_;
    bool ready_ = false;
    std::string error_;
};

} // namespace korework::client
