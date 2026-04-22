#pragma once

#include "ocb/tools/ifr/IfrExtractor.hpp"

namespace ocb::tools::ifr {

class NativeIfrExtractor final : public IfrExtractor {
public:
    [[nodiscard]] std::vector<IfrQuestion> extractQuestions(
        std::span<const std::uint8_t> setupPe32Body) const override;
};

} // namespace ocb::tools::ifr
