#pragma once
#include <windows.h>
#include <string>
#include <filesystem>

namespace secure_dcl {

struct PluginContext {
    HMODULE game_module;
    std::filesystem::path game_dir;
    void (*log)(const std::string& message);
    const struct GameProfile* profile;
};

typedef bool (*PluginInitFn)(const PluginContext& context);

} // namespace secure_dcl
