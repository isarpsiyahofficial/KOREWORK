#include "content/asset_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace korework::content {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void addByExtension(KoAssetCatalog& catalog, const std::filesystem::path& relativePath, const std::string& extension) {
    if (extension == ".smd") {
        catalog.serverMaps.push_back(relativePath);
    } else if (extension == ".gtd" || extension == ".trn") {
        catalog.terrainFiles.push_back(relativePath);
    } else if (extension == ".opd" || extension == ".sdt") {
        catalog.objectPlacementFiles.push_back(relativePath);
    } else if (extension == ".n3chr") {
        catalog.characterFiles.push_back(relativePath);
    } else if (extension == ".n3cpart") {
        catalog.characterPartFiles.push_back(relativePath);
    } else if (extension == ".n3cplug") {
        catalog.equipmentPlugFiles.push_back(relativePath);
    } else if (extension == ".n3joint") {
        catalog.jointFiles.push_back(relativePath);
    } else if (extension == ".n3anim") {
        catalog.animationFiles.push_back(relativePath);
    } else if (extension == ".n3shape") {
        catalog.shapeFiles.push_back(relativePath);
    } else if (extension == ".n3fx" || extension == ".fxb" || extension == ".fxd" || extension == ".fxp") {
        catalog.effectFiles.push_back(relativePath);
    } else if (extension == ".dxt" || extension == ".tga" || extension == ".bmp" || extension == ".jpg" || extension == ".png"
               || extension == ".tct" || extension == ".tlt") {
        catalog.textureFiles.push_back(relativePath);
    } else if (extension == ".uif") {
        catalog.uiFiles.push_back(relativePath);
    }
}

void sortPaths(std::vector<std::filesystem::path>& paths) {
    std::sort(paths.begin(), paths.end(), [](const auto& left, const auto& right) {
        return left.generic_string() < right.generic_string();
    });
}

} // namespace

KoAssetCatalog KoAssetCatalog::scan(const std::filesystem::path& rootPath) {
    if (!std::filesystem::exists(rootPath) || !std::filesystem::is_directory(rootPath)) {
        throw std::runtime_error("KO asset root is not a directory: " + rootPath.string());
    }

    KoAssetCatalog catalog;
    catalog.root = std::filesystem::weakly_canonical(rootPath);

    std::error_code error;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator(catalog.root, options, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (!iterator->is_regular_file(error)) {
            error.clear();
            continue;
        }

        const auto fileSize = iterator->file_size(error);
        if (!error) {
            catalog.totalBytes += fileSize;
        }
        error.clear();
        ++catalog.totalFiles;

        const auto relativePath = std::filesystem::relative(iterator->path(), catalog.root, error);
        if (error) {
            error.clear();
            continue;
        }
        const std::string extension = lower(iterator->path().extension().string());
        ++catalog.extensionCounts[extension.empty() ? "<none>" : extension];
        addByExtension(catalog, relativePath, extension);
    }

    sortPaths(catalog.serverMaps);
    sortPaths(catalog.terrainFiles);
    sortPaths(catalog.objectPlacementFiles);
    sortPaths(catalog.characterFiles);
    sortPaths(catalog.characterPartFiles);
    sortPaths(catalog.equipmentPlugFiles);
    sortPaths(catalog.jointFiles);
    sortPaths(catalog.animationFiles);
    sortPaths(catalog.shapeFiles);
    sortPaths(catalog.effectFiles);
    sortPaths(catalog.textureFiles);
    sortPaths(catalog.uiFiles);
    return catalog;
}

bool KoAssetCatalog::hasRuntimeMinimum() const noexcept {
    return !serverMaps.empty()
        && !terrainFiles.empty()
        && !objectPlacementFiles.empty()
        && !characterFiles.empty()
        && !characterPartFiles.empty()
        && !jointFiles.empty()
        && !animationFiles.empty()
        && !shapeFiles.empty()
        && !textureFiles.empty()
        && !uiFiles.empty();
}

std::string KoAssetCatalog::summary() const {
    std::ostringstream output;
    output << "KO asset root: " << root.generic_string() << '\n'
           << "Files: " << totalFiles << '\n'
           << "Bytes: " << totalBytes << '\n'
           << "SMD maps: " << serverMaps.size() << '\n'
           << "Terrain: " << terrainFiles.size() << '\n'
           << "Object placement: " << objectPlacementFiles.size() << '\n'
           << "Characters: " << characterFiles.size() << '\n'
           << "Character parts: " << characterPartFiles.size() << '\n'
           << "Equipment plugs: " << equipmentPlugFiles.size() << '\n'
           << "Joints: " << jointFiles.size() << '\n'
           << "Animations: " << animationFiles.size() << '\n'
           << "Shapes: " << shapeFiles.size() << '\n'
           << "Effects: " << effectFiles.size() << '\n'
           << "Textures: " << textureFiles.size() << '\n'
           << "UI files: " << uiFiles.size() << '\n'
           << "Runtime minimum: " << (hasRuntimeMinimum() ? "yes" : "no") << '\n';
    return output.str();
}

} // namespace korework::content
