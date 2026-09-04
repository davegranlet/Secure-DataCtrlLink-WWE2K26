#include "plugin_interface.hpp"
#include "logging.hpp"
#include "cak_validator.hpp"
#include "mod_manifest.hpp"
#include "runtime_config.hpp"
#include "status_message.hpp"
#include "profiles.hpp"
#include <MinHook.h>

#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

secure_dcl::PluginContext g_context;
secure_dcl::RuntimeConfig g_config;
secure_dcl::CakMountSummary g_cak_summary;

struct CakPathView {
    const char *data;
    std::size_t length;
};
using MountCakFn = bool (*)(const CakPathView *, void *, std::uint32_t, std::uint32_t);
using CakPhaseFn = void (*)(void *, void *, std::uint32_t, void *);

struct PreparedCak {
    std::string path;
    std::string name;
    secure_dcl::CakInfo info;
};

std::vector<PreparedCak> g_prepared_caks;
MountCakFn g_cak_mount = nullptr;
CakPhaseFn g_original_cak_phase = nullptr;
HANDLE g_cak_complete = nullptr;
volatile LONG g_cak_triggered = 0;
volatile LONG g_cak_detour_active = 0;

void cak_phase_detour(void *first, void *second, std::uint32_t third, void *fourth) {
    g_original_cak_phase(first, second, third, fourth);
    if (InterlockedCompareExchange(&g_cak_triggered, 1, 0) != 0)
        return;

    InterlockedExchange(&g_cak_detour_active, 1);
    g_context.log("Stock archive phase returned; releasing the game thread before addon mounting");
    InterlockedExchange(&g_cak_detour_active, 0);
    if (g_cak_complete)
        SetEvent(g_cak_complete);
}

void show_cak_notification(const secure_dcl::CakMountSummary &summary) {
    const auto text = secure_dcl::cak_status_message(summary);
    MessageBoxW(nullptr, text.c_str(), L"Secure DataCtrlLink - Mod Loader",
                MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

DWORD WINAPI cak_notification_worker(void *raw_summary) {
    auto *summary = static_cast<secure_dcl::CakMountSummary *>(raw_summary);
    show_cak_notification(*summary);
    delete summary;
    return 0;
}

void queue_cak_notification(const secure_dcl::CakMountSummary &summary) {
    auto *copy = new (std::nothrow) secure_dcl::CakMountSummary(summary);
    if (!copy)
        return;
    HANDLE thread = CreateThread(nullptr, 0, cak_notification_worker, copy, 0, nullptr);
    if (thread)
        CloseHandle(thread);
    else
        delete copy;
}

bool regular_non_reparse(const std::filesystem::path &file) {
    const DWORD attrs = GetFileAttributesW(file.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
}

std::optional<std::filesystem::path> utf8_path(const std::string &value) {
    if (value.empty() || value.size() > 255)
        return std::nullopt;
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
        return std::nullopt;
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), wide.data(), length) != length)
        return std::nullopt;
    return std::filesystem::path(std::move(wide));
}

} // namespace

extern "C" __declspec(dllexport) bool PluginInit(const secure_dcl::PluginContext& context) {
    g_context = context;
    g_config = secure_dcl::load_runtime_config(context.game_dir);
    
    const auto manifest = secure_dcl::load_mod_manifest(context.game_dir);
    
    g_cak_summary = {};
    g_prepared_caks.clear();
    g_cak_triggered = 0;
    g_cak_detour_active = 0;
    
    constexpr std::array<std::uint8_t, 12> signature{0x40, 0x46, 0x74, 0x3e, 0x72, 0xff,
                                                     0x48, 0x85, 0xc0, 0x48, 0x0f, 0x44};
    constexpr std::array<std::uint8_t, 16> phase_signature{0x48, 0x89, 0x5c, 0x24, 0x10, 0x48,
                                                           0x89, 0x6c, 0x24, 0x18, 0x56, 0x57,
                                                           0x41, 0x55, 0x41, 0x56};
                                                           
    const auto base = reinterpret_cast<std::uintptr_t>(context.game_module);
    if (std::memcmp(reinterpret_cast<const void *>(base + context.profile->signature_rva), signature.data(),
                    signature.size()) != 0) {
        g_context.log("ERROR: [ModLoader] CAK mount signature does not match supported game build");
        return false;
    }
    if (std::memcmp(reinterpret_cast<const void *>(base + context.profile->phase_rva), phase_signature.data(),
                    phase_signature.size()) != 0) {
        g_context.log("ERROR: [ModLoader] archive-phase signature does not match supported game build");
        return false;
    }
    
    g_cak_mount = reinterpret_cast<MountCakFn>(base + context.profile->mount_rva);
    auto target = reinterpret_cast<void *>(base + context.profile->phase_rva);
    
    std::vector<std::filesystem::path> archives;
    if (manifest.present) {
        if (!manifest.valid) {
            g_context.log("ERROR: [ModLoader] mod manifest invalid: " + manifest.error);
            return false;
        }
        for (const auto &name : manifest.cak_order) {
            const auto relative = utf8_path(name);
            if (!relative) {
                ++g_cak_summary.rejected;
                continue;
            }
            const auto archive = context.game_dir / L"mods" / *relative;
            if (!regular_non_reparse(archive)) {
                ++g_cak_summary.rejected;
                continue;
            }
            archives.push_back(archive);
        }
    } else {
        const auto pattern = context.game_dir / L"mods" / L"*.cak";
        WIN32_FIND_DATAW found{};
        HANDLE search = FindFirstFileW(pattern.c_str(), &found);
        if (search != INVALID_HANDLE_VALUE) {
            do {
                if ((found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0)
                    archives.push_back(context.game_dir / L"mods" / found.cFileName);
            } while (archives.size() < g_config.max_cak_archives && FindNextFileW(search, &found));
            FindClose(search);
            std::sort(archives.begin(), archives.end());
        }
    }
    
    for (const auto &archive : archives) {
        secure_dcl::CakInfo info;
        std::string error;
        if (!secure_dcl::validate_cak(archive, info, error)) {
            ++g_cak_summary.rejected;
            g_context.log("REJECTED CAK " + archive.filename().string() + ": " + error);
            continue;
        }
        auto path = archive.string();
        std::replace(path.begin(), path.end(), '\\', '/');
        g_prepared_caks.push_back({std::move(path), archive.filename().string(), info});
    }
    
    if (g_prepared_caks.empty()) {
        g_context.log("No mod CAKs prepared for mounting");
        return true;
    }
    
    g_cak_complete = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_cak_complete) return false;
    
    MH_Initialize();
    MH_CreateHook(target, reinterpret_cast<void *>(cak_phase_detour), reinterpret_cast<void **>(&g_original_cak_phase));
    MH_EnableHook(target);
    
    // The actual mounting happens on the game's thread, but we'll wait for the event here
    // in a separate thread if needed, or just let it run.
    // For simplicity in this refactor, we'll spawn a thread to handle the mounting sequence.
    CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        if (WaitForSingleObject(g_cak_complete, 30000) == WAIT_OBJECT_0) {
            while (InterlockedCompareExchange(&g_cak_detour_active, 0, 0) != 0) Sleep(1);
            
            auto target = reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(g_context.game_module) + g_context.profile->phase_rva);
            MH_DisableHook(target);
            MH_RemoveHook(target);
            
            std::vector<std::string> stock_archives;
            WIN32_FIND_DATAW found{};
            HANDLE search = FindFirstFileW((g_context.game_dir / L"bakedfile*.cak").c_str(), &found);
            if (search != INVALID_HANDLE_VALUE) {
                do {
                    if ((found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0) {
                        std::wstring wide(found.cFileName);
                        int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
                        if (len > 0) {
                            std::string utf8(len, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), len, nullptr, nullptr);
                            stock_archives.push_back(std::move(utf8));
                        }
                    }
                } while (FindNextFileW(search, &found));
                FindClose(search);
                std::sort(stock_archives.begin(), stock_archives.end(), [](const std::string& a, const std::string& b) {
                    return a.length() == b.length() ? a < b : a.length() < b.length();
                });
            }

            for (std::uint32_t slot = 0; slot < stock_archives.size(); ++slot) {
                const std::string_view path(stock_archives[slot]);
                const CakPathView path_view{path.data(), path.size()};
                g_cak_mount(&path_view, nullptr, 0, slot);
            }
            
            Sleep(g_config.cak_post_prime_delay_ms);
            for (const auto &archive : g_prepared_caks) {
                const CakPathView path_view{archive.path.c_str(), archive.path.size()};
                const bool accepted = g_cak_mount(&path_view, nullptr, 2, 1);
                accepted ? ++g_cak_summary.accepted : ++g_cak_summary.failed;
            }
            queue_cak_notification(g_cak_summary);
        }
        CloseHandle(g_cak_complete);
        g_cak_complete = nullptr;
        return 0;
    }, nullptr, 0, nullptr);

    return true;
}
