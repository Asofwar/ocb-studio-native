#include "ocb/core/BiosAnalysisService.hpp"

#include "ocb/core/OcbException.hpp"

namespace ocb::core {

BiosAnalysisService::BiosAnalysisService(
    const tools::uefi::UefiExtractor& uefiExtractor,
    const tools::ifr::IfrExtractor& ifrExtractor)
    : uefiExtractor_(uefiExtractor),
      ifrExtractor_(ifrExtractor) {}

BiosAnalysisResult BiosAnalysisService::analyze(std::span<const std::uint8_t> biosImage) const {
    auto root = uefiExtractor_.parseImage(biosImage);
    auto setup = uefiExtractor_.findBestSetupModule(root);
    if (!setup.has_value()) {
        throw OcbException("Setup PE32 module was not found in BIOS image.");
    }

    auto questions = ifrExtractor_.extractQuestions(setup->pe32Body);
    auto fields = mapper_.mapQuestions(questions);

    return {std::move(questions), std::move(fields), std::move(*setup)};
}

} // namespace ocb::core
