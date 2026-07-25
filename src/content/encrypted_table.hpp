#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace korework::content {

enum class KoTableType : std::uint32_t {
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

using KoTableCell = std::variant<std::int64_t, double, std::string>;
using KoTableRow = std::vector<KoTableCell>;

class EncryptedKoTable final {
public:
    [[nodiscard]] static EncryptedKoTable load(const std::filesystem::path& path,
                                                std::size_t maximumRows = 2'000'000U,
                                                std::size_t maximumColumns = 512U);

    [[nodiscard]] const std::vector<KoTableType>& columnTypes() const noexcept { return columnTypes_; }
    [[nodiscard]] const std::vector<KoTableRow>& rows() const noexcept { return rows_; }

    [[nodiscard]] static std::int64_t integer(const KoTableRow& row, std::size_t column);
    [[nodiscard]] static double number(const KoTableRow& row, std::size_t column);
    [[nodiscard]] static std::string text(const KoTableRow& row, std::size_t column);

private:
    std::vector<KoTableType> columnTypes_;
    std::vector<KoTableRow> rows_;
};

} // namespace korework::content
