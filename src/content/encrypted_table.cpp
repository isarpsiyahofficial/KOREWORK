#include "content/encrypted_table.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace korework::content {
namespace {

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
        if (length < 0 || length > 4 * 1024 * 1024) throw std::runtime_error("Invalid encrypted table string length");
        require(static_cast<std::size_t>(length));
        std::string value(reinterpret_cast<const char*>(bytes_.data() + position_), static_cast<std::size_t>(length));
        position_ += static_cast<std::size_t>(length);
        value.erase(std::remove(value.begin(), value.end(), '\0'), value.end());
        return value;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }

private:
    void require(std::size_t count) const {
        if (count > bytes_.size() - position_) throw std::runtime_error("Unexpected end of encrypted KO table");
    }

    std::vector<std::byte> bytes_;
    std::size_t position_ = 0;
};

std::vector<std::byte> readAndDecrypt(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Unable to open encrypted KO table: " + path.string());
    const auto end = input.tellg();
    if (end <= 0 || end > static_cast<std::streamoff>(2ULL * 1024ULL * 1024ULL * 1024ULL)) {
        throw std::runtime_error("Invalid encrypted KO table size: " + path.string());
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) throw std::runtime_error("Unable to read encrypted KO table: " + path.string());

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

KoTableCell readCell(MemoryReader& reader, KoTableType type) {
    switch (type) {
        case KoTableType::Character: return static_cast<std::int64_t>(reader.read<std::int8_t>());
        case KoTableType::Byte: return static_cast<std::int64_t>(reader.read<std::uint8_t>());
        case KoTableType::Short: return static_cast<std::int64_t>(reader.read<std::int16_t>());
        case KoTableType::Word: return static_cast<std::int64_t>(reader.read<std::uint16_t>());
        case KoTableType::Integer: return static_cast<std::int64_t>(reader.read<std::int32_t>());
        case KoTableType::Dword: return static_cast<std::int64_t>(reader.read<std::uint32_t>());
        case KoTableType::String: return reader.readString();
        case KoTableType::Float: return static_cast<double>(reader.read<float>());
        case KoTableType::Double: return reader.read<double>();
        case KoTableType::None: break;
    }
    throw std::runtime_error("Unsupported encrypted KO table column type");
}

} // namespace

EncryptedKoTable EncryptedKoTable::load(const std::filesystem::path& path,
                                         std::size_t maximumRows,
                                         std::size_t maximumColumns) {
    static_assert(std::endian::native == std::endian::little, "Encrypted KO tables require little-endian targets");
    MemoryReader reader(readAndDecrypt(path));

    const auto rawColumnCount = reader.read<std::int32_t>();
    if (rawColumnCount <= 0 || static_cast<std::size_t>(rawColumnCount) > maximumColumns) {
        throw std::runtime_error("Invalid encrypted KO table column count: " + path.string());
    }

    EncryptedKoTable table;
    table.columnTypes_.reserve(static_cast<std::size_t>(rawColumnCount));
    for (std::int32_t index = 0; index < rawColumnCount; ++index) {
        const auto raw = reader.read<std::uint32_t>();
        if (raw < static_cast<std::uint32_t>(KoTableType::Character)
            || raw > static_cast<std::uint32_t>(KoTableType::Double)) {
            throw std::runtime_error("Invalid encrypted KO table column type: " + path.string());
        }
        table.columnTypes_.push_back(static_cast<KoTableType>(raw));
    }
    if (table.columnTypes_.front() != KoTableType::Dword) {
        throw std::runtime_error("Encrypted KO table key column is not DWORD: " + path.string());
    }

    const auto rawRowCount = reader.read<std::int32_t>();
    if (rawRowCount < 0 || static_cast<std::size_t>(rawRowCount) > maximumRows) {
        throw std::runtime_error("Invalid encrypted KO table row count: " + path.string());
    }

    table.rows_.reserve(static_cast<std::size_t>(rawRowCount));
    for (std::int32_t rowIndex = 0; rowIndex < rawRowCount; ++rowIndex) {
        KoTableRow row;
        row.reserve(table.columnTypes_.size());
        for (const KoTableType type : table.columnTypes_) row.push_back(readCell(reader, type));
        table.rows_.push_back(std::move(row));
    }
    if (reader.remaining() != 0U) {
        throw std::runtime_error("Unexpected trailing bytes in encrypted KO table: " + path.string());
    }
    return table;
}

std::int64_t EncryptedKoTable::integer(const KoTableRow& row, std::size_t column) {
    if (column >= row.size()) throw std::runtime_error("Encrypted KO table integer column is missing");
    if (const auto* value = std::get_if<std::int64_t>(&row[column]); value != nullptr) return *value;
    throw std::runtime_error("Encrypted KO table integer column type mismatch");
}

double EncryptedKoTable::number(const KoTableRow& row, std::size_t column) {
    if (column >= row.size()) throw std::runtime_error("Encrypted KO table numeric column is missing");
    if (const auto* value = std::get_if<double>(&row[column]); value != nullptr) return *value;
    if (const auto* integerValue = std::get_if<std::int64_t>(&row[column]); integerValue != nullptr) {
        return static_cast<double>(*integerValue);
    }
    throw std::runtime_error("Encrypted KO table numeric column type mismatch");
}

std::string EncryptedKoTable::text(const KoTableRow& row, std::size_t column) {
    if (column >= row.size()) throw std::runtime_error("Encrypted KO table string column is missing");
    if (const auto* value = std::get_if<std::string>(&row[column]); value != nullptr) return *value;
    throw std::runtime_error("Encrypted KO table string column type mismatch");
}

} // namespace korework::content
