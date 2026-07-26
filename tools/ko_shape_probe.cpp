#include "content/ko_asset_resolver.hpp"
#include "content/n3_shape.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: ko_shape_probe <asset-root> <relative-shape>");
        const std::filesystem::path root = argv[1];
        const std::filesystem::path shapePath = root / "game" / argv[2];
        const korework::content::KoAssetResolver resolver(root / "game");
        const korework::content::N3ShapeLoader loader(resolver);
        const auto shape = loader.load(shapePath);
        std::size_t vertices = 0U;
        std::size_t triangles = 0U;
        std::size_t partIndex = 0U;
        for (const auto& part : shape.parts) {
            vertices += part.mesh.vertices.size();
            triangles += part.mesh.indices.size() / 3U;
            std::cout << "Part[" << partIndex++ << "] mesh=" << part.meshPath.filename().string()
                      << " textures=" << part.texturePaths.size()
                      << " flags=0x" << std::hex << part.renderFlags << std::dec;
            for (const auto& texture : part.texturePaths) std::cout << " tex=" << texture.filename().string();
            std::cout << '\n';
        }
        if (shape.parts.empty() || vertices == 0U || triangles == 0U) throw std::runtime_error("shape has no geometry");
        std::cout << "Shape: " << shape.name << '\n'
                  << "Parts: " << shape.parts.size() << '\n'
                  << "Vertices: " << vertices << '\n'
                  << "Triangles: " << triangles << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
