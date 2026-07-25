#pragma once

#include "data/game_data_pack.hpp"
#include "offline_roster.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace korework {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct SkillDefinition {
    std::uint32_t id = 0;
    std::string name;
    float damage = 0.0F;
    float heal = 0.0F;
    float manaCost = 0.0F;
    float cooldown = 0.0F;
    float range = 0.0F;
    bool unlocked = false;
};

struct MonsterTemplate {
    std::uint32_t sid = 0;
    std::uint32_t modelId = 0;
    std::uint32_t rightHandItem = 0;
    std::uint32_t leftHandItem = 0;
    std::uint32_t dropTableId = 0;
    std::string name;
    int level = 1;
    float maxHp = 20.0F;
    float attack = 2.0F;
    float defense = 0.0F;
    float attackDelay = 1.3F;
    float movementSpeed = 1.0F;
    float runningSpeed = 1.5F;
    float attackRange = 1.55F;
    float searchRange = 9.5F;
    float chaseRange = 14.0F;
    int exp = 1;
    int money = 0;
    float scale = 1.0F;
};

struct MonsterState {
    std::uint64_t runtimeId = 0;
    std::size_t templateIndex = 0;
    Vec3 position;
    Vec3 spawnPosition;
    float hp = 0.0F;
    float attackCooldown = 0.0F;
    float respawnTimer = 0.0F;
    bool alive = true;
};

struct InventoryEntry {
    std::uint32_t itemId = 0;
    std::string name;
    int count = 0;
    std::uint8_t upgradeLevel = 0;
};

struct PlayerState {
    std::string name = "Adventurer";
    PlayerClass playerClass = PlayerClass::Warrior;
    Vec3 position {0.0F, 0.0F, 0.0F};
    float hp = 220.0F;
    float maxHp = 220.0F;
    float mp = 80.0F;
    float maxMp = 80.0F;
    int level = 1;
    int exp = 0;
    int gold = 2000;
    int deaths = 0;
    int strength = 65;
    int stamina = 60;
    int dexterity = 50;
    int intelligence = 30;
    int magicPower = 20;
    int bonusPoints = 0;
    int attackPower = 10;
    int defensePower = 5;
    std::array<std::uint32_t, 14> equipmentItemIds {};
    std::array<std::uint8_t, 14> equipmentUpgradeLevels {};
};

class OfflineRuntime final {
public:
    OfflineRuntime();

    void configureProfile(std::size_t slot, std::string name, PlayerClass playerClass);
    void initialize();
    void initialize(const std::filesystem::path& dataPackPath);
    void update(float deltaSeconds);
    void save() const;
    bool load();
    void reset();

    [[nodiscard]] const PlayerState& player() const noexcept;
    [[nodiscard]] PlayerState& player() noexcept;
    [[nodiscard]] const std::vector<MonsterState>& monsters() const noexcept;
    [[nodiscard]] const std::vector<MonsterTemplate>& monsterTemplates() const noexcept;
    [[nodiscard]] const std::array<SkillDefinition, 10>& skills() const noexcept;
    [[nodiscard]] const std::array<float, 10>& cooldowns() const noexcept;
    [[nodiscard]] const std::vector<InventoryEntry>& inventory() const noexcept;
    [[nodiscard]] const std::deque<std::string>& log() const noexcept;
    [[nodiscard]] const data::GameDataPack& gameData() const noexcept { return gameData_; }
    [[nodiscard]] bool usingGameData() const noexcept { return usingGameData_; }
    [[nodiscard]] std::size_t profileSlot() const noexcept { return profileSlot_; }

    [[nodiscard]] std::optional<std::size_t> nearestAliveMonster(float maxDistance) const;
    [[nodiscard]] const data::ItemRecord* itemRecord(std::uint32_t itemId) const noexcept;
    bool useSkill(std::size_t slot, std::optional<std::size_t> targetIndex);
    bool equipInventory(std::size_t inventoryIndex);
    bool unequip(std::size_t equipmentSlot);
    bool upgradeInventory(std::size_t inventoryIndex);
    bool spendStatPoint(std::size_t statIndex);
    void movePlayer(Vec3 delta);
    void refreshSkills() { createSkills(); }

    bool purchaseItem(std::uint32_t itemId, int count, int unitPrice) {
        if (count <= 0 || unitPrice < 0 || itemRecord(itemId) == nullptr) return false;
        const std::int64_t total = static_cast<std::int64_t>(count) * static_cast<std::int64_t>(unitPrice);
        if (total > player_.gold || total > std::numeric_limits<int>::max()) return false;
        player_.gold -= static_cast<int>(total);
        addInventory(itemId, itemName(itemId), count);
        appendLog(itemName(itemId) + " satin alindi.");
        save();
        return true;
    }

    bool restoreAtHealer(int cost) {
        if (cost < 0 || player_.gold < cost) return false;
        if (player_.hp >= player_.maxHp && player_.mp >= player_.maxMp) return false;
        player_.gold -= cost;
        player_.hp = player_.maxHp;
        player_.mp = player_.maxMp;
        appendLog("Sifaci HP ve MP degerlerini yeniledi.");
        save();
        return true;
    }

    bool grantQuestReward(std::uint32_t itemId, int count, int gold) {
        if (count < 0 || gold < 0 || player_.gold > std::numeric_limits<int>::max() - gold) return false;
        if (itemId != 0U && count > 0 && itemRecord(itemId) == nullptr) return false;
        player_.gold += gold;
        if (itemId != 0U && count > 0) addInventory(itemId, itemName(itemId), count);
        appendLog("Gorev odulu alindi: +" + std::to_string(gold) + " Noah.");
        save();
        return true;
    }

private:
    void loadGameData(const std::filesystem::path& dataPackPath);
    void applyClassDefaults();
    void recalculateDerivedStats(bool restoreResources);
    void createTemplates();
    void createFallbackTemplates();
    void createSkills();
    void createFallbackSkills();
    void spawnWorld();
    void updateMonsters(float deltaSeconds);
    void attackPlayer(MonsterState& monster, const MonsterTemplate& definition);
    void damageMonster(std::size_t monsterIndex, float amount, const SkillDefinition& skill);
    void killMonster(MonsterState& monster, const MonsterTemplate& definition);
    void awardDrops(const MonsterState& monster, const MonsterTemplate& definition);
    void addInventory(std::uint32_t itemId, std::string name, int count, std::uint8_t upgradeLevel = 0);
    void appendLog(std::string message);
    void respawnPlayer();
    [[nodiscard]] bool meetsRequirements(const data::ItemRecord& item) const noexcept;
    [[nodiscard]] std::filesystem::path savePath() const;
    [[nodiscard]] std::string itemName(std::uint32_t itemId) const;

    PlayerState player_;
    data::GameDataPack gameData_;
    bool usingGameData_ = false;
    std::size_t profileSlot_ = 0U;
    std::string configuredName_ = "Adventurer";
    PlayerClass configuredClass_ = PlayerClass::Warrior;
    std::vector<MonsterTemplate> monsterTemplates_;
    std::vector<MonsterState> monsters_;
    std::array<SkillDefinition, 10> skills_ {};
    std::array<float, 10> cooldowns_ {};
    std::vector<InventoryEntry> inventory_;
    std::deque<std::string> log_;
    std::unordered_map<std::uint32_t, std::size_t> dropTableIndex_;
    std::unordered_map<std::uint32_t, std::size_t> itemIndex_;
    std::unordered_map<std::uint32_t, std::string> itemNames_;
    std::uint64_t nextRuntimeId_ = 1;
    float autosaveTimer_ = 0.0F;
};

} // namespace korework
