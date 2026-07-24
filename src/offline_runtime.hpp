#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace korework {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct SkillDefinition {
    int id = 0;
    std::string name;
    float damage = 0.0F;
    float heal = 0.0F;
    float manaCost = 0.0F;
    float cooldown = 0.0F;
    float range = 0.0F;
    bool unlocked = false;
};

struct MonsterTemplate {
    int sid = 0;
    std::string name;
    int level = 1;
    float maxHp = 20.0F;
    float attack = 2.0F;
    float defense = 0.0F;
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
    std::string name;
    int count = 0;
};

struct PlayerState {
    Vec3 position {0.0F, 0.0F, 0.0F};
    float hp = 180.0F;
    float maxHp = 180.0F;
    float mp = 120.0F;
    float maxMp = 120.0F;
    int level = 1;
    int exp = 0;
    int gold = 0;
    int deaths = 0;
};

class OfflineRuntime final {
public:
    OfflineRuntime();

    void initialize();
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

    [[nodiscard]] std::optional<std::size_t> nearestAliveMonster(float maxDistance) const;
    bool useSkill(std::size_t slot, std::optional<std::size_t> targetIndex);
    void movePlayer(Vec3 delta);

private:
    void createTemplates();
    void createSkills();
    void spawnWorld();
    void updateMonsters(float deltaSeconds);
    void attackPlayer(MonsterState& monster, const MonsterTemplate& definition);
    void damageMonster(std::size_t monsterIndex, float amount, const SkillDefinition& skill);
    void killMonster(MonsterState& monster, const MonsterTemplate& definition);
    void addInventory(std::string name, int count);
    void appendLog(std::string message);
    void respawnPlayer();
    [[nodiscard]] std::filesystem::path savePath() const;

    PlayerState player_;
    std::vector<MonsterTemplate> monsterTemplates_;
    std::vector<MonsterState> monsters_;
    std::array<SkillDefinition, 10> skills_ {};
    std::array<float, 10> cooldowns_ {};
    std::vector<InventoryEntry> inventory_;
    std::deque<std::string> log_;
    std::uint64_t nextRuntimeId_ = 1;
    float autosaveTimer_ = 0.0F;
};

} // namespace korework
