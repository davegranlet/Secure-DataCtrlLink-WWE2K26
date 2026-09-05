#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace secure_dcl {

struct AddonSelection {
    bool present = false;
    bool valid = true;
    std::vector<std::string> filenames;
    std::string error;
};

AddonSelection load_addon_selection(const std::filesystem::path &plugins_directory);

} // namespace secure_dcl

