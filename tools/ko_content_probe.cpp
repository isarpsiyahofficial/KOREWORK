#include "content/asset_catalog.hpp"
#include "content/ko_asset_resolver.hpp"
#include "content/n3_character.hpp"
#include "content/smd_map.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const std::filesystem::path assetRoot = argc > 1 ? argv[1] : "upstream/ko-assets";
        const auto catalog = korework::content::KoAssetCatalog::scan(assetRoot);
        std::cout << catalog.summary();

        if (!catalog.hasRuntimeMinimum()) {
            std::cerr << "KO asset set is missing one or more required runtime categories.\n";
            return 2;
        }
        if (catalog.serverMaps.empty()) {
            std::cerr << "No SMD server maps were discovered.\n";
            return 3;
        }

        const std::filesystem::path selectedMap = argc > 2
            ? std::filesystem::path(argv[2])
            : assetRoot / catalog.serverMaps.front();

        korework::content::SmdMap map;
        if (!map.load(selectedMap)) {
            std::cerr << "SMD parse failed for " << selectedMap << ": " << map.error() << '\n';
            return 4;
        }

        std::cout << "Selected SMD: " << selectedMap.generic_string() << '\n'
                  << "Map grid: " << map.mapSize() << " x " << map.mapSize() << '\n'
                  << "Unit distance: " << map.unitDistance() << '\n'
                  << "World size: " << map.width() << " x " << map.length() << '\n'
                  << "Height samples: " << map.heights().size() << '\n'
                  << "Collision triangles: " << map.collisionTriangles().size() << '\n'
                  << "Collision main cells: " << map.collisionCells().size() << '\n'
                  << "Object events: " << map.objectEvents().size() << '\n'
                  << "Regene events: " << map.regeneEvents().size() << '\n'
                  << "Warps: " << map.warps().size() << '\n';

        const korework::content::KoAssetResolver resolver(assetRoot / "game");
        auto characterPath = resolver.findFilename("mob_deruvisy.n3chr");
        if (!characterPath.has_value() && !catalog.characterFiles.empty()) {
            characterPath = assetRoot / catalog.characterFiles.front();
        }
        if (!characterPath.has_value()) {
            std::cerr << "No N3 character root was discovered.\n";
            return 5;
        }

        const korework::content::N3CharacterLoader characterLoader(resolver);
        const auto character = characterLoader.load(*characterPath);
        std::size_t populatedLods = 0;
        std::size_t totalVertices = 0;
        std::size_t totalFaces = 0;
        for (const auto& part : character.parts) {
            for (const auto& lod : part.lods) {
                if (!lod.positions.empty()) {
                    ++populatedLods;
                    totalVertices += lod.positions.size();
                    totalFaces += lod.faceIndices.size() / 3U;
                }
            }
        }
        if (totalVertices == 0 || totalFaces == 0) {
            std::cerr << "N3 character has no populated skin geometry.\n";
            return 6;
        }

        std::cout << "Selected N3 character: " << characterPath->generic_string() << '\n'
                  << "Character name: " << character.name << '\n'
                  << "Character parts: " << character.parts.size() << '\n'
                  << "Character plugs: " << character.plugs.size() << '\n'
                  << "Populated skin LODs: " << populatedLods << '\n'
                  << "Skin vertices: " << totalVertices << '\n'
                  << "Skin faces: " << totalFaces << '\n'
                  << "Joint: " << character.jointPath.generic_string() << '\n'
                  << "Animation: " << character.animationPath.generic_string() << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "KO content probe failed: " << exception.what() << '\n';
        return 1;
    }
}
