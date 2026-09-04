#include "plugin_interface.hpp"
#include "logging.hpp"
#include "bank_builder.hpp"
#include "mod_manifest.hpp"
#include "runtime_config.hpp"
#include "profiles.hpp"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <iterator>
#include <algorithm>

namespace {

secure_dcl::PluginContext g_context;
secure_dcl::RuntimeConfig g_config;

using IsInitializedFn = bool (*)();
using LoadBankFn = std::int32_t (*)(const void *, std::uint32_t, std::uint32_t &, std::uint32_t &);
using UnloadBankFn = std::int32_t (*)(std::uint32_t, const void *, std::uint32_t);
using GetResolverFn = void *(*)();
using AddPackageFn = std::int32_t (*)(void *, const wchar_t *, std::uint32_t *);

struct CakPathView {
    const char *data;
    std::size_t length;
};

bool wait_until(const auto &predicate, DWORD timeout_ms) {
    const ULONGLONG deadline = GetTickCount64() + timeout_ms;
    while (GetTickCount64() < deadline) {
        if (predicate()) return true;
        Sleep(250);
    }
    return predicate();
}

std::vector<std::byte> read_file(const std::filesystem::path &path, std::string &error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > g_config.max_file_bytes) {
        error = "file missing or exceeds size limit";
        return {};
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "could not open file";
        return {};
    }
    std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
    std::vector<std::byte> result(chars.size());
    std::transform(chars.begin(), chars.end(), result.begin(),
                   [](char value) { return std::byte(static_cast<unsigned char>(value)); });
    return result;
}

bool looks_like_akpk(const std::filesystem::path &path) {
    std::string error;
    auto bytes = read_file(path, error);
    return error.empty() && secure_dcl::validate_akpk(bytes, error);
}

bool regular_non_reparse(const std::filesystem::path &file) {
    const DWORD attrs = GetFileAttributesW(file.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
}

std::optional<std::filesystem::path> utf8_path(const std::string &value) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) return std::nullopt;
    std::wstring wide(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), length);
    return std::filesystem::path(wide);
}

} // namespace

extern "C" __declspec(dllexport) bool PluginInit(const secure_dcl::PluginContext& context) {
    g_context = context;
    g_config = secure_dcl::load_runtime_config(context.game_dir);
    
    HMODULE game = context.game_module;
    auto initialized = reinterpret_cast<IsInitializedFn>(GetProcAddress(game, MAKEINTRESOURCEA(129)));
    auto load_bank = reinterpret_cast<LoadBankFn>(GetProcAddress(game, MAKEINTRESOURCEA(137)));
    auto unload_bank = reinterpret_cast<UnloadBankFn>(GetProcAddress(game, MAKEINTRESOURCEA(327)));
    
    if (!initialized || !load_bank || !unload_bank) {
        g_context.log("ERROR: [MusicLoader] required Wwise exports missing");
        return false;
    }
    
    CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        const auto manifest = secure_dcl::load_mod_manifest(g_context.game_dir);
        
        // Register packages logic
        auto stream_global = reinterpret_cast<void **>(GetProcAddress(g_context.game_module, MAKEINTRESOURCEA(351)));
        auto get_resolver = reinterpret_cast<GetResolverFn>(GetProcAddress(g_context.game_module, MAKEINTRESOURCEA(73)));
        
        if (stream_global && get_resolver && wait_until([&] { return *stream_global != nullptr; }, g_config.ready_timeout_ms)) {
            void *resolver = get_resolver();
            if (resolver) {
                auto vtable = *reinterpret_cast<void ***>(resolver);
                auto add_package = reinterpret_cast<AddPackageFn>(vtable[4]);
                
                std::vector<std::filesystem::path> packages;
                if (manifest.present && manifest.valid) {
                    for (const auto &name : manifest.package_order) {
                        if (auto rel = utf8_path(name)) {
                            auto p = g_context.game_dir / L"sound" / *rel;
                            if (regular_non_reparse(p) && looks_like_akpk(p)) packages.push_back(p);
                        }
                    }
                }
                
                for (const auto &package : packages) {
                    std::uint32_t id = 0;
                    add_package(resolver, package.native().c_str(), &id);
                    g_context.log("Registered custom package: " + package.filename().string());
                }
            }
        }
        
        // Music bank logic
        auto initialized = reinterpret_cast<IsInitializedFn>(GetProcAddress(g_context.game_module, MAKEINTRESOURCEA(129)));
        if (wait_until([&] { return initialized(); }, g_config.ready_timeout_ms)) {
            std::string error;
            auto package = read_file(g_context.game_dir / L"sound" / L"music.pck", error);
            if (error.empty()) {
                if (auto extracted = secure_dcl::extract_music_bank(package)) {
                    if (auto rebuilt = secure_dcl::rebuild_music_bank(extracted.bank)) {
                        std::uint32_t b_id = 0, b_type = 0;
                        auto load_bank = reinterpret_cast<LoadBankFn>(GetProcAddress(g_context.game_module, MAKEINTRESOURCEA(137)));
                        auto result = load_bank(rebuilt.bank.data(), static_cast<std::uint32_t>(rebuilt.bank.size()), b_id, b_type);
                        if (result == 1) g_context.log("Successfully loaded custom music bank");
                    }
                }
            }
        }
        
        return 0;
    }, nullptr, 0, nullptr);

    return true;
}
