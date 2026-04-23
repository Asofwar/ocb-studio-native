#include "ocb/core/OcbProfile.hpp"

#include "MetadataDetection.hpp"
#include "ocb/core/FieldValidation.hpp"
#include "ocb/core/OcbException.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
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

std::uint8_t reflect8(std::uint8_t value) {
    std::uint8_t reflected = 0;
    for (int bit = 0; bit < 8; ++bit) {
        if ((value & (1U << bit)) != 0U) {
            reflected |= static_cast<std::uint8_t>(1U << (7 - bit));
        }
    }
    return reflected;
}

std::uint8_t crc8(
    std::span<const std::uint8_t> bytes,
    std::uint8_t polynomial,
    std::uint8_t initial,
    std::uint8_t xorOut,
    bool reflectInput,
    bool reflectOutput) {
    auto crc = initial;
    for (auto byte : bytes) {
        const auto input = reflectInput ? reflect8(byte) : byte;
        crc ^= input;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x80U) != 0U) {
                crc = static_cast<std::uint8_t>((crc << 1U) ^ polynomial);
            } else {
                crc = static_cast<std::uint8_t>(crc << 1U);
            }
        }
    }
    if (reflectOutput) {
        crc = reflect8(crc);
    }
    return static_cast<std::uint8_t>(crc ^ xorOut);
}

std::uint8_t bcdByte(int value) {
    const auto tens = static_cast<std::uint8_t>((value / 10) % 10);
    const auto ones = static_cast<std::uint8_t>(value % 10);
    return static_cast<std::uint8_t>((tens << 4U) | ones);
}

bool parseTimestampOverride(std::tm& result) {
    const auto* raw = std::getenv("OCB_EXPORT_TIMESTAMP");
    if (raw == nullptr || std::strlen(raw) != 14U) {
        return false;
    }
    for (std::size_t index = 0; index < 14U; ++index) {
        if (raw[index] < '0' || raw[index] > '9') {
            return false;
        }
    }

    const auto parsePart = [&](std::size_t offset, std::size_t length) {
        int value = 0;
        for (std::size_t index = 0; index < length; ++index) {
            value = (value * 10) + static_cast<int>(raw[offset + index] - '0');
        }
        return value;
    };

    result = {};
    result.tm_year = parsePart(0, 4) - 1900;
    result.tm_mon = parsePart(4, 2) - 1;
    result.tm_mday = parsePart(6, 2);
    result.tm_hour = parsePart(8, 2);
    result.tm_min = parsePart(10, 2);
    result.tm_sec = parsePart(12, 2);
    result.tm_isdst = -1;
    return std::mktime(&result) != -1;
}

std::tm exportTimestamp() {
    std::tm timestamp{};
    if (parseTimestampOverride(timestamp)) {
        return timestamp;
    }

    const auto now = std::chrono::system_clock::now();
    const auto nowTime = std::chrono::system_clock::to_time_t(now);
#if defined(_WIN32)
    localtime_s(&timestamp, &nowTime);
#else
    localtime_r(&nowTime, &timestamp);
#endif
    return timestamp;
}

void writeAsciiAt(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view value) {
    for (std::size_t index = 0; index < value.size() && offset + index < bytes.size(); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value[index]);
    }
}

void applyBiosStyleMetadata(std::vector<std::uint8_t>& bytes) {
    if (bytes.size() <= 0x2E8AU) {
        return;
    }

    const auto timestamp = exportTimestamp();

    bytes[0x2D3C] = bcdByte(timestamp.tm_sec);
    bytes[0x2D3D] = 0;
    bytes[0x2D3E] = bcdByte(timestamp.tm_min);
    bytes[0x2D3F] = 0;
    bytes[0x2D40] = bcdByte(timestamp.tm_hour);
    bytes[0x2D41] = 0;
    bytes[0x2D42] = static_cast<std::uint8_t>(timestamp.tm_wday);
    bytes[0x2D43] = bcdByte(timestamp.tm_mday);

    std::array<char, 15> stampText{};
    std::snprintf(
        stampText.data(),
        stampText.size(),
        "%04d%02d%02d%02d%02d%02d",
        timestamp.tm_year + 1900,
        timestamp.tm_mon + 1,
        timestamp.tm_mday,
        timestamp.tm_hour,
        timestamp.tm_min,
        timestamp.tm_sec);
    writeAsciiAt(bytes, 0x2E6B, std::string_view(stampText.data(), 14));

    bytes[0x0012] = static_cast<std::uint8_t>((static_cast<std::uint8_t>(timestamp.tm_mon) << 4U)
        | static_cast<std::uint8_t>((timestamp.tm_year + 1900) % 10));
    bytes[0x2E26] = (bytes[0x0F49] & 0x80U) != 0U ? 0x00U : 0x80U;
    const auto view = std::span<const std::uint8_t>(bytes);
    bytes[0x2D7F] = crc8(view.subspan(0x2E3C, 0x2E8A - 0x2E3C), 0x27, 0xFF, 0xFF, false, true);
    bytes[0x2E8A] = crc8(view.subspan(0x2D3C, 0x2D7F - 0x2D3C), 0x7F, 0xFF, 0x00, true, true);
    bytes[0x0011] = crc8(view.subspan(0x2D3C, 0x2E8A - 0x2D3C), 0xC1, 0x00, 0x00, false, false);
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
    if (output != original_) {
        applyBiosStyleMetadata(output);
    }
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
