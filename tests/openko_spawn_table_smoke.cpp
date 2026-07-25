#include "data/openko_spawn_table.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "korework_openko_spawn_smoke";
    const auto setup = root / "ManualSetup";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(setup);

    std::ofstream sql(setup / "6_InsertData_K_NPCPOS.sql");
    sql << "INSERT INTO [K_NPCPOS] ([ZoneID], [NpcID], [ActType], [RegenType], [DungeonFamily], [SpecialType], [TrapNumber], [LeftX], [TopZ], [RightX], [BottomZ], [LimitMinZ], [LimitMinX], [LimitMaxX], [LimitMaxZ], [NumNPC], [RegTime], [byDirection], [DotCnt], [path]) VALUES\n"
        << "(1, 1350, 1, 0, 0, 0, 0, 1376, 1815, 1400, 1784, 1376, 1816, 1400, 1782, 5, 30, 0, 0, NULL),\n"
        << "(1, 26055, 100, 0, 0, 7, 1, 1735, 834, 1736, 833, 1734, 835, 1736, 833, 1, 1800, 90, 2, N'10 20 30 40'),\n"
        << "(21, 650, 1, 0, 0, 0, 0, 413, 918, 439, 894, 410, 921, 439, 893, 3, 23, 180, 0, NULL);\n";
    sql.close();

    const auto compiled = korework::data::OpenKoSpawnTable::compile(root);
    assert(compiled.records().size() == 3U);
    assert(compiled.records()[0].zoneId == 1U);
    assert(compiled.records()[0].npcId == 1350U);
    assert(compiled.records()[0].count == 5U);
    assert(compiled.records()[1].actType == 100U);
    assert(compiled.records()[1].path == "10 20 30 40");
    assert(compiled.records()[2].zoneId == 21U);
    assert(compiled.records()[2].respawnSeconds == 23U);

    const auto output = root / "world_spawns.kospawn";
    compiled.save(output);
    const auto loaded = korework::data::OpenKoSpawnTable::load(output);
    assert(loaded.records().size() == compiled.records().size());
    assert(loaded.records()[0].leftX == 1376);
    assert(loaded.records()[1].direction == 90);
    assert(loaded.records()[2].bottomZ == 894);

    std::filesystem::remove_all(root);
    return 0;
}
