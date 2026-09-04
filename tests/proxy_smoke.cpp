#define WIN32_LEAN_AND_MEAN
#include <objbase.h>
#include <windows.h>

#include <array>
#include <iostream>

int wmain(int argc, wchar_t **argv) {
    if (argc != 2) {
        std::cerr << "usage: proxy_smoke <dinput8.dll>\n";
        return 2;
    }
    HMODULE proxy = LoadLibraryW(argv[1]);
    if (!proxy) {
        std::cerr << "LoadLibrary failed: " << GetLastError() << '\n';
        return 1;
    }
    constexpr std::array<const char *, 6> names = {"DirectInput8Create",  "DllCanUnloadNow",
                                                   "DllGetClassObject",   "DllRegisterServer",
                                                   "DllUnregisterServer", "GetdfDIJoystick"};
    for (const char *name : names) {
        if (!GetProcAddress(proxy, name)) {
            std::cerr << "missing export: " << name << '\n';
            return 1;
        }
    }
    using GetJoystickFn = const void *(WINAPI *)();
    auto get_joystick = reinterpret_cast<GetJoystickFn>(GetProcAddress(proxy, "GetdfDIJoystick"));
    if (!get_joystick()) {
        std::cerr << "system forwarding returned null joystick format\n";
        return 1;
    }
    using CreateFn = HRESULT(WINAPI *)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
    auto create = reinterpret_cast<CreateFn>(GetProcAddress(proxy, "DirectInput8Create"));
    void *output = nullptr;
    const GUID null_guid{};
    (void)create(GetModuleHandleW(nullptr), 0, null_guid, &output, nullptr);
    Sleep(250);
    FreeLibrary(proxy);
    std::cout << "PASS: proxy loaded, all six exports resolved, and System32 forwarding executed\n";
    return 0;
}
