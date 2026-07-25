#include "offline_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace korework {
namespace {

float distanceSquared(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

float distance(const Vec3& a, const Vec3& b) {
    return std::sqrt(distanceSquared(a, b));
}

Vec3 normalizedDirection(const Vec3& from, const Vec3& to) {
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length < 0.0001F) return {};
    return {dx / length, 0.0F, dz / length};
}

float clampValue(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(value, maximum));
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

int safeInt(std::uint32_t value) {
    return value > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        ? std::numeric_limits<int>::max()
        : static_cast<int>(value);
}

std::uint64_t mix(std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    return value;
}

std::array<std::uint32_t, 3> classSkillPrefixes(PlayerClass playerClass) {
    switch (playerClass) {
        case PlayerClass::Warrior: return {101U, 105U, 106U};
        case PlayerClass::Rogue: return {102U, 107U, 108U};
        case PlayerClass::Mage: return {103U, 109U, 110U};
        case PlayerClass::Priest: return {104U, 111U, 112U};
    }
    return {101U, 105U, 106U};
}

bool classPrefixMatches(PlayerClass playerClass, std::uint32_t prefix) {
    const auto prefixes = classSkillPrefixes(playerClass);
    return std::find(prefixes.begin(), prefixes.end(), prefix) != prefixes.end();
}

} // namespace

OfflineRuntime::OfflineRuntime() = default;

void OfflineRuntime::configureProfile(std::size_t slot, std::string name, PlayerClass playerClass) {
    profileSlot_ = std::min<std::size_t>(slot, OfflineRoster::SlotCount - 1U);
    configuredName_ = std::move(name);
    if (configuredName_.empty()) configuredName_ = "Adventurer";
    if (configuredName_.size() > 20U) configuredName_.resize(20U);
    configuredClass_ = OfflineRoster::validClass(static_cast<std::uint16_t>(playerClass))
        ? playerClass : PlayerClass::Warrior;
}

void OfflineRuntime::initialize() {
    std::vector<std::filesystem::path> candidates;
    if (const char* environment = std::getenv("KOREWORK_DATA_PACK"); environment != nullptr && *environment != '\0') {
        candidates.emplace_back(environment);
    }
    candidates.push_back(std::filesystem::current_path() / "data" / "game_data.kopack");
    candidates.push_back(std::filesystem::current_path() / "game_data.kopack");

    for (const auto& candidate : candidates) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(candidate, error)) continue;
        initialize(candidate);
        return;
    }

    usingGameData_ = false;
    gameData_ = {};
    dropTableIndex_.clear();
    itemIndex_.clear();
    itemNames_.clear();
    applyClassDefaults();
    createTemplates();
    createSkills();
    if (!load()) reset();
    else {
        spawnWorld();
        appendLog("Offline profil yuklendi. Ag baglantisi kullanilmiyor.");
        appendLog("KOPACK bulunamadi; guvenli fallback icerik kullaniliyor.");
    }
}

void OfflineRuntime::initialize(const std::filesystem::path& dataPackPath) {
    try {
        loadGameData(dataPackPath);
    } catch (const std::exception& exception) {
        usingGameData_ = false;
        gameData_ = {};
        dropTableIndex_.clear();
        itemIndex_.clear();
        itemNames_.clear();
        applyClassDefaults();
        createTemplates();
        createSkills();
        if (!load()) reset();
        appendLog(std::string("KOPACK reddedildi: ") + exception.what());
        return;
    }

    applyClassDefaults();
    createTemplates();
    createSkills();
    if (!load()) reset();
    else {
        spawnWorld();
        appendLog("Offline profil yuklendi. Ag baglantisi kullanilmiyor.");
    }
    appendLog("OpenKO Fire Drake verileri KOPACK uzerinden yuklendi.");
}

void OfflineRuntime::loadGameData(const std::filesystem::path& dataPackPath) {
    gameData_ = data::GameDataPack::load(dataPackPath);
    usingGameData_ = true;
    dropTableIndex_.clear();
    itemIndex_.clear();
    itemNames_.clear();
    dropTableIndex_.reserve(gameData_.dropTables.size());
    itemIndex_.reserve(gameData_.items.size());
    itemNames_.reserve(gameData_.items.size());
    for (std::size_t index = 0; index < gameData_.dropTables.size(); ++index) {
        dropTableIndex_.emplace(gameData_.dropTables[index].id, index);
    }
    for (std::size_t index = 0; index < gameData_.items.size(); ++index) {
        itemIndex_.emplace(gameData_.items[index].id, index);
        itemNames_.emplace(gameData_.items[index].id, gameData_.items[index].name);
    }
}

void OfflineRuntime::applyClassDefaults() {
    player_ = {};
    player_.name = configuredName_;
    player_.playerClass = configuredClass_;
    player_.gold = 2000;
    switch (configuredClass_) {
        case PlayerClass::Warrior:
            player_.strength = 65; player_.stamina = 60; player_.dexterity = 50; player_.intelligence = 30; player_.magicPower = 20;
            break;
        case PlayerClass::Rogue:
            player_.strength = 50; player_.stamina = 45; player_.dexterity = 70; player_.intelligence = 30; player_.magicPower = 25;
            break;
        case PlayerClass::Mage:
            player_.strength = 30; player_.stamina = 40; player_.dexterity = 40; player_.intelligence = 70; player_.magicPower = 70;
            break;
        case PlayerClass::Priest:
            player_.strength = 50; player_.stamina = 55; player_.dexterity = 35; player_.intelligence = 65; player_.magicPower = 50;
            break;
    }
    recalculateDerivedStats(true);
}

void OfflineRuntime::recalculateDerivedStats(bool restoreResources) {
    int weaponDamage = 0;
    int armor = 0;
    int bonusHp = 0;
    int bonusMp = 0;
    for (std::size_t slot = 0; slot < player_.equipmentItemIds.size(); ++slot) {
        const auto* item = itemRecord(player_.equipmentItemIds[slot]);
        if (item == nullptr) continue;
        const float upgradeMultiplier = 1.0F + static_cast<float>(player_.equipmentUpgradeLevels[slot]) * 0.08F;
        weaponDamage += static_cast<int>(static_cast<float>(std::max<std::int16_t>(0, item->damage)) * upgradeMultiplier);
        armor += static_cast<int>(static_cast<float>(std::max<std::int16_t>(0, item->armor)) * upgradeMultiplier);
        bonusHp += item->bonusHp;
        bonusMp += item->bonusMp;
    }

    float classHp = 1.0F;
    float classMp = 1.0F;
    int primary = player_.strength;
    switch (player_.playerClass) {
        case PlayerClass::Warrior: classHp = 1.45F; classMp = 0.65F; primary = player_.strength; break;
        case PlayerClass::Rogue: classHp = 1.05F; classMp = 0.90F; primary = player_.dexterity; break;
        case PlayerClass::Mage: classHp = 0.80F; classMp = 1.55F; primary = player_.magicPower; break;
        case PlayerClass::Priest: classHp = 1.15F; classMp = 1.25F; primary = std::max(player_.strength, player_.intelligence); break;
    }

    player_.maxHp = std::max(100.0F, (120.0F + static_cast<float>(player_.stamina) * 2.0F
                                     + static_cast<float>(player_.level) * 18.0F) * classHp + static_cast<float>(bonusHp));
    player_.maxMp = std::max(50.0F, (60.0F + static_cast<float>(player_.intelligence + player_.magicPower)
                                    + static_cast<float>(player_.level) * 10.0F) * classMp + static_cast<float>(bonusMp));
    player_.attackPower = std::max(1, primary / 3 + player_.level * 2 + weaponDamage);
    player_.defensePower = std::max(0, player_.stamina / 4 + player_.level + armor);
    if (restoreResources) {
        player_.hp = player_.maxHp;
        player_.mp = player_.maxMp;
    } else {
        player_.hp = clampValue(player_.hp, 1.0F, player_.maxHp);
        player_.mp = clampValue(player_.mp, 0.0F, player_.maxMp);
    }
}

void OfflineRuntime::reset() {
    applyClassDefaults();
    inventory_.clear();
    log_.clear();
    cooldowns_.fill(0.0F);
    nextRuntimeId_ = 1;

    if (usingGameData_) {
        std::array<bool, 14> starterSlots {};
        std::size_t starterCount = 0;
        for (const auto& item : gameData_.items) {
            if (item.slot >= starterSlots.size() || starterSlots[item.slot] || item.appearanceId == 0U
                || item.requiredLevel > 1U || !meetsRequirements(item)) continue;
            addInventory(item.id, item.name, 1);
            starterSlots[item.slot] = true;
            if (++starterCount >= 5U) break;
        }
    }
    if (inventory_.empty()) addInventory(0U, "Beginner Potion", 5);

    spawnWorld();
    appendLog("Yeni offline karakter olusturuldu: " + player_.name + " / " + OfflineRoster::className(player_.playerClass));
    appendLog("Dunya tek process icinde calisiyor; sunucu, SQL ve localhost yok.");
    if (usingGameData_) appendLog("OpenKO Fire Drake KOPACK aktif.");
    save();
}

void OfflineRuntime::createTemplates() {
    monsterTemplates_.clear();
    if (!usingGameData_ || gameData_.monsters.empty()) {
        createFallbackTemplates();
        return;
    }

    monsterTemplates_.reserve(gameData_.monsters.size());
    for (const auto& source : gameData_.monsters) {
        MonsterTemplate definition;
        definition.sid = source.sid;
        definition.modelId = source.modelId;
        definition.rightHandItem = source.rightHandItem;
        definition.leftHandItem = source.leftHandItem;
        definition.dropTableId = source.dropTableId;
        definition.name = source.name;
        definition.level = source.level;
        definition.maxHp = static_cast<float>(source.hp);
        definition.attack = static_cast<float>(std::max<std::uint16_t>(source.damage, source.attack));
        definition.defense = static_cast<float>(source.defense);
        definition.attackDelay = std::clamp(static_cast<float>(source.attackDelayMs) / 1000.0F, 0.25F, 10.0F);
        definition.movementSpeed = std::clamp(source.movementSpeed, 0.35F, 8.0F);
        definition.runningSpeed = std::clamp(source.runningSpeed, definition.movementSpeed, 12.0F);
        definition.attackRange = std::clamp(source.attackRange, 0.8F, 25.0F);
        definition.searchRange = std::clamp(source.searchRange, 3.0F, 80.0F);
        definition.chaseRange = std::max(definition.searchRange, std::clamp(source.chaseRange, 5.0F, 160.0F));
        definition.exp = safeInt(source.experience);
        definition.money = safeInt(source.money);
        definition.scale = std::clamp(static_cast<float>(source.sizePercent) / 100.0F, 0.35F, 3.5F);
        monsterTemplates_.push_back(std::move(definition));
    }
}

void OfflineRuntime::createFallbackTemplates() {
    monsterTemplates_ = {
        {100, 100, 120150000, 0, 100, "Kecoon", 6, 32.0F, 3.0F, 31.0F, 1.5F, 1.1F, 2.1F, 1.5F, 9.0F, 15.0F, 725, 73, 1.00F},
        {101, 100, 120150000, 0, 101, "Kecoon Scout", 10, 68.0F, 8.0F, 52.0F, 1.5F, 1.1F, 2.1F, 1.5F, 9.0F, 15.0F, 2050, 130, 0.90F},
        {102, 100, 0, 0, 102, "Kecoon Fighter", 12, 114.0F, 10.0F, 62.0F, 1.5F, 1.1F, 2.1F, 1.5F, 9.0F, 15.0F, 3075, 162, 1.20F},
        {103, 100, 130110000, 0, 103, "Kecoon Raider", 14, 149.0F, 13.0F, 72.0F, 1.5F, 1.1F, 2.1F, 1.5F, 9.0F, 15.0F, 4325, 196, 1.20F},
        {105, 100, 130250000, 0, 105, "Kecoon Warrior", 16, 190.0F, 16.0F, 83.0F, 1.5F, 1.1F, 2.1F, 1.5F, 9.0F, 15.0F, 5825, 233, 1.35F},
        {106, 100, 180110000, 0, 106, "Kecoon Sorcerer", 17, 214.0F, 18.0F, 88.0F, 1.5F, 1.1F, 2.1F, 1.5F, 9.0F, 15.0F, 6675, 252, 0.95F}
    };
}

void OfflineRuntime::createSkills() {
    skills_ = {};
    if (!usingGameData_ || gameData_.skills.empty()) {
        createFallbackSkills();
        return;
    }

    std::vector<const data::SkillRecord*> candidates;
    candidates.reserve(gameData_.skills.size());
    for (const auto& skill : gameData_.skills) {
        if (classPrefixMatches(configuredClass_, skill.id / 1000U)) candidates.push_back(&skill);
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto* left, const auto* right) {
        if (left->skillLevel != right->skillLevel) return left->skillLevel < right->skillLevel;
        return left->id < right->id;
    });
    if (candidates.size() < skills_.size()) {
        for (const auto& skill : gameData_.skills) {
            if (std::find(candidates.begin(), candidates.end(), &skill) == candidates.end()) candidates.push_back(&skill);
            if (candidates.size() >= skills_.size()) break;
        }
    }

    for (std::size_t index = 0; index < skills_.size(); ++index) {
        if (index >= candidates.size()) {
            skills_[index] = {0, "Locked", 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, false};
            continue;
        }
        const auto& source = *candidates[index];
        const std::string normalizedName = lower(source.name);
        const bool healing = normalizedName.find("heal") != std::string::npos || source.moral == 2U;
        SkillDefinition definition;
        definition.id = source.id;
        definition.name = source.name;
        definition.damage = healing ? 0.0F : std::max(8.0F, 12.0F + static_cast<float>(source.skillLevel) * 2.6F);
        definition.heal = healing ? std::max(15.0F, 12.0F + static_cast<float>(source.skillLevel) * 4.0F) : 0.0F;
        definition.manaCost = static_cast<float>(source.manaCost);
        definition.cooldown = std::clamp(static_cast<float>(source.cooldown) * 0.10F, 0.35F, 30.0F);
        definition.range = std::max(2.2F, static_cast<float>(source.range) * 0.10F);
        definition.unlocked = source.skillLevel <= static_cast<std::uint16_t>(std::max(1, player_.level));
        skills_[index] = std::move(definition);
    }
}

void OfflineRuntime::createFallbackSkills() {
    switch (configuredClass_) {
        case PlayerClass::Warrior:
            skills_ = {SkillDefinition{1,"Attack",18,0,0,0.55F,2.2F,true}, SkillDefinition{2,"Slash",31,0,8,1.1F,2.4F,true}, SkillDefinition{3,"Heavy Strike",52,0,18,2.8F,2.5F,true}, SkillDefinition{4,"Battle Cry",0,26,12,7,0,true}, SkillDefinition{5,"Second Wind",0,48,24,12,0,true}, {},{},{},{},{}};
            break;
        case PlayerClass::Rogue:
            skills_ = {SkillDefinition{1,"Attack",16,0,0,0.45F,2.2F,true}, SkillDefinition{2,"Stab",29,0,7,0.9F,2.2F,true}, SkillDefinition{3,"Piercing Arrow",42,0,15,1.8F,18,true}, SkillDefinition{4,"Evade",0,22,10,6,0,true}, SkillDefinition{5,"Shadow Step",35,0,20,4,4,true}, {},{},{},{},{}};
            break;
        case PlayerClass::Mage:
            skills_ = {SkillDefinition{1,"Staff Attack",12,0,0,0.8F,2.5F,true}, SkillDefinition{2,"Flame",32,0,12,1.2F,20,true}, SkillDefinition{3,"Cold Wave",40,0,18,2.2F,18,true}, SkillDefinition{4,"Spark",48,0,22,2.8F,22,true}, SkillDefinition{5,"Mana Shield",0,35,18,8,0,true}, {},{},{},{},{}};
            break;
        case PlayerClass::Priest:
            skills_ = {SkillDefinition{1,"Mace Attack",15,0,0,0.7F,2.3F,true}, SkillDefinition{2,"Light Strike",27,0,8,1.1F,14,true}, SkillDefinition{3,"Tiny Healing",0,35,10,1.4F,0,true}, SkillDefinition{4,"Strength",0,25,16,7,0,true}, SkillDefinition{5,"Light Healing",0,70,24,3,0,true}, {},{},{},{},{}};
            break;
    }
    for (std::size_t index = 5; index < skills_.size(); ++index) skills_[index].name = "Locked";
}

void OfflineRuntime::spawnWorld() {
    monsters_.clear();
    if (monsterTemplates_.empty()) return;

    std::vector<std::size_t> selected;
    selected.reserve(48);
    std::uint32_t lastModel = std::numeric_limits<std::uint32_t>::max();
    const int maximumLevel = std::max(12, player_.level + 10);
    for (std::size_t index = 0; index < monsterTemplates_.size() && selected.size() < 48U; ++index) {
        const auto& definition = monsterTemplates_[index];
        if (definition.level > maximumLevel || definition.level < 1) continue;
        if (!selected.empty() && definition.modelId == lastModel && selected.size() > 12U) continue;
        selected.push_back(index);
        lastModel = definition.modelId;
    }
    if (selected.empty()) {
        for (std::size_t index = 0; index < std::min<std::size_t>(monsterTemplates_.size(), 48U); ++index) selected.push_back(index);
    }

    const std::size_t spawnCount = std::max<std::size_t>(18U, std::min<std::size_t>(48U, selected.size()));
    monsters_.reserve(spawnCount);
    for (std::size_t index = 0; index < spawnCount; ++index) {
        const float ring = 12.0F + static_cast<float>(index / 12U) * 9.0F;
        const float angle = static_cast<float>(index % 12U) * (6.283185307F / 12.0F) + static_cast<float>(index / 12U) * 0.25F;
        const Vec3 position {std::cos(angle) * ring, 0.0F, std::sin(angle) * ring};
        MonsterState monster;
        monster.runtimeId = nextRuntimeId_++;
        monster.templateIndex = selected[index % selected.size()];
        monster.position = position;
        monster.spawnPosition = position;
        monster.hp = monsterTemplates_[monster.templateIndex].maxHp;
        monsters_.push_back(monster);
    }
}

void OfflineRuntime::update(float deltaSeconds) {
    for (float& cooldown : cooldowns_) cooldown = std::max(0.0F, cooldown - deltaSeconds);
    player_.mp = std::min(player_.maxMp, player_.mp + deltaSeconds * 2.5F);
    updateMonsters(deltaSeconds);

    autosaveTimer_ += deltaSeconds;
    if (autosaveTimer_ >= 10.0F) {
        autosaveTimer_ = 0.0F;
        save();
    }
}

void OfflineRuntime::updateMonsters(float deltaSeconds) {
    for (MonsterState& monster : monsters_) {
        const MonsterTemplate& definition = monsterTemplates_.at(monster.templateIndex);
        if (!monster.alive) {
            monster.respawnTimer -= deltaSeconds;
            if (monster.respawnTimer <= 0.0F) {
                monster.alive = true;
                monster.hp = definition.maxHp;
                monster.position = monster.spawnPosition;
            }
            continue;
        }

        monster.attackCooldown = std::max(0.0F, monster.attackCooldown - deltaSeconds);
        const float playerDistance = distance(monster.position, player_.position);
        const float homeDistance = distance(monster.position, monster.spawnPosition);
        if (playerDistance <= definition.searchRange && playerDistance > definition.attackRange
            && homeDistance <= definition.chaseRange) {
            const Vec3 direction = normalizedDirection(monster.position, player_.position);
            monster.position.x += direction.x * definition.runningSpeed * deltaSeconds;
            monster.position.z += direction.z * definition.runningSpeed * deltaSeconds;
        } else if (playerDistance <= definition.attackRange && monster.attackCooldown <= 0.0F) {
            attackPlayer(monster, definition);
        } else if (homeDistance > 0.35F && (playerDistance > definition.searchRange || homeDistance > definition.chaseRange)) {
            const Vec3 homeDirection = normalizedDirection(monster.position, monster.spawnPosition);
            monster.position.x += homeDirection.x * definition.movementSpeed * deltaSeconds;
            monster.position.z += homeDirection.z * definition.movementSpeed * deltaSeconds;
        }
    }
}

void OfflineRuntime::attackPlayer(MonsterState& monster, const MonsterTemplate& definition) {
    const float mitigation = static_cast<float>(player_.defensePower) * 0.18F;
    const float damage = std::max(1.0F, definition.attack - mitigation);
    player_.hp = std::max(0.0F, player_.hp - damage);
    monster.attackCooldown = definition.attackDelay;

    std::ostringstream message;
    message << definition.name << " sana " << static_cast<int>(damage) << " hasar verdi.";
    appendLog(message.str());
    if (player_.hp <= 0.0F) respawnPlayer();
}

bool OfflineRuntime::useSkill(std::size_t slot, std::optional<std::size_t> targetIndex) {
    if (slot >= skills_.size()) return false;
    const SkillDefinition& skill = skills_[slot];
    if (!skill.unlocked) {
        appendLog("Bu skill henuz acik degil.");
        return false;
    }
    if (cooldowns_[slot] > 0.0F) return false;
    if (player_.mp < skill.manaCost) {
        appendLog("Yeterli MP yok.");
        return false;
    }

    if (skill.damage > 0.0F) {
        if (!targetIndex.has_value() || *targetIndex >= monsters_.size()) {
            appendLog("Menzilde hedef yok.");
            return false;
        }
        const MonsterState& target = monsters_.at(*targetIndex);
        if (!target.alive || distance(player_.position, target.position) > skill.range) {
            appendLog("Hedef skill menzilinde degil.");
            return false;
        }
    }

    player_.mp -= skill.manaCost;
    cooldowns_[slot] = skill.cooldown;
    if (skill.heal > 0.0F) {
        const float before = player_.hp;
        const float scaling = player_.playerClass == PlayerClass::Priest ? static_cast<float>(player_.intelligence) * 0.35F : 0.0F;
        player_.hp = std::min(player_.maxHp, player_.hp + skill.heal + scaling);
        std::ostringstream message;
        message << skill.name << " ile " << static_cast<int>(player_.hp - before) << " HP yenilendi.";
        appendLog(message.str());
    }
    if (skill.damage > 0.0F && targetIndex.has_value()) damageMonster(*targetIndex, skill.damage, skill);
    return true;
}

void OfflineRuntime::damageMonster(std::size_t monsterIndex, float amount, const SkillDefinition& skill) {
    MonsterState& monster = monsters_.at(monsterIndex);
    const MonsterTemplate& definition = monsterTemplates_.at(monster.templateIndex);
    const float powerScale = static_cast<float>(player_.attackPower) * 0.35F;
    const float damage = std::max(1.0F, amount + powerScale - definition.defense * 0.10F);
    monster.hp = std::max(0.0F, monster.hp - damage);

    std::ostringstream message;
    message << skill.name << " -> " << definition.name << ": " << static_cast<int>(damage) << " hasar.";
    appendLog(message.str());
    if (monster.hp <= 0.0F) killMonster(monster, definition);
}

void OfflineRuntime::killMonster(MonsterState& monster, const MonsterTemplate& definition) {
    monster.alive = false;
    monster.respawnTimer = 5.0F + static_cast<float>(definition.level) * 0.18F;
    player_.exp += definition.exp;
    player_.gold += definition.money;
    awardDrops(monster, definition);

    std::ostringstream message;
    message << definition.name << " yenildi. +" << definition.exp << " EXP, +" << definition.money << " Noah.";
    appendLog(message.str());

    auto requiredExperience = [this]() { return std::max(1000, player_.level * player_.level * 500); };
    while (player_.exp >= requiredExperience() && player_.level < 83) {
        player_.exp -= requiredExperience();
        ++player_.level;
        player_.bonusPoints += 3;
        recalculateDerivedStats(true);
        createSkills();
        appendLog("Seviye atladin! 3 stat puani kazandin.");
    }
    save();
}

void OfflineRuntime::awardDrops(const MonsterState& monster, const MonsterTemplate& definition) {
    const auto iterator = dropTableIndex_.find(definition.dropTableId);
    if (iterator == dropTableIndex_.end() || iterator->second >= gameData_.dropTables.size()) return;
    const auto& table = gameData_.dropTables[iterator->second];
    for (std::size_t slot = 0; slot < table.entries.size(); ++slot) {
        const auto& entry = table.entries[slot];
        if (entry.itemId == 0U || entry.chance == 0U) continue;
        const std::uint64_t seed = mix(monster.runtimeId ^ (static_cast<std::uint64_t>(definition.sid) << 32U)
                                       ^ static_cast<std::uint64_t>(player_.exp) ^ (slot * 0x9e3779b97f4a7c15ULL));
        if (seed % 10'000ULL < entry.chance) addInventory(entry.itemId, itemName(entry.itemId), 1);
    }
}

void OfflineRuntime::addInventory(std::uint32_t itemId, std::string name, int count, std::uint8_t upgradeLevel) {
    if (count <= 0) return;
    const auto* item = itemRecord(itemId);
    const bool stackable = itemId == 0U || item == nullptr || item->countable;
    if (stackable && upgradeLevel == 0U) {
        const auto iterator = std::find_if(inventory_.begin(), inventory_.end(), [&](const InventoryEntry& entry) {
            return entry.upgradeLevel == 0U && (itemId != 0U ? entry.itemId == itemId : entry.name == name);
        });
        if (iterator != inventory_.end()) {
            iterator->count += count;
            return;
        }
    }
    inventory_.push_back({itemId, std::move(name), count, upgradeLevel});
}

const data::ItemRecord* OfflineRuntime::itemRecord(std::uint32_t itemId) const noexcept {
    const auto iterator = itemIndex_.find(itemId);
    if (iterator == itemIndex_.end() || iterator->second >= gameData_.items.size()) return nullptr;
    return &gameData_.items[iterator->second];
}

bool OfflineRuntime::meetsRequirements(const data::ItemRecord& item) const noexcept {
    if (player_.level < static_cast<int>(item.requiredLevel)) return false;
    if (player_.strength < static_cast<int>(item.requiredStrength)) return false;
    if (player_.stamina < static_cast<int>(item.requiredStamina)) return false;
    if (player_.dexterity < static_cast<int>(item.requiredDexterity)) return false;
    if (player_.intelligence < static_cast<int>(item.requiredIntelligence)) return false;
    if (player_.magicPower < static_cast<int>(item.requiredMagicPower)) return false;
    if (item.classRestriction >= 100U) {
        const std::uint16_t base = static_cast<std::uint16_t>(item.classRestriction);
        if (base != static_cast<std::uint16_t>(player_.playerClass)) return false;
    }
    return true;
}

bool OfflineRuntime::equipInventory(std::size_t inventoryIndex) {
    if (inventoryIndex >= inventory_.size()) return false;
    const InventoryEntry selected = inventory_[inventoryIndex];
    const auto* item = itemRecord(selected.itemId);
    if (item == nullptr || item->slot >= player_.equipmentItemIds.size()) {
        appendLog("Bu item ekipman slotuna takilamaz.");
        return false;
    }
    if (!meetsRequirements(*item)) {
        appendLog("Item gereksinimleri karsilanmiyor.");
        return false;
    }

    const std::size_t slot = item->slot;
    if (player_.equipmentItemIds[slot] != 0U) {
        addInventory(player_.equipmentItemIds[slot], itemName(player_.equipmentItemIds[slot]), 1,
                     player_.equipmentUpgradeLevels[slot]);
    }
    player_.equipmentItemIds[slot] = selected.itemId;
    player_.equipmentUpgradeLevels[slot] = selected.upgradeLevel;
    if (--inventory_[inventoryIndex].count <= 0) inventory_.erase(inventory_.begin() + static_cast<std::ptrdiff_t>(inventoryIndex));
    recalculateDerivedStats(false);
    appendLog(selected.name + " kusanildi.");
    save();
    return true;
}

bool OfflineRuntime::unequip(std::size_t equipmentSlot) {
    if (equipmentSlot >= player_.equipmentItemIds.size() || player_.equipmentItemIds[equipmentSlot] == 0U) return false;
    const std::uint32_t itemId = player_.equipmentItemIds[equipmentSlot];
    addInventory(itemId, itemName(itemId), 1, player_.equipmentUpgradeLevels[equipmentSlot]);
    player_.equipmentItemIds[equipmentSlot] = 0U;
    player_.equipmentUpgradeLevels[equipmentSlot] = 0U;
    recalculateDerivedStats(false);
    appendLog("Ekipman cikarildi: " + itemName(itemId));
    save();
    return true;
}

bool OfflineRuntime::upgradeInventory(std::size_t inventoryIndex) {
    if (inventoryIndex >= inventory_.size()) return false;
    InventoryEntry& entry = inventory_[inventoryIndex];
    const auto* item = itemRecord(entry.itemId);
    if (item == nullptr || item->slot >= player_.equipmentItemIds.size() || entry.upgradeLevel >= 10U) {
        appendLog("Bu item upgrade edilemez.");
        return false;
    }
    const int baseCost = std::max(100, safeInt(item->buyPrice / 50U));
    const int cost = baseCost * (static_cast<int>(entry.upgradeLevel) + 1);
    if (player_.gold < cost) {
        appendLog("Upgrade icin yeterli Noah yok.");
        return false;
    }
    static constexpr std::array<std::uint16_t, 10> chances {10000, 9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1000};
    player_.gold -= cost;
    const std::uint64_t seed = mix(static_cast<std::uint64_t>(entry.itemId)
                                   ^ (static_cast<std::uint64_t>(entry.upgradeLevel) << 40U)
                                   ^ static_cast<std::uint64_t>(player_.gold)
                                   ^ static_cast<std::uint64_t>(player_.level * 7919));
    if (seed % 10'000ULL < chances[entry.upgradeLevel]) {
        ++entry.upgradeLevel;
        appendLog(entry.name + " +" + std::to_string(entry.upgradeLevel) + " oldu.");
    } else {
        appendLog("Upgrade basarisiz; item korunuyor.");
    }
    save();
    return true;
}

bool OfflineRuntime::spendStatPoint(std::size_t statIndex) {
    if (player_.bonusPoints <= 0 || statIndex > 4U) return false;
    int* stat = nullptr;
    switch (statIndex) {
        case 0: stat = &player_.strength; break;
        case 1: stat = &player_.stamina; break;
        case 2: stat = &player_.dexterity; break;
        case 3: stat = &player_.intelligence; break;
        case 4: stat = &player_.magicPower; break;
        default: return false;
    }
    if (*stat >= 255) return false;
    ++(*stat);
    --player_.bonusPoints;
    recalculateDerivedStats(false);
    save();
    return true;
}

std::string OfflineRuntime::itemName(std::uint32_t itemId) const {
    const auto iterator = itemNames_.find(itemId);
    return iterator == itemNames_.end() ? "KO Item #" + std::to_string(itemId) : iterator->second;
}

void OfflineRuntime::appendLog(std::string message) {
    log_.push_front(std::move(message));
    while (log_.size() > 8) log_.pop_back();
}

void OfflineRuntime::respawnPlayer() {
    ++player_.deaths;
    player_.position = {0.0F, 0.0F, 0.0F};
    player_.hp = player_.maxHp;
    player_.mp = player_.maxMp;
    appendLog("Oldun ve baslangic noktasinda yeniden dogdun.");
    save();
}

void OfflineRuntime::movePlayer(Vec3 delta) {
    player_.position.x = clampValue(player_.position.x + delta.x, -4096.0F, 4096.0F);
    player_.position.z = clampValue(player_.position.z + delta.z, -4096.0F, 4096.0F);
    player_.position.y = 0.0F;
}

std::optional<std::size_t> OfflineRuntime::nearestAliveMonster(float maxDistance) const {
    std::optional<std::size_t> result;
    float closestDistanceSquared = maxDistance * maxDistance;
    for (std::size_t index = 0; index < monsters_.size(); ++index) {
        if (!monsters_[index].alive) continue;
        const float currentDistanceSquared = distanceSquared(player_.position, monsters_[index].position);
        if (currentDistanceSquared < closestDistanceSquared) {
            closestDistanceSquared = currentDistanceSquared;
            result = index;
        }
    }
    return result;
}

std::filesystem::path OfflineRuntime::savePath() const {
    return OfflineRoster::storageRoot() / ("offline_profile_" + std::to_string(profileSlot_) + ".kosave");
}

void OfflineRuntime::save() const {
    const std::filesystem::path path = savePath();
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return;
    output << "KOREWORK_SAVE_V3\n";
    output << profileSlot_ << ' ' << std::quoted(player_.name) << ' '
           << static_cast<std::uint16_t>(player_.playerClass) << '\n';
    output << std::setprecision(9)
           << player_.position.x << ' ' << player_.position.y << ' ' << player_.position.z << ' '
           << player_.hp << ' ' << player_.maxHp << ' ' << player_.mp << ' ' << player_.maxMp << ' '
           << player_.level << ' ' << player_.exp << ' ' << player_.gold << ' ' << player_.deaths << ' '
           << player_.strength << ' ' << player_.stamina << ' ' << player_.dexterity << ' '
           << player_.intelligence << ' ' << player_.magicPower << ' ' << player_.bonusPoints << '\n';
    for (std::size_t slot = 0; slot < player_.equipmentItemIds.size(); ++slot) {
        output << player_.equipmentItemIds[slot] << ' ' << static_cast<unsigned int>(player_.equipmentUpgradeLevels[slot])
               << (slot + 1U == player_.equipmentItemIds.size() ? '\n' : ' ');
    }
    output << inventory_.size() << '\n';
    for (const InventoryEntry& entry : inventory_) {
        output << entry.itemId << ' ' << std::quoted(entry.name) << ' ' << entry.count << ' '
               << static_cast<unsigned int>(entry.upgradeLevel) << '\n';
    }
    output.close();
    if (!output) return;
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (!error) return;
    error.clear();
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
}

bool OfflineRuntime::load() {
    std::ifstream input(savePath());
    if (!input) return false;
    std::string header;
    std::getline(input, header);
    const bool version1 = header == "KOREWORK_SAVE_V1";
    const bool version2 = header == "KOREWORK_SAVE_V2";
    const bool version3 = header == "KOREWORK_SAVE_V3";
    if (!version1 && !version2 && !version3) return false;

    applyClassDefaults();
    if (version3) {
        std::size_t storedSlot = 0;
        std::uint16_t classId = 0;
        input >> storedSlot >> std::quoted(player_.name) >> classId;
        if (!input || storedSlot != profileSlot_ || !OfflineRoster::validClass(classId)
            || static_cast<PlayerClass>(classId) != configuredClass_) return false;
        player_.playerClass = static_cast<PlayerClass>(classId);
    }

    input >> player_.position.x >> player_.position.y >> player_.position.z
          >> player_.hp >> player_.maxHp >> player_.mp >> player_.maxMp
          >> player_.level >> player_.exp >> player_.gold >> player_.deaths;
    if (!input) return false;
    if (version3) {
        input >> player_.strength >> player_.stamina >> player_.dexterity
              >> player_.intelligence >> player_.magicPower >> player_.bonusPoints;
        if (!input) return false;
        for (std::size_t slot = 0; slot < player_.equipmentItemIds.size(); ++slot) {
            unsigned int upgrade = 0;
            input >> player_.equipmentItemIds[slot] >> upgrade;
            if (!input || upgrade > 10U) return false;
            player_.equipmentUpgradeLevels[slot] = static_cast<std::uint8_t>(upgrade);
        }
    }

    if (player_.level < 1 || player_.level > 83 || player_.exp < 0 || player_.gold < 0 || player_.bonusPoints < 0) return false;
    std::size_t inventoryCount = 0;
    input >> inventoryCount;
    if (!input || inventoryCount > 100'000U) return false;
    inventory_.clear();
    for (std::size_t index = 0; index < inventoryCount; ++index) {
        InventoryEntry entry;
        unsigned int upgrade = 0;
        if (version1) input >> std::quoted(entry.name) >> entry.count;
        else if (version2) input >> entry.itemId >> std::quoted(entry.name) >> entry.count;
        else input >> entry.itemId >> std::quoted(entry.name) >> entry.count >> upgrade;
        if (!input || entry.count < 0 || upgrade > 10U) return false;
        entry.upgradeLevel = static_cast<std::uint8_t>(upgrade);
        inventory_.push_back(std::move(entry));
    }
    recalculateDerivedStats(false);
    return true;
}

const PlayerState& OfflineRuntime::player() const noexcept { return player_; }
PlayerState& OfflineRuntime::player() noexcept { return player_; }
const std::vector<MonsterState>& OfflineRuntime::monsters() const noexcept { return monsters_; }
const std::vector<MonsterTemplate>& OfflineRuntime::monsterTemplates() const noexcept { return monsterTemplates_; }
const std::array<SkillDefinition, 10>& OfflineRuntime::skills() const noexcept { return skills_; }
const std::array<float, 10>& OfflineRuntime::cooldowns() const noexcept { return cooldowns_; }
const std::vector<InventoryEntry>& OfflineRuntime::inventory() const noexcept { return inventory_; }
const std::deque<std::string>& OfflineRuntime::log() const noexcept { return log_; }

} // namespace korework
