#pragma once

#include "ocb/core/OcbField.hpp"

#include <cstdint>
#include <string>

namespace ocb::core {

enum class ValueEditorKind {
    Numeric,
    Boolean,
    Enumeration,
};

struct FieldValidationResult {
    bool valid{true};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return valid;
    }
};

[[nodiscard]] std::uint64_t fieldMaxValue(const OcbField& field);
[[nodiscard]] FieldValidationResult validateField(const OcbField& field);
[[nodiscard]] FieldValidationResult validateValue(const OcbField& field, std::uint64_t value);
[[nodiscard]] bool hasBooleanOptions(const OcbField& field);
[[nodiscard]] ValueEditorKind valueEditorKind(const OcbField& field);

} // namespace ocb::core
