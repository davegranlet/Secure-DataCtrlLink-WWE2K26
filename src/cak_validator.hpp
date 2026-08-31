#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace music_only {

struct CakInfo {
    std::uint32_t file_count{};
    std::uint32_t folder_count{};
    std::uint32_t key{};
};

bool validate_cak(const std::filesystem::path& path, CakInfo& info, std::string& error);

}  // namespace music_only
