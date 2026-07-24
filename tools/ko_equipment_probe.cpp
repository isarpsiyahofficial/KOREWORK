#include "content/ko_asset_resolver.hpp"
#include "content/n3_equipment.hpp"
#include "content/n3_texture.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const std::filesystem::path assetRoot = argc > 1 ? argv[1] : "upstream/ko-assets";
        const korework::content::KoAssetResolver resolver(assetRoot / "game");
        auto plugPath = resolver.findFilename("cane.n3cplug");
        if (!plugPath.has_value()) plugPath = resolver.findFilename("bow_c.n3cplug");
        if (!plugPath.has_value()) {
            std::cerr << "No known N3 equipment plug was found.\n";
            return 2;
        }

        const korework::content::N3EquipmentLoader loader(resolver);
        const auto plug = loader.load(*plugPath);
        if (plug.jointIndex < 0 || plug.mesh.vertices.empty() || plug.mesh.indices.empty()) {
            std::cerr << "N3 equipment plug is missing its joint or mesh geometry.\n";
            return 3;
        }

        std::cout << "Equipment plug: " << plugPath->generic_string() << '\n'
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
