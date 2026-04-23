#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ocb::tools::ifr {

enum class IfrQuestionKind {
    Numeric,
    OneOf,
};

struct IfrOption {
    std::uint64_t value{};
    std::string label;
};

struct IfrQuestion {
    IfrQuestionKind kind{IfrQuestionKind::Numeric};
    std::string prompt;
    std::string help;
    std::uint32_t questionId{};
    std::uint32_t varStoreId{};
    std::string varStoreName;
    std::uint32_t varOffset{};
    std::uint32_t sizeBits{};
    std::uint32_t sourceLine{};
    std::vector<IfrOption> options;
};

} // namespace ocb::tools::ifr
