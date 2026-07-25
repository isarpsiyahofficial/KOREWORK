#include "offline_runtime.hpp"

#include <algorithm>
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

} // namespace

OfflineRuntime::OfflineRuntime() = default;

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
    itemNames_.clear();
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
        itemNames_.clear();
        createTemplates();
        createSkills();
        if (!load()) reset();
        appendLog(std::string("KOPACK reddedildi: ") + exception.what());
        return;
    }

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
    itemNames_.clear();
    dropTableIndex_.reserve(gameData_.dropTables.size());
    itemNames_.reserve(gameData_.items.size());
    for (std::size_t index = 0; index < gameData_.dropTables.size(); ++index) {
        dropTableIndex_.emplace(gameData_.dropTables[index].id, index);
    }
    for (const auto& item : gameData_.items) itemNames_.emplace(item.id, item.name);
}

void OfflineRuntime::reset() {
    player_ = {};
    inventory_.clear();
    log_.clear();
    cooldowns_.fill(0.0F);
    nextRuntimeId_ = 1;
    spawnWorld();
    appendLog("Yeni offline karakter olusturuldu.");
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
        const std::uint32_t classPrefix = skill.id / 1000U;
        if (classPrefix == 101U || classPrefix == 105U || classPrefix == 106U) candidates.push_back(&skill);
    }
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
        definition.damage = healing ? 0.0F : std::max(10.0F, 16.0F + static_cast<float>(source.skillLevel) * 3.2F);
        definition.heal = healing ? std::max(15.0F, 12.0F + static_cast<float>(source.skillLevel) * 4.0F) : 0.0F;
        definition.manaCost = static_cast<float>(source.manaCost);
        definition.cooldown = std::clamp(static_cast<float>(source.cooldown) * 0.10F, 0.35F, 30.0F);
        definition.range = std::max(2.2F, static_cast<float>(source.range) * 0.10F);
        definition.unlocked = true;
        skills_[index] = std::move(definition);
    }
}

void OfflineRuntime::createFallbackSkills() {
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
    if (monsterTemplates_.empty()) return;

    std::vector<std::size_t> selected;
    selected.reserve(48);
    std::uint32_t lastModel = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t index = 0; index < monsterTemplates_.size() && selected.size() < 48U; ++index) {
        const auto& definition = monsterTemplates_[index];
        if (definition.level > 35 || definition.level < 1) continue;
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
    const float mitigation = static_cast<float>(player_.level) * 0.25F;
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
        player_.hp = std::min(player_.maxHp, player_.hp + skill.heal);
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
    const float levelBonus = static_cast<float>(player_.level - 1) * 2.0F;
    const float damage = std::max(1.0F, amount + levelBonus - definition.defense * 0.10F);
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
    while (player_.exp >= requiredExperience()) {
        player_.exp -= requiredExperience();
        ++player_.level;
        player_.maxHp += 22.0F;
        player_.maxMp += 12.0F;
        player_.hp = player_.maxHp;
        player_.mp = player_.maxMp;
        appendLog("Seviye atladin! Temel statlar arttirildi.");
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

void OfflineRuntime::addInventory(std::uint32_t itemId, std::string name, int count) {
    const auto iterator = std::find_if(inventory_.begin(), inventory_.end(), [&](const InventoryEntry& entry) {
        return itemId != 0U ? entry.itemId == itemId : entry.name == name;
    });
    if (iterator == inventory_.end()) inventory_.push_back({itemId, std::move(name), count});
    else iterator->count += count;
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
    if (!output) return;
    output << "KOREWORK_SAVE_V2\n";
    output << std::setprecision(9)
           << player_.position.x << ' ' << player_.position.y << ' ' << player_.position.z << ' '
           << player_.hp << ' ' << player_.maxHp << ' ' << player_.mp << ' ' << player_.maxMp << ' '
           << player_.level << ' ' << player_.exp << ' ' << player_.gold << ' ' << player_.deaths << '\n';
    output << inventory_.size() << '\n';
    for (const InventoryEntry& entry : inventory_) {
        output << entry.itemId << ' ' << std::quoted(entry.name) << ' ' << entry.count << '\n';
    }
}

bool OfflineRuntime::load() {
    std::ifstream input(savePath());
    if (!input) return false;
    std::string header;
    std::getline(input, header);
    const bool version1 = header == "KOREWORK_SAVE_V1";
    if (!version1 && header != "KOREWORK_SAVE_V2") return false;
    input >> player_.position.x >> player_.position.y >> player_.position.z
          >> player_.hp >> player_.maxHp >> player_.mp >> player_.maxMp
          >> player_.level >> player_.exp >> player_.gold >> player_.deaths;
    if (!input) return false;

    std::size_t inventoryCount = 0;
    input >> inventoryCount;
    if (inventoryCount > 100'000U) return false;
    inventory_.clear();
    for (std::size_t index = 0; index < inventoryCount; ++index) {
        InventoryEntry entry;
        if (version1) input >> std::quoted(entry.name) >> entry.count;
        else input >> entry.itemId >> std::quoted(entry.name) >> entry.count;
        if (!input || entry.count < 0) return false;
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
