#include "content/npc_looks_table.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

enum Type : std::uint32_t { Character = 1, Byte = 2, Short = 3, Word = 4, Integer = 5, Dword = 6, String = 7 };

class Writer {
public:
    template <typename T>
    void value(T input) {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* bytes = reinterpret_cast<const std::byte*>(&input);
        data.insert(data.end(), bytes, bytes + sizeof(T));
    }

    void text(const std::string& value) {
        this->value(static_cast<std::int32_t>(value.size()));
        const auto* bytes = reinterpret_cast<const std::byte*>(value.data());
        data.insert(data.end(), bytes, bytes + value.size());
    }

    std::vector<std::byte> data;
};

std::vector<std::byte> encrypt(std::vector<std::byte> plain) {
    std::uint16_t key = 0x0816U;
    constexpr std::uint16_t multiplier = 0x6081U;
    constexpr std::uint16_t increment = 0x1608U;
    for (std::byte& byte : plain) {
        const auto source = std::to_integer<std::uint8_t>(byte);
        const auto cipher = static_cast<std::uint8_t>(source ^ static_cast<std::uint8_t>(key >> 8U));
        key = static_cast<std::uint16_t>((static_cast<std::uint32_t>(cipher) + key) * multiplier + increment);
        byte = static_cast<std::byte>(cipher);
    }
    return plain;
}

} // namespace

int main() {
    Writer writer;
    const std::array<std::uint32_t, 38> types {
        Dword, String, String, String,
        String, String, String, String, String, String, String, String, String, String,
        String, String, String,
        Integer, Integer, Integer, Integer, Integer,
        Integer, Integer, Integer, Integer, Integer, Integer, Integer, Integer, Integer, Integer, Integer,
        Integer, Integer, Byte, Byte, Byte
    };
    writer.value(static_cast<std::int32_t>(types.size()));
    for (const auto type : types) writer.value(type);
    writer.value<std::int32_t>(1);
    writer.value<std::uint32_t>(100);
    writer.text("Kecoon");
    writer.text("chr\\kecoon.n3joint");
    writer.text("chr\\kecoon.n3anim");
    for (int index = 0; index < 10; ++index) writer.text(index == 0 ? "chr\\kecoon.n3cpart" : "");
    writer.text("");
    writer.text("chr\\mob_kecoon.n3chr");
    writer.text("");
    writer.value<std::int32_t>(0);
    writer.value<std::int32_t>(12);
    writer.value<std::int32_t>(13);
    writer.value<std::int32_t>(14);
    writer.value<std::int32_t>(15);
    for (int index = 0; index < 11; ++index) writer.value<std::int32_t>(0);
    writer.value<std::int32_t>(0);
    writer.value<std::int32_t>(0);
    writer.value<std::uint8_t>(0);
    writer.value<std::uint8_t>(0);
    writer.value<std::uint8_t>(0);

    const auto path = std::filesystem::temp_directory_path() / "korework_npc_looks.tbl";
    const auto encrypted = encrypt(std::move(writer.data));
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(encrypted.data()), static_cast<std::streamsize>(encrypted.size()));
    output.close();

    const auto table = korework::content::NpcLooksTable::load(path);
    assert(table.records().size() == 1U);
    const auto* look = table.find(100);
    assert(look != nullptr);
    assert(look->name == "Kecoon");
    assert(look->characterReference == "chr\\mob_kecoon.n3chr");
    assert(look->rightHandJoint == 12);
    std::filesystem::remove(path);
    return 0;
}
