#include "logging.hpp"
#include <windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace secure_dcl {

void log_to_file(const std::filesystem::path& path, const std::string& message) {
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    std::ostringstream line;
    line << std::setfill('0') << '[' << now.wYear << '-' << std::setw(2) << now.wMonth << '-'
         << std::setw(2) << now.wDay << ' ' << std::setw(2) << now.wHour << ':' << std::setw(2)
         << now.wMinute << ':' << std::setw(2) << now.wSecond << "] " << message << "\r\n";
    const auto text = line.str();
    DWORD written = 0;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(file);
}

} // namespace secure_dcl
