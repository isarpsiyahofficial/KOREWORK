#include "data/game_data_pack.hpp"
#include "offline_roster.hpp"
#include "offline_runtime.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

void setHome(const std::filesystem::path& path) {
#ifdef _WIN32
    _putenv_s("APPDATA", path.string().c_str());
#else
    setenv("HOME", path.string().c_str(), 1);
#endif
}

korework::data::GameDataPack createPack() {
    korework::data::GameDataPack pack;

    korework::data::MonsterRecord monster;
    monster.id = 100;
    monster.sid = 100;
    monster.name = "Kecoon";
    monster.modelId = 100;
    monster.sizePercent = 100;
    monster.level = 1;
    monster.hp = 20;
    monster.damage = 1;
    monster.attackDelayMs = 1500;
    monster.movementSpeed = 1.0F;
    monster.runningSpeed = 2.0F;
    monster.attackRange = 1.5F;
    monster.searchRange = 6.0F;
    monster.chaseRange = 10.0F;
    monster.experience = 100;
    monster.money = 10;
    monster.dropTableId = 100;
    pack.monsters.push_back(monster);

    korework::data::DropTableRecord drops;
    drops.id = 100;
    drops.entries[0] = {110000001U, 10000U};
    pack.dropTables.push_back(drops);

    for (std::uint32_t classPrefix = 101U; classPrefix <= 104U; ++classPrefix) {
        for (std::uint32_t level = 1U; level <= 10U; ++level) {
            korework::data::SkillRecord skill;
            skill.id = classPrefix * 1000U + level;
            skill.name = "Class Skill " + std::to_string(classPrefix) + "-" + std::to_string(level);
            skill.skillLevel = static_cast<std::uint16_t>(level);
            skill.manaCost = 1;
            skill.cooldown = 5;
            skill.successRate = 100;
            skill.range = 20;
            pack.skills.push_back(skill);
        }
    }

    for (std::uint16_t classId = 101; classId <= 104; ++classId) {
        korework::data::ClassCoefficientRecord coefficient;
        coefficient.classId = classId;
        coefficient.hp = 1.0F;
        coefficient.mp = 1.0F;
        coefficient.armor = 1.0F;
        coefficient.hitRate = 1.0F;
        coefficient.evasionRate = 1.0F;
        pack.classes.push_back(coefficient);
    }

    const std::array<std::uint8_t, 5> slots {1U, 4U, 6U, 8U, 13U};
    for (std::size_t index = 0; index < slots.size(); ++index) {
        korework::data::ItemRecord item;
        item.id = 110000001U + static_cast<std::uint32_t>(index);
        item.name = "Starter Equipment " + std::to_string(index);
        item.description = "Profile runtime test equipment";
        item.slot = slots[index];
        item.damage = slots[index] == 6U ? 12 : 0;
        item.armor = slots[index] != 6U ? 8 : 0;
        item.durability = 100;
        item.buyPrice = 500;
        item.appearanceId = 120000000U + static_cast<std::uint32_t>(index);
        item.iconId = item.appearanceId;
        item.requiredLevel = 1;
        pack.items.push_back(item);
    }

    pack.validate();
    return pack;
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "korework_profile_runtime_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    setHome(root);

    const auto packPath = root / "profile-test.kopack";
    createPack().save(packPath);

    const std::array<korework::PlayerClass, 4> classes {
        korework::PlayerClass::Warrior,
        korework::PlayerClass::Rogue,
        korework::PlayerClass::Mage,
        korework::PlayerClass::Priest
    };
    std::array<float, 4> maxHp {};
    std::array<float, 4> maxMp {};

    for (std::size_t index = 0; index < classes.size(); ++index) {
        const auto classRoot = root / ("class-" + std::to_string(index));
        std::filesystem::create_directories(classRoot);
        setHome(classRoot);

        korework::OfflineRuntime runtime;
        runtime.configureProfile(index % korework::OfflineRoster::SlotCount,
                                 "Hero" + std::to_string(index), classes[index]);
        runtime.initialize(packPath);
        assert(runtime.usingGameData());
        assert(runtime.player().playerClass == classes[index]);
        assert(runtime.player().name == "Hero" + std::to_string(index));
        assert(runtime.inventory().size() >= 5U);
        assert(runtime.skills()[0].unlocked);
        maxHp[index] = runtime.player().maxHp;
        maxMp[index] = runtime.player().maxMp;

        std::size_t weaponIndex = runtime.inventory().size();
        for (std::size_t itemIndex = 0; itemIndex < runtime.inventory().size(); ++itemIndex) {
            const auto* item = runtime.itemRecord(runtime.inventory()[itemIndex].itemId);
            if (item != nullptr && item->slot == 6U) {
                weaponIndex = itemIndex;
                break;
            }
        }
        assert(weaponIndex < runtime.inventory().size());
        const int attackBefore = runtime.player().attackPower;
        assert(runtime.equipInventory(weaponIndex));
        assert(runtime.player().equipmentItemIds[6] != 0U);
        assert(runtime.player().attackPower > attackBefore);
        assert(runtime.unequip(6U));
        assert(runtime.player().equipmentItemIds[6] == 0U);

        runtime.player().bonusPoints = 1;
        const int strengthBefore = runtime.player().strength;
        assert(runtime.spendStatPoint(0U));
        assert(runtime.player().strength == strengthBefore + 1);
        assert(runtime.player().bonusPoints == 0);

        assert(!runtime.inventory().empty());
        runtime.player().gold = 100000;
        assert(runtime.upgradeInventory(0U));
        runtime.save();

        korework::OfflineRuntime reloaded;
        reloaded.configureProfile(index % korework::OfflineRoster::SlotCount,
                                  "Hero" + std::to_string(index), classes[index]);
        reloaded.initialize(packPath);
        assert(reloaded.player().name == runtime.player().name);
        assert(reloaded.player().playerClass == runtime.player().playerClass);
        assert(reloaded.player().strength == runtime.player().strength);
        assert(reloaded.inventory().size() == runtime.inventory().size());
    }

    assert(maxHp[0] > maxHp[2]);
    assert(maxMp[2] > maxMp[0]);
    assert(maxMp[3] > maxMp[1]);

    setHome(root);
    korework::OfflineRoster roster;
    assert(roster.create(0U, "Alpha", korework::PlayerClass::Warrior));
    assert(roster.create(1U, "Beta", korework::PlayerClass::Mage));
    assert(!roster.create(2U, "alpha", korework::PlayerClass::Rogue));
    assert(roster.slot(0U) != nullptr && roster.slot(0U)->playerClass == korework::PlayerClass::Warrior);
    assert(roster.slot(1U) != nullptr && roster.slot(1U)->playerClass == korework::PlayerClass::Mage);
    assert(roster.updateLevel(1U, 42));
    assert(roster.slot(1U)->level == 42);
    assert(roster.remove(0U));
    assert(roster.slot(0U) == nullptr);

    std::filesystem::remove_all(root);
    return 0;
}
