#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace korework::content {

class BinaryReader final {
public:
    explicit BinaryReader(const std::filesystem::path& path)
        : stream_(path, std::ios::binary), path_(path) {
        static_assert(std::endian::native == std::endian::little,
                      "KOREWORK currently expects a little-endian target");
        if (!stream_) throw std::runtime_error("Unable to open binary file: " + path.string());
        stream_.seekg(0, std::ios::end);
        const auto end = stream_.tellg();
        if (end < 0) throw std::runtime_error("Unable to determine file size: " + path.string());
        size_ = static_cast<std::uint64_t>(end);
        stream_.seekg(0, std::ios::beg);
    }

    template <typename T>
    [[nodiscard]] T read() {
        static_assert(std::is_trivially_copyable_v<T>);
        T value {};
        readExact(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }

    template <typename T>
    [[nodiscard]] std::vector<T> readVector(std::size_t count) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (count > remaining() / sizeof(T)) throw std::runtime_error("Binary vector exceeds remaining file data: " + path_.string());
        std::vector<T> values(count);
        if (!values.empty()) readExact(reinterpret_cast<char*>(values.data()), values.size() * sizeof(T));
        return values;
    }

    [[nodiscard]] std::vector<std::byte> readBytes(std::size_t count) {
        if (count > remaining()) throw std::runtime_error("Binary byte range exceeds remaining file data: " + path_.string());
        std::vector<std::byte> bytes(count);
        if (!bytes.empty()) readExact(reinterpret_cast<char*>(bytes.data()), bytes.size());
        return bytes;
    }

    [[nodiscard]] std::string readString(std::size_t maximumLength = 16'384) {
        const std::int32_t signedLength = read<std::int32_t>();
        if (signedLength <= 0) return {};
        const auto length = static_cast<std::size_t>(signedLength);
        if (length > maximumLength || length > remaining()) {
            throw std::runtime_error("Invalid KO string length in: " + path_.string());
        }
        const auto bytes = readBytes(length);
        std::string value;
        value.reserve(length);
        for (const std::byte byte : bytes) value.push_back(static_cast<char>(byte));
        return value;
    }

    [[nodiscard]] std::string readFixedString(std::size_t count) {
        const auto bytes = readBytes(count);
        std::string value;
        value.reserve(count);
        for (const std::byte byte : bytes) {
            const char character = static_cast<char>(byte);
            if (character == '\0') break;
            value.push_back(character);
        }
        return value;
    }

    void skip(std::uint64_t count) {
        if (count > remaining()) throw std::runtime_error("Binary skip exceeds remaining file data: " + path_.string());
        stream_.seekg(static_cast<std::streamoff>(count), std::ios::cur);
        if (!stream_) throw std::runtime_error("Unable to seek binary file: " + path_.string());
    }

    [[nodiscard]] std::uint64_t position() {
        const auto current = stream_.tellg();
        if (current < 0) throw std::runtime_error("Unable to determine binary position: " + path_.string());
        return static_cast<std::uint64_t>(current);
    }

    [[nodiscard]] std::uint64_t size() const noexcept { return size_; }
    [[nodiscard]] std::uint64_t remaining() {
        const auto current = position();
        return current <= size_ ? size_ - current : 0;
    }

private:
    void readExact(char* destination, std::size_t count) {
        if (count > remaining()) throw std::runtime_error("Unexpected end of binary file: " + path_.string());
        stream_.read(destination, static_cast<std::streamsize>(count));
        if (stream_.gcount() != static_cast<std::streamsize>(count)) throw std::runtime_error("Short binary read: " + path_.string());
    }

    std::ifstream stream_;
    std::filesystem::path path_;
    std::uint64_t size_ = 0;
};

} // namespace korework::content
