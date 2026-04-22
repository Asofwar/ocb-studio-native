#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ocb::tools::uefi {

struct FirmwareNode {
    std::string name;
    std::string guid;
    std::string type;
    std::vector<std::uint8_t> body;
    std::vector<FirmwareNode> children;
};

struct SetupModule {
    std::string pathHint;
    std::vector<std::uint8_t> pe32Body;
};

} // namespace ocb::tools::uefi
