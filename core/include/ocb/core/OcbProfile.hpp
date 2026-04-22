#pragma once

#include "ocb/core/ChecksumCompensator.hpp"
#include "ocb/core/OcbField.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ocb::core {

class OcbProfile {
public:
    OcbProfile() = default;
    explicit OcbProfile(std::vector<std::uint8_t> bytes);

    static OcbProfile loadFromFile(const std::filesystem::path& path);

    void saveToFile(const std::filesystem::path& path, bool compensateChecksums = true) const;
    [[nodiscard]] std::vector<std::uint8_t> exportBytes(bool compensateChecksums = true) const;

    [[nodiscard]] std::uint64_t read(const OcbField& field) const;
    void write(const OcbField& field, std::uint64_t value);
    void resetToOriginal();

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> originalBytes() const noexcept;
    [[nodiscard]] CheckSums targetSums() const noexcept;

private:
    std::vector<std::uint8_t> original_;
    std::vector<std::uint8_t> data_;
    CheckSums targetSums_{};

    void validate() const;
};

} // namespace ocb::core
