#include "content/asset_catalog.hpp"
#include "content/ko_asset_resolver.hpp"
#include "content/n3_equipment.hpp"
#include "content/n3_texture.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    try {
        const std::filesystem::path assetRoot = argc > 1 ? argv[1] : "upstream/ko-assets";
        const auto catalog = korework::content::KoAssetCatalog::scan(assetRoot);
        const korework::content::KoAssetResolver resolver(assetRoot / "game");
        const korework::content::N3EquipmentLoader loader(resolver);

        std::vector<std::filesystem::path> candidates;
        for (const std::string& preferred : {"cane.n3cplug", "bow_c.n3cplug"}) {
            const auto matches = resolver.findAllFilenames(preferred);
            candidates.insert(candidates.end(), matches.begin(), matches.end());
        }
        for (const auto& relative : catalog.equipmentPlugFiles) {
            candidates.push_back(assetRoot / relative);
        }

        std::optional<korework::content::N3EquipmentPlug> selected;
        std::filesystem::path selectedPath;
        std::size_t attempted = 0;
        std::string lastError;
        for (const auto& candidate : candidates) {
            if (!std::filesystem::is_regular_file(candidate)) continue;
            ++attempted;
            try {
                auto plug = loader.load(candidate);
                if (plug.jointIndex < 0 || plug.mesh.vertices.empty() || plug.mesh.indices.empty()) continue;
                selectedPath = candidate;
                selected = std::move(plug);
                break;
            } catch (const std::exception& exception) {
                lastError = exception.what();
            }
        }

        if (!selected.has_value()) {
            std::cerr << "No parseable N3 equipment plug was found after " << attempted << " candidates."
                      << (lastError.empty() ? "" : " Last error: " + lastError) << '\n';
            return 2;
        }

        const auto& plug = *selected;
        std::cout << "Equipment candidates: " << candidates.size() << '\n'
                  << "Equipment attempted: " << attempted << '\n'
                  << "Equipment plug: " << selectedPath.generic_string() << '\n'
                  << "Equipment name: " << plug.name << '\n'
                  << "Equipment joint: " << plug.jointIndex << '\n'
                  << "Equipment mesh: " << plug.meshPath.generic_string() << '\n'
                  << "Equipment vertices: " << plug.mesh.vertices.size() << '\n'
                  << "Equipment faces: " << plug.mesh.indices.size() / 3U << '\n'
                  << "Equipment minimum vertices: " << plug.mesh.minimumVertices << '\n'
                  << "Equipment minimum indices: " << plug.mesh.minimumIndices << '\n';

        if (!plug.texturePath.empty()) {
            const auto texture = korework::content::N3TextureLoader::load(plug.texturePath);
            std::cout << "Equipment texture: " << plug.texturePath.generic_string() << '\n'
                      << "Equipment texture dimensions: " << texture.width << " x " << texture.height << '\n'
                      << "Equipment texture bytes: " << texture.rgba.size() << '\n';
        } else {
            std::cout << "Equipment texture: none\n";
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "KO equipment probe failed: " << exception.what() << '\n';
        return 1;
    }
}
