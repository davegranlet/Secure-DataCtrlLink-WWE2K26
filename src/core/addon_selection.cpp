#include "addon_selection.hpp"

#include "addon_registry.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

namespace secure_dcl {

namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char c) { return std::isspace(c); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char c) { return std::isspace(c); })
                          .base();
    return first < last ? std::string(first, last) : std::string{};
}

std::string fold(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

} // namespace

AddonSelection load_addon_selection(const std::filesystem::path &plugins_directory) {
    AddonSelection result;
    const auto manifest = plugins_directory / L"addons.txt";
    if (!std::filesystem::exists(manifest)) return result;
    result.present = true;

    const auto attributes = GetFileAttributesW(manifest.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        result.valid = false;
        result.error = "addons.txt must be a regular non-reparse file";
        return result;
    }

    std::ifstream input(manifest);
    if (!input) {
        result.valid = false;
        result.error = "addons.txt could not be opened";
        return result;
    }

    std::set<std::string> seen;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line_number > 64) {
            result.valid = false;
            result.error = "addons.txt exceeds the 64-line safety limit";
            return result;
        }
        line = trim(std::move(line));
        if (line.empty() || line.starts_with('#') || line.starts_with(';')) continue;
        if (line.find('/') != std::string::npos || line.find('\\') != std::string::npos ||
            line == "." || line == "..") {
            result.valid = false;
            result.error = "addons.txt line " + std::to_string(line_number) +
                           " must contain one filename, not a path";
            return result;
        }
        const auto *approved = approved_addon_for_filename(line);
        if (!approved) {
            result.valid = false;
            result.error = "addons.txt line " + std::to_string(line_number) +
                           " names an addon not approved by this DataCtrlLink release";
            return result;
        }
        const auto key = fold(std::string(approved->filename));
        if (!seen.insert(key).second) {
            result.valid = false;
            result.error = "addons.txt line " + std::to_string(line_number) +
                           " duplicates an addon";
            return result;
        }
        result.filenames.emplace_back(approved->filename);
    }
    return result;
}

} // namespace secure_dcl

