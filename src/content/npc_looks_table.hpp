#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace korework::content {

struct NpcLookRecord {
    std::uint32_t id = 0;
    std::string name;
    std::string jointReference;
    std::string animationReference;
    std::vector<std::string> partReferences;
    std::string skinReference;
    std::string characterReference;
    std::string effectPlugReference;
    std::int32_t rightHandJoint = -1;
    std::int32_t leftHandJoint = -1;
    std::int32_t leftForearmJoint = -1;
    std::int32_t cloakJoint = -1;
};

class NpcLooksTable final {
public:
    [[nodiscard]] static NpcLooksTable load(const std::filesystem::path& encryptedTablePath);
    [[nodiscard]] const NpcLookRecord* find(std::uint32_t id) const noexcept;
    [[nodiscard]] const std::vector<NpcLookRecord>& records() const noexcept { return records_; }

private:
    std::vector<NpcLookRecord> records_;
    std::unordered_map<std::uint32_t, std::size_t> index_;
};

} // namespace korework::content
