#include "addon_registry.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    using secure_dcl::approved_addon_for_filename;
    require(approved_addon_for_filename("AuroraForge.CAKModLoader.ftrib") != nullptr,
            "CAK addon was not approved");
    require(approved_addon_for_filename("auroraforge.custommusicloader.ftrib") != nullptr,
            "case-insensitive Windows filename lookup failed");
    require(approved_addon_for_filename("mod_loader.ftrib") == nullptr,
            "legacy generic addon name was approved");
    require(approved_addon_for_filename("unknown.ftrib") == nullptr,
            "unknown addon was approved");
    require(approved_addon_for_filename("AuroraForge.CustomMusicLoader.ftrib.dll") == nullptr,
            "suffix-confusion addon was approved");
    std::cout << "Addon registry tests passed\n";
}

