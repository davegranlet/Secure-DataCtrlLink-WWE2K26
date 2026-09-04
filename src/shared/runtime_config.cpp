#include "runtime_config.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace secure_dcl {
namespace {
constexpr std::size_t kMaxIniBytes = 16 * 1024;
std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return value;
}
bool number(std::string_view text, std::uint64_t &value) {
    text = trim(text);
    if (text.empty())
        return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
RuntimeConfig invalid(std::string reason) {
    RuntimeConfig value;
    value.file_present = true;
    value.used_defaults = true;
    value.warning = std::move(reason);
    return value;
}
} // namespace
RuntimeConfig parse_runtime_config(std::string_view text) {
    RuntimeConfig result;
    result.file_present = true;
    std::unordered_set<std::string> seen;
    std::size_t cursor = 0;
    while (cursor <= text.size()) {
        const auto end = text.find_first_of("\r\n", cursor);
        auto line = trim(text.substr(cursor, end == std::string_view::npos ? text.size() - cursor
                                                                           : end - cursor));
        cursor = end == std::string_view::npos ? text.size() + 1 : end + 1;
        if (line.empty() || line.front() == ';' || line.front() == '#')
            continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos)
            return invalid("configuration line is missing '='");
        std::string key(trim(line.substr(0, equals)));
        std::uint64_t value = 0;
        if (!number(line.substr(equals + 1), value))
            return invalid("configuration value is not an unsigned integer");
        if (!seen.insert(key).second)
            return invalid("duplicate configuration key");
        if (key == "MaxFileBytes") {
            if (value < 1024 * 1024 || value > 64ull * 1024 * 1024)
                return invalid("MaxFileBytes is outside 1-64 MiB");
            result.max_file_bytes = static_cast<std::size_t>(value);
        } else if (key == "MaxCustomPackages") {
            if (value < 1 || value > 16)
                return invalid("MaxCustomPackages is outside 1-16");
            result.max_custom_packages = static_cast<std::size_t>(value);
        } else if (key == "MaxCakArchives") {
            if (value < 1 || value > 32)
                return invalid("MaxCakArchives is outside 1-32");
            result.max_cak_archives = static_cast<std::size_t>(value);
        } else if (key == "ReadyTimeoutMs") {
            if (value < 5000 || value > 300000)
                return invalid("ReadyTimeoutMs is outside 5000-300000");
            result.ready_timeout_ms = static_cast<std::uint32_t>(value);
        } else if (key == "CakPostPrimeDelayMs") {
            if (value > 10000)
                return invalid("CakPostPrimeDelayMs is outside 0-10000");
            result.cak_post_prime_delay_ms = static_cast<std::uint32_t>(value);
        } else
            return invalid("unknown configuration key");
    }
    return result;
}
RuntimeConfig load_runtime_config(const std::filesystem::path &game_dir) {
    const auto file = game_dir / L"DataCtrlLink.ini";
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(file, ec);
    if (ec || !std::filesystem::exists(status))
        return {};
    if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status))
        return invalid("DataCtrlLink.ini must be a regular non-link file");
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size == 0 || size > kMaxIniBytes)
        return invalid("DataCtrlLink.ini is empty or exceeds 16 KiB");
    std::ifstream input(file, std::ios::binary);
    if (!input)
        return invalid("could not open DataCtrlLink.ini");
    std::string text((std::istreambuf_iterator<char>(input)), {});
    if (text.size() != size)
        return invalid("short DataCtrlLink.ini read");
    return parse_runtime_config(text);
}
} // namespace secure_dcl