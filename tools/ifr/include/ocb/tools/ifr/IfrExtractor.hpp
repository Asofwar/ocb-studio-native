#pragma once

#include "ocb/tools/ifr/IfrModel.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ocb::tools::ifr {

class IfrExtractor {
public:
    virtual ~IfrExtractor() = default;

    [[nodiscard]] virtual std::vector<IfrQuestion> extractQuestions(
        std::span<const std::uint8_t> setupPe32Body) const = 0;
};

} // namespace ocb::tools::ifr
