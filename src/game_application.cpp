#include "game_application.hpp"

#include "client/ko_monster_visual_bank.hpp"
#include "client/smd_terrain.hpp"
#include "content/asset_catalog.hpp"
#include "content/smd_map.hpp"
#include "offline_roster.hpp"
#include "offline_runtime.hpp"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace korework {
namespace {

using client::KoMonsterVisualBank;
using client::N3AnimationState;
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
            if (IsKeyPressed(KEY_N) && roster.slot(selected) == nullptr) {
                creating = true;
                newName.clear();
                newClass = PlayerClass::Warrior;
                error.clear();
            }
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
                    error = "Isim 3-20 karakter olmali ve diger slotlardan farkli olmali.";
                }
            }
        }

        BeginDrawing();
        ClearBackground(Color{13, 16, 22, 255});
        drawCentered("KOREWORK", 42, 46, Color{224, 189, 92, 255});
        drawCentered("OFFLINE FIRE DRAKE", 94, 20, Color{170, 176, 188, 255});

        const float cardWidth = clampFloat(static_cast<float>(GetScreenWidth()) * 0.24F, 230.0F, 360.0F);
        const float totalWidth = cardWidth * 3.0F + 28.0F * 2.0F;
        const float startX = (static_cast<float>(GetScreenWidth()) - totalWidth) * 0.5F;
        const float cardY = 170.0F;
        const float cardHeight = 330.0F;
        for (std::size_t index = 0; index < OfflineRoster::SlotCount; ++index) {
            const Rectangle card {startX + static_cast<float>(index) * (cardWidth + 28.0F), cardY, cardWidth, cardHeight};
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
            && std::filesystem::is_directory(candidate / "server", error)) {
            return std::filesystem::weakly_canonical(candidate, error);
        }
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
    candidates.push_back(current / "game_data.kopack");
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
    if (!catalog.serverMaps.empty()) return catalog.root / catalog.serverMaps.front();
    return std::nullopt;
}

float groundHeight(const SmdMap* map, const Vec3& position) {
    if (map == nullptr || !map->loaded()) return 0.0F;
    return map->heightAt(position.x + map->width() * 0.5F, position.z + map->length() * 0.5F);
}

void drawProgressBar(Rectangle rectangle, float value, float maximum, Color fill, const char* label) {
    DrawRectangleRounded(rectangle, 0.22F, 6, Color{10, 12, 16, 225});
    DrawRectangleLinesEx(rectangle, 1.0F, Color{170, 170, 180, 190});
    Rectangle filled = rectangle;
    filled.width *= maximum > 0.0F ? clampFloat(value / maximum, 0.0F, 1.0F) : 0.0F;
    DrawRectangleRounded(filled, 0.22F, 6, fill);
    char text[128] {};
    std::snprintf(text, sizeof(text), "%s %d / %d", label, static_cast<int>(value), static_cast<int>(maximum));
    DrawText(text, static_cast<int>(rectangle.x + 8.0F), static_cast<int>(rectangle.y + 4.0F), 16, RAYWHITE);
}

void drawPlayer(const OfflineRuntime& runtime, float ground) {
    const auto& player = runtime.player();
    const Vector3 base {player.position.x, ground, player.position.z};
    Color tint = classColor(player.playerClass);
    int armorPieces = 0;
    int totalUpgrade = 0;
    for (std::size_t slot = 0; slot < player.equipmentItemIds.size(); ++slot) {
        if (player.equipmentItemIds[slot] != 0U) {
            ++armorPieces;
            totalUpgrade += player.equipmentUpgradeLevels[slot];
        }
    }
    const int brightness = std::min(55, armorPieces * 6 + totalUpgrade * 2);
    tint.r = static_cast<unsigned char>(std::min(255, static_cast<int>(tint.r) + brightness));
    tint.g = static_cast<unsigned char>(std::min(255, static_cast<int>(tint.g) + brightness));
    tint.b = static_cast<unsigned char>(std::min(255, static_cast<int>(tint.b) + brightness));

    DrawCylinder({base.x, base.y + 0.95F, base.z}, 0.42F, 0.55F, 1.35F, 12, tint);
    DrawSphere({base.x, base.y + 1.88F, base.z}, 0.34F, Color{211, 178, 133, 255});
    if (armorPieces > 0) DrawCylinderWires({base.x, base.y + 0.95F, base.z}, 0.45F, 0.58F, 1.40F, 12, Color{235, 224, 190, 255});

    const std::uint32_t weaponId = player.equipmentItemIds[6];
    if (weaponId != 0U) {
        const auto* weapon = runtime.itemRecord(weaponId);
        const float length = weapon == nullptr ? 1.0F : clampFloat(0.9F + static_cast<float>(std::max<std::int16_t>(0, weapon->damage)) * 0.012F, 1.0F, 2.2F);
        const Color weaponColor = player.equipmentUpgradeLevels[6] >= 7U ? Color{255, 180, 68, 255} : Color{215, 218, 226, 255};
        DrawCube({base.x + 0.62F, base.y + 1.10F, base.z}, length, 0.08F, 0.13F, weaponColor);
    }
    if (player.equipmentItemIds[8] != 0U) {
        DrawCylinder({base.x - 0.53F, base.y + 1.05F, base.z}, 0.42F, 0.42F, 0.10F, 16, Color{105, 115, 135, 255});
    }
}

void drawFallbackMonster(const MonsterState& monster, const MonsterTemplate& definition, float ground) {
    const Vector3 base {monster.position.x, ground, monster.position.z};
    const float scale = definition.scale;
    DrawCylinder({base.x, base.y + 0.62F * scale, base.z}, 0.35F * scale, 0.46F * scale, 0.9F * scale, 10, Color{124, 82, 45, 255});
    DrawSphere({base.x, base.y + 1.35F * scale, base.z}, 0.34F * scale, Color{91, 145, 75, 255});
}

void drawMonster(KoMonsterVisualBank& visualBank,
                 const MonsterState& monster,
                 const MonsterTemplate& definition,
                 bool targeted,
                 float ground) {
    const Vector3 base {monster.position.x, ground, monster.position.z};
    const float targetHeight = 1.85F * definition.scale;
    if (!visualBank.ready() || !visualBank.draw(definition.modelId, base, targetHeight, WHITE)) drawFallbackMonster(monster, definition, ground);
    if (targeted) DrawCircle3D({base.x, base.y + 0.03F, base.z}, 0.8F * definition.scale, {1.0F, 0.0F, 0.0F}, 90.0F, Color{255, 190, 40, 225});
    const float ratio = clampFloat(monster.hp / definition.maxHp, 0.0F, 1.0F);
    const float barHeight = 2.2F * definition.scale;
    DrawCube({base.x, base.y + barHeight, base.z}, 1.4F * definition.scale, 0.08F, 0.08F, Color{35, 12, 12, 255});
    DrawCube({base.x - (1.4F * definition.scale * (1.0F - ratio)) * 0.5F, base.y + barHeight, base.z - 0.001F},
             1.4F * definition.scale * ratio, 0.085F, 0.085F, Color{185, 35, 35, 255});
}

void drawFallbackEnvironment() {
    DrawPlane({0.0F, -0.03F, 0.0F}, {128.0F, 128.0F}, Color{77, 105, 57, 255});
    DrawGrid(64, 2.0F);
}

void drawSkillBar(const OfflineRuntime& runtime) {
    const float slotSize = clampFloat(static_cast<float>(GetScreenWidth()) / 27.5F, 38.0F, 62.0F);
    const float gap = clampFloat(slotSize * 0.09F, 3.0F, 6.0F);
    const float totalWidth = slotSize * 10.0F + gap * 9.0F;
    const float startX = (static_cast<float>(GetScreenWidth()) - totalWidth) * 0.5F;
    const float startY = static_cast<float>(GetScreenHeight()) - slotSize - 18.0F;
    const std::array<const char*, 10> keys {"1","2","3","4","5","6","7","8","9","0"};
    DrawRectangleRounded({startX - 8.0F, startY - 8.0F, totalWidth + 16.0F, slotSize + 16.0F}, 0.12F, 6, Color{8, 10, 14, 215});
    for (std::size_t index = 0; index < 10U; ++index) {
        const float x = startX + static_cast<float>(index) * (slotSize + gap);
        const Rectangle rectangle {x, startY, slotSize, slotSize};
        const auto& skill = runtime.skills()[index];
        const float cooldown = runtime.cooldowns()[index];
        DrawRectangleRounded(rectangle, 0.16F, 5, skill.unlocked ? Color{67, 72, 88, 245} : Color{33, 34, 39, 245});
        DrawRectangleLinesEx(rectangle, 1.5F, skill.unlocked ? Color{205, 171, 75, 255} : Color{80, 80, 85, 255});
        if (skill.unlocked) DrawCircle(static_cast<int>(x + slotSize * 0.5F), static_cast<int>(startY + slotSize * 0.52F), slotSize * 0.28F,
                                      index < 3 ? Color{180, 61, 41, 255} : Color{52, 112, 164, 255});
        if (cooldown > 0.0F) {
            const float ratio = clampFloat(cooldown / std::max(0.01F, skill.cooldown), 0.0F, 1.0F);
            DrawRectangle(static_cast<int>(x), static_cast<int>(startY), static_cast<int>(slotSize), static_cast<int>(slotSize * ratio), Color{5, 6, 9, 185});
        }
        DrawText(keys[index], static_cast<int>(x + 4.0F), static_cast<int>(startY + 3.0F), static_cast<int>(slotSize * 0.23F), Color{255, 230, 145, 255});
        std::string shortName = skill.name.substr(0, std::min<std::size_t>(skill.name.size(), 7U));
        DrawText(shortName.c_str(), static_cast<int>(x + 3.0F), static_cast<int>(startY + slotSize - 14.0F), 10, RAYWHITE);
    }
}

void drawMinimap(const OfflineRuntime& runtime, const SmdMap* map) {
    const float size = clampFloat(static_cast<float>(GetScreenHeight()) * 0.20F, 130.0F, 210.0F);
    const Rectangle panel {static_cast<float>(GetScreenWidth()) - size - 18.0F, 18.0F, size, size};
    DrawRectangleRounded(panel, 0.08F, 6, Color{7, 12, 13, 215});
    DrawRectangleLinesEx(panel, 2.0F, Color{160, 143, 78, 230});
    DrawText(map != nullptr ? "FIRE DRAKE SMD" : "FALLBACK", static_cast<int>(panel.x + 10.0F), static_cast<int>(panel.y + 8.0F), 14, Color{238, 219, 153, 255});
    const auto project = [&panel, map](const Vec3& position) {
        if (map != nullptr) {
            return Vector2{panel.x + clampFloat((position.x + map->width() * 0.5F) / map->width(), 0.0F, 1.0F) * panel.width,
                           panel.y + clampFloat((position.z + map->length() * 0.5F) / map->length(), 0.0F, 1.0F) * panel.height};
        }
        return Vector2{panel.x + panel.width * 0.5F + position.x, panel.y + panel.height * 0.5F + position.z};
    };
    for (const auto& monster : runtime.monsters()) if (monster.alive) DrawCircleV(project(monster.position), 2.5F, RED);
    DrawCircleV(project(runtime.player().position), 4.0F, YELLOW);
}

Rectangle inventoryPanelRectangle() {
    const float width = clampFloat(static_cast<float>(GetScreenWidth()) * 0.34F, 430.0F, 620.0F);
    return {static_cast<float>(GetScreenWidth()) - width - 18.0F, 90.0F, width, static_cast<float>(GetScreenHeight()) - 190.0F};
}

void processInventoryInput(OfflineRuntime& runtime, std::size_t& selection) {
    if (!runtime.inventory().empty()) {
        if (IsKeyPressed(KEY_DOWN)) selection = (selection + 1U) % runtime.inventory().size();
        if (IsKeyPressed(KEY_UP)) selection = (selection + runtime.inventory().size() - 1U) % runtime.inventory().size();
        if (IsKeyPressed(KEY_E)) {
            (void) runtime.equipInventory(selection);
            if (!runtime.inventory().empty()) selection = std::min(selection, runtime.inventory().size() - 1U);
            else selection = 0U;
        }
        if (IsKeyPressed(KEY_U)) (void) runtime.upgradeInventory(selection);
    }

    const Rectangle panel = inventoryPanelRectangle();
    const float equipmentY = panel.y + 58.0F;
    const float cell = 42.0F;
    const float gap = 5.0F;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = GetMousePosition();
        for (std::size_t slot = 0; slot < 14U; ++slot) {
            const Rectangle rectangle {panel.x + 18.0F + static_cast<float>(slot % 7U) * (cell + gap),
                                       equipmentY + static_cast<float>(slot / 7U) * (cell + gap), cell, cell};
            if (CheckCollisionPointRec(mouse, rectangle) && runtime.player().equipmentItemIds[slot] != 0U) {
                (void) runtime.unequip(slot);
                break;
            }
        }
        const float listY = equipmentY + 112.0F;
        for (std::size_t index = 0; index < runtime.inventory().size(); ++index) {
            const Rectangle row {panel.x + 18.0F, listY + static_cast<float>(index) * 30.0F, panel.width - 36.0F, 27.0F};
            if (row.y + row.height > panel.y + panel.height - 16.0F) break;
            if (CheckCollisionPointRec(mouse, row)) {
                selection = index;
                break;
            }
        }
    }
}

void drawInventoryPanel(const OfflineRuntime& runtime, std::size_t selection) {
    const Rectangle panel = inventoryPanelRectangle();
    DrawRectangleRounded(panel, 0.04F, 8, Color{8, 11, 16, 242});
    DrawRectangleLinesEx(panel, 2.0F, Color{196, 161, 71, 255});
    DrawText("INVENTORY & EQUIPMENT", static_cast<int>(panel.x + 18.0F), static_cast<int>(panel.y + 18.0F), 23, Color{239, 214, 143, 255});
    const float equipmentY = panel.y + 58.0F;
    const float cell = 42.0F;
    const float gap = 5.0F;
    const std::array<const char*, 14> slotNames {"ER","HD","EL","NK","UP","SH","RH","BT","LH","RR","LW","RL","GL","FT"};
    for (std::size_t slot = 0; slot < 14U; ++slot) {
        const Rectangle rectangle {panel.x + 18.0F + static_cast<float>(slot % 7U) * (cell + gap),
                                   equipmentY + static_cast<float>(slot / 7U) * (cell + gap), cell, cell};
        DrawRectangleRounded(rectangle, 0.12F, 5, Color{38, 42, 52, 255});
        DrawRectangleLinesEx(rectangle, 1.0F, Color{105, 110, 124, 255});
        DrawText(slotNames[slot], static_cast<int>(rectangle.x + 4.0F), static_cast<int>(rectangle.y + 3.0F), 11, LIGHTGRAY);
        if (runtime.player().equipmentItemIds[slot] != 0U) {
            DrawCircle(static_cast<int>(rectangle.x + rectangle.width * 0.5F), static_cast<int>(rectangle.y + rectangle.height * 0.58F), 12.0F, Color{175, 135, 54, 255});
            if (runtime.player().equipmentUpgradeLevels[slot] > 0U) {
                const std::string upgrade = "+" + std::to_string(runtime.player().equipmentUpgradeLevels[slot]);
                DrawText(upgrade.c_str(), static_cast<int>(rectangle.x + 23.0F), static_cast<int>(rectangle.y + 25.0F), 11, YELLOW);
            }
        }
    }

    const float listY = equipmentY + 112.0F;
    DrawText("ITEMS", static_cast<int>(panel.x + 18.0F), static_cast<int>(listY - 27.0F), 18, LIGHTGRAY);
    for (std::size_t index = 0; index < runtime.inventory().size(); ++index) {
        const Rectangle row {panel.x + 18.0F, listY + static_cast<float>(index) * 30.0F, panel.width - 36.0F, 27.0F};
        if (row.y + row.height > panel.y + panel.height - 72.0F) break;
        DrawRectangleRounded(row, 0.08F, 4, index == selection ? Color{75, 66, 42, 255} : Color{28, 32, 40, 255});
        const auto& entry = runtime.inventory()[index];
        std::string label = entry.name;
        if (entry.upgradeLevel > 0U) label += " +" + std::to_string(entry.upgradeLevel);
        label += " x" + std::to_string(entry.count);
        DrawText(label.c_str(), static_cast<int>(row.x + 8.0F), static_cast<int>(row.y + 5.0F), 16, RAYWHITE);
    }

    if (!runtime.inventory().empty() && selection < runtime.inventory().size()) {
        const auto& entry = runtime.inventory()[selection];
        const auto* item = runtime.itemRecord(entry.itemId);
        const float detailY = panel.y + panel.height - 62.0F;
        std::string detail = "E: EQUIP   U: UPGRADE";
        if (item != nullptr) detail += "   DMG " + std::to_string(item->damage) + "   AC " + std::to_string(item->armor);
        DrawText(detail.c_str(), static_cast<int>(panel.x + 18.0F), static_cast<int>(detailY), 15, Color{226, 199, 120, 255});
    }
    DrawText("Click equipped slot to unequip", static_cast<int>(panel.x + 18.0F), static_cast<int>(panel.y + panel.height - 32.0F), 14, LIGHTGRAY);
}

void drawStatsPanel(const OfflineRuntime& runtime) {
    const auto& player = runtime.player();
    const Rectangle panel {18.0F, 155.0F, 330.0F, 235.0F};
    DrawRectangleRounded(panel, 0.05F, 6, Color{7, 10, 15, 232});
    DrawRectangleLinesEx(panel, 2.0F, classColor(player.playerClass));
    DrawText("CHARACTER STATS", 34, 171, 22, Color{239, 214, 143, 255});
    const std::array<std::pair<const char*, int>, 5> stats {{{"F1 STR",player.strength},{"F2 STA",player.stamina},{"F3 DEX",player.dexterity},{"F4 INT",player.intelligence},{"F5 MP",player.magicPower}}};
    for (std::size_t index = 0; index < stats.size(); ++index) {
        const std::string line = std::string(stats[index].first) + "  " + std::to_string(stats[index].second);
        DrawText(line.c_str(), 38, 211 + static_cast<int>(index) * 28, 18, RAYWHITE);
    }
    DrawText(("Bonus points: " + std::to_string(player.bonusPoints)).c_str(), 190, 211, 17, YELLOW);
    DrawText(("Attack: " + std::to_string(player.attackPower)).c_str(), 190, 247, 17, LIGHTGRAY);
    DrawText(("Defense: " + std::to_string(player.defensePower)).c_str(), 190, 278, 17, LIGHTGRAY);
}

void drawHud(const OfflineRuntime& runtime,
             std::optional<std::size_t> target,
             bool inventoryOpen,
             bool statsOpen,
             std::size_t inventorySelection,
             const SmdMap* map,
             const std::string& status) {
    const auto& player = runtime.player();
    drawProgressBar({18.0F, 18.0F, 310.0F, 26.0F}, player.hp, player.maxHp, Color{170, 32, 39, 255}, "HP");
    drawProgressBar({18.0F, 49.0F, 310.0F, 24.0F}, player.mp, player.maxMp, Color{36, 79, 171, 255}, "MP");
    const std::string identity = player.name + " | " + OfflineRoster::className(player.playerClass) + " | Level " + std::to_string(player.level);
    DrawText(identity.c_str(), 20, 80, 19, Color{246, 226, 166, 255});
    const std::string economy = "EXP " + std::to_string(player.exp) + "/" + std::to_string(std::max(1000, player.level * player.level * 500))
        + "   Noah " + std::to_string(player.gold) + "   ATK " + std::to_string(player.attackPower) + "   AC " + std::to_string(player.defensePower);
    DrawText(economy.c_str(), 20, 106, 16, RAYWHITE);
    DrawText("OFFLINE | ONE PROCESS | NO SERVER | NO SQL | NO LOCALHOST", 20, 130, 15, Color{104, 230, 159, 255});
    DrawText(status.c_str(), 20, 150, 14, map != nullptr ? Color{255, 220, 122, 255} : Color{255, 130, 110, 255});

    if (target.has_value()) {
        const auto& monster = runtime.monsters().at(*target);
        const auto& definition = runtime.monsterTemplates().at(monster.templateIndex);
        const float width = clampFloat(static_cast<float>(GetScreenWidth()) * 0.28F, 300.0F, 520.0F);
        const float x = (static_cast<float>(GetScreenWidth()) - width) * 0.5F;
        DrawText(definition.name.c_str(), static_cast<int>(x), 18, 20, Color{255, 218, 142, 255});
        drawProgressBar({x, 44.0F, width, 23.0F}, monster.hp, definition.maxHp, Color{170, 38, 38, 255}, "");
    }

    int y = GetScreenHeight() - 245;
    DrawRectangleRounded({12.0F, static_cast<float>(y - 8), 520.0F, 150.0F}, 0.06F, 4, Color{5, 7, 10, 170});
    int row = 0;
    for (const auto& line : runtime.log()) {
        DrawText(line.c_str(), 18, y + row * 18, 16, row == 0 ? Color{255, 222, 139, 255} : Color{216, 219, 224, 235});
        if (++row >= 8) break;
    }

    if (inventoryOpen) drawInventoryPanel(runtime, inventorySelection);
    if (statsOpen) drawStatsPanel(runtime);
    drawSkillBar(runtime);
    if (!inventoryOpen) drawMinimap(runtime, map);
    DrawText("WASD Move | Right Mouse Camera | 1-0 Skill | I Inventory | C Stats | F5 Save | ESC Character Select", 18, GetScreenHeight() - 18, 14, LIGHTGRAY);
}

bool runWorld(const ProfileSelection& profile) {
    SmdMap worldMap;
    SmdTerrainModel terrain;
    KoMonsterVisualBank visualBank;
    std::string status = "Asset set bulunamadi; fallback renderer aktif.";

    const auto assetRoot = locateAssetRoot();
    if (assetRoot.has_value()) {
        try {
            const auto catalog = KoAssetCatalog::scan(*assetRoot);
            const auto mapPath = selectMap(catalog);
            const bool mapReady = mapPath.has_value() && worldMap.load(*mapPath) && terrain.load(worldMap);
            const bool visualsReady = visualBank.initialize(*assetRoot);
            status = std::string(mapReady ? "REAL SMD OK" : "SMD FAIL")
                + " | " + (visualsReady ? "NPC_LOOKS/N3 BANK OK" : "N3 BANK FAIL")
                + (mapPath.has_value() ? " | " + mapPath->filename().string() : "");
            if (!visualsReady && !visualBank.error().empty()) status += " | " + visualBank.error();
        } catch (const std::exception& exception) {
            status = std::string("KO content error: ") + exception.what();
        }
    }

    OfflineRuntime runtime;
    runtime.configureProfile(profile.slot, profile.character.name, profile.character.playerClass);
    if (const auto dataPack = locateDataPack(); dataPack.has_value()) runtime.initialize(*dataPack);
    else runtime.initialize();

    if (visualBank.ready()) {
        std::vector<std::uint32_t> modelIds;
        modelIds.reserve(runtime.monsterTemplates().size());
        for (const auto& definition : runtime.monsterTemplates()) if (definition.modelId != 0U) modelIds.push_back(definition.modelId);
        visualBank.preload(modelIds, 48U);
        status += " | Models " + std::to_string(visualBank.loadedCount()) + "/" + std::to_string(visualBank.mappedCount());
    }

    Camera3D camera {};
    camera.up = {0.0F, 1.0F, 0.0F};
    camera.fovy = 52.0F;
    camera.projection = CAMERA_PERSPECTIVE;
    float cameraYaw = 0.75F;
    float cameraPitch = 0.42F;
    float cameraDistance = 10.5F;
    bool inventoryOpen = false;
    bool statsOpen = false;
    std::size_t inventorySelection = 0U;
    const std::array<int, 10> skillKeys {KEY_ONE,KEY_TWO,KEY_THREE,KEY_FOUR,KEY_FIVE,KEY_SIX,KEY_SEVEN,KEY_EIGHT,KEY_NINE,KEY_ZERO};

    while (!WindowShouldClose()) {
        const float delta = std::min(GetFrameTime(), 0.05F);
        if (IsKeyPressed(KEY_ESCAPE)) {
            runtime.save();
            OfflineRoster roster;
            (void) roster.updateLevel(profile.slot, runtime.player().level);
            return true;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            const Vector2 mouse = GetMouseDelta();
            cameraYaw -= mouse.x * 0.004F;
            cameraPitch = clampFloat(cameraPitch - mouse.y * 0.003F, 0.12F, 1.05F);
        }
        cameraDistance = clampFloat(cameraDistance - GetMouseWheelMove() * 0.8F, 5.5F, 17.0F);

        const Vector3 forward {std::sin(cameraYaw), 0.0F, std::cos(cameraYaw)};
        const Vector3 right {std::cos(cameraYaw), 0.0F, -std::sin(cameraYaw)};
        Vector3 movement {};
        if (!inventoryOpen && !statsOpen) {
            if (IsKeyDown(KEY_W)) movement = Vector3Add(movement, forward);
            if (IsKeyDown(KEY_S)) movement = Vector3Subtract(movement, forward);
            if (IsKeyDown(KEY_D)) movement = Vector3Add(movement, right);
            if (IsKeyDown(KEY_A)) movement = Vector3Subtract(movement, right);
        }
        if (Vector3LengthSqr(movement) > 0.001F) {
            movement = Vector3Scale(Vector3Normalize(movement), (IsKeyDown(KEY_LEFT_SHIFT) ? 6.0F : 4.0F) * delta);
            runtime.movePlayer({movement.x, 0.0F, movement.z});
        }

        const auto target = runtime.nearestAliveMonster(24.0F);
        if (!inventoryOpen && !statsOpen) {
            for (std::size_t index = 0; index < skillKeys.size(); ++index) if (IsKeyPressed(skillKeys[index])) runtime.useSkill(index, target);
        }
        if (IsKeyPressed(KEY_I)) {
            inventoryOpen = !inventoryOpen;
            statsOpen = false;
        }
        if (IsKeyPressed(KEY_C)) {
            statsOpen = !statsOpen;
            inventoryOpen = false;
        }
        if (inventoryOpen) processInventoryInput(runtime, inventorySelection);
        if (statsOpen) {
            const std::array<int, 5> keys {KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5};
            for (std::size_t index = 0; index < keys.size(); ++index) if (IsKeyPressed(keys[index])) runtime.spendStatPoint(index);
        }
        if (IsKeyPressed(KEY_F5) && !statsOpen) runtime.save();

        std::vector<Vec3> previous;
        previous.reserve(runtime.monsters().size());
        for (const auto& monster : runtime.monsters()) previous.push_back(monster.position);
        runtime.update(delta);
        visualBank.beginFrame();
        for (std::size_t index = 0; index < runtime.monsters().size(); ++index) {
            const auto& monster = runtime.monsters()[index];
            if (!monster.alive) continue;
            N3AnimationState state = N3AnimationState::Idle;
            const float dx = monster.position.x - previous[index].x;
            const float dz = monster.position.z - previous[index].z;
            if (monster.attackCooldown > 1.02F) state = N3AnimationState::Attack;
            else if (dx * dx + dz * dz > 0.000001F) state = N3AnimationState::Move;
            const auto& definition = runtime.monsterTemplates()[monster.templateIndex];
            (void) visualBank.update(definition.modelId, state, delta);
        }

        const SmdMap* activeMap = terrain.ready() ? &worldMap : nullptr;
        const Vec3& player = runtime.player().position;
        const float playerGround = groundHeight(activeMap, player);
        const float horizontal = std::cos(cameraPitch) * cameraDistance;
        camera.target = {player.x, playerGround + 1.15F, player.z};
        camera.position = {player.x - std::sin(cameraYaw) * horizontal,
                           playerGround + 1.8F + std::sin(cameraPitch) * cameraDistance,
                           player.z - std::cos(cameraYaw) * horizontal};

        BeginDrawing();
        ClearBackground(Color{133, 170, 198, 255});
        BeginMode3D(camera);
        if (terrain.ready()) terrain.draw(); else drawFallbackEnvironment();
        drawPlayer(runtime, playerGround);
        for (std::size_t index = 0; index < runtime.monsters().size(); ++index) {
            const auto& monster = runtime.monsters()[index];
            if (!monster.alive) continue;
            drawMonster(visualBank, monster, runtime.monsterTemplates()[monster.templateIndex],
                        target.has_value() && *target == index, groundHeight(activeMap, monster.position));
        }
        EndMode3D();
        drawHud(runtime, target, inventoryOpen, statsOpen, inventorySelection, activeMap, status);
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
