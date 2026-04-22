#pragma once

#include "ocb/core/OcbField.hpp"
#include "ocb/tools/ifr/IfrModel.hpp"

#include <vector>

namespace ocb::core {

class IfrFieldMapper final {
public:
    [[nodiscard]] std::vector<OcbField> mapQuestions(
        const std::vector<tools::ifr::IfrQuestion>& questions) const;
};

} // namespace ocb::core
