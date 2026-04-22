#pragma once

#include "ocb/core/IfrFieldMapper.hpp"
#include "ocb/core/OcbField.hpp"
#include "ocb/tools/ifr/IfrExtractor.hpp"
#include "ocb/tools/uefi/UefiExtractor.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ocb::core {

struct BiosMetadata {
    std::uint64_t imageSize{};
    std::uint64_t setupPe32Size{};
    std::uint64_t questionCount{};
    std::uint64_t fieldCount{};
    std::string boardName;
    std::string biosVersion;
    std::string setupPath;
};

struct BiosAnalysisResult {
    std::vector<tools::ifr::IfrQuestion> questions;
    std::vector<OcbField> fields;
    tools::uefi::SetupModule setupModule;
    BiosMetadata metadata;
};

class BiosAnalysisService final {
public:
    BiosAnalysisService(
        const tools::uefi::UefiExtractor& uefiExtractor,
        const tools::ifr::IfrExtractor& ifrExtractor);

    [[nodiscard]] BiosAnalysisResult analyze(std::span<const std::uint8_t> biosImage) const;

private:
    const tools::uefi::UefiExtractor& uefiExtractor_;
    const tools::ifr::IfrExtractor& ifrExtractor_;
    IfrFieldMapper mapper_;
};

} // namespace ocb::core
