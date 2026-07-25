#pragma once

#include "client/n3_animation_player.hpp"
#include "client/n3_character_model.hpp"
#include "client/n3_equipment_model.hpp"
#include "content/asset_catalog.hpp"
#include "content/ko_asset_resolver.hpp"
#include "offline_roster.hpp"
#include "data/game_data_pack.hpp"

#include "raylib.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace korework::client {

class KoPlayerVisual final {
public:
    KoPlayerVisual() = default;

    bool initialize(const std::filesystem::path& assetRoot, PlayerClass playerClass);
    void update(N3AnimationState state, float deltaSeconds);
    bool setWeapon(const data::ItemRecord* item);
    void draw(Vector3 worldPosition, float targetHeight, Color tint = WHITE) const;

    [[nodiscard]] bool ready() const noexcept { return model_.ready(); }
    [[nodiscard]] bool weaponReady() const noexcept { return weapon_.ready(); }
    [[nodiscard]] const std::string& sourceName() const noexcept { return sourceName_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    [[nodiscard]] static int scorePath(const std::filesystem::path& path, PlayerClass playerClass);
    [[nodiscard]] bool tryLoadCharacter(const std::filesystem::path& path);
    [[nodiscard]] bool tryLoadWeapon(const std::filesystem::path& path);

    std::filesystem::path assetRoot_;
    content::KoAssetCatalog catalog_;
    std::unique_ptr<content::KoAssetResolver> resolver_;
    N3CharacterModel model_;
    N3AnimationPlayer animation_;
    N3EquipmentModel weapon_;
    std::uint32_t weaponAppearanceId_ = 0;
    std::string sourceName_;
    std::string error_;
};

} // namespace korework::client
