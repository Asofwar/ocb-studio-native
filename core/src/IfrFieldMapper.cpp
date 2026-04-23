#include "ocb/core/IfrFieldMapper.hpp"

#include <set>

namespace ocb::core {
namespace {

FieldKind mapKind(tools::ifr::IfrQuestionKind kind) {
    return kind == tools::ifr::IfrQuestionKind::OneOf ? FieldKind::OneOf : FieldKind::Numeric;
}

std::vector<OcbOption> mapOptions(const std::vector<tools::ifr::IfrOption>& options) {
    std::vector<OcbOption> mapped;
    mapped.reserve(options.size());
    for (const auto& option : options) {
        mapped.push_back({option.value, option.label});
    }
    return mapped;
}

} // namespace

std::vector<OcbField> IfrFieldMapper::mapQuestions(
    const std::vector<tools::ifr::IfrQuestion>& questions) const {
    std::vector<OcbField> fields;
    std::set<std::string> seen;

    for (const auto& question : questions) {
        if (!varStoreBase(question.varStoreName).has_value()) {
            continue;
        }
        if (question.sizeBits != 8 && question.sizeBits != 16 && question.sizeBits != 32 && question.sizeBits != 64) {
            continue;
        }

        OcbField field;
        field.prompt = question.prompt;
        field.help = question.help;
        field.kind = mapKind(question.kind);
        field.varStore = question.varStoreName;
        field.varOffset = question.varOffset;
        field.sizeBits = question.sizeBits;
        field.questionId = "0x" + std::to_string(question.questionId);
        field.ifrLine = question.sourceLine;
        field.options = mapOptions(question.options);

        if (seen.insert(field.id()).second) {
            fields.push_back(std::move(field));
        }
    }

    return fields;
}

} // namespace ocb::core
