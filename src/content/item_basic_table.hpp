#pragma once

#include "data/game_data_pack.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

namespace korework::content {

class ItemBasicTable final {
public:
    [[nodiscard]] static std::filesystem::path locate(const std::filesystem::path& assetRoot);
    [[nodiscard]] static ItemBasicTable load(const std::filesystem::path& encryptedTablePath);

    [[nodiscard]] const std::vector<data::ItemRecord>& items() const noexcept { return items_; }
    [[nodiscard]] const data::ItemRecord* find(std::uint32_t id) const noexcept;

private:
    std::vector<data::ItemRecord> items_;
    std::unordered_map<std::uint32_t, std::size_t> index_;
};

} // namespace korework::content
