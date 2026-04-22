#include "ocb/core/BiosAnalysisService.hpp"

#include "MetadataDetection.hpp"
#include "ocb/core/OcbException.hpp"

namespace ocb::core {
namespace {

BiosMetadata detectBiosMetadata(
    std::span<const std::uint8_t> biosImage,
    const tools::uefi::SetupModule& setup,
    std::size_t questionCount,
    std::size_t fieldCount) {
    const auto strings = detail::extractTextStrings(biosImage);

    BiosMetadata metadata;
    metadata.imageSize = static_cast<std::uint64_t>(biosImage.size());
    metadata.setupPe32Size = static_cast<std::uint64_t>(setup.pe32Body.size());
    metadata.questionCount = static_cast<std::uint64_t>(questionCount);
    metadata.fieldCount = static_cast<std::uint64_t>(fieldCount);
    metadata.setupPath = setup.pathHint;
    metadata.boardName = detail::chooseBoardName(strings);
    metadata.biosVersion = detail::firstBiosVersion(strings);
    return metadata;
}

} // namespace

BiosAnalysisService::BiosAnalysisService(
    const tools::uefi::UefiExtractor& uefiExtractor,
    const tools::ifr::IfrExtractor& ifrExtractor)
    : uefiExtractor_(uefiExtractor),
      ifrExtractor_(ifrExtractor) {}

BiosAnalysisResult BiosAnalysisService::analyze(std::span<const std::uint8_t> biosImage) const {
    auto root = uefiExtractor_.parseImage(biosImage);
    auto setup = uefiExtractor_.findBestSetupModule(root);
    if (!setup.has_value()) {
        throw OcbException("Модуль Setup PE32 не найден в образе BIOS.");
    }

    auto questions = ifrExtractor_.extractQuestions(setup->pe32Body);
    auto fields = mapper_.mapQuestions(questions);
    auto metadata = detectBiosMetadata(biosImage, *setup, questions.size(), fields.size());

    return {std::move(questions), std::move(fields), std::move(*setup), std::move(metadata)};
}

} // namespace ocb::core
