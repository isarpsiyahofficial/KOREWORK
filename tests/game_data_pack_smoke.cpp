#include "data/game_data_pack.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>

int main() {
    korework::data::GameDataPack pack;

    korework::data::MonsterRecord monster;
    monster.id = 101;
    monster.sid = 106;
    monster.name = "Kecoon";
    monster.modelId = 106;
    monster.level = 4;
    monster.hp = 140;
    monster.damage = 12;
    monster.movementSpeed = 1.4F;
    monster.runningSpeed = 2.8F;
    monster.attackRange = 1.8F;
    monster.searchRange = 12.0F;
    monster.chaseRange = 22.0F;
    monster.experience = 25;
    monster.money = 8;
    monster.dropTableId = 5001;
    pack.monsters.push_back(monster);

    korework::data::ItemRecord item;
    item.id = 900001;
    item.name = "Kecoon Fang";
    item.description = "A local-pack test item.";
    item.kind = 91;
    item.countable = true;
    item.iconId = 244;
    pack.items.push_back(item);

    korework::data::SkillRecord skill;
    skill.id = 1001;
    skill.name = "Slash";
    skill.description = "A deterministic local skill.";
    skill.userAnimation = 12;
    skill.targetAnimation = 18;
    skill.manaCost = 8;
    skill.cooldown = 12;
    skill.successRate = 100;
    skill.type1 = 1;
    skill.range = 15;
    pack.skills.push_back(skill);

    korework::data::ClassCoefficientRecord coefficient;
    coefficient.classId = 101;
    coefficient.sword = 1.2F;
    coefficient.axe = 1.1F;
    coefficient.hp = 1.3F;
    coefficient.mp = 0.7F;
    coefficient.sp = 1.0F;
    coefficient.armor = 1.2F;
    coefficient.hitRate = 1.0F;
    coefficient.evasionRate = 0.8F;
    pack.classes.push_back(coefficient);

    const auto path = std::filesystem::temp_directory_path() / "korework_game_data_smoke.kopack";
    pack.save(path);
    const auto loaded = korework::data::GameDataPack::load(path);
    assert(loaded.monsters.size() == 1U);
    assert(loaded.monsters.front().name == "Kecoon");
    assert(loaded.items.front().countable);
    assert(loaded.skills.front().cooldown == 12U);
    assert(loaded.classes.front().sword == 1.2F);

    {
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        assert(file);
        file.seekg(-1, std::ios::end);
        char byte = 0;
        file.read(&byte, 1);
        byte ^= static_cast<char>(0x5A);
        file.seekp(-1, std::ios::end);
        file.write(&byte, 1);
    }

    bool checksumRejected = false;
    try {
        (void) korework::data::GameDataPack::load(path);
    } catch (const std::runtime_error&) {
        checksumRejected = true;
    }
    assert(checksumRejected);

    std::filesystem::remove(path);
    return 0;
}
