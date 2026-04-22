#include "ocb/core/FieldValidation.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace ocb::core {
namespace {

FieldValidationResult error(std::string message) {
    return {false, std::move(message)};
}

bool supportedSize(std::uint32_t sizeBits) {
    return sizeBits == 8 || sizeBits == 16 || sizeBits == 32 || sizeBits == 64;
}

bool optionValueExists(const OcbField& field, std::uint64_t value) {
    return std::any_of(field.options.begin(), field.options.end(), [&](const OcbOption& option) {
        return option.value == value;
    });
}

} // namespace

std::uint64_t fieldMaxValue(const OcbField& field) {
    if (field.sizeBits >= 64) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return (std::uint64_t{1} << field.sizeBits) - 1U;
}

FieldValidationResult validateField(const OcbField& field) {
    if (field.prompt.empty()) {
        return error("Field prompt is empty.");
    }
    if (!varStoreBase(field.varStore).has_value()) {
        return error("Unknown VarStore: " + field.varStore);
    }
    if (!supportedSize(field.sizeBits)) {
        return error("Unsupported field width: " + std::to_string(field.sizeBits));
    }

    const auto maxValue = fieldMaxValue(field);
    std::set<std::uint64_t> seenValues;
    for (const auto& option : field.options) {
        if (option.value > maxValue) {
            return error("Option value is outside the field width for " + field.prompt + ".");
        }
        if (!seenValues.insert(option.value).second) {
            return error("Duplicate option value for " + field.prompt + ".");
        }
    }

    if (field.kind == FieldKind::OneOf && field.options.empty()) {
        return error("OneOf field has no options: " + field.prompt);
    }

    return {};
}

FieldValidationResult validateValue(const OcbField& field, std::uint64_t value) {
    if (const auto fieldResult = validateField(field); !fieldResult) {
        return fieldResult;
    }

    if (value > fieldMaxValue(field)) {
        return error("Value is outside the field width for " + field.prompt + ".");
    }

    if (field.kind == FieldKind::OneOf && !field.options.empty() && !optionValueExists(field, value)) {
        return error("Value is not one of the IFR options for " + field.prompt + ".");
    }

    return {};
}

bool hasBooleanOptions(const OcbField& field) {
    if (field.options.size() != 2) {
        return false;
    }
    return optionValueExists(field, 0) && optionValueExists(field, 1);
}

ValueEditorKind valueEditorKind(const OcbField& field) {
    if (hasBooleanOptions(field)) {
        return ValueEditorKind::Boolean;
    }
    if (field.kind == FieldKind::OneOf && !field.options.empty()) {
        return ValueEditorKind::Enumeration;
    }
    return ValueEditorKind::Numeric;
}

} // namespace ocb::core
