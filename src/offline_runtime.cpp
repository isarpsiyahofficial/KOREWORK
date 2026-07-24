#include "offline_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
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
    if (length < 0.0001F) {
        return {};
    }
    return {dx / length, 0.0F, dz / length};
}

float clampValue(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(value, maximum));
}

} // namespace

OfflineRuntime::OfflineRuntime() = default;

void OfflineRuntime::initialize() {
    createTemplates();
    createSkills();
    if (!load()) {
        reset();
    } else {
        spawnWorld();
        appendLog("Offline profil yuklendi. Ag baglantisi kullanilmiyor.");
    }
}

void OfflineRuntime::reset() {
    player_ = {};
    inventory_.clear();
    log_.clear();
    cooldowns_.fill(0.0F);
    nextRuntimeId_ = 1;
    spawnWorld();
    appendLog("Yeni offline karakter olusturuldu.");
    appendLog("Dunya yerel bellek icinde calisiyor; sunucu ve SQL yok.");
    save();
}

void OfflineRuntime::createTemplates() {
    monsterTemplates_.clear();
    // Fire Drake/OpenKO K_MONSTER kayitlarindaki Kecoon ailesi temel alindi.
    monsterTemplates_.push_back({100, "Kecoon", 6, 32.0F, 3.0F, 1.0F, 25, 73, 1.00F});
    monsterTemplates_.push_back({101, "Kecoon Scout", 10, 68.0F, 8.0F, 2.0F, 45, 130, 0.90F});
    monsterTemplates_.push_back({102, "Kecoon Fighter", 12, 114.0F, 10.0F, 4.0F, 65, 162, 1.15F});
    monsterTemplates_.push_back({103, "Kecoon Raider", 14, 149.0F, 13.0F, 5.0F, 85, 196, 1.20F});
    monsterTemplates_.push_back({105, "Kecoon Warrior", 16, 190.0F, 16.0F, 7.0F, 110, 233, 1.35F});
    monsterTemplates_.push_back({106, "Kecoon Sorcerer", 17, 214.0F, 18.0F, 5.0F, 130, 252, 0.95F});
}

void OfflineRuntime::createSkills() {
    skills_ = {
        SkillDefinition{1, "Attack", 18.0F, 0.0F, 0.0F, 0.55F, 2.2F, true},
        SkillDefinition{2, "Slash", 31.0F, 0.0F, 8.0F, 1.10F, 2.4F, true},
        SkillDefinition{3, "Heavy Strike", 52.0F, 0.0F, 18.0F, 2.80F, 2.5F, true},
        SkillDefinition{4, "Battle Cry", 0.0F, 26.0F, 12.0F, 7.0F, 0.0F, true},
        SkillDefinition{5, "Second Wind", 0.0F, 48.0F, 24.0F, 12.0F, 0.0F, true},
        SkillDefinition{6, "Locked", 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, false},
        SkillDefinition{7, "Locked", 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, false},
        SkillDefinition{8, "Locked", 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, false},
        SkillDefinition{9, "Locked", 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, false},
        SkillDefinition{10, "Locked", 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, false}
    };
}

void OfflineRuntime::spawnWorld() {
    monsters_.clear();
    const std::array<Vec3, 18> positions {{
        {-9.0F, 0.0F, -7.0F}, {-5.0F, 0.0F, -11.0F}, {2.0F, 0.0F, -13.0F},
        {8.0F, 0.0F, -9.0F}, {12.0F, 0.0F, -2.0F}, {11.0F, 0.0F, 6.0F},
        {6.0F, 0.0F, 11.0F}, {-1.0F, 0.0F, 13.0F}, {-8.0F, 0.0F, 10.0F},
        {-13.0F, 0.0F, 3.0F}, {-15.0F, 0.0F, -5.0F}, {-3.0F, 0.0F, -18.0F},
        {16.0F, 0.0F, -12.0F}, {18.0F, 0.0F, 7.0F}, {4.0F, 0.0F, 19.0F},
        {-14.0F, 0.0F, 16.0F}, {-21.0F, 0.0F, 1.0F}, {21.0F, 0.0F, 1.0F}
    }};

    for (std::size_t i = 0; i < positions.size(); ++i) {
        const std::size_t templateIndex = std::min(i / 3, monsterTemplates_.size() - 1);
        MonsterState monster;
        monster.runtimeId = nextRuntimeId_++;
        monster.templateIndex = templateIndex;
        monster.position = positions[i];
        monster.spawnPosition = positions[i];
        monster.hp = monsterTemplates_[templateIndex].maxHp;
        monsters_.push_back(monster);
    }
}

void OfflineRuntime::update(float deltaSeconds) {
    for (float& cooldown : cooldowns_) {
        cooldown = std::max(0.0F, cooldown - deltaSeconds);
    }

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
        if (playerDistance < 9.5F && playerDistance > 1.55F) {
            const Vec3 direction = normalizedDirection(monster.position, player_.position);
            const float speed = 1.05F + static_cast<float>(definition.level) * 0.015F;
            monster.position.x += direction.x * speed * deltaSeconds;
            monster.position.z += direction.z * speed * deltaSeconds;
        } else if (playerDistance <= 1.55F && monster.attackCooldown <= 0.0F) {
            attackPlayer(monster, definition);
        } else if (playerDistance > 14.0F) {
            const Vec3 homeDirection = normalizedDirection(monster.position, monster.spawnPosition);
            monster.position.x += homeDirection.x * 0.9F * deltaSeconds;
            monster.position.z += homeDirection.z * 0.9F * deltaSeconds;
        }
    }
}

void OfflineRuntime::attackPlayer(MonsterState& monster, const MonsterTemplate& definition) {
    const float mitigation = static_cast<float>(player_.level) * 0.25F;
    const float damage = std::max(1.0F, definition.attack - mitigation);
    player_.hp = std::max(0.0F, player_.hp - damage);
    monster.attackCooldown = 1.35F;

    std::ostringstream message;
    message << definition.name << " sana " << static_cast<int>(damage) << " hasar verdi.";
    appendLog(message.str());

    if (player_.hp <= 0.0F) {
        respawnPlayer();
    }
}

bool OfflineRuntime::useSkill(std::size_t slot, std::optional<std::size_t> targetIndex) {
    if (slot >= skills_.size()) {
        return false;
    }

    const SkillDefinition& skill = skills_[slot];
    if (!skill.unlocked) {
        appendLog("Bu skill henuz acik degil.");
        return false;
    }
    if (cooldowns_[slot] > 0.0F) {
        return false;
    }
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
        player_.hp = std::min(player_.maxHp, player_.hp + skill.heal);
        std::ostringstream message;
        message << skill.name << " ile " << static_cast<int>(player_.hp - before) << " HP yenilendi.";
        appendLog(message.str());
    }
    if (skill.damage > 0.0F && targetIndex.has_value()) {
        damageMonster(*targetIndex, skill.damage, skill);
    }
    return true;
}

void OfflineRuntime::damageMonster(std::size_t monsterIndex, float amount, const SkillDefinition& skill) {
    MonsterState& monster = monsters_.at(monsterIndex);
    const MonsterTemplate& definition = monsterTemplates_.at(monster.templateIndex);
    const float levelBonus = static_cast<float>(player_.level - 1) * 2.0F;
    const float damage = std::max(1.0F, amount + levelBonus - definition.defense * 0.35F);
    monster.hp = std::max(0.0F, monster.hp - damage);

    std::ostringstream message;
    message << skill.name << " -> " << definition.name << ": " << static_cast<int>(damage) << " hasar.";
    appendLog(message.str());

    if (monster.hp <= 0.0F) {
        killMonster(monster, definition);
    }
}

void OfflineRuntime::killMonster(MonsterState& monster, const MonsterTemplate& definition) {
    monster.alive = false;
    monster.respawnTimer = 6.0F + static_cast<float>(definition.level) * 0.12F;
    player_.exp += definition.exp;
    player_.gold += definition.money;

    if ((monster.runtimeId + static_cast<std::uint64_t>(player_.exp)) % 2U == 0U) {
        addInventory("Kecoon Hide", 1);
    }
    if ((monster.runtimeId + static_cast<std::uint64_t>(definition.sid)) % 5U == 0U) {
        addInventory("Kecoon Fang", 1);
    }

    std::ostringstream message;
    message << definition.name << " yenildi. +" << definition.exp << " EXP, +" << definition.money << " Noah.";
    appendLog(message.str());

    while (player_.exp >= player_.level * 200) {
        player_.exp -= player_.level * 200;
        ++player_.level;
        player_.maxHp += 22.0F;
        player_.maxMp += 12.0F;
        player_.hp = player_.maxHp;
        player_.mp = player_.maxMp;
        appendLog("Seviye atladin! Temel statlar arttirildi.");
    }
    save();
}

void OfflineRuntime::addInventory(std::string name, int count) {
    const auto iterator = std::find_if(inventory_.begin(), inventory_.end(), [&name](const InventoryEntry& entry) {
        return entry.name == name;
    });
    if (iterator == inventory_.end()) {
        inventory_.push_back({std::move(name), count});
    } else {
        iterator->count += count;
    }
}

void OfflineRuntime::appendLog(std::string message) {
    log_.push_front(std::move(message));
    while (log_.size() > 8) {
        log_.pop_back();
    }
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
    player_.position.x = clampValue(player_.position.x + delta.x, -28.0F, 28.0F);
    player_.position.z = clampValue(player_.position.z + delta.z, -28.0F, 28.0F);
    player_.position.y = 0.0F;
}

std::optional<std::size_t> OfflineRuntime::nearestAliveMonster(float maxDistance) const {
    std::optional<std::size_t> result;
    float closestDistanceSquared = maxDistance * maxDistance;
    for (std::size_t index = 0; index < monsters_.size(); ++index) {
        if (!monsters_[index].alive) {
            continue;
        }
        const float currentDistanceSquared = distanceSquared(player_.position, monsters_[index].position);
        if (currentDistanceSquared < closestDistanceSquared) {
            closestDistanceSquared = currentDistanceSquared;
            result = index;
        }
    }
    return result;
}

std::filesystem::path OfflineRuntime::savePath() const {
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
    return directory / "offline_profile.kosave";
}

void OfflineRuntime::save() const {
    std::ofstream output(savePath(), std::ios::trunc);
    if (!output) {
        return;
    }
    output << "KOREWORK_SAVE_V1\n";
    output << std::setprecision(9)
           << player_.position.x << ' ' << player_.position.y << ' ' << player_.position.z << ' '
           << player_.hp << ' ' << player_.maxHp << ' ' << player_.mp << ' ' << player_.maxMp << ' '
           << player_.level << ' ' << player_.exp << ' ' << player_.gold << ' ' << player_.deaths << '\n';
    output << inventory_.size() << '\n';
    for (const InventoryEntry& entry : inventory_) {
        output << std::quoted(entry.name) << ' ' << entry.count << '\n';
    }
}

bool OfflineRuntime::load() {
    std::ifstream input(savePath());
    if (!input) {
        return false;
    }
    std::string header;
    std::getline(input, header);
    if (header != "KOREWORK_SAVE_V1") {
        return false;
    }
    input >> player_.position.x >> player_.position.y >> player_.position.z
          >> player_.hp >> player_.maxHp >> player_.mp >> player_.maxMp
          >> player_.level >> player_.exp >> player_.gold >> player_.deaths;
    if (!input) {
        return false;
    }

    std::size_t inventoryCount = 0;
    input >> inventoryCount;
    inventory_.clear();
    for (std::size_t i = 0; i < inventoryCount; ++i) {
        InventoryEntry entry;
        input >> std::quoted(entry.name) >> entry.count;
        if (!input) {
            return false;
        }
        inventory_.push_back(std::move(entry));
    }
    player_.hp = clampValue(player_.hp, 1.0F, player_.maxHp);
    player_.mp = clampValue(player_.mp, 0.0F, player_.maxMp);
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
