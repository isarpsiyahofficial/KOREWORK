#include "data/openko_sql_compiler.hpp"

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: korework_dataset_compiler <OpenKO-db root> <output.kopack>\n";
        return 2;
    }

    try {
        const std::filesystem::path source = argv[1];
        const std::filesystem::path output = argv[2];
        const auto pack = korework::data::OpenKoSqlCompiler::compile(source);
        pack.save(output);
        std::cout << "KOPACK version: " << korework::data::GameDataPack::CurrentVersion << '\n'
                  << "Monsters: " << pack.monsters.size() << '\n'
                  << "Drop tables: " << pack.dropTables.size() << '\n'
                  << "Skills: " << pack.skills.size() << '\n'
                  << "Classes: " << pack.classes.size() << '\n'
                  << "Items indexed: " << pack.items.size() << '\n'
                  << "Output: " << output.generic_string() << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "OpenKO dataset compilation failed: " << exception.what() << '\n';
        return 1;
    }
}
