#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace secure_dcl {
struct ModManifest {
    bool present{};
    bool valid{};
    std::vector<std::string> cak_order;
    std::vector<std::string> package_order;
    std::string error;
};

ModManifest parse_mod_manifest(std::string_view json);
ModManifest load_mod_manifest(const std::filesystem::path &game_dir);
} // namespace secure_dcl
