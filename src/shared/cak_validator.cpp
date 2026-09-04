#include "cak_validator.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace secure_dcl {
namespace {

// The catalog header is encoded with a stateful 32-bit transform. Validation
// first recovers a candidate header, then accepts it only when all redundant
// counts, sizes, offsets, and archive bounds agree.
constexpr std::uint32_t rotate_left(std::uint32_t value, unsigned shift) {
    return (value << (shift & 31)) | (value >> ((32 - shift) & 31));
}
constexpr std::uint32_t rotate_right(std::uint32_t value, unsigned shift) {
    return (value >> (shift & 31)) | (value << ((32 - shift) & 31));
}
constexpr std::uint32_t inverse_odd(std::uint32_t value) {
    std::uint32_t inverse = value;
    for (unsigned i = 0; i < 5; ++i)
        inverse *= 2u - value * inverse;
    return inverse;
}
constexpr std::uint32_t kInverse85 = inverse_odd(0x85EBCA6Bu);
constexpr std::uint32_t kInverseC2 = inverse_odd(0xC2B2AE35u);

std::uint32_t read_u32(const std::uint8_t *data) {
    return std::uint32_t(data[0]) | (std::uint32_t(data[1]) << 8) | (std::uint32_t(data[2]) << 16) |
           (std::uint32_t(data[3]) << 24);
}
void write_u32(std::uint8_t *data, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        data[i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint32_t undo_xor_right(std::uint32_t value, unsigned shift) {
    std::uint32_t result = value;
    for (unsigned width = shift; width < 32; width += shift)
        result ^= value >> width;
    return result;
}
std::uint32_t key_from_first_mask(std::uint32_t mask) {
    auto mixed = undo_xor_right(mask, 13) * kInverseC2;
    mixed = undo_xor_right(mixed, 16);
    mixed = rotate_right(mixed, 7);
    mixed *= kInverse85;
    return mixed ^ 0xA3C59AC3u;
}

std::array<std::uint8_t, 84> decode_header(const std::array<std::uint8_t, 84> &input,
                                           std::uint32_t initial_key) {
    std::array<std::uint8_t, 84> output{};
    std::uint32_t key = initial_key, counter_a = 0, first_state = 0, state_b = 0,
                  previous_first = 0;
    std::size_t cursor = 0;
    for (unsigned block = 0; block < 10; ++block) {
        key ^= state_b;
        if (first_state != 0) {
            state_b += 0xC2B2AE35u;
            key ^= first_state;
            key = rotate_left(key, 11) + 0x165667B1u;
        }
        auto mixed = (key ^ counter_a ^ 0xA3C59AC3u) * 0x85EBCA6Bu;
        mixed = rotate_left(mixed, 7);
        mixed ^= mixed >> 16;
        auto mask = mixed * 0xC2B2AE35u;
        mask ^= mask >> 13;
        const auto cipher_first = read_u32(input.data() + cursor);
        write_u32(output.data() + cursor, cipher_first ^ mask);
        cursor += 4;
        first_state = cipher_first ^ previous_first;
        if (key != 0) {
            first_state ^= key;
            first_state = rotate_left(first_state, 7) - 0x7A143595u;
            previous_first += 0x9E3779B9u;
        }
        mixed = ((counter_a - 0x61C88647u) ^ first_state ^ 0x1B873593u);
        counter_a += 0x3C6EF372u;
        mixed *= 0x85EBCA6Bu;
        mixed = rotate_left(mixed, 7);
        mixed ^= mixed >> 16;
        mask = mixed * 0xC2B2AE35u;
        mask ^= mask >> 13;
        const auto cipher_second = read_u32(input.data() + cursor);
        key = cipher_second;
        write_u32(output.data() + cursor, cipher_second ^ mask);
        cursor += 4;
    }
    for (unsigned tail = 0; tail < 4; ++tail) {
        const std::uint32_t absolute = 80 + tail;
        const auto index_mix = absolute * 0x27D4EB2Du + 0x7F4A7C15u;
        state_b ^= key;
        auto rotation = (first_state ^ key) & 0xfu;
        if (tail == 0)
            rotation = 11;
        first_state ^= state_b;
        first_state = rotate_left(first_state, rotation);
        first_state -= tail == 0 ? 0xE9A9984Fu : 0x7A143595u;
        auto mixed = first_state ^ index_mix;
        mixed ^= mixed >> 15;
        const auto mask = mixed * 0x85EBCA6Bu;
        const auto cipher = input[80 + tail];
        key = cipher;
        output[80 + tail] =
            static_cast<std::uint8_t>(cipher ^ (mask & 0xff) ^ ((mask >> 13) & 0xff));
        state_b = absolute * 0x9E3779B9u;
    }
    return output;
}

bool header_valid(const std::array<std::uint32_t, 21> &words, std::uint64_t archive_size) {
    if (words[0] > 2'000'000 || words[1] > 1'000'000)
        return false;
    const std::array<unsigned, 5> size_indexes{3, 6, 9, 12, 15};
    const std::array<unsigned, 5> offset_indexes{5, 8, 11, 14, 17};
    if (words[3] != std::uint64_t(words[1]) * 12 || words[6] != std::uint64_t(words[0]) * 12)
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (std::uint64_t(words[offset_indexes[i]]) + words[size_indexes[i]] !=
            words[offset_indexes[i + 1]])
            return false;
    return words[offset_indexes[0]] >= 92 &&
           std::uint64_t(words[offset_indexes[4]]) + words[size_indexes[4]] <= archive_size &&
           words[20] <= archive_size;
}

} // namespace

bool validate_cak(const std::filesystem::path &path, CakInfo &info, std::string &error) {
    // Archive files are untrusted input. Counts remain capped and arithmetic
    // is checked before indexing or allocating.
    info = {};
    error.clear();
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size < 92 || size > 8ull * 1024 * 1024 * 1024) {
        error = "invalid archive size";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    std::array<std::uint8_t, 92> prefix{};
    if (!input.read(reinterpret_cast<char *>(prefix.data()), prefix.size())) {
        error = "could not read CAK header";
        return false;
    }
    constexpr std::array<std::uint8_t, 8> expected{'F', 'D', 'I', 'R', 0x09, 0x09, 0x00, 0x81};
    if (!std::equal(expected.begin(), expected.end(), prefix.begin())) {
        error = "unexpected CAK magic/version";
        return false;
    }
    std::array<std::uint8_t, 84> encrypted{};
    std::memcpy(encrypted.data(), prefix.data() + 8, encrypted.size());
    const std::uint32_t cipher_first = read_u32(encrypted.data());
    for (std::uint32_t count = 0; count <= 2'000'000; ++count) {
        const auto key = key_from_first_mask(cipher_first ^ count);
        const auto decoded = decode_header(encrypted, key);
        std::array<std::uint32_t, 21> words{};
        for (unsigned i = 0; i < words.size(); ++i)
            words[i] = read_u32(decoded.data() + i * 4);
        if (words[0] == count && header_valid(words, size)) {
            info = {words[0], words[1], key};
            return true;
        }
    }
    error = "encrypted CAK catalog header failed structural validation";
    return false;
}

} // namespace secure_dcl
