#include "addon_selection.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void write(const std::filesystem::path &path, const char *text) {
    std::ofstream output(path);
    output << text;
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "secure-dcl-addon-selection-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    auto absent = secure_dcl::load_addon_selection(root);
    require(!absent.present && absent.valid, "absent selection should use safe defaults");

    write(root / "addons.txt", "# enabled in this order\nAuroraForge.CustomMusicLoader.ftrib\nAuroraForge.CAKModLoader.ftrib\n");
    auto ordered = secure_dcl::load_addon_selection(root);
    require(ordered.present && ordered.valid && ordered.filenames.size() == 2,
            "valid ordered selection was rejected");
    require(ordered.filenames.front() == "AuroraForge.CustomMusicLoader.ftrib",
            "selection order was not preserved");

    write(root / "addons.txt", "unknown.ftrib\n");
    require(!secure_dcl::load_addon_selection(root).valid, "unknown addon was accepted");
    write(root / "addons.txt", "..\\evil.ftrib\n");
    require(!secure_dcl::load_addon_selection(root).valid, "addon path was accepted");
    write(root / "addons.txt", "AuroraForge.CAKModLoader.ftrib\nauroraforge.cakmodloader.ftrib\n");
    require(!secure_dcl::load_addon_selection(root).valid, "duplicate addon was accepted");

    std::filesystem::remove_all(root);
    std::cout << "Addon selection tests passed\n";
}

