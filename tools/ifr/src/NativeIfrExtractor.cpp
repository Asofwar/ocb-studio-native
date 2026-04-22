#include "ocb/tools/ifr/NativeIfrExtractor.hpp"

#include "UEFI.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ocb::tools::ifr {
namespace {

constexpr std::uint32_t kHeaderLengthMask = 0x7F;
constexpr std::uint32_t kHeaderScopeMask = 0x80;

[[nodiscard]] std::uint8_t byteAt(const std::string& buffer, std::size_t offset) {
    return static_cast<std::uint8_t>(buffer.at(offset));
}

[[nodiscard]] std::uint16_t u16At(const std::string& buffer, std::size_t offset) {
    return static_cast<std::uint16_t>(byteAt(buffer, offset))
        | (static_cast<std::uint16_t>(byteAt(buffer, offset + 1)) << 8);
}

[[nodiscard]] std::uint32_t u32At(const std::string& buffer, std::size_t offset) {
    return static_cast<std::uint32_t>(u16At(buffer, offset))
        | (static_cast<std::uint32_t>(u16At(buffer, offset + 2)) << 16);
}

[[nodiscard]] std::uint64_t u64At(const std::string& buffer, std::size_t offset) {
    return static_cast<std::uint64_t>(u32At(buffer, offset))
        | (static_cast<std::uint64_t>(u32At(buffer, offset + 4)) << 32);
}

[[nodiscard]] std::uint32_t numericSizeBits(std::uint8_t flags) {
    switch (flags & 0x03U) {
    case 0:
        return 8;
    case 1:
        return 16;
    case 2:
        return 32;
    case 3:
        return 64;
    default:
        return 0;
    }
}

[[nodiscard]] std::string stringById(
    const std::vector<UEFI_IFR_STRING_PACK>& stringPackages,
    const std::vector<std::string>& strings,
    const UEFI_IFR_FORM_SET_PACK& formSet,
    std::uint16_t stringId) {
    if (formSet.usingStringPackage >= stringPackages.size()) {
        return {};
    }

    const auto index = static_cast<std::size_t>(
        stringId + stringPackages[formSet.usingStringPackage].structureOffset);
    if (index >= strings.size()) {
        return {};
    }
    return strings[index];
}

[[nodiscard]] std::string readNullTerminated(
    const std::string& buffer,
    std::size_t offset,
    std::size_t maxOffset) {
    std::string value;
    for (auto index = offset; index < maxOffset && index < buffer.size() && buffer[index] != '\0'; ++index) {
        value.push_back(buffer[index]);
    }
    return value;
}

[[nodiscard]] IfrOption parseOption(
    const std::string& buffer,
    std::uint32_t opOffset,
    const std::vector<UEFI_IFR_STRING_PACK>& stringPackages,
    const std::vector<std::string>& strings,
    const UEFI_IFR_FORM_SET_PACK& formSet) {
    const auto type = byteAt(buffer, opOffset + 5);
    std::uint64_t value{};
    switch (type) {
    case 0x00:
        value = byteAt(buffer, opOffset + 6);
        break;
    case 0x01:
        value = u16At(buffer, opOffset + 6);
        break;
    case 0x02:
        value = u32At(buffer, opOffset + 6);
        break;
    case 0x03:
        value = u64At(buffer, opOffset + 6);
        break;
    default:
        break;
    }

    return {
        value,
        stringById(stringPackages, strings, formSet, u16At(buffer, opOffset + 2)),
    };
}

[[nodiscard]] IfrQuestion parseQuestion(
    IfrQuestionKind kind,
    const std::string& buffer,
    std::uint32_t opOffset,
    const std::map<std::uint32_t, std::string>& varStores,
    const std::vector<UEFI_IFR_STRING_PACK>& stringPackages,
    const std::vector<std::string>& strings,
    const UEFI_IFR_FORM_SET_PACK& formSet) {
    const auto varStoreId = u16At(buffer, opOffset + 8);
    const auto store = varStores.find(varStoreId);

    IfrQuestion question;
    question.kind = kind;
    question.prompt = stringById(stringPackages, strings, formSet, u16At(buffer, opOffset + 2));
    question.questionId = u16At(buffer, opOffset + 6);
    question.varStoreId = varStoreId;
    question.varStoreName = store == varStores.end() ? std::string{} : store->second;
    question.varOffset = u16At(buffer, opOffset + 10);
    question.sizeBits = byteAt(buffer, opOffset) == UEFI_IFR_CHECKBOX_OP
        ? 8
        : numericSizeBits(byteAt(buffer, opOffset + 13));
    question.sourceLine = opOffset;
    return question;
}

} // namespace

std::vector<IfrQuestion> NativeIfrExtractor::extractQuestions(
    std::span<const std::uint8_t> setupPe32Body) const {
    const std::string buffer(
        reinterpret_cast<const char*>(setupPe32Body.data()),
        reinterpret_cast<const char*>(setupPe32Body.data() + setupPe32Body.size()));

    std::vector<UEFI_IFR_STRING_PACK> stringPackages;
    std::vector<std::string> strings;
    std::vector<UEFI_IFR_FORM_SET_PACK> formSets;

    getUEFIStringPackages(stringPackages, buffer);
    getUEFIStrings(stringPackages, strings, buffer);
    getUEFIFormSets(formSets, buffer, stringPackages, strings);

    std::vector<IfrQuestion> questions;
    std::map<std::uint32_t, std::string> varStores;
    std::vector<std::size_t> optionQuestionStack;

    for (const auto& formSet : formSets) {
        optionQuestionStack.clear();
        const auto begin = formSet.header.offset + 4;
        const auto end = std::min<std::uint32_t>(
            formSet.header.offset + formSet.header.length,
            static_cast<std::uint32_t>(buffer.size()));

        for (auto offset = begin; offset + 1 < end;) {
            const auto opcode = byteAt(buffer, offset);
            const auto header = byteAt(buffer, offset + 1);
            const auto length = header & kHeaderLengthMask;
            const auto hasScope = (header & kHeaderScopeMask) != 0;
            if (length < 2 || offset + length > end) {
                break;
            }

            if (opcode == UEFI_IFR_VARSTORE_OP && length >= 22) {
                const auto varStoreId = u16At(buffer, offset + 18);
                varStores[varStoreId] = readNullTerminated(buffer, offset + 22, offset + length);
            } else if (opcode == UEFI_IFR_ONE_OF_OP && length >= 14) {
                questions.push_back(parseQuestion(
                    IfrQuestionKind::OneOf,
                    buffer,
                    offset,
                    varStores,
                    stringPackages,
                    strings,
                    formSet));
                if (hasScope) {
                    optionQuestionStack.push_back(questions.size() - 1);
                }
            } else if (opcode == UEFI_IFR_NUMERIC_OP && length >= 14) {
                questions.push_back(parseQuestion(
                    IfrQuestionKind::Numeric,
                    buffer,
                    offset,
                    varStores,
                    stringPackages,
                    strings,
                    formSet));
            } else if (opcode == UEFI_IFR_CHECKBOX_OP && length >= 14) {
                auto question = parseQuestion(
                    IfrQuestionKind::OneOf,
                    buffer,
                    offset,
                    varStores,
                    stringPackages,
                    strings,
                    formSet);
                question.options = {{0, "Disabled"}, {1, "Enabled"}};
                questions.push_back(std::move(question));
            } else if (opcode == UEFI_IFR_ONE_OF_OPTION_OP && length >= 7 && !optionQuestionStack.empty()) {
                questions[optionQuestionStack.back()].options.push_back(
                    parseOption(buffer, offset, stringPackages, strings, formSet));
            } else if (opcode == UEFI_IFR_END_OP && !optionQuestionStack.empty()) {
                optionQuestionStack.pop_back();
            }

            offset += length;
        }
    }

    if (questions.empty()) {
        throw std::runtime_error("No IFR questions were found in Setup PE32 module.");
    }

    return questions;
}

} // namespace ocb::tools::ifr
