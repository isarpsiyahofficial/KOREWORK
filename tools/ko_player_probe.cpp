#include "content/asset_catalog.hpp"
#include "content/ko_asset_resolver.hpp"
#include "content/n3_animation.hpp"
#include "content/n3_character.hpp"
#include "content/n3_skeleton.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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

int score(const std::filesystem::path& path) {
    const std::string value = lower(path.generic_string());
    if (containsAny(value, {"/npc/", "\\npc\\", "mob_", "/monster/", "\\monster\\"})) return -10000;
    int result = 0;
    if (value.find("upc") != std::string::npos) result += 100;
    if (value.find("player") != std::string::npos) result += 80;
    if (value.find("character") != std::string::npos) result += 30;
    if (containsAny(value, {"warrior", "fighter", "rogue", "archer", "assassin", "mage", "wizard", "priest", "cleric"})) result += 60;
    if (value.find("item") != std::string::npos) result -= 15;
    return result;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path assetRoot = argc > 1 ? argv[1] : "upstream/ko-assets";
        const auto catalog = korework::content::KoAssetCatalog::scan(assetRoot);
        const korework::content::KoAssetResolver resolver(assetRoot / "game");
        const korework::content::N3CharacterLoader loader(resolver);

        std::vector<std::pair<int, std::filesystem::path>> candidates;
        for (const auto& relative : catalog.characterFiles) {
            const int value = score(relative);
            if (value > -10000) candidates.emplace_back(value, assetRoot / relative);
        }
        std::stable_sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
            if (left.first != right.first) return left.first > right.first;
            return left.second.generic_string() < right.second.generic_string();
        });

        std::string lastError;
        const std::size_t maximumAttempts = std::min<std::size_t>(candidates.size(), 256U);
        for (std::size_t index = 0U; index < maximumAttempts; ++index) {
            try {
                const auto character = loader.load(candidates[index].second);
                if (character.parts.empty() || character.jointPath.empty() || character.animationPath.empty()) continue;
                const auto skeleton = korework::content::N3Skeleton::load(character.jointPath);
                const auto animations = korework::content::N3AnimationSet::load(character.animationPath);
                if (skeleton.joints().empty() || animations.preferredIdle() == nullptr
                    || animations.preferredMove() == nullptr || animations.preferredAttack() == nullptr) continue;
                std::size_t vertices = 0U;
                for (const auto& part : character.parts) for (const auto& lod : part.lods) vertices += lod.bindPositions.size();
                if (vertices == 0U) continue;
                std::cout << "Player character: " << candidates[index].second.generic_string() << '\n'
                          << "Player parts: " << character.parts.size() << '\n'
                          << "Player vertices: " << vertices << '\n'
                          << "Player skeleton joints: " << skeleton.joints().size() << '\n'
                          << "Player animation clips: " << animations.clips().size() << '\n';
                return 0;
            } catch (const std::exception& exception) {
                lastError = exception.what();
            }
        }
        std::cerr << "No usable non-mob N3 player character chain was found after " << maximumAttempts
                  << " candidates." << (lastError.empty() ? "" : " Last error: " + lastError) << '\n';
        return 2;
    } catch (const std::exception& exception) {
        std::cerr << "KO player probe failed: " << exception.what() << '\n';
        return 1;
    }
}
