#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace secure_dcl {
struct RuntimeConfig {
    std::size_t max_file_bytes{64u * 1024u * 1024u};
    std::size_t max_custom_packages{16};
    std::size_t max_cak_archives{32};
    std::uint32_t ready_timeout_ms{120000};
    std::uint32_t cak_post_prime_delay_ms{2500};
    bool file_present{};
    bool used_defaults{};
    std::string warning;
};
RuntimeConfig parse_runtime_config(std::string_view text);
RuntimeConfig load_runtime_config(const std::filesystem::path &game_dir);
} // namespace secure_dcl