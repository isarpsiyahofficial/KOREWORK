#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

namespace korework {

enum class PlayerClass : std::uint16_t {
    Warrior = 101,
    Rogue = 102,
    Mage = 103,
    Priest = 104
};

struct CharacterSlot {
    bool occupied = false;
    std::string name;
    PlayerClass playerClass = PlayerClass::Warrior;
    int level = 1;
};

class OfflineRoster final {
public:
    static constexpr std::size_t SlotCount = 3U;

    OfflineRoster();

    bool load();
    bool save() const;
    bool create(std::size_t slot, std::string name, PlayerClass playerClass);
    bool remove(std::size_t slot);
    bool updateLevel(std::size_t slot, int level);

    [[nodiscard]] const std::array<CharacterSlot, SlotCount>& slots() const noexcept { return slots_; }
    [[nodiscard]] const CharacterSlot* slot(std::size_t index) const noexcept;
    [[nodiscard]] static const char* className(PlayerClass playerClass) noexcept;
    [[nodiscard]] static bool validClass(std::uint16_t classId) noexcept;
    [[nodiscard]] static std::filesystem::path storageRoot();

private:
    [[nodiscard]] static std::string sanitizeName(std::string name);

    std::array<CharacterSlot, SlotCount> slots_ {};
};

} // namespace korework
