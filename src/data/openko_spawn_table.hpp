#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace korework::data {

struct OpenKoSpawnRecord {
    std::uint16_t zoneId = 0;
    std::uint32_t npcId = 0;
    std::uint8_t actType = 0;
    std::uint8_t regenType = 0;
    std::uint8_t specialType = 0;
    std::int32_t leftX = 0;
    std::int32_t topZ = 0;
    std::int32_t rightX = 0;
    std::int32_t bottomZ = 0;
    std::int32_t limitMinZ = 0;
    std::int32_t limitMinX = 0;
    std::int32_t limitMaxX = 0;
    std::int32_t limitMaxZ = 0;
    std::uint16_t count = 1;
    std::uint16_t respawnSeconds = 30;
    std::int32_t direction = 0;
    std::string path;
};

class OpenKoSpawnTable final {
public:
    [[nodiscard]] static OpenKoSpawnTable compile(const std::filesystem::path& databaseRoot);
    [[nodiscard]] static OpenKoSpawnTable load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;

    [[nodiscard]] const std::vector<OpenKoSpawnRecord>& records() const noexcept { return records_; }

private:
    void validate() const;
    std::vector<OpenKoSpawnRecord> records_;
};

} // namespace korework::data
