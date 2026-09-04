#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace secure_dcl {

struct BuildResult {
    std::vector<std::byte> bank;
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};

BuildResult extract_music_bank(std::span<const std::byte> package);
BuildResult rebuild_music_bank(std::span<const std::byte> stock_bank);
bool validate_akpk(std::span<const std::byte> package, std::string &error);
std::uint32_t wwise_hash(std::string value);

} // namespace secure_dcl