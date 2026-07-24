#include "content/asset_catalog.hpp"
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

        std::filesystem::path selectedMap;
        if (argc > 2) {
            selectedMap = argv[2];
        } else {
            selectedMap = assetRoot / catalog.serverMaps.front();
        }

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
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "KO content probe failed: " << exception.what() << '\n';
        return 1;
    }
}
