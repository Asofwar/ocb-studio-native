#pragma once

#include "ocb/core/BiosAnalysisService.hpp"
#include "ocb/core/FieldCatalog.hpp"
#include "ocb/core/OcbProfile.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace ocb {

class AppController final {
public:
    void openOcb(const std::filesystem::path& path);
    void openIfrText(const std::filesystem::path& path);
    void openBiosImage(const std::filesystem::path& path);
    void saveOcb(const std::filesystem::path& path, bool compensateChecksums) const;
    void applyPreset(const std::string& presetName);
    void writeField(const std::string& fieldId, std::uint64_t value);
    void resetProfile();

    [[nodiscard]] bool hasProfile() const noexcept;
    [[nodiscard]] const core::OcbProfile& profile() const;
    [[nodiscard]] core::OcbProfile& profile();
    [[nodiscard]] const core::FieldCatalog& catalog() const noexcept;
    [[nodiscard]] core::FieldCatalog& catalog() noexcept;
    [[nodiscard]] const std::filesystem::path& ocbPath() const noexcept;
    [[nodiscard]] const std::filesystem::path& ifrPath() const noexcept;
    [[nodiscard]] const std::filesystem::path& biosPath() const noexcept;
    [[nodiscard]] const std::optional<core::BiosMetadata>& biosMetadata() const noexcept;

private:
    std::optional<core::OcbProfile> profile_;
    std::optional<core::BiosMetadata> biosMetadata_;
    core::FieldCatalog catalog_;
    std::filesystem::path ocbPath_;
    std::filesystem::path ifrPath_;
    std::filesystem::path biosPath_;
};

} // namespace ocb
