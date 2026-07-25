#include "data/openko_spawn_table.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <set>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: korework_spawn_compiler <OpenKO-db root> <output.kospawn>\n";
        return 2;
    }
    try {
        const auto table = korework::data::OpenKoSpawnTable::compile(argv[1]);
        table.save(argv[2]);
        std::set<std::uint16_t> zones;
        std::size_t monsterRows = 0U;
        std::size_t requestedCreatures = 0U;
        for (const auto& record : table.records()) {
            zones.insert(record.zoneId);
            if (record.actType < 100U) {
                ++monsterRows;
                requestedCreatures += record.count;
            }
        }
        std::cout << "Spawns: " << table.records().size() << '\n'
                  << "Monster spawn rows: " << monsterRows << '\n'
                  << "Requested creatures: " << requestedCreatures << '\n'
                  << "Zones: " << zones.size() << '\n'
                  << "Output: " << std::filesystem::path(argv[2]).generic_string() << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "OpenKO spawn compilation failed: " << exception.what() << '\n';
        return 1;
    }
}
