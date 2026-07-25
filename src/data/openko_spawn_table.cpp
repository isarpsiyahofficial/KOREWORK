#include "data/openko_spawn_table.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace korework::data {
namespace {

using Row = std::vector<std::string>;
constexpr std::size_t MaximumRecords = 1'000'000U;
constexpr std::size_t MaximumPathLength = 16'384U;

std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to read OpenKO spawn file: " + path.string());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char character) { return !std::isspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string cleanSqlString(std::string value) {
    value = trim(std::move(value));
    if (value.empty() || value == "NULL" || value == "null") return {};
    std::size_t prefix = 0U;
    if (value.size() >= 2U && (value[0] == 'N' || value[0] == 'n') && value[1] == '\'') prefix = 1U;
    if (value.size() >= prefix + 2U && value[prefix] == '\'' && value.back() == '\'') {
        value = value.substr(prefix + 1U, value.size() - prefix - 2U);
    }
    std::string output;
    output.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\'' && index + 1U < value.size() && value[index + 1U] == '\'') {
            output.push_back('\'');
            ++index;
        } else if (static_cast<unsigned char>(value[index]) >= 32U) {
            output.push_back(value[index]);
        }
    }
    if (output.size() > MaximumPathLength) output.resize(MaximumPathLength);
    return output;
}

Row splitTuple(const std::string& source) {
    Row columns;
    std::string token;
    bool quoted = false;
    for (std::size_t index = 0; index < source.size(); ++index) {
        const char character = source[index];
        if (character == '\'') {
            token.push_back(character);
            if (quoted && index + 1U < source.size() && source[index + 1U] == '\'') token.push_back(source[++index]);
            else quoted = !quoted;
            continue;
        }
        if (character == ',' && !quoted) {
            columns.push_back(trim(std::move(token)));
            token.clear();
            continue;
        }
        token.push_back(character);
    }
    if (quoted) throw std::runtime_error("Malformed quoted K_NPCPOS tuple");
    columns.push_back(trim(std::move(token)));
    return columns;
}

std::vector<Row> parseRows(const std::string& text) {
    std::vector<Row> rows;
    std::size_t searchPosition = 0U;
    while (true) {
        const std::size_t valuesPosition = text.find("VALUES", searchPosition);
        if (valuesPosition == std::string::npos) break;
        std::string tuple;
        bool inQuote = false;
        int depth = 0;
        std::size_t index = valuesPosition + 6U;
        for (; index < text.size(); ++index) {
            const char character = text[index];
            if (character == '\'') {
                if (depth > 0) tuple.push_back(character);
                if (inQuote && index + 1U < text.size() && text[index + 1U] == '\'') {
                    if (depth > 0) tuple.push_back(text[++index]);
                } else inQuote = !inQuote;
                continue;
            }
            if (!inQuote && depth == 0 && character == ';') {
                ++index;
                break;
            }
            if (!inQuote && depth == 0 && index + 11U <= text.size() && text.compare(index, 11U, "INSERT INTO") == 0) break;
            if (!inQuote && character == '(') {
                if (depth++ == 0) {
                    tuple.clear();
                    continue;
                }
            }
            if (!inQuote && character == ')') {
                if (depth <= 0) throw std::runtime_error("Malformed K_NPCPOS tuple depth");
                if (--depth == 0) {
                    rows.push_back(splitTuple(tuple));
                    if (rows.size() > MaximumRecords) throw std::runtime_error("K_NPCPOS record limit exceeded");
                    tuple.clear();
                    continue;
                }
            }
            if (depth > 0) tuple.push_back(character);
        }
        if (depth != 0 || inQuote) throw std::runtime_error("Malformed K_NPCPOS tuple stream");
        searchPosition = std::max(index, valuesPosition + 6U);
    }
    return rows;
}

std::int64_t integer(const Row& row, std::size_t column, std::size_t rowNumber) {
    if (column >= row.size()) throw std::runtime_error("K_NPCPOS row " + std::to_string(rowNumber) + " has too few columns");
    const std::string value = trim(row[column]);
    if (value.empty() || value == "NULL" || value == "null") return 0;
    try {
        std::size_t consumed = 0U;
        const auto result = std::stoll(value, &consumed, 10);
        if (consumed != value.size()) throw std::invalid_argument("trailing characters");
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error("K_NPCPOS row " + std::to_string(rowNumber) + " column "
                                 + std::to_string(column) + " has invalid integer: " + value);
    }
}

template <typename T>
T boundedUnsigned(const Row& row, std::size_t column, std::size_t rowNumber) {
    const std::int64_t raw = integer(row, column, rowNumber);
    if (raw <= 0) return static_cast<T>(0);
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<T>::max());
    return static_cast<T>(std::min<std::uint64_t>(static_cast<std::uint64_t>(raw), maximum));
}

std::int32_t boundedCoordinate(const Row& row, std::size_t column, std::size_t rowNumber) {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(integer(row, column, rowNumber), -1'000'000, 1'000'000));
}

std::filesystem::path setupFile(const std::filesystem::path& root) {
    const auto direct = root / "ManualSetup" / "6_InsertData_K_NPCPOS.sql";
    if (std::filesystem::is_regular_file(direct)) return direct;
    const auto nested = root / "6_InsertData_K_NPCPOS.sql";
    if (std::filesystem::is_regular_file(nested)) return nested;
    throw std::runtime_error("OpenKO setup file is missing: 6_InsertData_K_NPCPOS.sql");
}

} // namespace

OpenKoSpawnTable OpenKoSpawnTable::compile(const std::filesystem::path& databaseRoot) {
    OpenKoSpawnTable table;
    const auto rows = parseRows(readText(setupFile(databaseRoot)));
    table.records_.reserve(rows.size());
    for (std::size_t index = 0U; index < rows.size(); ++index) {
        const Row& row = rows[index];
        const std::size_t rowNumber = index + 1U;
        if (row.size() != 20U) {
            throw std::runtime_error("K_NPCPOS row " + std::to_string(rowNumber)
                                     + " has unexpected column count: " + std::to_string(row.size()));
        }
        OpenKoSpawnRecord record;
        record.zoneId = boundedUnsigned<std::uint16_t>(row, 0, rowNumber);
        record.npcId = boundedUnsigned<std::uint32_t>(row, 1, rowNumber);
        record.actType = boundedUnsigned<std::uint8_t>(row, 2, rowNumber);
        record.regenType = boundedUnsigned<std::uint8_t>(row, 3, rowNumber);
        record.specialType = boundedUnsigned<std::uint8_t>(row, 5, rowNumber);
        record.leftX = boundedCoordinate(row, 7, rowNumber);
        record.topZ = boundedCoordinate(row, 8, rowNumber);
        record.rightX = boundedCoordinate(row, 9, rowNumber);
        record.bottomZ = boundedCoordinate(row, 10, rowNumber);
        record.limitMinZ = boundedCoordinate(row, 11, rowNumber);
        record.limitMinX = boundedCoordinate(row, 12, rowNumber);
        record.limitMaxX = boundedCoordinate(row, 13, rowNumber);
        record.limitMaxZ = boundedCoordinate(row, 14, rowNumber);
        record.count = std::max<std::uint16_t>(1U, boundedUnsigned<std::uint16_t>(row, 15, rowNumber));
        record.respawnSeconds = std::max<std::uint16_t>(1U, boundedUnsigned<std::uint16_t>(row, 16, rowNumber));
        record.direction = boundedCoordinate(row, 17, rowNumber);
        record.path = cleanSqlString(row[19]);
        if (record.zoneId == 0U || record.npcId == 0U) continue;
        table.records_.push_back(std::move(record));
    }
    table.validate();
    return table;
}

OpenKoSpawnTable OpenKoSpawnTable::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Unable to open KOSPAWN file: " + path.string());
    std::string header;
    std::getline(input, header);
    if (header != "KOSPAWN1") throw std::runtime_error("Invalid KOSPAWN header");
    std::size_t count = 0U;
    input >> count;
    if (!input || count > MaximumRecords) throw std::runtime_error("Invalid KOSPAWN record count");
    OpenKoSpawnTable table;
    table.records_.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        OpenKoSpawnRecord record;
        unsigned int act = 0U, regen = 0U, special = 0U;
        input >> record.zoneId >> record.npcId >> act >> regen >> special
              >> record.leftX >> record.topZ >> record.rightX >> record.bottomZ
              >> record.limitMinZ >> record.limitMinX >> record.limitMaxX >> record.limitMaxZ
              >> record.count >> record.respawnSeconds >> record.direction >> std::quoted(record.path);
        if (!input || act > 255U || regen > 255U || special > 255U || record.path.size() > MaximumPathLength) {
            throw std::runtime_error("Invalid KOSPAWN record at index " + std::to_string(index));
        }
        record.actType = static_cast<std::uint8_t>(act);
        record.regenType = static_cast<std::uint8_t>(regen);
        record.specialType = static_cast<std::uint8_t>(special);
        table.records_.push_back(std::move(record));
    }
    std::string trailing;
    if (input >> trailing) throw std::runtime_error("Unexpected trailing KOSPAWN data");
    table.validate();
    return table;
}

void OpenKoSpawnTable::save(const std::filesystem::path& path) const {
    validate();
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) throw std::runtime_error("Unable to create KOSPAWN file: " + temporary.string());
    output << "KOSPAWN1\n" << records_.size() << '\n';
    for (const auto& record : records_) {
        output << record.zoneId << ' ' << record.npcId << ' '
               << static_cast<unsigned int>(record.actType) << ' '
               << static_cast<unsigned int>(record.regenType) << ' '
               << static_cast<unsigned int>(record.specialType) << ' '
               << record.leftX << ' ' << record.topZ << ' ' << record.rightX << ' ' << record.bottomZ << ' '
               << record.limitMinZ << ' ' << record.limitMinX << ' ' << record.limitMaxX << ' ' << record.limitMaxZ << ' '
               << record.count << ' ' << record.respawnSeconds << ' ' << record.direction << ' '
               << std::quoted(record.path) << '\n';
    }
    output.close();
    if (!output) throw std::runtime_error("Unable to finish KOSPAWN file: " + temporary.string());
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (!error) return;
    error.clear();
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) throw std::runtime_error("Unable to replace KOSPAWN file: " + path.string());
}

void OpenKoSpawnTable::validate() const {
    if (records_.empty()) throw std::runtime_error("KOSPAWN contains no records");
    if (records_.size() > MaximumRecords) throw std::runtime_error("KOSPAWN record limit exceeded");
    for (const auto& record : records_) {
        if (record.zoneId == 0U || record.npcId == 0U || record.count == 0U || record.respawnSeconds == 0U
            || record.path.size() > MaximumPathLength) {
            throw std::runtime_error("Invalid KOSPAWN record for NPC " + std::to_string(record.npcId));
        }
    }
}

} // namespace korework::data
