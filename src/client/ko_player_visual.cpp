#include "client/ko_player_visual.hpp"

#include "content/n3_animation.hpp"
#include "content/n3_character.hpp"
#include "content/n3_equipment.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <string>

namespace korework::client {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool isUpperOutfitPart(const content::N3CharacterPart& part) {
    const std::string value = lower(part.sourcePath.filename().string());
    return value.find("head") != std::string::npos
        || value.find("hair") != std::string::npos
        || value.find("_u_") != std::string::npos;
}

} // namespace

std::filesystem::path KoPlayerVisual::authenticCharacterPath() const {
    // The complete El Morad female Fire Drake character contains the base head/hair
    // and both underwear/upper outfit sets. Mage has its own complete Karus model.
    switch (playerClass_) {
        case PlayerClass::Mage:
            return assetRoot_ / "game" / "ChrSelect" / "upc_ka_wt_ma.n3chr";
        case PlayerClass::Warrior:
        case PlayerClass::Rogue:
        case PlayerClass::Priest:
            return assetRoot_ / "game" / "ChrSelect" / "upc_el_rf_bone.n3chr";
    }
    return {};
}


std::filesystem::path KoPlayerVisual::authenticAnimationPath() const {
    switch (playerClass_) {
        case PlayerClass::Warrior:
            return assetRoot_ / "game" / "ChrSelect" / "upc_el_rf_wa.n3anim";
        case PlayerClass::Rogue:
            return assetRoot_ / "game" / "ChrSelect" / "upc_el_rf_rog.n3anim";
        case PlayerClass::Mage:
            return assetRoot_ / "game" / "ChrSelect" / "upc_ka_wt_ma.n3anim";
        case PlayerClass::Priest:
            return assetRoot_ / "game" / "ChrSelect" / "upc_el_rf_pri.n3anim";
    }
    return {};
}

std::filesystem::path KoPlayerVisual::authenticWeaponPath() const {
    switch (playerClass_) {
        case PlayerClass::Warrior:
            return assetRoot_ / "game" / "ChrSelect" / "wea_el_long_sword_left.n3cplug";
        case PlayerClass::Rogue:
            return assetRoot_ / "game" / "ChrSelect" / "wea_el_rf_rog_bow.n3cplug";
        case PlayerClass::Mage:
            return assetRoot_ / "game" / "ChrSelect" / "wea_ka_staff.n3cplug";
        case PlayerClass::Priest:
            return assetRoot_ / "game" / "ChrSelect" / "wea_el_wand.n3cplug";
    }
    return {};
}

bool KoPlayerVisual::tryLoadCharacter(const std::filesystem::path& path, bool selectUpperOutfit) {
    try {
        const content::N3CharacterLoader loader(*resolver_);
        auto character = loader.load(path);
        if (selectUpperOutfit) {
            character.parts.erase(std::remove_if(character.parts.begin(), character.parts.end(), [](const auto& part) {
                return !isUpperOutfitPart(part);
            }), character.parts.end());
        }
        if (character.parts.empty()) {
            error_ = "Authentic Fire Drake player has no selected render parts";
            return false;
        }
        const auto animationPath = authenticAnimationPath();
        if (std::filesystem::is_regular_file(animationPath)) character.animationPath = animationPath;
        if (character.animationPath.empty() || !model_.load(character)) {
            error_ = model_.error().empty() ? "N3 player model load failed" : model_.error();
            return false;
        }
        if (!std::isfinite(model_.sourceHeight()) || model_.sourceHeight() < 0.25F || model_.sourceHeight() > 20.0F) {
            error_ = "N3 player geometry has an invalid source height";
            model_.unload();
            return false;
        }
        const auto animations = content::N3AnimationSet::load(character.animationPath);
        if (!animation_.configure(animations)) {
            model_.unload();
            error_ = "N3 player animation metadata has no usable idle/move/attack clips";
            return false;
        }
        sourceName_ = path.filename().string();
        return true;
    } catch (const std::exception& exception) {
        model_.unload();
        error_ = exception.what();
        return false;
    }
}

bool KoPlayerVisual::initialize(const std::filesystem::path& assetRoot, PlayerClass playerClass) {
    assetRoot_ = assetRoot;
    playerClass_ = playerClass;
    catalog_ = content::KoAssetCatalog::scan(assetRoot_);
    resolver_ = std::make_unique<content::KoAssetResolver>(assetRoot_ / "game");
    model_.unload();
    weapon_.unload();
    animation_.reset();
    weaponAppearanceId_ = 0U;
    sourceName_.clear();
    error_.clear();

    const auto characterPath = authenticCharacterPath();
    if (characterPath.empty() || !std::filesystem::is_regular_file(characterPath)) {
        error_ = "Required Fire Drake ChrSelect player asset is missing: " + characterPath.string();
        return false;
    }
    const bool selectUpperOutfit = playerClass_ != PlayerClass::Mage;
    if (!tryLoadCharacter(characterPath, selectUpperOutfit)) return false;
    (void) loadDefaultWeapon();
    return true;
}

bool KoPlayerVisual::tryLoadWeapon(const std::filesystem::path& path) {
    try {
        const content::N3EquipmentLoader loader(*resolver_);
        const auto plug = loader.load(path);
        if (plug.jointIndex < 0 || !weapon_.load(plug)) {
            error_ = weapon_.error().empty() ? "N3 weapon plug load failed" : weapon_.error();
            return false;
        }
        const auto joint = model_.jointWorldMatrix(static_cast<std::size_t>(plug.jointIndex), animation_.frame());
        if (!joint.has_value() || !weapon_.update(*joint, model_.renderCenterX(), model_.renderCenterZ(), model_.renderMinimumY())) {
            error_ = weapon_.error().empty() ? "N3 weapon joint update failed" : weapon_.error();
            weapon_.unload();
            return false;
        }
        return true;
    } catch (const std::exception& exception) {
        weapon_.unload();
        error_ = exception.what();
        return false;
    }
}

bool KoPlayerVisual::loadDefaultWeapon() {
    const auto path = authenticWeaponPath();
    weaponAppearanceId_ = 0U;
    weapon_.unload();
    if (path.empty() || !std::filesystem::is_regular_file(path)) return false;
    return tryLoadWeapon(path);
}

bool KoPlayerVisual::setWeapon(const data::ItemRecord* item) {
    const std::uint32_t appearanceId = item == nullptr ? 0U : item->appearanceId;
    if (appearanceId == weaponAppearanceId_ && weapon_.ready()) return true;
    if (appearanceId == 0U) return loadDefaultWeapon();

    weaponAppearanceId_ = appearanceId;
    weapon_.unload();
    if (!ready()) return false;

    const std::string exact = std::to_string(appearanceId);
    const std::string broad = std::to_string(appearanceId / 1000U);
    for (const auto& relative : catalog_.equipmentPlugFiles) {
        const std::string filename = lower(relative.filename().string());
        if (filename.find(exact) == std::string::npos
            && (broad.size() < 4U || filename.find(broad) == std::string::npos)) continue;
        if (tryLoadWeapon(assetRoot_ / relative)) return true;
    }
    error_ = "No N3 weapon plug matched Item_Org appearance id " + exact;
    return loadDefaultWeapon();
}


void KoPlayerVisual::unload() noexcept {
    weapon_.unload();
    model_.unload();
    animation_.reset();
    resolver_.reset();
    catalog_ = {};
    assetRoot_.clear();
    weaponAppearanceId_ = 0U;
    sourceName_.clear();
    error_.clear();
}

void KoPlayerVisual::update(N3AnimationState state, float deltaSeconds) {
    if (!ready() || !animation_.ready()) return;
    const bool restart = state == N3AnimationState::Attack && animation_.state() != state;
    animation_.setState(state, restart);
    animation_.update(deltaSeconds);
    (void) model_.updateAnimation(animation_.frame());
    if (weapon_.ready()) {
        const auto joint = model_.jointWorldMatrix(static_cast<std::size_t>(weapon_.jointIndex()), animation_.frame());
        if (joint.has_value()) {
            (void) weapon_.update(*joint, model_.renderCenterX(), model_.renderCenterZ(), model_.renderMinimumY());
        }
    }
}

void KoPlayerVisual::draw(Vector3 worldPosition, float targetHeight, Color tint, float yawDegrees) const {
    if (!ready()) return;
    model_.draw(worldPosition, targetHeight, tint, yawDegrees);
    if (weapon_.ready()) weapon_.draw(worldPosition, targetHeight, model_.sourceHeight(), tint, yawDegrees);
}

} // namespace korework::client
