#include "status_message.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main() try {
    using secure_dcl::CakMountSummary;
    using secure_dcl::cak_status_message;

    require(cak_status_message(CakMountSummary{1, 0, 0}) ==
                L"Registered 1 CAK archive.\nVerify mod content in-game.",
            "singular registered message is wrong");
    require(cak_status_message(CakMountSummary{4, 0, 0}) ==
                L"Registered 4 CAK archives.\nVerify mod content in-game.",
            "plural registered message is wrong");
    require(cak_status_message(CakMountSummary{2, 1, 1}) ==
                L"Registered 2 CAK archives.\nVerify mod content in-game.\nRejected 1 invalid archive.\n1 validated archive failed to mount.",
            "mixed status message is wrong");
    require(cak_status_message(CakMountSummary{0, 2, 3}) ==
                L"Registered 0 CAK archives.\nVerify mod content in-game.\nRejected 2 invalid archives.\n3 validated archives failed to mount.",
            "zero/error status message is wrong");

    std::cout << "PASS: CAK registration notification and verification warning\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
