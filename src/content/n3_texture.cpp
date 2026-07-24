#include "content/n3_texture.hpp"

#include "content/binary_reader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace korework::content {
namespace {

constexpr std::uint32_t kDxt1 = 0x31545844U; // DXT1
constexpr std::uint32_t kDxt3 = 0x33545844U; // DXT3
constexpr std::uint32_t kDxt5 = 0x35545844U; // DXT5
constexpr std::int32_t kMaximumTextureDimension = 8192;

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

std::uint16_t read16(const std::byte* data) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(data[0]))
        | static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(data[1]) << 8U);
}

std::uint32_t read32(const std::byte* data) {
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[0]))
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[3])) << 24U);
}

Rgba rgb565(std::uint16_t value) {
    const std::uint8_t red5 = static_cast<std::uint8_t>((value >> 11U) & 0x1FU);
    const std::uint8_t green6 = static_cast<std::uint8_t>((value >> 5U) & 0x3FU);
    const std::uint8_t blue5 = static_cast<std::uint8_t>(value & 0x1FU);
    return {
        static_cast<std::uint8_t>((red5 << 3U) | (red5 >> 2U)),
        static_cast<std::uint8_t>((green6 << 2U) | (green6 >> 4U)),
        static_cast<std::uint8_t>((blue5 << 3U) | (blue5 >> 2U)),
        255U
    };
}

Rgba blend(const Rgba& left, const Rgba& right, unsigned leftWeight, unsigned rightWeight, unsigned divisor) {
    return {
        static_cast<std::uint8_t>((left.r * leftWeight + right.r * rightWeight) / divisor),
        static_cast<std::uint8_t>((left.g * leftWeight + right.g * rightWeight) / divisor),
        static_cast<std::uint8_t>((left.b * leftWeight + right.b * rightWeight) / divisor),
        255U
    };
}

std::array<Rgba, 4> colorPalette(const std::byte* block, bool forceFourColors) {
    const std::uint16_t color0 = read16(block);
    const std::uint16_t color1 = read16(block + 2);
    std::array<Rgba, 4> palette {rgb565(color0), rgb565(color1), {}, {}};
    if (forceFourColors || color0 > color1) {
        palette[2] = blend(palette[0], palette[1], 2U, 1U, 3U);
        palette[3] = blend(palette[0], palette[1], 1U, 2U, 3U);
    } else {
        palette[2] = blend(palette[0], palette[1], 1U, 1U, 2U);
        palette[3] = {0U, 0U, 0U, 0U};
    }
    return palette;
}

void writePixel(N3TextureData& texture, std::int32_t x, std::int32_t y, const Rgba& pixel) {
    if (x < 0 || y < 0 || x >= texture.width || y >= texture.height) return;
    const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(texture.width)
                                + static_cast<std::size_t>(x)) * 4U;
    texture.rgba[offset + 0U] = pixel.r;
    texture.rgba[offset + 1U] = pixel.g;
    texture.rgba[offset + 2U] = pixel.b;
    texture.rgba[offset + 3U] = pixel.a;
}

void decodeDxt1Block(N3TextureData& texture, const std::byte* block, std::int32_t baseX, std::int32_t baseY) {
    const auto palette = colorPalette(block, false);
    const std::uint32_t selectors = read32(block + 4);
    for (std::int32_t pixel = 0; pixel < 16; ++pixel) {
        const std::uint32_t selector = (selectors >> static_cast<unsigned>(pixel * 2)) & 0x3U;
        writePixel(texture, baseX + pixel % 4, baseY + pixel / 4, palette[selector]);
    }
}

void decodeDxt3Block(N3TextureData& texture, const std::byte* block, std::int32_t baseX, std::int32_t baseY) {
    std::uint64_t alphaBits = 0;
    for (unsigned index = 0; index < 8; ++index) {
        alphaBits |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(block[index])) << (index * 8U);
    }
    const auto palette = colorPalette(block + 8, true);
    const std::uint32_t selectors = read32(block + 12);
    for (std::int32_t pixel = 0; pixel < 16; ++pixel) {
        const std::uint32_t selector = (selectors >> static_cast<unsigned>(pixel * 2)) & 0x3U;
        Rgba color = palette[selector];
        color.a = static_cast<std::uint8_t>(((alphaBits >> static_cast<unsigned>(pixel * 4)) & 0xFU) * 17U);
        writePixel(texture, baseX + pixel % 4, baseY + pixel / 4, color);
    }
}

void decodeDxt5Block(N3TextureData& texture, const std::byte* block, std::int32_t baseX, std::int32_t baseY) {
    const std::uint8_t alpha0 = std::to_integer<std::uint8_t>(block[0]);
    const std::uint8_t alpha1 = std::to_integer<std::uint8_t>(block[1]);
    std::array<std::uint8_t, 8> alpha {alpha0, alpha1, 0U, 0U, 0U, 0U, 0U, 0U};
    if (alpha0 > alpha1) {
        for (unsigned index = 1; index <= 6; ++index) {
            alpha[index + 1U] = static_cast<std::uint8_t>(((7U - index) * alpha0 + index * alpha1) / 7U);
        }
    } else {
        for (unsigned index = 1; index <= 4; ++index) {
            alpha[index + 1U] = static_cast<std::uint8_t>(((5U - index) * alpha0 + index * alpha1) / 5U);
        }
        alpha[6] = 0U;
        alpha[7] = 255U;
    }

    std::uint64_t alphaSelectors = 0;
    for (unsigned index = 0; index < 6; ++index) {
        alphaSelectors |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(block[2U + index])) << (index * 8U);
    }
    const auto palette = colorPalette(block + 8, true);
    const std::uint32_t colorSelectors = read32(block + 12);
    for (std::int32_t pixel = 0; pixel < 16; ++pixel) {
        const std::uint32_t colorIndex = (colorSelectors >> static_cast<unsigned>(pixel * 2)) & 0x3U;
        const std::uint32_t alphaIndex = static_cast<std::uint32_t>((alphaSelectors >> static_cast<unsigned>(pixel * 3)) & 0x7U);
        Rgba color = palette[colorIndex];
        color.a = alpha[alphaIndex];
        writePixel(texture, baseX + pixel % 4, baseY + pixel / 4, color);
    }
}

} // namespace

N3TextureData N3TextureLoader::load(const std::filesystem::path& path) {
    BinaryReader reader(path);
    N3TextureData texture;
    texture.sourcePath = path;
    texture.name = reader.readString();

    const auto identifier = reader.readBytes(4);
    if (identifier.size() != 4U
        || std::to_integer<char>(identifier[0]) != 'N'
        || std::to_integer<char>(identifier[1]) != 'T'
        || std::to_integer<char>(identifier[2]) != 'F') {
        throw std::runtime_error("Invalid Noah texture header: " + path.string());
    }
    const std::uint8_t version = std::to_integer<std::uint8_t>(identifier[3]);
    if (version == 7U) {
        throw std::runtime_error("Encrypted NTF7 textures are intentionally unsupported: " + path.string());
    }
    if (version < 3U || version > 6U) {
        throw std::runtime_error("Unsupported Noah texture version: " + std::to_string(version));
    }

    texture.width = reader.read<std::int32_t>();
    texture.height = reader.read<std::int32_t>();
    texture.format = reader.read<std::uint32_t>();
    texture.hasMipMaps = reader.read<std::int32_t>() != 0;
    if (texture.width <= 0 || texture.height <= 0
        || texture.width > kMaximumTextureDimension || texture.height > kMaximumTextureDimension) {
        throw std::runtime_error("Invalid Noah texture dimensions");
    }

    const std::size_t pixelCount = static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height);
    texture.rgba.assign(pixelCount * 4U, 0U);
    const std::int32_t blocksX = (texture.width + 3) / 4;
    const std::int32_t blocksY = (texture.height + 3) / 4;

    if (texture.format == 0U) {
        const auto raw = reader.readBytes(pixelCount * 4U);
        for (std::size_t index = 0; index < raw.size(); ++index) {
            texture.rgba[index] = std::to_integer<std::uint8_t>(raw[index]);
        }
        return texture;
    }

    const std::size_t blockBytes = texture.format == kDxt1 ? 8U : 16U;
    if (texture.format != kDxt1 && texture.format != kDxt3 && texture.format != kDxt5) {
        throw std::runtime_error("Unsupported Noah texture DXT format: " + std::to_string(texture.format));
    }

    const std::size_t topLevelSize = static_cast<std::size_t>(blocksX) * static_cast<std::size_t>(blocksY) * blockBytes;
    const auto compressed = reader.readBytes(topLevelSize);
    for (std::int32_t blockY = 0; blockY < blocksY; ++blockY) {
        for (std::int32_t blockX = 0; blockX < blocksX; ++blockX) {
            const std::size_t offset = (static_cast<std::size_t>(blockY) * static_cast<std::size_t>(blocksX)
                                        + static_cast<std::size_t>(blockX)) * blockBytes;
            const std::byte* block = compressed.data() + offset;
            if (texture.format == kDxt1) decodeDxt1Block(texture, block, blockX * 4, blockY * 4);
            else if (texture.format == kDxt3) decodeDxt3Block(texture, block, blockX * 4, blockY * 4);
            else decodeDxt5Block(texture, block, blockX * 4, blockY * 4);
        }
    }
    return texture;
}

} // namespace korework::content
