#include "data/openko_sql_compiler.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace korework::data {
namespace {

using Row = std::vector<std::string>;

std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to read OpenKO data file: " + path.string());
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
    if (value == "NULL" || value == "null") return {};
    std::size_t prefix = 0;
    if (value.size() >= 2U && (value[0] == 'N' || value[0] == 'n') && value[1] == '\'') prefix = 1U;
    if (value.size() >= prefix + 2U && value[prefix] == '\'' && value.back() == '\'') {
        value = value.substr(prefix + 1U, value.size() - prefix - 2U);
    }

    std::string output;
    output.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(value[index]);
        if (character == '\'' && index + 1U < value.size() && value[index + 1U] == '\'') {
            output.push_back('\'');
            ++index;
            continue;
        }
        if (character == 0U) continue;
        if (character < 32U && character != '\t' && character != '\n' && character != '\r') continue;
        output.push_back(static_cast<char>(character));
    }

    for (char& character : output) if (character == '\t' || character == '\n' || character == '\r') character = ' ';
    std::string collapsed;
    collapsed.reserve(output.size());
    bool previousSpace = false;
    for (const unsigned char character : output) {
        const bool currentSpace = std::isspace(character) != 0;
        if (!currentSpace || !previousSpace) collapsed.push_back(currentSpace ? ' ' : static_cast<char>(character));
        previousSpace = currentSpace;
    }
    return trim(std::move(collapsed));
}

Row splitTuple(const std::string& source) {
    Row columns;
    std::string token;
    bool quoted = false;
    for (std::size_t index = 0; index < source.size(); ++index) {
        const char character = source[index];
        if (character == '\'') {
            token.push_back(character);
            if (quoted && index + 1U < source.size() && source[index + 1U] == '\'') {
                token.push_back(source[++index]);
            } else {
                quoted = !quoted;
            }
            continue;
        }
        if (character == ',' && !quoted) {
            columns.push_back(trim(std::move(token)));
            token.clear();
            continue;
        }
        token.push_back(character);
    }
    if (quoted) throw std::runtime_error("Malformed quoted OpenKO SQL tuple");
    columns.push_back(trim(std::move(token)));
    return columns;
}

std::vector<Row> parseRows(const std::string& text) {
    std::vector<Row> rows;
    std::size_t searchPosition = 0;

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
                } else {
                    inQuote = !inQuote;
                }
                continue;
            }
            if (!inQuote && depth == 0 && character == ';') {
                ++index;
                break;
            }
            if (!inQuote && depth == 0 && index + 11U <= text.size()
                && text.compare(index, 11U, "INSERT INTO") == 0) {
                break;
            }
            if (!inQuote && character == '(') {
                if (depth++ == 0) {
                    tuple.clear();
                    continue;
                }
            }
            if (!inQuote && character == ')') {
                if (depth <= 0) throw std::runtime_error("Malformed OpenKO SQL tuple depth");
                if (--depth == 0) {
                    rows.push_back(splitTuple(tuple));
                    tuple.clear();
                    continue;
                }
            }
            if (depth > 0) tuple.push_back(character);
        }
        if (depth != 0 || inQuote) throw std::runtime_error("Malformed OpenKO SQL tuple stream");
        searchPosition = std::max(index, valuesPosition + 6U);
    }
    return rows;
}

std::int64_t integer(const Row& row, std::size_t index, std::string_view table, std::size_t rowNumber) {
    if (index >= row.size()) {
        throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber)
                                 + " has too few columns; requested column " + std::to_string(index));
    }
    const std::string value = trim(row[index]);
    if (value.empty() || value == "NULL" || value == "null") return 0;
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoll(value, &consumed, 10);
        if (consumed != value.size()) throw std::invalid_argument("trailing characters");
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber)
                                 + " column " + std::to_string(index) + " has invalid integer: " + value);
    }
}

float floating(const Row& row, std::size_t index, std::string_view table, std::size_t rowNumber) {
    if (index >= row.size()) {
        throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber)
                                 + " has too few columns; requested column " + std::to_string(index));
    }
    const std::string value = trim(row[index]);
    if (value.empty() || value == "NULL" || value == "null") return 0.0F;
    try {
        std::size_t consumed = 0;
        const float parsed = std::stof(value, &consumed);
        if (consumed != value.size() || !std::isfinite(parsed)) throw std::invalid_argument("invalid float");
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber)
                                 + " column " + std::to_string(index) + " has invalid float: " + value);
    }
}

template <typename T>
T strictUnsigned(const Row& row, std::size_t index, std::string_view table, std::size_t rowNumber) {
    const std::int64_t value = integer(row, index, table, rowNumber);
    if (value <= 0 || static_cast<std::uint64_t>(value) > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
        throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber)
                                 + " column " + std::to_string(index) + " has invalid required id: " + std::to_string(value));
    }
    return static_cast<T>(value);
}

template <typename T>
T boundedUnsigned(const Row& row, std::size_t index, std::string_view table, std::size_t rowNumber,
                  std::uint64_t maximum = static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    const std::int64_t raw = integer(row, index, table, rowNumber);
    if (raw <= 0) return static_cast<T>(0);
    const std::uint64_t value = std::min<std::uint64_t>(static_cast<std::uint64_t>(raw), maximum);
    return static_cast<T>(value);
}

template <typename T>
T boundedSigned(const Row& row, std::size_t index, std::string_view table, std::size_t rowNumber) {
    const std::int64_t raw = integer(row, index, table, rowNumber);
    const std::int64_t minimum = static_cast<std::int64_t>(std::numeric_limits<T>::min());
    const std::int64_t maximum = static_cast<std::int64_t>(std::numeric_limits<T>::max());
    return static_cast<T>(std::clamp(raw, minimum, maximum));
}

std::filesystem::path setupFile(const std::filesystem::path& root, const char* filename) {
    const auto direct = root / "ManualSetup" / filename;
    if (std::filesystem::is_regular_file(direct)) return direct;
    const auto nested = root / filename;
    if (std::filesystem::is_regular_file(nested)) return nested;
    throw std::runtime_error(std::string("OpenKO setup file is missing: ") + filename);
}

void compileMonsters(GameDataPack& pack, const std::filesystem::path& root) {
    constexpr std::string_view table = "K_MONSTER";
    const auto rows = parseRows(readText(setupFile(root, "6_InsertData_K_MONSTER.sql")));
    for (std::size_t rowNumber = 0; rowNumber < rows.size(); ++rowNumber) {
        const auto& row = rows[rowNumber];
        if (row.size() != 46U) {
            throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber + 1U)
                                     + " has unexpected column count: " + std::to_string(row.size()));
        }
        const std::size_t displayRow = rowNumber + 1U;
        MonsterRecord record;
        record.id = strictUnsigned<std::uint32_t>(row, 0, table, displayRow);
        record.sid = static_cast<std::uint16_t>(std::min<std::uint32_t>(record.id, std::numeric_limits<std::uint16_t>::max()));
        record.name = cleanSqlString(row[1]);
        record.modelId = boundedUnsigned<std::uint32_t>(row, 2, table, displayRow);
        record.sizePercent = std::clamp<std::uint16_t>(boundedUnsigned<std::uint16_t>(row, 3, table, displayRow), 1U, 1000U);
        record.rightHandItem = boundedUnsigned<std::uint32_t>(row, 4, table, displayRow);
        record.leftHandItem = boundedUnsigned<std::uint32_t>(row, 5, table, displayRow);
        record.group = boundedUnsigned<std::uint8_t>(row, 6, table, displayRow);
        record.rank = boundedUnsigned<std::uint8_t>(row, 10, table, displayRow);
        record.title = boundedUnsigned<std::uint8_t>(row, 11, table, displayRow);
        record.level = std::max<std::uint16_t>(1U, boundedUnsigned<std::uint16_t>(row, 13, table, displayRow));
        record.experience = boundedUnsigned<std::uint32_t>(row, 14, table, displayRow);
        record.loyalty = boundedUnsigned<std::uint32_t>(row, 15, table, displayRow);
        record.hp = std::max<std::uint32_t>(1U, boundedUnsigned<std::uint32_t>(row, 16, table, displayRow));
        record.mp = boundedUnsigned<std::uint32_t>(row, 17, table, displayRow);
        record.attack = boundedUnsigned<std::uint16_t>(row, 18, table, displayRow);
        record.defense = boundedUnsigned<std::uint16_t>(row, 19, table, displayRow);
        record.hitRate = boundedUnsigned<std::uint16_t>(row, 20, table, displayRow);
        record.evasionRate = boundedUnsigned<std::uint16_t>(row, 21, table, displayRow);
        record.damage = boundedUnsigned<std::uint16_t>(row, 22, table, displayRow);
        record.attackDelayMs = std::max<std::uint16_t>(100U, boundedUnsigned<std::uint16_t>(row, 23, table, displayRow));
        record.movementSpeed = static_cast<float>(boundedUnsigned<std::uint8_t>(row, 24, table, displayRow)) * 0.55F;
        record.runningSpeed = static_cast<float>(boundedUnsigned<std::uint8_t>(row, 25, table, displayRow)) * 0.70F;
        record.skill1 = boundedUnsigned<std::uint32_t>(row, 27, table, displayRow);
        record.skill2 = boundedUnsigned<std::uint32_t>(row, 28, table, displayRow);
        record.skill3 = boundedUnsigned<std::uint32_t>(row, 29, table, displayRow);
        record.fireResistance = boundedSigned<std::int16_t>(row, 30, table, displayRow);
        record.coldResistance = boundedSigned<std::int16_t>(row, 31, table, displayRow);
        record.lightningResistance = boundedSigned<std::int16_t>(row, 32, table, displayRow);
        record.magicResistance = boundedSigned<std::int16_t>(row, 33, table, displayRow);
        record.diseaseResistance = boundedSigned<std::int16_t>(row, 34, table, displayRow);
        record.poisonResistance = boundedSigned<std::int16_t>(row, 35, table, displayRow);
        record.attackRange = static_cast<float>(boundedUnsigned<std::uint8_t>(row, 38, table, displayRow)) * 0.30F;
        record.searchRange = static_cast<float>(boundedUnsigned<std::uint8_t>(row, 39, table, displayRow)) * 1.80F;
        record.chaseRange = static_cast<float>(boundedUnsigned<std::uint8_t>(row, 40, table, displayRow)) * 0.35F;
        record.money = boundedUnsigned<std::uint32_t>(row, 41, table, displayRow);
        record.dropTableId = boundedUnsigned<std::uint32_t>(row, 42, table, displayRow);
        if (record.name.empty()) record.name = "KO Monster " + std::to_string(record.id);
        pack.monsters.push_back(std::move(record));
    }
}

void compileDrops(GameDataPack& pack, const std::filesystem::path& root, std::set<std::uint32_t>& discoveredItems) {
    constexpr std::string_view table = "K_MONSTER_ITEM";
    const auto rows = parseRows(readText(setupFile(root, "6_InsertData_K_MONSTER_ITEM.sql")));
    for (std::size_t rowNumber = 0; rowNumber < rows.size(); ++rowNumber) {
        const auto& row = rows[rowNumber];
        if (row.size() != 11U) {
            throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber + 1U)
                                     + " has unexpected column count: " + std::to_string(row.size()));
        }
        const std::size_t displayRow = rowNumber + 1U;
        DropTableRecord dropTable;
        dropTable.id = strictUnsigned<std::uint32_t>(row, 0, table, displayRow);
        for (std::size_t slot = 0; slot < dropTable.entries.size(); ++slot) {
            auto& entry = dropTable.entries[slot];
            entry.itemId = boundedUnsigned<std::uint32_t>(row, 1U + slot * 2U, table, displayRow);
            entry.chance = boundedUnsigned<std::uint16_t>(row, 2U + slot * 2U, table, displayRow, 10'000U);
            if (entry.itemId == 0U) entry.chance = 0U;
            else discoveredItems.insert(entry.itemId);
        }
        pack.dropTables.push_back(dropTable);
    }
}

void compileSkills(GameDataPack& pack, const std::filesystem::path& root, std::set<std::uint32_t>& discoveredItems) {
    constexpr std::string_view table = "MAGIC";
    const auto rows = parseRows(readText(setupFile(root, "6_InsertData_MAGIC.sql")));
    for (std::size_t rowNumber = 0; rowNumber < rows.size(); ++rowNumber) {
        const auto& row = rows[rowNumber];
        if (row.size() != 24U) {
            throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber + 1U)
                                     + " has unexpected column count: " + std::to_string(row.size()));
        }
        const std::size_t displayRow = rowNumber + 1U;
        SkillRecord record;
        record.id = strictUnsigned<std::uint32_t>(row, 0, table, displayRow);
        record.name = cleanSqlString(row[1]);
        record.description = cleanSqlString(row[3]);
        record.userAnimation = boundedUnsigned<std::uint16_t>(row, 4, table, displayRow);
        record.targetAnimation = boundedUnsigned<std::uint16_t>(row, 5, table, displayRow);
        record.selfEffect = boundedUnsigned<std::uint16_t>(row, 6, table, displayRow);
        record.projectileEffect = boundedUnsigned<std::uint16_t>(row, 7, table, displayRow);
        record.targetEffect = boundedUnsigned<std::uint16_t>(row, 8, table, displayRow);
        record.targetType = boundedUnsigned<std::uint8_t>(row, 9, table, displayRow);
        record.moral = record.targetType;
        record.skillLevel = boundedUnsigned<std::uint16_t>(row, 10, table, displayRow);
        record.requiredSkillPoints = boundedUnsigned<std::uint16_t>(row, 11, table, displayRow);
        record.manaCost = boundedUnsigned<std::uint16_t>(row, 12, table, displayRow);
        record.hpCost = boundedUnsigned<std::uint16_t>(row, 13, table, displayRow);
        record.requiredItem = boundedUnsigned<std::uint32_t>(row, 15, table, displayRow);
        record.castTime = boundedUnsigned<std::uint16_t>(row, 16, table, displayRow);
        record.cooldown = boundedUnsigned<std::uint16_t>(row, 17, table, displayRow);
        record.successRate = boundedUnsigned<std::uint8_t>(row, 18, table, displayRow, 100U);
        record.type1 = boundedUnsigned<std::uint8_t>(row, 19, table, displayRow);
        record.type2 = boundedUnsigned<std::uint8_t>(row, 20, table, displayRow);
        record.range = boundedUnsigned<std::uint16_t>(row, 21, table, displayRow);
        if (record.name.empty()) record.name = "KO Skill " + std::to_string(record.id);
        if (record.requiredItem != 0U) discoveredItems.insert(record.requiredItem);
        pack.skills.push_back(std::move(record));
    }
}

void compileClasses(GameDataPack& pack, const std::filesystem::path& root) {
    constexpr std::string_view table = "COEFFICIENT";
    const auto rows = parseRows(readText(setupFile(root, "6_InsertData_COEFFICIENT.sql")));
    for (std::size_t rowNumber = 0; rowNumber < rows.size(); ++rowNumber) {
        const auto& row = rows[rowNumber];
        if (row.size() != 15U) {
            throw std::runtime_error(std::string(table) + " row " + std::to_string(rowNumber + 1U)
                                     + " has unexpected column count: " + std::to_string(row.size()));
        }
        const std::size_t displayRow = rowNumber + 1U;
        ClassCoefficientRecord record;
        record.classId = strictUnsigned<std::uint16_t>(row, 0, table, displayRow);
        record.shortSword = std::clamp(floating(row, 1, table, displayRow), 0.0F, 1000.0F);
        record.sword = std::clamp(floating(row, 2, table, displayRow), 0.0F, 1000.0F);
        record.axe = std::clamp(floating(row, 3, table, displayRow), 0.0F, 1000.0F);
        record.club = std::clamp(floating(row, 4, table, displayRow), 0.0F, 1000.0F);
        record.spear = std::clamp(floating(row, 5, table, displayRow), 0.0F, 1000.0F);
        record.pole = std::clamp(floating(row, 6, table, displayRow), 0.0F, 1000.0F);
        record.staff = std::clamp(floating(row, 7, table, displayRow), 0.0F, 1000.0F);
        record.bow = std::clamp(floating(row, 8, table, displayRow), 0.0F, 1000.0F);
        record.hp = std::clamp(floating(row, 9, table, displayRow), 0.0F, 1000.0F);
        record.mp = std::clamp(floating(row, 10, table, displayRow), 0.0F, 1000.0F);
        record.sp = std::clamp(floating(row, 11, table, displayRow), 0.0F, 1000.0F);
        record.armor = std::clamp(floating(row, 12, table, displayRow), 0.0F, 1000.0F);
        record.hitRate = std::clamp(floating(row, 13, table, displayRow), 0.0F, 1000.0F);
        record.evasionRate = std::clamp(floating(row, 14, table, displayRow), 0.0F, 1000.0F);
        pack.classes.push_back(record);
    }
}

void createItemIndex(GameDataPack& pack, const std::set<std::uint32_t>& discoveredItems) {
    for (const std::uint32_t itemId : discoveredItems) {
        if (itemId == 0U) continue;
        ItemRecord record;
        record.id = itemId;
        record.name = "KO Item #" + std::to_string(itemId);
        record.description = "Imported from the OpenKO Fire Drake drop and skill dataset.";
        record.countable = true;
        record.iconId = itemId;
        record.appearanceId = itemId;
        pack.items.push_back(std::move(record));
    }
}

template <typename T, typename Getter>
void sortBy(std::vector<T>& records, Getter getter) {
    std::sort(records.begin(), records.end(), [&](const T& left, const T& right) { return getter(left) < getter(right); });
}

} // namespace

GameDataPack OpenKoSqlCompiler::compile(const std::filesystem::path& databaseRoot) {
    GameDataPack pack;
    std::set<std::uint32_t> discoveredItems;
    compileMonsters(pack, databaseRoot);
    compileDrops(pack, databaseRoot, discoveredItems);
    compileSkills(pack, databaseRoot, discoveredItems);
    compileClasses(pack, databaseRoot);
    createItemIndex(pack, discoveredItems);

    sortBy(pack.monsters, [](const MonsterRecord& record) { return record.id; });
    sortBy(pack.items, [](const ItemRecord& record) { return record.id; });
    sortBy(pack.skills, [](const SkillRecord& record) { return record.id; });
    sortBy(pack.classes, [](const ClassCoefficientRecord& record) { return record.classId; });
    sortBy(pack.dropTables, [](const DropTableRecord& record) { return record.id; });

    if (pack.monsters.size() < 50U || pack.skills.size() < 20U || pack.classes.size() < 4U || pack.dropTables.size() < 20U) {
        throw std::runtime_error("OpenKO dataset is incomplete; refusing to create a partial KOPACK");
    }
    pack.validate();
    return pack;
}

void OpenKoSqlCompiler::compileToFile(const std::filesystem::path& databaseRoot,
                                      const std::filesystem::path& outputPath) {
    compile(databaseRoot).save(outputPath);
}

} // namespace korework::data
