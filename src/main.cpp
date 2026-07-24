#include "client/smd_terrain.hpp"
#include "content/asset_catalog.hpp"
#include "content/smd_map.hpp"
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

namespace {

using korework::MonsterState;
using korework::MonsterTemplate;
using korework::OfflineRuntime;
using korework::Vec3;
using korework::client::SmdTerrainModel;
using korework::content::KoAssetCatalog;
using korework::content::SmdMap;

Vector3 toRay(const Vec3& value, float groundHeight = 0.0F) {
    return {value.x, groundHeight + value.y, value.z};
}

float clampFloat(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(value, maximum));
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::optional<std::filesystem::path> locateAssetRoot() {
    std::vector<std::filesystem::path> candidates;
    if (const char* environment = std::getenv("KOREWORK_ASSET_ROOT"); environment != nullptr && *environment != '\0') {
        candidates.emplace_back(environment);
    }
    const auto current = std::filesystem::current_path();
    candidates.push_back(current / "assets" / "ko");
    candidates.push_back(current / "upstream" / "ko-assets");

    const std::filesystem::path applicationDirectory = GetApplicationDirectory();
    candidates.push_back(applicationDirectory / "assets" / "ko");
    candidates.push_back(applicationDirectory / "upstream" / "ko-assets");
    candidates.push_back(applicationDirectory.parent_path() / "upstream" / "ko-assets");

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

std::optional<std::filesystem::path> selectMap(const KoAssetCatalog& catalog) {
    const std::array<std::string, 5> priorities {
        "elmorad_0516.smd", "karus_0516.smd", "free_0810.smd", "battle_0810.smd", "moradon"
    };
    for (const std::string& priority : priorities) {
        for (const auto& relative : catalog.serverMaps) {
            if (lower(relative.filename().string()).find(priority) != std::string::npos) {
                return catalog.root / relative;
            }
        }
    }
    if (!catalog.serverMaps.empty()) {
        return catalog.root / catalog.serverMaps.front();
    }
    return std::nullopt;
}

float groundHeight(const SmdMap* map, const Vec3& worldPosition) {
    if (map == nullptr || !map->loaded()) {
        return 0.0F;
    }
    const float mapX = worldPosition.x + map->width() * 0.5F;
    const float mapZ = worldPosition.z + map->length() * 0.5F;
    return map->heightAt(mapX, mapZ);
}

void drawProgressBar(Rectangle rectangle, float value, float maximum, Color fill, const char* label) {
    DrawRectangleRounded(rectangle, 0.25F, 6, Color{12, 14, 18, 220});
    DrawRectangleLinesEx(rectangle, 1.0F, Color{170, 170, 180, 180});
    const float ratio = maximum > 0.0F ? clampFloat(value / maximum, 0.0F, 1.0F) : 0.0F;
    Rectangle filled = rectangle;
    filled.width *= ratio;
    DrawRectangleRounded(filled, 0.25F, 6, fill);
    char text[96] {};
    std::snprintf(text, sizeof(text), "%s %d / %d", label, static_cast<int>(value), static_cast<int>(maximum));
    DrawText(text, static_cast<int>(rectangle.x + 8.0F), static_cast<int>(rectangle.y + 4.0F), 16, RAYWHITE);
}

void drawPlayer(const Vec3& position, float terrainHeight) {
    const Vector3 base = toRay(position, terrainHeight);
    DrawCylinder({base.x, base.y + 0.9F, base.z}, 0.42F, 0.55F, 1.25F, 12, Color{185, 148, 56, 255});
    DrawSphere({base.x, base.y + 1.85F, base.z}, 0.34F, Color{210, 178, 105, 255});
    DrawCube({base.x, base.y + 1.05F, base.z - 0.47F}, 0.16F, 0.22F, 1.15F, Color{150, 160, 175, 255});
    DrawCube({base.x + 0.52F, base.y + 1.05F, base.z}, 0.78F, 0.09F, 0.15F, Color{216, 216, 224, 255});
    DrawCubeWires({base.x, base.y + 1.15F, base.z}, 0.95F, 1.55F, 0.72F, Color{88, 62, 20, 255});
}

void drawKecoon(const MonsterState& monster, const MonsterTemplate& definition, bool targeted, float terrainHeight) {
    const Vector3 base = toRay(monster.position, terrainHeight);
    const float scale = definition.scale;
    const Color skin = definition.sid == 106 ? Color{93, 124, 77, 255} : Color{91, 145, 75, 255};
    const Color cloth = definition.level >= 14 ? Color{105, 55, 38, 255} : Color{124, 82, 45, 255};

    DrawCylinder({base.x, base.y + 0.62F * scale, base.z}, 0.35F * scale, 0.46F * scale, 0.9F * scale, 10, cloth);
    DrawSphere({base.x, base.y + 1.35F * scale, base.z}, 0.34F * scale, skin);
    DrawCube({base.x - 0.18F * scale, base.y + 0.18F * scale, base.z}, 0.2F * scale, 0.48F * scale, 0.2F * scale, skin);
    DrawCube({base.x + 0.18F * scale, base.y + 0.18F * scale, base.z}, 0.2F * scale, 0.48F * scale, 0.2F * scale, skin);
    DrawCube({base.x + 0.46F * scale, base.y + 0.82F * scale, base.z}, 0.75F * scale, 0.12F * scale, 0.12F * scale, Color{91, 54, 24, 255});

    if (targeted) {
        DrawCircle3D({base.x, base.y + 0.03F, base.z}, 0.75F * scale, {1.0F, 0.0F, 0.0F}, 90.0F, Color{255, 190, 40, 220});
    }

    const float ratio = clampFloat(monster.hp / definition.maxHp, 0.0F, 1.0F);
    const Vector3 hpPosition = {base.x, base.y + 2.05F * scale, base.z};
    DrawCube(hpPosition, 1.35F * scale, 0.08F, 0.08F, Color{35, 12, 12, 255});
    DrawCube({hpPosition.x - (1.35F * scale * (1.0F - ratio)) * 0.5F, hpPosition.y, hpPosition.z - 0.001F},
             1.35F * scale * ratio, 0.085F, 0.085F, Color{185, 35, 35, 255});
}

void drawFallbackEnvironment() {
    DrawPlane({0.0F, -0.03F, 0.0F}, {64.0F, 64.0F}, Color{77, 105, 57, 255});
    DrawGrid(32, 2.0F);
    for (int i = -28; i <= 28; i += 7) {
        const float offset = static_cast<float>((i * i + 13) % 9) - 4.0F;
        DrawCylinder({static_cast<float>(i), 0.8F, -25.0F + offset}, 0.22F, 0.34F, 1.6F, 8, Color{82, 50, 25, 255});
        DrawSphere({static_cast<float>(i), 2.0F, -25.0F + offset}, 1.05F, Color{44, 95, 46, 255});
        DrawCylinder({static_cast<float>(i), 0.8F, 25.0F - offset}, 0.22F, 0.34F, 1.6F, 8, Color{82, 50, 25, 255});
        DrawSphere({static_cast<float>(i), 2.0F, 25.0F - offset}, 1.05F, Color{44, 95, 46, 255});
    }
}

void drawSkillBar(const OfflineRuntime& runtime) {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const float slot = clampFloat(static_cast<float>(width) / 27.5F, 38.0F, 62.0F);
    const float gap = clampFloat(slot * 0.09F, 3.0F, 6.0F);
    const float totalWidth = slot * 10.0F + gap * 9.0F;
    const float startX = (static_cast<float>(width) - totalWidth) * 0.5F;
    const float startY = static_cast<float>(height) - slot - 18.0F;
    const std::array<const char*, 10> keys {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};

    DrawRectangleRounded({startX - 8.0F, startY - 8.0F, totalWidth + 16.0F, slot + 16.0F}, 0.12F, 6, Color{8, 10, 14, 210});
    for (std::size_t i = 0; i < 10; ++i) {
        const float x = startX + static_cast<float>(i) * (slot + gap);
        const Rectangle rectangle {x, startY, slot, slot};
        const auto& skill = runtime.skills()[i];
        const float cooldown = runtime.cooldowns()[i];
        const Color base = skill.unlocked ? Color{67, 72, 88, 245} : Color{33, 34, 39, 245};
        DrawRectangleRounded(rectangle, 0.16F, 5, base);
        DrawRectangleLinesEx(rectangle, 1.5F, skill.unlocked ? Color{205, 171, 75, 255} : Color{80, 80, 85, 255});
        if (skill.unlocked) {
            const Color icon = i < 3 ? Color{180, 61, 41, 255} : Color{52, 112, 164, 255};
            DrawCircle(static_cast<int>(x + slot * 0.5F), static_cast<int>(startY + slot * 0.52F), slot * 0.28F, icon);
            const char symbol = skill.name.empty() ? '?' : skill.name.front();
            char label[2] {symbol, '\0'};
            DrawText(label, static_cast<int>(x + slot * 0.42F), static_cast<int>(startY + slot * 0.31F), static_cast<int>(slot * 0.38F), RAYWHITE);
        }
        if (cooldown > 0.0F) {
            const float ratio = clampFloat(cooldown / std::max(0.01F, skill.cooldown), 0.0F, 1.0F);
            DrawRectangle(static_cast<int>(x), static_cast<int>(startY), static_cast<int>(slot), static_cast<int>(slot * ratio), Color{5, 6, 9, 185});
            char cooldownText[16] {};
            std::snprintf(cooldownText, sizeof(cooldownText), "%.1f", cooldown);
            DrawText(cooldownText, static_cast<int>(x + slot * 0.28F), static_cast<int>(startY + slot * 0.38F), static_cast<int>(slot * 0.25F), RAYWHITE);
        }
        DrawText(keys[i], static_cast<int>(x + 4.0F), static_cast<int>(startY + 3.0F), static_cast<int>(slot * 0.23F), Color{255, 230, 145, 255});
    }
}

void drawMinimap(const OfflineRuntime& runtime, const SmdMap* map) {
    const int width = GetScreenWidth();
    const float size = clampFloat(static_cast<float>(GetScreenHeight()) * 0.20F, 130.0F, 210.0F);
    const Rectangle panel {static_cast<float>(width) - size - 18.0F, 18.0F, size, size};
    DrawRectangleRounded(panel, 0.08F, 6, Color{7, 12, 13, 210});
    DrawRectangleLinesEx(panel, 2.0F, Color{160, 143, 78, 230});
    DrawText(map != nullptr ? "REAL KO SMD" : "PROTOTYPE MAP", static_cast<int>(panel.x + 10.0F), static_cast<int>(panel.y + 8.0F), 15, Color{238, 219, 153, 255});

    const auto project = [&panel, map](const Vec3& position) {
        if (map != nullptr && map->loaded()) {
            const float normalizedX = clampFloat((position.x + map->width() * 0.5F) / map->width(), 0.0F, 1.0F);
            const float normalizedZ = clampFloat((position.z + map->length() * 0.5F) / map->length(), 0.0F, 1.0F);
            return Vector2 {panel.x + normalizedX * panel.width, panel.y + normalizedZ * panel.height};
        }
        return Vector2 {panel.x + panel.width * 0.5F + (position.x / 32.0F) * panel.width * 0.45F,
                        panel.y + panel.height * 0.5F + (position.z / 32.0F) * panel.height * 0.45F};
    };

    for (const MonsterState& monster : runtime.monsters()) {
        if (monster.alive) DrawCircleV(project(monster.position), 2.5F, Color{220, 64, 52, 255});
    }
    DrawCircleV(project(runtime.player().position), 4.0F, Color{255, 221, 67, 255});
}

void drawHud(const OfflineRuntime& runtime, std::optional<std::size_t> target, bool inventoryOpen,
             const SmdMap* map, const std::string& contentStatus) {
    const auto& player = runtime.player();
    drawProgressBar({18.0F, 18.0F, 290.0F, 26.0F}, player.hp, player.maxHp, Color{170, 32, 39, 255}, "HP");
    drawProgressBar({18.0F, 49.0F, 290.0F, 24.0F}, player.mp, player.maxMp, Color{36, 79, 171, 255}, "MP");
    char stats[128] {};
    std::snprintf(stats, sizeof(stats), "Level %d   EXP %d/%d   Noah %d", player.level, player.exp, player.level * 200, player.gold);
    DrawText(stats, 20, 79, 18, Color{246, 226, 166, 255});
    DrawText("OFFLINE | ONE PROCESS | NO SERVER | NO SQL | NO LOCALHOST", 20, 105, 16, Color{104, 230, 159, 255});
    DrawText(contentStatus.c_str(), 20, 129, 15, map != nullptr ? Color{255, 220, 122, 255} : Color{255, 130, 110, 255});

    if (target.has_value()) {
        const MonsterState& monster = runtime.monsters().at(*target);
        const MonsterTemplate& definition = runtime.monsterTemplates().at(monster.templateIndex);
        const float barWidth = clampFloat(static_cast<float>(GetScreenWidth()) * 0.28F, 300.0F, 520.0F);
        const float x = (static_cast<float>(GetScreenWidth()) - barWidth) * 0.5F;
        DrawText(definition.name.c_str(), static_cast<int>(x), 18, 20, Color{255, 218, 142, 255});
        drawProgressBar({x, 44.0F, barWidth, 23.0F}, monster.hp, definition.maxHp, Color{170, 38, 38, 255}, "");
    }

    const int logX = 18;
    const int logY = GetScreenHeight() - 245;
    DrawRectangleRounded({12.0F, static_cast<float>(logY - 8), 500.0F, 150.0F}, 0.06F, 4, Color{5, 7, 10, 165});
    int row = 0;
    for (const std::string& line : runtime.log()) {
        DrawText(line.c_str(), logX, logY + row * 18, 16, row == 0 ? Color{255, 222, 139, 255} : Color{216, 219, 224, 235});
        if (++row >= 8) break;
    }

    DrawText("WASD: Hareket   Sag tik: Kamera   1-0: Skill   I: Envanter   F5: Kaydet", 18, GetScreenHeight() - 18, 14, Color{215, 218, 222, 235});
    if (inventoryOpen) {
        const float panelWidth = 340.0F;
        const float panelHeight = 420.0F;
        const float x = static_cast<float>(GetScreenWidth()) - panelWidth - 24.0F;
        const float y = (static_cast<float>(GetScreenHeight()) - panelHeight) * 0.5F;
        DrawRectangleRounded({x, y, panelWidth, panelHeight}, 0.05F, 6, Color{9, 12, 17, 235});
        DrawRectangleLinesEx({x, y, panelWidth, panelHeight}, 2.0F, Color{193, 158, 70, 255});
        DrawText("INVENTORY", static_cast<int>(x + 18.0F), static_cast<int>(y + 16.0F), 24, Color{243, 222, 156, 255});
        int itemY = static_cast<int>(y + 62.0F);
        if (runtime.inventory().empty()) {
            DrawText("Henuz item yok.", static_cast<int>(x + 18.0F), itemY, 18, LIGHTGRAY);
        } else {
            for (const auto& item : runtime.inventory()) {
                char itemText[160] {};
                std::snprintf(itemText, sizeof(itemText), "%s  x%d", item.name.c_str(), item.count);
                DrawRectangle(static_cast<int>(x + 16.0F), itemY - 4, static_cast<int>(panelWidth - 32.0F), 32, Color{30, 34, 43, 220});
                DrawText(itemText, static_cast<int>(x + 27.0F), itemY + 3, 18, RAYWHITE);
                itemY += 39;
            }
        }
    }
    drawSkillBar(runtime);
    drawMinimap(runtime, map);
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1600, 900, "KOREWORK - Offline Fire Drake Rework");
    SetWindowMinSize(960, 540);
    SetTargetFPS(60);

    std::optional<KoAssetCatalog> catalog;
    SmdMap worldMap;
    SmdTerrainModel terrain;
    std::string contentStatus = "KO asset set bulunamadi; teknik fallback kullaniliyor.";
    if (const auto assetRoot = locateAssetRoot(); assetRoot.has_value()) {
        try {
            catalog = KoAssetCatalog::scan(*assetRoot);
            if (const auto mapPath = selectMap(*catalog); mapPath.has_value() && worldMap.load(*mapPath)) {
                if (terrain.load(worldMap)) {
                    contentStatus = "KO SMD: " + mapPath->filename().string() + " | grid "
                        + std::to_string(worldMap.mapSize()) + " | collision "
                        + std::to_string(worldMap.collisionTriangles().size());
                } else {
                    contentStatus = "SMD okundu fakat terrain mesh olusturulamadi: " + terrain.error();
                }
            } else {
                contentStatus = "KO asset bulundu fakat SMD acilamadi: " + worldMap.error();
            }
        } catch (const std::exception& exception) {
            contentStatus = std::string("KO content hatasi: ") + exception.what();
        }
    }

    OfflineRuntime runtime;
    runtime.initialize();

    Camera3D camera {};
    camera.up = {0.0F, 1.0F, 0.0F};
    camera.fovy = 52.0F;
    camera.projection = CAMERA_PERSPECTIVE;
    float cameraYaw = 0.75F;
    float cameraPitch = 0.42F;
    float cameraDistance = 10.5F;
    bool inventoryOpen = false;
    const std::array<int, 10> skillKeys {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO};

    while (!WindowShouldClose()) {
        const float delta = std::min(GetFrameTime(), 0.05F);
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            const Vector2 mouseDelta = GetMouseDelta();
            cameraYaw -= mouseDelta.x * 0.004F;
            cameraPitch = clampFloat(cameraPitch - mouseDelta.y * 0.003F, 0.12F, 1.05F);
        }
        cameraDistance = clampFloat(cameraDistance - GetMouseWheelMove() * 0.8F, 5.5F, 17.0F);

        const Vector3 forward {std::sin(cameraYaw), 0.0F, std::cos(cameraYaw)};
        const Vector3 right {std::cos(cameraYaw), 0.0F, -std::sin(cameraYaw)};
        Vector3 movement {};
        if (IsKeyDown(KEY_W)) movement = Vector3Add(movement, forward);
        if (IsKeyDown(KEY_S)) movement = Vector3Subtract(movement, forward);
        if (IsKeyDown(KEY_D)) movement = Vector3Add(movement, right);
        if (IsKeyDown(KEY_A)) movement = Vector3Subtract(movement, right);
        if (Vector3LengthSqr(movement) > 0.001F) {
            movement = Vector3Scale(Vector3Normalize(movement), (IsKeyDown(KEY_LEFT_SHIFT) ? 6.0F : 4.0F) * delta);
            runtime.movePlayer({movement.x, 0.0F, movement.z});
        }

        const std::optional<std::size_t> target = runtime.nearestAliveMonster(24.0F);
        for (std::size_t i = 0; i < skillKeys.size(); ++i) {
            if (IsKeyPressed(skillKeys[i])) runtime.useSkill(i, target);
        }
        if (IsKeyPressed(KEY_I)) inventoryOpen = !inventoryOpen;
        if (IsKeyPressed(KEY_F5)) runtime.save();
        runtime.update(delta);

        const SmdMap* activeMap = terrain.ready() ? &worldMap : nullptr;
        const Vec3& playerPosition = runtime.player().position;
        const float playerGround = groundHeight(activeMap, playerPosition);
        const float horizontalDistance = std::cos(cameraPitch) * cameraDistance;
        camera.target = {playerPosition.x, playerGround + 1.15F, playerPosition.z};
        camera.position = {playerPosition.x - std::sin(cameraYaw) * horizontalDistance,
                           playerGround + 1.8F + std::sin(cameraPitch) * cameraDistance,
                           playerPosition.z - std::cos(cameraYaw) * horizontalDistance};

        BeginDrawing();
        ClearBackground(Color{133, 170, 198, 255});
        BeginMode3D(camera);
        if (terrain.ready()) terrain.draw(); else drawFallbackEnvironment();
        drawPlayer(playerPosition, playerGround);
        for (std::size_t i = 0; i < runtime.monsters().size(); ++i) {
            const MonsterState& monster = runtime.monsters()[i];
            if (!monster.alive) continue;
            drawKecoon(monster, runtime.monsterTemplates()[monster.templateIndex], target.has_value() && *target == i,
                       groundHeight(activeMap, monster.position));
        }
        EndMode3D();

        drawHud(runtime, target, inventoryOpen, activeMap, contentStatus);
        DrawFPS(GetScreenWidth() - 90, GetScreenHeight() - 28);
        EndDrawing();
    }

    runtime.save();
    CloseWindow();
    return 0;
}
