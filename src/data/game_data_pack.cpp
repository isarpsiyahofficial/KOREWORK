#include "data/game_data_pack.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace korework::data {
namespace {

constexpr std::array<char, 8> kMagic {'K', 'O', 'P', 'A', 'C', 'K', '2', '\0'};
constexpr std::uint32_t kMaximumRecords = 10'000'000;
constexpr std::uint32_t kMaximumStringBytes = 65'536;
constexpr std::uint64_t kMaximumPayloadBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

class BufferWriter final {
public:
    template <typename T>
    void value(const T& input) {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* bytes = reinterpret_cast<const std::byte*>(&input);
        data_.insert(data_.end(), bytes, bytes + sizeof(T));
    }

    void boolean(bool input) { value(static_cast<std::uint8_t>(input ? 1U : 0U)); }

    void string(const std::string& input) {
        if (input.size() > kMaximumStringBytes) throw std::runtime_error("KOPACK string exceeds maximum length");
        value(static_cast<std::uint32_t>(input.size()));
        const auto* bytes = reinterpret_cast<const std::byte*>(input.data());
        data_.insert(data_.end(), bytes, bytes + input.size());
    }

    [[nodiscard]] const std::vector<std::byte>& data() const noexcept { return data_; }

private:
    std::vector<std::byte> data_;
};

class BufferReader final {
public:
    explicit BufferReader(const std::vector<std::byte>& data) : data_(data) {}

    template <typename T>
    [[nodiscard]] T value() {
        static_assert(std::is_trivially_copyable_v<T>);
        require(sizeof(T));
        T output {};
        std::memcpy(&output, data_.data() + position_, sizeof(T));
        position_ += sizeof(T);
        return output;
    }

    [[nodiscard]] bool boolean() {
        const std::uint8_t raw = value<std::uint8_t>();
        if (raw > 1U) throw std::runtime_error("Invalid KOPACK boolean value");
        return raw != 0U;
    }

    [[nodiscard]] std::string string() {
        const std::uint32_t length = value<std::uint32_t>();
        if (length > kMaximumStringBytes) throw std::runtime_error("Invalid KOPACK string length");
        require(length);
        std::string output(reinterpret_cast<const char*>(data_.data() + position_), length);
        position_ += length;
        return output;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - position_; }

private:
    void require(std::size_t count) const {
        if (count > data_.size() - position_) throw std::runtime_error("Unexpected end of KOPACK payload");
    }

    const std::vector<std::byte>& data_;
    std::size_t position_ = 0;
};

std::uint64_t checksum(const std::vector<std::byte>& bytes) noexcept {
    std::uint64_t value = kFnvOffset;
    for (const std::byte byte : bytes) {
        value ^= std::to_integer<std::uint8_t>(byte);
        value *= kFnvPrime;
    }
    return value;
}

void requireCount(std::size_t count, const char* label) {
    if (count > kMaximumRecords) throw std::runtime_error(std::string("Too many KOPACK ") + label + " records");
}

void requireFinite(float value, const char* label) {
    if (!std::isfinite(value)) throw std::runtime_error(std::string("Invalid KOPACK ") + label + " value");
}

template <typename T, typename Getter>
void requireUnique(const std::vector<T>& records, Getter getter, const char* label) {
    std::unordered_set<std::uint64_t> ids;
    ids.reserve(records.size());
    for (const auto& record : records) {
        const std::uint64_t id = static_cast<std::uint64_t>(getter(record));
        if (id == 0U) throw std::runtime_error(std::string("KOPACK ") + label + " id cannot be zero");
        if (!ids.insert(id).second) throw std::runtime_error(std::string("Duplicate KOPACK ") + label + " id: " + std::to_string(id));
    }
}

void writeMonster(BufferWriter& writer, const MonsterRecord& record) {
    writer.value(record.id); writer.value(record.sid); writer.string(record.name); writer.value(record.modelId); writer.value(record.sizePercent);
    writer.value(record.rightHandItem); writer.value(record.leftHandItem); writer.value(record.group); writer.value(record.rank);
    writer.value(record.title); writer.value(record.level); writer.value(record.hp); writer.value(record.mp); writer.value(record.attack);
    writer.value(record.defense); writer.value(record.hitRate); writer.value(record.evasionRate); writer.value(record.damage);
    writer.value(record.attackDelayMs); writer.value(record.movementSpeed); writer.value(record.runningSpeed); writer.value(record.attackRange);
    writer.value(record.searchRange); writer.value(record.chaseRange); writer.value(record.skill1); writer.value(record.skill2); writer.value(record.skill3);
    writer.value(record.fireResistance); writer.value(record.coldResistance); writer.value(record.lightningResistance);
    writer.value(record.magicResistance); writer.value(record.diseaseResistance); writer.value(record.poisonResistance);
    writer.value(record.experience); writer.value(record.loyalty); writer.value(record.money); writer.value(record.dropTableId);
}

MonsterRecord readMonster(BufferReader& reader) {
    MonsterRecord record;
    record.id = reader.value<std::uint32_t>(); record.sid = reader.value<std::uint16_t>(); record.name = reader.string();
    record.modelId = reader.value<std::uint32_t>(); record.sizePercent = reader.value<std::uint16_t>();
    record.rightHandItem = reader.value<std::uint32_t>(); record.leftHandItem = reader.value<std::uint32_t>();
    record.group = reader.value<std::uint8_t>(); record.rank = reader.value<std::uint8_t>(); record.title = reader.value<std::uint8_t>();
    record.level = reader.value<std::uint16_t>(); record.hp = reader.value<std::uint32_t>(); record.mp = reader.value<std::uint32_t>();
    record.attack = reader.value<std::uint16_t>(); record.defense = reader.value<std::uint16_t>(); record.hitRate = reader.value<std::uint16_t>();
    record.evasionRate = reader.value<std::uint16_t>(); record.damage = reader.value<std::uint16_t>();
    record.attackDelayMs = reader.value<std::uint16_t>(); record.movementSpeed = reader.value<float>(); record.runningSpeed = reader.value<float>();
    record.attackRange = reader.value<float>(); record.searchRange = reader.value<float>(); record.chaseRange = reader.value<float>();
    record.skill1 = reader.value<std::uint32_t>(); record.skill2 = reader.value<std::uint32_t>(); record.skill3 = reader.value<std::uint32_t>();
    record.fireResistance = reader.value<std::int16_t>(); record.coldResistance = reader.value<std::int16_t>();
    record.lightningResistance = reader.value<std::int16_t>(); record.magicResistance = reader.value<std::int16_t>();
    record.diseaseResistance = reader.value<std::int16_t>(); record.poisonResistance = reader.value<std::int16_t>();
    record.experience = reader.value<std::uint32_t>(); record.loyalty = reader.value<std::uint32_t>();
    record.money = reader.value<std::uint32_t>(); record.dropTableId = reader.value<std::uint32_t>();
    return record;
}

void writeItem(BufferWriter& writer, const ItemRecord& record) {
    writer.value(record.id); writer.value(record.extensionId); writer.string(record.name); writer.string(record.description);
    writer.value(record.kind); writer.value(record.slot); writer.value(record.race); writer.value(record.classRestriction);
    writer.value(record.damage); writer.value(record.attackDelay); writer.value(record.range); writer.value(record.weight);
    writer.value(record.durability); writer.value(record.buyPrice); writer.value(record.sellPrice); writer.value(record.armor);
    writer.boolean(record.countable); writer.value(record.effect1); writer.value(record.effect2); writer.value(record.requiredLevel);
    writer.value(record.requiredRank); writer.value(record.requiredTitle); writer.value(record.requiredStrength); writer.value(record.requiredStamina);
    writer.value(record.requiredDexterity); writer.value(record.requiredIntelligence); writer.value(record.requiredMagicPower);
    writer.value(record.bonusStrength); writer.value(record.bonusStamina); writer.value(record.bonusDexterity);
    writer.value(record.bonusIntelligence); writer.value(record.bonusMagicPower); writer.value(record.bonusHp); writer.value(record.bonusMp);
    writer.value(record.fireDamage); writer.value(record.coldDamage); writer.value(record.lightningDamage); writer.value(record.poisonDamage);
    writer.value(record.fireResistance); writer.value(record.coldResistance); writer.value(record.lightningResistance);
    writer.value(record.magicResistance); writer.value(record.diseaseResistance); writer.value(record.poisonResistance);
    writer.value(record.iconId); writer.value(record.appearanceId);
}

ItemRecord readItem(BufferReader& reader) {
    ItemRecord record;
    record.id = reader.value<std::uint32_t>(); record.extensionId = reader.value<std::uint16_t>(); record.name = reader.string();
    record.description = reader.string(); record.kind = reader.value<std::uint8_t>(); record.slot = reader.value<std::uint8_t>();
    record.race = reader.value<std::uint8_t>(); record.classRestriction = reader.value<std::uint8_t>(); record.damage = reader.value<std::int16_t>();
    record.attackDelay = reader.value<std::int16_t>(); record.range = reader.value<std::int16_t>(); record.weight = reader.value<std::int16_t>();
    record.durability = reader.value<std::int16_t>(); record.buyPrice = reader.value<std::uint32_t>(); record.sellPrice = reader.value<std::uint32_t>();
    record.armor = reader.value<std::int16_t>(); record.countable = reader.boolean(); record.effect1 = reader.value<std::uint32_t>();
    record.effect2 = reader.value<std::uint32_t>(); record.requiredLevel = reader.value<std::uint8_t>(); record.requiredRank = reader.value<std::uint8_t>();
    record.requiredTitle = reader.value<std::uint8_t>(); record.requiredStrength = reader.value<std::uint8_t>();
    record.requiredStamina = reader.value<std::uint8_t>(); record.requiredDexterity = reader.value<std::uint8_t>();
    record.requiredIntelligence = reader.value<std::uint8_t>(); record.requiredMagicPower = reader.value<std::uint8_t>();
    record.bonusStrength = reader.value<std::int16_t>(); record.bonusStamina = reader.value<std::int16_t>();
    record.bonusDexterity = reader.value<std::int16_t>(); record.bonusIntelligence = reader.value<std::int16_t>();
    record.bonusMagicPower = reader.value<std::int16_t>(); record.bonusHp = reader.value<std::int16_t>(); record.bonusMp = reader.value<std::int16_t>();
    record.fireDamage = reader.value<std::int16_t>(); record.coldDamage = reader.value<std::int16_t>();
    record.lightningDamage = reader.value<std::int16_t>(); record.poisonDamage = reader.value<std::int16_t>();
    record.fireResistance = reader.value<std::int16_t>(); record.coldResistance = reader.value<std::int16_t>();
    record.lightningResistance = reader.value<std::int16_t>(); record.magicResistance = reader.value<std::int16_t>();
    record.diseaseResistance = reader.value<std::int16_t>(); record.poisonResistance = reader.value<std::int16_t>();
    record.iconId = reader.value<std::uint32_t>(); record.appearanceId = reader.value<std::uint32_t>();
    return record;
}

void writeSkill(BufferWriter& writer, const SkillRecord& record) {
    writer.value(record.id); writer.string(record.name); writer.string(record.description); writer.value(record.userAnimation);
    writer.value(record.targetAnimation); writer.value(record.selfEffect); writer.value(record.projectileEffect); writer.value(record.targetEffect);
    writer.value(record.targetType); writer.value(record.moral); writer.value(record.skillLevel); writer.value(record.requiredSkillPoints);
    writer.value(record.manaCost); writer.value(record.hpCost); writer.value(record.requiredItem); writer.value(record.castTime);
    writer.value(record.cooldown); writer.value(record.successRate); writer.value(record.type1); writer.value(record.type2); writer.value(record.range);
}

SkillRecord readSkill(BufferReader& reader) {
    SkillRecord record;
    record.id = reader.value<std::uint32_t>(); record.name = reader.string(); record.description = reader.string();
    record.userAnimation = reader.value<std::uint16_t>(); record.targetAnimation = reader.value<std::uint16_t>();
    record.selfEffect = reader.value<std::uint16_t>(); record.projectileEffect = reader.value<std::uint16_t>();
    record.targetEffect = reader.value<std::uint16_t>(); record.targetType = reader.value<std::uint8_t>(); record.moral = reader.value<std::uint8_t>();
    record.skillLevel = reader.value<std::uint16_t>(); record.requiredSkillPoints = reader.value<std::uint16_t>();
    record.manaCost = reader.value<std::uint16_t>(); record.hpCost = reader.value<std::uint16_t>(); record.requiredItem = reader.value<std::uint32_t>();
    record.castTime = reader.value<std::uint16_t>(); record.cooldown = reader.value<std::uint16_t>(); record.successRate = reader.value<std::uint8_t>();
    record.type1 = reader.value<std::uint8_t>(); record.type2 = reader.value<std::uint8_t>(); record.range = reader.value<std::uint16_t>();
    return record;
}

void writeClass(BufferWriter& writer, const ClassCoefficientRecord& record) {
    writer.value(record.classId); writer.value(record.shortSword); writer.value(record.sword); writer.value(record.axe);
    writer.value(record.club); writer.value(record.spear); writer.value(record.pole); writer.value(record.staff); writer.value(record.bow);
    writer.value(record.hp); writer.value(record.mp); writer.value(record.sp); writer.value(record.armor); writer.value(record.hitRate); writer.value(record.evasionRate);
}

ClassCoefficientRecord readClass(BufferReader& reader) {
    ClassCoefficientRecord record;
    record.classId = reader.value<std::uint16_t>(); record.shortSword = reader.value<float>(); record.sword = reader.value<float>();
    record.axe = reader.value<float>(); record.club = reader.value<float>(); record.spear = reader.value<float>(); record.pole = reader.value<float>();
    record.staff = reader.value<float>(); record.bow = reader.value<float>(); record.hp = reader.value<float>(); record.mp = reader.value<float>();
    record.sp = reader.value<float>(); record.armor = reader.value<float>(); record.hitRate = reader.value<float>(); record.evasionRate = reader.value<float>();
    return record;
}

void writeDropTable(BufferWriter& writer, const DropTableRecord& record) {
    writer.value(record.id);
    for (const auto& entry : record.entries) { writer.value(entry.itemId); writer.value(entry.chance); }
}

DropTableRecord readDropTable(BufferReader& reader) {
    DropTableRecord record;
    record.id = reader.value<std::uint32_t>();
    for (auto& entry : record.entries) { entry.itemId = reader.value<std::uint32_t>(); entry.chance = reader.value<std::uint16_t>(); }
    return record;
}

} // namespace

void GameDataPack::validate() const {
    static_assert(std::endian::native == std::endian::little, "KOPACK currently requires a little-endian target");
    requireCount(monsters.size(), "monster"); requireCount(items.size(), "item"); requireCount(skills.size(), "skill");
    requireCount(classes.size(), "class"); requireCount(dropTables.size(), "drop table");
    requireUnique(monsters, [](const MonsterRecord& record) { return record.id; }, "monster");
    requireUnique(items, [](const ItemRecord& record) { return record.id; }, "item");
    requireUnique(skills, [](const SkillRecord& record) { return record.id; }, "skill");
    requireUnique(classes, [](const ClassCoefficientRecord& record) { return record.classId; }, "class");
    requireUnique(dropTables, [](const DropTableRecord& record) { return record.id; }, "drop table");

    for (const auto& record : monsters) {
        if (record.name.empty() || record.name.size() > kMaximumStringBytes || record.level == 0 || record.hp == 0
            || record.sizePercent == 0 || record.sizePercent > 1000) {
            throw std::runtime_error("Invalid KOPACK monster record: " + std::to_string(record.id));
        }
        requireFinite(record.movementSpeed, "monster movement speed"); requireFinite(record.runningSpeed, "monster running speed");
        requireFinite(record.attackRange, "monster attack range"); requireFinite(record.searchRange, "monster search range");
        requireFinite(record.chaseRange, "monster chase range");
        if (record.movementSpeed < 0.0F || record.runningSpeed < 0.0F || record.attackRange < 0.0F
            || record.searchRange < 0.0F || record.chaseRange < 0.0F) {
            throw std::runtime_error("Negative KOPACK monster movement/range value");
        }
    }
    for (const auto& record : items) {
        if (record.name.empty() || record.name.size() > kMaximumStringBytes || record.description.size() > kMaximumStringBytes) {
            throw std::runtime_error("Invalid KOPACK item record: " + std::to_string(record.id));
        }
    }
    for (const auto& record : skills) {
        if (record.name.empty() || record.name.size() > kMaximumStringBytes || record.description.size() > kMaximumStringBytes) {
            throw std::runtime_error("Invalid KOPACK skill record: " + std::to_string(record.id));
        }
        if (record.successRate > 100U) throw std::runtime_error("Invalid KOPACK skill success rate");
    }
    for (const auto& record : classes) {
        const std::array<float, 14> values {record.shortSword, record.sword, record.axe, record.club, record.spear, record.pole,
                                            record.staff, record.bow, record.hp, record.mp, record.sp, record.armor,
                                            record.hitRate, record.evasionRate};
        for (const float value : values) {
            requireFinite(value, "class coefficient");
            if (value < 0.0F || value > 1000.0F) throw std::runtime_error("Invalid KOPACK class coefficient range");
        }
    }
    for (const auto& table : dropTables) {
        for (const auto& entry : table.entries) {
            if (entry.chance > 10'000U) throw std::runtime_error("Invalid KOPACK drop chance");
            if (entry.itemId == 0U && entry.chance != 0U) throw std::runtime_error("KOPACK drop chance without item");
        }
    }
}

void GameDataPack::save(const std::filesystem::path& path) const {
    validate();
    BufferWriter writer;
    for (const auto& record : monsters) writeMonster(writer, record);
    for (const auto& record : items) writeItem(writer, record);
    for (const auto& record : skills) writeSkill(writer, record);
    for (const auto& record : classes) writeClass(writer, record);
    for (const auto& record : dropTables) writeDropTable(writer, record);
    const auto& payload = writer.data();
    if (payload.size() > kMaximumPayloadBytes) throw std::runtime_error("KOPACK payload exceeds maximum size");

    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Unable to create KOPACK file: " + path.string());
    output.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
    const std::uint32_t version = CurrentVersion;
    const std::uint64_t payloadSize = payload.size();
    const std::uint64_t payloadChecksum = checksum(payload);
    const std::array<std::uint32_t, 5> counts {
        static_cast<std::uint32_t>(monsters.size()), static_cast<std::uint32_t>(items.size()),
        static_cast<std::uint32_t>(skills.size()), static_cast<std::uint32_t>(classes.size()),
        static_cast<std::uint32_t>(dropTables.size())
    };
    output.write(reinterpret_cast<const char*>(&version), sizeof(version));
    output.write(reinterpret_cast<const char*>(&payloadSize), sizeof(payloadSize));
    output.write(reinterpret_cast<const char*>(&payloadChecksum), sizeof(payloadChecksum));
    output.write(reinterpret_cast<const char*>(counts.data()), static_cast<std::streamsize>(sizeof(counts)));
    if (!payload.empty()) output.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (!output) throw std::runtime_error("Unable to finish KOPACK file: " + path.string());
}

GameDataPack GameDataPack::load(const std::filesystem::path& path) {
    static_assert(std::endian::native == std::endian::little, "KOPACK currently requires a little-endian target");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open KOPACK file: " + path.string());

    std::array<char, 8> magic {};
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (magic != kMagic) throw std::runtime_error("Invalid KOPACK magic");
    std::uint32_t version = 0;
    std::uint64_t payloadSize = 0;
    std::uint64_t expectedChecksum = 0;
    std::array<std::uint32_t, 5> counts {};
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    input.read(reinterpret_cast<char*>(&payloadSize), sizeof(payloadSize));
    input.read(reinterpret_cast<char*>(&expectedChecksum), sizeof(expectedChecksum));
    input.read(reinterpret_cast<char*>(counts.data()), static_cast<std::streamsize>(sizeof(counts)));
    if (!input) throw std::runtime_error("Truncated KOPACK header");
    if (version != CurrentVersion) throw std::runtime_error("Unsupported KOPACK version: " + std::to_string(version));
    if (payloadSize > kMaximumPayloadBytes) throw std::runtime_error("Invalid KOPACK payload size");
    for (const std::uint32_t count : counts) if (count > kMaximumRecords) throw std::runtime_error("Invalid KOPACK record count");

    std::vector<std::byte> payload(static_cast<std::size_t>(payloadSize));
    if (!payload.empty()) input.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (!input) throw std::runtime_error("Truncated KOPACK payload");
    char trailing = 0;
    if (input.read(&trailing, 1)) throw std::runtime_error("Unexpected trailing KOPACK data");
    if (checksum(payload) != expectedChecksum) throw std::runtime_error("KOPACK checksum mismatch");

    BufferReader reader(payload);
    GameDataPack pack;
    pack.monsters.reserve(counts[0]); pack.items.reserve(counts[1]); pack.skills.reserve(counts[2]);
    pack.classes.reserve(counts[3]); pack.dropTables.reserve(counts[4]);
    for (std::uint32_t index = 0; index < counts[0]; ++index) pack.monsters.push_back(readMonster(reader));
    for (std::uint32_t index = 0; index < counts[1]; ++index) pack.items.push_back(readItem(reader));
    for (std::uint32_t index = 0; index < counts[2]; ++index) pack.skills.push_back(readSkill(reader));
    for (std::uint32_t index = 0; index < counts[3]; ++index) pack.classes.push_back(readClass(reader));
    for (std::uint32_t index = 0; index < counts[4]; ++index) pack.dropTables.push_back(readDropTable(reader));
    if (reader.remaining() != 0U) throw std::runtime_error("Unexpected trailing KOPACK payload bytes");
    pack.validate();
    return pack;
}

} // namespace korework::data
