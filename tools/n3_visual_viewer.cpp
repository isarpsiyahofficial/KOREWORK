#include "client/n3_animation_player.hpp"
#include "client/n3_character_model.hpp"
#include "client/n3_shape_model.hpp"
#include "client/ko_player_visual.hpp"
#include "content/ko_asset_resolver.hpp"
#include "content/n3_animation.hpp"
#include "content/n3_character.hpp"
#include "content/n3_shape.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: n3_visual_viewer <asset-root> <character-or-shape-relative> <screenshot>\n";
        return 2;
    }
    const std::filesystem::path root = argv[1];
    const std::filesystem::path relative = argv[2];
    const std::filesystem::path path = root / "game" / relative;
    const std::string shot = argv[3];
    InitWindow(1280, 720, "N3 visual viewer");
    SetTargetFPS(60);
    Camera3D camera {};
    camera.position = {0.0F, 1.2F, 7.0F};
    camera.target = {0.0F, 1.0F, 0.0F};
    camera.up = {0.0F, 1.0F, 0.0F};
    camera.fovy = 55.0F;
    camera.projection = CAMERA_PERSPECTIVE;
    bool ok = false;
    std::string error;
    {
    korework::content::KoAssetResolver resolver(root / "game");
    korework::client::N3CharacterModel characterModel;
    korework::client::N3AnimationPlayer animation;
    korework::client::N3ShapeModel shapeModel;
    korework::client::KoPlayerVisual playerVisual;
    std::array<korework::client::KoPlayerVisual, 3> selectionPlayers;
    bool selectionScene = false;
    try {
        std::string ext = relative.extension().string();
        const std::string relativeText = relative.generic_string();
        if (relativeText == "selection") {
            selectionScene = true;
            korework::content::N3ShapeLoader shapeLoader(resolver);
            ok = shapeModel.load(shapeLoader.load(root / "game" / "ChrSelect" / "el_chairs.n3shape"));
            const std::array<korework::PlayerClass, 3> classes {
                korework::PlayerClass::Warrior, korework::PlayerClass::Rogue, korework::PlayerClass::Mage
            };
            for (std::size_t i = 0; i < selectionPlayers.size(); ++i) {
                ok = selectionPlayers[i].initialize(root, classes[i]) && ok;
            }
            camera.position = {0.0F, -0.2F, 7.0F};
            camera.target = {0.0F, -0.4F, 0.0F};
            camera.fovy = 55.0F;
            error = ok ? "" : "Fire Drake selection scene asset load failed";
        } else if (relativeText.rfind("player:", 0) == 0) {
            const std::string className = relativeText.substr(7);
            korework::PlayerClass playerClass = korework::PlayerClass::Warrior;
            if (className == "rogue") playerClass = korework::PlayerClass::Rogue;
            else if (className == "mage") playerClass = korework::PlayerClass::Mage;
            else if (className == "priest") playerClass = korework::PlayerClass::Priest;
            ok = playerVisual.initialize(root, playerClass);
            error = playerVisual.error();
        } else if (ext == ".n3shape") {
            korework::content::N3ShapeLoader loader(resolver);
            ok = shapeModel.load(loader.load(path));
            error = shapeModel.error();
            camera.position = {0.0F, 0.0F, 7.0F};
            camera.target = {0.0F, -0.3F, 0.0F};
        } else {
            korework::content::N3CharacterLoader loader(resolver);
            auto character = loader.load(path);
            if (argc >= 5) {
                const std::string filter = argv[4];
                character.parts.erase(std::remove_if(character.parts.begin(), character.parts.end(), [&](const auto& part) {
                    const std::string name = part.name;
                    const bool permanent = name.find("head") != std::string::npos || name.find("hair") != std::string::npos;
                    return !permanent && name.find(filter) == std::string::npos;
                }), character.parts.end());
            }
            ok = characterModel.load(character);
            if (ok && !character.animationPath.empty()) {
                auto animations = korework::content::N3AnimationSet::load(character.animationPath);
                animation.configure(animations);
            }
            error = characterModel.error();
        }
    } catch (const std::exception& ex) { error = ex.what(); }
    for (int frame = 0; frame < 90 && !WindowShouldClose(); ++frame) {
        const float delta = GetFrameTime();
        if (animation.ready()) {
            animation.update(delta);
            characterModel.updateAnimation(animation.frame());
        }
        playerVisual.update(korework::client::N3AnimationState::Idle, delta);
        for (auto& selectionPlayer : selectionPlayers) selectionPlayer.update(korework::client::N3AnimationState::Idle, delta);
        BeginDrawing();
        ClearBackground({20,20,24,255});
        BeginMode3D(camera);
        if (!selectionScene) DrawGrid(20, 1.0F);
        if (selectionScene) {
            if (shapeModel.ready()) shapeModel.draw();
            selectionPlayers[0].draw({0.0F, -1.20F, 2.74F}, 2.05F, WHITE, 0.0F);
            selectionPlayers[1].draw({1.86F, -1.20F, 2.0F}, 2.05F, WHITE, 42.0F);
            selectionPlayers[2].draw({-1.90F, -1.20F, 2.0F}, 2.05F, WHITE, -46.0F);
        } else {
            if (characterModel.ready()) characterModel.draw({0,0,0}, 2.1F);
            if (playerVisual.ready()) playerVisual.draw({0,0,0}, 2.1F);
            if (shapeModel.ready()) shapeModel.draw();
        }
        EndMode3D();
        DrawText(ok ? relative.generic_string().c_str() : error.c_str(), 20, 20, 20, ok ? RAYWHITE : RED);
        EndDrawing();
        if (frame == 60) TakeScreenshot(shot.c_str());
    }
    }
    CloseWindow();
    return ok ? 0 : 1;
}
