#include "plugin_interface.hpp"
#include "logging.hpp"
#include "profiles.hpp"

#include <windows.h>
#include <psapi.h>
#include <vector>
#include <cstdint>

namespace {

secure_dcl::PluginContext g_context;

// The five normal new-campaign starting-budget choices:
// $3M, $4M, $5M, $6M, and $7M
// In hex (little-endian 32-bit):
// C0 C6 2D 00, 00 09 3D 00, 40 4B 4C 00, 80 8D 5B 00, C0 DF 6A 00
const std::uint8_t kTargetBudgets[] = {
    0xC0, 0xC6, 0x2D, 0x00, 
    0x00, 0x09, 0x3D, 0x00, 
    0x40, 0x4B, 0x4C, 0x00, 
    0x80, 0x8D, 0x5B, 0x00, 
    0xC0, 0xDF, 0x6A, 0x00
};

// Target value: $50,000,000
// In hex (little-endian 32-bit): 80 F0 FA 02
const std::uint8_t kNewBudgets[] = {
    0x80, 0xF0, 0xFA, 0x02,
    0x80, 0xF0, 0xFA, 0x02,
    0x80, 0xF0, 0xFA, 0x02,
    0x80, 0xF0, 0xFA, 0x02,
    0x80, 0xF0, 0xFA, 0x02
};

void apply_mygm_patch() {
    MODULEINFO mi{};
    if (!GetModuleInformation(GetCurrentProcess(), g_context.game_module, &mi, sizeof(mi))) {
        g_context.log("ERROR: [MyGMMod] Failed to get module information");
        return;
    }

    const std::uint8_t* base = reinterpret_cast<const std::uint8_t*>(mi.lpBaseOfDll);
    const std::size_t size = mi.SizeOfImage;

    // Scan for the budget vector in memory.
    // We scan the whole image, but it's likely in a data section.
    for (std::size_t i = 0; i < size - sizeof(kTargetBudgets); ++i) {
        if (std::memcmp(base + i, kTargetBudgets, sizeof(kTargetBudgets)) == 0) {
            void* target = const_cast<std::uint8_t*>(base + i);
            DWORD old_protect;
            if (VirtualProtect(target, sizeof(kNewBudgets), PAGE_READWRITE, &old_protect)) {
                std::memcpy(target, kNewBudgets, sizeof(kNewBudgets));
                VirtualProtect(target, sizeof(kNewBudgets), old_protect, &old_protect);
                g_context.log("Successfully patched MyGM budgets to $50,000,000 at RVA 0x" + 
                              std::to_string(i));
                return;
            }
        }
    }

    g_context.log("WARNING: [MyGMMod] Could not find MyGM budget vector in memory. "
                  "The game might not have loaded the relevant data yet, or the version is different.");
}

} // namespace

extern "C" __declspec(dllexport) bool PluginInit(const secure_dcl::PluginContext& context) {
    g_context = context;
    g_context.log("MyGM Mod: 5 for 50 million initialized");
    
    // We run the patch in a separate thread to give the game time to load its data
    CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        // Wait a bit for the game to initialize its data structures
        Sleep(5000); 
        apply_mygm_patch();
        return 0;
    }, nullptr, 0, nullptr);
    
    return true;
}
