#include "status_message.hpp"

#include <sstream>

namespace secure_dcl {

std::wstring cak_status_message(const CakMountSummary &summary) {
    std::wostringstream text;
    text << L"Registered " << summary.accepted << L" CAK "
         << (summary.accepted == 1 ? L"archive" : L"archives") << L'.'
         << L"\nVerify mod content in-game.";
    if (summary.rejected) {
        text << L"\nRejected " << summary.rejected << L" invalid "
             << (summary.rejected == 1 ? L"archive" : L"archives") << L'.';
    }
    if (summary.failed) {
        text << L"\n"
             << summary.failed << L" validated "
             << (summary.failed == 1 ? L"archive failed" : L"archives failed") << L" to mount.";
    }
    return text.str();
}

} // namespace secure_dcl