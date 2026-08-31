#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>

#include "bank_builder.hpp"
#include "cak_validator.hpp"
#include "status_message.hpp"
#include "MinHook.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

// COMPATIBILITY PROFILE
// These values are one indivisible, live-tested profile for a specific game
// executable. Never update only the hash or only an RVA: all signatures, ABIs,
// ordinals, timing, and in-game results must be revalidated together.
constexpr wchar_t kExpectedExe[] = L"WWE2K26_x64.exe";
constexpr char kExpectedExeSha256[] = "7f558607bd6881c4547d752ea107fb03282a2980e47ef19ade597e096de4a0e9";
constexpr DWORD kReadyTimeoutMs = 120'000;
constexpr DWORD kPollMs = 250;
constexpr std::size_t kMaxFileBytes = 64u * 1024u * 1024u;
constexpr std::size_t kMaxCustomPackages = 16;
constexpr std::size_t kMaxCakArchives = 32;
constexpr DWORD kCakPostPrimeDelayMs = 2'500;
constexpr std::uintptr_t kCakMountRva = 0x2811A91;
constexpr std::uintptr_t kCakSignatureRva = 0x2811BFC;
constexpr std::uintptr_t kCakPhaseRva = 0x27E0E50;

HMODULE g_self = nullptr;
HMODULE g_real = nullptr;
INIT_ONCE g_real_once = INIT_ONCE_STATIC_INIT;

using DirectInput8CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
using DllCanUnloadNowFn = HRESULT(WINAPI*)();
using DllGetClassObjectFn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
using SimpleDllFn = HRESULT(WINAPI*)();
using GetJoystickFn = const void*(WINAPI*)();
using IsInitializedFn = bool(*)();
using LoadBankFn = std::int32_t(*)(const void*, std::uint32_t, std::uint32_t&, std::uint32_t&);
using UnloadBankFn = std::int32_t(*)(std::uint32_t, const void*, std::uint32_t);
using GetResolverFn = void*(*)();
using AddPackageFn = std::int32_t(*)(void*, const wchar_t*, std::uint32_t*);
struct CakPathView {
    const char* data;
    std::size_t length;
};
using MountCakFn = bool(*)(const CakPathView*, void*, std::uint32_t, std::uint32_t);
using CakPhaseFn = void(*)(void*, void*, std::uint32_t, void*);

using secure_dcl::CakMountSummary;
struct PreparedCak {
    std::string path;
    std::string name;
    music_only::CakInfo info;
};

std::vector<PreparedCak> g_prepared_caks;
CakMountSummary g_cak_summary;
MountCakFn g_cak_mount = nullptr;
CakPhaseFn g_original_cak_phase = nullptr;
HANDLE g_cak_complete = nullptr;
volatile LONG g_cak_triggered = 0;
volatile LONG g_cak_detour_active = 0;

// LOGGING AND FAIL-CLOSED HOST VERIFICATION
// The full executable digest prevents version-specific calls from running in
// an unknown update. DirectInput forwarding remains independent of this gate.
std::filesystem::path module_directory() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(g_self, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length == buffer.size()) return {};
    return std::filesystem::path(buffer.data(), buffer.data() + length).parent_path();
}

void log_line(const std::string& message) {
    const auto path = module_directory() / L"DataCtrlLink-MusicOnly.log";
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME now{}; GetLocalTime(&now);
    std::ostringstream line;
    line << std::setfill('0') << '[' << now.wYear << '-' << std::setw(2) << now.wMonth << '-'
         << std::setw(2) << now.wDay << ' ' << std::setw(2) << now.wHour << ':' << std::setw(2)
         << now.wMinute << ':' << std::setw(2) << now.wSecond << "] " << message << "\r\n";
    const auto text = line.str(); DWORD written = 0;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(file);
}

std::vector<std::byte> read_file(const std::filesystem::path& path, std::string& error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaxFileBytes) { error = "file missing or exceeds size limit"; return {}; }
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = "could not open file"; return {}; }
    std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
    if (chars.size() != size) { error = "short file read"; return {}; }
    std::vector<std::byte> result(chars.size());
    std::transform(chars.begin(), chars.end(), result.begin(),
                   [](char value) { return std::byte(static_cast<unsigned char>(value)); });
    return result;
}

std::string sha256_file(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0, hash_size = 0, returned = 0;
    std::vector<UCHAR> object, digest;
    std::ifstream input(path, std::ios::binary);
    if (!input || BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    auto cleanup = [&] { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); };
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &returned, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size), &returned, 0) < 0) {
        cleanup(); return {};
    }
    object.resize(object_size); digest.resize(hash_size);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) { cleanup(); return {}; }
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size()); const auto count = input.gcount();
        if (count > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(count), 0) < 0) {
            cleanup(); return {};
        }
    }
    if (BCryptFinishHash(hash, digest.data(), hash_size, 0) < 0) { cleanup(); return {}; }
    cleanup(); std::ostringstream text; text << std::hex << std::setfill('0');
    for (UCHAR byte : digest) text << std::setw(2) << static_cast<unsigned>(byte);
    return text.str();
}

bool supported_game(std::filesystem::path& game_dir) {
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length == path.size()) { log_line("ERROR: cannot resolve game executable path"); return false; }
    const std::filesystem::path exe(path.data(), path.data() + length);
    if (_wcsicmp(exe.filename().c_str(), kExpectedExe) != 0) { log_line("ERROR: host executable is not WWE2K26_x64.exe"); return false; }
    const auto actual = sha256_file(exe);
    if (actual != kExpectedExeSha256) { log_line("ERROR: unsupported game build; executable SHA-256 is " + actual); return false; }
    game_dir = exe.parent_path(); return true;
}

bool wait_until(const auto& predicate, DWORD timeout_ms) {
    const ULONGLONG deadline = GetTickCount64() + timeout_ms;
    while (GetTickCount64() < deadline) { if (predicate()) return true; Sleep(kPollMs); }
    return predicate();
}

// RESTRICTED CUSTOM-PACKAGE DISCOVERY
// Only bounded, regular Custom*.pck files in the fixed sound directory reach
// the AKPK validator. Reparse points and arbitrary plugin extensions do not.
bool looks_like_akpk(const std::filesystem::path& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return false;
    std::error_code ec; const auto size = std::filesystem::file_size(path, ec);
    if (ec || size < 24 || size > kMaxFileBytes) return false;
    std::string error; auto bytes = read_file(path, error);
    return error.empty() && music_only::validate_akpk(bytes, error);
}

std::vector<std::filesystem::path> custom_packages(const std::filesystem::path& game_dir) {
    std::vector<std::filesystem::path> result;
    const auto pattern = game_dir / L"sound" / L"Custom*.pck";
    WIN32_FIND_DATAW found{}; HANDLE search = FindFirstFileW(pattern.c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) return result;
    do {
        const auto path = game_dir / L"sound" / found.cFileName;
        if (looks_like_akpk(path)) result.push_back(path);
    } while (result.size() < kMaxCustomPackages && FindNextFileW(search, &found));
    FindClose(search);
    std::sort(result.begin(), result.end()); return result;
}

void register_packages(HMODULE game, const std::filesystem::path& game_dir) {
    auto stream_global = reinterpret_cast<void**>(GetProcAddress(game, MAKEINTRESOURCEA(351)));
    auto get_resolver = reinterpret_cast<GetResolverFn>(GetProcAddress(game, MAKEINTRESOURCEA(73)));
    if (!stream_global || !get_resolver) { log_line("ERROR: package-registration exports missing"); return; }
    if (!wait_until([&] { return *stream_global != nullptr; }, kReadyTimeoutMs)) {
        log_line("ERROR: stream manager did not become ready"); return;
    }
    void* resolver = get_resolver();
    if (!resolver) { log_line("ERROR: file-location resolver is null"); return; }
    auto vtable = *reinterpret_cast<void***>(resolver);
    if (!vtable || !vtable[4]) { log_line("ERROR: resolver AddFilePackage slot missing"); return; }
    auto add_package = reinterpret_cast<AddPackageFn>(vtable[4]);
    const auto packages = custom_packages(game_dir);
    if (packages.empty()) { log_line("WARNING: no validated sound\\Custom*.pck packages found"); return; }
    for (const auto& package : packages) {
        std::uint32_t package_id = 0;
        const auto absolute = package.native();
        const auto result = add_package(resolver, absolute.c_str(), &package_id);
        std::ostringstream line; line << (result == 1 ? "Registered " : "ERROR registering ")
                                     << package.filename().string() << " (result=" << result << ", id=" << package_id << ')';
        log_line(line.str());
    }
}

// ONE-SHOT NATIVE ARCHIVE-PHASE TRIGGER
// The detour observes the supported game's stock phase and immediately wakes
// the worker. Expensive validation/mount work is never performed in the hook.
void cak_phase_detour(void* first, void* second, std::uint32_t third, void* fourth) {
    g_original_cak_phase(first, second, third, fourth);
    if (InterlockedCompareExchange(&g_cak_triggered, 1, 0) != 0) return;

    InterlockedExchange(&g_cak_detour_active, 1);
    log_line("Stock archive phase returned; releasing the game thread before addon mounting");
    InterlockedExchange(&g_cak_detour_active, 0);
    if (g_cak_complete) SetEvent(g_cak_complete);
}

CakMountSummary mount_cak_archives(HMODULE game, const std::filesystem::path& game_dir) {
    g_cak_summary = {};
    g_prepared_caks.clear();
    g_cak_triggered = 0;
    g_cak_detour_active = 0;
    constexpr std::array<std::uint8_t, 12> signature{
        0x40,0x46,0x74,0x3e,0x72,0xff,0x48,0x85,0xc0,0x48,0x0f,0x44
    };
    constexpr std::array<std::uint8_t, 16> phase_signature{
        0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,
        0x24,0x18,0x56,0x57,0x41,0x55,0x41,0x56
    };
    const auto base = reinterpret_cast<std::uintptr_t>(game);
    if (std::memcmp(reinterpret_cast<const void*>(base + kCakSignatureRva), signature.data(), signature.size()) != 0) {
        log_line("ERROR: CAK mount signature does not match supported game build"); g_cak_summary.failed = 1; return g_cak_summary;
    }
    if (std::memcmp(reinterpret_cast<const void*>(base + kCakPhaseRva), phase_signature.data(), phase_signature.size()) != 0) {
        log_line("ERROR: archive-phase signature does not match supported game build"); g_cak_summary.failed = 1; return g_cak_summary;
    }
    g_cak_mount = reinterpret_cast<MountCakFn>(base + kCakMountRva);
    auto target = reinterpret_cast<void*>(base + kCakPhaseRva);
    const auto pattern = game_dir / L"mods" / L"*.cak";
    WIN32_FIND_DATAW found{}; HANDLE search = FindFirstFileW(pattern.c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) { log_line("WARNING: no mods\\*.cak archives found"); return g_cak_summary; }
    std::vector<std::filesystem::path> archives;
    do {
        if ((found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0)
            archives.push_back(game_dir / L"mods" / found.cFileName);
    } while (archives.size() < kMaxCakArchives && FindNextFileW(search, &found));
    FindClose(search);
    for (const auto& archive : archives) {
        music_only::CakInfo info; std::string error;
        if (!music_only::validate_cak(archive, info, error)) {
            ++g_cak_summary.rejected; log_line("REJECTED CAK " + archive.filename().string() + ": " + error); continue;
        }
        auto path = archive.string(); std::replace(path.begin(), path.end(), '\\', '/');
        g_prepared_caks.push_back({std::move(path), archive.filename().string(), info});
    }
    if (g_prepared_caks.empty()) return g_cak_summary;
    g_cak_complete = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_cak_complete) { g_cak_summary.failed += g_prepared_caks.size(); return g_cak_summary; }
    const auto initialize = MH_Initialize();
    const auto create = initialize == MH_OK || initialize == MH_ERROR_ALREADY_INITIALIZED
        ? MH_CreateHook(target, reinterpret_cast<void*>(cak_phase_detour), reinterpret_cast<void**>(&g_original_cak_phase))
        : initialize;
    const auto enable = create == MH_OK ? MH_EnableHook(target) : create;
    bool stock_phase_returned = false;
    if (enable != MH_OK) {
        log_line("ERROR: could not install the one-shot CAK mount hook");
        g_cak_summary.failed += g_prepared_caks.size();
    } else {
        log_line("Waiting for the game's stock archive phase");
        if (WaitForSingleObject(g_cak_complete, 30'000) != WAIT_OBJECT_0) {
            log_line("ERROR: timed out waiting for the game's archive-mount thread");
            g_cak_summary.failed += g_prepared_caks.size();
        } else {
            stock_phase_returned = true;
        }
        while (InterlockedCompareExchange(&g_cak_detour_active, 0, 0) != 0) Sleep(1);
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    CloseHandle(g_cak_complete);
    g_cak_complete = nullptr;
    MH_Uninitialize();

    if (!stock_phase_returned) return g_cak_summary;

    // The original addon performs these fixed stock mounts on its own runtime
    // thread before adding mods. Besides reproducing ordering, this initializes
    // the game's archive state for the calling thread.
    log_line("Game thread released; reproducing the recovered fixed archive-prime sequence on the addon thread");
    constexpr std::array<const char*, 4> stock_archives{
        "bakedfile00.cak", "bakedfile01.cak", "bakedfile02.cak", "bakedfile03.cak"
    };
    for (std::uint32_t slot = 0; slot < stock_archives.size(); ++slot) {
        const std::string_view path(stock_archives[slot]);
        const CakPathView path_view{path.data(), path.size()};
        const bool accepted = g_cak_mount(&path_view, nullptr, 0, slot);
        std::ostringstream line;
        line << (accepted ? "Primed stock CAK " : "ERROR priming stock CAK ") << path
             << " (slot=" << slot << ')';
        log_line(line.str());
        if (!accepted) {
            g_cak_summary.failed += g_prepared_caks.size();
            return g_cak_summary;
        }
    }

    log_line("Fixed stock CAK prime completed; waiting for the recovered mod-mount window");
    Sleep(kCakPostPrimeDelayMs);
    for (const auto& archive : g_prepared_caks) {
        const CakPathView path_view{archive.path.c_str(), archive.path.size()};
        log_line("Mounting validated CAK " + archive.name);
        const bool accepted = g_cak_mount(&path_view, nullptr, 2, 1);
        accepted ? ++g_cak_summary.accepted : ++g_cak_summary.failed;
        std::ostringstream line; line << (accepted ? "Mount call accepted for " : "ERROR mounting ") << archive.name
            << " (files=" << archive.info.file_count << ", folders=" << archive.info.folder_count << ')';
        log_line(line.str());
    }
    // The recovered loader retains accepted path strings in process-lifetime state.
    // Keep this vector intact in case the game resolves an archive path lazily.
    return g_cak_summary;
}

// USER-VISIBLE STATUS IS QUEUED OFF THE GAME'S ARCHIVE THREAD.
void show_cak_notification(const CakMountSummary& summary) {
    const auto text = secure_dcl::cak_status_message(summary);
    MessageBoxW(nullptr, text.c_str(), L"Secure DataCtrlLink", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

DWORD WINAPI cak_notification_worker(void* raw_summary) {
    auto* summary = static_cast<CakMountSummary*>(raw_summary);
    show_cak_notification(*summary); delete summary; return 0;
}

void queue_cak_notification(const CakMountSummary& summary) {
    auto* copy = new (std::nothrow) CakMountSummary(summary);
    if (!copy) return;
    HANDLE thread = CreateThread(nullptr, 0, cak_notification_worker, copy, 0, nullptr);
    if (thread) CloseHandle(thread); else delete copy;
}

DWORD music_worker_impl() {
    log_line("Starting custom-music-only runtime");
    std::filesystem::path game_dir;
    if (!supported_game(game_dir)) return 0;
    HMODULE game = GetModuleHandleW(nullptr);
    auto initialized = reinterpret_cast<IsInitializedFn>(GetProcAddress(game, MAKEINTRESOURCEA(129)));
    auto load_bank = reinterpret_cast<LoadBankFn>(GetProcAddress(game, MAKEINTRESOURCEA(137)));
    auto unload_bank = reinterpret_cast<UnloadBankFn>(GetProcAddress(game, MAKEINTRESOURCEA(327)));
    if (!initialized || !load_bank || !unload_bank) { log_line("ERROR: required Wwise exports missing"); return 0; }
    const auto cak_summary = mount_cak_archives(game, game_dir);
    queue_cak_notification(cak_summary);
    register_packages(game, game_dir);
    if (!wait_until([&] { return initialized(); }, kReadyTimeoutMs)) { log_line("ERROR: Wwise did not initialize"); return 0; }
    std::string error;
    auto package = read_file(game_dir / L"sound" / L"music.pck", error);
    if (!error.empty()) { log_line("ERROR reading music.pck: " + error); return 0; }
    auto extracted = music_only::extract_music_bank(package);
    if (!extracted) { log_line("ERROR extracting music bank: " + extracted.error); return 0; }
    auto rebuilt = music_only::rebuild_music_bank(extracted.bank);
    if (!rebuilt) { log_line("ERROR rebuilding music bank: " + rebuilt.error); return 0; }
    std::uint32_t bank_id = 0, bank_type = 0;
    auto result = load_bank(rebuilt.bank.data(), static_cast<std::uint32_t>(rebuilt.bank.size()), bank_id, bank_type);
    if (result == 0x45) {
        log_line("Existing music bank detected; unloading and retrying once");
        unload_bank(bank_id, nullptr, bank_type); bank_id = 0; bank_type = 0;
        result = load_bank(rebuilt.bank.data(), static_cast<std::uint32_t>(rebuilt.bank.size()), bank_id, bank_type);
    }
    std::ostringstream line; line << (result == 1 ? "SUCCESS" : "ERROR") << ": LoadBankMemoryCopy result="
                                 << result << ", bank=0x" << std::hex << bank_id << ", bytes=" << std::dec << rebuilt.bank.size();
    log_line(line.str()); return 0;
}

DWORD WINAPI music_worker(void*) {
    try { return music_worker_impl(); }
    catch (const std::exception& error) { log_line(std::string("ERROR: unexpected exception: ") + error.what()); }
    catch (...) { log_line("ERROR: unexpected non-standard exception"); }
    return 0;
}

BOOL CALLBACK load_real(PINIT_ONCE, PVOID, PVOID*) {
    // Resolve the Microsoft library by absolute System32 path. Using the normal
    // DLL search path here would allow accidental proxy chaining or hijacking.
    std::array<wchar_t, MAX_PATH> system{};
    const UINT length = GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
    if (!length || length >= system.size() - 13) return TRUE;
    std::wstring path(system.data(), length); path += L"\\dinput8.dll";
    g_real = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return TRUE;
}

FARPROC real_export(const char* name) {
    InitOnceExecuteOnce(&g_real_once, load_real, nullptr, nullptr);
    return g_real ? GetProcAddress(g_real, name) : nullptr;
}

}  // namespace

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, REFIID iid, LPVOID* output, LPUNKNOWN outer) {
    auto function = reinterpret_cast<DirectInput8CreateFn>(real_export("DirectInput8Create"));
    return function ? function(instance, version, iid, output, outer) : E_FAIL;
}
extern "C" HRESULT WINAPI DllCanUnloadNow() {
    auto function = reinterpret_cast<DllCanUnloadNowFn>(real_export("DllCanUnloadNow")); return function ? function() : S_FALSE;
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* output) {
    auto function = reinterpret_cast<DllGetClassObjectFn>(real_export("DllGetClassObject"));
    return function ? function(clsid, iid, output) : CLASS_E_CLASSNOTAVAILABLE;
}
extern "C" HRESULT WINAPI DllRegisterServer() {
    auto function = reinterpret_cast<SimpleDllFn>(real_export("DllRegisterServer")); return function ? function() : E_FAIL;
}
extern "C" HRESULT WINAPI DllUnregisterServer() {
    auto function = reinterpret_cast<SimpleDllFn>(real_export("DllUnregisterServer")); return function ? function() : E_FAIL;
}
extern "C" const void* WINAPI GetdfDIJoystick() {
    auto function = reinterpret_cast<GetJoystickFn>(real_export("GetdfDIJoystick")); return function ? function() : nullptr;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    // Keep DllMain loader-safe: store the handle, disable thread callbacks, and
    // hand all filesystem, hashing, waiting, hook, and Wwise work to a worker.
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = instance;
        DisableThreadLibraryCalls(instance);
        // Thread entry is serialized behind DLL initialization; no work or waiting occurs here.
        HANDLE thread = CreateThread(nullptr, 0, music_worker, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
