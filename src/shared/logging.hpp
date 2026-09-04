#pragma once
#include <string>
#include <filesystem>

namespace secure_dcl {

void log_to_file(const std::filesystem::path& path, const std::string& message);

} // namespace secure_dcl
