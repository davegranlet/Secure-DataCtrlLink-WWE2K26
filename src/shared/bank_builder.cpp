#include "bank_builder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <optional>

namespace secure_dcl {
namespace {

// Object identifiers used by the custom-music bank.
// The supported game bank expects one fixed bank ID and paired normal/looping
// objects for 2,500 custom slots. These ranges are part of the compatibility
// profile, not general-purpose Wwise authoring settings.
constexpr std::uint32_t kBankId = 0x8FD5129D;
constexpr std::uint32_t kCount = 2500;
constexpr std::uint32_t kSourceBase = 1'080'000'000;
constexpr std::uint32_t kNormalSoundBase = 1'081'000'000;
constexpr std::uint32_t kLoopSoundBase = 1'082'000'000;
constexpr std::uint32_t kNormalActionBase = 1'083'000'000;
constexpr std::uint32_t kLoopActionBase = 1'084'000'000;
constexpr std::size_t kMaxPackageSize = 64u * 1024u * 1024u;
constexpr std::size_t kMaxBankSize = 4u * 1024u * 1024u;

[[maybe_unused]] constexpr std::array<std::uint8_t, 112> kNormalSound = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x14, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xc8, 0xf1, 0x96,
    0x96, 0x86, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7b, 0xe2, 0x82,
    0x0c, 0x00, 0x01, 0x06, 0x00, 0x00, 0xb4, 0x42, 0x00, 0x00, 0x08, 0xe8, 0x25, 0x68, 0xbe, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x72, 0x9d, 0x4e, 0xc2, 0x00, 0x02, 0x22, 0x82, 0x6f, 0xcf,
    0x32, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xfa, 0x43, 0x00, 0x00, 0xfa, 0x43, 0x04, 0x00, 0x00, 0x00};
[[maybe_unused]] constexpr std::array<std::uint8_t, 117> kLoopSound = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x14, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xc8, 0xf1, 0x96,
    0x96, 0x4c, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7b, 0xe2, 0x82,
    0x0c, 0x00, 0x02, 0x06, 0x4a, 0x00, 0x00, 0xb4, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0xe8,
    0x25, 0x68, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x72, 0x9d, 0x4e, 0xc2, 0x00, 0x02, 0x22,
    0x97, 0xdb, 0x17, 0x39, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xfa, 0x43, 0x00, 0x00, 0xfa, 0x43, 0x04, 0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 22> kAction = {0, 0, 0,    0,    0x03, 0x04, 0,    0, 0, 0, 0,
                                                  0, 0, 0x04, 0x9d, 0x12, 0xd5, 0x8f, 0, 0, 0, 0};

constexpr std::uint8_t hex_digit(char value) {
    return value >= '0' && value <= '9'   ? static_cast<std::uint8_t>(value - '0')
           : value >= 'a' && value <= 'f' ? static_cast<std::uint8_t>(value - 'a' + 10)
                                          : 0;
}
template <std::size_t N> consteval auto hex_bytes(const char (&text)[N]) {
    static_assert((N - 1) % 2 == 0);
    std::array<std::uint8_t, (N - 1) / 2> result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] =
            static_cast<std::uint8_t>((hex_digit(text[i * 2]) << 4) | hex_digit(text[i * 2 + 1]));
    return result;
}
constexpr auto kNormalSoundExact =
    hex_bytes("00000000010014000200000000c8f19696863000000000000000000000007be2820c"
              "0001060000b442000008e82568be000000000000000000000000000000000000000000"
              "0000000100729d4ec2000222826fcf320002000000000000000000040000000000fa43"
              "0000fa4304000000");
constexpr auto kLoopSoundExact =
    hex_bytes("00000000010014000200000000c8f196964c4100000000000000000000007be2820c00"
              "02064a0000b44200000000000008e82568be0000000000000000000000000000000000"
              "000000000000000100729d4ec200022297db1739000200000000000000000004000000"
              "0000fa430000fa4304000000");

std::optional<std::uint32_t> read_u32(std::span<const std::byte> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4)
        return std::nullopt;
    const auto *p = reinterpret_cast<const std::uint8_t *>(data.data() + offset);
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}

void append_u32(std::vector<std::byte> &out, std::uint32_t value) {
    for (unsigned shift : {0u, 8u, 16u, 24u})
        out.push_back(std::byte((value >> shift) & 0xff));
}

void patch_u32(std::vector<std::byte> &out, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        out[offset + i] = std::byte((value >> (i * 8)) & 0xff);
}

void append_record(std::vector<std::byte> &out, std::uint8_t type,
                   std::span<const std::byte> payload) {
    out.push_back(std::byte(type));
    append_u32(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
}

template <std::size_t N>
std::vector<std::byte> make_patched(const std::array<std::uint8_t, N> &source,
                                    std::uint32_t object_id, std::uint32_t target_id,
                                    std::size_t target_offset) {
    std::vector<std::byte> result(N);
    std::memcpy(result.data(), source.data(), N);
    patch_u32(result, 0, object_id);
    patch_u32(result, target_offset, target_id);
    return result;
}

struct HircRecord {
    std::uint8_t type;
    std::span<const std::byte> payload;
};

bool checked_end(std::size_t offset, std::uint64_t size, std::size_t total, std::size_t &end) {
    if (size > total || offset > total - static_cast<std::size_t>(size))
        return false;
    end = offset + static_cast<std::size_t>(size);
    return true;
}

} // namespace

std::uint32_t wwise_hash(std::string value) {
    std::uint32_t result = 0x811C9DC5;
    for (unsigned char ch : value) {
        const auto lower = static_cast<std::uint8_t>(std::tolower(ch));
        result = (result * 0x01000193u) ^ lower;
    }
    return result;
}

bool validate_akpk(std::span<const std::byte> package, std::string &error) {
    // Package files are untrusted input, so every table and payload range is
    // validated before an entry is extracted or passed to the game resolver.
    error.clear();
    if (package.size() < 24 || package.size() > kMaxPackageSize ||
        std::memcmp(package.data(), "AKPK", 4) != 0) {
        error = "invalid AKPK package";
        return false;
    }
    auto header_size = read_u32(package, 4);
    auto lang_size = read_u32(package, 12);
    auto bank_size = read_u32(package, 16);
    auto sound_size = read_u32(package, 20);
    if (!header_size || !lang_size || !bank_size || !sound_size) {
        error = "truncated AKPK header";
        return false;
    }
    std::size_t cursor = 24;
    std::uint32_t external_size = 0;
    const std::uint64_t known = 16ull + *lang_size + *bank_size + *sound_size;
    if (known < *header_size) {
        auto value = read_u32(package, cursor);
        if (!value) {
            error = "truncated external-table size";
            return false;
        }
        external_size = *value;
        cursor += 4;
    }
    std::size_t end = 0;
    if (!checked_end(cursor, *lang_size, package.size(), end)) {
        error = "bad language table";
        return false;
    }
    cursor = end;
    auto parse_table = [&](std::uint32_t section_size) -> bool {
        if (!checked_end(cursor, section_size, package.size(), end))
            return false;
        if (section_size == 0) {
            cursor = end;
            return true;
        }
        auto count = read_u32(package, cursor);
        if (!count || *count > 100'000 || section_size < 4)
            return false;
        if (*count == 0) {
            cursor = end;
            return true;
        }
        const std::size_t payload_size = section_size - 4;
        if (payload_size % *count)
            return false;
        const std::size_t entry_size = payload_size / *count;
        if (entry_size != 20 && entry_size != 24)
            return false;
        std::size_t entry = cursor + 4;
        for (std::uint32_t i = 0; i < *count; ++i, entry += entry_size) {
            auto block = read_u32(package, entry + 4);
            auto size_lo = read_u32(package, entry + 8);
            auto size_hi =
                entry_size == 24 ? read_u32(package, entry + 12) : std::optional<std::uint32_t>(0);
            auto start = read_u32(package, entry + (entry_size == 24 ? 16 : 12));
            if (!block || !size_lo || !size_hi || !start)
                return false;
            const std::uint64_t file_size = *size_lo | (std::uint64_t(*size_hi) << 32);
            const std::uint64_t file_offset = *block ? std::uint64_t(*start) * *block : *start;
            if (file_offset > package.size() ||
                file_size > package.size() - static_cast<std::size_t>(file_offset))
                return false;
        }
        cursor = end;
        return true;
    };
    if (!parse_table(*bank_size) || !parse_table(*sound_size) || !parse_table(external_size)) {
        error = "invalid AKPK entry table";
        return false;
    }
    return true;
}

BuildResult extract_music_bank(std::span<const std::byte> package) {
    // Locate only the expected bank ID after the shared AKPK validator passes.
    std::string validation_error;
    if (!validate_akpk(package, validation_error))
        return {{}, std::move(validation_error)};
    auto header_size = read_u32(package, 4);
    auto lang_size = read_u32(package, 12);
    auto bank_size = read_u32(package, 16);
    auto sound_size = read_u32(package, 20);
    if (!header_size || !lang_size || !bank_size || !sound_size)
        return {{}, "truncated AKPK header"};
    std::size_t cursor = 24;
    const std::uint64_t known = 16ull + *lang_size + *bank_size + *sound_size;
    if (known < *header_size)
        cursor += 4;
    std::size_t end = 0;
    if (!checked_end(cursor, *lang_size, package.size(), end))
        return {{}, "bad language table"};
    cursor = end;
    if (*bank_size < 4 || !checked_end(cursor, *bank_size, package.size(), end))
        return {{}, "bad bank table"};
    auto count = read_u32(package, cursor);
    if (!count || *count == 0 || *count > 1024)
        return {{}, "invalid bank count"};
    const std::size_t table_payload = *bank_size - 4;
    if (table_payload % *count)
        return {{}, "invalid bank table size"};
    const std::size_t entry_size = table_payload / *count;
    if (entry_size != 20 && entry_size != 24)
        return {{}, "unsupported bank table layout"};
    cursor += 4;
    for (std::uint32_t i = 0; i < *count; ++i, cursor += entry_size) {
        auto id = read_u32(package, cursor);
        auto block = read_u32(package, cursor + 4);
        if (!id || !block)
            return {{}, "truncated bank entry"};
        const std::uint64_t size = entry_size == 24
                                       ? std::uint64_t(*read_u32(package, cursor + 8)) |
                                             (std::uint64_t(*read_u32(package, cursor + 12)) << 32)
                                       : *read_u32(package, cursor + 8);
        auto start = read_u32(package, cursor + (entry_size == 24 ? 16 : 12));
        if (!start || size > kMaxBankSize)
            return {{}, "invalid bank entry"};
        const std::uint64_t offset = *block ? std::uint64_t(*start) * *block : *start;
        if (*id == kBankId) {
            if (offset > package.size() || size > package.size() - static_cast<std::size_t>(offset))
                return {{}, "music bank exceeds package"};
            std::vector<std::byte> bank(package.begin() + static_cast<std::size_t>(offset),
                                        package.begin() + static_cast<std::size_t>(offset + size));
            return {std::move(bank), {}};
        }
    }
    return {{}, "music bank ID not found"};
}

BuildResult rebuild_music_bank(std::span<const std::byte> stock) {
    // Rebuild entirely in memory. The stock bank on disk is never modified.
    // HIRC records are parsed with bounds checks before new objects are added.
    if (stock.size() < 60 || stock.size() > kMaxBankSize)
        return {{}, "invalid stock bank size"};
    std::size_t pos = 0, hirc_offset = 0, hirc_size = 0;
    bool saw_bkhd = false, saw_hirc = false;
    while (pos < stock.size()) {
        if (stock.size() - pos < 8)
            return {{}, "truncated bank chunk"};
        auto size = read_u32(stock, pos + 4);
        if (!size)
            return {{}, "truncated chunk size"};
        std::size_t end = 0;
        if (!checked_end(pos + 8, *size, stock.size(), end))
            return {{}, "oversized bank chunk"};
        if (std::memcmp(stock.data() + pos, "BKHD", 4) == 0) {
            if (saw_bkhd || *size < 8 || *read_u32(stock, pos + 12) != kBankId)
                return {{}, "unexpected BKHD"};
            saw_bkhd = true;
        } else if (std::memcmp(stock.data() + pos, "HIRC", 4) == 0) {
            if (saw_hirc)
                return {{}, "multiple HIRC chunks"};
            saw_hirc = true;
            hirc_offset = pos;
            hirc_size = *size;
        }
        pos = end;
    }
    if (!saw_bkhd || !saw_hirc || hirc_size < 4)
        return {{}, "required chunks missing"};
    auto hirc = stock.subspan(hirc_offset + 8, hirc_size);
    auto original_count = read_u32(hirc, 0);
    if (!original_count || *original_count > 100'000)
        return {{}, "bad HIRC count"};
    std::size_t cursor = 4;
    std::span<const std::byte> actor;
    for (std::uint32_t i = 0; i < *original_count; ++i) {
        if (hirc.size() - cursor < 5)
            return {{}, "truncated HIRC object"};
        const auto type = std::to_integer<std::uint8_t>(hirc[cursor]);
        auto size = read_u32(hirc, cursor + 1);
        if (!size)
            return {{}, "bad HIRC object size"};
        std::size_t end = 0;
        if (!checked_end(cursor + 5, *size, hirc.size(), end))
            return {{}, "oversized HIRC object"};
        if (type == 7 && actor.empty())
            actor = hirc.subspan(cursor + 5, *size);
        cursor = end;
    }
    if (cursor != hirc.size() || actor.size() < 104)
        return {{}, "unsupported HIRC layout"};
    auto old_children = read_u32(actor, 100);
    if (!old_children || 104ull + std::uint64_t(*old_children) * 4 != actor.size())
        return {{}, "unsupported actor mixer layout"};

    std::vector<std::byte> additions;
    additions.reserve(850'000);
    for (std::uint32_t i = 0; i < kCount; ++i) {
        auto payload = make_patched(kNormalSoundExact, kNormalSoundBase + i, kSourceBase + i, 9);
        append_record(additions, 2, payload);
    }
    for (std::uint32_t i = 0; i < kCount; ++i) {
        auto payload = make_patched(kLoopSoundExact, kLoopSoundBase + i, kSourceBase + i, 9);
        append_record(additions, 2, payload);
    }
    std::vector<std::byte> mixer(actor.begin(), actor.begin() + 100);
    append_u32(mixer, kCount * 2);
    for (std::uint32_t i = 0; i < kCount; ++i)
        append_u32(mixer, kNormalSoundBase + i);
    for (std::uint32_t i = 0; i < kCount; ++i)
        append_u32(mixer, kLoopSoundBase + i);
    append_record(additions, 7, mixer);
    for (std::uint32_t i = 0; i < kCount; ++i) {
        auto payload = make_patched(kAction, kNormalActionBase + i, kNormalSoundBase + i, 6);
        append_record(additions, 3, payload);
    }
    for (std::uint32_t i = 0; i < kCount; ++i) {
        auto payload = make_patched(kAction, kLoopActionBase + i, kLoopSoundBase + i, 6);
        append_record(additions, 3, payload);
    }
    for (std::uint32_t i = 0; i < kCount; ++i) {
        std::vector<std::byte> event;
        append_u32(event, wwise_hash("Play_MUS_ID_" + std::to_string(i + 3000)));
        event.push_back(std::byte{1});
        append_u32(event, kNormalActionBase + i);
        append_record(additions, 4, event);
    }
    for (std::uint32_t i = 0; i < kCount; ++i) {
        std::vector<std::byte> event;
        append_u32(event, wwise_hash("Play_MUS_ID_" + std::to_string(i + 3000) + "_LOOP"));
        event.push_back(std::byte{1});
        append_u32(event, kLoopActionBase + i);
        append_record(additions, 4, event);
    }
    constexpr std::uint32_t kAdded = kCount * 6 + 1;
    const std::uint64_t new_hirc_size = std::uint64_t(hirc_size) + additions.size();
    if (new_hirc_size > std::numeric_limits<std::uint32_t>::max())
        return {{}, "rebuilt HIRC too large"};
    std::vector<std::byte> out;
    out.reserve(stock.size() + additions.size());
    out.insert(out.end(), stock.begin(), stock.begin() + hirc_offset + 4);
    append_u32(out, static_cast<std::uint32_t>(new_hirc_size));
    append_u32(out, *original_count + kAdded);
    out.insert(out.end(), hirc.begin() + 4, hirc.end());
    out.insert(out.end(), additions.begin(), additions.end());
    out.insert(out.end(), stock.begin() + hirc_offset + 8 + hirc_size, stock.end());
    return {std::move(out), {}};
}

} // namespace secure_dcl