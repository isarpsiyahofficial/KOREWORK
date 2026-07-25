#pragma once

#include "data/game_data_pack.hpp"

#include <filesystem>

namespace korework::data {

class OpenKoSqlCompiler final {
public:
    [[nodiscard]] static GameDataPack compile(const std::filesystem::path& databaseRoot);
    static void compileToFile(const std::filesystem::path& databaseRoot,
                              const std::filesystem::path& outputPath);
};

} // namespace korework::data
