#include "offline_roster.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>

namespace korework {

OfflineRoster::OfflineRoster() {
    (void) load();
}

std::filesystem::path OfflineRoster::storageRoot() {
#ifdef _WIN32
    const char* root = std::getenv("APPDATA");
    std::filesystem::path base = root != nullptr ? std::filesystem::path(root) : std::filesystem::current_path();
#else
    const char* root = std::getenv("HOME");
    std::filesystem::path base = root != nullptr ? std::filesystem::path(root) : std::filesystem::current_path();
#endif
    const std::filesystem::path directory = base / ".korework" / "saves";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return directory;
}

std::string OfflineRoster::sanitizeName(std::string name) {
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char character) {
        return character < 32U || character == '"' || character == '\\';
    }), name.end());
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.erase(name.begin());
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
    if (name.size() > 20U) name.resize(20U);
    return name;
}

bool OfflineRoster::validClass(std::uint16_t classId) noexcept {
    return classId >= static_cast<std::uint16_t>(PlayerClass::Warrior)
        && classId <= static_cast<std::uint16_t>(PlayerClass::Priest);
}

const char* OfflineRoster::className(PlayerClass playerClass) noexcept {
    switch (playerClass) {
        case PlayerClass::Warrior: return "Warrior";
        case PlayerClass::Rogue: return "Rogue";
        case PlayerClass::Mage: return "Mage";
        case PlayerClass::Priest: return "Priest";
    }
    return "Unknown";
}

const CharacterSlot* OfflineRoster::slot(std::size_t index) const noexcept {
    return index < slots_.size() && slots_[index].occupied ? &slots_[index] : nullptr;
}

bool OfflineRoster::load() {
    std::ifstream input(storageRoot() / "roster.koroster");
    if (!input) return false;
    std::string header;
    std::getline(input, header);
    if (header != "KOREWORK_ROSTER_V1") return false;

    std::array<CharacterSlot, SlotCount> loaded {};
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        int occupied = 0;
        std::uint16_t classId = 0;
        input >> occupied >> std::quoted(loaded[index].name) >> classId >> loaded[index].level;
        if (!input || (occupied != 0 && occupied != 1)) return false;
        loaded[index].occupied = occupied != 0;
        if (loaded[index].occupied) {
            loaded[index].name = sanitizeName(std::move(loaded[index].name));
            if (loaded[index].name.empty() || !validClass(classId) || loaded[index].level < 1 || loaded[index].level > 83) return false;
            loaded[index].playerClass = static_cast<PlayerClass>(classId);
        }
    }
    slots_ = std::move(loaded);
    return true;
}

bool OfflineRoster::save() const {
    const std::filesystem::path path = storageRoot() / "roster.koroster";
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << "KOREWORK_ROSTER_V1\n";
    for (const auto& slot : slots_) {
        output << (slot.occupied ? 1 : 0) << ' '
               << std::quoted(slot.name) << ' '
               << static_cast<std::uint16_t>(slot.playerClass) << ' '
               << std::max(1, slot.level) << '\n';
    }
    output.close();
    if (!output) return false;
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (!error) return true;
    error.clear();
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    return !error;
}

bool OfflineRoster::create(std::size_t slotIndex, std::string name, PlayerClass playerClass) {
    if (slotIndex >= slots_.size() || slots_[slotIndex].occupied
        || !validClass(static_cast<std::uint16_t>(playerClass))) return false;
    name = sanitizeName(std::move(name));
    if (name.size() < 3U) return false;
    for (const auto& existing : slots_) {
        if (!existing.occupied) continue;
        std::string left = existing.name;
        std::string right = name;
        std::transform(left.begin(), left.end(), left.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        std::transform(right.begin(), right.end(), right.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        if (left == right) return false;
    }
    slots_[slotIndex] = {true, std::move(name), playerClass, 1};
    return save();
}

bool OfflineRoster::remove(std::size_t slotIndex) {
    if (slotIndex >= slots_.size() || !slots_[slotIndex].occupied) return false;
    slots_[slotIndex] = {};
    std::error_code error;
    std::filesystem::remove(storageRoot() / ("offline_profile_" + std::to_string(slotIndex) + ".kosave"), error);
    return save();
}

bool OfflineRoster::updateLevel(std::size_t slotIndex, int level) {
    if (slotIndex >= slots_.size() || !slots_[slotIndex].occupied) return false;
    slots_[slotIndex].level = std::clamp(level, 1, 83);
    return save();
}

} // namespace korework
