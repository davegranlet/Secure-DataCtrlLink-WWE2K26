#define WIN32_LEAN_AND_MEAN
// Windows base types must be declared before the BCrypt header.
// clang-format off
#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>
// clang-format on

#include "shared/logging.hpp"
#include "shared/plugin_interface.hpp"
#include "shared/runtime_config.hpp"
#include "core/profiles.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace {

HMODULE g_self = nullptr;
HMODULE g_real = nullptr;
INIT_ONCE g_real_once = INIT_ONCE_STATIC_INIT;
secure_dcl::RuntimeConfig g_config;
const secure_dcl::GameProfile* g_current_profile = nullptr;

using DirectInput8CreateFn = HRESULT(WINAPI *)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

std::filesystem::path module_directory() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length =
        GetModuleFileNameW(g_self, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length == buffer.size())
        return {};
    return std::filesystem::path(buffer.data(), buffer.data() + length).parent_path();
}

void log_line(const std::string &message) {
    secure_dcl::log_to_file(module_directory() / L"DataCtrlLink.log", message);
}

BOOL CALLBACK load_real_dinput8(PINIT_ONCE, PVOID, PVOID *) {
    std::array<wchar_t, 32768> buffer{};
    const UINT length = GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (!length || length >= buffer.size())
        return FALSE;
    const auto path = std::filesystem::path(buffer.data(), buffer.data() + length) / L"dinput8.dll";
    g_real = LoadLibraryW(path.c_str());
    return g_real != nullptr;
}

std::string sha256_hex(const std::filesystem::path &path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        CloseHandle(file);
        return {};
    }
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        CloseHandle(file);
        return {};
    }
    std::array<std::byte, 65536> buffer{};
    DWORD bytes = 0;
    while (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes, nullptr) &&
           bytes > 0) {
        BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), bytes, 0);
    }
    std::array<std::uint8_t, 32> digest{};
    BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(file);
    std::string hex;
    hex.reserve(64);
    for (const auto byte : digest) {
        static constexpr char kDigits[] = "0123456789abcdef";
        hex.push_back(kDigits[(byte >> 4) & 0x0F]);
        hex.push_back(kDigits[byte & 0x0F]);
    }
    return hex;
}

void load_plugins(HMODULE game_module, const std::filesystem::path& game_dir) {
    const auto plugins_dir = game_dir / L"plugins";
    if (!std::filesystem::exists(plugins_dir)) return;

    secure_dcl::PluginContext context{
        game_module,
        game_dir,
        log_line,
        g_current_profile
    };

    for (const auto& entry : std::filesystem::directory_iterator(plugins_dir)) {
        if (entry.path().extension() == L".ftrib") {
            HMODULE plugin = LoadLibraryW(entry.path().c_str());
            if (plugin) {
                auto init = reinterpret_cast<secure_dcl::PluginInitFn>(GetProcAddress(plugin, "PluginInit"));
                if (init) {
                    if (init(context)) {
                        log_line("Loaded plugin: " + entry.path().filename().string());
                    } else {
                        log_line("Plugin initialization failed: " + entry.path().filename().string());
                    }
                } else {
                    log_line("Plugin missing PluginInit export: " + entry.path().filename().string());
                }
            } else {
                log_line("Failed to load plugin: " + entry.path().filename().string() + " Error: " + std::to_string(GetLastError()));
            }
        }
    }
}

DWORD WINAPI main_worker(LPVOID) {
    const auto game_dir = module_directory();
    g_config = secure_dcl::load_runtime_config(game_dir);

    log_line("Secure DataCtrlLink Orchestrator started");

    const auto game_path = game_dir / L"WWE2K26_x64.exe";
    if (!std::filesystem::exists(game_path)) {
        log_line("ERROR: Game executable not found at " + game_path.string());
        return 0;
    }

    const auto hash = sha256_hex(game_path);
    g_current_profile = secure_dcl::get_profile(hash);

    if (!g_current_profile) {
        log_line("ERROR: Unsupported game version (Hash: " + hash + ")");
        MessageBoxW(nullptr, L"Secure DataCtrlLink: Unsupported game version. Mod loading disabled.", 
                    L"Secure DataCtrlLink", MB_OK | MB_ICONERROR);
        return 0;
    }

    log_line("Identified game version: " + g_current_profile->name);

    HMODULE game_module = GetModuleHandleW(nullptr);
    load_plugins(game_module, game_dir);

    return 0;
}

} // namespace

extern "C" HRESULT WINAPI
DirectInput8Create(HINSTANCE instance, DWORD version, REFIID iid, LPVOID *output, LPUNKNOWN outer) {
    if (!InitOnceExecuteOnce(&g_real_once, load_real_dinput8, nullptr, nullptr))
        return E_FAIL;
    const auto create = reinterpret_cast<DirectInput8CreateFn>(GetProcAddress(g_real, "DirectInput8Create"));
    if (!create)
        return E_FAIL;
    return create(instance, version, iid, output, outer);
}

extern "C" HRESULT WINAPI DllCanUnloadNow() {
    return S_FALSE;
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID *output) {
    if (!InitOnceExecuteOnce(&g_real_once, load_real_dinput8, nullptr, nullptr))
        return E_FAIL;
    const auto get_class_object = reinterpret_cast<HRESULT(WINAPI *)(REFCLSID, REFIID, LPVOID *)>(
        GetProcAddress(g_real, "DllGetClassObject"));
    if (!get_class_object)
        return E_FAIL;
    return get_class_object(clsid, iid, output);
}

extern "C" HRESULT WINAPI DllRegisterServer() {
    return S_OK;
}

extern "C" HRESULT WINAPI DllUnregisterServer() {
    return S_OK;
}

extern "C" LPVOID WINAPI GetdfDIJoystick() {
    if (!InitOnceExecuteOnce(&g_real_once, load_real_dinput8, nullptr, nullptr))
        return nullptr;
    const auto get_df_di_joystick = reinterpret_cast<LPVOID(WINAPI *)()>(
        GetProcAddress(g_real, "GetdfDIJoystick"));
    if (!get_df_di_joystick)
        return nullptr;
    return get_df_di_joystick();
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, main_worker, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
