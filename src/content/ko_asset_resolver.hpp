#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace korework::content {

class KoAssetResolver final {
public:
    explicit KoAssetResolver(std::filesystem::path gameRoot);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] std::optional<std::filesystem::path> resolve(const std::filesystem::path& baseFile,
                                                               const std::string& storedPath) const;
    [[nodiscard]] std::optional<std::filesystem::path> findFilename(const std::string& filename) const;

private:
    [[nodiscard]] static std::string normalized(const std::filesystem::path& path);
    std::filesystem::path root_;
    std::unordered_map<std::string, std::filesystem::path> files_;
};

} // namespace korework::content
