#include "content/ko_asset_resolver.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

namespace korework::content {

std::string KoAssetResolver::normalized(const std::filesystem::path& path) {
    std::string value = path.generic_string();
    std::replace(value.begin(), value.end(), '\\', '/');
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    while (value.starts_with("./")) value.erase(0, 2);
    return value;
}

KoAssetResolver::KoAssetResolver(std::filesystem::path gameRoot) {
    if (!std::filesystem::is_directory(gameRoot)) throw std::runtime_error("KO game root is not a directory: " + gameRoot.string());
    std::error_code error;
    root_ = std::filesystem::weakly_canonical(std::move(gameRoot), error);
    if (error) throw std::runtime_error("Unable to canonicalize KO game root");

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator(root_, options, error), end; iterator != end; iterator.increment(error)) {
        if (error) { error.clear(); continue; }
        if (!iterator->is_regular_file(error)) { error.clear(); continue; }
        const auto relative = std::filesystem::relative(iterator->path(), root_, error);
        if (error) { error.clear(); continue; }
        files_.try_emplace(normalized(relative), iterator->path());
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
        if (const auto iterator = files_.find(normalized(candidate)); iterator != files_.end()) return iterator->second;
    }
    return findFilename(stored.filename().string());
}

std::optional<std::filesystem::path> KoAssetResolver::findFilename(const std::string& filename) const {
    const std::string wanted = normalized(std::filesystem::path(filename).filename());
    for (const auto& [relative, absolute] : files_) {
        if (normalized(std::filesystem::path(relative).filename()) == wanted) return absolute;
    }
    return std::nullopt;
}

} // namespace korework::content
