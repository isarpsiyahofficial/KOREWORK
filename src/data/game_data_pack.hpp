#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace korework::data {

struct MonsterRecord {
    std::uint32_t id = 0;
    std::uint16_t sid = 0;
    std::string name;
    std::uint32_t modelId = 0;
    std::uint16_t sizePercent = 100;
    std::uint32_t rightHandItem = 0;
    std::uint32_t leftHandItem = 0;
    std::uint8_t group = 0;
    std::uint8_t rank = 0;
    std::uint8_t title = 0;
    std::uint16_t level = 1;
    std::uint32_t hp = 1;
    std::uint32_t mp = 0;
    std::uint16_t attack = 0;
    std::uint16_t defense = 0;
    std::uint16_t hitRate = 0;
    std::uint16_t evasionRate = 0;
    std::uint16_t damage = 0;
    std::uint16_t attackDelayMs = 1000;
    float movementSpeed = 0.0F;
    float runningSpeed = 0.0F;
    float attackRange = 1.5F;
    float searchRange = 0.0F;
    float chaseRange = 0.0F;
    std::uint32_t skill1 = 0;
    std::uint32_t skill2 = 0;
    std::uint32_t skill3 = 0;
    std::int16_t fireResistance = 0;
    std::int16_t coldResistance = 0;
    std::int16_t lightningResistance = 0;
    std::int16_t magicResistance = 0;
    std::int16_t diseaseResistance = 0;
    std::int16_t poisonResistance = 0;
    std::uint32_t experience = 0;
    std::uint32_t loyalty = 0;
    std::uint32_t money = 0;
    std::uint32_t dropTableId = 0;
};

struct ItemRecord {
    std::uint32_t id = 0;
    std::uint16_t extensionId = 0;
    std::string name;
    std::string description;
    std::uint8_t kind = 0;
    std::uint8_t slot = 0;
    std::uint8_t race = 0;
    std::uint8_t classRestriction = 0;
    std::int16_t damage = 0;
    std::int16_t attackDelay = 0;
    std::int16_t range = 0;
    std::int16_t weight = 0;
    std::int16_t durability = 0;
    std::uint32_t buyPrice = 0;
    std::uint32_t sellPrice = 0;
    std::int16_t armor = 0;
    bool countable = false;
    std::uint32_t effect1 = 0;
    std::uint32_t effect2 = 0;
    std::uint8_t requiredLevel = 0;
    std::uint8_t requiredRank = 0;
    std::uint8_t requiredTitle = 0;
    std::uint8_t requiredStrength = 0;
    std::uint8_t requiredStamina = 0;
    std::uint8_t requiredDexterity = 0;
    std::uint8_t requiredIntelligence = 0;
    std::uint8_t requiredMagicPower = 0;
    std::int16_t bonusStrength = 0;
    std::int16_t bonusStamina = 0;
    std::int16_t bonusDexterity = 0;
    std::int16_t bonusIntelligence = 0;
    std::int16_t bonusMagicPower = 0;
    std::int16_t bonusHp = 0;
    std::int16_t bonusMp = 0;
    std::int16_t fireDamage = 0;
    std::int16_t coldDamage = 0;
    std::int16_t lightningDamage = 0;
    std::int16_t poisonDamage = 0;
    std::int16_t fireResistance = 0;
    std::int16_t coldResistance = 0;
    std::int16_t lightningResistance = 0;
    std::int16_t magicResistance = 0;
    std::int16_t diseaseResistance = 0;
    std::int16_t poisonResistance = 0;
    std::uint32_t iconId = 0;
    std::uint32_t appearanceId = 0;
};

struct SkillRecord {
    std::uint32_t id = 0;
    std::string name;
    std::string description;
    std::uint16_t userAnimation = 0;
    std::uint16_t targetAnimation = 0;
    std::uint16_t selfEffect = 0;
    std::uint16_t projectileEffect = 0;
    std::uint16_t targetEffect = 0;
    std::uint8_t targetType = 0;
    std::uint8_t moral = 0;
    std::uint16_t skillLevel = 0;
    std::uint16_t requiredSkillPoints = 0;
    std::uint16_t manaCost = 0;
    std::uint16_t hpCost = 0;
    std::uint32_t requiredItem = 0;
    std::uint16_t castTime = 0;
    std::uint16_t cooldown = 0;
    std::uint8_t successRate = 100;
    std::uint8_t type1 = 0;
    std::uint8_t type2 = 0;
    std::uint16_t range = 0;
};

struct ClassCoefficientRecord {
    std::uint16_t classId = 0;
    float shortSword = 0.0F;
    float sword = 0.0F;
    float axe = 0.0F;
    float club = 0.0F;
    float spear = 0.0F;
    float pole = 0.0F;
    float staff = 0.0F;
    float bow = 0.0F;
    float hp = 0.0F;
    float mp = 0.0F;
    float sp = 0.0F;
    float armor = 0.0F;
    float hitRate = 0.0F;
    float evasionRate = 0.0F;
};

struct DropEntry {
    std::uint32_t itemId = 0;
    std::uint16_t chance = 0; // KO probability scale: 0..10000.
};

struct DropTableRecord {
    std::uint32_t id = 0;
    std::array<DropEntry, 5> entries {};
};

class GameDataPack final {
public:
    static constexpr std::uint32_t CurrentVersion = 2;

    std::vector<MonsterRecord> monsters;
    std::vector<ItemRecord> items;
    std::vector<SkillRecord> skills;
    std::vector<ClassCoefficientRecord> classes;
    std::vector<DropTableRecord> dropTables;

    void validate() const;
    void save(const std::filesystem::path& path) const;
    [[nodiscard]] static GameDataPack load(const std::filesystem::path& path);
};

} // namespace korework::data
