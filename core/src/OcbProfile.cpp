#include "ocb/core/OcbProfile.hpp"

#include "ocb/core/OcbException.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

namespace ocb::core {
namespace {

std::vector<std::uint8_t> readAllBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw OcbException("Failed to open OCB file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeAllBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw OcbException("Failed to create OCB file: " + path.string());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw OcbException("Failed to write OCB file: " + path.string());
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

} // namespace

OcbProfile::OcbProfile(std::vector<std::uint8_t> bytes)
    : original_(std::move(bytes)),
      data_(original_),
      targetSums_(ChecksumCompensator::compute(original_)) {
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
        throw OcbException("Field outside OCB data: " + field.prompt);
    }
    return readLittleEndian(data_, offset, size);
}

void OcbProfile::write(const OcbField& field, std::uint64_t value) {
    const auto offset = field.fileOffset();
    const auto size = field.sizeBytes();
    const std::uint64_t maxValue = field.sizeBits == 64
        ? std::numeric_limits<std::uint64_t>::max()
        : ((std::uint64_t{1} << field.sizeBits) - 1U);

    if (value > maxValue) {
        throw OcbException("Value out of range for " + field.prompt);
    }
    if (offset + size > data_.size()) {
        throw OcbException("Field outside OCB data: " + field.prompt);
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

void OcbProfile::validate() const {
    if (data_.size() < 0x1000) {
        throw OcbException("File is too small to be an MSI OC Profile.");
    }

    constexpr std::string_view mos = "$MOS$";
    if (!std::equal(mos.begin(), mos.end(), data_.begin())) {
        throw OcbException("Missing $MOS$ header.");
    }

    constexpr std::array<std::uint8_t, 5> oci{'$', 'O', 'C', 'I', '$'};
    if (std::search(data_.begin(), data_.end(), oci.begin(), oci.end()) == data_.end()) {
        throw OcbException("Missing $OCI$ section marker.");
    }
}

} // namespace ocb::core
