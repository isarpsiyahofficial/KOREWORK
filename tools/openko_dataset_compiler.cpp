#include "content/item_basic_table.hpp"
#include "data/openko_sql_compiler.hpp"

#include <array>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>

namespace {

std::optional<std::filesystem::path> locateAssetRoot(int argc,
                                                      char** argv,
                                                      const std::filesystem::path& output) {
    std::array<std::filesystem::path, 7> candidates {
        argc == 4 ? std::filesystem::path(argv[3]) : std::filesystem::path(),
        std::filesystem::current_path() / "upstream" / "ko-assets",
        std::filesystem::current_path() / "assets" / "ko",
        output.parent_path() / "assets" / "ko",
        output.parent_path().parent_path() / "assets" / "ko",
        output.parent_path().parent_path().parent_path() / "upstream" / "ko-assets",
        output.parent_path().parent_path().parent_path() / "assets" / "ko"
    };

    std::error_code error;
    for (const auto& candidate : candidates) {
        if (candidate.empty()) continue;
        if (std::filesystem::is_directory(candidate / "game", error)
            && std::filesystem::is_directory(candidate / "server", error)) {
            return std::filesystem::weakly_canonical(candidate, error);
        }
        error.clear();
    }
    return std::nullopt;
}

} // namespace

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
        const auto assetRoot = locateAssetRoot(argc, argv, output);
        if (assetRoot.has_value()) {
            itemTablePath = korework::content::ItemBasicTable::locate(*assetRoot);
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
        else std::cout << "Item table: unavailable; placeholder index retained\n";
        std::cout << "Output: " << output.generic_string() << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "OpenKO dataset compilation failed: " << exception.what() << '\n';
        return 1;
    }
}
