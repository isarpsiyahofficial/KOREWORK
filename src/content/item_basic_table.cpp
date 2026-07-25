#include "content/item_basic_table.hpp"

#include "content/encrypted_table.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

namespace korework::content {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

template <typename T>
T unsignedCell(const KoTableRow& row, std::size_t column) {
    const std::int64_t raw = EncryptedKoTable::integer(row, column);
    if (raw <= 0) return static_cast<T>(0);
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<T>::max());
    return static_cast<T>(std::min<std::uint64_t>(static_cast<std::uint64_t>(raw), maximum));
}

template <typename T>
T signedCell(const KoTableRow& row, std::size_t column) {
    const std::int64_t raw = EncryptedKoTable::integer(row, column);
    return static_cast<T>(std::clamp<std::int64_t>(raw,
                                                   static_cast<std::int64_t>(std::numeric_limits<T>::min()),
                                                   static_cast<std::int64_t>(std::numeric_limits<T>::max())));
}

} // namespace

std::filesystem::path ItemBasicTable::locate(const std::filesystem::path& assetRoot) {
    std::filesystem::path fallback;
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(
             assetRoot, std::filesystem::directory_options::skip_permission_denied, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (!iterator->is_regular_file(error)) continue;
        const std::string filename = lower(iterator->path().filename().string());
        if (filename == "item_org_us.tbl") return iterator->path();
        if (fallback.empty() && filename.starts_with("item_org") && filename.ends_with(".tbl")) fallback = iterator->path();
    }
    if (!fallback.empty()) return fallback;
    throw std::runtime_error("Item_Org table was not found in the KO asset tree");
}

ItemBasicTable ItemBasicTable::load(const std::filesystem::path& encryptedTablePath) {
    const EncryptedKoTable source = EncryptedKoTable::load(encryptedTablePath);

    // Fire Drake clients exist with 35, 36 and 37-column Item_Org layouts.
    // The fields required by the offline runtime end at column 34; sell-group
    // and grade columns are optional trailing fields and must not reject an
    // otherwise valid legacy table.
    constexpr std::size_t RequiredColumnCount = 35U;
    if (source.columnTypes().size() < RequiredColumnCount) {
        throw std::runtime_error("Item_Org table has " + std::to_string(source.columnTypes().size())
                                 + " columns; at least " + std::to_string(RequiredColumnCount) + " are required");
    }

    ItemBasicTable table;
    table.items_.reserve(source.rows().size());
    table.index_.reserve(source.rows().size());

    for (const KoTableRow& row : source.rows()) {
        if (row.size() < RequiredColumnCount) {
            throw std::runtime_error("Item_Org row has " + std::to_string(row.size())
                                     + " columns; at least " + std::to_string(RequiredColumnCount) + " are required");
        }
        const std::int64_t rawId = EncryptedKoTable::integer(row, 0);
        if (rawId <= 0 || static_cast<std::uint64_t>(rawId) > std::numeric_limits<std::uint32_t>::max()) continue;

        data::ItemRecord item;
        item.id = static_cast<std::uint32_t>(rawId);
        item.extensionId = unsignedCell<std::uint16_t>(row, 1);
        item.name = EncryptedKoTable::text(row, 2);
        item.description = EncryptedKoTable::text(row, 3);
        item.appearanceId = unsignedCell<std::uint32_t>(row, 6);
        item.iconId = unsignedCell<std::uint32_t>(row, 7);
        item.kind = unsignedCell<std::uint8_t>(row, 10);
        item.slot = unsignedCell<std::uint8_t>(row, 12);
        item.race = unsignedCell<std::uint8_t>(row, 13);
        item.classRestriction = unsignedCell<std::uint8_t>(row, 14);
        item.damage = signedCell<std::int16_t>(row, 15);
        item.attackDelay = signedCell<std::int16_t>(row, 16);
        item.range = signedCell<std::int16_t>(row, 17);
        item.weight = signedCell<std::int16_t>(row, 18);
        item.durability = signedCell<std::int16_t>(row, 19);
        item.buyPrice = unsignedCell<std::uint32_t>(row, 20);
        item.sellPrice = item.buyPrice / 4U;
        item.armor = signedCell<std::int16_t>(row, 22);
        item.countable = unsignedCell<std::uint8_t>(row, 23) != 0U;
        item.effect1 = unsignedCell<std::uint32_t>(row, 24);
        item.effect2 = unsignedCell<std::uint32_t>(row, 25);
        item.requiredLevel = unsignedCell<std::uint8_t>(row, 26);
        item.requiredRank = unsignedCell<std::uint8_t>(row, 28);
        item.requiredTitle = unsignedCell<std::uint8_t>(row, 29);
        item.requiredStrength = unsignedCell<std::uint8_t>(row, 30);
        item.requiredStamina = unsignedCell<std::uint8_t>(row, 31);
        item.requiredDexterity = unsignedCell<std::uint8_t>(row, 32);
        item.requiredIntelligence = unsignedCell<std::uint8_t>(row, 33);
        item.requiredMagicPower = unsignedCell<std::uint8_t>(row, 34);

        if (item.name.empty()) item.name = "KO Item #" + std::to_string(item.id);
        const std::size_t position = table.items_.size();
        if (!table.index_.emplace(item.id, position).second) {
            throw std::runtime_error("Duplicate Item_Org id: " + std::to_string(item.id));
        }
        table.items_.push_back(std::move(item));
    }

    if (table.items_.empty()) throw std::runtime_error("Item_Org table contains no usable item rows");
    return table;
}

const data::ItemRecord* ItemBasicTable::find(std::uint32_t id) const noexcept {
    const auto iterator = index_.find(id);
    return iterator == index_.end() ? nullptr : &items_[iterator->second];
}

} // namespace korework::content {
