#pragma once

#include "ocb/tools/uefi/FirmwareModel.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ocb::tools::uefi {

class UefiExtractor {
public:
    virtual ~UefiExtractor() = default;

    [[nodiscard]] virtual FirmwareNode parseImage(std::span<const std::uint8_t> image) const = 0;
    [[nodiscard]] virtual std::optional<SetupModule> findBestSetupModule(const FirmwareNode& root) const = 0;
};

class NativeUefiExtractor final : public UefiExtractor {
public:
    [[nodiscard]] FirmwareNode parseImage(std::span<const std::uint8_t> image) const override;
    [[nodiscard]] std::optional<SetupModule> findBestSetupModule(const FirmwareNode& root) const override;
};

} // namespace ocb::tools::uefi
