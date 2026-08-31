#include "bank_builder.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {
std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::vector<char> chars((std::istreambuf_iterator<char>(in)), {});
    std::vector<std::byte> bytes(chars.size());
    for (std::size_t i = 0; i < chars.size(); ++i) bytes[i] = std::byte(static_cast<unsigned char>(chars[i]));
    return bytes;
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main(int argc, char** argv) try {
    if (argc != 4) {
        std::cerr << "usage: bank_builder_tests <music.pck> <stock.bnk> <golden.bnk>\n";
        return 2;
    }
    auto package = read_file(argv[1]);
    auto stock = read_file(argv[2]);
    auto golden = read_file(argv[3]);
    require(music_only::wwise_hash("Play_MUS_ID_3000") == 521188677u, "normal event hash mismatch");
    require(music_only::wwise_hash("Play_MUS_ID_3000_LOOP") == 1282648074u, "loop event hash mismatch");
    auto extracted = music_only::extract_music_bank(package);
    require(bool(extracted), "stock bank extraction failed");
    std::string validation_error;
    require(music_only::validate_akpk(package, validation_error), "valid package was rejected");
    require(extracted.bank == stock, "extracted bank differs from known stock bank");
    auto rebuilt = music_only::rebuild_music_bank(stock);
    require(bool(rebuilt), "bank rebuild failed");
    if (rebuilt.bank != golden) {
        std::size_t at = 0;
        while (at < rebuilt.bank.size() && at < golden.size() && rebuilt.bank[at] == golden[at]) ++at;
        std::cerr << "generated size=" << rebuilt.bank.size() << ", golden size=" << golden.size()
                  << ", first difference=" << at << '\n';
        require(false, "rebuilt bank differs from captured original output");
    }
    auto bad_package = package; bad_package[0] = std::byte{'X'};
    require(!music_only::extract_music_bank(bad_package), "invalid package was accepted");
    require(!music_only::validate_akpk(bad_package, validation_error), "invalid package validation passed");
    auto bad_bank = stock; bad_bank.resize(57);
    require(!music_only::rebuild_music_bank(bad_bank), "truncated bank was accepted");
    std::cout << "PASS: extraction, hashes, byte-perfect rebuild, and malformed-input rejection\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
