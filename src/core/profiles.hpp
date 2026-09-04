#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace secure_dcl {

struct GameProfile {
    std::string name;
    std::string sha256;
    std::uintptr_t mount_rva;
    std::uintptr_t signature_rva;
    std::uintptr_t phase_rva;
};

const GameProfile* get_profile(const std::string& sha256);
bool is_stock_archive(const std::string& name);

} // namespace secure_dcl
