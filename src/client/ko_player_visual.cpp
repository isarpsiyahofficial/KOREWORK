#include "client/ko_player_visual.hpp"

#include "content/n3_animation.hpp"
#include "content/n3_character.hpp"
#include "content/n3_equipment.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <string>
#include <vector>

namespace korework::client {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool containsAny(const std::string& value, const std::vector<std::string>& needles) {
    return std::any_of(needles.begin(), needles.end(), [&](const std::string& needle) {
        return value.find(needle) != std::string::npos;
    });
}

} // namespace

int KoPlayerVisual::scorePath(const std::filesystem::path& path, PlayerClass playerClass) {
    const std::string value = lower(path.generic_string());
    if (containsAny(value, {"/npc/", "\\npc\\", "mob_", "/monster/", "\\monster\\"})) return -10000;

    int score = 0;
    if (value.find("upc") != std::string::npos) score += 80;
    if (value.find("player") != std::string::npos) score += 60;
    if (value.find("character") != std::string::npos) score += 20;
    if (value.find("item") != std::string::npos) score -= 10;

    switch (playerClass) {
        case PlayerClass::Warrior:
            if (containsAny(value, {"warrior", "fighter", "blade", "berserker"})) score += 120;
            break;
        case PlayerClass::Rogue:
            if (containsAny(value, {"rogue", "archer", "assassin", "hunter", "ranger"})) score += 120;
            break;
        case PlayerClass::Mage:
            if (containsAny(value, {"mage", "wizard", "sorcerer", "enchanter"})) score += 120;
            break;
        case PlayerClass::Priest:
            if (containsAny(value, {"priest", "cleric", "druid", "shaman"})) score += 120;
            break;
    }
    if (path.extension() == ".n3chr" || path.extension() == ".N3CHR") score += 5;
    return score;
}

bool KoPlayerVisual::tryLoadCharacter(const std::filesystem::path& path) {
    try {
        const content::N3CharacterLoader loader(*resolver_);
        const auto character = loader.load(path);
        if (character.animationPath.empty() || !model_.load(character)) {
            error_ = model_.error().empty() ? "N3 player model load failed" : model_.error();
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
    catalog_ = content::KoAssetCatalog::scan(assetRoot_);
    resolver_ = std::make_unique<content::KoAssetResolver>(assetRoot_ / "game");
    model_.unload();
    weapon_.unload();
    animation_.reset();
    sourceName_.clear();
    error_.clear();

    std::vector<std::pair<int, std::filesystem::path>> candidates;
    candidates.reserve(catalog_.characterFiles.size());
    for (const auto& relative : catalog_.characterFiles) {
        const int score = scorePath(relative, playerClass);
        if (score > -10000) candidates.emplace_back(score, assetRoot_ / relative);
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        if (left.first != right.first) return left.first > right.first;
        return left.second.generic_string() < right.second.generic_string();
    });

    const std::size_t maximumAttempts = std::min<std::size_t>(candidates.size(), 128U);
    for (std::size_t index = 0; index < maximumAttempts; ++index) {
        if (tryLoadCharacter(candidates[index].second)) return true;
    }
    if (error_.empty()) error_ = "No non-mob N3 player character could be loaded";
    return false;
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

bool KoPlayerVisual::setWeapon(const data::ItemRecord* item) {
    const std::uint32_t appearanceId = item == nullptr ? 0U : item->appearanceId;
    if (appearanceId == weaponAppearanceId_) return weapon_.ready() || appearanceId == 0U;
    weaponAppearanceId_ = appearanceId;
    weapon_.unload();
    if (!ready() || item == nullptr || appearanceId == 0U) return appearanceId == 0U;

    const std::string exact = std::to_string(appearanceId);
    const std::string broad = std::to_string(appearanceId / 1000U);
    std::vector<std::filesystem::path> candidates;
    for (const auto& relative : catalog_.equipmentPlugFiles) {
        const std::string filename = lower(relative.filename().string());
        if (filename.find(exact) != std::string::npos || (broad.size() >= 4U && filename.find(broad) != std::string::npos)) {
            candidates.push_back(assetRoot_ / relative);
        }
    }
    std::sort(candidates.begin(), candidates.end());
    for (const auto& candidate : candidates) if (tryLoadWeapon(candidate)) return true;
    error_ = "No N3 weapon plug matched Item_Org appearance id " + exact;
    return false;
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

void KoPlayerVisual::draw(Vector3 worldPosition, float targetHeight, Color tint) const {
    if (!ready()) return;
    model_.draw(worldPosition, targetHeight, tint);
    if (weapon_.ready()) weapon_.draw(worldPosition, targetHeight, model_.sourceHeight(), tint);
}

} // namespace korework::client
