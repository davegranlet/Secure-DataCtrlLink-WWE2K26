#include "mod_manifest.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace secure_dcl {
namespace {
constexpr std::size_t kMaxManifestBytes = 64 * 1024;
constexpr std::size_t kMaxCaks = 32;
constexpr std::size_t kMaxPackages = 16;

struct Parser {
    std::string_view text;
    std::size_t at{};
    std::string error;
    void ws() {
        while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])))
            ++at;
    }
    bool take(char value) {
        ws();
        if (at < text.size() && text[at] == value) {
            ++at;
            return true;
        }
        return false;
    }
    bool string(std::string &out) {
        ws();
        if (at >= text.size() || text[at++] != '"')
            return fail("expected JSON string");
        while (at < text.size()) {
            const unsigned char value = static_cast<unsigned char>(text[at++]);
            if (value == '"')
                return true;
            if (value < 0x20)
                return fail("control character in string");
            if (value != '\\') {
                out.push_back(static_cast<char>(value));
                continue;
            }
            if (at >= text.size())
                return fail("truncated string escape");
            const char escaped = text[at++];
            if (escaped == '"' || escaped == '\\' || escaped == '/')
                out.push_back(escaped);
            else if (escaped == 'b')
                out.push_back('\b');
            else if (escaped == 'f')
                out.push_back('\f');
            else if (escaped == 'n')
                out.push_back('\n');
            else if (escaped == 'r')
                out.push_back('\r');
            else if (escaped == 't')
                out.push_back('\t');
            else
                return fail("unsupported or invalid string escape");
        }
        return fail("unterminated string");
    }
    bool number_one() {
        ws();
        if (at < text.size() && text[at] == '1') {
            ++at;
            return true;
        }
        return fail("version must be 1");
    }
    bool fail(const char *message) {
        if (error.empty())
            error = message;
        return false;
    }
};

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
bool safe_name(const std::string &name, bool package, std::string &error) {
    if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos || name.find(':') != std::string::npos) {
        error = "entry must be one direct-child filename";
        return false;
    }
    const auto folded = lower(name);
    if (package) {
        if (!folded.starts_with("custom") || !folded.ends_with(".pck")) {
            error = "package entries must match Custom*.pck";
            return false;
        }
    } else if (!folded.ends_with(".cak")) {
        error = "CAK entries must end in .cak";
        return false;
    }
    return true;
}

bool array(Parser &parser, std::vector<std::string> &output, std::size_t limit, bool package) {
    if (!parser.take('['))
        return parser.fail("expected array");
    std::unordered_set<std::string> seen;
    parser.ws();
    if (parser.take(']'))
        return true;
    while (true) {
        std::string name;
        if (!parser.string(name))
            return false;
        std::string reason;
        if (!safe_name(name, package, reason))
            return parser.fail(reason.c_str());
        if (output.size() >= limit)
            return parser.fail("manifest entry limit exceeded");
        if (!seen.insert(lower(name)).second)
            return parser.fail("duplicate manifest entry");
        output.push_back(std::move(name));
        if (parser.take(']'))
            return true;
        if (!parser.take(','))
            return parser.fail("expected comma in array");
    }
}
} // namespace

ModManifest parse_mod_manifest(std::string_view json) {
    ModManifest result;
    result.present = true;
    Parser parser{json, 0, {}};
    if (!parser.take('{')) {
        result.error = "expected JSON object";
        return result;
    }
    bool version = false, caks = false, packages = false;
    parser.ws();
    while (!parser.take('}')) {
        std::string key;
        if (!parser.string(key) || !parser.take(':'))
            break;
        if (key == "version") {
            if (version || !parser.number_one()) {
                if (parser.error.empty())
                    parser.error = "duplicate version";
                break;
            }
            version = true;
        } else if (key == "cakOrder") {
            if (caks || !array(parser, result.cak_order, kMaxCaks, false)) {
                if (parser.error.empty())
                    parser.error = "duplicate cakOrder";
                break;
            }
            caks = true;
        } else if (key == "packageOrder") {
            if (packages || !array(parser, result.package_order, kMaxPackages, true)) {
                if (parser.error.empty())
                    parser.error = "duplicate packageOrder";
                break;
            }
            packages = true;
        } else {
            parser.error = "unknown manifest field";
            break;
        }
        if (parser.take('}')) {
            --parser.at;
            continue;
        }
        if (!parser.take(',')) {
            parser.error = "expected comma in object";
            break;
        }
    }
    parser.ws();
    if (parser.error.empty() && parser.at == json.size() && version && caks && packages)
        result.valid = true;
    else
        result.error = parser.error.empty()
                           ? "manifest requires version, cakOrder, and packageOrder"
                           : parser.error;
    return result;
}

ModManifest load_mod_manifest(const std::filesystem::path &game_dir) {
    const auto manifest = game_dir / L"mods" / L"manifest.json";
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(manifest, ec);
    if (ec || !std::filesystem::exists(status))
        return {};
    if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status))
        return {true, false, {}, {}, "manifest must be a regular non-link file"};
    const auto size = std::filesystem::file_size(manifest, ec);
    if (ec || size == 0 || size > kMaxManifestBytes)
        return {true, false, {}, {}, "manifest is empty or exceeds 64 KiB"};
    std::ifstream input(manifest, std::ios::binary);
    if (!input)
        return {true, false, {}, {}, "could not open manifest"};
    std::string text((std::istreambuf_iterator<char>(input)), {});
    if (text.size() != size)
        return {true, false, {}, {}, "short manifest read"};
    return parse_mod_manifest(text);
}
} // namespace secure_dcl
