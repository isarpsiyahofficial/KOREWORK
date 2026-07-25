#include "client/ko_monster_visual_bank.hpp"

#include "content/n3_animation.hpp"
#include "content/n3_character.hpp"
#include "content/n3_equipment.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <system_error>

namespace korework::client {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

} // namespace

std::filesystem::path KoMonsterVisualBank::locateLooksTable(const std::filesystem::path& assetRoot) const {
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(
             assetRoot, std::filesystem::directory_options::skip_permission_denied, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (!iterator->is_regular_file(error)) continue;
        if (lower(iterator->path().filename().string()) == "npc_looks.tbl") return iterator->path();
    }
    throw std::runtime_error("NPC_Looks.tbl was not found in the KO asset tree");
}

bool KoMonsterVisualBank::initialize(const std::filesystem::path& assetRoot) {
    unload();
    error_.clear();
    try {
        assetRoot_ = std::filesystem::weakly_canonical(assetRoot);
        const auto gameRoot = assetRoot_ / "game";
        if (!std::filesystem::is_directory(gameRoot)) throw std::runtime_error("KO game asset directory is missing");
        resolver_ = std::make_unique<content::KoAssetResolver>(gameRoot);
        looksTablePath_ = locateLooksTable(assetRoot_);
        looks_ = content::NpcLooksTable::load(looksTablePath_);
        if (looks_.records().empty()) throw std::runtime_error("NPC_Looks.tbl contains no model mappings");
        ready_ = true;
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
        unload();
        return false;
    }
}

void KoMonsterVisualBank::unload() noexcept {
    visuals_.clear();
    failed_.clear();
    updatedThisFrame_.clear();
    resolver_.reset();
    looks_ = {};
    assetRoot_.clear();
    looksTablePath_.clear();
    ready_ = false;
}

void KoMonsterVisualBank::beginFrame() {
    updatedThisFrame_.clear();
}

bool KoMonsterVisualBank::attachFirstEquipment(Visual& visual,
                                               const content::N3Character& character,
                                               float frame) {
    if (resolver_ == nullptr || character.plugs.empty()) return false;
    const content::N3EquipmentLoader loader(*resolver_);
    for (const auto& plugPath : character.plugs) {
        try {
            const auto plug = loader.load(plugPath);
            if (plug.jointIndex < 0) continue;
            const auto joint = visual.character.jointWorldMatrix(static_cast<std::size_t>(plug.jointIndex), frame);
            if (!joint.has_value()) continue;
            if (!visual.equipment.load(plug)) continue;
            if (!visual.equipment.update(*joint,
                                         visual.character.renderCenterX(),
                                         visual.character.renderCenterZ(),
                                         visual.character.renderMinimumY())) {
                visual.equipment.unload();
                continue;
            }
            return true;
        } catch (const std::exception& exception) {
            visual.error = exception.what();
        }
    }
    return false;
}

KoMonsterVisualBank::Visual* KoMonsterVisualBank::ensure(std::uint32_t modelId) {
    if (!ready_ || resolver_ == nullptr || failed_.contains(modelId)) return nullptr;
    if (const auto iterator = visuals_.find(modelId); iterator != visuals_.end()) return iterator->second.get();

    try {
        const auto* look = looks_.find(modelId);
        if (look == nullptr) throw std::runtime_error("NPC_Looks has no mapping for model id " + std::to_string(modelId));
        if (look->characterReference.empty()) {
            throw std::runtime_error("NPC_Looks model has no .n3chr reference: " + std::to_string(modelId));
        }
        const auto characterPath = resolver_->resolve(looksTablePath_, look->characterReference);
        if (!characterPath.has_value()) {
            throw std::runtime_error("Unable to resolve NPC .n3chr for model id " + std::to_string(modelId));
        }

        const content::N3CharacterLoader loader(*resolver_);
        const auto character = loader.load(*characterPath);
        auto visual = std::make_unique<Visual>();
        if (!visual->character.load(character)) throw std::runtime_error(visual->character.error());
        if (!character.animationPath.empty()) {
            visual->animation.configure(content::N3AnimationSet::load(character.animationPath));
        }
        const float initialFrame = visual->animation.ready() ? visual->animation.frame() : 0.0F;
        visual->equipmentReady = attachFirstEquipment(*visual, character, initialFrame);
        auto* result = visual.get();
        visuals_.emplace(modelId, std::move(visual));
        return result;
    } catch (const std::exception& exception) {
        failed_.insert(modelId);
        error_ = exception.what();
        return nullptr;
    }
}

bool KoMonsterVisualBank::update(std::uint32_t modelId, N3AnimationState state, float deltaSeconds) {
    Visual* visual = ensure(modelId);
    if (visual == nullptr) return false;
    if (!updatedThisFrame_.insert(modelId).second) return true;

    float frame = 0.0F;
    if (visual->animation.ready()) {
        const bool restart = state == N3AnimationState::Attack && visual->animation.state() != state;
        visual->animation.setState(state, restart);
        visual->animation.update(deltaSeconds);
        frame = visual->animation.frame();
        if (!visual->character.updateAnimation(frame)) {
            visual->error = visual->character.error();
            return false;
        }
    }

    if (visual->equipmentReady) {
        const auto joint = visual->character.jointWorldMatrix(
            static_cast<std::size_t>(visual->equipment.jointIndex()), frame);
        if (!joint.has_value()
            || !visual->equipment.update(*joint,
                                         visual->character.renderCenterX(),
                                         visual->character.renderCenterZ(),
                                         visual->character.renderMinimumY())) {
            visual->equipmentReady = false;
            visual->error = visual->equipment.error();
        }
    }
    return true;
}

bool KoMonsterVisualBank::draw(std::uint32_t modelId, Vector3 worldPosition, float targetHeight, Color tint) {
    Visual* visual = ensure(modelId);
    if (visual == nullptr || !visual->character.ready()) return false;
    visual->character.draw(worldPosition, targetHeight, tint);
    if (visual->equipmentReady) {
        visual->equipment.draw(worldPosition, targetHeight, visual->character.sourceHeight(), tint);
    }
    return true;
}

void KoMonsterVisualBank::preload(const std::vector<std::uint32_t>& modelIds, std::size_t maximumModels) {
    std::unordered_set<std::uint32_t> unique;
    unique.reserve(std::min(modelIds.size(), maximumModels));
    for (const auto modelId : modelIds) {
        if (unique.size() >= maximumModels) break;
        if (!unique.insert(modelId).second) continue;
        (void) ensure(modelId);
    }
}

} // namespace korework::client
