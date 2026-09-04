#pragma once

#include <cstddef>
#include <string>

namespace secure_dcl {

struct CakMountSummary {
    std::size_t accepted{};
    std::size_t rejected{};
    std::size_t failed{};
};

std::wstring cak_status_message(const CakMountSummary &summary);

} // namespace secure_dcl