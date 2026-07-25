#include "data/openko_sql_compiler.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void writeMonsterBatch(std::ofstream& out, int firstIndex, int count) {
    out << "INSERT INTO [K_MONSTER] VALUES\n";
    for (int offset = 0; offset < count; ++offset) {
        const int index = firstIndex + offset;
        const int sid = 100 + index;
        const bool sentinel = index == 0;
        out << '(' << sid << ", N'Monster " << sid << "', " << (100 + index % 5)
            << ", 100, " << (sentinel ? -1 : 0) << ", " << (sentinel ? -1 : 0)
            << ", 0, 5, 0, 1, 0, 0, 0, " << (1 + index % 30)
            << ", 725, 0, " << (32 + index) << ", 120, 10, 31, 3, 4, 3, 1500, 2, 3, 5000, "
            << (sentinel ? -1 : 0) << ", " << (sentinel ? -1 : 0) << ", " << (sentinel ? -1 : 0) << ", "
            << "18, 18, 18, 13, 18, 18, 18, 70, 5, 5, 45, 73, " << sid << ", 0, 0, 0)"
            << (offset + 1 == count ? ";\n" : ",\n");
    }
}

void writeMonsters(const std::filesystem::path& path) {
    std::ofstream out(path);
    writeMonsterBatch(out, 0, 25);
    writeMonsterBatch(out, 25, 25);
}

void writeDropBatch(std::ofstream& out, int firstIndex, int count) {
    out << "INSERT INTO [K_MONSTER_ITEM] VALUES\n";
    for (int offset = 0; offset < count; ++offset) {
        const int index = firstIndex + offset;
        const int id = 100 + index;
        const bool sentinel = index == 0;
        out << '(' << id << ", " << (1000 + index) << ", 5000, "
            << (sentinel ? -1 : 0) << ", " << (sentinel ? 5000 : 0)
            << ", 0, 0, 0, 0, 0, 0)"
            << (offset + 1 == count ? ";\n" : ",\n");
    }
}

void writeDrops(const std::filesystem::path& path) {
    std::ofstream out(path);
    writeDropBatch(out, 0, 10);
    writeDropBatch(out, 10, 10);
}

void writeMagicBatch(std::ofstream& out, int firstIndex, int count) {
    out << "INSERT INTO [MAGIC] VALUES\n";
    for (int offset = 0; offset < count; ++offset) {
        const int index = firstIndex + offset;
        const int id = 101001 + index;
        const bool sentinel = index == 0;
        out << '(' << id << ", N'Skill " << id << "', N'', N'Description', "
            << (sentinel ? -1 : 0) << ", " << (sentinel ? -1 : 0) << ", "
            << (sentinel ? -1 : 0) << ", " << (sentinel ? -1 : 0) << ", 10002, 7, "
            << (1 + index) << ", 1010, 5, 0, 0, " << (sentinel ? -1 : 0)
            << ", " << (sentinel ? -1 : 0) << ", 31, 100, 1, 0, 15, 1, 0)"
            << (offset + 1 == count ? ";\n" : ",\n");
    }
}

void writeMagic(const std::filesystem::path& path) {
    std::ofstream out(path);
    writeMagicBatch(out, 0, 10);
    writeMagicBatch(out, 10, 10);
}

void writeClasses(const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "INSERT INTO [COEFFICIENT] VALUES\n"
        << "(101, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 1, 0.1, 0.1),\n"
        << "(102, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 1, 0.1, 0.1);\n"
        << "INSERT INTO [COEFFICIENT] VALUES\n"
        << "(103, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 1, 0.1, 0.1),\n"
        << "(104, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 1, 0.1, 0.1);\n";
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "korework_openko_sql_smoke";
    const auto setup = root / "ManualSetup";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(setup);
    writeMonsters(setup / "6_InsertData_K_MONSTER.sql");
    writeDrops(setup / "6_InsertData_K_MONSTER_ITEM.sql");
    writeMagic(setup / "6_InsertData_MAGIC.sql");
    writeClasses(setup / "6_InsertData_COEFFICIENT.sql");

    const auto pack = korework::data::OpenKoSqlCompiler::compile(root);
    assert(pack.monsters.size() == 50U);
    assert(pack.skills.size() == 20U);
    assert(pack.classes.size() == 4U);
    assert(pack.dropTables.size() == 20U);
    assert(!pack.items.empty());
    assert(pack.monsters.front().name == "Monster 100");
    assert(pack.monsters.front().dropTableId == 100U);
    assert(pack.monsters.front().rightHandItem == 0U);
    assert(pack.monsters.front().leftHandItem == 0U);
    assert(pack.monsters.front().skill1 == 0U);
    assert(pack.dropTables.front().entries.front().chance == 5000U);
    assert(pack.dropTables.front().entries[1].itemId == 0U);
    assert(pack.dropTables.front().entries[1].chance == 0U);
    assert(pack.skills.front().userAnimation == 0U);
    assert(pack.skills.front().targetAnimation == 0U);
    assert(pack.skills.front().requiredItem == 0U);
    assert(pack.skills.front().castTime == 0U);

    const auto output = root / "game_data.kopack";
    pack.save(output);
    const auto loaded = korework::data::GameDataPack::load(output);
    assert(loaded.monsters.size() == pack.monsters.size());
    assert(loaded.skills.size() == pack.skills.size());
    assert(loaded.classes.size() == pack.classes.size());
    assert(loaded.dropTables.size() == pack.dropTables.size());
    assert(loaded.monsters.front().rightHandItem == 0U);
    assert(loaded.skills.front().requiredItem == 0U);
    std::filesystem::remove_all(root);
    return 0;
}
