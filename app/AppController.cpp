#include "AppController.hpp"

#include "ocb/core/BiosAnalysisService.hpp"
#include "ocb/core/IfrFieldMapper.hpp"
#include "ocb/core/OcbException.hpp"
#include "ocb/core/Preset.hpp"
#include "ocb/tools/ifr/IfrTextParser.hpp"
#include "ocb/tools/ifr/NativeIfrExtractor.hpp"
#include "ocb/tools/uefi/UefiExtractor.hpp"

#include <fstream>
#include <iterator>
#include <vector>

namespace ocb {

void AppController::openOcb(const std::filesystem::path& path) {
    profile_ = core::OcbProfile::loadFromFile(path);
    ocbPath_ = path;
}

void AppController::openIfrText(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw core::OcbException("Не удалось открыть IFR-файл: " + path.string());
    }

    const auto questions = tools::ifr::IfrTextParser{}.parse(input);
    const auto fields = core::IfrFieldMapper{}.mapQuestions(questions);
    catalog_.merge(fields);
    ifrPath_ = path;
}

void AppController::openBiosImage(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw core::OcbException("Не удалось открыть образ BIOS: " + path.string());
    }

    const std::vector<std::uint8_t> biosImage{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};

    const tools::uefi::UefiToolExtractor uefiExtractor;
    const tools::ifr::NativeIfrExtractor ifrExtractor;
    const core::BiosAnalysisService analysisService(uefiExtractor, ifrExtractor);
    const auto result = analysisService.analyze(biosImage);

    catalog_.merge(result.fields);
    biosMetadata_ = result.metadata;
    biosPath_ = path;
}

void AppController::saveOcb(const std::filesystem::path& path, bool compensateChecksums) const {
    if (!profile_) {
        throw core::OcbException("OCB-профиль не загружен.");
    }
    profile_->saveToFile(path, compensateChecksums);
}

void AppController::applyPreset(const std::string& presetName) {
    if (!profile_) {
        throw core::OcbException("OCB-профиль не загружен.");
    }
    core::applyPreset(*profile_, presetName);
}

void AppController::applyPreset(const core::Preset& preset) {
    if (!profile_) {
        throw core::OcbException("OCB-профиль не загружен.");
    }
    core::applyPreset(*profile_, catalog_.fields(), preset);
}

void AppController::writeField(const std::string& fieldId, std::uint64_t value) {
    if (!profile_) {
        throw core::OcbException("OCB-профиль не загружен.");
    }
    const auto* field = catalog_.findById(fieldId);
    if (field == nullptr) {
        throw core::OcbException("Неизвестный идентификатор поля: " + fieldId);
    }
    profile_->write(*field, value);
}

void AppController::resetProfile() {
    if (profile_) {
        profile_->resetToOriginal();
    }
}

bool AppController::hasProfile() const noexcept {
    return profile_.has_value();
}

const core::OcbProfile& AppController::profile() const {
    if (!profile_) {
        throw core::OcbException("OCB-профиль не загружен.");
    }
    return *profile_;
}

core::OcbProfile& AppController::profile() {
    if (!profile_) {
        throw core::OcbException("OCB-профиль не загружен.");
    }
    return *profile_;
}

const core::FieldCatalog& AppController::catalog() const noexcept {
    return catalog_;
}

core::FieldCatalog& AppController::catalog() noexcept {
    return catalog_;
}

const std::filesystem::path& AppController::ocbPath() const noexcept {
    return ocbPath_;
}

const std::filesystem::path& AppController::ifrPath() const noexcept {
    return ifrPath_;
}

const std::filesystem::path& AppController::biosPath() const noexcept {
    return biosPath_;
}

const std::optional<core::BiosMetadata>& AppController::biosMetadata() const noexcept {
    return biosMetadata_;
}

} // namespace ocb
