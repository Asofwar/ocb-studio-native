#include "ocb/core/OcbProfile.hpp"

#include "MetadataDetection.hpp"
#include "ocb/core/FieldValidation.hpp"
#include "ocb/core/OcbException.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>
#include <utility>

namespace ocb::core {
namespace {

std::vector<std::uint8_t> readAllBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw OcbException("Не удалось открыть OCB-файл: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeAllBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw OcbException("Не удалось создать OCB-файл: " + path.string());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw OcbException("Не удалось записать OCB-файл: " + path.string());
    }
}

std::uint64_t readLittleEndian(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t size) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < size; ++i) {
        value |= static_cast<std::uint64_t>(bytes[offset + i]) << (i * 8U);
    }
    return value;
}

void writeLittleEndian(std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t size, std::uint64_t value) {
    for (std::size_t i = 0; i < size; ++i) {
        bytes.at(offset + i) = static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU);
    }
}

OcbProfileMetadata detectMetadata(std::span<const std::uint8_t> bytes) {
    OcbProfileMetadata metadata;
    metadata.fileSize = static_cast<std::uint64_t>(bytes.size());

    constexpr std::string_view mos = "$MOS$";
    metadata.hasMosHeader = bytes.size() >= mos.size() && std::equal(mos.begin(), mos.end(), bytes.begin());

    constexpr std::array<std::uint8_t, 5> oci{'$', 'O', 'C', 'I', '$'};
    const auto ociIt = std::search(bytes.begin(), bytes.end(), oci.begin(), oci.end());
    metadata.hasOciSection = ociIt != bytes.end();
    metadata.ociOffset = metadata.hasOciSection
        ? static_cast<std::uint64_t>(std::distance(bytes.begin(), ociIt))
        : 0;

    if (metadata.hasMosHeader && metadata.hasOciSection) {
        metadata.format = "MSI OC profile ($MOS$/$OCI$)";
    } else if (metadata.hasMosHeader) {
        metadata.format = "MSI OC profile ($MOS$)";
    } else {
        metadata.format = "Unknown";
    }

    const auto strings = detail::extractTextStrings(bytes);
    metadata.boardName = detail::chooseBoardName(strings);
    metadata.profileName = detail::chooseProfileName(strings);
    metadata.biosVersion = detail::firstBiosVersion(strings);

    return metadata;
}

} // namespace

OcbProfile::OcbProfile(std::vector<std::uint8_t> bytes)
    : original_(std::move(bytes)),
      data_(original_),
      targetSums_(ChecksumCompensator::compute(original_)),
      metadata_(detectMetadata(original_)) {
    validate();
}

OcbProfile OcbProfile::loadFromFile(const std::filesystem::path& path) {
    return OcbProfile(readAllBytes(path));
}

void OcbProfile::saveToFile(const std::filesystem::path& path, bool compensateChecksums) const {
    writeAllBytes(path, exportBytes(compensateChecksums));
}

std::vector<std::uint8_t> OcbProfile::exportBytes(bool compensateChecksums) const {
    auto output = data_;
    if (compensateChecksums) {
        ChecksumCompensator::compensate(output, targetSums_);
    }
    return output;
}

std::uint64_t OcbProfile::read(const OcbField& field) const {
    const auto offset = field.fileOffset();
    const auto size = field.sizeBytes();
    if (offset + size > data_.size()) {
        throw OcbException("Поле находится за пределами OCB-данных: " + field.prompt);
    }
    return readLittleEndian(data_, offset, size);
}

void OcbProfile::write(const OcbField& field, std::uint64_t value) {
    const auto offset = field.fileOffset();
    const auto size = field.sizeBytes();

    if (const auto validation = validateValue(field, value); !validation) {
        throw OcbException(validation.message);
    }
    if (offset + size > data_.size()) {
        throw OcbException("Поле находится за пределами OCB-данных: " + field.prompt);
    }
    writeLittleEndian(data_, offset, size, value);
}

void OcbProfile::resetToOriginal() {
    data_ = original_;
}

bool OcbProfile::empty() const noexcept {
    return data_.empty();
}

std::span<const std::uint8_t> OcbProfile::bytes() const noexcept {
    return data_;
}

std::span<const std::uint8_t> OcbProfile::originalBytes() const noexcept {
    return original_;
}

CheckSums OcbProfile::targetSums() const noexcept {
    return targetSums_;
}

const OcbProfileMetadata& OcbProfile::metadata() const noexcept {
    return metadata_;
}

void OcbProfile::validate() const {
    if (data_.size() < 0x1000) {
        throw OcbException("Файл слишком мал, чтобы быть профилем MSI OC.");
    }

    constexpr std::string_view mos = "$MOS$";
    if (!std::equal(mos.begin(), mos.end(), data_.begin())) {
        throw OcbException("Отсутствует заголовок $MOS$.");
    }

    constexpr std::array<std::uint8_t, 5> oci{'$', 'O', 'C', 'I', '$'};
    if (std::search(data_.begin(), data_.end(), oci.begin(), oci.end()) == data_.end()) {
        throw OcbException("Отсутствует маркер секции $OCI$.");
    }
}

} // namespace ocb::core
