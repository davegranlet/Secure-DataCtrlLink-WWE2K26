#include "mod_manifest.hpp"
#include <cstdlib>
#include <iostream>

void require(bool value, const char *message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
int main() {
    auto valid = secure_dcl::parse_mod_manifest(
        R"({"version":1,"cakOrder":["base.cak","zzz-fix.cak"],"packageOrder":["Custom_Music.pck"]})");
    require(valid.valid && valid.cak_order.size() == 2 && valid.cak_order[1] == "zzz-fix.cak",
            "valid order was not preserved");
    require(!secure_dcl::parse_mod_manifest(
                 R"({"version":1,"cakOrder":["../bad.cak"],"packageOrder":[]})")
                 .valid,
            "traversal was accepted");
    require(!secure_dcl::parse_mod_manifest(
                 R"({"version":1,"cakOrder":["a.cak","A.cak"],"packageOrder":[]})")
                 .valid,
            "case-insensitive duplicate was accepted");
    require(!secure_dcl::parse_mod_manifest(
                 R"({"version":1,"cakOrder":["plugin.asi"],"packageOrder":[]})")
                 .valid,
            "plugin extension was accepted");
    require(!secure_dcl::parse_mod_manifest(
                 R"({"version":1,"cakOrder":[],"packageOrder":["music.pck"]})")
                 .valid,
            "non-Custom package was accepted");
    require(!secure_dcl::parse_mod_manifest(
                 R"({"version":1,"cakOrder":[],"packageOrder":[],"extra":true})")
                 .valid,
            "unknown field was accepted");
    std::cout << "PASS: bounded manifest allowlists and explicit order\n";
}
