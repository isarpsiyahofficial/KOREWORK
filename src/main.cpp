#include "client/ko_monster_visual_bank.hpp"
#include "client/smd_terrain.hpp"
#include "content/asset_catalog.hpp"
#include "content/smd_map.hpp"
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
#include <unordered_map>
#include <vector>

namespace {

using korework::MonsterState;
using korework::MonsterTemplate;
using korework::OfflineRuntime;
using korework::Vec3;
using korework::client::KoMonsterVisualBank;
using korework::client::N3AnimationState;
using korework::client::SmdTerrainModel;
using korework::content::KoAssetCatalog;
using korework::content::SmdMap;

float clampFloat(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(value, maximum));
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::filesystem::path applicationDirectory() {
    const char* directory = GetApplicationDirectory();
    return directory != nullptr && *directory != '\0'
        ? std::filesystem::path(directory)
        : std::filesystem::current_path();
}

std::optional<std::filesystem::path> locateAssetRoot() {
    std::vector<std::filesystem::path> candidates;
    if (const char* environment = std::getenv("KOREWORK_ASSET_ROOT"); environment != nullptr && *environment != '\0') {
        candidates.emplace_back(environment);
    }
    const auto current = std::filesystem::current_path();
    const auto application = applicationDirectory();
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
    std::vector<std::filesystem::path> candidates;
    if (const char* environment = std::getenv("KOREWORK_DATA_PACK"); environment != nullptr && *environment != '\0') {
        candidates.emplace_back(environment);
    }
    const auto current = std::filesystem::current_path();
    const auto application = applicationDirectory();
    candidates.push_back(current / "data" / "game_data.kopack");
    candidates.push_back(current / "game_data.kopack");
    candidates.push_back(application / "data" / "game_data.kopack");
    candidates.push_back(application / "game_data.kopack");
    candidates.push_back(application.parent_path() / "data" / "game_data.kopack");

    std::error_code error;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, error)) return std::filesystem::weakly_canonical(candidate, error);
        error.clear();
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> selectMap(const KoAssetCatalog& catalog) {
    const std::array<std::string, 7> priorities {
        "elmorad_0516.smd", "karus_0516.smd", "free_0810.smd", "battle_0810.smd",
        "elmorad", "karus", "moradon"
    };
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

void drawPlayer(const Vec3& position, float ground) {
    const Vector3 base {position.x, ground, position.z};
    DrawCylinder({base.x, base.y + 0.9F, base.z}, 0.42F, 0.55F, 1.25F, 12, Color{185, 148, 56, 255});
    DrawSphere({base.x, base.y + 1.85F, base.z}, 0.34F, Color{210, 178, 105, 255});
    DrawCube({base.x + 0.52F, base.y + 1.05F, base.z}, 0.78F, 0.09F, 0.15F, Color{216, 216, 224, 255});
}

void drawFallbackMonster(const MonsterState& monster, const MonsterTemplate& definition, float ground) {
    const Vector3 base {monster.position.x, ground, monster.position.z};
    const float scale = definition.scale;
    DrawCylinder({base.x, base.y + 0.62F * scale, base.z}, 0.35F * scale, 0.46F * scale, 0.9F * scale, 10, Color{124, 82, 45, 255});
    DrawSphere({base.x, base.y + 1.35F * scale, base.z}, 0.34F * scale, Color{91, 145, 75, 255});
}

void drawMonster(const MonsterState& monster,
                 const MonsterTemplate& definition,
                 bool targeted,
                 float ground,
                 KoMonsterVisualBank& visualBank) {
    const Vector3 base {monster.position.x, ground, monster.position.z};
    const float targetHeight = 1.85F * definition.scale;
    if (!visualBank.draw(definition.modelId, base, targetHeight, WHITE)) {
        drawFallbackMonster(monster, definition, ground);
    }

    if (targeted) {
        DrawCircle3D({base.x, base.y + 0.03F, base.z}, 0.8F * definition.scale,
                     {1.0F, 0.0F, 0.0F}, 90.0F, Color{255, 190, 40, 225});
    }
    const float ratio = clampFloat(monster.hp / definition.maxHp, 0.0F, 1.0F);
    const float barHeight = 2.2F * definition.scale;
    DrawCube({base.x, base.y + barHeight, base.z}, 1.4F * definition.scale, 0.08F, 0.08F, Color{35, 12, 12, 255});
    DrawCube({base.x - (1.4F * definition.scale * (1.0F - ratio)) * 0.5F, base.y + barHeight, base.z - 0.001F},
             1.4F * definition.scale * ratio, 0.085F, 0.085F, Color{185, 35, 35, 255});
}

void drawFallbackEnvironment() {
    DrawPlane({0.0F, -0.03F, 0.0F}, {96.0F, 96.0F}, Color{77, 105, 57, 255});
    DrawGrid(48, 2.0F);
}

void drawSkillBar(const OfflineRuntime& runtime) {
    const float slot = clampFloat(static_cast<float>(GetScreenWidth()) / 27.5F, 38.0F, 62.0F);
    const float gap = clampFloat(slot * 0.09F, 3.0F, 6.0F);
    const float totalWidth = slot * 10.0F + gap * 9.0F;
    const float startX = (static_cast<float>(GetScreenWidth()) - totalWidth) * 0.5F;
    const float startY = static_cast<float>(GetScreenHeight()) - slot - 20.0F;
    const std::array<const char*, 10> keys {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};

    DrawRectangleRounded({startX - 8.0F, startY - 8.0F, totalWidth + 16.0F, slot + 16.0F}, 0.12F, 6, Color{8, 10, 14, 215});
    for (std::size_t index = 0; index < 10; ++index) {
        const float x = startX + static_cast<float>(index) * (slot + gap);
        const Rectangle rectangle {x, startY, slot, slot};
        const auto& skill = runtime.skills()[index];
        const float cooldown = runtime.cooldowns()[index];
        DrawRectangleRounded(rectangle, 0.16F, 5, skill.unlocked ? Color{67, 72, 88, 245} : Color{33, 34, 39, 245});
        DrawRectangleLinesEx(rectangle, 1.5F, skill.unlocked ? Color{205, 171, 75, 255} : Color{80, 80, 85, 255});
        if (skill.unlocked) {
            const std::uint32_t hash = skill.id * 2654435761U;
            DrawCircle(static_cast<int>(x + slot * 0.5F), static_cast<int>(startY + slot * 0.52F), slot * 0.28F,
                       Color{static_cast<unsigned char>(70U + hash % 150U),
                             static_cast<unsigned char>(60U + (hash >> 8U) % 150U),
                             static_cast<unsigned char>(70U + (hash >> 16U) % 150U), 255});
        }
        if (cooldown > 0.0F) {
            const float ratio = clampFloat(cooldown / std::max(0.01F, skill.cooldown), 0.0F, 1.0F);
            DrawRectangle(static_cast<int>(x), static_cast<int>(startY), static_cast<int>(slot), static_cast<int>(slot * ratio), Color{5, 6, 9, 185});
        }
        DrawText(keys[index], static_cast<int>(x + 4.0F), static_cast<int>(startY + 3.0F), static_cast<int>(slot * 0.23F), Color{255, 230, 145, 255});
    }
}

void drawMinimap(const OfflineRuntime& runtime, const SmdMap* map) {
    const float size = clampFloat(static_cast<float>(GetScreenHeight()) * 0.20F, 130.0F, 210.0F);
    const Rectangle panel {static_cast<float>(GetScreenWidth()) - size - 18.0F, 18.0F, size, size};
    DrawRectangleRounded(panel, 0.08F, 6, Color{7, 12, 13, 215});
    DrawRectangleLinesEx(panel, 2.0F, Color{160, 143, 78, 230});
    DrawText(map != nullptr ? "KO SMD" : "FALLBACK", static_cast<int>(panel.x + 10.0F), static_cast<int>(panel.y + 8.0F), 15, Color{238, 219, 153, 255});

    const auto project = [&panel, map](const Vec3& position) {
        if (map != nullptr) {
            return Vector2 {
                panel.x + clampFloat((position.x + map->width() * 0.5F) / map->width(), 0.0F, 1.0F) * panel.width,
                panel.y + clampFloat((position.z + map->length() * 0.5F) / map->length(), 0.0F, 1.0F) * panel.height
            };
        }
        return Vector2 {panel.x + panel.width * 0.5F + position.x, panel.y + panel.height * 0.5F + position.z};
    };
    for (const auto& monster : runtime.monsters()) if (monster.alive) DrawCircleV(project(monster.position), 2.5F, RED);
    DrawCircleV(project(runtime.player().position), 4.0F, YELLOW);
}

void drawHud(const OfflineRuntime& runtime,
             std::optional<std::size_t> target,
             bool inventoryOpen,
             const SmdMap* map,
             const std::string& status) {
    const auto& player = runtime.player();
    drawProgressBar({18.0F, 18.0F, 290.0F, 26.0F}, player.hp, player.maxHp, Color{170, 32, 39, 255}, "HP");
    drawProgressBar({18.0F, 49.0F, 290.0F, 24.0F}, player.mp, player.maxMp, Color{36, 79, 171, 255}, "MP");
    const int neededExperience = std::max(1000, player.level * player.level * 500);
    char stats[160] {};
    std::snprintf(stats, sizeof(stats), "Level %d   EXP %d/%d   Noah %d", player.level, player.exp, neededExperience, player.gold);
    DrawText(stats, 20, 79, 18, Color{246, 226, 166, 255});
    DrawText("OFFLINE | TEK PROCESS | SUNUCU YOK | SQL YOK | LOCALHOST YOK", 20, 105, 16, Color{104, 230, 159, 255});
    DrawText(status.c_str(), 20, 129, 15, map != nullptr ? Color{255, 220, 122, 255} : Color{255, 130, 110, 255});

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

    if (inventoryOpen) {
        const float panelWidth = clampFloat(static_cast<float>(GetScreenWidth()) * 0.26F, 320.0F, 440.0F);
        const float panelHeight = clampFloat(static_cast<float>(GetScreenHeight()) * 0.55F, 360.0F, 580.0F);
        const float x = static_cast<float>(GetScreenWidth()) - panelWidth - 24.0F;
        const float panelY = (static_cast<float>(GetScreenHeight()) - panelHeight) * 0.5F;
        DrawRectangleRounded({x, panelY, panelWidth, panelHeight}, 0.05F, 6, Color{9, 12, 17, 235});
        DrawRectangleLinesEx({x, panelY, panelWidth, panelHeight}, 2.0F, Color{193, 158, 70, 255});
        DrawText("INVENTORY", static_cast<int>(x + 18.0F), static_cast<int>(panelY + 16.0F), 24, Color{243, 222, 156, 255});
        int itemY = static_cast<int>(panelY + 62.0F);
        for (const auto& item : runtime.inventory()) {
            char itemText[200] {};
            std::snprintf(itemText, sizeof(itemText), "%s x%d", item.name.c_str(), item.count);
            DrawText(itemText, static_cast<int>(x + 24.0F), itemY, 18, RAYWHITE);
            itemY += 30;
            if (itemY > panelY + panelHeight - 28.0F) break;
        }
    }

    drawSkillBar(runtime);
    drawMinimap(runtime, map);
    DrawText("WASD Hareket | Sag tik Kamera | 1-0 Skill | I Envanter | F5 Kaydet", 18, GetScreenHeight() - 18, 14, LIGHTGRAY);
}

int statePriority(N3AnimationState state) {
    switch (state) {
        case N3AnimationState::Attack: return 3;
        case N3AnimationState::Move: return 2;
        case N3AnimationState::Idle: return 1;
    }
    return 0;
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1600, 900, "KOREWORK - Offline Fire Drake Rework");
    SetWindowMinSize(960, 540);
    SetTargetFPS(60);

    OfflineRuntime runtime;
    const auto dataPack = locateDataPack();
    if (dataPack.has_value()) runtime.initialize(*dataPack);
    else runtime.initialize();

    SmdMap worldMap;
    SmdTerrainModel terrain;
    KoMonsterVisualBank monsterVisuals;
    std::string status = runtime.usingGameData() ? "KOPACK V2 OK" : "KOPACK FALLBACK";

    if (const auto assetRoot = locateAssetRoot(); assetRoot.has_value()) {
        try {
            const auto catalog = KoAssetCatalog::scan(*assetRoot);
            const auto mapPath = selectMap(catalog);
            const bool mapReady = mapPath.has_value() && worldMap.load(*mapPath) && terrain.load(worldMap);
            const bool looksReady = monsterVisuals.initialize(*assetRoot);
            std::vector<std::uint32_t> modelIds;
            modelIds.reserve(runtime.monsterTemplates().size());
            for (const auto& definition : runtime.monsterTemplates()) modelIds.push_back(definition.modelId);
            if (looksReady) monsterVisuals.preload(modelIds, 32U);

            status += std::string(" | ") + (mapReady ? "SMD OK" : "SMD FAIL")
                + " | " + (looksReady ? "NPC_LOOKS OK" : "NPC_LOOKS FAIL")
                + " | MODELS " + std::to_string(monsterVisuals.loadedCount())
                + "/" + std::to_string(monsterVisuals.mappedCount())
                + (mapPath.has_value() ? " | " + mapPath->filename().string() : "");
            if (!looksReady && !monsterVisuals.error().empty()) status += " | " + monsterVisuals.error();
        } catch (const std::exception& exception) {
            status += std::string(" | KO content hatasi: ") + exception.what();
        }
    } else {
        status += " | KO asset set bulunamadi";
    }

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
            const Vector2 mouse = GetMouseDelta();
            cameraYaw -= mouse.x * 0.004F;
            cameraPitch = clampFloat(cameraPitch - mouse.y * 0.003F, 0.12F, 1.05F);
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

        const auto target = runtime.nearestAliveMonster(24.0F);
        for (std::size_t index = 0; index < skillKeys.size(); ++index) {
            if (IsKeyPressed(skillKeys[index])) runtime.useSkill(index, target);
        }
        if (IsKeyPressed(KEY_I)) inventoryOpen = !inventoryOpen;
        if (IsKeyPressed(KEY_F5)) runtime.save();

        std::vector<Vec3> previousPositions;
        previousPositions.reserve(runtime.monsters().size());
        for (const auto& monster : runtime.monsters()) previousPositions.push_back(monster.position);
        runtime.update(delta);

        monsterVisuals.beginFrame();
        std::unordered_map<std::uint32_t, N3AnimationState> modelStates;
        for (std::size_t index = 0; index < runtime.monsters().size(); ++index) {
            const auto& monster = runtime.monsters()[index];
            if (!monster.alive) continue;
            const auto& definition = runtime.monsterTemplates()[monster.templateIndex];
            N3AnimationState state = N3AnimationState::Idle;
            if (monster.attackCooldown > definition.attackDelay * 0.72F) state = N3AnimationState::Attack;
            else if (index < previousPositions.size()) {
                const float dx = monster.position.x - previousPositions[index].x;
                const float dz = monster.position.z - previousPositions[index].z;
                if (dx * dx + dz * dz > 0.000001F) state = N3AnimationState::Move;
            }
            auto [iterator, inserted] = modelStates.emplace(definition.modelId, state);
            if (!inserted && statePriority(state) > statePriority(iterator->second)) iterator->second = state;
        }
        for (const auto& [modelId, state] : modelStates) monsterVisuals.update(modelId, state, delta);

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
        drawPlayer(player, playerGround);
        for (std::size_t index = 0; index < runtime.monsters().size(); ++index) {
            const auto& monster = runtime.monsters()[index];
            if (!monster.alive) continue;
            drawMonster(monster,
                        runtime.monsterTemplates()[monster.templateIndex],
                        target.has_value() && *target == index,
                        groundHeight(activeMap, monster.position),
                        monsterVisuals);
        }
        EndMode3D();

        drawHud(runtime, target, inventoryOpen, activeMap, status);
        DrawFPS(GetScreenWidth() - 90, GetScreenHeight() - 28);
        EndDrawing();
    }

    runtime.save();
    monsterVisuals.unload();
    CloseWindow();
    return 0;
}
