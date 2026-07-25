#include "content/item_basic_table.hpp"
#include "data/openko_sql_compiler.hpp"

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: korework_dataset_compiler <OpenKO-db root> <output.kopack> [KO asset root]\n";
        return 2;
    }

    try {
        const std::filesystem::path source = argv[1];
        const std::filesystem::path output = argv[2];
        auto pack = korework::data::OpenKoSqlCompiler::compile(source);

        std::filesystem::path itemTablePath;
        if (argc == 4) {
            const std::filesystem::path assetRoot = argv[3];
            itemTablePath = korework::content::ItemBasicTable::locate(assetRoot);
            const auto itemTable = korework::content::ItemBasicTable::load(itemTablePath);
            pack.items = itemTable.items();
            pack.validate();
        }

        pack.save(output);
        std::cout << "KOPACK version: " << korework::data::GameDataPack::CurrentVersion << '\n'
                  << "Monsters: " << pack.monsters.size() << '\n'
                  << "Drop tables: " << pack.dropTables.size() << '\n'
                  << "Skills: " << pack.skills.size() << '\n'
                  << "Classes: " << pack.classes.size() << '\n'
                  << "Items: " << pack.items.size() << '\n';
        if (!itemTablePath.empty()) std::cout << "Item table: " << itemTablePath.generic_string() << '\n';
        std::cout << "Output: " << output.generic_string() << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "OpenKO dataset compilation failed: " << exception.what() << '\n';
        return 1;
    }
}
