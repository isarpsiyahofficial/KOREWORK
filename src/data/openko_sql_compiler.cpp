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
        unsigned char character = static_cast<unsigned char>(value[index]);
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
    for (unsigned char character : output) {
        const bool currentSpace = std::isspace(character) != 0;
        if (!currentSpace || !previousSpace) collapsed.push_back(currentSpace ? ' ' : static_cast<char>(character));
        previousSpace = currentSpace;
    }
    return trim(std::move(collapsed));
}

std::vector<Row> parseRows(const std::string& text) {
    const auto valuesPosition = text.find("VALUES");
    if (valuesPosition == std::string::npos) return {};

    std::vector<Row> rows;
    std::string tuple;
    bool inQuote = false;
    int depth = 0;

    auto splitTuple = [](const std::string& source) {
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
        columns.push_back(trim(std::move(token)));
        return columns;
    };

    for (std::size_t index = valuesPosition + 6U; index < text.size(); ++index) {
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
        if (!inQuote && character == '(') {
            if (depth++ == 0) {
                tuple.clear();
                continue;
            }
        }
        if (!inQuote && character == ')') {
            if (--depth == 0) {
                rows.push_back(splitTuple(tuple));
                tuple.clear();
                continue;
            }
        }
        if (depth > 0) tuple.push_back(character);
    }
    if (depth != 0 || inQuote) throw std::runtime_error("Malformed OpenKO SQL tuple stream");
    return rows;
}

std::int64_t integer(const Row& row, std::size_t index) {
    if (index >= row.size()) throw std::runtime_error("OpenKO SQL row has too few columns");
    const std::string value = trim(row[index]);
    if (value.empty() || value == "NULL" || value == "null") return 0;
    std::size_t consumed = 0;
    const auto parsed = std::stoll(value, &consumed, 10);
    if (consumed != value.size()) throw std::runtime_error("Invalid OpenKO integer: " + value);
    return parsed;
}

float floating(const Row& row, std::size_t index) {
    if (index >= row.size()) throw std::runtime_error("OpenKO SQL row has too few columns");
    const std::string value = trim(row[index]);
    if (value.empty() || value == "NULL" || value == "null") return 0.0F;
    std::size_t consumed = 0;
    const float parsed = std::stof(value, &consumed);
    if (consumed != value.size() || !std::isfinite(parsed)) throw std::runtime_error("Invalid OpenKO float: " + value);
    return parsed;
}

template <typename T>
T unsignedValue(const Row& row, std::size_t index) {
    const auto value = integer(row, index);
    if (value < 0 || static_cast<std::uint64_t>(value) > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
        throw std::runtime_error("OpenKO unsigned value outside target range");
    }
    return static_cast<T>(value);
}

template <typename T>
T signedValue(const Row& row, std::size_t index) {
    const auto value = integer(row, index);
    if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min())
        || value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
        throw std::runtime_error("OpenKO signed value outside target range");
    }
    return static_cast<T>(value);
}

std::filesystem::path setupFile(const std::filesystem::path& root, const char* filename) {
    const auto direct = root / "ManualSetup" / filename;
    if (std::filesystem::is_regular_file(direct)) return direct;
    const auto nested = root / filename;
    if (std::filesystem::is_regular_file(nested)) return nested;
    throw std::runtime_error(std::string("OpenKO setup file is missing: ") + filename);
}

void compileMonsters(GameDataPack& pack, const std::filesystem::path& root) {
    const auto rows = parseRows(readText(setupFile(root, "6_InsertData_K_MONSTER.sql")));
    for (const auto& row : rows) {
        if (row.size() != 46U) throw std::runtime_error("Unexpected K_MONSTER column count: " + std::to_string(row.size()));
        MonsterRecord record;
        record.id = unsignedValue<std::uint32_t>(row, 0);
        record.sid = unsignedValue<std::uint16_t>(row, 0);
        record.name = cleanSqlString(row[1]);
        record.modelId = unsignedValue<std::uint32_t>(row, 2);
        record.sizePercent = std::clamp<unsignedValue<std::uint16_t>(row, 3), 1U, 1000U);
        record.rightHandItem = unsignedValue<std::uint32_t>(row, 4);
        record.leftHandItem = unsignedValue<std::uint32_t>(row, 5);
        record.group = unsignedValue<std::uint8_t>(row, 6);
        record.rank = unsignedValue<std::uint8_t>(row, 10);
        record.title = unsignedValue<std::uint8_t>(row, 11);
        record.level = std::max<std::uint16_t>(1U, unsignedValue<std::uint16_t>(row, 13));
        record.experience = unsignedValue<std::uint32_t>(row, 14);
        record.loyalty = unsignedValue<std::uint32_t>(row, 15);
        record.hp = std::max<std::uint32_t>(1U, unsignedValue<std::uint32_t>(row, 16));
        record.mp = unsignedValue<std::uint32_t>(row, 17);
        record.attack = unsignedValue<std::uint16_t>(row, 18);
        record.defense = unsignedValue<std::uint16_t>(row, 19);
        record.hitRate = unsignedValue<std::uint16_t>(row, 20);
        record.evasionRate = unsignedValue<std::uint16_t>(row, 21);
        record.damage = unsignedValue<std::uint16_t>(row, 22);
        record.attackDelayMs = std::max<std::uint16_t>(100U, unsignedValue<std::uint16_t>(row, 23));
        record.movementSpeed = static_cast<float>(unsignedValue<std::uint8_t>(row, 24)) * 0.55F;
        record.runningSpeed = static_cast<float>(unsignedValue<std::uint8_t>(row, 25)) * 0.70F;
        record.skill1 = unsignedValue<std::uint32_t>(row, 27);
        record.skill2 = unsignedValue<std::uint32_t>(row, 28);
        record.skill3 = unsignedValue<std::uint32_t>(row, 29);
        record.fireResistance = signedValue<std::int16_t>(row, 30);
        record.coldResistance = signedValue<std::int16_t>(row, 31);
        record.lightningResistance = signedValue<std::int16_t>(row, 32);
        record.magicResistance = signedValue<std::int16_t>(row, 33);
        record.diseaseResistance = signedValue<std::int16_t>(row, 34);
        record.poisonResistance = signedValue<std::int16_t>(row, 35);
        record.attackRange = static_cast<float>(unsignedValue<std::uint8_t>(row, 38)) * 0.30F;
        record.searchRange = static_cast<float>(unsignedValue<std::uint8_t>(row, 39)) * 1.80F;
        record.chaseRange = static_cast<float>(unsignedValue<std::uint8_t>(row, 40)) * 0.35F;
        record.money = unsignedValue<std::uint32_t>(row, 41);
        record.dropTableId = unsignedValue<std::uint32_t>(row, 42);
        if (record.name.empty()) record.name = "KO Monster " + std::to_string(record.sid);
        pack.monsters.push_back(std::move(record));
    }
}

void compileDrops(GameDataPack& pack, const std::filesystem::path& root, std::set<std::uint32_t>& discoveredItems) {
    const auto rows = parseRows(readText(setupFile(root, "6_InsertData_K_MONSTER_ITEM.sql")));
    for (const auto& row : rows) {
        if (row.size() != 11U) throw std::runtime_error("Unexpected K_MONSTER_ITEM column count: " + std::to_string(row.size()));
        DropTableRecord table;
        table.id = unsignedValue<std::uint32_t>(row, 0);
        for (std::size_t slot = 0; slot < table.entries.size(); ++slot) {
            table.entries[slot].itemId = unsignedValue<std::uint32_t>(row, 1U + slot * 2U);
            table.entries[slot].chance = std::min<std::uint16_t>(10'000U, unsignedValue<std::uint16_t>(row, 2U + slot * 2U));
            if (table.entries[slot].itemId != 0U) discoveredItems.insert(table.entries[slot].itemId);
        }
        pack.dropTables.push_back(table);
    }
}

void compileSkills(GameDataPack& pack, const std::filesystem::path& root, std::set<std::uint32_t>& discoveredItems) {
    const auto rows = parseRows(readText(setupFile(root, "6_InsertData_MAGIC.sql")));
    for (const auto& row : rows) {
        if (row.size() != 24U) throw std::runtime_error("Unexpected MAGIC column count: " + std::to_string(row.size()));
        SkillRecord record;
        record.id = unsignedValue<std::uint32_t>(row, 0);
        record.name = cleanSqlString(row[1]);
        record.description = cleanSqlString(row[3]);
        record.userAnimation = unsignedValue<std::uint16_t>(row, 4);
        record.targetAnimation = unsignedValue<std::uint16_t>(row, 5);
        record.selfEffect = unsignedValue<std::uint16_t>(row, 6);
        record.projectileEffect = unsignedValue<std::uint16_t>(row, 7);
        record.targetEffect = unsignedValue<std::uint16_t>(row, 8);
        record.targetType = unsignedValue<std::uint8_t>(row, 9);
        record.moral = unsignedValue<std::uint8_t>(row, 9);
        record.skillLevel = unsignedValue<std::uint16_t>(row, 10);
        record.requiredSkillPoints = unsignedValue<std::uint16_t>(row, 11);
        record.manaCost = unsignedValue<std::uint16_t>(row, 12);
        record.hpCost = unsignedValue<std::uint16_t>(row, 13);
        record.requiredItem = unsignedValue<std::uint32_t>(row, 15);
        record.castTime = unsignedValue<std::uint16_t>(row, 16);
        record.cooldown = unsignedValue<std::uint16_t>(row, 17);
        record.successRate = std::min<std::uint8_t>(100U, unsignedValue<std::uint8_t>(row, 18));
        record.type1 = unsignedValue<std::uint8_t>(row, 19);
        record.type2 = unsignedValue<std::uint8_t>(row, 20);
        record.range = unsignedValue<std::uint16_t>(row, 21);
        if (record.name.empty()) record.name = "KO Skill " + std::to_string(record.id);
        if (record.requiredItem != 0U) discoveredItems.insert(record.requiredItem);
        pack.skills.push_back(std::move(record));
    }
}

void compileClasses(GameDataPack& pack, const std::filesystem::path& root) {
    const auto rows = parseRows(readText(setupFile(root, "6_InsertData_COEFFICIENT.sql")));
    for (const auto& row : rows) {
        if (row.size() != 15U) throw std::runtime_error("Unexpected COEFFICIENT column count: " + std::to_string(row.size()));
        ClassCoefficientRecord record;
        record.classId = unsignedValue<std::uint16_t>(row, 0);
        record.shortSword = floating(row, 1); record.sword = floating(row, 2); record.axe = floating(row, 3);
        record.club = floating(row, 4); record.spear = floating(row, 5); record.pole = floating(row, 6);
        record.staff = floating(row, 7); record.bow = floating(row, 8); record.hp = floating(row, 9);
        record.mp = floating(row, 10); record.sp = floating(row, 11); record.armor = floating(row, 12);
        record.hitRate = floating(row, 13); record.evasionRate = floating(row, 14);
        pack.classes.push_back(record);
    }
}

void createItemIndex(GameDataPack& pack, const std::set<std::uint32_t>& discoveredItems) {
    for (const auto itemId : discoveredItems) {
        if (itemId == 0U) continue;
        ItemRecord record;
        record.id = itemId;
        record.name = "KO Item #" + std::to_string(itemId);
        record.description = "Imported from the OpenKO Fire Drake drop/skill dataset.";
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
