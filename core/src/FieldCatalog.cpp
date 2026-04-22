#include "ocb/core/FieldCatalog.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace ocb::core {
namespace {

std::string lower(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return out;
}

bool sameLocation(const OcbField& lhs, const OcbField& rhs) {
    return lhs.varStore == rhs.varStore
        && lhs.varOffset == rhs.varOffset
        && lhs.sizeBits == rhs.sizeBits
        && lhs.prompt == rhs.prompt;
}

} // namespace

FieldCatalog::FieldCatalog() {
    resetToBuiltin();
}

FieldCatalog::FieldCatalog(std::vector<OcbField> fields)
    : fields_(std::move(fields)) {
    rebuildIndex();
}

void FieldCatalog::resetToBuiltin() {
    fields_ = builtinFields();
    rebuildIndex();
}

void FieldCatalog::merge(std::span<const OcbField> fields) {
    for (const auto& field : fields) {
        const auto existing = std::find_if(fields_.begin(), fields_.end(), [&](const OcbField& candidate) {
            return sameLocation(candidate, field);
        });
        if (existing == fields_.end()) {
            fields_.push_back(field);
        } else if (!field.options.empty()) {
            *existing = field;
        }
    }
    rebuildIndex();
}

const std::vector<OcbField>& FieldCatalog::fields() const noexcept {
    return fields_;
}

std::vector<OcbField> FieldCatalog::builtinOnly() const {
    std::vector<OcbField> result;
    const auto& builtin = builtinFields();
    for (const auto& field : fields_) {
        const auto found = std::find_if(builtin.begin(), builtin.end(), [&](const OcbField& candidate) {
            return sameLocation(candidate, field);
        });
        if (found != builtin.end()) {
            result.push_back(field);
        }
    }
    return result;
}

std::vector<OcbField> FieldCatalog::search(std::string_view query) const {
    const auto needle = lower(query);
    if (needle.empty()) {
        return fields_;
    }

    std::vector<OcbField> result;
    for (const auto& field : fields_) {
        const auto haystack = lower(field.prompt + " " + field.varStore + " " + field.id());
        if (haystack.find(needle) != std::string::npos) {
            result.push_back(field);
        }
    }
    return result;
}

const OcbField* FieldCatalog::findById(std::string_view id) const {
    const auto found = indexById_.find(std::string(id));
    if (found == indexById_.end()) {
        return nullptr;
    }
    return &fields_.at(found->second);
}

const OcbField* FieldCatalog::findByPrompt(std::string_view prompt) const {
    const auto found = std::find_if(fields_.begin(), fields_.end(), [&](const OcbField& field) {
        return field.prompt == prompt;
    });
    return found == fields_.end() ? nullptr : &*found;
}

void FieldCatalog::rebuildIndex() {
    indexById_.clear();
    for (std::size_t i = 0; i < fields_.size(); ++i) {
        indexById_[fields_[i].id()] = i;
    }
}

} // namespace ocb::core
