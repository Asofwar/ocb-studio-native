#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ocb::core {

struct CheckSums {
    std::uint8_t sum8{};
    std::uint16_t sum16{};
    std::uint32_t sum32{};

    [[nodiscard]] bool operator==(const CheckSums& other) const = default;
};

struct CompensationResult {
    CheckSums target;
    CheckSums actual;
    std::string report;
};

class ChecksumCompensator {
public:
    static constexpr std::size_t kCompensationOffset = 0x2D84;
    static constexpr std::size_t kCompensationSize = 16;

    [[nodiscard]] static CheckSums compute(std::span<const std::uint8_t> data);
    static CompensationResult compensate(std::vector<std::uint8_t>& data, CheckSums target);

private:
    [[nodiscard]] static std::uint8_t sum8(std::span<const std::uint8_t> data);
    [[nodiscard]] static std::uint16_t sum16(std::span<const std::uint8_t> data);
    [[nodiscard]] static std::uint32_t sum32(std::span<const std::uint8_t> data);
};

} // namespace ocb::core
