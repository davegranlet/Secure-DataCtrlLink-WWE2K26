#include "addon_registry.hpp"

#include <algorithm>
#include <cctype>

namespace secure_dcl {

namespace {

bool same_filename(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(),
                      [](unsigned char a, unsigned char b) {
                          return std::tolower(a) == std::tolower(b);
                      });
}

} // namespace

const ApprovedAddon *approved_addon_for_filename(std::string_view filename) {
    const auto found = std::find_if(kApprovedAddons.begin(), kApprovedAddons.end(),
                                    [filename](const ApprovedAddon &addon) {
                                        return same_filename(filename, addon.filename);
                                    });
    return found == kApprovedAddons.end() ? nullptr : &*found;
}

} // namespace secure_dcl

