#include "cak_validator.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
void require_rejected(const std::filesystem::path &path, const char *message) {
    secure_dcl::CakInfo info;
    std::string error;
    if (secure_dcl::validate_cak(path, info, error))
        throw std::runtime_error(message);
}
} // namespace

int wmain(int argc, wchar_t **argv) try {
    if (argc < 2) {
        std::cerr << "usage: cak_validator_tests <archive.cak>...\n";
        return 2;
    }
    for (int i = 1; i < argc; ++i) {
        secure_dcl::CakInfo info;
        std::string error;
        if (!secure_dcl::validate_cak(argv[i], info, error)) {
            std::cerr << "FAIL: " << error << '\n';
            return 1;
        }
        std::wcout << L"PASS: " << argv[i] << L" files=" << info.file_count << L" folders="
                   << info.folder_count << L"\n";
    }

    const auto temp_root = std::filesystem::temp_directory_path();
    const auto truncated = temp_root / L"secure-dcl-truncated.cak";
    const auto corrupt = temp_root / L"secure-dcl-corrupt.cak";
    {
        std::ofstream output(truncated, std::ios::binary | std::ios::trunc);
        constexpr std::array<char, 8> prefix{'F',  'D',  'I',  'R',
                                             0x09, 0x09, 0x00, static_cast<char>(0x81)};
        output.write(prefix.data(), prefix.size());
    }
    {
        std::ifstream input(argv[1], std::ios::binary);
        std::vector<char> prefix(92);
        if (!input.read(prefix.data(), static_cast<std::streamsize>(prefix.size())))
            throw std::runtime_error("could not construct corrupt-header fixture");
        prefix[8] ^= 0x5a;
        std::ofstream output(corrupt, std::ios::binary | std::ios::trunc);
        output.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));
    }
    require_rejected(truncated, "truncated CAK was accepted");
    require_rejected(corrupt, "corrupt encrypted CAK header was accepted");
    std::error_code ignored;
    std::filesystem::remove(truncated, ignored);
    std::filesystem::remove(corrupt, ignored);
    std::cout << "PASS: truncated and corrupt CAK rejection\n";
    return 0;
} catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
