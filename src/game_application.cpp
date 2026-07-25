#include "game_application.hpp"

#include "client/ko_monster_visual_bank.hpp"
#include "client/ko_player_visual.hpp"
#include "client/offline_skill_vfx.hpp"
#include "client/smd_terrain.hpp"
#include "content/asset_catalog.hpp"
#include "content/smd_map.hpp"
#include "offline_npc_system.hpp"
#include "offline_roster.hpp"
#include "offline_runtime.hpp"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace korework {
namespace {

using client::KoMonsterVisualBank;
using client::KoPlayerVisual;
using client::N3AnimationState;
using client::OfflineSkillVfx;
using client::SmdTerrainModel;
using content::KoAssetCatalog;
using content::SmdMap;

struct ProfileSelection {
    std::size_t slot = 0U;
    CharacterSlot character;
};

float clampFloat(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(value, maximum));
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

Color classColor(PlayerClass playerClass) {
    switch (playerClass) {
        case PlayerClass::Warrior: return Color{184, 76, 52, 255};
        case PlayerClass::Rogue: return Color{77, 145, 78, 255};
        case PlayerClass::Mage: return Color{77, 100, 190, 255};
        case PlayerClass::Priest: return Color{198, 164, 72, 255};
    }
    return GRAY;
}

PlayerClass cycleClass(PlayerClass current, int direction) {
    int index = static_cast<int>(static_cast<std::uint16_t>(current)) - 101;
    index = (index + direction + 4) % 4;
    return static_cast<PlayerClass>(101 + index);
}

void drawCentered(const std::string& text, int y, int fontSize, Color color) {
    DrawText(text.c_str(), (GetScreenWidth() - MeasureText(text.c_str(), fontSize)) / 2, y, fontSize, color);
}

std::optional<ProfileSelection> selectCharacter() {
    OfflineRoster roster;
    std::size_t selected = 0U;
    bool creating = false;
    std::string newName;
    PlayerClass newClass = PlayerClass::Warrior;
    std::string error;

    while (!WindowShouldClose()) {
        if (!creating) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected = (selected + 1U) % OfflineRoster::SlotCount;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected = (selected + OfflineRoster::SlotCount - 1U) % OfflineRoster::SlotCount;
            if (IsKeyPressed(KEY_ENTER)) {
                if (const CharacterSlot* slot = roster.slot(selected); slot != nullptr) return ProfileSelection{selected, *slot};
                creating = true;
                newName.clear();
                newClass = PlayerClass::Warrior;
                error.clear();
            }
            if (IsKeyPressed(KEY_N) && roster.slot(selected) == nullptr) creating = true;
            if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_DELETE) && roster.slot(selected) != nullptr) {
                (void) roster.remove(selected);
            }
            if (IsKeyPressed(KEY_ESCAPE)) return std::nullopt;
        } else {
            int character = GetCharPressed();
            while (character > 0) {
                if (character >= 32 && character <= 126 && newName.size() < 20U) newName.push_back(static_cast<char>(character));
                character = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !newName.empty()) newName.pop_back();
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) newClass = cycleClass(newClass, -1);
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) newClass = cycleClass(newClass, 1);
            if (IsKeyPressed(KEY_ESCAPE)) {
                creating = false;
                error.clear();
            }
            if (IsKeyPressed(KEY_ENTER)) {
                if (roster.create(selected, newName, newClass)) {
                    if (const CharacterSlot* slot = roster.slot(selected); slot != nullptr) return ProfileSelection{selected, *slot};
                } else {
                    error = "Name must be unique and contain 3-20 characters.";
                }
            }
        }

        BeginDrawing();
        ClearBackground(Color{13, 16, 22, 255});
        drawCentered("KOREWORK", 42, 46, Color{224, 189, 92, 255});
        drawCentered("OFFLINE FIRE DRAKE", 94, 20, Color{170, 176, 188, 255});

        const float cardWidth = clampFloat(static_cast<float>(GetScreenWidth()) * 0.24F, 230.0F, 360.0F);
        const float totalWidth = cardWidth * 3.0F + 56.0F;
        const float startX = (static_cast<float>(GetScreenWidth()) - totalWidth) * 0.5F;
        for (std::size_t index = 0; index < OfflineRoster::SlotCount; ++index) {
            const Rectangle card {startX + static_cast<float>(index) * (cardWidth + 28.0F), 170.0F, cardWidth, 330.0F};
            const bool highlighted = index == selected;
            DrawRectangleRounded(card, 0.07F, 8, highlighted ? Color{38, 43, 55, 255} : Color{24, 28, 36, 255});
            DrawRectangleLinesEx(card, highlighted ? 3.0F : 1.0F,
                                 highlighted ? Color{225, 186, 82, 255} : Color{82, 88, 102, 255});
            const CharacterSlot* slot = roster.slot(index);
            if (slot == nullptr) {
                DrawCircle(static_cast<int>(card.x + card.width * 0.5F), static_cast<int>(card.y + 95.0F), 45.0F, Color{50, 54, 65, 255});
                DrawText("EMPTY SLOT", static_cast<int>(card.x + 24.0F), static_cast<int>(card.y + 175.0F), 24, Color{145, 150, 164, 255});
                DrawText("ENTER / N: CREATE", static_cast<int>(card.x + 24.0F), static_cast<int>(card.y + 220.0F), 17, LIGHTGRAY);
            } else {
                const Color tint = classColor(slot->playerClass);
                DrawCircle(static_cast<int>(card.x + card.width * 0.5F), static_cast<int>(card.y + 90.0F), 48.0F, tint);
                DrawRectangle(static_cast<int>(card.x + card.width * 0.5F - 25.0F), static_cast<int>(card.y + 115.0F), 50, 70, tint);
                DrawText(slot->name.c_str(), static_cast<int>(card.x + 22.0F), static_cast<int>(card.y + 205.0F), 26, RAYWHITE);
                DrawText(OfflineRoster::className(slot->playerClass), static_cast<int>(card.x + 22.0F), static_cast<int>(card.y + 244.0F), 21, Color{234, 204, 124, 255});
                const std::string level = "Level " + std::to_string(slot->level);
                DrawText(level.c_str(), static_cast<int>(card.x + 22.0F), static_cast<int>(card.y + 278.0F), 19, LIGHTGRAY);
            }
        }

        if (creating) {
            const Rectangle modal {(GetScreenWidth() - 520.0F) * 0.5F, 535.0F, 520.0F, 185.0F};
            DrawRectangleRounded(modal, 0.05F, 8, Color{8, 10, 14, 245});
            DrawRectangleLinesEx(modal, 2.0F, Color{220, 181, 77, 255});
            DrawText("CREATE CHARACTER", static_cast<int>(modal.x + 20.0F), static_cast<int>(modal.y + 16.0F), 24, Color{238, 210, 132, 255});
            DrawRectangleRounded({modal.x + 20.0F, modal.y + 55.0F, 480.0F, 38.0F}, 0.15F, 5, Color{31, 35, 44, 255});
            DrawText(newName.empty() ? "Type character name..." : newName.c_str(), static_cast<int>(modal.x + 32.0F), static_cast<int>(modal.y + 64.0F), 20,
                     newName.empty() ? Color{130, 135, 148, 255} : RAYWHITE);
            const std::string classLine = std::string("<  ") + OfflineRoster::className(newClass) + "  >";
            DrawText(classLine.c_str(), static_cast<int>(modal.x + 22.0F), static_cast<int>(modal.y + 108.0F), 23, classColor(newClass));
            DrawText("ENTER: CREATE   ESC: CANCEL", static_cast<int>(modal.x + 235.0F), static_cast<int>(modal.y + 113.0F), 16, LIGHTGRAY);
            if (!error.empty()) DrawText(error.c_str(), static_cast<int>(modal.x + 20.0F), static_cast<int>(modal.y + 153.0F), 15, Color{255, 116, 104, 255});
        } else {
            drawCentered("UP/DOWN: SELECT   ENTER: PLAY/CREATE   SHIFT+DELETE: DELETE   ESC: EXIT", GetScreenHeight() - 48, 17, LIGHTGRAY);
        }
        EndDrawing();
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> locateAssetRoot() {
    const std::filesystem::path current = std::filesystem::current_path();
    const std::filesystem::path application = GetApplicationDirectory();
    std::vector<std::filesystem::path> candidates;
    if (const char* environment = std::getenv("KOREWORK_ASSET_ROOT"); environment != nullptr && *environment != '\0') candidates.emplace_back(environment);
    candidates.push_back(current / "assets" / "ko");
    candidates.push_back(current / "upstream" / "ko-assets");
    candidates.push_back(application / "assets" / "ko");
    candidates.push_back(application / "upstream" / "ko-assets");
    candidates.push_back(application.parent_path() / "assets" / "ko");
    candidates.push_back(application.parent_path() / "upstream" / "ko-assets");
    std::error_code error;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_directory(candidate / "game", error)
            && std::filesystem::is_directory(candidate / "server", error)) return std::filesystem::weakly_canonical(candidate, error);
        error.clear();
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> locateDataPack() {
    const std::filesystem::path current = std::filesystem::current_path();
    const std::filesystem::path application = GetApplicationDirectory();
    std::vector<std::filesystem::path> candidates;
    if (const char* environment = std::getenv("KOREWORK_DATA_PACK"); environment != nullptr && *environment != '\0') candidates.emplace_back(environment);
    candidates.push_back(current / "data" / "game_data.kopack");
    candidates.push_back(application / "data" / "game_data.kopack");
    candidates.push_back(application.parent_path() / "data" / "game_data.kopack");
    std::error_code error;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, error)) return std::filesystem::weakly_canonical(candidate, error);
        error.clear();
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> selectMap(const KoAssetCatalog& catalog) {
    const std::array<std::string, 5> priorities {"elmorad_0516.smd", "karus_0516.smd", "free_0810.smd", "battle_0810.smd", "moradon"};
    for (const auto& priority : priorities) {
        for (const auto& relative : catalog.serverMaps) {
            if (lower(relative.filename().string()).find(priority) != std::string::npos) return catalog.root / relative;
        }
    }
    return catalog.serverMaps.empty() ? std::nullopt : std::optional<std::filesystem::path>(catalog.root / catalog.serverMaps.front());
}

float groundHeight(const SmdMap* map, const Vec3& position) {
    if (map == nullptr || !map->loaded()) return 0.0F;
    return map->heightAt(position.x + map->width() * 0.5F, position.z + map->length() * 0.5F);
}

void drawBar(Rectangle rectangle, float value, float maximum, Color fill, const char* label) {
    DrawRectangleRounded(rectangle, 0.22F, 6, Color{10, 12, 16, 225});
    Rectangle filled = rectangle;
    filled.width *= maximum > 0.0F ? clampFloat(value / maximum, 0.0F, 1.0F) : 0.0F;
    DrawRectangleRounded(filled, 0.22F, 6, fill);
    DrawRectangleLinesEx(rectangle, 1.0F, Color{170, 170, 180, 190});
    char text[128] {};
    std::snprintf(text, sizeof(text), "%s %d / %d", label, static_cast<int>(value), static_cast<int>(maximum));
    DrawText(text, static_cast<int>(rectangle.x + 8.0F), static_cast<int>(rectangle.y + 4.0F), 16, RAYWHITE);
}

void drawPlayerFallback(const OfflineRuntime& runtime, float ground) {
    const auto& player = runtime.player();
    const Vector3 base {player.position.x, ground, player.position.z};
    Color tint = classColor(player.playerClass);
    int equipped = 0;
    for (const auto itemId : player.equipmentItemIds) if (itemId != 0U) ++equipped;
    const int brighten = std::min(50, equipped * 7);
    tint.r = static_cast<unsigned char>(std::min(255, static_cast<int>(tint.r) + brighten));
    tint.g = static_cast<unsigned char>(std::min(255, static_cast<int>(tint.g) + brighten));
    tint.b = static_cast<unsigned char>(std::min(255, static_cast<int>(tint.b) + brighten));
    DrawCylinder({base.x, base.y + 0.95F, base.z}, 0.42F, 0.55F, 1.35F, 12, tint);
    DrawSphere({base.x, base.y + 1.88F, base.z}, 0.34F, Color{211, 178, 133, 255});
    if (player.equipmentItemIds[6] != 0U) {
        const auto* weapon = runtime.itemRecord(player.equipmentItemIds[6]);
        const float length = weapon == nullptr ? 1.0F : clampFloat(0.9F + static_cast<float>(std::max<std::int16_t>(0, weapon->damage)) * 0.012F, 1.0F, 2.2F);
        const Color color = player.equipmentUpgradeLevels[6] >= 7U ? ORANGE : LIGHTGRAY;
        DrawCube({base.x + 0.62F, base.y + 1.10F, base.z}, length, 0.08F, 0.13F, color);
    }
    if (player.equipmentItemIds[8] != 0U) DrawCube({base.x - 0.53F, base.y + 1.05F, base.z}, 0.12F, 0.9F, 0.75F, GRAY);
}

void drawPlayer(KoPlayerVisual& visual, const OfflineRuntime& runtime, float ground) {
    const Vector3 position {runtime.player().position.x, ground, runtime.player().position.z};
    if (visual.ready()) visual.draw(position, 2.05F, WHITE);
    else drawPlayerFallback(runtime, ground);
}

void drawFallbackMonster(const MonsterState& monster, const MonsterTemplate& definition, float ground) {
    const Vector3 base {monster.position.x, ground, monster.position.z};
    DrawCylinder({base.x, base.y + 0.62F * definition.scale, base.z}, 0.35F * definition.scale, 0.46F * definition.scale,
                 0.9F * definition.scale, 10, Color{124, 82, 45, 255});
    DrawSphere({base.x, base.y + 1.35F * definition.scale, base.z}, 0.34F * definition.scale, Color{91, 145, 75, 255});
}

void drawMonster(KoMonsterVisualBank& bank, const MonsterState& monster, const MonsterTemplate& definition,
                 bool targeted, float ground) {
    const Vector3 base {monster.position.x, ground, monster.position.z};
    if (!bank.ready() || !bank.draw(definition.modelId, base, 1.85F * definition.scale, WHITE)) drawFallbackMonster(monster, definition, ground);
    if (targeted) DrawCircle3D({base.x, base.y + 0.03F, base.z}, 0.8F * definition.scale, {1.0F, 0.0F, 0.0F}, 90.0F, YELLOW);
    const float ratio = clampFloat(monster.hp / definition.maxHp, 0.0F, 1.0F);
    DrawCube({base.x, base.y + 2.2F * definition.scale, base.z}, 1.4F * definition.scale, 0.08F, 0.08F, MAROON);
    DrawCube({base.x - 0.7F * definition.scale * (1.0F - ratio), base.y + 2.2F * definition.scale, base.z - 0.001F},
             1.4F * definition.scale * ratio, 0.085F, 0.085F, RED);
}

void drawFallbackEnvironment() {
    DrawPlane({0.0F, -0.03F, 0.0F}, {128.0F, 128.0F}, Color{77, 105, 57, 255});
    DrawGrid(64, 2.0F);
}

void drawSkillBar(const OfflineRuntime& runtime) {
    const float size = clampFloat(static_cast<float>(GetScreenWidth()) / 27.5F, 38.0F, 60.0F);
    const float gap = 5.0F;
    const float width = size * 10.0F + gap * 9.0F;
    const float x0 = (GetScreenWidth() - width) * 0.5F;
    const float y = GetScreenHeight() - size - 20.0F;
    const std::array<const char*, 10> keys {"1","2","3","4","5","6","7","8","9","0"};
    for (std::size_t index = 0; index < 10U; ++index) {
        const Rectangle box {x0 + static_cast<float>(index) * (size + gap), y, size, size};
        const auto& skill = runtime.skills()[index];
        DrawRectangleRounded(box, 0.14F, 5, skill.unlocked ? Color{58, 64, 78, 245} : Color{28, 30, 36, 245});
        DrawRectangleLinesEx(box, 1.0F, skill.unlocked ? GOLD : DARKGRAY);
        DrawText(keys[index], static_cast<int>(box.x + 4.0F), static_cast<int>(box.y + 3.0F), 14, RAYWHITE);
        if (skill.unlocked) DrawCircle(static_cast<int>(box.x + size * 0.5F), static_cast<int>(box.y + size * 0.52F), size * 0.23F,
                                      index < 3U ? RED : BLUE);
        if (runtime.cooldowns()[index] > 0.0F) {
            const float ratio = clampFloat(runtime.cooldowns()[index] / std::max(0.01F, skill.cooldown), 0.0F, 1.0F);
            DrawRectangle(static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(size), static_cast<int>(size * ratio), Fade(BLACK, 0.70F));
        }
    }
}

void processInventory(OfflineRuntime& runtime, std::size_t& selected) {
    if (runtime.inventory().empty()) {
        selected = 0U;
        return;
    }
    if (IsKeyPressed(KEY_DOWN)) selected = (selected + 1U) % runtime.inventory().size();
    if (IsKeyPressed(KEY_UP)) selected = (selected + runtime.inventory().size() - 1U) % runtime.inventory().size();
    if (IsKeyPressed(KEY_E)) {
        (void) runtime.equipInventory(selected);
        selected = runtime.inventory().empty() ? 0U : std::min(selected, runtime.inventory().size() - 1U);
    }
    if (IsKeyPressed(KEY_U)) (void) runtime.upgradeInventory(selected);
}

void drawInventory(const OfflineRuntime& runtime, std::size_t selected) {
    const Rectangle panel {GetScreenWidth() - 520.0F, 95.0F, 500.0F, GetScreenHeight() - 200.0F};
    DrawRectangleRounded(panel, 0.04F, 8, Color{8, 11, 16, 242});
    DrawRectangleLinesEx(panel, 2.0F, GOLD);
    DrawText("INVENTORY / EQUIPMENT", static_cast<int>(panel.x + 18.0F), static_cast<int>(panel.y + 17.0F), 23, LIGHTGRAY);
    const std::array<const char*, 14> slotNames {"ER","HD","EL","NK","UP","SH","RH","BT","LH","RR","LW","RL","GL","FT"};
    for (std::size_t slot = 0; slot < 14U; ++slot) {
        const Rectangle box {panel.x + 18.0F + static_cast<float>(slot % 7U) * 48.0F,
                             panel.y + 55.0F + static_cast<float>(slot / 7U) * 48.0F, 42.0F, 42.0F};
        DrawRectangleRounded(box, 0.10F, 4, Color{35, 39, 48, 255});
        DrawText(slotNames[slot], static_cast<int>(box.x + 4.0F), static_cast<int>(box.y + 3.0F), 11, LIGHTGRAY);
        if (runtime.player().equipmentItemIds[slot] != 0U) {
            DrawCircle(static_cast<int>(box.x + 21.0F), static_cast<int>(box.y + 25.0F), 11.0F, BROWN);
            const std::string upgrade = "+" + std::to_string(runtime.player().equipmentUpgradeLevels[slot]);
            DrawText(upgrade.c_str(), static_cast<int>(box.x + 24.0F), static_cast<int>(box.y + 27.0F), 10, YELLOW);
        }
    }
    int y = static_cast<int>(panel.y + 170.0F);
    for (std::size_t index = 0; index < runtime.inventory().size() && y < panel.y + panel.height - 65.0F; ++index, y += 29) {
        const Rectangle row {panel.x + 18.0F, static_cast<float>(y), panel.width - 36.0F, 26.0F};
        DrawRectangleRounded(row, 0.06F, 4, index == selected ? Color{80, 68, 39, 255} : Color{27, 31, 39, 255});
        const auto& entry = runtime.inventory()[index];
        const std::string label = entry.name + (entry.upgradeLevel > 0U ? " +" + std::to_string(entry.upgradeLevel) : "")
            + " x" + std::to_string(entry.count);
        DrawText(label.c_str(), static_cast<int>(row.x + 7.0F), static_cast<int>(row.y + 4.0F), 16, RAYWHITE);
    }
    DrawText("UP/DOWN select | E equip | U upgrade", static_cast<int>(panel.x + 18.0F), static_cast<int>(panel.y + panel.height - 38.0F), 15, GOLD);
}

void drawStats(const OfflineRuntime& runtime) {
    const auto& player = runtime.player();
    const Rectangle panel {18.0F, 165.0F, 330.0F, 235.0F};
    DrawRectangleRounded(panel, 0.05F, 6, Color{7, 10, 15, 232});
    DrawRectangleLinesEx(panel, 2.0F, classColor(player.playerClass));
    DrawText("CHARACTER STATS", 34, 181, 22, GOLD);
    const std::array<std::pair<const char*, int>, 5> stats {{{"F1 STR",player.strength},{"F2 STA",player.stamina},{"F3 DEX",player.dexterity},{"F4 INT",player.intelligence},{"F5 MP",player.magicPower}}};
    for (std::size_t index = 0; index < stats.size(); ++index) {
        const std::string line = std::string(stats[index].first) + "  " + std::to_string(stats[index].second);
        DrawText(line.c_str(), 38, 222 + static_cast<int>(index) * 28, 18, RAYWHITE);
    }
    DrawText(("Points: " + std::to_string(player.bonusPoints)).c_str(), 190, 222, 17, YELLOW);
    DrawText(("Attack: " + std::to_string(player.attackPower)).c_str(), 190, 258, 17, LIGHTGRAY);
    DrawText(("Defense: " + std::to_string(player.defensePower)).c_str(), 190, 289, 17, LIGHTGRAY);
}

void drawHud(const OfflineRuntime& runtime, std::optional<std::size_t> target, bool inventoryOpen,
             bool statsOpen, std::size_t inventorySelection, const std::string& status) {
    const auto& player = runtime.player();
    drawBar({18.0F, 18.0F, 310.0F, 26.0F}, player.hp, player.maxHp, RED, "HP");
    drawBar({18.0F, 49.0F, 310.0F, 24.0F}, player.mp, player.maxMp, BLUE, "MP");
    const std::string identity = player.name + " | " + OfflineRoster::className(player.playerClass) + " | Level " + std::to_string(player.level);
    DrawText(identity.c_str(), 20, 80, 19, GOLD);
    DrawText(("EXP " + std::to_string(player.exp) + " | Noah " + std::to_string(player.gold)
              + " | ATK " + std::to_string(player.attackPower) + " | AC " + std::to_string(player.defensePower)).c_str(), 20, 107, 16, RAYWHITE);
    DrawText(status.c_str(), 20, 133, 14, LIGHTGRAY);
    if (target.has_value()) {
        const auto& monster = runtime.monsters()[*target];
        const auto& definition = runtime.monsterTemplates()[monster.templateIndex];
        const float width = 420.0F;
        const float x = (GetScreenWidth() - width) * 0.5F;
        DrawText(definition.name.c_str(), static_cast<int>(x), 18, 20, GOLD);
        drawBar({x, 44.0F, width, 23.0F}, monster.hp, definition.maxHp, RED, "");
    }
    int y = GetScreenHeight() - 240;
    for (const auto& line : runtime.log()) {
        DrawText(line.c_str(), 18, y, 15, LIGHTGRAY);
        y += 18;
        if (y > GetScreenHeight() - 100) break;
    }
    if (inventoryOpen) drawInventory(runtime, inventorySelection);
    if (statsOpen) drawStats(runtime);
    drawSkillBar(runtime);
    DrawText("WASD move | 1-0 skill | F interact | I inventory | C stats | F5 save | ESC select", 18, GetScreenHeight() - 18, 14, LIGHTGRAY);
}

bool runWorld(const ProfileSelection& profile) {
    SmdMap worldMap;
    SmdTerrainModel terrain;
    KoMonsterVisualBank visualBank;
    KoPlayerVisual playerVisual;
    OfflineSkillVfx skillVfx;
    std::string status = "Fallback content mode";

    const auto assetRoot = locateAssetRoot();
    if (assetRoot.has_value()) {
        try {
            const auto catalog = KoAssetCatalog::scan(*assetRoot);
            const auto mapPath = selectMap(catalog);
            const bool mapReady = mapPath.has_value() && worldMap.load(*mapPath) && terrain.load(worldMap);
            const bool visualsReady = visualBank.initialize(*assetRoot);
            const bool playerReady = playerVisual.initialize(*assetRoot, profile.character.playerClass);
            status = std::string(mapReady ? "REAL SMD OK" : "SMD FAIL") + " | "
                + (visualsReady ? "NPC_LOOKS/N3 OK" : "N3 FAIL") + " | "
                + (playerReady ? "PLAYER N3 " + playerVisual.sourceName() : "PLAYER N3 FAIL")
                + (mapPath.has_value() ? " | " + mapPath->filename().string() : "");
        } catch (const std::exception& exception) {
            status = std::string("Content error: ") + exception.what();
        }
    }

    OfflineRuntime runtime;
    runtime.configureProfile(profile.slot, profile.character.name, profile.character.playerClass);
    if (const auto dataPack = locateDataPack(); dataPack.has_value()) runtime.initialize(*dataPack); else runtime.initialize();
    runtime.refreshSkills();

    const SmdMap* activeMap = terrain.ready() ? &worldMap : nullptr;
    OfflineNpcSystem npcSystem(profile.slot);
    npcSystem.initialize(runtime, activeMap);

    if (visualBank.ready()) {
        std::vector<std::uint32_t> modelIds;
        for (const auto& definition : runtime.monsterTemplates()) if (definition.modelId != 0U) modelIds.push_back(definition.modelId);
        visualBank.preload(modelIds, 48U);
        status += " | Models " + std::to_string(visualBank.loadedCount());
    }
    (void) playerVisual.setWeapon(runtime.itemRecord(runtime.player().equipmentItemIds[6]));

    Camera3D camera {};
    camera.up = {0.0F, 1.0F, 0.0F};
    camera.fovy = 52.0F;
    camera.projection = CAMERA_PERSPECTIVE;
    float yaw = 0.75F;
    float pitch = 0.42F;
    float distance = 10.5F;
    bool inventoryOpen = false;
    bool statsOpen = false;
    std::size_t inventorySelection = 0U;
    const std::array<int, 10> skillKeys {KEY_ONE,KEY_TWO,KEY_THREE,KEY_FOUR,KEY_FIVE,KEY_SIX,KEY_SEVEN,KEY_EIGHT,KEY_NINE,KEY_ZERO};

    while (!WindowShouldClose()) {
        const float delta = std::min(GetFrameTime(), 0.05F);
        const bool npcWasOpen = npcSystem.modalOpen();
        npcSystem.update(runtime, activeMap);
        if (IsKeyPressed(KEY_ESCAPE) && !npcWasOpen) {
            runtime.save();
            OfflineRoster roster;
            (void) roster.updateLevel(profile.slot, runtime.player().level);
            return true;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            const Vector2 mouse = GetMouseDelta();
            yaw -= mouse.x * 0.004F;
            pitch = clampFloat(pitch - mouse.y * 0.003F, 0.12F, 1.05F);
        }
        distance = clampFloat(distance - GetMouseWheelMove() * 0.8F, 5.5F, 17.0F);

        const bool modal = inventoryOpen || statsOpen || npcSystem.modalOpen();
        const Vector3 forward {std::sin(yaw), 0.0F, std::cos(yaw)};
        const Vector3 right {std::cos(yaw), 0.0F, -std::sin(yaw)};
        Vector3 movement {};
        if (!modal) {
            if (IsKeyDown(KEY_W)) movement = Vector3Add(movement, forward);
            if (IsKeyDown(KEY_S)) movement = Vector3Subtract(movement, forward);
            if (IsKeyDown(KEY_D)) movement = Vector3Add(movement, right);
            if (IsKeyDown(KEY_A)) movement = Vector3Subtract(movement, right);
        }
        const bool playerMoved = Vector3LengthSqr(movement) > 0.001F;
        if (playerMoved) {
            const Vec3 previous = runtime.player().position;
            movement = Vector3Scale(Vector3Normalize(movement), (IsKeyDown(KEY_LEFT_SHIFT) ? 6.0F : 4.0F) * delta);
            runtime.movePlayer({movement.x, 0.0F, movement.z});
            runtime.player().position = npcSystem.constrainToMap(activeMap, runtime.player().position, previous);
        }

        const auto target = runtime.nearestAliveMonster(24.0F);
        bool playerAttacked = false;
        if (!modal) {
            for (std::size_t index = 0; index < skillKeys.size(); ++index) {
                if (!IsKeyPressed(skillKeys[index])) continue;
                const SkillDefinition skill = runtime.skills()[index];
                Vec3 source = runtime.player().position;
                source.y = groundHeight(activeMap, source);
                Vec3 destination = source;
                if (target.has_value()) {
                    destination = runtime.monsters()[*target].position;
                    destination.y = groundHeight(activeMap, destination);
                }
                if (runtime.useSkill(index, target)) {
                    skillVfx.spawn(skill, runtime.player().playerClass, source, destination);
                    playerAttacked = skill.damage > 0.0F;
                }
            }
        }
        if (!npcSystem.modalOpen() && IsKeyPressed(KEY_I)) {
            inventoryOpen = !inventoryOpen;
            statsOpen = false;
        }
        if (!npcSystem.modalOpen() && IsKeyPressed(KEY_C)) {
            statsOpen = !statsOpen;
            inventoryOpen = false;
        }
        if (inventoryOpen) processInventory(runtime, inventorySelection);
        if (statsOpen) {
            const std::array<int, 5> keys {KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5};
            for (std::size_t index = 0; index < keys.size(); ++index) if (IsKeyPressed(keys[index])) runtime.spendStatPoint(index);
        }
        if (IsKeyPressed(KEY_F5) && !statsOpen) runtime.save();

        std::vector<Vec3> previousMonsterPositions;
        previousMonsterPositions.reserve(runtime.monsters().size());
        for (const auto& monster : runtime.monsters()) previousMonsterPositions.push_back(monster.position);
        if (!modal) runtime.update(delta);

        visualBank.beginFrame();
        for (std::size_t index = 0; index < runtime.monsters().size(); ++index) {
            const auto& monster = runtime.monsters()[index];
            if (!monster.alive) continue;
            N3AnimationState animation = N3AnimationState::Idle;
            const float dx = monster.position.x - previousMonsterPositions[index].x;
            const float dz = monster.position.z - previousMonsterPositions[index].z;
            if (monster.attackCooldown > 1.02F) animation = N3AnimationState::Attack;
            else if (dx * dx + dz * dz > 0.000001F) animation = N3AnimationState::Move;
            (void) visualBank.update(runtime.monsterTemplates()[monster.templateIndex].modelId, animation, delta);
        }

        (void) playerVisual.setWeapon(runtime.itemRecord(runtime.player().equipmentItemIds[6]));
        playerVisual.update(playerAttacked ? N3AnimationState::Attack : playerMoved ? N3AnimationState::Move : N3AnimationState::Idle, delta);
        skillVfx.update(delta);

        const Vec3& player = runtime.player().position;
        const float ground = groundHeight(activeMap, player);
        const float horizontal = std::cos(pitch) * distance;
        camera.target = {player.x, ground + 1.15F, player.z};
        camera.position = {player.x - std::sin(yaw) * horizontal,
                           ground + 1.8F + std::sin(pitch) * distance,
                           player.z - std::cos(yaw) * horizontal};

        BeginDrawing();
        ClearBackground(Color{133, 170, 198, 255});
        BeginMode3D(camera);
        if (terrain.ready()) terrain.draw(); else drawFallbackEnvironment();
        npcSystem.drawWorld(activeMap);
        drawPlayer(playerVisual, runtime, ground);
        for (std::size_t index = 0; index < runtime.monsters().size(); ++index) {
            const auto& monster = runtime.monsters()[index];
            if (!monster.alive) continue;
            drawMonster(visualBank, monster, runtime.monsterTemplates()[monster.templateIndex],
                        target.has_value() && *target == index, groundHeight(activeMap, monster.position));
        }
        skillVfx.draw();
        EndMode3D();
        drawHud(runtime, target, inventoryOpen, statsOpen, inventorySelection, status);
        npcSystem.drawUi(runtime);
        DrawFPS(GetScreenWidth() - 90, GetScreenHeight() - 28);
        EndDrawing();
    }

    runtime.save();
    OfflineRoster roster;
    (void) roster.updateLevel(profile.slot, runtime.player().level);
    return false;
}

} // namespace

int runGameApplication() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1600, 900, "KOREWORK - Offline Fire Drake Rework");
    SetWindowMinSize(960, 540);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    while (!WindowShouldClose()) {
        const auto profile = selectCharacter();
        if (!profile.has_value()) break;
        if (!runWorld(*profile)) break;
    }
    CloseWindow();
    return 0;
}

} // namespace korework
