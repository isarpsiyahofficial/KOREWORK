#include "content/ko_asset_resolver.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace korework::content {

std::string KoAssetResolver::normalized(const std::filesystem::path& path) {
    std::string value = path.generic_string();
    std::replace(value.begin(), value.end(), '\\', '/');
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    while (value.starts_with("./")) value.erase(0, 2);
    return value;
}

int KoAssetResolver::directoryPreference(const std::filesystem::path& relativePath) noexcept {
    const std::string directory = normalized(relativePath.parent_path());
    if (directory == "item" || directory.starts_with("item/")) return 0;
    if (directory == "chr" || directory.starts_with("chr/")) return 1;
    if (directory == "object" || directory.starts_with("object/")) return 2;
    if (directory == "ui" || directory.starts_with("ui/")) return 3;
    return 10;
}

KoAssetResolver::KoAssetResolver(std::filesystem::path gameRoot) {
    if (!std::filesystem::is_directory(gameRoot)) {
        throw std::runtime_error("KO game root is not a directory: " + gameRoot.string());
    }
    std::error_code error;
    root_ = std::filesystem::weakly_canonical(std::move(gameRoot), error);
    if (error) throw std::runtime_error("Unable to canonicalize KO game root");

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator(root_, options, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (!iterator->is_regular_file(error)) {
            error.clear();
            continue;
        }
        const auto relative = std::filesystem::relative(iterator->path(), root_, error);
        if (error) {
            error.clear();
            continue;
        }
        const std::string relativeKey = normalized(relative);
        files_.try_emplace(relativeKey, iterator->path());
        filenames_[normalized(relative.filename())].push_back(iterator->path());
    }

    for (auto& [filename, paths] : filenames_) {
        (void) filename;
        std::sort(paths.begin(), paths.end(), [this](const auto& left, const auto& right) {
            std::error_code leftError;
            std::error_code rightError;
            const auto leftRelative = std::filesystem::relative(left, root_, leftError);
            const auto rightRelative = std::filesystem::relative(right, root_, rightError);
            const int leftPreference = leftError ? 100 : directoryPreference(leftRelative);
            const int rightPreference = rightError ? 100 : directoryPreference(rightRelative);
            return std::tuple{leftPreference, normalized(leftRelative)}
                < std::tuple{rightPreference, normalized(rightRelative)};
        });
    }
}

std::optional<std::filesystem::path> KoAssetResolver::resolve(const std::filesystem::path& baseFile,
                                                               const std::string& storedPath) const {
    if (storedPath.empty()) return std::nullopt;
    std::string fixed = storedPath;
    std::replace(fixed.begin(), fixed.end(), '\\', '/');
    const std::filesystem::path stored(fixed);

    std::vector<std::filesystem::path> relativeCandidates;
    std::error_code error;
    const auto baseRelative = std::filesystem::relative(baseFile.parent_path(), root_, error);
    if (!error) {
        relativeCandidates.push_back(baseRelative / stored.filename());
        relativeCandidates.push_back(baseRelative / stored);
    }
    relativeCandidates.push_back(stored);
    relativeCandidates.push_back(stored.filename());

    for (const auto& candidate : relativeCandidates) {
        if (const auto iterator = files_.find(normalized(candidate)); iterator != files_.end()) {
            return iterator->second;
        }
    }
    return findFilename(stored.filename().string());
}

std::vector<std::filesystem::path> KoAssetResolver::findAllFilenames(const std::string& filename) const {
    const std::string wanted = normalized(std::filesystem::path(filename).filename());
    if (const auto iterator = filenames_.find(wanted); iterator != filenames_.end()) return iterator->second;
    return {};
}

std::optional<std::filesystem::path> KoAssetResolver::findFilename(const std::string& filename) const {
    const auto matches = findAllFilenames(filename);
    if (matches.empty()) return std::nullopt;
    return matches.front();
}

} // namespace korework::content
