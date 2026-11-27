#pragma once
#include <array>
#include <cstdint>

namespace jsd::shell::detail {
inline constexpr int atlasWidth = 512, atlasHeight = 256;
inline constexpr int glyphWidth = 32, glyphHeight = 40;
inline constexpr float glyphAdvance = 6, glyphBodyHeight = 7, lineAdvance = 8;
const std::array<std::uint8_t, atlasWidth * atlasHeight>& fontAtlas();
}
