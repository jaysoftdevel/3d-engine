#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace jsd {
// C++20 UTF-8 path construction, including Windows native wide paths.
inline std::filesystem::path utf8_path(std::string_view bytes){
    return std::filesystem::path(std::u8string(bytes.begin(),bytes.end()));
}
}
