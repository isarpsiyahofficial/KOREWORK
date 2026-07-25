#include "content/npc_looks_table.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace korework::content {
namespace {

enum class TableType : std::uint32_t {
    None = 0,
    Character = 1,
    Byte = 2,
    Short = 3,
    Word = 4,
    Integer = 5,
    Dword = 6,
    String = 7,
    Float = 8,
    Double = 9
};

using Cell = std::variant<std::int64_t, double, std::string>;
using Row = std::vector<Cell>;

class MemoryReader final {
public:
    explicit MemoryReader(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}

    template <typename T>
    [[nodiscard]] T read() {
        static_assert(std::is_trivially_copyable_v<T>);
        require(sizeof(T));
        T value {};
        std::memcpy(&value, bytes_.data() + position_, sizeof(T));
        position_ += sizeof(T);
        return value;
    }

    [[nodiscard]] std::string readString() {
        const auto length = read<std::int32_t>();
        if (length < 0 || length > 1'048'576) throw std::runtime_error("Invalid NPC_Looks string length");
        require(static_cast<std::size_t>(length));
        std::string value(reinterpret_cast<const char*>(bytes_.data() + position_), static_cast<std::size_t>(length));
        position_ += static_cast<std::size_t>(length);
        value.erase(std::remove(value.begin(), value.end(), '\0'), value.end());
        return value;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }

private:
    void require(std::size_t count) const {
        if (count > bytes_.size() - position_) throw std::runtime_error("Unexpected end of NPC_Looks table");
    }

    std::vector<std::byte> bytes_;
    std::size_t position_ = 0;
};

std::vector<std::byte> readAndDecrypt(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Unable to open NPC_Looks table: " + path.string());
    const auto end = input.tellg();
    if (end <= 0 || end > static_cast<std::streamoff>(512ULL * 1024ULL * 1024ULL)) {
        throw std::runtime_error("Invalid NPC_Looks table size");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) throw std::runtime_error("Unable to read NPC_Looks table");

    std::uint16_t key = 0x0816U;
    constexpr std::uint16_t multiplier = 0x6081U;
    constexpr std::uint16_t increment = 0x1608U;
    for (std::byte& byte : bytes) {
        const auto cipher = std::to_integer<std::uint8_t>(byte);
        const auto plain = static_cast<std::uint8_t>(cipher ^ static_cast<std::uint8_t>(key >> 8U));
        key = static_cast<std::uint16_t>((static_cast<std::uint32_t>(cipher) + key) * multiplier + increment);
        byte = static_cast<std::byte>(plain);
    }
    return bytes;
}

Cell readCell(MemoryReader& reader, TableType type) {
    switch (type) {
        case TableType::Character: return static_cast<std::int64_t>(reader.read<std::int8_t>());
        case TableType::Byte: return static_cast<std::int64_t>(reader.read<std::uint8_t>());
        case TableType::Short: return static_cast<std::int64_t>(reader.read<std::int16_t>());
        case TableType::Word: return static_cast<std::int64_t>(reader.read<std::uint16_t>());
        case TableType::Integer: return static_cast<std::int64_t>(reader.read<std::int32_t>());
        case TableType::Dword: return static_cast<std::int64_t>(reader.read<std::uint32_t>());
        case TableType::String: return reader.readString();
        case TableType::Float: return static_cast<double>(reader.read<float>());
        case TableType::Double: return reader.read<double>();
        case TableType::None: break;
    }
    throw std::runtime_error("Unsupported NPC_Looks column type");
}

std::int64_t integer(const Row& row, std::size_t index) {
    if (index >= row.size() || !std::holds_alternative<std::int64_t>(row[index])) {
        throw std::runtime_error("NPC_Looks integer column mismatch");
    }
    return std::get<std::int64_t>(row[index]);
}

std::string text(const Row& row, std::size_t index) {
    if (index >= row.size() || !std::holds_alternative<std::string>(row[index])) {
        throw std::runtime_error("NPC_Looks string column mismatch");
    }
    return std::get<std::string>(row[index]);
}

} // namespace

NpcLooksTable NpcLooksTable::load(const std::filesystem::path& encryptedTablePath) {
    static_assert(std::endian::native == std::endian::little, "NPC_Looks requires little-endian targets");
    MemoryReader reader(readAndDecrypt(encryptedTablePath));
    const auto columnCount = reader.read<std::int32_t>();
    if (columnCount < 1 || columnCount > 512) throw std::runtime_error("Invalid NPC_Looks column count");

    std::vector<TableType> types;
    types.reserve(static_cast<std::size_t>(columnCount));
    for (std::int32_t index = 0; index < columnCount; ++index) {
        const auto raw = reader.read<std::uint32_t>();
        if (raw < static_cast<std::uint32_t>(TableType::Character)
            || raw > static_cast<std::uint32_t>(TableType::Double)) {
            throw std::runtime_error("Invalid NPC_Looks column type");
        }
        types.push_back(static_cast<TableType>(raw));
    }
    if (types.front() != TableType::Dword) throw std::runtime_error("NPC_Looks key is not a DWORD");

    const auto rowCount = reader.read<std::int32_t>();
    if (rowCount < 0 || rowCount > 1'000'000) throw std::runtime_error("Invalid NPC_Looks row count");

    NpcLooksTable table;
    table.records_.reserve(static_cast<std::size_t>(rowCount));
    table.index_.reserve(static_cast<std::size_t>(rowCount));
    for (std::int32_t rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
        Row row;
        row.reserve(types.size());
        for (const auto type : types) row.push_back(readCell(reader, type));
        if (row.size() < 38U) throw std::runtime_error("NPC_Looks row has fewer than 38 columns");

        const auto rawId = integer(row, 0);
        if (rawId <= 0 || rawId > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Invalid NPC_Looks resource id");
        }
        NpcLookRecord record;
        record.id = static_cast<std::uint32_t>(rawId);
        record.name = text(row, 1);
        record.jointReference = text(row, 2);
        record.animationReference = text(row, 3);
        record.partReferences.reserve(10U);
        for (std::size_t index = 4; index < 14; ++index) record.partReferences.push_back(text(row, index));
        record.skinReference = text(row, 14);
        record.characterReference = text(row, 15);
        record.effectPlugReference = text(row, 16);
        record.rightHandJoint = static_cast<std::int32_t>(integer(row, 18));
        record.leftHandJoint = static_cast<std::int32_t>(integer(row, 19));
        record.leftForearmJoint = static_cast<std::int32_t>(integer(row, 20));
        record.cloakJoint = static_cast<std::int32_t>(integer(row, 21));

        const auto position = table.records_.size();
        if (!table.index_.emplace(record.id, position).second) {
            throw std::runtime_error("Duplicate NPC_Looks resource id: " + std::to_string(record.id));
        }
        table.records_.push_back(std::move(record));
    }
    if (reader.remaining() != 0U) throw std::runtime_error("Unexpected trailing NPC_Looks table bytes");
    return table;
}

const NpcLookRecord* NpcLooksTable::find(std::uint32_t id) const noexcept {
    const auto iterator = index_.find(id);
    return iterator == index_.end() ? nullptr : &records_[iterator->second];
}

} // namespace korework::content
