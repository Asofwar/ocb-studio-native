#pragma once

#include "ocb/tools/ifr/IfrModel.hpp"

#include <istream>
#include <string_view>
#include <vector>

namespace ocb::tools::ifr {

class IfrTextParser final {
public:
    [[nodiscard]] std::vector<IfrQuestion> parse(std::string_view text) const;
    [[nodiscard]] std::vector<IfrQuestion> parse(std::istream& input) const;
};

} // namespace ocb::tools::ifr
