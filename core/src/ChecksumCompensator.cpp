#include "ocb/core/ChecksumCompensator.hpp"

#include "ocb/core/OcbException.hpp"

#include <algorithm>

namespace ocb::core {
namespace {

void writeLe16(std::vector<std::uint8_t>& data, std::size_t offset, std::uint16_t value) {
    data.at(offset) = static_cast<std::uint8_t>(value & 0xFFU);
    data.at(offset + 1) = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void writeLe32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
    data.at(offset) = static_cast<std::uint8_t>(value & 0xFFU);
    data.at(offset + 1) = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    data.at(offset + 2) = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    data.at(offset + 3) = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::uint16_t readLe16(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset] | (static_cast<std::uint16_t>(data[offset + 1]) << 8U));
}

std::uint32_t readLe32(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset])
        | (static_cast<std::uint32_t>(data[offset + 1]) << 8U)
        | (static_cast<std::uint32_t>(data[offset + 2]) << 16U)
        | (static_cast<std::uint32_t>(data[offset + 3]) << 24U);
}

} // namespace

CheckSums ChecksumCompensator::compute(std::span<const std::uint8_t> data) {
    return {sum8(data), sum16(data), sum32(data)};
}

CompensationResult ChecksumCompensator::compensate(std::vector<std::uint8_t>& data, CheckSums target) {
    if (kCompensationOffset + kCompensationSize > data.size()) {
        throw OcbException("File is too small for the known checksum compensation area.");
    }

    std::fill(
        data.begin() + static_cast<std::ptrdiff_t>(kCompensationOffset),
        data.begin() + static_cast<std::ptrdiff_t>(kCompensationOffset + kCompensationSize),
        std::uint8_t{0});

    const auto current32 = sum32(data);
    const auto needed32 = static_cast<std::uint32_t>(target.sum32 - current32);
    writeLe32(data, kCompensationOffset, needed32);

    auto actual = compute(data);
    if (actual == target) {
        return {target, actual, "preserved sum8/sum16/sum32"};
    }
    if (actual.sum16 == target.sum16 && actual.sum32 == target.sum32) {
        return {target, actual, "preserved sum16/sum32"};
    }

    std::fill(
        data.begin() + static_cast<std::ptrdiff_t>(kCompensationOffset),
        data.begin() + static_cast<std::ptrdiff_t>(kCompensationOffset + kCompensationSize),
        std::uint8_t{0});

    const auto current16 = sum16(data);
    const auto needed16 = static_cast<std::uint16_t>(target.sum16 - current16);
    writeLe16(data, kCompensationOffset, needed16);

    actual = compute(data);
    if (actual.sum16 == target.sum16) {
        return {target, actual, "preserved sum16 only"};
    }

    throw OcbException("Could not preserve checksum-style sums.");
}

std::uint8_t ChecksumCompensator::sum8(std::span<const std::uint8_t> data) {
    std::uint32_t total = 0;
    for (const auto byte : data) {
        total = (total + byte) & 0xFFU;
    }
    return static_cast<std::uint8_t>(total);
}

std::uint16_t ChecksumCompensator::sum16(std::span<const std::uint8_t> data) {
    std::uint32_t total = 0;
    for (std::size_t i = 0; i + 1 < data.size(); i += 2) {
        total = (total + readLe16(data, i)) & 0xFFFFU;
    }
    return static_cast<std::uint16_t>(total);
}

std::uint32_t ChecksumCompensator::sum32(std::span<const std::uint8_t> data) {
    std::uint32_t total = 0;
    for (std::size_t i = 0; i + 3 < data.size(); i += 4) {
        total += readLe32(data, i);
    }
    return total;
}

} // namespace ocb::core
