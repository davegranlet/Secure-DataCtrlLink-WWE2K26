#pragma once

#include <array>
#include <string_view>

namespace secure_dcl {

struct ApprovedAddon {
    std::string_view product;
    std::string_view filename;
    std::string_view sha256;
};

inline constexpr std::array<ApprovedAddon, 3> kApprovedAddons{{
    {"Aurora Forge CAK Mod Loader", "AuroraForge.CAKModLoader.ftrib",
     "b4848480ab485b90e43e8d933981a10a6e25d201ef4ec4f2733ac9d3da2a6872"},
    {"Aurora Forge Custom Music Loader", "AuroraForge.CustomMusicLoader.ftrib",
     "5b38674c38964c0bf2c48dd72264dfc31b5d5109ab0ab6391e98dd23d0db02be"},
    {"Aurora Forge MyGM Starting Cash 50M Example",
     "AuroraForge.Example.MyGMStartingCash50M.ftrib",
     "9abdfb00838a2bcaf7a77201f244e1d6da7a0ad59b280b0a420b20ef2c5df440"},
}};

const ApprovedAddon *approved_addon_for_filename(std::string_view filename);

} // namespace secure_dcl
