#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace korework::content {

class KoAssetResolver final {
public:
    explicit KoAssetResolver(std::filesystem::path gameRoot);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] std::optional<std::filesystem::path> resolve(const std::filesystem::path& baseFile,
                                                               const std::string& storedPath) const;
    [[nodiscard]] std::optional<std::filesystem::path> findFilename(const std::string& filename) const;
    [[nodiscard]] std::vector<std::filesystem::path> findAllFilenames(const std::string& filename) const;

private:
    [[nodiscard]] static std::string normalized(const std::filesystem::path& path);
    [[nodiscard]] static int directoryPreference(const std::filesystem::path& relativePath) noexcept;

    std::filesystem::path root_;
    std::unordered_map<std::string, std::filesystem::path> files_;
    std::unordered_map<std::string, std::vector<std::filesystem::path>> filenames_;
};

} // namespace korework::content
