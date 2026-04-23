#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ocb::core {

enum class FieldKind {
    Numeric,
    OneOf,
};

struct OcbOption {
    std::uint64_t value{};
    std::string label;
};

struct OcbField {
    std::string prompt;
    FieldKind kind{FieldKind::Numeric};
    std::string varStore;
    std::uint32_t varOffset{};
    std::uint32_t sizeBits{};
    std::string questionId;
    std::uint32_t ifrLine{};
    std::vector<OcbOption> options;
    std::string help;

    [[nodiscard]] std::string id() const;
    [[nodiscard]] std::size_t fileOffset() const;
    [[nodiscard]] std::size_t sizeBytes() const;
};

[[nodiscard]] std::optional<std::size_t> varStoreBase(std::string_view name);
[[nodiscard]] const std::vector<OcbField>& builtinFields();

} // namespace ocb::core
